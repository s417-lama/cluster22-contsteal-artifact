#ifndef MADI_IBV_H
#define MADI_IBV_H

#include "madm_misc.h"
#include <memory>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cinttypes>
#include <cstring>
#include <cassert>
#include <infiniband/verbs.h>
#include <mpi.h>

extern "C" int gasnet_AMPoll();

namespace madi {
namespace ibv {

    static int debug = 0;

    template <class T, class D>
    class resource {
        T obj_;
        D d_;
        bool valid_;

    public:
        resource() : obj_(), d_(), valid_(false) {}

        resource(T obj, D d) : obj_(obj), d_(d), valid_(true)
        {
        }

        template <class... Args>
            resource(D d, Args... args) : obj_(args...), d_(d), valid_(true)
        {
        }

        ~resource()
        {
            if (valid_)
                d_(obj_);
        }

        resource(const resource<T, D>& rsc) = default;
        resource<T, D>& operator=(resource<T, D>& rsc)
        {
            this->obj_   = rsc.obj_;
            this->d_     = rsc.d_;
            this->valid_ = rsc.valid_;

            rsc.valid_ = false;

            return *this;
        }

        resource<T, D>& operator=(resource<T, D>&& rsc)
        {
            this->obj_   = rsc.obj_;
            this->d_     = rsc.d_;
            this->valid_ = rsc.valid_;

            rsc.valid_ = false;

            return *this;
        }

        T& get() { return obj_; }
    };

    template <class T, class D, class... Args>
    static resource<T, D> make_resource(D d, Args... args)
    {
        return resource<T, D>(d, args...);
    }

    template <class T, class D>
    static resource<T, D> make_resource(T obj, D d)
    {
        return resource<T, D>(obj, d);
    }

    class environment {
        size_t pid_;
        size_t n_procs_;

        using ctx_deleter = decltype(&ibv_close_device);
        std::unique_ptr<ibv_context, ctx_deleter> ctx_;
        uint16_t lid_;

        using pd_deleter = decltype(&ibv_dealloc_pd);
        std::unique_ptr<ibv_pd, pd_deleter> pd_;

    public:
        static constexpr auto PORT_NUM = 1;

    public:
        environment()
            : ctx_(nullptr, ibv_close_device)
              , lid_(-1)
              , pd_(nullptr, ibv_dealloc_pd)
        {
            // init pid and n_procs
            int pid, n_procs;
            MPI_Comm_rank(MPI_COMM_WORLD, &pid);
            MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

            pid_ = pid;
            n_procs_ = n_procs;

#if 0
            // initialize infiniband verbs
            int r = ibv_fork_init();

            if (r != 0)
                MADI_PERR_DIE("ibv_fork_init");
#endif

            // make context
            auto devs = std::make_unique(ibv_get_device_list(NULL),
                                         ibv_free_device_list);

            if (devs == nullptr)
                MADI_PERR_DIE("ibv_get_device_list");

            auto dev = devs.get()[0];

            if (dev == nullptr) 
                MADI_DIE("no available Infiniband devices");

            if (debug)
                printf("IB device: %s\n", ibv_get_device_name(dev));

            ctx_ = std::make_unique(ibv_open_device(dev),
                                    ibv_close_device);

            assert(ctx_ != nullptr);

            // get LID
            ibv_port_attr port_attr;
            int ret = ibv_query_port(ctx_.get(), PORT_NUM, &port_attr);

            assert(ret == 0);

            lid_ = port_attr.lid;

            assert(lid_ != 0);

            // make protection domain
            pd_ = std::make_unique(ibv_alloc_pd(ctx_.get()),
                                   ibv_dealloc_pd);

            assert(pd_ != nullptr);
        }

        ~environment() = default;

        size_t pid() { return pid_; }
        size_t n_procs() { return n_procs_; }
        uint16_t lid() { return lid_; }

        ibv_context * context() { return ctx_.get(); }
        ibv_pd * prot_domain() { return pd_.get(); }

        void native_barrier() { MPI_Barrier(MPI_COMM_WORLD); }

        template <class T>
        void native_sendrecv(T *send_buf, int send_target,
                             T *recv_buf, int recv_target)
        {
            MPI_Sendrecv(send_buf, sizeof(T), MPI_BYTE, send_target, 0,
                         recv_buf, sizeof(T), MPI_BYTE, recv_target, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    };

    template <class T>
    class rdma_ptr {
        T *p_;
        uint32_t rkey_;
        ibv_mr *mr_;
    public:
        rdma_ptr() : p_(nullptr), rkey_(0), mr_(nullptr) {}
        rdma_ptr(T *p, ibv_mr * mr) : p_(p), rkey_(mr->rkey), mr_(mr) {}
        ~rdma_ptr() = default;

        rdma_ptr(const rdma_ptr<T>& rp) = default;

        T * get() { return p_; }
        uint32_t rkey() { return rkey_; }
        ibv_mr * mem_region() { return mr_; }

        rdma_ptr<T> operator+(int n)
        {
            return rdma_ptr<T>(p_ + n, mr_);
        }

        template <class U>
        explicit operator rdma_ptr<U>()
        {
            return rdma_ptr<U>(reinterpret_cast<U *>(p_), mr_);
        }
    };

    class endpoint {

        using qp_vector = std::vector<ibv_qp *>;

        environment& env_;
        resource<ibv_cq *, int(*)(ibv_cq *)> cq_;
        resource<qp_vector, void(*)(qp_vector&)> qps_;
        resource<ibv_mr *, int(*)(ibv_mr *)> mr_;

    public:
        endpoint(environment& env)
            : env_(env)
        {
            auto ctx = env_.context();
            auto n_cq_entries = 64;
            cq_ = make_resource(ibv_create_cq(ctx, n_cq_entries,
                                              NULL, NULL, 0),
                                ibv_destroy_cq);
            assert(cq_.get() != nullptr);

            qps_ = make_resource(spmd_create_qps(cq_.get()),
                                 &spmd_destroy_qps);
        }

        size_t pid() { return env_.pid(); }
        size_t n_procs() { return env_.n_procs(); }

        ibv_qp * create_qp(ibv_cq * cq)
        {
            auto pd = env_.prot_domain();

            ibv_qp_init_attr qia;
            memset(&qia, 0, sizeof(qia));
            qia.qp_type    = IBV_QPT_RC;
            qia.send_cq    = cq;
            qia.recv_cq    = cq;
            qia.sq_sig_all = 1;
            qia.cap.max_send_wr  = 32;
            qia.cap.max_recv_wr  = 32;
            qia.cap.max_send_sge =  1;
            qia.cap.max_recv_sge =  1;

            auto qp = ibv_create_qp(pd, &qia);

            assert(qp != nullptr);

            return qp;
        }

        void modify_qp(ibv_qp * qp, uint32_t src_psn, uint16_t dst_lid,
                       uint32_t dst_qp_num, uint32_t dst_psn)
        {
            // Reset -> Init
            auto access =
                IBV_ACCESS_LOCAL_WRITE
                | IBV_ACCESS_REMOTE_WRITE
                | IBV_ACCESS_REMOTE_READ
                | IBV_ACCESS_REMOTE_ATOMIC;

            ibv_qp_attr init_attr;
            memset(&init_attr, 0, sizeof(init_attr));
            init_attr.qp_state        = IBV_QPS_INIT;
            init_attr.port_num        = environment::PORT_NUM;
            init_attr.qp_access_flags = access;

            auto r0 = ibv_modify_qp(qp, &init_attr,
                                    IBV_QP_STATE
                                    | IBV_QP_PKEY_INDEX
                                    | IBV_QP_PORT
                                    | IBV_QP_ACCESS_FLAGS);
            assert(r0 == 0);

            // Init -> RTR (Ready To Receive)
            ibv_qp_attr rtr_attr;
            memset(&rtr_attr, 0, sizeof(rtr_attr));
            rtr_attr.qp_state               = IBV_QPS_RTR;
            rtr_attr.path_mtu               = IBV_MTU_4096;
            rtr_attr.dest_qp_num            = dst_qp_num;
            rtr_attr.rq_psn                 = dst_psn;
            rtr_attr.max_rd_atomic          = 16;
            rtr_attr.max_dest_rd_atomic     = 16;
            rtr_attr.min_rnr_timer          = 0;
            rtr_attr.ah_attr.is_global      = 0;
            rtr_attr.ah_attr.dlid           = dst_lid;
            rtr_attr.ah_attr.sl             = 0;
            rtr_attr.ah_attr.src_path_bits  = 0;
            rtr_attr.ah_attr.port_num       = environment::PORT_NUM;

            auto r1 = ibv_modify_qp(qp, &rtr_attr,
                                    IBV_QP_STATE
                                    | IBV_QP_AV
                                    | IBV_QP_PATH_MTU
                                    | IBV_QP_DEST_QPN
                                    | IBV_QP_RQ_PSN
                                    | IBV_QP_MAX_DEST_RD_ATOMIC
                                    | IBV_QP_MIN_RNR_TIMER);
            assert(r1 == 0);

            // RTR -> RTS (Ready To Send)
            ibv_qp_attr rts_attr;
            memset(&rts_attr, 0, sizeof(rts_attr));
            rts_attr.qp_state           = IBV_QPS_RTS;
            rts_attr.timeout            = 0;
            rts_attr.retry_cnt          = 7;
            rts_attr.rnr_retry          = 7;
            rts_attr.sq_psn             = src_psn;
            rts_attr.max_rd_atomic      = 0;

            auto r2 = ibv_modify_qp(qp, &rts_attr,
                                    IBV_QP_STATE
                                    | IBV_QP_TIMEOUT
                                    | IBV_QP_RETRY_CNT
                                    | IBV_QP_RNR_RETRY
                                    | IBV_QP_SQ_PSN
                                    | IBV_QP_MAX_QP_RD_ATOMIC);
            assert(r2 == 0);
        }

        qp_vector spmd_create_qps(ibv_cq * cq)
        {
            auto me = env_.pid();
            auto n_procs = env_.n_procs();
            std::vector<ibv_qp *> qps(n_procs);

            // create a qp for each process
            for (auto& qp : qps) {
                qp = create_qp(cq);
            }

            // modify qp state to Ready To Send
            auto lid = env_.lid();
            uint32_t psn = rand() & 0xFFFFFF;

            for (auto i = 0UL; i < n_procs; i++) {
                auto send_target = (me + i) % n_procs;
                auto recv_target = (me + n_procs - i) % n_procs;

                uint16_t recv_lid;
                env_.native_sendrecv(&lid, send_target,
                                     &recv_lid, recv_target);

                uint32_t recv_qp_num;
                auto send_qp_num = qps[send_target]->qp_num;
                env_.native_sendrecv(&send_qp_num, send_target,
                                     &recv_qp_num, recv_target);

                uint32_t recv_psn;
                env_.native_sendrecv(&psn, send_target,
                                     &recv_psn, recv_target);

                auto& qp = qps[recv_target];
                modify_qp(qp, psn, recv_lid, recv_qp_num, recv_psn);

                if (debug) {
                    printf("%zu: modify_qp(qps[%zu]:qp_num=%d, recv_lid=%d, "
                           "recv_qp_num=%d, recv_psn=%d\n",
                           me, recv_target, qp->qp_num,
                           recv_lid, recv_qp_num, recv_psn);
                }
            }

            return qps;
        }

        static void spmd_destroy_qps(qp_vector& qps)
        {
            for (auto qp : qps)
                ibv_destroy_qp(qp);
        }

        ~endpoint() = default;

        template <class T>
        rdma_ptr<T> register_memory(T *addr, size_t size)
        {
            assert(size <= 1UL * 1024 * 1024 * 1024);        // FIXME

            auto pd = env_.prot_domain();

            auto access =
                IBV_ACCESS_LOCAL_WRITE
                | IBV_ACCESS_REMOTE_WRITE
                | IBV_ACCESS_REMOTE_READ
                | IBV_ACCESS_REMOTE_ATOMIC;

            auto mr = ibv_reg_mr(pd, addr, size, access);

            if (mr == nullptr)
                perror("ibv_reg_mr");

            assert(mr != nullptr);

            return rdma_ptr<T>(addr, mr);
        }

        template <class T>
        void deregister_memory(rdma_ptr<T> p, size_t size)
        {
            auto mr = p.mem_region();

            auto r = ibv_dereg_mr(mr);

            assert(r == 0);
        }

        class rdma_handle {
            size_t count_;
        public:
            explicit rdma_handle(size_t count) : count_(count) {}
            ~rdma_handle() = default;
            rdma_handle(const rdma_handle&) = delete;
            rdma_handle& operator=(rdma_handle&) = delete;
            rdma_handle& operator=(rdma_handle&&) = delete;

            void notify(size_t num)
            {
                count_ -= num;
                assert(count_ >= 0);
            }

            bool done()
            {
                return count_ == 0;
            }
        };

        template <class T>
        void rdma_put(rdma_ptr<T> dst, rdma_ptr<T> src, size_t size,
                      size_t target)
        {
            rdma_handle handle(1);

            auto qp = qps_.get()[target];

            // fill scatter gather elements
            ibv_sge sge;
            memset(&sge, 0, sizeof(sge));
            sge.addr   = (uint64_t)(uintptr_t)src.get();
            sge.length = size;
            sge.lkey   = src.mem_region()->lkey;

            // fill send work request
            ibv_send_wr swr;
            memset(&swr, 0, sizeof(swr));
            swr.wr_id          = (uint64_t)(uintptr_t)&handle;
            swr.next           = NULL;
            swr.sg_list        = &sge;
            swr.num_sge        = 1;
            swr.opcode         = IBV_WR_RDMA_WRITE;
            swr.send_flags     = 0;
            swr.wr.rdma.remote_addr    = (uint64_t)(uintptr_t)dst.get();
            swr.wr.rdma.rkey           = dst.rkey();

            // start RDMA fetch-and-add
            struct ibv_send_wr *bad_wr;
            auto r = ibv_post_send(qp, &swr, &bad_wr);

            if (r != 0)
                MADI_PERR_DIE("ibv_post_send");

            if (debug) {
                printf("%zu: post RDMA WRITE: dst=%p, src=%p, size=%d, "
                       "target=%zu\n",
                       env_.pid(),
                       (void *)swr.wr.rdma.remote_addr,
                       (void *)sge.addr,
                       sge.length,
                       target);
            }

            // wait for completion
            while (!handle.done())
                poll();
        }

        template <class T>
        void rdma_get(rdma_ptr<T> dst, rdma_ptr<T> src, size_t size,
                      size_t target)
        {
            rdma_handle handle(1);

            auto qp = qps_.get()[target];

            // fill scatter gather elements
            ibv_sge sge;
            memset(&sge, 0, sizeof(sge));
            sge.addr   = (uint64_t)(uintptr_t)dst.get();
            sge.length = size;
            sge.lkey   = dst.mem_region()->lkey;

            // fill send work request
            ibv_send_wr swr;
            memset(&swr, 0, sizeof(swr));
            swr.wr_id          = (uint64_t)(uintptr_t)&handle;
            swr.next           = NULL;
            swr.sg_list        = &sge;
            swr.num_sge        = 1;
            swr.opcode         = IBV_WR_RDMA_READ;
            swr.send_flags     = 0;
            swr.wr.rdma.remote_addr    = (uint64_t)(uintptr_t)src.get();
            swr.wr.rdma.rkey           = src.rkey();

            // start RDMA fetch-and-add
            struct ibv_send_wr *bad_wr;
            auto r = ibv_post_send(qp, &swr, &bad_wr);

            if (r != 0)
                MADI_PERR_DIE("ibv_post_send");

            if (debug) {
                printf("%zu: post RDMA WRITE: dst=%p, src=%p, size=%d, "
                       "target=%zu\n",
                       env_.pid(),
                       (void *)sge.addr,
                       (void *)swr.wr.rdma.remote_addr,
                       sge.length,
                       target);
            }

            // wait for completion
            while (!handle.done())
                poll();
        }

        void rdma_fetch_and_add(rdma_ptr<uint64_t> dst,
                                rdma_ptr<uint64_t> result_ptr,
                                uint32_t value, size_t target)
        {
            rdma_handle handle(1);

            auto qp = qps_.get()[target];

            // fill scatter gather elements
            ibv_sge sge;
            memset(&sge, 0, sizeof(sge));
            sge.addr   = (uint64_t)(uintptr_t)result_ptr.get();
            sge.length = sizeof(unsigned long);
            sge.lkey   = result_ptr.mem_region()->lkey;

            // fill send work request
            ibv_send_wr swr;
            memset(&swr, 0, sizeof(swr));
            swr.wr_id          = (uint64_t)(uintptr_t)&handle;
            swr.next           = NULL;
            swr.sg_list        = &sge;
            swr.num_sge        = 1;
            swr.opcode         = IBV_WR_ATOMIC_FETCH_AND_ADD;
            swr.send_flags     = IBV_SEND_FENCE;
            swr.wr.atomic.remote_addr    = (uint64_t)(uintptr_t)dst.get();
            swr.wr.atomic.compare_add    = value;
            swr.wr.atomic.rkey           = dst.rkey();

            MADI_DPUTSB1("FETCH_AND_ADD( (%p, %u), (%p, %u), %u, %u, %zu )",
                        (void *)swr.wr.atomic.remote_addr,
                        swr.wr.atomic.rkey,
                        (void *)sge.addr,
                        sge.lkey,
                        value,
                        sge.length,
                        target);

            // start RDMA fetch-and-add
            struct ibv_send_wr *bad_wr;
            auto r = ibv_post_send(qp, &swr, &bad_wr);

            if (r != 0)
                MADI_PERR_DIE("ibv_post_send");

            if (debug) {
                printf("%zu: post fetch-and-add: p=%p, value=%" PRIu32 ", "
                       "target=%zu\n",
                       env_.pid(),
                       (void *)swr.wr.atomic.remote_addr,
                       value,
                       target);
            }

            // wait for completion
            while (!handle.done())
                poll();

            MADI_DPUTSG1("FAD RESULT: %u", *result_ptr.get());
        }

        void poll()
        {
            ibv_wc wc;
            auto n_entries = 1;
            auto r = ibv_poll_cq(cq_.get(), n_entries, &wc);

            if (r == 0)
                return;

            if (r < 0)
                MADI_PERR_DIE("ibv_poll_cq");

            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr, "%zu: ibv_poll_cq: status=%d (%s)\n",
                        env_.pid(), wc.status, ibv_wc_status_str(wc.status));
                MPI_Finalize();
                exit(1);
            }

            auto& handle = *(rdma_handle *)wc.wr_id;
            switch (wc.opcode) {
                case IBV_WC_RDMA_WRITE:
                    handle.notify(1);
                    if (debug)
                        printf("%zu: RDMA WRITE completed\n", env_.pid());
                    break;
                case IBV_WC_RDMA_READ:
                    handle.notify(1);
                    if (debug)
                        printf("%zu: RDMA READ completed\n", env_.pid());
                    break;
                case IBV_WC_FETCH_ADD:
                    handle.notify(1);
                    if (debug)
                        printf("%zu: fetch-and-add completed\n", env_.pid());
                    break;
                default:
                    fprintf(stderr, "%zu: ibv_poll_cq: unknown opcode (%d)\n",
                            env_.pid(), wc.opcode);
                    break;
            }

#ifdef MADI_GASNET_USE_IBV_ATOMIC
#if MADI_GASNET_USE_IBV_ATOMIC
            gasnet_AMPoll();
#endif
#endif
        }

        void barrier()
        {
            env_.native_barrier();
        }

        template <class T>
        void broadcast(T *buf, size_t root)
        {
            MPI_Bcast(reinterpret_cast<void *>(buf),
                      sizeof(T), MPI_BYTE,
                      static_cast<int>(root),
                      MPI_COMM_WORLD);
        }

        template <class T>
        void allgather(T& value, T *buf)
        {
            void *sendbuf = reinterpret_cast<void *>(&value);
            void *recvbuf = reinterpret_cast<void *>(buf);
            MPI_Allgather(sendbuf, sizeof(T), MPI_BYTE,
                          recvbuf, sizeof(T), MPI_BYTE,
                          MPI_COMM_WORLD);
        }
    };

    class prepinned_endpoint : public endpoint {
        size_t size_;
        rdma_ptr<uint8_t> rp_;
        std::vector<ibv_mr> mrs_;

    public:
        prepinned_endpoint(environment& env, void *addr, size_t size)
            : endpoint(env)
              , size_(size)
              , mrs_(env.n_procs())
        {
            rp_ = this->register_memory((uint8_t *)addr, size);

            auto mr = rp_.mem_region();
            auto mrs_buf = reinterpret_cast<ibv_mr *>(mrs_.data());
            this->allgather(*mr, mrs_buf);
        }

        ~prepinned_endpoint()
        {
            deregister_memory(rp_, size_);
        }

        using endpoint::rdma_put;

        template <class T>
        void rdma_put(T *dst, T *src, size_t size, size_t target)
        {
            auto me = this->pid();

            auto remote_mr = &mrs_[target];
            auto local_mr  = &mrs_[me];

            rdma_ptr<T> dst_rp(dst, remote_mr);
            rdma_ptr<T> src_rp(src, local_mr);

            this->rdma_put(dst_rp, src_rp, size, target);
        }

        using endpoint::rdma_get;

        template <class T>
        void rdma_get(T *dst, T *src, size_t size, size_t target)
        {
            auto me = this->pid();

            auto remote_mr = &mrs_[target];
            auto local_mr  = &mrs_[me];

            rdma_ptr<T> dst_rp(dst, local_mr);
            rdma_ptr<T> src_rp(src, remote_mr);

            this->rdma_get(dst_rp, src_rp, size, target);
        }

        using endpoint::rdma_fetch_and_add;

        void rdma_fetch_and_add(uint64_t *dst, uint64_t  *result_ptr,
                                uint32_t value, size_t target)
        {
            auto me = this->pid();

            auto remote_mr = &mrs_[target];
            auto local_mr  = &mrs_[me];

            rdma_ptr<uint64_t> dst_rp(dst, remote_mr);
            rdma_ptr<uint64_t> result_rp(result_ptr, local_mr);

            this->rdma_fetch_and_add(dst_rp, result_rp, value, target);
        }
    };

}
}

#endif

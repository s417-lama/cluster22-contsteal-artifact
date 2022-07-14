#ifndef MADI_COMM_AMPEER_H
#define MADI_COMM_AMPEER_H

#include "process_config.h"
#include "id_pool.h"
#include "madm_misc.h"
#include "madm_debug.h"
#include "madm_comm-decls.h"

extern bool ampeer_prof_enabled;

namespace madi {
namespace comm {

    enum {
        AM_IMPLICIT_REPLY = 128,
        AM_FETCH_AND_ADD_INT_REQ,
        AM_FETCH_AND_ADD_INT_REP,
        AM_FETCH_AND_ADD_LONG_REQ,
        AM_FETCH_AND_ADD_LONG_REP,
        AM_FETCH_AND_ADD_ULONG_REQ,
        AM_FETCH_AND_ADD_ULONG_REP,
    };

    struct aminfo {
        int initiator;
        uint8_t *replybuf;
        bool replied;
    };

    struct amheader {
        uint8_t *replybuf;
        uint32_t initiator;
        uint16_t tag;
        uint16_t size;
    };

    template <class CB>
    class ampeer;

    //typedef void (*ampeer_do_send_t)(void *, int, void *, size_t, int,
    //                                 const aminfo *);

    // GCC 4.6.1 does not support template aliases
    // typedef void (ampeer::*ampeer_do_send_t)(int, void *, size_t, int, 
    //                                               const aminfo *);
    template <class CB>
    struct ampeer_do_send {
        typedef void (ampeer<CB>::*type)(int, void *, size_t, int,
                                         const aminfo *);
    };

    template <class CB>
    struct ampending {
        typename ampeer_do_send<CB>::type do_send;
        int tag;
        void *p;
        size_t size;
        int pid;
        aminfo info;
    };

    // local RDMA buffer pool
    class buffer_pool {
        uint8_t *buffers_;
        size_t buffer_size_;
        id_pool<int> free_list_;
    public:
        buffer_pool(uint8_t *buffer, size_t buffer_size, size_t n_buffers) :
            buffers_(buffer),
            buffer_size_(buffer_size),
            free_list_(0, n_buffers)
        {
        }

        ~buffer_pool() {}

        uint8_t * bufptr(int id) const { return buffers_ + buffer_size_ * id; }
        int from() const { return free_list_.from(); }
        int to() const { return free_list_.to(); }
        bool valid(int id) const { return free_list_.valid(id); }
        bool empty() const { return free_list_.empty(); }
        bool filled() const { return free_list_.filled(); }
 
        void push(int id) { free_list_.push(id); }

        bool pop(int *id, uint8_t **buf)
        {
            int n;
            if (free_list_.pop(&n)) {
                if (id != NULL)
                    *id = n;
                if (buf != NULL)
                    *buf = bufptr(n);
                return true;
            } else {
                return false;
            }
        }
    };

    // remote RDMA buffer pool
    class remote_buffer_pool {
        int me_;
        size_t ambuf_size_;
        uint8_t **server_addrs_;             // pid -> remote buf address
        std::vector<bool> remotebuf_using_;  // pid -> using or not
    public:
        remote_buffer_pool(uint8_t **server_addrs, size_t ambuf_size,
                           process_config& config)
            : me_(config.get_native_pid())
            , ambuf_size_(ambuf_size)
            , server_addrs_(server_addrs)
            , remotebuf_using_(config.get_n_procs(), false)
        {
            // initialize for page allocation (vector initializer already done?)
            for (size_t i = 0; i < remotebuf_using_.size(); i++)
                remotebuf_using_[i] = false;
        }

        uint8_t * remotebufptr(int pid) const
        {
            return server_addrs_[pid] + ambuf_size_ * me_;
        }

        uint8_t * recvbufptr(int pid) const
        {
            return server_addrs_[me_] + ambuf_size_ * pid;
        }

        bool empty(int pid) const
        {
            return remotebuf_using_[pid];
        }

        void push(int pid)
        {
            MADI_ASSERT(remotebuf_using_[pid]);
            remotebuf_using_[pid] = false;
        }

        bool pop(int pid, uint8_t **buf)
        {
            if (remotebuf_using_[pid]) {
                return false;
            } else {
                remotebuf_using_[pid] = true;
                *buf = remotebufptr(pid);
                return true;
            }
        }
    };

    // Active Messages peer 
    // (including communication server process)
    template <class CommBase>
    class ampeer : noncopyable {

        int me_;

        const amhandler_t handler_;
        const int server_mod_;

        const size_t ambuf_size_;
        buffer_pool *sendbufs_;               // local send buffer pool
        buffer_pool *recvbufs_;               // local recv buffer pool
        remote_buffer_pool *remotebufs_;      // remote (recv) buffer pool
     
        typedef std::vector<ampending<CommBase>> pending_list;

        pending_list pendings_;               // pending send request queue

        CommBase& c_;
        const int fjmpi_tag_base_;

        uint8_t *ambufs_;
        uint8_t **server_addrs_;
    public:
        ampeer(CommBase& c, amhandler_t handler, int server_mod,
               size_t n_max_sends, int fjmpi_tag_base);
        ~ampeer();
        
        void request(int tag, void *p, size_t size, int pid,
                     process_config& config);
        void request_nbi(int tag, void *p, size_t size, int pid,
                         process_config& config);
        void amfence();
        void reply(int tag, void *p, size_t size, aminfo *info,
                   process_config& config);
        bool amhandle(int r, int tag, int pid, process_config& config);
        void ampoll(process_config& config);

        template <class T>
        T fetch_and_add(T *dst, T value, int target, process_config& config);

    private:
        bool is_server(int pid);
        int server_pid(size_t pid);

        int sendbuf_id_of_tag(int tag);
        int tag_of_sendbuf_id(int id);

        bool is_sending(int pid);
        void add_sending(int pid);
        void remove_sending(int pid);

        amheader& get_amheader(uint8_t *ambuf);
        bool is_recvbuf_filled(uint8_t *buf);
        size_t fill_sendbuf(uint8_t *buf, uint8_t *replybuf, 
                            int tag, void *p, size_t size);
        void clear_recvbuf(uint8_t *buf);

        void do_request_nbi(int tag, void *p, size_t size, int pid,
                            MADI_UNUSED const aminfo *info);
        void do_reply(int tag, void *p, size_t size, int pid,
                      const aminfo *info);
        void send_with_pool(typename ampeer_do_send<CommBase>::type f, int tag,
                            void *p, size_t size, int pid, const aminfo *info,
                            process_config& config);

        void do_send(uint64_t raddr, uint64_t laddr, size_t size, int pid);

        void handle_request(int pid, process_config& config);
        void handle_reply(int replybuf_id, uint8_t *recvbuf,
                          process_config& config);
        void handle_local_completion(int sendbuf_id);
        void call_handler(int tag, int pid, void *p, size_t size,
                          aminfo *info);
    };

#define ENABLE_PROF 1

    struct prof_item {
        long t;
        long n;
        long min;
        long max;
        long tmp;

        prof_item() : t(0), n(0), min(0), max(0), tmp(0) {}
#if ENABLE_PROF
        void begin() { if (ampeer_prof_enabled) tmp = rdtsc(); }
        void end() { if (ampeer_prof_enabled) {
                         long v = rdtsc() - tmp;
                         t += v; n += 1; 
                         min = (min == 0 || v < min) ? v : min; 
                         max = (v > max) ? v : max;
                     }
                   }
#else
        void begin() {}
        void end() {}
#endif
        long average() { return (n == 0) ? 0 : t / n; }
    };

    struct ampeer_prof {
        prof_item do_request_nbi;
        prof_item do_reply;
        prof_item reply;
        prof_item fence;
        prof_item handle_request;
        prof_item handle_reply;
        prof_item amreply;
        prof_item fill_repbuf;
        prof_item reply_put;
        
        ampeer_prof() {}

#if ENABLE_PROF
        void output()
        {
            prof_item items[] = {
                do_request_nbi, do_reply, reply, fence, handle_request,
                handle_reply, amreply, fill_repbuf, reply_put,
            };

            long n = 0;
            for (auto item : items)
                n += item.n;

            if (n == 0)
                return;

#define FMT(name) #name "={mn%ld,mx%ld,av%ld,n%ld}, "
#define VAL(name) name.min, name.max, (name.average()), name.n
            MADI_DPUTS(
                   FMT(do_request_nbi)
                   FMT(fence)
                   FMT(handle_request)
                   FMT(reply)
                   FMT(do_reply)
                   FMT(fill_repbuf)
                   FMT(reply_put)
                   FMT(handle_reply)
                   , VAL(do_request_nbi)
                   , VAL(fence)
                   , VAL(handle_request)
                   , VAL(reply)
                   , VAL(do_reply)
                   , VAL(fill_repbuf)
                   , VAL(reply_put)
                   , VAL(handle_reply)
                   );
#undef FMT
#undef VAL
        }
#else
        void output() {}
#endif
    };

    extern ampeer_prof *g_amprof;


}
}

#endif

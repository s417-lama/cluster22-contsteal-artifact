#ifndef MADI_COMM_SYSTEM_H
#define MADI_COMM_SYSTEM_H

#include "process_config.h"
#include "comm_base.h"
#include "collectives.h"
#include "options.h"
#include "madm_comm-decls.h"
#include "madm_misc.h"
#include "madm_debug.h"


namespace madi {
namespace comm {

    class comm_system : noncopyable {
        typedef comm_base CommBase;
        typedef collectives<CommBase> coll_type;

        CommBase c_;
        process_config& config_native_;
        process_config  config_compute_;
        process_config* config_;

        std::unique_ptr<coll_type> coll_native_;
        std::unique_ptr<coll_type> coll_compute_;
        coll_type *coll_;

    public:
        explicit comm_system(int& argc, char **& argv, amhandler_t handler)
            : c_(argc, argv, handler)
            , config_native_(c_.native_config())
            , config_compute_(make_comm_compute(config_native_),
                              native_of_compute_pid,
                              compute_of_native_pid)
            , config_(&config_native_)
        {
            coll_native_ = std::make_unique<coll_type>(c_, config_native_);

            if (config_compute_.is_compute())
                coll_compute_ = std::make_unique<coll_type>(c_, config_compute_);
            else
                coll_compute_ = nullptr;

            coll_ = coll_native_.get();
        }

        ~comm_system() = default;

        int get_pid() const { return config_->get_pid(); }
        int get_n_procs() const { return config_->get_n_procs(); }

        const process_config& config() const { return *config_; }
        CommBase& base() { return c_; }

        template <class... Args>
        void start(void (*f)(int, char **, Args...),
                   int& argc, char **&argv, Args... args)
        {
            if (config_compute_.is_compute()) {
                config_ = &config_compute_;
                coll_ = coll_compute_.get();

                f(argc, argv, args...);

                config_ = &config_native_;
                coll_ = coll_native_.get();
            }

            coll_native_->barrier();
        }

        template <class T>
        T **coll_malloc(size_t size)
        { return (T **)c_.coll_malloc(sizeof(T) * size, *config_); }

        template <class T>
        void coll_free(T **ptrs)
        { c_.coll_free((void **)ptrs, *config_); }

        template <class T>
        T *malloc(size_t size)
        { return (T *)c_.malloc(sizeof(T) * size, *config_); }

        template <class T>
        void free(T *p)
        { c_.free((void *)p, *config_); }

        int coll_mmap(uint8_t *addr, size_t size)
        { return c_.coll_mmap(addr, size, *config_); }

        void coll_munmap(int memid)
        { c_.coll_munmap(memid, *config_); }

        void put_nbi(void *dst, void *src, size_t size, int target)
        { c_.put_nbi(dst, src, size, target, *config_); }

        void reg_put_nbi(int memid, void *dst, void *src, size_t size,
                         int target)
        { c_.reg_put_nbi(memid, dst, src, size, target, *config_); }

        void get_nbi(void *dst, void *src, size_t size, int target)
        { c_.get_nbi(dst, src, size, target, *config_); }

        void reg_get_nbi(int memid, void *dst, void *src, size_t size,
                         int target)
        { c_.reg_get_nbi(memid, dst, src, size, target, *config_); }

        int poll(int *tag_out, int *pid_out)
        { return c_.poll(tag_out, pid_out, *config_); }

        void fence()
        { c_.fence(); }

        void native_barrier()
        { c_.native_barrier(*config_); }

        void put(void *dst, void *src, size_t size, int target)
        { c_.put(dst, src, size, target, *config_); }

        void reg_put(int memid, void *dst, void *src, size_t size,
                     int target)
        { c_.reg_put(memid, dst, src, size, target, *config_); }

        void get(void *dst, void *src, size_t size, int target)
        { c_.get(dst, src, size, target, *config_); }

        void reg_get(int memid, void *dst, void *src, size_t size,
                     int target)
        { c_.reg_get(memid, dst, src, size, target, *config_); }

        template <class T>
        void put_value(T *dst, T value, int target)
        { c_.put_value(dst, value, target, *config_); }

        template <class T>
        T get_value(T *src, int target)
        { return c_.get_value(src, target, *config_); }

        template <class T>
        T fetch_and_add(T *dst, T value, int target)
        { return c_.fetch_and_add(dst, value, target, *config_); }

        void lock_init(lock_t* lp)
        { c_.lock_init(lp, *config_); }

        bool trylock(lock_t* lp, int target)
        { return c_.trylock(lp, target, *config_); }

        void lock(lock_t* lp, int target)
        { c_.lock(lp, target, *config_); }

        void unlock(lock_t* lp, int target)
        { c_.unlock(lp, target, *config_); }

        void request(int tag, void *p, size_t size, int pid)
        { c_.request(tag, p, size, pid, *config_); }

#if 0
        void request_nbi(int tag, void *p, size_t size, int pid)
        { c_.request_nbi(tag, p, size, pid, *config_); }
#endif

        void reply(int tag, void *p, size_t size, aminfo *info)
        { c_.reply(tag, p, size, info, *config_); }

#if 0
        bool handle(int r, int tag, int pid)
        { return c_.handle(r, tag, pid, *config_); }
#endif

        bool barrier_try()
        {
            return coll_->barrier_try();
        }

        void barrier()
        {
            return coll_->barrier();
        }

        template <class T>
        void broadcast(T* buf, size_t size, pid_t root)
        {
            coll_->broadcast(buf, size, root);
        }

        template <class T>
        void reduce(T dst[], const T src[], size_t size, pid_t root,
                    reduce_op op)
        {
            coll_->reduce(dst, src, size, root, op);
        }

    private:
        static MPI_Comm make_comm_compute(process_config& config)
        {
            int me = config.get_pid();

            MPI_Comm comm_native = config.comm();

            int server_mod = options.server_mod;

            if (server_mod == 0) {
                return MPI_COMM_WORLD;
            } else {
                int comp_color = (me % server_mod != 0) ? 0 : 1;

                MPI_Comm comm_compute;
                int r = MPI_Comm_split(comm_native, comp_color, me,
                                       &comm_compute);

                MADI_CHECK(r == MPI_SUCCESS);

                return comm_compute;
            }
        }

        static int native_of_compute_pid(int pid)
        { 
            int server_mod = options.server_mod;

            if (server_mod == 0) {
                return pid;
            } else {
                int r = pid / (server_mod - 1) * server_mod
                    + pid % (server_mod - 1) + 1;

#if 0
                MADI_DPUTS("compute:%d -> native:%d", pid, r);
#endif
                return r;
            }
        }

        static int compute_of_native_pid(int pid)
        {
            int server_mod = options.server_mod;

            if (server_mod == 0) {
                return pid;
            } else {
                int node_pid = pid % server_mod;

                if (node_pid == 0) {
                    return -1;
                } else {
                    int r = pid - pid / server_mod - 1;

#if 0
                MADI_DPUTS("native:%d -> compute:%d", pid, r);
#endif
                    return r;
                }
            }
        }
    };

}
}

#endif

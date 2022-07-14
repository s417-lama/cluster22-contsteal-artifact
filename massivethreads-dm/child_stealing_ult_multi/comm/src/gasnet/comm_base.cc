#include "gasnet/comm_base.h"
#include "gasnet/comm_memory.h"
#include "madm_comm-cdecls.h"
#include "madm_comm-inl.h"
#include "threadsafe.h"
#include "options.h"

#include "../gasnet_ext.h"
#include <mpi.h>
#include <type_traits>

#define MADI_CB_DEBUG  0

#if MADI_CB_DEBUG
#define MADI_CB_DPUTS(s, ...)  MADI_DPUTS(s, ##__VA_ARGS__)
#else
#define MADI_CB_DPUTS(s, ...)
#endif

/* Macro to check return codes and terminate with useful message. */
#define GASNET_SAFE(fncall) do {                                     \
    int _retval;                                                     \
    if ((_retval = fncall) != GASNET_OK) {                           \
      fprintf(stderr, "ERROR calling: %s\n"                          \
                      " at: %s:%i\n"                                 \
                      " error: %s (%s)\n",                           \
              #fncall, __FILE__, __LINE__,                           \
              gasnet_ErrorName(_retval), gasnet_ErrorDesc(_retval)); \
      fflush(stderr);                                                \
      abort();                                                       \
      /*gasnet_exit(_retval);*/                                      \
    }                                                                \
  } while(0)

#define MAKEWORD(T, hi,lo)                                           \
    ((T)((static_cast<uint64_t>(hi) << 32)                           \
     | (static_cast<uint64_t>(lo) & 0xFFFFFFFF)))
#define HIWORD(arg) \
    (static_cast<uint32_t>((uint64_t)arg >> 32))
#define LOWORD(arg) \
    (static_cast<uint32_t>((uint64_t)arg))


namespace madi {
namespace comm {

    enum comm_base_constants {
        AM_FAD_INT_REQ = 128,
        AM_FAD_INT_REP,
        AM_FAD_LONG_REQ,
        AM_FAD_LONG_REP,
        AM_FAD_U64_REQ,
        AM_FAD_U64_REP,
        AM_HANDLE_AM,
        AM_HANDLE_LAST__,        
    };

    class join_counter : noncopyable {
        volatile size_t count_;
    public:
        join_counter(size_t n) : count_(n) {}
        ~join_counter() = default;

        void notify()
        {
            threadsafe::fetch_and_add(&count_, -1UL);
        }

        bool try_wait()
        {
            if (count_ > 0) {
                return false;
            } else {
                threadsafe::rwbarrier();
                return true;
            }
        }
    };

    template <class T>
    class sync_var : noncopyable {
        join_counter counter_;
        T result_;

    public:
        sync_var() : counter_(1) {}
        ~sync_var() = default;

        void set(T& value)
        {
            result_ = value;
            counter_.notify();
        }

        bool try_get(T *result)
        {
            if (counter_.try_wait()) {
                *result = result_;
                return true;
            } else {
                return false;
            }
        }

        T get()
        {
            while (!counter_.try_wait())
                madi::comm::poll();

            return result_;
        }
    };

#if 0
    template <class T> T am_fad_req_tag();
    template <class T> T am_fad_rep_tag();

    template <> int  am_fad_req_tag() { return AM_FAD_INT_REQ; }
    template <> int  am_fad_rep_tag() { return AM_FAD_INT_REP; }

    template <> long am_fad_req_tag() { return AM_FAD_LONG_REQ; }
    template <> long am_fad_rep_tag() { return AM_FAD_LONG_REP; }

    template <> unsigned long am_fad_req_tag() { return AM_FAD_LONG_REQ; }
    template <> unsigned long am_fad_rep_tag() { return AM_FAD_LONG_REP; }

    template <> uint64_t am_fad_req_tag() { return AM_FAD_U64_REQ; }
    template <> uint64_t am_fad_rep_tag() { return AM_FAD_U64_REP; }

#else
    // Clang typedefs unsigned long as uint64_t, so we cannot use naive 
    // overloading; instead, we use std::enable_if and std::is_same.

#define OVERLOAD(ovl_type) \
    template <class T> \
    typename std::enable_if<std::is_same<T, ovl_type>::value, ovl_type>::type

#define OVERLOAD_IF_DIFFER(ovl_type, differ_type) \
    template <class T> \
    typename std::enable_if<std::is_same<T, ovl_type>::value \
                              && !std::is_same<T, differ_type>::value, \
                            ovl_type>::type

    OVERLOAD(int) am_fad_req_tag() { return AM_FAD_INT_REQ; }
    OVERLOAD(int) am_fad_rep_tag() { return AM_FAD_INT_REP; }

    OVERLOAD(long) am_fad_req_tag() { return AM_FAD_LONG_REQ; }
    OVERLOAD(long) am_fad_rep_tag() { return AM_FAD_LONG_REP; }

    OVERLOAD(unsigned long) am_fad_req_tag() { return AM_FAD_LONG_REQ; }
    OVERLOAD(unsigned long) am_fad_rep_tag() { return AM_FAD_LONG_REP; }

    OVERLOAD_IF_DIFFER(uint64_t, unsigned long)
        am_fad_req_tag() { return AM_FAD_U64_REQ; }
    OVERLOAD_IF_DIFFER(uint64_t, unsigned long)
        am_fad_rep_tag() { return AM_FAD_U64_REP; }

#undef OVERLOAD
#undef OVERLOAD_IF_DIFFER

#endif

    template <class T>
    void handle_fetch_and_add_i64_rep(gasnet_token_t token,
                                      gasnet_handlerarg_t result_high,
                                      gasnet_handlerarg_t result_low,
                                      gasnet_handlerarg_t buf_high,
                                      gasnet_handlerarg_t buf_low)
    {
        T result = MAKEWORD(T, result_high, result_low);
        sync_var<T> *buf = MAKEWORD(sync_var<T> *, buf_high, buf_low);

        buf->set(result);
    }

    template <class T>
    void handle_fetch_and_add_i64_req(gasnet_token_t token,
                                      gasnet_handlerarg_t ptr_high,
                                      gasnet_handlerarg_t ptr_low,
                                      gasnet_handlerarg_t value_high,
                                      gasnet_handlerarg_t value_low,
                                      gasnet_handlerarg_t buf_high,
                                      gasnet_handlerarg_t buf_low)
    {
        T *p = MAKEWORD(T *, ptr_high, ptr_low);
        T v  = MAKEWORD(T, value_high, value_low);

        T result = threadsafe::fetch_and_add(p, v);

        uint32_t result_high = HIWORD(result);
        uint32_t result_low  = LOWORD(result);

        gasnet_AMReplyShort4(token, am_fad_rep_tag<T>(),
                             result_high, result_low, buf_high, buf_low);
    }

    template <class T>
    T am_fetch_and_add_i64(T *p, T value, int target)
    {
        sync_var<T> buf;

        uint32_t ptr_high = HIWORD(p);
        uint32_t ptr_low  = LOWORD(p);
        uint32_t value_high = HIWORD(value);
        uint32_t value_low  = LOWORD(value);
        uint32_t buf_high = HIWORD(&buf);
        uint32_t buf_low  = LOWORD(&buf);

        gasnet_AMRequestShort6(target, am_fad_req_tag<T>(),
                               ptr_high, ptr_low, value_high, value_low,
                               buf_high, buf_low);

        return buf.get();
    }

    template <class T>
    void handle_fetch_and_add_i32_rep(gasnet_token_t token,
                                      gasnet_handlerarg_t result_value,
                                      gasnet_handlerarg_t buf_high,
                                      gasnet_handlerarg_t buf_low)
    {
        T result = static_cast<T>(result_value);
        sync_var<T> *buf = MAKEWORD(sync_var<T> *, buf_high, buf_low);

        buf->set(result);
    }

    template <class T>
    void handle_fetch_and_add_i32_req(gasnet_token_t token,
                                      gasnet_handlerarg_t ptr_high,
                                      gasnet_handlerarg_t ptr_low,
                                      gasnet_handlerarg_t value,
                                      gasnet_handlerarg_t buf_high,
                                      gasnet_handlerarg_t buf_low)
    {
        T *p = MAKEWORD(T *, ptr_high, ptr_low);
        T v  = static_cast<T>(value);

        T result = threadsafe::fetch_and_add(p, v);

        uint32_t result_buf = static_cast<T>(result);

        gasnet_AMReplyShort3(token, am_fad_rep_tag<T>(),
                             result_buf, buf_high, buf_low);
    }

    template <class T>
    T am_fetch_and_add_i32(T *p, T value, int target)
    {
        sync_var<T> buf;

        uint32_t ptr_high = HIWORD(p);
        uint32_t ptr_low  = LOWORD(p);
        uint32_t v = static_cast<uint32_t>(value);
        uint32_t buf_high = HIWORD(&buf);
        uint32_t buf_low  = LOWORD(&buf);

        gasnet_AMRequestShort5(target, am_fad_req_tag<T>(),
                               ptr_high, ptr_low, v, buf_high, buf_low);

        return buf.get();
    }

    void handle_am(gasnet_token_t token, void *p, size_t size,
                   gasnet_handlerarg_t tag, gasnet_handlerarg_t pid)
    {
        g.comm->base().amhandle(tag, pid, p, size, (aminfo *)token);
    }

    void gasnet_barrier()
    {
        gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
        gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);
    }
  
    void * poll_thread_start(void *p)
    {
        poll_thread_arg& arg = *reinterpret_cast<poll_thread_arg *>(p);

        while (!arg.done)
            gasnet_AMPoll();

        return NULL;
    }

    comm_base::comm_base(int& argc, char **& argv, amhandler_t handler)
        : cm_(NULL)
        , comm_alc_(NULL)
        , native_config_()
        , handler_(handler)
#if MADI_GASNET_USE_IBV_ATOMIC
        , env_(NULL)
        , ep_(NULL)
        , value_buf_(NULL)
#else
        , poll_arg_()
#endif
    {
        if (GASNET_ALIGNED_SEGMENTS != 1) {
            MADI_SPMD_DIE("This library does not support GASNet "
                          "configured with GASNET_ALIGNED_SEGMENTS == 0.");
        }

        static_assert(MADI_COMM_GASNET_HANDLER_LAST == AM_HANDLE_LAST__,
                      "MADI_COMM_GASNET_HANDLER_LAST must be "
                      "same as AM_HANDLE_LAST");
        
        // # of handler entries is up to 256
        gasnet_handlerentry_t amentries[256] = {
            { AM_FAD_INT_REQ,
              (void (*)())handle_fetch_and_add_i32_req<int> },
            { AM_FAD_INT_REP,
              (void (*)())handle_fetch_and_add_i32_rep<int> },
            { AM_FAD_LONG_REQ,
              (void (*)())handle_fetch_and_add_i64_req<long> },
            { AM_FAD_LONG_REP,
              (void (*)())handle_fetch_and_add_i64_rep<long> },
            { AM_FAD_U64_REQ,
              (void (*)())handle_fetch_and_add_i64_req<uint64_t> },
            { AM_FAD_U64_REP,
              (void (*)())handle_fetch_and_add_i64_rep<uint64_t> },
            { AM_HANDLE_AM,
              (void (*)())handle_am },
        };
        int n_amentries = AM_HANDLE_LAST__ - 128;

        initialize_gasnet(argc, argv, amentries, n_amentries);
       
        gasnet_seginfo_t seginfo;
        gasnet_getSegmentInfo(&seginfo, 1);

        int init;
        MPI_Initialized(&init);

        if (!init)
            MPI_Init(&argc, &argv);

        int me = gasnet_mynode();
        int n_procs = gasnet_nodes();

        int mpi_me, mpi_n_procs;
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_me);
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_n_procs);

        MADI_CHECK(me == mpi_me);
        MADI_CHECK(n_procs == mpi_n_procs);

        native_config_ = make_unique<process_config>();

        cm_ = new comm_memory(seginfo.addr, seginfo.size, *native_config_);

        // initialize basic RDMA features (malloc/free/put/get)
        comm_alc_ = new allocator<comm_memory>(cm_);

#if MADI_GASNET_USE_IBV_ATOMIC
        env_ = new ibv::environment();

        void *seg_addr = seginfo.addr;
        size_t seg_size = seginfo.size;
        ep_ = new ibv::prepinned_endpoint(*env_, seg_addr, seg_size);

        value_buf_ = (uint64_t *)comm_alc_->allocate(sizeof(uint64_t),
                                                     *native_config_);
#else
        if (options.gasnet_poll_thread) {

#ifndef GASNET_PAR
            MADI_SPMD_DIE("polling thread for GASNet requires GASNET_PAR");
#endif

            pthread_create(&poll_thread_, NULL,
                           poll_thread_start,
                           reinterpret_cast<void *>(&poll_arg_));
        }
#endif
    }

    comm_base::~comm_base()
    {
#if MADI_GASNET_USE_IBV_ATOMIC
        comm_alc_->deallocate((void *)value_buf_);
        delete ep_;
        delete env_;
#else
        if (options.gasnet_poll_thread) {
            poll_arg_.done = 1;
            pthread_join(poll_thread_, NULL);
        }
#endif

        delete comm_alc_;
        delete cm_;

        int init;
        MPI_Initialized(&init);

#if 0
        if (init)
            MPI_Finalize();
#else
        // FIXME: GASNet returns non-zero exit code if a program terminated
        // without gasnet_exit.
        gasnet_exit(0);
#endif
    }

    void ** comm_base::coll_malloc(size_t size, process_config& config)
    {
        int n_procs = config.get_n_procs();
        MPI_Comm comm = config.comm();
        comm_allocator *alc = comm_alc_;

        void **ptrs = new void *[n_procs];

        void *p = alc->allocate(size, config);

        MADI_ASSERT(p != NULL);

        if (p == NULL) {
            MADI_DPUTS("coll_malloc cannot allocate memory.");
            return NULL;
        }

        MPI_Allgather(&p, sizeof(p), MPI_BYTE, 
                      ptrs, sizeof(p), MPI_BYTE,
                      comm);

        MADI_UNUSED int me = config.get_pid();
        MADI_ASSERT(ptrs[me] != NULL);

        return ptrs;
    }

    void comm_base::coll_free(void **ptrs, process_config& config)
    {
        if (ptrs == NULL)
            return;

        int me = config.get_pid();
        comm_allocator *alc = comm_alc_;

        alc->deallocate(ptrs[me]);

        delete [] ptrs;
    }

    void * comm_base::malloc(size_t size, process_config& config)
    {
//         MPI_Comm comm = config.comm();

        // FIXME: 1. non-collective implementation
        // currently, the comm_allocater::allocate function may call
        // collective function `extend_to'.
 
        // FIXME: 2. Use GASNet barrier internally calling gasnet_AMPoll()
        //MPI_Barrier(comm);
        gasnet_barrier();

        comm_allocator *alc = comm_alc_;
        return alc->allocate(size, config);
    }

    void comm_base::free(void *p, process_config& config)
    {
//         MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        // FIXME: Use GASNet barrier internally calling gasnet_AMPoll()
        //MPI_Barrier(comm);
        gasnet_barrier();

        comm_allocator *alc = comm_alc_;
        alc->deallocate(p);
    }

    int comm_base::coll_mmap(uint8_t *addr, size_t size, process_config& config)
    {
        return -1;
    }

    void comm_base::coll_munmap(int memid, process_config& config)
    {
        MADI_UNDEFINED;
    }

    void comm_base::put_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        gasnet_put_bulk(target, dst, src, size);
    }

    void comm_base::reg_put_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        put_nbi(dst, src, size, target, config);
    }

    void comm_base::get_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        gasnet_get_bulk(dst, target, src, size);
    }

    void comm_base::reg_get_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        get_nbi(dst, src, size, target, config);
    }

    int comm_base::poll(int *tag_out, int *pid_out, process_config& config)
    {
#if MADI_GASNET_USE_IBV_ATOMIC
        gasnet_AMPoll();
#else
        if (!options.gasnet_poll_thread)
            gasnet_AMPoll();
#endif
        return 0;
    }

    void comm_base::fence()
    {
        // do nothing
    }

    void comm_base::native_barrier(process_config& config)
    {
        //MPI_Comm comm = config.comm();
        //MPI_Barrier(comm);
        gasnet_barrier();
    }

    template <class T>
    void comm_base::put_value(T *dst, T value, int target,
                              process_config& config)
    {
        static_assert(sizeof(T) == sizeof(uint32_t) ||
                      sizeof(T) == sizeof(uint64_t),
                      "T must be a 32 or 64 bit type");

        gasnet_put_val(target, dst,
                       static_cast<gasnet_register_value_t>(value),
                       sizeof(T));
    }

    template <class T>
    T comm_base::get_value(T *src, int target, process_config& config)
    {
        static_assert(sizeof(T) == sizeof(uint32_t) ||
                      sizeof(T) == sizeof(uint64_t),
                      "T must be a 32 or 64 bit type");

        gasnet_register_value_t v = gasnet_get_val(target, src, sizeof(T));

        return static_cast<T>(v);   
    }

    template <class T>
    T comm_base::fetch_and_add(T *dst, T value, int target,
                               process_config& config)
    {
#if MADI_GASNET_USE_IBV_ATOMIC
        static_assert(sizeof(T) == sizeof(uint64_t),
                      "T must be a 64 bit type");

        MADI_ASSERTP1((value & ~0xFFFFFFFF) == 0, value);

        ep_->rdma_fetch_and_add(dst, value_buf_, (uint32_t)value, target);

        return (T)*value_buf_;
#else
        static_assert(sizeof(T) == sizeof(int32_t) ||
                      sizeof(T) == sizeof(int64_t),
                      "T must be a 32 or 64 bit type");

        if (sizeof(T) == sizeof(int32_t))
            return am_fetch_and_add_i32(dst, value, target);
        else if (sizeof(T) == sizeof(int64_t))
            return am_fetch_and_add_i64(dst, value, target);
#endif
    }

    void comm_base::request(int tag, void *p, size_t size, int pid,
                            process_config& config)
    {
        gasnet_AMRequestMedium2(pid, AM_HANDLE_AM, p, size, tag, pid);
    }

    void comm_base::reply(int tag, void *p, size_t size, aminfo *info,
                          process_config& config)
    {
        int pid = madi::comm::get_pid();
        gasnet_AMReplyMedium2((gasnet_token_t)info, AM_HANDLE_AM, p, size,
                              tag, pid);
    }


    // template instantiation for put_value
    template void comm_base::put_value(int *, int, int, process_config&);
    template void comm_base::put_value(long *, long, int, process_config&);
    template void comm_base::put_value(unsigned int *, unsigned int,
                                       int, process_config&);
    template void comm_base::put_value(unsigned long *, unsigned long,
                                       int, process_config&);
    template void comm_base::put_value(unsigned long long *,
                                       unsigned long long,
                                       int, process_config&);

    // template instantiation for get_value
    template int
        comm_base::get_value(int *src, int target,
                             process_config& config);
    template long
        comm_base::get_value(long *src, int target,
                             process_config& config);
    template unsigned int
        comm_base::get_value(unsigned int *src, int target,
                             process_config& config);
    template unsigned long 
        comm_base::get_value(unsigned long *src, int target,
                             process_config& config);
    template unsigned long long 
        comm_base::get_value(unsigned long long *src,int target,
                             process_config& config);


    // template instantiation for fetch_and_add
//     template int comm_base::fetch_and_add<int>(int *, int, int,
//                                                process_config&);
//     template long comm_base::fetch_and_add<long>(long *, long, int,
//                                                  process_config&);
    template uint64_t comm_base::fetch_and_add<uint64_t>(uint64_t*, uint64_t,
                                                         int, process_config&);

    // template instantiation for handle_fetch_and_add_*_{rep,req}
    template void handle_fetch_and_add_i32_req<int>(gasnet_token_t,
                                                    gasnet_handlerarg_t,
                                                    gasnet_handlerarg_t,
                                                    gasnet_handlerarg_t,
                                                    gasnet_handlerarg_t,
                                                    gasnet_handlerarg_t);
    template void handle_fetch_and_add_i32_rep<int>(gasnet_token_t,
                                                    gasnet_handlerarg_t,
                                                    gasnet_handlerarg_t,
                                                    gasnet_handlerarg_t);
    template void handle_fetch_and_add_i64_req<long>(gasnet_token_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t);
    template void handle_fetch_and_add_i64_req<unsigned long>(gasnet_token_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t);
    template void handle_fetch_and_add_i64_rep<long>(gasnet_token_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t);
    template void handle_fetch_and_add_i64_rep<unsigned long>(gasnet_token_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t,
                                                     gasnet_handlerarg_t);
}
}


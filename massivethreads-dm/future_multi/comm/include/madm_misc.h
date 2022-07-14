#ifndef MADM_MISC_H
#define MADM_MISC_H

#include <memory>
#include <tuple>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <sstream>
#include <iostream>

#include <sys/time.h>

#define MADI_NORETURN    __attribute__((noreturn))
#define MADI_UNUSED      __attribute__((unused))
#define MADI_NOINLINE    __attribute__((noinline))

// noncopyable attribute suppressing -Weff++
#define MADI_NONCOPYABLE(name) \
    name(const name&); \
    name& operator=(const name&)


extern "C" {
    void madi_exit(int) MADI_NORETURN;
    size_t madi_get_debug_pid();
}

namespace madi {

    // utility class similar to boost::noncopyable
    struct noncopyable {
    protected:
        noncopyable() = default;
        ~noncopyable() = default;
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
    };

    // apply tuple elements to a function
    template <size_t N, class R>
    struct tapply {
        template <class F, class... Args, class... FArgs>
        static R f(F f, std::tuple<Args...> t, FArgs... args) {
            return tapply<N-1, R>::f(f, t, std::get<N-1>(t), args...);
        }
    };
    template <>
    struct tapply<0, void> {
        template <class F, class... Args, class... FArgs>
        static void f(F f, MADI_UNUSED std::tuple<Args...> t, FArgs... args) {
            f(args...);
        }
    };
    template <class R>
    struct tapply<0, R> {
       template <class F, class... Args, class... FArgs>
       static R f(F f, MADI_UNUSED std::tuple<Args...> t, FArgs... args) {
           return f(args...);
       }
    };
    template <size_t N>
    struct tapply<N, void> {
        template <class F, class... Args, class... FArgs>
        static void f(F f, std::tuple<Args...> t, FArgs... args) {
            tapply<N-1, void>::f(f, t, std::get<N-1>(t), args...);
        }
    };
    template <class R>
    struct tuple_apply {
        template <class F, class... Args>
        static R f(F f, std::tuple<Args...> t) {
            return tapply<sizeof...(Args), R>::f(f, t);
        }
    };
    template <>
    struct tuple_apply<void> {
        template <class F, class... Args>
        static void f(F f, std::tuple<Args...> t) {
            tapply<sizeof...(Args), void>::f(f, t);
        }
    };

    typedef long tsc_t;

    static inline tsc_t rdtsc(void)
    {
#if (defined __i386__) || (defined __x86_64__)
        uint32_t hi,lo;
        asm volatile("lfence\nrdtsc" : "=a"(lo),"=d"(hi));
        return (tsc_t)((uint64_t)hi)<<32 | lo;
#elif (defined __sparc__) && (defined __arch64__)
        uint64_t tick;
        asm volatile("rd %%tick, %0" : "=r" (tick));
        return (tsc_t)tick;
#elif defined(__aarch64__)
        uint64_t tick;
        asm volatile("isb\nmrs %0, cntvct_el0" : "=r" (tick));
        return (tsc_t)tick;
#else
#warning "rdtsc() is not implemented."
        return 0;
#endif
    }

    static inline tsc_t rdtsc_nofence(void)
    {
#if (defined __i386__) || (defined __x86_64__)
        uint32_t hi,lo;
        asm volatile("rdtsc" : "=a"(lo),"=d"(hi));
        return (tsc_t)((uint64_t)hi)<<32 | lo;
#elif (defined __sparc__) && (defined __arch64__)
        uint64_t tick;
        asm volatile("rd %%tick, %0" : "=r" (tick));
        return (tsc_t)tick;
#elif defined(__aarch64__)
        uint64_t tick;
        asm volatile("mrs %0, cntvct_el0" : "=r" (tick));
        return (tsc_t)tick;
#else
#warning "rdtsc() is not implemented."
        return 0;
#endif
    }

    static inline double now(void)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (double)tv.tv_sec + (double)tv.tv_usec*1e-6;
    }

    inline int random_int(int n)
    {
        assert(n > 0);

        size_t rand_max =
            ((size_t)RAND_MAX + 1) - ((size_t)RAND_MAX + 1) % (size_t)n;
        int r;
        do {
            r = rand();
        } while ((size_t)r >= rand_max);

        return (int)((double)n * (double)r / (double)rand_max);
    }

    void die(const char *s) MADI_NORETURN;

    inline void die(const char *s)
    {
        fprintf(stderr, "MassiveThreads/DM fatal error: %s\n", s);
        madi_exit(1);
    }

#define MADI_DIE(fmt, ...)                                              \
    do {                                                                \
        fprintf(stderr,                                                 \
                "MassiveThreads/DM fatal error: " fmt " (%s:%d)\n",     \
                ##__VA_ARGS__, __FILE__, __LINE__);                     \
        fflush(stderr);                                                 \
        madi_exit(1);                                                   \
    } while (false)

#define MADI_SPMD_DIE(fmt, ...)                         \
    do {                                                \
        int me = madi_get_debug_pid();                  \
        if (me == 0)                                    \
            MADI_DIE(fmt, ##__VA_ARGS__);               \
        /*else                                          \
            madi_exit(1);*/                             \
    } while (false)

#define MADI_PERR_DIE(str) \
    MADI_DIE("`%s' failed with error `%s'", (str), strerror(errno))

#define MADI_SPMD_PERR_DIE(str) \
    MADI_SPMD_DIE("`%s' failed with error `%s'", (str), strerror(errno))

    template <typename T>
    T get_env_(const char* env_var, T default_val) {
        if (const char* val_str = std::getenv(env_var)) {
            T val;
            std::stringstream ss(val_str);
            ss >> val;
            if (ss.fail()) {
                fprintf(stderr, "Environment variable '%s' is invalid.\n", env_var);
                exit(1);
            }
            return val;
        } else {
            return default_val;
        }
    }

    template <typename T>
    T get_env(const char* env_var, T default_val) {
        static bool print_env = get_env_("MADM_PRINT_ENV", false);

        T val = get_env_(env_var, default_val);
        if (print_env && madi_get_debug_pid() == 0) {
            std::cout << env_var << " = " << val << std::endl;
        }
        return val;
    }
}

#endif

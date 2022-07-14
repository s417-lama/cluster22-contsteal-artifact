#ifndef MADM_DEBUG_H
#define MADM_DEBUG_H

#include "madm/madm_comm_config.h"
#include "madm/options.h"
#include "madm_misc.h"

#include <string>
#include <sstream>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

#define MADI_DEBUG_RUNTIME(level) \
    ((level) <= ::madi::comm::options.debug_level)


#define MADI_DEBUG1(stmt)       if (MADI_DEBUG_RUNTIME(1)) stmt
#define MADI_DEBUG2(stmt)       if (MADI_DEBUG_RUNTIME(2)) stmt
#define MADI_DEBUG3(stmt)       if (MADI_DEBUG_RUNTIME(3)) stmt

#ifndef MADI_DEBUG_LEVEL
#define MADI_DEBUG_LEVEL 0
#endif

#if MADI_DEBUG_LEVEL < 1
#undef MADI_DEBUG1
#define MADI_DEBUG1(stmt)
#endif

#if MADI_DEBUG_LEVEL < 2
#undef MADI_DEBUG2
#define MADI_DEBUG2(stmt)
#endif

#if MADI_DEBUG_LEVEL < 3
#undef MADI_DEBUG3
#define MADI_DEBUG3(stmt)
#endif


// debug printers
#define MADI_DPRINT_RAW(type, pid, tid, format, ...)                    \
    madi_dprint_raw("%-2s:%5ld:%2ld: %-30.30s :: " format "\n",         \
                     (type), (pid), (tid), __FUNCTION__,         \
                     ##__VA_ARGS__)

#define MADI_DPRINT(type, ...)                                          \
      MADI_DPRINT_RAW((type),                                           \
                      madi_get_debug_pid(),                             \
                      0,                                                \
                      __VA_ARGS__)                                      \

#define MADI_COLOR_     39
#define MADI_COLOR_R    31
#define MADI_COLOR_G    32
#define MADI_COLOR_Y    33
#define MADI_COLOR_B    34
#define MADI_COLOR_P    35

#define MADI_PRI_COLOR_BEGIN(color_no)    "\x1b[" #color_no "m"
#define MADI_PRI_COLOR_END()              MADI_PRI_COLOR_BEGIN(39)

#define MADI_DPRINT_COLOR(color_no, type, format, ...)          \
    MADI_DPRINT((type),                                         \
                "\x1b[%dm" format "\x1b[39m",                   \
                MADI_COLOR_##color_no,                          \
                ##__VA_ARGS__)

#define MADI_DPRINTR(...)       MADI_DPRINT_COLOR(R, ##__VA_ARGS__)
#define MADI_DPRINTG(...)       MADI_DPRINT_COLOR(G, ##__VA_ARGS__)
#define MADI_DPRINTY(...)       MADI_DPRINT_COLOR(Y, ##__VA_ARGS__)
#define MADI_DPRINTB(...)       MADI_DPRINT_COLOR(B, ##__VA_ARGS__)
#define MADI_DPRINTP(...)       MADI_DPRINT_COLOR(P, ##__VA_ARGS__)

#define MADI_DPUTS_COLOR(suffix, level, format, ...)                    \
    do {                                                                \
        if (MADI_DEBUG_RUNTIME(level)) {                                \
            MADI_DPRINT_COLOR(suffix, "D" #level, format, ##__VA_ARGS__); \
        }                                                               \
    } while (0)

#define MADI_DPUTS(...)     MADI_DPUTS_COLOR(,  0, __VA_ARGS__)
#define MADI_DPUTSR(...)    MADI_DPUTS_COLOR(R, 0, __VA_ARGS__)
#define MADI_DPUTSG(...)    MADI_DPUTS_COLOR(G, 0, __VA_ARGS__)
#define MADI_DPUTSY(...)    MADI_DPUTS_COLOR(Y, 0, __VA_ARGS__)
#define MADI_DPUTSB(...)    MADI_DPUTS_COLOR(B, 0, __VA_ARGS__)
#define MADI_DPUTSP(...)    MADI_DPUTS_COLOR(P, 0, __VA_ARGS__)

#if (MADI_DEBUG_LEVEL >= 0)
#define MADI_DPUTS0(...)     MADI_DPUTS_COLOR(,  0, __VA_ARGS__)
#define MADI_DPUTSR0(...)    MADI_DPUTS_COLOR(R, 0, __VA_ARGS__)
#define MADI_DPUTSG0(...)    MADI_DPUTS_COLOR(G, 0, __VA_ARGS__)
#define MADI_DPUTSY0(...)    MADI_DPUTS_COLOR(Y, 0, __VA_ARGS__)
#define MADI_DPUTSB0(...)    MADI_DPUTS_COLOR(B, 0, __VA_ARGS__)
#define MADI_DPUTSP0(...)    MADI_DPUTS_COLOR(P, 0, __VA_ARGS__)
#else
#define MADI_DPUTS1(...)
#define MADI_DPUTSR1(...) 
#define MADI_DPUTSG1(...)
#define MADI_DPUTSY1(...)
#define MADI_DPUTSB1(...)
#define MADI_DPUTSP1(...)
#endif

#if (MADI_DEBUG_LEVEL >= 1)
#define MADI_DPUTS1(...)     MADI_DPUTS_COLOR(,  1, __VA_ARGS__)
#define MADI_DPUTSR1(...)    MADI_DPUTS_COLOR(R, 1, __VA_ARGS__)
#define MADI_DPUTSG1(...)    MADI_DPUTS_COLOR(G, 1, __VA_ARGS__)
#define MADI_DPUTSY1(...)    MADI_DPUTS_COLOR(Y, 1, __VA_ARGS__)
#define MADI_DPUTSB1(...)    MADI_DPUTS_COLOR(B, 1, __VA_ARGS__)
#define MADI_DPUTSP1(...)    MADI_DPUTS_COLOR(P, 1, __VA_ARGS__)
#else
#define MADI_DPUTS1(...)
#define MADI_DPUTSR1(...) 
#define MADI_DPUTSG1(...)
#define MADI_DPUTSY1(...)
#define MADI_DPUTSB1(...)
#define MADI_DPUTSP1(...)
#endif

#if (MADI_DEBUG_LEVEL >= 2)
#define MADI_DPUTS2(...)     MADI_DPUTS_COLOR(,  2, __VA_ARGS__)
#define MADI_DPUTSR2(...)    MADI_DPUTS_COLOR(R, 2, __VA_ARGS__)
#define MADI_DPUTSG2(...)    MADI_DPUTS_COLOR(G, 2, __VA_ARGS__)
#define MADI_DPUTSY2(...)    MADI_DPUTS_COLOR(Y, 2, __VA_ARGS__)
#define MADI_DPUTSB2(...)    MADI_DPUTS_COLOR(B, 2, __VA_ARGS__)
#define MADI_DPUTSP2(...)    MADI_DPUTS_COLOR(P, 2, __VA_ARGS__)
#else
#define MADI_DPUTS2(...)
#define MADI_DPUTSR2(...) 
#define MADI_DPUTSG2(...)
#define MADI_DPUTSY2(...)
#define MADI_DPUTSB2(...)
#define MADI_DPUTSP2(...)
#endif

#if (MADI_DEBUG_LEVEL >= 3)
#define MADI_DPUTS3(...)     MADI_DPUTS_COLOR(,  3, __VA_ARGS__)
#define MADI_DPUTSR3(...)    MADI_DPUTS_COLOR(R, 3, __VA_ARGS__)
#define MADI_DPUTSG3(...)    MADI_DPUTS_COLOR(G, 3, __VA_ARGS__)
#define MADI_DPUTSY3(...)    MADI_DPUTS_COLOR(Y, 3, __VA_ARGS__)
#define MADI_DPUTSB3(...)    MADI_DPUTS_COLOR(B, 3, __VA_ARGS__)
#define MADI_DPUTSP3(...)    MADI_DPUTS_COLOR(P, 3, __VA_ARGS__)
#else
#define MADI_DPUTS3(...)
#define MADI_DPUTSR3(...) 
#define MADI_DPUTSG3(...)
#define MADI_DPUTSY3(...)
#define MADI_DPUTSB3(...)
#define MADI_DPUTSP3(...)
#endif


#if (MADI_DEBUG_LEVEL >= 1)
#define MADI_ASSERT(cond)                                               \
    do {                                                                \
        if (MADI_DEBUG_RUNTIME(1) && !(cond)) {                         \
            MADI_DPRINTR("E", "assertion `" #cond "' failed at %s:%d\n", \
                         __FILE__, __LINE__);                           \
            madi_exit(1);                                               \
        }                                                               \
    } while (0)
#else
#define MADI_ASSERT(cond)
#endif

#if (MADI_DEBUG_LEVEL >= 1)
#define MADI_ASSERTP1(cond, v)                                          \
    do {                                                                \
        if (MADI_DEBUG_RUNTIME(1) && !(cond)) {                                                  \
            std::string s = to_string(v);                               \
            MADI_DPRINTR("E", "assertion `" #cond "' "                  \
                         "(" #v " = %s) failed at %s:%d\n",             \
                         ::madi::to_string(v).data(),                   \
                         __FILE__, __LINE__);                           \
            madi_exit(1);                                               \
        }                                                               \
    } while (0)

#define MADI_ASSERTP2(cond, v0, v1)                                     \
    do {                                                                \
        if (MADI_DEBUG_RUNTIME(2) && !(cond)) {                                                  \
            MADI_DPRINTR("E", "assertion `" #cond "' "                  \
                         "(" #v0 " = %s," #v1 " = %s) "                 \
                         "failed at %s:%d\n",                           \
                         ::madi::to_string(v0).data(),                  \
                         ::madi::to_string(v1).data(),                  \
                         __FILE__, __LINE__);                           \
            madi_exit(1);                                               \
        }                                                               \
    } while (0)

#define MADI_ASSERTP3(cond, v0, v1, v2)                                 \
    do {                                                                \
        if (MADI_DEBUG_RUNTIME(3) && !(cond)) {                                                  \
            MADI_DPRINTR("E", "assertion `" #cond "' "                  \
                         "(" #v0 " = %s," #v1 " = %s, " #v2 " = %s) "   \
                         "failed at %s:%d\n",                           \
                         ::madi::to_string(v0).data(),                  \
                         ::madi::to_string(v1).data(),                  \
                         ::madi::to_string(v2).data(),                  \
                         __FILE__, __LINE__);                           \
            madi_exit(1);                                               \
        }                                                               \
    } while (0)

#define MADI_ASSERTP4(cond, v0, v1, v2, v3)                             \
    do {                                                                \
        if (MADI_DEBUG_RUNTIME(4) && !(cond)) {                                                  \
            MADI_DPRINTR("E", "assertion `" #cond "' "                  \
                         "(" #v0 " = %s," #v1 " = %s, " #v2 " = %s, "   \
                             #v3 " = %s) "                              \
                         "failed at %s:%d\n",                           \
                         ::madi::to_string(v0).data(),                  \
                         ::madi::to_string(v1).data(),                  \
                         ::madi::to_string(v2).data(),                  \
                         ::madi::to_string(v3).data(),                  \
                         __FILE__, __LINE__);                           \
            madi_exit(1);                                               \
        }                                                               \
    } while (0)

#define MADI_ASSERTP5(cond, v0, v1, v2, v3, v4)                         \
    do {                                                                \
        if (MADI_DEBUG_RUNTIME(5) && !(cond)) {                                                  \
            MADI_DPRINTR("E", "assertion `" #cond "' "                  \
                         "(" #v0 " = %s," #v1 " = %s, " #v2 " = %s, "   \
                             #v3 " = %s, " #v4 " = %s) "                \
                         "failed at %s:%d\n",                           \
                         ::madi::to_string(v0).data(),                  \
                         ::madi::to_string(v1).data(),                  \
                         ::madi::to_string(v2).data(),                  \
                         ::madi::to_string(v3).data(),                  \
                         ::madi::to_string(v4).data(),                  \
                         __FILE__, __LINE__);                           \
            madi_exit(1);                                               \
        }                                                               \
    } while (0)

#else
#define MADI_ASSERTP1(cond, v0)
#define MADI_ASSERTP2(cond, v0, v1)
#define MADI_ASSERTP3(cond, v0, v1, v2)
#define MADI_ASSERTP4(cond, v0, v1, v2, v3)
#define MADI_ASSERTP5(cond, v0, v1, v2, v3, v4)
#endif

#define MADI_CHECK(cond)                                                \
    do {                                                                \
        if (!(cond)) {                                                  \
        MADI_DPRINTR("E", "check `" #cond "' failed at %s:%d\n",        \
                     __FILE__, __LINE__);                               \
        madi_exit(1);                                                   \
        }                                                               \
    } while (0)

#define MADI_NOT_REACHED                                                \
    do {                                                                \
        MADI_DPRINTR("E", "function %s (%s:%d) not reached",            \
                     __PRETTY_FUNCTION__, __FILE__, __LINE__);          \
        madi_exit(1);                                                   \
    } while (0)

#define MADI_UNDEFINED                                           \
    do {                                                         \
        MADI_DPRINTR("E", "function %s (%s:%d) not implemented", \
                     __PRETTY_FUNCTION__, __FILE__, __LINE__);   \
        madi_exit(1);                                            \
    } while (0)

#define MADI_DP(value) \
    ::madi::dprint(value, #value, __FUNCTION__)


extern "C" {
    int madi_initialized();
    size_t madi_get_debug_pid();
    void madi_exit(int exitcode) MADI_NORETURN;
    int madi_dprint_raw(const char *format, ...);
}

namespace madi {

    template <class T>
    inline std::string to_string(T v)
    {
        return std::to_string(v);
    }

    template <class T>
    inline std::string to_string(T *p)
    {
        std::stringstream ss;
        ss << reinterpret_cast<void *>(p);
        return ss.str();
    }

    template <class T>
    inline int dprint(T v, const char *expstr, const char *func);

#define MADI_DEFINE_DP(type_params, type, fmt) \
    template <type_params> \
    inline int dprint(type v, const char *expstr, const char *func) \
    { \
        size_t pid = madi_get_debug_pid(); \
        size_t tid = 0; \
        madi_dprint_raw("P :%5zu:%2zu: %-30.30s :: %s = " fmt "\n", \
                        pid, tid, func, expstr, v); \
        return 0; \
    }

    MADI_DEFINE_DP(class T, T *, "%p")
    MADI_DEFINE_DP(, int, "%d")
    MADI_DEFINE_DP(, size_t, "%zu")
    MADI_DEFINE_DP(, char *, "%s")
//    MADI_DEFINE_DP(, uintptr_t, "%lu")
}

#endif

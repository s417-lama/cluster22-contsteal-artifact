#ifndef MADI_COMM_THREADSAFE_H
#define MADI_COMM_THREADSAFE_H

#include "madm_misc.h"

namespace madi {
namespace comm {

    struct threadsafe : private noncopyable {

        static bool compare_and_swap(volatile int *dst, int old_v, int new_v)
        {
            return __sync_bool_compare_and_swap(dst, old_v, new_v);
        }

        static void swap(volatile int *dst, int *src)
        {
            int new_v = *src;

            for (;;) {
                int old_v = *dst;

                if (compare_and_swap(dst, old_v, new_v))
                    break;
            }
        }

        template <class T>
        static T fetch_and_add(T *dst, T value)
        {
            return __sync_fetch_and_add(dst, value);
        }

        template <class T>
        static T fetch_and_add(volatile T *dst, T value)
        {
            return __sync_fetch_and_add(dst, value);
        }

#ifdef __x86_64__
        static void rbarrier()
        {
            // R->R orderings are guaranteed by x86-64
            asm volatile("":::"memory");
            //asm volatile ("lfence\n\t":::"memory");
        }

        static void wbarrier()
        {
            // W->W orderings are guaranteed by x86-64
            asm volatile("":::"memory");
            //asm volatile ("sfence\n\t":::"memory");
        }

        static void rwbarrier()
        {
            // R->W orderings are NOT guaranteed.
            // Need to serialize by atomic or fence insns
#if 1
            int x, y = 0;
            asm volatile("xchgl %0, %1\n\t"
                         : "=r"(x), "=m"(y)
                         :
                         : "memory");
#else
            asm volatile ("mfence\n\t":::"memory");
#endif
        }
#elif (defined __sparc__) && (defined __arch64__)
        static void rbarrier()
        {
            // R->R orderings are guaranteed by SPARC V9 (TSO)
            asm volatile("":::"memory");
        }

        static void wbarrier()
        {
            // W->W orderings are guaranteed by SPARC V9 (TSO)
            asm volatile("":::"memory");
        }

        static void rwbarrier()
        {
            // R->W orderings are NOT guaranteed by SPARC V9 (TSO)
            __sync_synchronize();
        }
#elif defined(__aarch64__)
        static void rbarrier()
        {
            asm volatile("dmb ishld":::"memory");
        }

        static void wbarrier()
        {
            asm volatile("dmb ishst":::"memory");
        }

        static void rwbarrier()
        {
            asm volatile("dmb ish":::"memory");
        }
#else
        static void rbarrier()
        {
            __sync_synchronize();
        }

        static void wbarrier()
        {
            __sync_synchronize();
        }

        static void rwbarrier()
        {
            __sync_synchronize();
        }
#endif
    };

}
}

#endif

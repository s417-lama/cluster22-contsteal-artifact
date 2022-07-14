#ifndef MADI_CONTEXT_X86_64_H
#define MADI_CONTEXT_X86_64_H

#include "../uth_config.h"
#include "../debug.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace madi {

#if   MADI_OS_TYPE == MADI_OS_LINUX

/* #define MADI_IP_MIN             ((uint8_t *)0x400000) */
/* #define MADI_IP_MAX             ((uint8_t *)0x1000000) */

#else
#error ""
#endif


struct context {
    void *fp;
    void *lr;
    void *top;
    context *parent;

    uint8_t *instr_ptr() const {
        return reinterpret_cast<uint8_t *>(lr);
    }

    uint8_t *stack_ptr() const {
        return reinterpret_cast<uint8_t *>(top);
    }

    uint8_t *top_ptr() const {
        return reinterpret_cast<uint8_t *>(top);
    }

    size_t stack_size() const {
        size_t size = (uint8_t *)parent + sizeof(context) - top_ptr();

        MADI_ASSERTP1(0 < size && size < 1024 * 1024, size);

        return size;
    }

    void print() const {
        MADI_DPUTS("FP = %p", fp);
        MADI_DPUTS("LR = %p", lr);
        MADI_DPUTS("stack_top = %p", top);
        MADI_DPUTS("parent = %p", parent);
    }
};

#define MADI_CONTEXT_PRINT(level, ctx) \
    do { \
        MADI_DPUTS##level("(" #ctx ")         = %p", (ctx)); \
        MADI_DPUTS##level("(" #ctx ")->FP     = %p", (ctx)->fp); \
        MADI_DPUTS##level("(" #ctx ")->LR     = %p", (ctx)->lr); \
        MADI_DPUTS##level("(" #ctx ")->top    = %p", (ctx)->top); \
        MADI_DPUTS##level("(" #ctx ")->parent = %p", (ctx)->parent); \
    } while (false)

#define MADI_GET_CURRENT_SP(ptr_sp)                             \
    do {                                                        \
        uint8_t *sp__ = NULL;                                   \
        asm volatile("mov %0, sp\n" : "=r"(sp__));              \
        *(ptr_sp) = sp__;                                       \
    } while (false)

#define MADI_GET_CURRENT_STACK_TOP(ptr_sp)                      \
    do {                                                        \
        MADI_GET_CURRENT_SP(ptr_sp);                            \
    } while (false)

#ifdef __ARM_FEATURE_SVE
#define MADI_FLOAT_CLOBBERS                                 \
    "p0" , "p1" , "p2" , "p3" , "p4" , "p5" , "p6" , "p7" , \
    "p8" , "p9" , "p10", "p11", "p12", "p13", "p14", "p15", \
    "z0" , "z1" , "z2" , "z3" , "z4" , "z5" , "z6" , "z7" , \
    "z8" , "z9" , "z10", "z11", "z12", "z13", "z14", "z15", \
    "z16", "z17", "z18", "z19", "z20", "z21", "z22", "z23", \
    "z24", "z25", "z26", "z27", "z28", "z29", "z30", "z31"
#else
#define MADI_FLOAT_CLOBBERS                                 \
    "v0" , "v1" , "v2" , "v3" , "v4" , "v5" , "v6" , "v7" , \
    "v8" , "v9" , "v10", "v11", "v12", "v13", "v14", "v15", \
    "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", \
    "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"
#endif

#ifdef __FUJITSU
// FIXME: Fujitsu compiler (v1.2.31) generates the illegal instruction
//            ldp     x19, x19, [x29, #-16]
//        if we specify x19 in the clobbered list.
//        As a workaround, we save x19 in the stack explicitly
#define MADI_SAVE_R19    "stp x0, x19, [sp, #-16]!\n\t"
#define MADI_RESTORE_R19 "ldp x0, x19, [sp], #16\n\t"
#define MADI_CLOBBER_R19
#else
#define MADI_SAVE_R19
#define MADI_RESTORE_R19
#define MADI_CLOBBER_R19 "x19",
#endif

#define MADI_SAVE_CONTEXT_WITH_CALL(parent_ctx_ptr, f, arg0, arg1)           \
    do {                                                                     \
        register void* parent_ctx_x9 asm("x9")  = (void*)(parent_ctx_ptr);   \
        register void* f_x10         asm("x10") = (void*)(f);                \
        register void* arg0_x1       asm("x1")  = (void*)(arg0);             \
        register void* arg1_x2       asm("x2")  = (void*)(arg1);             \
        asm volatile (                                                       \
            MADI_SAVE_R19                                                    \
            /* stack top and parent field of context */                      \
            "sub x0, sp, #32\n\t"                                            \
            "stp x0, %0, [sp, #-16]!\n\t"                                    \
            /* save FP (r29) and LR (r30) */                                 \
            "adr x30, 1f\n\t"                                                \
            "stp x29, x30, [sp, #-16]!\n\t"                                  \
            /* call function */                                              \
            "mov x0, sp\n\t"                                                 \
            "blr %1\n\t"                                                     \
            /* skip saved FP and LR when normally returned */                \
            "add sp, sp, #16\n\t"                                            \
                                                                             \
            "1:\n\t"                                                         \
            /* skip parent field */                                          \
            "add sp, sp, #16\n\t"                                            \
            MADI_RESTORE_R19                                                 \
            : "+r"(parent_ctx_x9), "+r"(f_x10), "+r"(arg0_x1), "+r"(arg1_x2) \
            :                                                                \
            : "x0", "x3", "x4", "x5", "x6", "x7",                            \
              "x8", "x11", "x12", "x13", "x14", "x15",                       \
              "x16", "x17", "x18", MADI_CLOBBER_R19 "x20", "x21", "x22", "x23", \
              "x24", "x25", "x26", "x27", "x28", "x29", "x30",               \
              MADI_FLOAT_CLOBBERS,                                           \
              "cc", "memory");                                               \
    } while (false)

#define MADI_RESUME_CONTEXT(ctx)                                             \
    do {                                                                     \
        asm volatile (                                                       \
            "mov sp, %0\n\t"                                                 \
            "ldp x29, x30, [sp], #16\n\t"                                    \
            "ret\n\t"                                                        \
            :                                                                \
            : "r"(ctx)                                                       \
            :);                                                              \
    } while (false)

#define MADI_EXECUTE_ON_STACK(f, arg0, arg1, arg2, arg3, stack_ptr)          \
    do {                                                                     \
        uint8_t *top__ = NULL;                                               \
        MADI_GET_CURRENT_SP(&top__);                                         \
                                                                             \
        uint8_t *stack__ = (uint8_t *)(stack_ptr);                           \
        uint8_t *smaller_top__ = (top__ < stack__) ? top__ : stack__;        \
                                                                             \
        register void* arg0_x0       asm("x0")  = (void*)(arg0);             \
        register void* arg1_x1       asm("x1")  = (void*)(arg1);             \
        register void* arg2_x2       asm("x2")  = (void*)(arg2);             \
        register void* arg3_x3       asm("x3")  = (void*)(arg3);             \
                                                                             \
        asm volatile (                                                       \
            "mov sp, %0\n\t"                                                 \
            "blr %1\n\t"                                                     \
            :                                                                \
            : "r"(smaller_top__), "r"(f),                                    \
              "r"(arg0_x0), "r"(arg1_x1), "r"(arg2_x2), "r"(arg3_x3)         \
            :);                                                              \
    } while (false)

// must be a callee-saved register
#define MADI_ORIG_SP_REG "x20"

#define MADI_CALL_ON_NEW_STACK(stack, stack_size,                            \
                               f, arg0, arg1, arg2, arg3)                    \
    do {                                                                     \
        /* calculate an initial stack pointer */                             \
        /* on the iso-address stack */                                       \
        uintptr_t stackintptr = (uintptr_t)(stack);                          \
        stackintptr = stackintptr + (stack_size);                            \
        stackintptr &= 0xFFFFFFFFFFFFFFF0;                                   \
        uint8_t *stack_ptr = (uint8_t *)(stackintptr);                       \
                                                                             \
        register void* stack_ptr_x9  asm("x9")  = (void*)(stack_ptr);        \
        register void* f_x10         asm("x10") = (void*)(f);                \
        register void* arg0_x0       asm("x0")  = (void*)(arg0);             \
        register void* arg1_x1       asm("x1")  = (void*)(arg1);             \
        register void* arg2_x2       asm("x2")  = (void*)(arg2);             \
        register void* arg3_x3       asm("x3")  = (void*)(arg3);             \
                                                                             \
       asm volatile (                                                        \
            "mov " MADI_ORIG_SP_REG ", sp\n\t"                               \
            "mov sp, %0\n\t"                                                 \
            "blr %1\n\t"                                                     \
            "mov sp, " MADI_ORIG_SP_REG "\n\t"                               \
            : "+r"(stack_ptr_x9), "+r"(f_x10),                               \
              "+r"(arg0_x0), "+r"(arg1_x1), "+r"(arg2_x2), "+r"(arg3_x3)     \
            :                                                                \
            : "x4", "x5", "x6", "x7",                                        \
              "x8", "x11", "x12", "x13", "x14", "x15",                       \
              "x16", "x17", "x18", MADI_ORIG_SP_REG,                         \
              /* callee-saved registers are saved */                         \
              MADI_FLOAT_CLOBBERS,                                           \
              "cc", "memory");                                               \
    } while (false)

}

#endif

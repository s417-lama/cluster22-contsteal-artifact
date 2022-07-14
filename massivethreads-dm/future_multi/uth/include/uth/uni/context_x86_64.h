#ifndef MADI_CONTEXT_X86_64_H
#define MADI_CONTEXT_X86_64_H

#include "../uth_config.h"
#include "../debug.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace madi {

#if   MADI_OS_TYPE == MADI_OS_LINUX

#define MADI_IP_MIN             ((uint8_t *)0x400000)
#define MADI_IP_MAX             ((uint8_t *)0x1000000)

#ifdef PIC
#define MADI_FUNC(name)         #name "@PLT"
#else
#define MADI_FUNC(name)         #name
#endif

#elif MADI_OS_TYPE == MADI_OS_DARWIN

#define MADI_IP_MIN             ((uint8_t *)0x100000000)
#define MADI_IP_MAX             ((uint8_t *)0x200000000)
#define MADI_FUNC(name)         "_" #name

#else
#error ""
#endif


struct context {
    void *rip;
    void *rsp;
    void *rbp;
    context *parent;

    uint8_t *instr_ptr() const {
        return reinterpret_cast<uint8_t *>(rip);
    }

    uint8_t *stack_ptr() const {
        return reinterpret_cast<uint8_t *>(rsp);
    }

    uint8_t *top_ptr() const {
        return reinterpret_cast<uint8_t *>(rsp); // - 128; // red zone
    }

    size_t stack_size() const {
        size_t size = (uint8_t *)parent + sizeof(context) - top_ptr();

        MADI_ASSERTP1(0 < size && size < 1024 * 1024, size);

        return size;
    }

    void print() const {
        MADI_DPUTS("rip = %p", rip);
        MADI_DPUTS("rsp = %p", rsp);
        MADI_DPUTS("rbp = %p", rbp);
        MADI_DPUTS("parent = %p", parent);
    }
};

#define MADI_CONTEXT_PRINT(level, ctx) \
    do { \
        MADI_DPUTS##level("(" #ctx ")         = %p", (ctx)); \
        MADI_DPUTS##level("(" #ctx ")->rip    = %p", (ctx)->rip); \
        MADI_DPUTS##level("(" #ctx ")->rsp    = %p", (ctx)->rsp); \
        MADI_DPUTS##level("(" #ctx ")->rbp    = %p", (ctx)->rbp); \
        MADI_DPUTS##level("(" #ctx ")->parent = %p", (ctx)->parent); \
    } while (false)

#define MADI_GET_CURRENT_SP(ptr_sp)                             \
    do {                                                        \
        uint8_t *sp__ = NULL;                                   \
        asm volatile("mov %%rsp,%0\n" : "=r"(sp__));            \
        *(ptr_sp) = sp__;                                       \
    } while (false)

#define MADI_GET_CURRENT_STACK_TOP(ptr_sp)                      \
    do {                                                        \
        MADI_GET_CURRENT_SP(ptr_sp);                            \
        *(uint8_t **)ptr_sp += 128;                             \
    } while (false)

#ifdef __AVX512F__
#define MADI_X86_FLOAT_REGS \
    "%xmm0" , "%xmm1" , "%xmm2" , "%xmm3" , "%xmm4" , "%xmm5" , "%xmm6" , "%xmm7" , \
    "%xmm8" , "%xmm9" , "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15", \
    "%xmm16", "%xmm17", "%xmm18", "%xmm19", "%xmm20", "%xmm21", "%xmm22", "%xmm23", \
    "%xmm24", "%xmm25", "%xmm26", "%xmm27", "%xmm28", "%xmm29", "%xmm30", "%xmm31"
#else
#define MADI_X86_FLOAT_REGS \
    "%xmm0" , "%xmm1" , "%xmm2" , "%xmm3" , "%xmm4" , "%xmm5" , "%xmm6" , "%xmm7" , \
    "%xmm8" , "%xmm9" , "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15"
#endif

#define MADI_SAVE_CONTEXT_WITH_CALL(parent_ctx_ptr, f, arg0, arg1)           \
    do {                                                                     \
        register void* parent_ctx_r8 asm("r8") = (void*)(parent_ctx_ptr);    \
        register void* f_r9          asm("r9") = (void*)(f);                 \
        void* arg0__ = (void*)(arg0);                                        \
        void* arg1__ = (void*)(arg1);                                        \
        asm volatile (                                                       \
            /* save red zone */                                              \
            "sub  $128, %%rsp\n\t"                                           \
            /* 16-byte sp alignment for SIMD registers */                    \
            "mov  %%rsp, %%rax\n\t"                                          \
            "and  $0xFFFFFFFFFFFFFFF0, %%rsp\n\t"                            \
            "push %%rax\n\t"                                                 \
            /* alignment */                                                  \
            "sub $0x8, %%rsp\n\t"                                            \
            /* parent field of context */                                    \
            "push %0\n\t"                                                    \
            /* push rbp */                                                   \
            "push %%rbp\n\t"                                                 \
            /* sp */                                                         \
            "lea  -16(%%rsp), %%rax\n\t"                                     \
            "push %%rax\n\t"                                                 \
            /* ip */                                                         \
            "lea  1f(%%rip), %%rax\n\t"                                      \
            "push %%rax\n\t"                                                 \
                                                                             \
            /* call function */                                              \
            "mov  %%rsp, %%rdi\n\t"                                          \
            "call *%1\n\t"                                                   \
                                                                             \
            /* pop ip from stack */                                          \
            "add $8, %%rsp\n\t"                                              \
                                                                             \
            "1:\n\t" /* ip is popped with ret operation at resume */         \
                                                                             \
            /* pop sp */                                                     \
            "add $8, %%rsp\n\t"                                              \
            /* pop rbp */                                                    \
            "pop %%rbp\n\t"                                                  \
            /* parent field of context and align */                          \
            "add $16, %%rsp\n\t"                                             \
            /* revert sp alignmment */                                       \
            "pop %%rsp\n\t"                                                  \
            /* restore red zone */                                           \
            "add $128, %%rsp\n\t"                                            \
            : "+r"(parent_ctx_r8), "+r"(f_r9),                               \
              "+S"(arg0__), "+d"(arg1__)                                     \
            :                                                                \
            : "%rax", "%rbx", "%rcx", "%rdi",                                \
              "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",                \
              MADI_X86_FLOAT_REGS,                                           \
              "cc", "memory");                                               \
    } while (false)

#define MADI_RESUME_CONTEXT(ctx)                                             \
    do {                                                                     \
        asm volatile (                                                       \
            "mov %0, %%rsp\n\t"                                              \
            "ret\n\t"                                                        \
            :                                                                \
            : "g"(ctx)                                                       \
            :);                                                              \
    } while (false)

#define MADI_EXECUTE_ON_STACK(f, arg0, arg1, arg2, arg3, stack_ptr)          \
    do {                                                                     \
        uint8_t *rsp__;                                                      \
        MADI_GET_CURRENT_SP(&rsp__);                                         \
                                                                             \
        uint8_t *stack__ = (uint8_t *)(stack_ptr);                           \
        uint8_t *top__ = rsp__ - 128;                                        \
        uint8_t *smaller_top__ = (top__ < stack__) ? top__ : stack__;        \
                                                                             \
        void* arg0__ = (void*)(arg0);                                        \
        void* arg1__ = (void*)(arg1);                                        \
        void* arg2__ = (void*)(arg2);                                        \
        void* arg3__ = (void*)(arg3);                                        \
                                                                             \
        asm volatile (                                                       \
            "mov %0, %%rsp\n\t"                                              \
            "call " MADI_FUNC(f) "\n\t"                                      \
            :                                                                \
            : "g"(smaller_top__),                                            \
              "D"(arg0__), "S"(arg1__), "d"(arg2__), "c"(arg3__)             \
            :);                                                              \
    } while (false)

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
        register void* stack_ptr_r8  asm("r8") = (void*)(stack_ptr);         \
        register void* f_r9          asm("r9") = (void*)(f);                 \
        void* arg0__ = (void*)(arg0);                                        \
        void* arg1__ = (void*)(arg1);                                        \
        void* arg2__ = (void*)(arg2);                                        \
        void* arg3__ = (void*)(arg3);                                        \
                                                                             \
        asm volatile (                                                       \
            "mov %%rsp, %%rax\n\t"                                           \
            "mov %0, %%rsp\n\t"                                              \
            /* alignment for xmm register accesses */                        \
            "sub $0x8, %%rsp\n\t"                                            \
            "push %%rax\n\t"                                                 \
            "call *%1\n\t"                                                   \
            "pop %%rsp\n\t"                                                  \
            : "+r"(stack_ptr_r8), "+r"(f_r9),                                \
              "+D"(arg0__), "+S"(arg1__), "+d"(arg2__), "+c"(arg3__)         \
            :                                                                \
            : "%rax", "%rbx",                                                \
              "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",                \
              MADI_X86_FLOAT_REGS,                                           \
              "cc", "memory");                                               \
    } while (false)

}

#endif

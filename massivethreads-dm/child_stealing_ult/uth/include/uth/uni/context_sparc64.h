#ifndef MADI_CONTEXT_SPARC64_H
#define MADI_CONTEXT_SPARC64_H

#include "../uth_config.h"
#include "../debug.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace madi {

#if MADI_OS_TYPE == MADI_OS_LINUX

// FIXME: specify always safe values
#define MADI_IP_MIN             ((uint8_t *)0x100000)
#define MADI_IP_MAX             ((uint8_t *)0x290000)
#define MADI_FUNC(name)         #name

#else
#error ""
#endif


struct context {
    uint8_t *sp;
    uint8_t *fp;
    uint8_t *ip;
    context *parent;

    uint8_t * instr_ptr() const { return ip; }
    uint8_t * stack_ptr() const { return sp; }
    uint8_t * top_ptr() const { return sp; }

    size_t stack_size() const
    {
        size_t size = (uint8_t *)parent + sizeof(context) - top_ptr();

        MADI_ASSERT(0 < size && size < 128 * 1024);

        return size;
    }
};

#define MADI_CONTEXT_PRINT(level, ctx) \
    do { \
        MADI_DPUTS##level("(" #ctx ")         = %p", (ctx)); \
        MADI_DPUTS##level("(" #ctx ")->sp     = %p", (ctx)->sp); \
        MADI_DPUTS##level("(" #ctx ")->fp     = %p", (ctx)->fp); \
        MADI_DPUTS##level("(" #ctx ")->ip     = %p", (ctx)->ip); \
        MADI_DPUTS##level("(" #ctx ")->parent = %p", (ctx)->parent); \
    } while (false)


struct saved_context {
    bool is_main_task;
    void *ip;
    void *sp;
    context *ctx;
    uint8_t *stack_top;
    size_t stack_size;
    uint8_t partial_stack[1];
};

#define MADI_SCONTEXT_ASSERT(sctx_ptr)
#define MADI_SCONTEXT_PRINT(level, sctx_ptr)

#define MADI_GET_CURRENT_SP(ptr_sp)                             \
    do {                                                        \
        uint8_t *sp__ = NULL;                                   \
        __asm__ ("mov %%sp,%0\n" : "=r"(sp__));                 \
        *(ptr_sp) = sp__;                                       \
    } while (false)

#define MADI_GET_CURRENT_STACK_TOP(ptr_sp)                      \
    MADI_GET_CURRENT_SP(ptr_sp);

#define MADI_GET_CURRENT_FP(ptr_fp)                             \
    do {                                                        \
        uint8_t *fp__ = NULL;                                   \
        __asm__ ("mov %%fp,%0\n" : "=r"(fp__));                 \
        *(ptr_fp) = fp__;                                       \
    } while (false)

#define MADI_GET_CURRENT_I7(ptr_i7)                             \
    do {                                                        \
        uint8_t *i7__ = NULL;                                   \
        __asm__ ("mov %%i7,%0\n" : "=r"(i7__));                 \
        *(ptr_i7) = i7__;                                       \
    } while (false)

#define MADI_SAVE_CONTEXT_WITH_CALL(parent_ctx_ptr, f, arg0, arg1) \
    madi_save_context_and_call__((parent_ctx_ptr), (f), (arg0), (arg1))

#define MADI_RESUME_CONTEXT(ctx) \
    madi_resume_context__(ctx)

#define MADI_EXECUTE_ON_STACK(f, arg0, arg1, arg2, arg3, stack_ptr) \
    do {                                                                \
        uint8_t *current_sp__;                                          \
        MADI_GET_CURRENT_SP(&current_sp__);                             \
                                                                        \
        uint8_t *stack__ = (uint8_t *)(stack_ptr);                      \
        uint8_t *top__ = current_sp__ - 176;                            \
        uint8_t *smaller_top__ = (top__ < stack__) ? top__ : stack__;   \
                                                                        \
        uintptr_t p = (uintptr_t)smaller_top__;                         \
        p -= 8;                                                         \
        p &= 0xFFFFFFFFFFFFFFF0;                                        \
        p -= 176 * 2 + 2047;                                            \
        p -= 8;                                                         \
        p &= 0xFFFFFFFFFFFFFFF0;                                        \
        p -= 176 * 2 + 2047;                                            \
                                                                         \
        /*MADI_DPUTS("MADI_EXECUTE_ON_STACK on %p(%p)",*/               \
        /*           (void *)p, smaller_top__);*/                       \
        smaller_top__ = (uint8_t *)p;                                   \
                                                                        \
        madi_execute_on_stack__(f, arg0, arg1, arg2, arg3, smaller_top__);  \
    } while (false)

#define MADI_CALL_ON_NEW_STACK(stack, stack_size,                       \
                               f, arg0, arg1, arg2, arg3)               \
    do {                                                                \
        size_t stack_bias = 2047;                                       \
        /*size_t frame_size = 176;*/                                    \
        /*size_t saved_fp = 128;*/                                      \
        size_t frame_size = 208;                                        \
        size_t saved_fp = 184;                                          \
                                                                        \
        uintptr_t stackintptr = (uintptr_t)(stack);                     \
        uintptr_t stack_tail = stackintptr + (stack_size);              \
        /*stack_tail -= 8;*/                                            \
        stack_tail &= 0xFFFFFFFFFFFFFFF0;                               \
        uintptr_t spval = stack_tail - frame_size * 2 - stack_bias;     \
                                                                        \
        void **fp = (void **)(spval + stack_bias + saved_fp);           \
        *fp = (void *)(stack_tail - frame_size - stack_bias);           \
                                                                        \
        void *sp = (void *)spval;                                       \
        madi_call_on_new_stack__(sp, f, arg0, arg1, arg2, arg3);        \
    } while (false)

}

extern "C" {
    void madi_save_context_and_call__(
        madi::context *parent_ctx_ptr,
        void (*f)(madi::context *ctx, void *arg0, void *arg1),
        void *arg0, void *arg1
    );
    void madi_resume_context__(madi::context *ctx);
    void madi_execute_on_stack__(void (*f)(void *, void *, void *, void *),
                                 void *arg0, void *arg1, void *arg2, void *arg3,
                                 uint8_t *stack_ptr);
    void madi_call_on_new_stack__(void *stack, 
                                  void (*f)(void *, void *, void *, void *),
                                void *arg0, void *arg1, void *arg2, void *arg3);
}

#endif

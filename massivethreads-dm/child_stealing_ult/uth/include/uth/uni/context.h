#ifndef MADI_CONTEXT_H
#define MADI_CONTEXT_H

#include "../uth_config.h"
#include "../madi.h"
#include "../debug.h"
#include "../misc.h"


#if 0
#define MADI_IP_ASSERT(ip) \
    do { \
        MADI_ASSERTP1((ip) > MADI_IP_MIN, (ip)); \
        MADI_ASSERTP1((ip) < MADI_IP_MAX, (ip)); \
    } while (false)
#else
#define MADI_IP_ASSERT(ip)
#endif


#if MADI_DEBUG_LEVEL >= 1
#define MADI_SP_ASSERT(sp) \
    do { \
        MADI_UNUSED madi::iso_space& ispace__ = madi::get_iso_space(); \
        MADI_ASSERTP2((uint8_t *)(sp) >= ispace__.shared_base_ptr(), \
                      (uint8_t *)(sp), ispace__.shared_base_ptr()); \
        MADI_ASSERTP2((uint8_t *)(sp) < ispace__.shared_end_ptr(), \
                      (uint8_t *)(sp), ispace__.shared_end_ptr()); \
    } while (false)
#else
#define MADI_SP_ASSERT(sp) do { } while (false)

#endif

#define MADI_CONTEXT_ASSERT(ctx_ptr) \
    do { \
        MADI_UNUSED context *ctx__ = (ctx_ptr); \
        MADI_ASSERT(ctx__ != NULL); \
        MADI_SP_ASSERT(ctx__); \
        MADI_IP_ASSERT(ctx__->instr_ptr()); \
        MADI_SP_ASSERT(ctx__->stack_ptr()); \
        MADI_ASSERT(ctx__->parent != NULL); \
        MADI_SP_ASSERT(ctx__->parent); \
        MADI_ASSERTP2(ctx__->parent > ctx__, ctx__->parent, ctx__); \
        MADI_IP_ASSERT(ctx__->parent->instr_ptr()); \
        MADI_ASSERT(ctx__->parent->stack_ptr() > ctx__->stack_ptr()); \
        /*MADI_ASSERT((uint8_t *)ctx__ == ctx__->stack_ptr());*/ \
        /*MADI_ASSERT((uint8_t *)ctx__ < ctx__->parent->stack_ptr());*/ \
        /*MADI_ASSERT((uint8_t *)ctx__->parent == ctx__->parent->stack_ptr());*/ \
    } while (false)

#define MADI_CONTEXT_ASSERT_WITHOUT_PARENT(ctx_ptr) \
    do { \
        MADI_UNUSED context *ctx__ = (ctx_ptr); \
        MADI_ASSERT(ctx__ != NULL); \
        MADI_SP_ASSERT(ctx__); \
        MADI_IP_ASSERT(ctx__->instr_ptr()); \
        MADI_SP_ASSERT(ctx__->stack_ptr()); \
        /*MADI_ASSERT((uint8_t *)ctx__ == ctx__->stack_ptr());*/ \
    } while (false)

#define MADI_CONTEXT_ASSERT_ON_HEAP(ctx_ptr) \
    do { \
        MADI_UNUSED context *ctx__ = (ctx_ptr); \
        MADI_ASSERT(ctx__ != NULL); \
        MADI_IP_ASSERT(ctx__->instr_ptr()); \
        MADI_SP_ASSERT(ctx__->stack_ptr()); \
        MADI_ASSERT(ctx__->parent != NULL); \
        MADI_SP_ASSERT(ctx__->parent); \
        MADI_IP_ASSERT(ctx__->parent->instr_ptr()); \
        MADI_ASSERT(ctx__->parent->stack_ptr() > ctx__->stack_ptr()); \
        /*MADI_ASSERT((uint8_t *)ctx__->parent == ctx__->parent->stack_ptr());*/ \
    } while (false)


#if   MADI_ARCH_TYPE == MADI_ARCH_X86_64

#include "context_x86_64.h"

#elif MADI_ARCH_TYPE == MADI_ARCH_SPARC64

#include "context_sparc64.h"

#elif MADI_ARCH_TYPE == MADI_ARCH_AARCH64

#include "context_aarch64.h"

#endif

#endif

#ifndef MADI_CONSTANTS_H
#define MADI_CONSTANTS_H

//
// Thread type
//
#define MADI_THREAD_TYPE_UNI 0
#define MADI_THREAD_TYPE_ISO 1

//
// CPU Architecture
//
#define MADI_ARCH_X86_64   0
#define MADI_ARCH_SPARC64  1
#define MADI_ARCH_AARCH64  2

#ifdef __x86_64__
#define MADI_ARCH_TYPE     MADI_ARCH_X86_64
#elif (defined __sparc__) && (defined __arch64__)
#define MADI_ARCH_TYPE     MADI_ARCH_SPARC64
#elif defined(__aarch64__)
#define MADI_ARCH_TYPE     MADI_ARCH_AARCH64
#else
#error "This architecture is not supported"
#endif

//
// Operating System
//
#define MADI_OS_LINUX      0
#define MADI_OS_DARWIN     1

#if   defined(__linux__)
#define MADI_OS_TYPE       MADI_OS_LINUX
#elif defined(__APPLE__)
#define MADI_OS_TYPE       MADI_OS_DARWIN
#else
#error "This OS is not supported"
#endif

#include "madm/madm_comm_config.h"

#endif

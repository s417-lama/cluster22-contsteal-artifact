#include "iso_space.h"

#include "uth_options.h"
#include "madi.h"
#include "uth_comm-inl.h"
#include "debug.h"
#include <errno.h>
#include <sysexits.h>
#include <sys/mman.h>
#include <signal.h>

namespace madi {

    iso_space::iso_space()
        : shared_base_(NULL)
        , shared_size_(0)
        , stack_(NULL)
        , stack_size_(0)
        , remote_ptrs_(NULL)
    {
    }

    static struct sigaction g_new_act, g_old_segv_act, g_old_bus_act;
    static void *g_stack_guard_begin = NULL;
    static void *g_stack_guard_end = NULL;

    static void stack_overflow_handler(int sig, siginfo_t* sig_info,
                                       void* sig_data)
    {

        MADI_CHECK(sig == SIGSEGV || sig == SIGBUS);

        if(sig_info->si_code == SEGV_ACCERR) {
            if (g_stack_guard_begin <= sig_info->si_addr &&
                sig_info->si_addr < g_stack_guard_end) {
                MADI_DIE("stack overflow");
            }
        }

        struct sigaction *act = NULL;
        if (sig == SIGSEGV) act = &g_old_segv_act;
        if (sig == SIGBUS)  act = &g_old_bus_act;

        int r0 = sigaction(sig, act, NULL);

        if (r0 != 0) {
            perror("sigaction");
            madi::exit(1);
        }

        raise(sig);
    }

    static void protect_stack(uint8_t *stack, size_t stack_size)
    {
        size_t page_size = madi::uth_options.page_size;

        if (stack_size == 0)
            return;

        if (stack_size < 3 * page_size)
            MADI_SPMD_DIE("The stack size is too small "
                          "to detect stack overflow");

        // align stack base along a page for mprotect
        void *protect_base =
            (void *)(((uintptr_t)stack + page_size - 1)
                     / page_size * page_size);

        g_stack_guard_begin = stack;
        g_stack_guard_end = stack + page_size;

        int r0 = mprotect(protect_base, page_size, PROT_NONE);

        if (r0 != 0)
            MADI_SPMD_PERR_DIE("mprotect");

        stack_t ss;
        ss.ss_sp    = malloc(MINSIGSTKSZ);
        ss.ss_size  = MINSIGSTKSZ;
        ss.ss_flags = 0;

        if( sigaltstack(&ss, NULL) != 0)
            MADI_SPMD_PERR_DIE("sigaltstack");

        int r1 = sigaction(SIGSEGV, NULL, &g_old_segv_act);

        if (r1 != 0)
            MADI_SPMD_PERR_DIE("sigaction");

        int r2 = sigaction(SIGBUS, NULL, &g_old_bus_act);

        if (r2 != 0)
            MADI_SPMD_PERR_DIE("sigaction");

        sigemptyset(&g_new_act.sa_mask);
        sigaddset(&g_new_act.sa_mask, SIGBUS);
        g_new_act.sa_sigaction = stack_overflow_handler;
        g_new_act.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

        int r3 = sigaction(SIGSEGV, &g_new_act, NULL);

        if (r3 != 0)
            MADI_SPMD_PERR_DIE("sigaction");

        int r4 = sigaction(SIGBUS, &g_new_act, NULL);

        if (r4 != 0)
            MADI_SPMD_PERR_DIE("sigaction");
    }

    void iso_space::validate_iso_space_options()
    {
        size_t stack_size = uth_options.stack_size;
        size_t page_size = uth_options.page_size;

        // Uni-address threads requires that Address Space Layout
        // Randomization (ASLR) is disabled (the madmrun command does that).
        if (getenv("MADM_RUN__") == NULL)
            MADI_SPMD_DIE("this program must be executed with madmrun command");

        // validate stack size
        if (stack_size < page_size)
            MADI_SPMD_DIE("specified root stack size is too small (%zu)",
                          stack_size);
//         if (stack_size > SHARED_LOCAL_SIZE)
//             MADI_SPMD_DIE("specified root stack size is too large (%zu)",
//                           stack_size);
    }

    void iso_space::initialize(uth_comm& c)
    {
        validate_iso_space_options();
        size_t stack_size = uth_options.stack_size;

        size_t me = c.get_pid();

        size_t shared_size = stack_size;

        // mmap shared local region and register it to the NICs for RDMA.
        remote_ptrs_ = (uint8_t **)c.reg_mmap_shared((uint8_t *)SHARED_BASE,
                                                     shared_size);
        if (remote_ptrs_ == NULL)
            MADI_SPMD_DIE("iso-address space cannot be allocated");

        uint8_t * shared_base = remote_ptrs_[me];
        uint8_t * stack = shared_base;

        if (uth_options.stack_overflow_detection) {
            // protect stack
            protect_stack(stack, stack_size);
        }

        shared_base_ = shared_base;
        shared_size_ = shared_size;
        stack_ = stack;
        stack_size_ = stack_size;

        MADI_DPUTS1("stack = [%p, %p), stack_size = %zu",
                    stack_, stack_ + stack_size_, stack_size_);
    }

    void iso_space::finalize(uth_comm& c)
    {
        size_t shared_size = stack_size_;

        c.reg_munmap_shared((void **)remote_ptrs_, (void *)SHARED_BASE,
                            shared_size);

        shared_base_ = NULL;
        stack_ = NULL;
        stack_size_ = 0;
        remote_ptrs_ = NULL;
    }

}


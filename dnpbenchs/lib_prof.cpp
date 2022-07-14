#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>

#include "dnplogger.hpp"

#define UNW_LOCAL_ONLY
#include <libunwind.h>

void print_backtrace() {
  unw_cursor_t cursor;
  unw_context_t context;
  unw_word_t offset;
  unw_proc_info_t pinfo;
  Dl_info dli;
  char sname[256];
  void *addr;
  int count = 0;
  char *buf;
  size_t buf_size;
  FILE* mstream = open_memstream(&buf, &buf_size);

  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  fprintf(mstream, "Backtrace:\n");
  while (unw_step(&cursor) > 0) {
    unw_get_proc_info(&cursor, &pinfo);
    unw_get_proc_name(&cursor, sname, sizeof(sname), &offset);
    addr = (char *)pinfo.start_ip + offset;
    dladdr(addr, &dli);
    fprintf(mstream, "  #%d %p in %s + 0x%lx from %s\n",
            count, addr, sname, offset, dli.dli_fname);
    count++;
  }
  fprintf(mstream, "\n");
  fflush(mstream);

  fwrite(buf, sizeof(char), buf_size, stdout);

  fclose(mstream);
  free(buf);
}

void segv_handler(int sig) {
  print_backtrace();
  exit(1);
}

static int (*main_orig)(int, char**, char**);

int main_hook(int argc, char** argv, char** envp) {
  struct sigaction sa;
  sa.sa_flags   = 0;
  sa.sa_handler = segv_handler;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGSEGV, &sa, NULL) == -1) {
    printf("sigacton for SIGSEGV failed.\n");
    exit(1);
  }

  return main_orig(argc, argv, envp);
}

extern "C" int __libc_start_main(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    int (*init)(int, char **, char **),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end) {
  main_orig = main;

  void* handle = dlsym(RTLD_NEXT, "__libc_start_main");
  auto func = reinterpret_cast<decltype(&__libc_start_main)>(handle);

  return func(main_hook, argc, argv, init, fini, rtld_fini, stack_end);
}


template <typename F>
using return_type_of = typename decltype(std::function{std::declval<F>()})::result_type;

template <typename F, typename... Args>
inline return_type_of<F> __lib_prof_common(const char* func_name, Args... args) {
  void *handle = dlsym(RTLD_NEXT, func_name);
  auto func = reinterpret_cast<F>(handle);

  void *bp = nullptr;
  if (dnplogger::initialized()) {
    bp = dnplogger::begin_event();
  }

  auto ret = func(args...);

  if (dnplogger::initialized() && bp) {
    dnplogger::end_event(bp, func_name);
  }

  return ret;
}

/* int pthread_mutex_lock(pthread_mutex_t* m) { */
/*   return __lib_prof_common<decltype(&pthread_mutex_lock)>(__func__, m); */
/* } */

/* int pthread_mutex_unlock(pthread_mutex_t* m) { */
/*   return __lib_prof_common<decltype(&pthread_mutex_unlock)>(__func__, m); */
/* } */

/* void *pthread_getspecific(pthread_key_t __key) { */
/*   return __lib_prof_common<decltype(&pthread_getspecific)>(__func__, __key); */
/* } */

/* int pthread_create(pthread_t *__restrict __newthread, */
/* 			   const pthread_attr_t *__restrict __attr, */
/* 			   void *(*__start_routine) (void *), */
/* 			   void *__restrict __arg) { */
/*   void *handle = dlsym(RTLD_NEXT, "pthread_create"); */
/*   auto func = reinterpret_cast<decltype(&pthread_create)>(handle); */

/*   static int count = 0; */
/*   count++; */
/*   printf("%d: pthread_create: %d\n", getpid(), count); */

/*   print_backtrace(); */

/*   return func(__newthread, __attr, __start_routine, __arg); */
/* } */

/* #include "ucp/api/ucp.h" */

/* ucs_status_ptr_t */
/* ucp_atomic_fetch_nb(ucp_ep_h ep, ucp_atomic_fetch_op_t opcode, */
/*                     uint64_t value, void *result, size_t op_size, */
/*                     uint64_t remote_addr, ucp_rkey_h rkey, */
/*                     ucp_send_callback_t cb) { */
/*   return __lib_prof_common<decltype(&ucp_atomic_fetch_nb)>(__func__, */
/*       ep, opcode, value, result, op_size, remote_addr, rkey, cb); */
/* } */

/* unsigned ucp_worker_progress(ucp_worker_h worker) { */
/*   return __lib_prof_common<decltype(&ucp_worker_progress)>(__func__, worker); */
/* } */

/* ucs_status_t ucp_put_nbi(ucp_ep_h ep, const void *buffer, size_t length, */
/*                          uint64_t remote_addr, ucp_rkey_h rkey) { */
/*   return __lib_prof_common<decltype(&ucp_put_nbi)>(__func__, */
/*       ep, buffer, length, remote_addr, rkey); */
/* } */

/* ucs_status_t ucp_get_nbi(ucp_ep_h ep, void *buffer, size_t length, */
/*                          uint64_t remote_addr, ucp_rkey_h rkey) { */
/*   return __lib_prof_common<decltype(&ucp_get_nbi)>(__func__, */
/*       ep, buffer, length, remote_addr, rkey); */
/* } */

/* ucs_status_t ucp_ep_create(ucp_worker_h worker, const ucp_ep_params_t *params, */
/*                            ucp_ep_h *ep_p) { */
/*   return __lib_prof_common<decltype(&ucp_ep_create)>(__func__, */
/*       worker, params, ep_p); */
/* } */

/* ucs_status_t ucp_ep_rkey_unpack(ucp_ep_h ep, const void *rkey_buffer, */
/*                                 ucp_rkey_h *rkey_p) { */
/*   return __lib_prof_common<decltype(&ucp_ep_rkey_unpack)>(__func__, */
/*       ep, rkey_buffer, rkey_p); */
/* } */

extern "C" {

void* dnp_prof_begin() {
  if (dnplogger::initialized()) {
    void* bp = dnplogger::begin_event();
    return bp;
  } else {
    return NULL;
  }
}

void dnp_prof_end(void* bp, const char* kind) {
  if (dnplogger::initialized() && bp) {
    dnplogger::end_event(bp, kind);
  }
}

void* dnp_prof_begin_tid(int tid) {
  if (dnplogger::initialized()) {
    void* bp = dnplogger::begin_event(tid);
    return bp;
  } else {
    return NULL;
  }
}

void dnp_prof_end_tid(void* bp, const char* kind, int tid) {
  if (dnplogger::initialized() && bp) {
    dnplogger::end_event(bp, kind, tid);
  }
}

}

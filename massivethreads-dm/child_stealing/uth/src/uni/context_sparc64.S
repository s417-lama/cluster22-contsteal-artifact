
.text

#define BIAS            2047 /* Required by V9 ABI */
#define FRAMESIZE       176

#if 0
#define CTX             128
#define SP              128
#define FP              136
#define IP              144
#define PARENT_CTX_PTR  152
#else
#define FULLFRAMESIZE   208
#define CTX             176
#define SP              176
#define FP              184
#define IP              192
#define PARENT_CTX_PTR  200
#endif

/*
 * void madi_save_context_and_call__(
 *     context *parent_ctx_ptr,
 *     void (*f)(context *ctx, void *arg0, void *arg1),
 *     void *arg0, void *arg1
 * );
 */
.p2align 4
.global madi_save_context_and_call__
.type madi_save_context_and_call__, @function
madi_save_context_and_call__:
  save %sp, -FULLFRAMESIZE, %sp
  /* save old stack pointer, frame pointer, return address */
  stx %sp, [%sp+BIAS+SP]
  stx %fp, [%sp+BIAS+FP]
  stx %i7, [%sp+BIAS+IP]
  stx %i0, [%sp+BIAS+PARENT_CTX_PTR]
  /* save general-purpose registers to the stack */
  flushw
  /* call function */
  add %sp, BIAS+CTX, %o0        /* ctx */
  mov %i2, %o1                  /* arg0 */
  mov %i3, %o2                  /* arg1 */
  call %i1
  nop
  /* restore and return */
  return %i7 + 8
  nop
.size madi_save_context_and_call__, .-madi_save_context_and_call__

/*
 * void madi_resume_context__(context *ctx);
 */
.p2align 4
.global madi_resume_context__
.type madi_resume_context__, @function
madi_resume_context__:
  save %sp, -FRAMESIZE, %sp
  /* flush register windows for the new context not to read old values */
  flushw
  /* load stack pointer, frame pointer, return address */
  ldx [%i0], %sp
  ldx [%i0+8], %fp
  ldx [%i0+16], %i7
  /* return */
  return %i7 + 8
  nop
.size madi_resume_context__, .-madi_resume_context__


/*
 * void madi_execute_on_stack__(void (*f)(void *, void *, void *, void *),
 *                              void *arg0, void *arg1, void *arg2, void *arg3,
 *                              uint8_t *smaller_top);
 */
.p2align 4
.global madi_execute_on_stack__
.type madi_execute_on_stack__, @function
madi_execute_on_stack__:
    save        %sp, -FRAMESIZE, %sp
    /* flush register windows for discarding old register values */
    flushw
    /* move the stack pointer to the safe point (smaller_top) */
    mov         %i5, %sp
    /* call a function */
    mov         %i1, %o0
    mov         %i2, %o1
    mov         %i3, %o2
    mov         %i4, %o3
    call        %i0
    nop
    /* not reached */
    return      %i7 + 8
    nop
.size madi_execute_on_stack__, .-madi_execute_on_stack__

/*
 * void madi_call_on_new_stack__(uint8_t *stack,
 *                               void (*f)(void *, void *, void *, void *),
 *                               void *arg0, void *arg1, void *arg2, void*arg3);
 */
.p2align 4
.global madi_call_on_new_stack__
.type madi_call_on_new_stack__, @function
madi_call_on_new_stack__:
  save %sp, -FULLFRAMESIZE, %sp
  /* save current context */
  /*
  mov %sp, %l0
  stx %fp, [%sp+BIAS+FP]
  stx %i7, [%sp+BIAS+IP]
  */
  flushw
  /* set new stack pointer, frame pointer, return address */
  mov %i0, %sp
  /*ldx [%sp+BIAS+FP], %fp*/
  /*ldx [%sp+BIAS+IP], %i7*/
  /* call function */
  mov %i2, %o0
  mov %i3, %o1
  mov %i4, %o2
  mov %i5, %o3
  call %i1
  nop
  /* resume context */
  /*
  mov %l0, %sp
  ldx [%sp+BIAS+FP], %fp
  ldx [%sp+BIAS+IP], %i7
  */
  /* return */
  return %i7 + 8
  nop
.size madi_call_on_new_stack__, .-madi_call_on_new_stack__


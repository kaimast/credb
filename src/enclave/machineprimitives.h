#pragma once

#include <stdint.h>

typedef void *stack_pointer_t;

typedef int *arg_t; /* function argument */
typedef void (*proc_t)(arg_t); /* generic function pointer */

/*
 *  Allocate a fresh stack.  Stacks are said to grow "down" (from higher
 *  memory locations towards lower ones) on the x86 architecture.
 *
 *  The bottom of the stack is returned in *stackbase; the top of
 *  the stack is returned in *stacktop.
 *
 *  -----------------
    struct p
 *  |  stacktop     |  <- next word pushed here
 *  |               |
 *  |               |
 *  |  stackbase    |  <- bottom of stack.
 *  -----------------
 */
void minithread_allocate_stack(stack_pointer_t *stackbase, stack_pointer_t *stacktop);

/*
 * minithread_free_stack(stack_pointer_t stackbase)
 *
 * Frees the stack at stackbase.  Care should be taken to ensure that the stack
 * is not in use when it is freed.
 */
void minithread_free_stack(stack_pointer_t stackbase);

/*
 *  Initialize the stackframe pointed to by *stacktop so that
 *  the thread running off of *stacktop will invoke:
 *      body_proc(body_arg);
 *      final_proc(final_arg);
 *
 *  The call to final_proc should be used for cleanup, since it is called
 *  when body_proc returns.  final_proc should not return; doing so will
 *  lead to undefined behavior and likely cause your system to crash.
 *
 *  body_proc and final_proc cannot be NULL. Passing invalid
 *      function pointers crashes the system.
 *
 *  This procedure changes the value of *stacktop.
 *
 */
void minithread_initialize_stack(stack_pointer_t *stacktop,

                                 proc_t body_proc,
                                 arg_t body_arg,

                                 proc_t final_proc,
                                 arg_t final_arg);


/*
 * Context switch primitive.
 *
 * This call will first save the caller's state (i.e. all of its registers) on
 * the stack.  It will then save the stack pointer in the location pointed to
 * by old_thread_sp. It will replace the processor's stack pointer with the
 * value pointed to by the new_thread_sp. Finally, it will reload the rest of
 * the machine registers that were saved on the new thread's stack previously,
 * and thus resume the new thread from where it left off.
 */

extern "C" {
void minithread_switch(stack_pointer_t *old_thread_sp, stack_pointer_t *new_thread_sp);
}

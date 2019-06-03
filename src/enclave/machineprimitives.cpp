/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include <cstdio>
#include <cstdlib>

#include "machineprimitives.h"

using initial_stack_state_t = struct initial_stack_state*;

struct initial_stack_state
{
    void *body_proc; /* v1 or ebx */
    void *body_arg; /* v2 or edi */
    void *final_proc; /* v3 or esi */
    void *final_arg; /* v4 or ebp */
    void *r8;
    void *r9;
    void *r10;
    void *r11;
    void *r12;
    void *r13;
    void *r14;
    void *r15;
    void *rax;
    void *rcx;
    void *rdx;
    void *root_proc; /* left on stack */
};

constexpr int STACK_GROWS_DOWN = 1;
constexpr int STACKSIZE = (256 * 1024);
constexpr int STACKALIGN = 0xf;

void minithread_allocate_stack(stack_pointer_t *stackbase, stack_pointer_t *stacktop)
{
    //NOLINTNEXTLINE
    *stackbase = reinterpret_cast<stack_pointer_t>(malloc(STACKSIZE));

    if(!*stackbase)
    {
        return;
    }

    if(STACK_GROWS_DOWN)
    {
        /* Stacks grow down, but malloc grows up. Compensate and word align
           (turn off low 2 bits by anding with ~3). */
        *stacktop = reinterpret_cast<stack_pointer_t>((long)((char *)*stackbase + STACKSIZE - 1) & ~STACKALIGN);
    }
    else
    {
        /* Word align (turn off low 2 bits by anding with ~3) */
        *stacktop = reinterpret_cast<stack_pointer_t>(((long)*stackbase + 3) & ~STACKALIGN);
    }
}

extern "C" {
/// Defined in assembly
extern int minithread_root();
}

void minithread_free_stack(stack_pointer_t stackbase)
{
    // NOLINTNEXTLINE(hicpp-no-malloc)
    free(stackbase);
}

void minithread_initialize_stack(stack_pointer_t *stacktop, proc_t body_proc, arg_t body_arg, proc_t final_proc, arg_t final_arg)
{
    initial_stack_state_t ss;

    /*
     * Configure initial machine state so that this thread starts
     * running inside a wrapper procedure named minithread_root.
     * minithread_root will invoke the procedures in order, and
     * then halt.
     */
    *(reinterpret_cast<char**>(stacktop)) -= sizeof(struct initial_stack_state);
    ss = reinterpret_cast<initial_stack_state_t>(*stacktop);

    ss->body_proc = reinterpret_cast<void*>(body_proc);
    ss->body_arg = reinterpret_cast<void*>(body_arg);
    ss->final_proc = reinterpret_cast<void*>(final_proc);
    ss->final_arg = reinterpret_cast<void*>(final_arg);
    ss->root_proc = reinterpret_cast<void*>(minithread_root);
}

.text

.global minithread_switch
.global minithread_root

minithread_switch:
    pushq %rax
    pushq %rcx
    pushq %rdx
    movq  %rdi,%rcx
    movq  %rsi,%rdx
    movq  %rdx,%rax
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %rbx
    movq %rsp,(%rcx)
    movq (%rax),%rsp
    popq %rbx
    popq %rdi
    popq %rsi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rdx
    popq %rcx
    popq %rax
    retq

minithread_root: 
    sub $0x78,%rsp
    pushq %rsi
    callq *%rbx    # call main proc

    popq %rsi      # get clean up location back
    sub $0x8,%rsp
    movq %rbp,%rdi
    callq *%rsi    # call the clean-up
    ret


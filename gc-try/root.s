.text

.globl get_sp
get_sp:
    movq %rsp,%rax
    ret

.globl get_bp
get_bp:
    movq %rbp,%rax
    ret

.globl get_di
get_di:
    movq %rdi,%rax
    ret

.globl get_si
get_si:
    movq %rsi,%rax
    ret

.globl get_dx
get_dx:
    movq %rdx,%rax
    ret

.globl get_cx
get_cx:
    movq %rcx,%rax
    ret

.globl get_r8
get_r8:
    movq %r8,%rax
    ret

.globl get_r9
get_r9:
    movq %r9,%rax
    ret

.globl get_ax
get_ax:
    movq %rax,%rax
    ret

.globl get_bx
get_bx:
    movq %rbx,%rax
    ret

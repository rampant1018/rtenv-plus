	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.thumb

.macro SYSCALL_ROUTINE syscall_number
        push {r7}
	mov r7, \syscall_number
	svc 0
	nop
	pop {r7}
	bx lr
.endm

.global fork
fork:
        SYSCALL_ROUTINE #0x1
.global getpid
getpid:
        SYSCALL_ROUTINE #0x2
.global write
write:
        SYSCALL_ROUTINE #0x3
.global read
read:
        SYSCALL_ROUTINE #0x4
.global interrupt_wait
interrupt_wait:
        SYSCALL_ROUTINE #0x5
.global getpriority
getpriority:
        SYSCALL_ROUTINE #0x6
.global setpriority
setpriority:
        SYSCALL_ROUTINE #0x7
.global mknod
mknod:
        SYSCALL_ROUTINE #0x8
.global sleep
sleep:
        SYSCALL_ROUTINE #0x9
.global lseek
lseek:
        SYSCALL_ROUTINE #0xA
.global sbrk
sbrk:
        SYSCALL_ROUTINE #0xB

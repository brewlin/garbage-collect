/**
 *@ClassName platform
 *@Deacription go
 *@Author brewlin
 *@Date 2021/3/31 0031 上午 11:08
 *@Version 1.0
 **/
#ifndef GOC_PLATFORM_H
#define GOC_PLATFORM_H

#define GoosAix  0
#define GoosAndroid  0
#define GoosDarwin  0
#define GoosDragonfly  0
#define GoosFreebsd  0
#define GoosHurd  0
#define GoosJs  0
#define GoosLinux  1
#define GoosNacl  0
#define GoosNetbsd  0
#define GoosOpenbsd  0
#define GoosPlan9  0
#define GoosSolaris  0
#define GoosWindows  0
#define GoosZos  0


#define Goarch386  0
#define GoarchAmd64  1
#define GoarchAmd64p32  0
#define GoarchArm  0
#define GoarchArmbe  0
#define GoarchArm64  0
#define GoarchArm64be  0
#define GoarchPpc64  0
#define GoarchPpc64le  0
#define GoarchMips  0
#define GoarchMipsle  0
#define GoarchMips64  0
#define GoarchMips64le  0
#define GoarchMips64p32  0
#define GoarchMips64p32le  0
#define GoarchPpc  0
#define GoarchRiscv  0
#define GoarchRiscv64  0
#define GoarchS390  0
#define GoarchS390x  0
#define GoarchSparc  0
#define GoarchSparc64  0
#define GoarchWasm  0


#define _EINTR   0x4
#define _EAGAIN  0xb
#define _ENOMEM  0xc

#define _PROT_NONE   0x0
#define _PROT_READ   0x1
#define _PROT_WRITE  0x2
#define _PROT_EXEC   0x4

#define _MAP_ANON     0x20
#define _MAP_PRIVATE  0x2
#define _MAP_FIXED    0x10

#define _MADV_DONTNEED    0x4
#define _MADV_FREE        0x8
#define _MADV_HUGEPAGE    0xe
#define _MADV_NOHUGEPAGE  0xf

#define _SA_RESTART   0x10000000
#define _SA_ONSTACK   0x8000000
#define _SA_RESTORER  0x4000000
#define _SA_SIGINFO   0x4

#define _SIGHUP     0x1
#define _SIGINT     0x2
#define _SIGQUIT    0x3
#define _SIGILL     0x4
#define _SIGTRAP    0x5
#define _SIGABRT    0x6
#define _SIGBUS     0x7
#define _SIGFPE     0x8
#define _SIGKILL    0x9
#define _SIGUSR1    0xa
#define _SIGSEGV    0xb
#define _SIGUSR2    0xc
#define _SIGPIPE    0xd
#define _SIGALRM    0xe
#define _SIGSTKFLT  0x10
#define _SIGCHLD    0x11
#define _SIGCONT    0x12
#define _SIGSTOP    0x13
#define _SIGTSTP    0x14
#define _SIGTTIN    0x15
#define _SIGTTOU    0x16
#define _SIGURG     0x17
#define _SIGXCPU    0x18
#define _SIGXFSZ    0x19
#define _SIGVTALRM  0x1a
#define _SIGPROF    0x1b
#define _SIGWINCH   0x1c
#define _SIGIO      0x1d
#define _SIGPWR     0x1e
#define _SIGSYS     0x1f

#define _FPE_INTDIV  0x1
#define _FPE_INTOVF  0x2
#define _FPE_FLTDIV  0x3
#define _FPE_FLTOVF  0x4
#define _FPE_FLTUND  0x5
#define _FPE_FLTRES  0x6
#define _FPE_FLTINV  0x7
#define _FPE_FLTSUB  0x8

#define _BUS_ADRALN  0x1
#define _BUS_ADRERR  0x2
#define _BUS_OBJERR  0x3

#define _SEGV_MAPERR  0x1
#define _SEGV_ACCERR  0x2

#define _ITIMER_REAL     0x0
#define _ITIMER_VIRTUAL  0x1
#define _ITIMER_PROF     0x2

#define _EPOLLIN        0x1
#define _EPOLLOUT       0x4
#define _EPOLLERR       0x8
#define _EPOLLHUP       0x10
#define _EPOLLRDHUP     0x2000
#define _EPOLLET        0x80000000
#define _EPOLL_CLOEXEC  0x80000
#define _EPOLL_CTL_ADD  0x1
#define _EPOLL_CTL_DEL  0x2
#define _EPOLL_CTL_MOD  0x3

#define _AF_UNIX     0x1
#define _F_SETFL     0x4
#define _SOCK_DGRAM  0x2


#endif //GOC_PLATFORM_H

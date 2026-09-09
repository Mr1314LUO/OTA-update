// syscalls.c —— newlib stubs(无 _sbrk,规避 libc malloc)
// _write 重定向到 UART1
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "stm32f1_uart.h"

#undef errno
extern int errno;

int _write(int fd, char *buf, int len) {
    (void)fd;
    uart1_write(buf, (uint32_t)len);
    return len;
}

int _close(int fd)        { (void)fd; errno = EBADF;  return -1; }
int _lseek(int fd, int off, int w) { (void)fd; (void)off; (void)w; return 0; }
int _read(int fd, char *buf, int len) { (void)fd; (void)buf; (void)len; return 0; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)      { (void)fd; return 1; }

void _exit(int status)   { (void)status; for(;;){} }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
int _getpid(void)        { return 1; }

// __libc_init_array 需要 _init/_fini stubs
void _init(void)         { }
void _fini(void)         { }

// 不实现 _sbrk,链接时若需 malloc 会失败(正是我们想要的:逼用 FreeRTOS heap)
// 但 newlib printf 可能内部调 _sbrk —— 故 P2 用 printf_lite 替代 printf
void _sbrk(void *unused) { (void)unused; for(;;){} }  // 拒绝任何堆分配

#define _GNU_SOURCE
#include <dlfcn.h>
#include <unistd.h>
#include <stdint.h>
static void wr(const char*s){ int n=0; while(s[n])n++; write(2,s,n); }
static void whex(unsigned long v){ char b[19]="0x"; static const char*H="0123456789abcdef"; int n=2; int st=0; for(int i=60;i>=0;i-=4){int d=(v>>i)&0xf; if(d||st||i==0){b[n++]=H[d];st=1;}} b[n++]='\n'; write(2,b,n); }
typedef void (*ef)(int);
void exit(int c){ wr("[DCM] exit ra="); whex((unsigned long)__builtin_return_address(0)); ((ef)dlsym(RTLD_NEXT,"exit"))(c); __builtin_unreachable(); }
void _exit(int c){ wr("[DCM] _exit ra="); whex((unsigned long)__builtin_return_address(0)); ((ef)dlsym(RTLD_NEXT,"_exit"))(c); __builtin_unreachable(); }
void _Exit(int c){ wr("[DCM] _Exit\n"); ((ef)dlsym(RTLD_NEXT,"_Exit"))(c); __builtin_unreachable(); }
typedef void (*af)(void);
void abort(void){ wr("[DCM] abort ra="); whex((unsigned long)__builtin_return_address(0)); ((af)dlsym(RTLD_NEXT,"abort"))(); __builtin_unreachable(); }
typedef void (*pf)(void*);
void pthread_exit(void*r){ wr("[DCM] pthread_exit\n"); ((pf)dlsym(RTLD_NEXT,"pthread_exit"))(r); __builtin_unreachable(); }
__attribute__((constructor)) static void ini(void){ wr("[DCM] loaded\n"); }

#include <stdio.h>
#include <unistd.h>
int main(int argc, char** argv) {
    printf("AARCH64_HELLO ok: pid=%d uid=%d argc=%d\n", getpid(), getuid(), argc);
    return 0;
}

#include <syscall.h>

int main() {
    syscall0(SYS_CLEAR);
    return 0;
}

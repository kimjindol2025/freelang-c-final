#include <unistd.h>

void main() {
    write(1, "Hello, World!\n", 14);
    _exit(0);
}

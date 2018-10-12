#include <cstdio>

extern "C" { void hello_world(); }

int main() {
    printf("Hello World");
    hello_world();

    return 0;
}

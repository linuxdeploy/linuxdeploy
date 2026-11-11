#include <cstdio>
#include <simple_library.h>
#include <dependency_parent.h>

int main() {
    printf("Hello World");
    hello_world();

    const auto value = dependency_parent_init();
    dependency_parent_destroy();
    return value == 42 ? 0 : 1;
}

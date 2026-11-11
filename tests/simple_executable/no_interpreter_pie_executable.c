int no_dependencies_value(void);

// built without the C runtime, so this is the entry point
void _start(void) {
    no_dependencies_value();
    for (;;) {}
}

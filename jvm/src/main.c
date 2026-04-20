#include "jvm.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) { fprintf(stderr, "Usage: %s <bytecode>\n", argv[0]); return 1; }
    JVM vm;
    jvm_init(&vm);
    if (jvm_load_program(&vm, argv[1]) != 0) return 1;
    jvm_run(&vm);
    jvm_free(&vm);
    return 0;
}

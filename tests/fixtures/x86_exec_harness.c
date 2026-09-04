/*
 * Arnes x86-32 real, analogo a arm64_exec_harness.c, para ejecutar el
 * codigo x86 ORIGINAL de forma nativa (sin traducir) y poder comparar su
 * resultado contra la traduccion ARM64 -- ver tests/test_ir_end_to_end.cpp.
 *
 * Uso: x86_exec_harness <fichero_bytes_x86_32> <valor_entrada>
 * Carga el fichero en memoria ejecutable e invoca como int(*)(int)
 * (cdecl, argumento en la pila), imprime el resultado en stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef int (*GeneratedFn)(int) __attribute__((regparm(1)));

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "uso: %s <bytes.bin> <entrada>\n", argv[0]);
        return 2;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 2; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* mem = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); fclose(f); return 2; }

    if (fread(mem, 1, (size_t)size, f) != (size_t)size) {
        perror("fread");
        fclose(f);
        return 2;
    }
    fclose(f);

    int input = atoi(argv[2]);
    GeneratedFn fn = (GeneratedFn)mem;
    int result = fn(input);

    printf("%d\n", result);
    return 0;
}

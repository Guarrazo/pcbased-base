/*
 * Arnes de ejecucion dinamica para validar cpu::arm64::Emitter -- NO es
 * parte del proyecto pc-arcade-switch en si (no compila para Switch, no
 * usa libnx), es una herramienta de TEST que corre en ARM64 Linux real
 * (bajo qemu-aarch64 en el sandbox de desarrollo, o en hardware ARM64
 * real) para ejecutar de verdad los bytes que produce el Emitter, no solo
 * comprobar su codificacion a nivel de bits.
 *
 * Uso: arm64_exec_harness <fichero_bytes_arm64> <valor_x0_entrada>
 * Carga el fichero en memoria ejecutable, lo invoca como
 * int64_t(*)(int64_t x0), y escribe el resultado (registro X0 al volver)
 * en stdout como un entero decimal.
 *
 * Ver docs/JIT.md y tests/test_arm64_dynamic_exec.cpp.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

typedef int64_t (*GeneratedFn)(int64_t);

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "uso: %s <bytes.bin> <x0_entrada>\n", argv[0]);
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

    int64_t x0_in = strtoll(argv[2], NULL, 0);
    GeneratedFn fn = (GeneratedFn)mem;
    int64_t result = fn(x0_in);

    printf("%lld\n", (long long)result);
    return 0;
}

/*
 * Fuente C minima para generar, en tiempo de build, un ELF32 x86 real
 * (dinamicamente enlazado, con relocaciones y simbolos reales de libc) --
 * ver docs/ROADMAP.md: no se dispone de un binario real de Lindbergh en
 * este entorno de desarrollo (esos son juegos comerciales con copyright,
 * no algo que este proyecto pueda o deba obtener por su cuenta), asi que
 * esto sirve como sustituto legal y reproducible: ejercita el ElfLoader
 * contra la salida real de un compilador (GCC con -m32), no contra un ELF
 * construido a mano como el de test_elf_loader.cpp. No pretende parecerse
 * al codigo real de Lindbergh (compilador y libc distintos, con toda
 * seguridad mas modernos que los de mediados-2000) -- solo valida que el
 * loader funciona con un ELF32 dinamico de verdad. La validacion final
 * contra un binario real de Lindbergh sigue pendiente (docs/ROADMAP.md).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int global_counter = 0;

int add_numbers(int a, int b) {
    return a + b;
}

int main(int argc, char** argv) {
    char* buf = (char*)malloc(64);
    strcpy(buf, "hola desde x86-32 real");
    global_counter = add_numbers(2, 3);
    printf("%s (%d)\n", buf, global_counter);
    free(buf);
    return 0;
}

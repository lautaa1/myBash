#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "command.h"
#include "execute.h"
#include "parser.h"
#include "parsing.h"
#include "builtin.h"
#include "obfuscated.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void show_prompt(void) {
    char ruta[PATH_MAX];

    // Obtengo la ruta actual
    if (getcwd(ruta, sizeof(ruta)) == NULL) {
        perror("getcwd() error");
        return;
    }

    // Mostrar el prompt estilo bash
    printf("Mybash>%s> ", ruta);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    pipeline pipe;
    Parser input;
    bool quit = false;

    input = parser_new(stdin);
    while (!quit) {
        show_prompt();  // Mostrar el prompt actualizado
        pipe = parse_pipeline(input);
        if (pipe != NULL) {
            execute_pipeline(pipe);
            pipeline_destroy(pipe);
        }
        quit = parser_at_eof(input);
    }
    parser_destroy(input); input = NULL;
    return EXIT_SUCCESS;
}
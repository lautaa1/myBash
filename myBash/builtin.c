#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "strextra.h"
#include "command.h"
#include "parsing.h"
#include "builtin.h"
#include "tests/syscall_mock.h"


bool builtin_is_internal(scommand cmd){
    assert(cmd != NULL);
    bool res = false;
    char *cmdd = scommand_front(cmd);
    if (strcmp(cmdd, "exit") == 0 || strcmp(cmdd, "cd") == 0 || strcmp(cmdd, "help") == 0){
        res = true;
    }
    return res;
}

bool builtin_alone(pipeline p){
    assert(p != NULL);
    bool res = (pipeline_length(p) == 1 && builtin_is_internal(pipeline_front(p)));
    return res;
}

void builtin_run(scommand cmd){
    assert (builtin_is_internal(cmd));
    char *cmdd = scommand_front(cmd);
    if(strcmp(cmdd, "help")==0){
        printf("WOLOS BASH SHELL v0.0.1\n\n");
        printf("Grupo 35:\n\tLautaro Gastón Peralta\n\tFrancisco Asis\n\tMaría Luz Varas\n\tSantiago Ezequiel Sanchez\n\n");
        printf("Commandos internos:\n\t*help\n\t\tMuestra la version del programa, los integrantes de grupo y los comandos internos\n\n\t*cd\n\t\tCambia directorio\n\n\t*exit\n\t\tCierra el programa\n\n");
    } else if (!strcmp(cmdd, "cd")){
        //const chardirec = NULL; //va a contener la info sobre a que directorio dirigirse
        unsigned int cmd_length = scommand_length(cmd);
        //usa input de argumento para cd 
        if(cmd_length > 2){
            printf("cd: too many arguments\n"); 
        } else if (cmd_length == 2){
            //si no se pasa ningun argumento se pasa al directorio HOME
            scommand_pop_front(cmd);
            char *arg = strmerge("", scommand_front(cmd));
            int result = chdir(arg);
            if (result != 0){
                perror("cd");
            }
        } else {
            printf("cd: missing argument\n");
        }
    }else if (strcmp (cmdd, "exit")==0){
        exit (EXIT_SUCCESS);
    }
}
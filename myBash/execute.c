#include "tests/syscall_mock.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "builtin.h"
#include "command.h"
#include "strextra.h"
#include "execute.h"

static void execute_cmd (scommand cmd){
    assert(cmd != NULL && !scommand_is_empty(cmd));

    char * redir_in = scommand_get_redir_in (cmd);
    char * redir_out = scommand_get_redir_out (cmd);
    unsigned int length_cmd = scommand_length(cmd);

    char ** array = malloc(sizeof(char*) * (length_cmd + 1));

    if (array == NULL){
        perror("error en el malloc del array");
        exit(EXIT_FAILURE);
    }
    
    for (unsigned int i = 0; i < length_cmd; i++){//lleno el array con los strings de comandos
        // array[i] = scommand_front(cmd);
        //scommand_pop_front(cmd); 
        char* cmd_str = scommand_front(cmd);  // Obtiene el comando
        if (cmd_str == NULL) {
            fprintf(stderr, "Error: scommand_front(cmd) devolvió NULL en el índice %d\n", i);
            free(array);
            exit(EXIT_FAILURE);
        }
        array[i] = strdup(cmd_str);  // Copia el comando a un nuevo espacio de memoria
        if (array[i] == NULL) {
            perror("strdup");
            free(array);
            exit(EXIT_FAILURE);
        }
        scommand_pop_front(cmd);
    }
    array[length_cmd] = NULL; // el ultimo elem tiene q ser NULL

    if (redir_in != NULL){ //Redireccion de entrada
        int in_nuevo = open (redir_in,O_RDONLY,O_CREAT);
        if (in_nuevo < 0){ //error por si el archivo no existe
            perror("Error al abrir archivo de entrada");
            free(array);
            exit(EXIT_FAILURE);
        }
        int dup = dup2(in_nuevo,STDIN_FILENO);
        if (dup < 0){
            perror("Error al redireccionar la entrada");
            free(array);
            exit(EXIT_FAILURE);
        }
        int closed_in = close(in_nuevo);
        if (closed_in < 0){
            perror("Error al cerrar el archivo de entrada");
            free(array);
            exit(EXIT_FAILURE);
        }
    }
    if (redir_out != NULL){ //Redireccion de salida
        int out_nuevo = open (redir_out, O_WRONLY | O_CREAT| O_TRUNC, 0644); //ver tea de S_IRUSR
        if (out_nuevo < 0){
            perror("Error al abrir archivo de salida");
            free(array);
            exit(EXIT_FAILURE);
        }
        int dup = dup2(out_nuevo,STDOUT_FILENO);
        if (dup < 0){
            perror("Error al redireccionar la salida");
            free(array);
            exit(EXIT_FAILURE);
        }
        int close_out = close(out_nuevo);
        if (close_out < 0){
            perror("Error al cerrar el archivo de salida");
            free(array);
            exit(EXIT_FAILURE);
        }
    }
    
    int err_return = execvp(array[0],array); //con execvp ejecuta los comandos del array
    if (err_return < 0){
        perror("Error al ejecutar comando");
        free(array);
        exit(EXIT_FAILURE);
    }
    //execvp(array[0],array); //con execvp ejecuta los comandos del array
    
    //perror("execvp no funciono");
    
    free(array);
    exit(EXIT_FAILURE); // el comando execvp no tiene q retornar si lo hace es porque fallo

}

void execute_pipeline(pipeline apipe) {
    assert(apipe != NULL);
    //manejo el caso del pipe nulo
    if (apipe == NULL || pipeline_length(apipe) == 0) {
        return;
    }
    
    bool weit = pipeline_get_wait(apipe);
    unsigned int length = pipeline_length(apipe);

    //caso del pipeline con un solo comando
    if (length == 1) {
        scommand cmd = pipeline_front(apipe);
        if (builtin_is_internal(cmd)) {
            builtin_run(cmd);
        } else {
            pid_t PID = fork();
            if (PID < 0) {
                perror("Error al crear el proceso hijo.");
                exit(EXIT_FAILURE);
            }
            if (PID == 0) { // Child process
                execute_cmd(cmd);
                exit(EXIT_SUCCESS); // Ensure child process exits after executing 
            }
            if (weit == true) {
                wait(NULL); // Parent waits for the child process
            }
        }
    } else {
        // Caso para pipes con más de un comando.
        int *paip = malloc(sizeof(int) * (length - 1) * 2);
        if (paip == NULL) {
            perror("malloc pipe");
            exit(EXIT_FAILURE);
        }

        // Create the necessary pipes between commands
        for (unsigned int i = 0; i < length - 1; i++) {
            if (pipe(paip + i * 2) < 0) {
                perror("pipe");
                free(paip);
                exit(EXIT_FAILURE);
            }
        }

        unsigned int counter = 0;
        pid_t pid;

        while (!pipeline_is_empty(apipe)) {
            scommand cmd = pipeline_front(apipe);
            pid = fork();

            if (pid < 0) { //Fallo al crear el proceso hijo
                perror("Error al crear el proceso hijo.");
                free(paip);
                exit(EXIT_FAILURE);
            }
            if (pid == 0) { //Proceso hijo
                if (counter > 0) {
                    // Connect the input to the previous pipe
                    if (dup2(paip[counter - 2], STDIN_FILENO) < 0) {
                        perror("Error al redirigir la entrada");
                        free(paip);
                        exit(EXIT_FAILURE);
                    }
                }

                if (counter < (length - 1) * 2) {
                    // Connect the output to the next pipe
                    if (dup2(paip[counter + 1], STDOUT_FILENO) < 0) {
                        perror("Error al redirigir la salida");
                        free(paip);
                        exit(EXIT_FAILURE);
                    }
                }

                // Close all pipe ends
                for (unsigned int i = 0; i < (length - 1) * 2; i++) {
                    close(paip[i]);
                }

                execute_cmd(cmd); // Execute the command
                exit(EXIT_SUCCESS); // Ensure child process exits after executing
            }

            if (pid > 0) { // Proceso padre
                pipeline_pop_front(apipe); // Remove the executed command
                counter += 2; // Move to the next pipe
            }
        }

        // Close all pipes in the parent
        for (unsigned int i = 0; i < (length - 1) * 2; i++) {
            close(paip[i]);
        }

        // Wait for all child processes to finish if required
        if (weit) {
            for (unsigned int i = 0; i < length; i++) {
                wait(NULL);
            }
        }
        free(paip);
    }
}


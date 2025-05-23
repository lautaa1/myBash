#include <stdlib.h>
#include <stdbool.h>

#include "parsing.h"
#include "parser.h"
#include "command.h"

bool syntax_error = false;

static scommand parse_scommand(Parser p) {
    /* Devuelve NULL cuando hay un error de parseo */
    assert(!parser_at_eof(p));
    scommand result = NULL;
    arg_kind_t new;
    char *temp = NULL;
    temp = parser_next_argument(p, &new);
    bool cmd_end = false;
    if (temp == NULL){
        return NULL;
    }
    result = scommand_new();
    while (!cmd_end){
        if ((temp == NULL) && (new == ARG_INPUT || new == ARG_OUTPUT)){ // Caso en que se espera un archivo y no se recibe
            result = scommand_destroy(result);
            result = NULL;
            cmd_end = true;
            printf("Syntax error\n");
        } else {
            if (temp == NULL){ // Caso en que se termina el comando
                cmd_end = true;
            } else { // Caso en que se recibe un comando
                if (new == ARG_NORMAL){
                scommand_push_back(result,temp);
                }
                else if (new == ARG_INPUT){
                scommand_set_redir_in(result,temp);
                }
                else if (new == ARG_OUTPUT){
                   scommand_set_redir_out(result,temp);
                } else {
                    return NULL;
                }
            }
        }
        if (!cmd_end){
            temp = parser_next_argument(p, &new);
        }
    }
    return result;
}

pipeline parse_pipeline(Parser p) {
    assert(p!=NULL && !parser_at_eof (p));
    pipeline result = NULL;
    scommand cmd = NULL;
    bool error = false;
    bool another_pipe=true;
    bool OP_BACKGROUND;
    bool garbage;

    cmd = parse_scommand(p);
    error = (cmd==NULL); /* Comando inválido al empezar */

    if (!error){ // Caso en que el comando es valido
        result = pipeline_new();
        pipeline_push_back(result,cmd);
        parser_op_pipe(p,&another_pipe);
    }
    
    while (another_pipe && !error) { // Caso en que hay mas de un comando
        cmd = parse_scommand(p);
        error = (cmd==NULL);
        if (error){ // Caso en que el comando es invalido
            pipeline_destroy(result);
            result = NULL;
            printf("Invalid command\n");
        } else { // Caso en que el comando es valido
            pipeline_push_back(result,cmd);
            if (parser_at_eof(p) == true){ 
                break;
            } else {
                parser_op_pipe(p,&another_pipe);
            }
        }
        if (parser_at_eof(p) == true){ 
            break;
        } else {
            parser_op_pipe(p,&another_pipe);
        }
    }
    parser_op_background(p,&OP_BACKGROUND);
    if (OP_BACKGROUND){ // Caso en que el comando es en background
        pipeline_set_wait(result,false);
    }
    another_pipe = false;
    parser_op_pipe(p,&another_pipe);
    if (another_pipe){ // Caso en que hay mas de un pipe
        printf("Syntax error\n");
        syntax_error = true;
        result = pipeline_destroy(result);
        result = NULL;
    }

    parser_garbage(p,&garbage);

    return result; // MODIFICAR
}


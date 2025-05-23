#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> /* para tener bool */
#include <glib.h>
#include <assert.h>

#include "strextra.h"
#include "command.h"

#include "tests/syscall_mock.h"


/* scommand: comando simple.
 * Ejemplo: ls -l ej1.c > out < in
 * Se presenta como una secuencia de cadenas donde la primera se denomina
 * comando y desde la segunda se denominan argumentos.
 * Almacena dos cadenas que representan los redirectores de entrada y salida.
 * Cualquiera de ellos puede estar NULL indicando que no hay redirección.
 *
 * En general, todas las operaciones hacen que el TAD adquiera propiedad de
 * los argumentos que le pasan. Es decir, el llamador queda desligado de la
 * memoria utilizada, y el TAD se encarga de liberarla.
 *
 * Externamente se presenta como una secuencia de strings donde:
 *           _________________________________
 *  front -> | cmd | arg1 | arg2 | ... | argn | <-back
 *           ---------------------------------
 *
 * La interfaz es esencialmente la de una cola. A eso se le
 * agrega dos accesores/modificadores para redirección de entrada y salida.
 */

// ([char*],char*,char*)
struct scommand_s{
    GSList* cmd_list;
    char* input_redirection;
    char* output_redirection;
};

scommand scommand_new(void){
    scommand new_s = malloc(sizeof(struct scommand_s));
    if (new_s == NULL){
        printf("Error al crear el comando simple\n");
        exit(EXIT_FAILURE);
    }
    
    new_s->cmd_list = NULL;
    new_s->input_redirection = NULL;
    new_s->output_redirection = NULL;
    assert (new_s != NULL && scommand_is_empty(new_s) && scommand_get_redir_in (new_s) == NULL && scommand_get_redir_out (new_s) == NULL);
    return new_s;
}

scommand scommand_destroy(scommand self){
    assert(self != NULL);
    g_slist_free_full(self->cmd_list, free);
    self->cmd_list = NULL;

    free(self->input_redirection);
    self->input_redirection = NULL;

    free(self->output_redirection);
    self->output_redirection = NULL;

    free(self);
    self = NULL;

    assert (self == NULL);
    return self;
}

/* Modificadores */

void scommand_push_back(scommand self, char * argument){
    assert(self!=NULL && argument!=NULL);
    self->cmd_list = g_slist_append(self->cmd_list,argument);
    assert(!scommand_is_empty(self));
}

void scommand_pop_front(scommand self){
    assert(self!=NULL && !scommand_is_empty(self));
    free(self->cmd_list->data);
    self->cmd_list = g_slist_remove(self->cmd_list, self->cmd_list->data);
}

void scommand_set_redir_in(scommand self, char * filename){
    assert(self!=NULL);
    free(self->input_redirection);
    self->input_redirection = NULL;
    self->input_redirection = filename;
}
void scommand_set_redir_out(scommand self, char * filename){
    assert(self!=NULL);
    free(self->output_redirection);
    self->output_redirection = NULL;
    self->output_redirection = filename;
}

/* Proyectores */

bool scommand_is_empty(const scommand self){
    assert(self != NULL);
    bool res = (self->cmd_list == NULL);
    return res;
}

unsigned int scommand_length(const scommand self){
    assert(self!=NULL);
    unsigned int res = g_slist_length(self->cmd_list);
    assert((res == 0) == scommand_is_empty(self));
    return res;
}

char * scommand_front(const scommand self){
    assert(self!=NULL && !scommand_is_empty(self));
    char *result = self->cmd_list->data;
    assert(result!=NULL);
    return result;
}

char * scommand_get_redir_in(const scommand self){
    assert(self!=NULL);
    char *result = NULL;
    result = self->input_redirection;
    return result;
}

char * scommand_get_redir_out(const scommand self){
    assert(self!=NULL);
    char *result = NULL;
    result = self->output_redirection;
    return result;
}

char * scommand_to_string(const scommand self){
    assert(self!=NULL);
    char *result = strdup("");
    GSList *aux = self->cmd_list;
    if (aux != NULL){
        result = str_merge_new(result, aux->data);
        aux = g_slist_next(aux);
        while (aux != NULL){ 
            result = str_merge_new(result, " ");
            result = str_merge_new(result, aux->data);
            aux = g_slist_next(aux);
        }
    }
    if (self ->input_redirection != NULL){
        result = str_merge_new(result, " < ");
        result = str_merge_new(result, scommand_get_redir_in(self));
    }
    if (self ->output_redirection != NULL){
        result = str_merge_new(result, " > ");
        result = str_merge_new(result, scommand_get_redir_out(self));
    }
    assert(scommand_is_empty(self) || scommand_get_redir_in(self)==NULL || scommand_get_redir_out(self)==NULL || strlen(result)>0);
    return result;
}



/*
 * pipeline: tubería de comandos.
 * Ejemplo: ls -l *.c > out < in  |  wc  |  grep -i glibc  &
 * Secuencia de comandos simples que se ejecutarán en un pipeline,
 *  más un booleano que indica si hay que esperar o continuar.
 *
 * Una vez que un comando entra en el pipeline, la memoria pasa a ser propiedad
 * del TAD. El llamador no debe intentar liberar la memoria de los comandos que
 * insertó, ni de los comandos devueltos por pipeline_front().
 * pipeline_to_string() pide memoria internamente y debe ser liberada
 * externamente.
 *
 * Externamente se presenta como una secuencia de comandos simples donde:
 *           ______________________________
 *  front -> | scmd1 | scmd2 | ... | scmdn | <-back
 *           ------------------------------
 */

struct pipeline_s{
    GSList* smcd;
    bool wait;
};

pipeline pipeline_new(void){
    pipeline new_p = malloc(sizeof(struct pipeline_s));
    if (new_p == NULL){
        printf("Error al crear el pipeline\n");
        exit(EXIT_FAILURE);
    }
    new_p->smcd = NULL;
    new_p->wait = true;
    assert( new_p != NULL && pipeline_is_empty(new_p) && pipeline_get_wait(new_p));
    return new_p;
}

pipeline pipeline_destroy(pipeline self){
    assert(self != NULL);
    while (!pipeline_is_empty(self)){
        self->smcd->data = scommand_destroy(self->smcd->data);
        self->smcd = self->smcd->next;
    }
    
    g_slist_free_full(self->smcd,free);
    self->smcd = NULL;
    free(self);
    self = NULL;
    assert(self == NULL);
    return self;
}

/* Modificadores */

void pipeline_push_back(pipeline self, scommand sc){
    assert(self!=NULL && sc!=NULL);
    self->smcd = g_slist_append(self->smcd, sc);
    assert(!pipeline_is_empty(self));
}

void pipeline_pop_front(pipeline self){
    assert(self!=NULL && !pipeline_is_empty(self));
    scommand_destroy(self->smcd->data);
    self->smcd = g_slist_remove(self->smcd, self->smcd->data);
}

void pipeline_set_wait(pipeline self, const bool w){
    assert(self!=NULL);
    self->wait = w;
}

/* Proyectores */

bool pipeline_is_empty(const pipeline self){
    assert( self!=NULL);
    bool res = self->smcd == NULL;
    return res;
}

unsigned int pipeline_length(const pipeline self){
    assert(self!=NULL);
    unsigned int res = g_slist_length(self->smcd);
    assert((res==0) == pipeline_is_empty(self));
    return res;
}

scommand pipeline_front(const pipeline self){
    assert(self!=NULL && !pipeline_is_empty(self));
    scommand p_front = self->smcd->data;
    return p_front;
}

bool pipeline_get_wait(const pipeline self){
    assert(self!=NULL);
    bool waitt = self->wait;
    return waitt;
}

char * pipeline_to_string(const pipeline self){
    assert(self!=NULL);
    char *result = strdup("");
    GSList *aux = self->smcd;
    if (aux != NULL){
        result = str_merge_new(result, scommand_to_string(aux->data));
        aux = g_slist_next(aux);
        while (aux != NULL){
            result = str_merge_new(result, " | ");
            result = str_merge_new(result, scommand_to_string(aux->data));
            aux = g_slist_next(aux);
        }
        if (!pipeline_get_wait(self)){
            result = str_merge_new(result, " & ");
        }     
    }
    assert(pipeline_is_empty(self) || pipeline_get_wait(self) || strlen(result)>0);
    return result;
}
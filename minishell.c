#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>


void main(void){
    char buff[1024];
    tline *line;    //Datos del comando introducido

    int i, j;

    printf("msh> ");
    char *entrada = fgets(buff, 1024, stdin);
    while(entrada != NULL){
        line = tokenize(buff);   //Se toqueniza el comando

        //Si no se introduce nada se hace un "salto de línea"
        if(line == NULL) continue;
        

        //Se ejecuta el comando
        int pid = fork();
        //Hijo
        if(pid == 0){
            //Si se tiene una redireción de salida
            if(line->redirect_output != NULL){
                //Si el archivo NO existe se crea y se abre. Si el archivo SI existe se abre
                FILE *file = fopen(line->redirect_output, "w");
                //Comprobar si ha fallado fopen
                if(file == NULL) fprintf(stderr, "No se pudo abrir el archivo %s\n", line->redirect_output);
                int fd_file = fileno(file);
                dup2(fd_file, STDOUT_FILENO);
                fclose(file);
            }

            //Si se tiene una redirección de entrada
            if(line->redirect_input != NULL) {
                FILE *file = fopen(line->redirect_input, "r");
                //Si el archivo de entrada no existe hay que mostrar un error
                if(file == NULL) fprintf(stderr, "No se pudo abrir el archivo de entrada %s\n", line->redirect_input);
                int fd_file = fileno(file);  //Se coge el fd del archivo
                dup2(fd_file, STDIN_FILENO);    //Se sustituye el fd de entrada estandar por el fd del fichero para que la entrada se lea de ahí
                fclose(file);
            }

            execvp(line->commands[0].filename, line->commands[0].argv);
            fprintf(stderr, "Ha habido un problema ejecutando %s\n", line->commands[0].filename);
            exit(1);
        }
        //padre 
        else{
            wait(NULL);
            printf("msh> ");
            entrada = fgets(buff, 1024, stdin);
        }
    }

    printf("Se ha salido del bucle");

}



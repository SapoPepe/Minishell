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
        
        if(line->ncommands == 1) {
            //Se ejecuta el comando
            pid_t pid = fork();
            //Hijo
            if(pid == 0){
                //Si se tiene una redireción de salida
                if(line->redirect_output != NULL){
                    //Si el archivo NO existe se crea y se abre. Si el archivo SI existe se abre
                    FILE *file = fopen(line->redirect_output, "w");
                    //Comprobar si ha fallado fopen
                    if(file == NULL) {
                        fprintf(stderr, "No se pudo abrir el archivo %s\n", line->redirect_output);
                        exit(1);
                    }
                    int fd_file = fileno(file);
                    dup2(fd_file, STDOUT_FILENO);
                    fclose(file);
                }

                //Si se tiene una redirección de entrada
                if(line->redirect_input != NULL) {
                    FILE *file = fopen(line->redirect_input, "r");
                    //Si el archivo de entrada no existe hay que mostrar un error
                    if(file == NULL) {
                        fprintf(stderr, "No se pudo abrir el archivo de entrada %s\n", line->redirect_input);
                        exit(1);
                    }
                    int fd_file = fileno(file);  //Se coge el fd del archivo
                    dup2(fd_file, STDIN_FILENO);    //Se sustituye el fd de salida estandar por el fd del fichero para que la salida se guarde ahí
                    fclose(file);

                }

                //Si se tiene redirección de error
                if (line->redirect_error != NULL) {
                    FILE *file = fopen(line->redirect_error, "w");
                    //Comprobamos si ha fallado el open
                    if (file == NULL) {
                        fprintf(stderr, "No se pudo abrir el archivo %s\n", line->redirect_error);
                        exit(1);
                    }
                    int fd_file = fileno(file);
                    dup2(fd_file, STDERR_FILENO);
                    fclose(file);
                }

                //Si el comando no existe
                if(line->commands[0].filename == NULL) {
                    fprintf(stderr, "%s: no se encuentra el mandato\n", line->commands[0].argv[0]);
                    exit(1);
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


        //Hay más de un comando (obligatoriamente tienen que estar separados por un pipe)
        else {
            pid_t command1, command2;
            int h1h2[2];
            pipe(h1h2);

            //El primer comando será el inicio (solo se podrá modificar su entrada estandar)
            command1 = fork();
            if(command1 == 0) {
                //Si el primer comando tiene una redirección de entrada
                if(line->redirect_input != NULL) {
                    FILE *file = fopen(line->redirect_input, "r");
                    if(file == NULL) {
                        fprintf(stderr, "No se pudo abrir el archivo de entrada %s\n", line->redirect_input);
                        exit(1);
                    }
                    int fd_file = fileno(file);
                    dup2(fd_file, STDIN_FILENO/*Descriptor de fichero de la entrada para el primer comando */);
                    fclose(file);
                }

                close(h1h2[0]); //Se cierra el extremo de lectura del pipe
                dup2(h1h2[1], STDOUT_FILENO); //Se pasa lo que saque el primer comando al extremo de escritura del pipe

                execvp(line->commands[0].filename, line->commands[0].argv);
                fprintf(stderr, "Ha habido un problema ejecutando %s\n", line->commands[0].filename);
                exit(1);
            }

            //El segundo comando será el final (solo se podrá modificar su salida estandar)
            command2 = fork();
            if(command2 == 0) {
                //Si tiene redirección de salida
                if(line->redirect_output != NULL) {
                    //Si el archivo NO existe se crea y se abre. Si el archivo SI existe se abre
                    FILE *file = fopen(line->redirect_output, "w");
                    //Comprobar si ha fallado fopen
                    if(file == NULL) fprintf(stderr, "No se pudo abrir el archivo %s\n", line->redirect_output);
                    int fd_file = fileno(file);
                    dup2(fd_file, STDOUT_FILENO/*Descriptor de fichero de la salida para el ultimo comando */);
                    fclose(file);
                }

                //Si se tiene redirección de error
                if (line->redirect_error != NULL) {
                    FILE *file = fopen(line->redirect_error, "w");
                    //Comprobamos si ha fallado el open
                    if (file == NULL) {
                        fprintf(stderr, "No se pudo abrir el archivo %s\n", line->redirect_error);
                        exit(1);
                    }
                    int fd_file = fileno(file);
                    dup2(fd_file, STDERR_FILENO);
                    fclose(file);
                }

                close(h1h2[1]); //Se cierra el extremo de escritura del pipe
                dup2(h1h2[0], STDIN_FILENO);

                execvp(line->commands[1].filename, line->commands[1].argv);
                fprintf(stderr, "Ha habido un problema ejecutando %s\n", line->commands[1].filename);
                exit(1);
            }

            //El padre cierra los dos extremos del pipe
            close(h1h2[0]);
            close(h1h2[1]);

            //El padre espera por los dos hijos
            waitpid(command1, NULL, 0);
            waitpid(command2, NULL, 0);

            printf("msh> ");
            entrada = fgets(buff, 1024, stdin);
        }

    }

    printf("Se ha salido del bucle");



}



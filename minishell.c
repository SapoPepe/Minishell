#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

int contador, **h1h2;
pid_t *pids;
tline *line;    //Datos del comando introducido

void main(void){
    char buff[1024];

    printf("msh> ");
    while(fgets(buff, 1024, stdin) != NULL){
        line = tokenize(buff);   //Se toqueniza el comando

        //Si no se introduce nada se hace un "salto de línea"
        if(line == NULL) continue;


        //Si el comando introducido es un "cd"
        if (strcmp(line->commands[0].argv[0], "cd") == 0) {
            int cd = 0;
            //Si no tiene dirección se accederá directamente a lo que contenga la variable HOME
            if(line->commands[0].argc == 1) {
                char *home = getenv("HOME");
                if(home == NULL) printf("cd: HOME is unset\n");
                else cd = chdir(home);
            }
            else cd = chdir(line->commands[0].argv[1]);

            //Si no se ha podido cambiar de directorio
            if(cd == -1) printf("cd: %s: No such file or directory\n", line->commands[0].argv[1]);

        }

        //Si el comando introducido es "jobs"
        else if(strcmp(line->commands[0].argv[0], "jobs") == 0){
            printf("Jobs\n");
        }

        //Si el comando introducido es "fg"
        else if(strcmp(line->commands[0].argv[0], "fg") == 0) {
            printf("Fg\n");
        }

        //Si es cualquier otro comando
        else {
            //Se crea un array para todos los pid_t de cada comando en la intrucción
            pids = calloc(line->ncommands, sizeof(pid_t));

            //Se crea un array para todos los pipes necesarios
            h1h2 = calloc(line->ncommands-1,  sizeof(int *));
            for(int j=0; j < line->ncommands-1; j++) {
                h1h2[j] = calloc(2, sizeof(int));
            }

            //Se inician todos los pipes
            for(int j=0; j < line->ncommands - 1; j++) pipe(h1h2[j]);


            //Se crean todos los procesos, se guardarn sus pid_t en el array creado anteriormente y se quedan esperando a recibir una señal
            for(contador = 0; contador < line->ncommands; contador++) {

                pid_t aux = fork();

                //Hijo se ejecuta
                if(aux == 0){

                    //Si el hijo es el primer comando y tiene una redirección
                    if(contador == 0) {
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

                        //Se utiliza el primer pipe para pasar la info del comando1 al comando2 en el caso de que haya más de un comandoo
                        if (line->ncommands > 1) {
                            dup2(h1h2[contador][1], STDOUT_FILENO);
                        }
                    }


                    //Si el hijo es el último y tiene una redirección
                    else if(contador == line->ncommands-1) {
                        if(line->redirect_output != NULL){
                            //Si el archivo NO existe se crea y se abre. Si el archivo SI existe se abre
                            FILE *file = fopen(line->redirect_output, "w");
                            //Comprobar si ha fallado fopen
                            if(file == NULL) fprintf(stderr, "No se pudo abrir el archivo %s\n", line->redirect_output);
                            int fd_file = fileno(file);
                            dup2(fd_file, STDOUT_FILENO);
                            fclose(file);
                        }

                        if(line->ncommands > 1) {
                            //Se coge como input lo que venga por el pipe y se imprime por la salida estandar a no ser que haya una redirección en el caso de que haya más de un comando
                            dup2(h1h2[contador-1][0], STDIN_FILENO);
                        }

                    }


                    //Cualquier otro caso solamente se va pasando la información mediante pipes entre los comandos intermedios
                    else {
                        dup2(h1h2[contador-1][0], STDIN_FILENO);    //El extremo de lectura del anterior pipe va como entrada a este comando
                        dup2(h1h2[contador][1], STDOUT_FILENO);     //La salida del comando se mandará al extremo de escritura del siguiente pip
                    }


                    // Cerrar los pipes que no son necesarios
                    for (int i = 0; i < line->ncommands - 1; i++) {
                        //Si es el primer comando
                        if (contador == 0) {
                            close(h1h2[i][0]); // Cerrar lectura del primer pipe
                        }
                        //Si es el último comando
                        else if (contador == line->ncommands - 1) {
                            close(h1h2[i][1]); // Cerrar escritura del último pipe
                        }
                        //En cualquier otro caso se cierran los dos extremos
                        else {
                            close(h1h2[i][0]);
                            close(h1h2[i][1]); // Cerrar ambos extremos en los comandos intermedios
                        }
                    }

                    //Se ejecuta el comando
                    execvp(line->commands[contador].filename, line->commands[contador].argv);
                    fprintf(stderr, "Ha habido un problema ejecutando %s\n", line->commands[contador].filename);
                    exit(1);

                }

                //Padre guarda el pid_t del hijo
                else {
                    pids[contador] = aux;
                }

            }

            //el padre cierra todos los pipes
            for (int j = 0; j < line->ncommands - 1; j++) {
                close(h1h2[j][0]);
                close(h1h2[j][1]);
            }

            // Esperar a que todos los procesos hijos terminen
            for (int i = 0; i < line->ncommands; i++) {
                waitpid(pids[i], NULL, 0);
            }
        }

        printf("msh> ");
        fflush(stdout);

    }

    printf("Se ha salido del bucle");
}

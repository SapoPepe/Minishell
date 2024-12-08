#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#define MAX_JOBS 1024

void addJob(pid_t pid, char *line);
void jobsUpdate();
void removeOldJobs();

typedef struct {
    pid_t pid;
    char command[1024];
    int active; //1 = active // 0 = inactive // -1 información basura
}Job;


Job jobs[MAX_JOBS]; //Lista donde se irán guardando todos los comandos que se encuentren en ejecución
int contador, **h1h2, contadorJob = 0, lastPosJob = 0;
pid_t *pids;
tline *line;    //Datos del comando introducido


void main(void){
    //Se inicializan todos los valores de active a -1 del array de jobs para que no haya problemas posteriores
    for(int i=0; i<MAX_JOBS; i++) jobs[i].active = -1;

    char buff[1024];
    char *start_line;
    //Ignoramos las señales SIGINT y SIGQUIT
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    printf("msh> ");
    while(fgets(buff, 1024, stdin) != NULL){
        //Se actualizan los estados de los jobs en background
        jobsUpdate();

        start_line = buff;
        //Nos saltamos todos los espacios o tabulaciones que haya al principio de la entrada
        while (isspace(*start_line) && *start_line != '\n') start_line++;


        //Comprobamos si se ha introducido algún valor antes de hacer el tokenize
        if (start_line[0] != '\n') line = tokenize(start_line);  //Se toqueniza el comando
        else line = NULL;


        //Si no se introduce nada se hace un "salto de línea"
        if(line == NULL){
            printf("msh> ");
            fflush(stdout);
            continue;
        }

        //Si el comando introducido es un exit se sale de la minishell
        if (!strcmp(line->commands[0].argv[0], "exit")) exit(0);


        //Si el comando introducido es un "cd"
        if (!strcmp(line->commands[0].argv[0], "cd")) {
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
        else if(!strcmp(line->commands[0].argv[0], "jobs")){
            if(contadorJob != 0) {
                for(int i=0; i<=lastPosJob; i++) {
                    if(jobs[i].active == 1) printf("[%d]+  Running\t\t%s", i, jobs[i].command);
                    else if (jobs[i].active == 0) printf("[%d]-  Done\t\t%s", i, jobs[i].command);
                }
            }
        }

        //Si el comando introducido es "fg"
        else if(!strcmp(line->commands[0].argv[0], "fg")) {
            printf("Fg\n");
        }

        //Si es cualquier otro comando
        else {
            //Si alguno de los comandos introducidos no existe no se hace nada y se informa de un error
            int inexistencia = -1, j = 0;
            while(j < line->ncommands && inexistencia == -1) {
                if(line->commands[j].filename == NULL) inexistencia = j;
                j++;
            }

            if(inexistencia != -1) fprintf(stderr, "%s: command not found\n", line->commands[inexistencia].argv[0]);
            else{
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
                            //Tiene redirección de entrada
                            if(line->redirect_input != NULL) {
                                FILE *file = fopen(line->redirect_input, "r");
                                //Si el archivo de entrada no existe hay que mostrar un error
                                if(file == NULL) {
                                    fprintf(stderr, "Input redirection error:\nAn error occurred while opening %s file\n", line->redirect_input);
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
                        if(contador == line->ncommands-1) {
                            //Tiene redirección de error
                            if(line->redirect_error != NULL){
                                //Si el archivo NO existe se crea y se abre. Si el archivo SI existe se abre
                                FILE *file = fopen(line->redirect_error, "w");
                                //Comprobar si ha fallado fopen
                                if(file == NULL) fprintf(stderr, "Error redirection error:\nAn error occurred while opening %s file\n", line->redirect_error);
                                int fd_file = fileno(file);
                                dup2(fd_file, STDERR_FILENO);
                                fclose(file);
                            }

                            //Tiene redirección de salida
                            else if(line->redirect_output != NULL){
                                //Si el archivo NO existe se crea y se abre. Si el archivo SI existe se abre
                                FILE *file = fopen(line->redirect_output, "w");
                                //Comprobar si ha fallado fopen
                                if(file == NULL) fprintf(stderr, "Output redirection error:\nAn error occurred while opening %s file\n", line->redirect_output);
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
                        if(contador != 0 && contador != line->ncommands-1 && line->ncommands != 1) {
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


                        //Si se tiene que hacer el comando en background, se muestra el pid del proceso
                        if (line->background == 1){
                            printf("[%d] %d\n", contadorJob+1, getpid());
                            printf("msh> ");
                            fflush(stdout);
                        }


                        execvp(line->commands[contador].filename, line->commands[contador].argv);
                        //Si no existe o ha habido un problema ejecutando el comando se imprime un mensaje de error
                        fprintf(stderr, "Execution error: An error occurred while executing %s\n", line->commands[contador].argv[0]);
                        exit(1);

                    }

                    //Padre
                    else {
                        //Padre guarda el pid_t del hijo
                        pids[contador] = aux;

                        //Si el proceso está en background se añade a la lista de procesos el nuevo proceso
                        if(line->background == 1) addJob(aux, start_line);
                    }

                }

                //el padre cierra todos los pipes
                for (int i = 0; i < line->ncommands - 1; i++) {
                    close(h1h2[i][0]);
                    close(h1h2[i][1]);
                }

                //Si el comando no se tiene que ejecutar en background
                if (line->background == 0){
                    // Esperar a que todos los procesos hijos terminen
                    for (int i = 0; i < line->ncommands; i++) {
                        waitpid(pids[i], NULL, 0);
                    }
                }

                //Liberamos toda la memoria que habíamos reservado para los PIDs
                free(pids);
                //Liberamos la memoria que habíamos reservado para los pipes
                for(int j=0; j < line->ncommands-1; j++) {
                    free(h1h2[j]);
                }
                free(h1h2);
            }
        }

        if (line->background == 0){
            printf("msh> ");
            fflush(stdout);
        }


        //Se eliminan los jobs que ya han acabado su ejecución
        removeOldJobs();
    }
    exit(0);
}




//Añade un job a la lista
void addJob(pid_t pid, char *line) {
    if(contadorJob <= MAX_JOBS) {
        int pos = 0;
        //Se busca la primera posición libre en el array
        while (jobs[pos].active != -1) pos++;
        //Si la posición alcanzada para poder guardar el nuevo job es mayor a la antigua mayor posición se guarda la nueva mayor posición (sirve para saber donde está el extremo ocupadp del array)
        if(pos > lastPosJob) lastPosJob = pos;

        //Se sobreescribe lo que hubiera guardado en esa posición por los datos del job actual
        jobs[pos].pid = pid;
        strcpy(jobs[pos].command, line);
        jobs[pos].active = 1;
        contadorJob++;
    }
    else fprintf(stderr, "Max concurrent background jobs reached!\n");
}




//Actualiza el estado de los jobs guardados
void jobsUpdate() {
    for(int i=0; i<=lastPosJob; i++) {
        if(jobs[i].active == 1) {
            int status;
            pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);

            //Si result toma un valor distinto de 0 significa que el hijo ya ha terminado y muerto, de lo contrario tomará el valor 0
            //Si es así, actualizamos el estado del job
            if(result != 0) jobs[i].active = 0;
        }
    }
}




void removeOldJobs() {
    for(int i=0; i<=contadorJob; i++) {
        if(jobs[i].active == 0) {
            jobs[i].active = -1;
            contadorJob--;
        }
    }
}
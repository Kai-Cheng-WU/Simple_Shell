#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* maximum number of args in a command */
#define LENGTH 20 
#define JOBS_SIZE 1024


/*
README

For the pipe command, please separate each argument by a space
e.g. ls | wc instead of ls|wc

*/

pid_t fg_pid = 0;
pid_t bg_pid[1024];

//Kill foreground process with CTRL + C
void handle_sigint(int signal) {
    if (signal == SIGINT){
        //So we don't kill the shell itself 
        if(fg_pid != 0){
            printf("\n KILLING the process: %d\n", fg_pid);
            kill(fg_pid, SIGTERM);
            fg_pid = 0;
        }
    }
    return;
}

//Don't do anything on CTRL + Z
void handle_sigtstp(int signal) {
    if (signal == SIGTSTP){
        printf("This signal is supposed to be ignored... \n");
    }
    return;
}

//Observe when a background child process terminates itself and update the job list
void handle_sigchld(int signal){
    if (signal == SIGCHLD){
        pid_t currpid = waitpid(-1, NULL, WNOHANG);
        for(int i=0; i<JOBS_SIZE; i++){
            if(bg_pid[i] == currpid)
                bg_pid[i] = 0;
        }
    }
    return;
}

/* Parses the user command in char *args[]. 
   Returns the number of entries in the array */

int getcmd(char *prompt, char *args[], int *background, int *redirection, int *piping){

    int length, flag, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    // check fof invalid command
    if (length == 0) {
        return 0; 
    }
    // check for CTRL + D
    else if (length == -1){
        exit(0);  
    }

    /* check if background is specified */
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } 
    else {
        *background = 0;
    }

    // Here you can add the logic to detect piping or redirection in the command and set the flags
    /* check if the output needs to be redirected */
    if ((loc = index(line, '>')) != NULL) {
        *redirection = 1;
        *loc = ' ';
    } 
    else {
        *redirection = 0;
    }

    /* check if there is piping */
    if ((index(line, '|')) != NULL) {
        *piping = 1;
    } 
    else {
        *piping = 0;
    }
    
    // Clear args 
    memset(args, '\0', LENGTH);

    // Splitting the command and putting the tokens inside args[]
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) { 
                token[j] = '\0'; 
            } 
        }
        if (strlen(token) > 0) {
            args[i++] = token;
        }
    }
    args[i] = '\0';
    
    return i;
}


int main(void) { 
    char* args[LENGTH];
    int redirection; /* flag for output redirection */
    int piping; /* flag for piping */
    int bg;     /* flag for running processes in the background */
    int cnt; /* count of the arguments in the command */

    // Check for signals
    if (signal(SIGINT, handle_sigint) == SIG_ERR){ 
        printf("ERROR: could not bind signal handler for SIGINT\n");
        exit(1);
    }
    
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR){ 
        printf("ERROR: could not bind signal handler for SIGTSTP\n");
        exit(1);
    }

    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR){
        printf("ERROR: could not bind signal handler for SIGCHLD\n");
        exit(1);
    }

    while(1){
        // reset flags 
        fg_pid = 0;
        bg = 0;    
        redirection = 0;
        piping = 0;

        if ((cnt = getcmd("\n>> ", args, &bg, &redirection, &piping)) == 0) {
            printf("Invalid command\n");
            continue;
        }

        //the cd command
        if(strcmp(args[0], "cd") == 0){
            if(cnt == 2){
                printf(args[1]);
                chdir(args[1]);
            }
            else{
                printf("wrong number of argument provided!");
                exit(1);
            }
        }

        //the pwd command
        else if(strcmp(args[0], "pwd") == 0){
            char cwd[1024];
            getcwd(cwd, sizeof(cwd));
            printf("the current working directory is: \n");
            printf(cwd);
        }
        
        //Note that this implementation of echo only prints the first word in case
        // there is a space in-between.
        //the echo command
        else if(strcmp(args[0], "echo") == 0){
            if(cnt == 1){
            }
            else{
                printf(args[1]);
            }
        }

        //the fg command
        else if(strcmp(args[0], "fg") == 0){
            if(cnt == 1){
                printf("Please give the job number of the job you want to put in the foreground");
            }
            else if(cnt > 2){
                printf("Please only give 1 integer job number!");
            }
            //We only accept job number from 0 to 1023 to avoid segmentation fault, as we store pids in a array of size 1024.
            else if(atoi(args[1]) >= 1024 || atoi(args[1]) < 0){
                printf("Please specify a valid job number");
            }
            //brings the process to the foreground and wait for it to finish, while updating the background job list
            else {
                fg_pid = bg_pid[atoi(args[1])];
                bg_pid[atoi(args[1])] = 0;
                waitpid(fg_pid, NULL, 0);
            }

        }

        //the jobs command
        else if(strcmp(args[0], "jobs") == 0){
            printf("\n Current jobs running in the background: \n");
            for(int i=0; i<JOBS_SIZE; i++){
                if(bg_pid[i] != 0)
                    printf("Job Number: %d  PID: %d \n", i, bg_pid[i]);
            }
        }
        
        //the exit command
        else if(strcmp(args[0], "exit") == 0){
            printf("Goodbye cruel world! :( \n");
            exit(0);
        }

        else{
            pid_t pid = fork();

            //this process is running in the foreground
            if(bg == 0)
                fg_pid = pid;

            // For the child process
            if (pid == 0) {
                // execute the command
                // look for the redirection flag
                if(redirection == 1){
                    if(cnt >= 2){
                        int fd = open(args[cnt-1], O_RDWR | O_CREAT | O_APPEND , S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                        //remove the destination file name from the arguments; this took me forever to debug
                        args[cnt-1] = NULL;
                        execvp(args[0], args);
                    }
                    else{
                        printf("Please specify a valid output location! \n");
                        exit(1);
                    }
                }
                // look for the piping flag
                if(piping == 1){
                    if(cnt >= 3) {
                        //Make the pipe
                        int pipeFD[2];
                        if (pipe(pipeFD) == -1){
                            printf("ERROR: pipe failed \n");
                        }
                        pid_t pipid = fork();

                        //child process for piping, the "grand-child"
                        if (pipid == 0){
                            dup2(pipeFD[0], STDIN_FILENO);
                            close(pipeFD[1]);
                            close(pipeFD[0]);
                            
                            char* secondArgs[LENGTH];
                            int flag = 0;
                            int j = 0;
                            for(int i=0; i<cnt; i++){
                                if(flag == 1){
                                    if(args[i] != NULL){
                                        secondArgs[j] = strdup(args[i]);
                                        j++;
                                    }
                                }
                                if(strcmp(args[i], "|") == 0){
                                    flag = 1;
                                }
                            }
                            //To avoid potential NULL termination error
                            secondArgs[j] = NULL;
                            
                            
                            return execvp(secondArgs[0], secondArgs);
                        }
                        else if(pipid == -1){
                            printf("ERROR: first fork failed\n");
                            exit(1);
                        }
                        //parent process for piping (the original child)
                        else{
                            char* firstArgs[LENGTH];
                            //this counter was taken out of the for loop so we could set the final arg to NULL
                            int i=0;
                            for(; i<cnt; i++){
                                if(strcmp(args[i], "|") == 0){
                                    break;
                                }
                                else{
                                    if(args[i] != NULL){
                                        firstArgs[i] = strdup(args[i]);
                                    }
                                    
                                }
                            }
                            //To avoid potential NULL termination error
                            firstArgs[i] = NULL;
                            
                            dup2(pipeFD[1], STDOUT_FILENO);
                            close(pipeFD[0]);
                            close(pipeFD[1]);
                            return execvp(firstArgs[0], firstArgs);

                        }

                    }
                    else{
                        printf("Invalid number of arguments for piping!  \n");
                        exit(1);
                    }

                }


                if (piping == 0 && redirection == 0){
                    if (execvp(args[0], args) < 0) { 
                        printf("exec failed, this command is not understood \n");
                        exit(1); 
                    }
                }
                

                exit(0); /* child termination */
            }  
            
            else if (pid == -1){      
                printf("ERROR: fork failed\n");
                exit(1);
            }
            
            // For the parent process
            else {
                // If the child process is not in the bg => wait for it
                if (bg == 0){ 
                    waitpid(pid, NULL, 0);
                } 

                // Otherwise, keep track of child process in an array (Job number + PID)
                else{
                    int jobNum = 0;
                    for(int i=0; i<JOBS_SIZE; i++){
                        if(bg_pid[i] == 0){
                            jobNum = i;
                            bg_pid[jobNum] = pid;
                            break;
                        }
                    }
                }

            }

        }
        //clear all args at the end of each loop  
        for(int i=0; i<LENGTH; i++){
            args[i] = NULL;
        }
        
    }
}

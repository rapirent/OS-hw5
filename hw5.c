#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#define LSH_RL_BUFSIZE 1024
#define TOKEN_MAX_BUF 64
#define ARG_MAX 10
// #define IGNORE_TOKEN " \t\r\n\a"

void lsh_loop(void);
char *lsh_read_line(void);
char ***split_line(char *line);
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_num_builtins();
// int lsh_execute(char ***args);
int build_pipe(char ***args);
// int lsh_launch(char **args);
int shell_launch(char **argv, int fd_in, int fd_out,
                 int pipes_count, int pipes_fd[][2]);
char *space_strip(char *str);


char *builtin_func_name[] = {
  "cd",
  "help",
  "exit"
};
//use the pointer to function to build the shell function
int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit
};


int main(int argc, char **argv)
{
  // Load config files, if any.

  // Run command loop.
  char **token_container;
  int status = 1;

    while(status)
    {
        //prompt
        // fputs("0$ ", stdout);
        printf("0$ ");
        //get the line from stdin
        char *line = NULL;
        ssize_t bufsize = 0; // have getline allocate a buffer for us
        getline(&line, &bufsize, stdin);
        status = build_pipe(split_line(line));
        // printf("status = %d\n", status);
        // token_container = split_line(line);
        // status = lsh_execute(token_container);

        // free(line);
        // free(token_container);
    }
    return 0;
}
char ***split_line(char *line)
{
    //pointer to the pointer(array)
    static char *tokens[TOKEN_MAX_BUF+1];
    memset(tokens, '\0', sizeof(tokens));
    int i;
    tokens[0] = space_strip(strtok(line, "|<>"));
    for(i = 1; i <= TOKEN_MAX_BUF; i++)
    {
        tokens[i] = space_strip(strtok(NULL, "|<>"));
        if(tokens[i] == NULL)
        {
            break;
        }
    }
    //rows store tokens, columns store the arguments
    static char *argvs_array[TOKEN_MAX_BUF + 1][ARG_MAX + 1];
    static char **token_container[TOKEN_MAX_BUF + 1];

    // char **token_container = malloc(bufsize * sizeof(char*));

    memset(argvs_array, '\0', sizeof(argvs_array));
    memset(token_container, '\0', sizeof(token_container));
    int j;
    for(i = 0; tokens[i]; i++)
    {
        token_container[i] = argvs_array[i];
        //first element of every rows is the origin command token
        token_container[i][0] = strtok(tokens[i], " \t\r\n\a");//ignore the space
        for(j = 1; j <= ARG_MAX; j++)
        {
            token_container[i][j] = strtok(NULL, " \t\r\n\a");
            if(token_container[i][j] == NULL)
            {
                break;
            }
        }
    }
    // while(token != NULL)
    // {
    //     token_container[position] = token;
    //     position++;

    //     if(position >= bufsize)
    //     {
    //         bufsize += TOKEN_MAX_BUF;
    //         token_container = realloc(token_container, bufsize * sizeof(char*));
    //         if (token_container == NULL)
    //         {
    //             fprintf(stderr, "ERROR:memory allocation fails\n");
    //             exit(EXIT_FAILURE);
    //         }
    //     }
    //     token = strtok(NULL, ignore_token);
    // }
    // token_container[position] = NULL;
    return token_container;
}
char *space_strip(char *str)
{
    if(!str)
    {
        return str;
    }

    while(isspace(*str))
    {
        ++str;
    }

    char *last = str;
    while(*last != '\0')
    {
        ++last;
    }
    last--;

    while(isspace(*last))
    {
        *last-- = '\0';
    }

    return str;
}
// int lsh_execute(char ***args)
int build_pipe(char ***args)
{
    int return_value;
    if(args[0] == NULL)
    {
        return 1;
    }
    int pipeline_count = 0;
    int pipes_fd[TOKEN_MAX_BUF][2];
    int command_count = 0;
    while(args[command_count]!=NULL)
    {
        command_count++;
    }
    //we need to build commnad_count -1 no pipe
    pipeline_count = command_count - 1;
    int i;
    for(i = 0; i < pipeline_count; i++)
    {
        if(pipe(pipes_fd[i]) == -1)//fails
        {
            fprintf(stderr, "ERROR:create pipe fails\n");
            exit(EXIT_FAILURE);
        }
    }

    for(i = 0; i < command_count; i++)
    {
        //first will not be the pipe out
        int fd_in = (i == 0)?
                    (STDIN_FILENO):
                    (pipes_fd[i - 1][0]);
        //last will not be the pipe in
        int fd_out = (i == command_count - 1)?
                    (STDOUT_FILENO):
                    (pipes_fd[i][1]);
        return_value = shell_launch(args[i], fd_in, fd_out, pipeline_count, pipes_fd);
    }
    for(i = 0; i < pipeline_count; i++)
    {
        close(pipes_fd[i][0]);
        close(pipes_fd[i][1]);
    }
    for(i = 0; i < command_count; i++)
    {
        int status;
        wait(&status);
    }
    return return_value;
}


int shell_launch(char **argv, int fd_in, int fd_out,
                 int pipes_count, int pipes_fd[][2])
{
    pid_t pid,wpid;
    int status;
    pid = fork();
    if(pid < 0)
    {
        fprintf(stderr, "ERROR:create child process fails\n");
        exit(EXIT_FAILURE);
    }
    else if(pid == 0)//child process
    {
        // int builtin_function_tag=0;
        if(fd_in != STDIN_FILENO)
        {
            dup2(fd_in, STDIN_FILENO);
        }
        if(fd_out != STDOUT_FILENO)
        {
            dup2(fd_out, STDOUT_FILENO);
        }
        int i;
        for(i = 0; i < pipes_count; i++)
        {
            close(pipes_fd[i][0]);
            close(pipes_fd[i][1]);
        }
        for(i = 0; i < sizeof(builtin_func_name) / sizeof(char *); i++)
        {
            if(strcmp(argv[0], builtin_func_name[i]) == 0)
            {
                //builtin_func
                //raise the flag
                exit(EXIT_SUCCESS);
            }
        }
            if(execvp(argv[0], argv) == -1)
            {
                fprintf(stderr,
                        "Error: Unable to load the executable %s.\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
        //never reach here
        exit(EXIT_FAILURE);
        // if(execvp(args[0], args) == -1)
        // {
            // fprintf(stderr, "ERROR:create child process fails\n");
        // }
        // exit(EXIT_SUCCESS);
    }
    else//parent process
    {
    // do {
    //   wpid = waitpid(pid, &status, WUNTRACED);
    // } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        // int status = -1;
        // wait(&status);//wait for the child process
        int i;
        for(i = 0; i < sizeof(builtin_func_name) / sizeof(char *); i++)
        {
            if(strcmp(argv[0], builtin_func_name[i]) == 0)
            {
                //builtin_func
                //call function by the pointer to function
                return (*builtin_func[i])(argv);
            }
        }
        // printf("The Child Process Returned with %d\n", WEXITSTATUS(status));
        // do
        // {
            // wpid = waitpid(pid, &status, WUNTRACED);
        // }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

///XXX

// int pipe(int fds[2]);
//fds[0]: read only
//fds[1]: write only
// int pipeline(**args)
// {
//     int pipe_fd[2];
//     if(pipe(pipe_fd)==-1)//fail
//     {
//         fprintf(stderr, "ERROR:create pipe fails\n");
//         exit(EXIT_FAILURE);
//     }
//     pid_t pid;
//     pid = fork();
//     if(pid < 0)
//     {
//         fprintf(stderr, "ERROR:create child process fails\n");
//         exit(EXIT_FAILURE);
//     }
//     else if(pid == 0)
//     {
//         close(pipe_fd[0]);
//         dup2(pipe_fd[1], STDIN_FILENO);
//         close(pipe_fd[1]);
//         exit(EXIT_SUCCESS);
//     }
//     else
//     {
//          -- In the Parent Process --------

//         close(pipe_fd[1]); /* Close write end, since we don't need it. */
//         /* 不會用到 Write-end 的 Process 一定要把 Write-end 關掉，不然 pipe
//            的 Read-end 會永遠等不到 EOF。 */
//         dup2(pipe_fd[0], STDIN_FILENO);
//         close(pipe_fd[0]);
//         int i;
//         for (i = 0; i < RANDOM_NUMBER_NEED_COUNT; ++i)
//         {
//             int gotnum;
//             /* 從 Read-end 把資料拿出來 */
//             read(pipe_fd[0], &gotnum, sizeof(int));
//             printf("got number : %d\n", gotnum);
//         }
//     }
// }
/*
  Builtin function implementations.
*/
int lsh_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "ERROR: there is expected argument to cd command\n");
    }
    else
    {
        if(chdir(args[1]) != 0)
        {
            fprintf(stderr,
                    "Error: Unable to execute the chdir function to %s\n",
                    args[1]);
            exit(EXIT_FAILURE);
        }
    }
    return 1;
}

int lsh_help(char **args)
{
    printf("The following function are built in:\n");
    int i;
    for(i = 0; i < sizeof(builtin_func_name) / sizeof(char *); i++)
    {
        printf("  %s\n", builtin_func_name[i]);
    }
    return 1;
}

int lsh_exit(char **args)
{
    printf("you choose exit\n");
    return 0;
}
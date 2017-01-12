#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#define LSH_RL_BUFSIZE 1024
#define TOKEN_MAX_BUF 64
#define ARG_MAX 10
// #define IGNORE_TOKEN " \t\r\n\a"

char ***split_line(char *line);
int builtin_cd(char **args);
// int shell_help(char **args);
int shell_exit(char **args);
// int lsh_execute(char ***args);
int build_pipe(char ***args);
// int lsh_launch(char **args);
// int shell_help(char **args);
int shell_launch(char **argv, int fd_in, int fd_out,
                 int pipes_count, int pipes_fd[][2]);
char *space_strip(char *str);
void signal_handle(int case_int);

char *builtin_func_name[] = {
  "cd",
  // "help",
  "exit"
};
//use the pointer to function to build the shell function
int (*builtin_function_pointer[]) (char **) = {
    &builtin_cd,
    // &shell_help,
    &shell_exit
};

char *redirect_in = NULL;
char *redirect_out = NULL;


int main(int argc, char **argv)
{
    int status = 1;
    //signal handle
    int getline_int;
    if(signal(SIGINT, signal_handle)==SIG_ERR)
    {
        fprintf(stderr, "ERROR:signal capture fails\n");
        exit(EXIT_FAILURE);
    }
    if(signal(SIGTSTP, signal_handle)==SIG_ERR)
    {
        fprintf(stderr, "ERROR:signal capture fails\n");
        exit(EXIT_FAILURE);
    }
    while(status)
    {
        //prompt
        // fputs("0$ ", stdout);
        printf("0$ ");
        //get the line from stdin
        char *line = NULL;
        redirect_in = NULL;
        redirect_out = NULL;
        ssize_t bufsize = 0; // have getline allocate a buffer for us
        getline_int = getline(&line, &bufsize, stdin);
        if(getline_int<=0)continue;
        status = build_pipe(split_line(line));

        // free(line);
        // free(token_container);
    }
    return 0;
}
void signal_handle(int case_int)
{
    switch (case_int)
    {
        case SIGINT:
            //do nothing
            break;
        case SIGTSTP:
            break;
        default:
            printf("Capture a signal %d",case_int);
            break;
    }
}

char ***split_line(char *line)
{
    //pointer to the pointer(array)
    static char *tokens[TOKEN_MAX_BUF+1];
    memset(tokens, '\0', sizeof(tokens));
    int i;
    tokens[0] = space_strip(strtok(line, "|"));
    for(i = 1; i <= TOKEN_MAX_BUF; i++)
    {
        tokens[i] = space_strip(strtok(NULL, "|"));
        if(tokens[i] == NULL)
        {
            break;
        }
    }
    int j;
    for(j = 0; tokens[0][j]; j++)
    {
        //'<' first
        if(tokens[0][j] == '<')
        {
            tokens[0] = space_strip(strtok(tokens[0], "<"));
            redirect_in = space_strip(strtok(NULL, "<"));
            if(strstr(redirect_in, ">")!=NULL)
            {
                redirect_in = space_strip(strtok(redirect_in, ">"));
                redirect_out = space_strip(strtok(NULL, ">"));
            }
            break;
        }
        //'>' first
        if(tokens[0][j] == '>')
        {
            tokens[0] = space_strip(strtok(tokens[0], ">"));
            redirect_out = space_strip(strtok(NULL, ">"));
            if(strstr(redirect_out, "<")!=NULL)
            {
                redirect_out = space_strip(strtok(redirect_out, "<"));
                redirect_in = space_strip(strtok(NULL, "<"));
            }
            break;
        }
    }
    if(redirect_out == NULL && i-1 != 0)
    {
        tokens[i-1] = space_strip(strtok(tokens[i-1], ">"));
        redirect_out = space_strip(strtok(NULL, ">"));
    }
    // printf("after\n");
    // printf("tokens[0]=%s\n",tokens[0]);
    // printf("redirect_in=%s\n",redirect_in);
    // printf("tokens[i-1]=%s\n",tokens[i-1]);
    // printf("redirect_out=%s\n",redirect_out);
    //rows store tokens, columns store the arguments
    static char *argvs_array[TOKEN_MAX_BUF + 1][ARG_MAX + 1];
    static char **token_container[TOKEN_MAX_BUF + 1];

    // char **token_container = malloc(bufsize * sizeof(char*));

    memset(argvs_array, '\0', sizeof(argvs_array));
    memset(token_container, '\0', sizeof(token_container));
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
    if(args[0][0] == NULL)
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
        int fd_in;
        int fd_out;
        //first will not be the pipe out
        if(redirect_in!=NULL)
        {
            fd_in = (i == 0)?
                    (open(redirect_in,O_RDONLY | O_CREAT, 0644)):
                    (pipes_fd[i - 1][0]);
        }
        else
        {
            fd_in = (i == 0)?
                    (STDIN_FILENO):
                    (pipes_fd[i - 1][0]);
        }

        //last will not be the pipe in
        if(redirect_out!=NULL)
        {
            fd_out = (i == command_count - 1)?
                    (open(redirect_out, O_WRONLY | O_CREAT | O_TRUNC, 0644)):
                    (pipes_fd[i][1]);
        }
        else
        {
            fd_out = (i == command_count - 1)?
                    (STDOUT_FILENO):
                    (pipes_fd[i][1]);

        }
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
        waitpid(0, &status, WUNTRACED);
        // wait(&status);
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
    }
    else//parent process
    {
        if(argv[0][0]=='\0')
        {
            return 1;
        }
        int i;
        for(i = 0; i < sizeof(builtin_func_name) / sizeof(char *); i++)
        {
            if(strcmp(argv[0], builtin_func_name[i]) == 0)
            {
                //builtin_func
                //call function by the pointer to function
                return (*builtin_function_pointer[i])(argv);
            }
        }
    }
    return 1;
}

int builtin_cd(char **args)
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

int shell_exit(char **args)
{
    printf("you choose exit\n");
    return 0;
}
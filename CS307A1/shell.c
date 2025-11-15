#include "parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>


static void rstrip_newline(char *s) {
    if(!s) return;
    size_t len = strlen(s);
    if (len && s[len-1] == '\n')    
        s[len-1] = '\0';
}

static int is_empty_line(const char *s){
    if(!s)return 1;
    while(*s){
        if(!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}
/* -1 = use normal STDIN or inFile ----- -1 = use normal STDOUT or outFile */
static void execute_pipeline_with_redirs_fd(CmdVec *vec,const char *inFile,const char *outFile,int in_fd, int out_fd) {        
    int n = (int)vec->n;
    if (n <= 0) return;

    /* Single command */
    if (n == 1) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }
        if (pid == 0) {
            if (in_fd != -1) { if (dup2(in_fd, STDIN_FILENO) == -1) { perror("dup2 in_fd"); _exit(1); } }
            if (out_fd != -1){ if (dup2(out_fd, STDOUT_FILENO)== -1) { perror("dup2 out_fd"); _exit(1);} }

            if (in_fd == -1 && inFile) {
                int infd = open(inFile, O_RDONLY);
                if (infd < 0) { perror("open inFile"); _exit(1); }
                if (dup2(infd, STDIN_FILENO) == -1) { perror("dup2 in"); _exit(1); }
                close(infd);
            }
            if (out_fd == -1 && outFile) {
                int outfd = open(outFile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
                if (outfd < 0) { perror("open outFile"); _exit(1); }
                if (dup2(outfd, STDOUT_FILENO) == -1) { perror("dup2 out"); _exit(1); }
                close(outfd);
            }

            execvp(vec->argvs[0][0], vec->argvs[0]);
            perror("execvp failed");
            _exit(1);
        }
        waitpid(pid, NULL, 0);
        return;
    }

    /* Multiple commands (pipeline) */
    int pipes[2*(n-1)];
    for (int i=0;i<n-1;i++) if (pipe(pipes + 2*i)==-1){ perror("pipe"); return; }

    for (int i=0;i<n;i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }
        if (pid == 0) {
            /* left side (stdin) */
            if (i==0) {
                if (in_fd != -1) {
                    if (dup2(in_fd, STDIN_FILENO)==-1){ perror("dup2 in_fd"); _exit(1); }
                } else if (inFile) {
                    int infd = open(inFile, O_RDONLY);
                    if (infd < 0) { perror("open inFile"); _exit(1); }
                    if (dup2(infd, STDIN_FILENO)==-1){ perror("dup2 in"); _exit(1); }
                    close(infd);
                }
            } else {
                if (dup2(pipes[2*(i-1)], STDIN_FILENO)==-1){ perror("dup2 stdin"); _exit(1); }
            }

            /* right side (stdout) */
            if (i==n-1) {
                if (out_fd != -1) {
                    if (dup2(out_fd, STDOUT_FILENO)==-1){ perror("dup2 out_fd"); _exit(1); }
                } else if (outFile) {
                    int outfd = open(outFile, O_CREAT|O_WRONLY|O_TRUNC,0644);
                    if (outfd < 0) { perror("open outFile"); _exit(1); }
                    if (dup2(outfd, STDOUT_FILENO)==-1){ perror("dup2 out"); _exit(1); }
                    close(outfd);
                }
            } else {
                if (dup2(pipes[2*i+1], STDOUT_FILENO)==-1){ perror("dup2 stdout"); _exit(1); }
            }

            for (int j=0;j<2*(n-1);j++) close(pipes[j]);
            execvp(vec->argvs[i][0], vec->argvs[i]);
            perror("execvp failed");
            _exit(1);
        }
    }
    for (int j=0;j<2*(n-1);j++) close(pipes[j]);
    for (int i=0;i<n;i++) wait(NULL);
}

/* Run a pipeline and CAPTURE its output: returns a read-end FD that
   produces the pipeline output*/
static int capture_pipeline_output(CmdVec *vec, const char *inFile, int in_fd_override) {
    int p[2];
    if (pipe(p)==-1){ perror("pipe"); return -1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); close(p[0]); close(p[1]); return -1; }

    if (pid == 0) { /* child: stdout -> p[1] */
        close(p[0]);
        if (dup2(p[1], STDOUT_FILENO)==-1){ perror("dup2 p[1]"); _exit(1); }
        close(p[1]);
        execute_pipeline_with_redirs_fd(vec, inFile, NULL,in_fd_override,-1);/* in fd override or -1 stdout already dup’d to pipe */
        _exit(0);
    }
    /* parent */
    close(p[1]);
    waitpid(pid, NULL, 0);
    return p[0];
}

static int run_looppipe_and_get_fd(compiledCmd *cmd, int start_in_fd) {
    int n = cmd->loopLen;
    if (n <= 0) return start_in_fd; 

    int in_fd = start_in_fd;  /* initial input for the first iteration */

    for (int iter = 0; iter < n; iter++) {
        /* capture output of one iteration, using in_fd as stdin override */
        int out_rd = capture_pipeline_output(&cmd->inLoop, NULL, in_fd);

        if (in_fd != -1 && in_fd != STDIN_FILENO)
            close(in_fd);

        if (out_rd < 0) return -1;
        in_fd = out_rd;  /* becomes input of next iteration */
    }
    return in_fd;  /* final output FD after n iterations */
}


static void execute_pipeline_with_redirs(CmdVec *vec, const char *inFile, const char *outFile) {
    int n = vec->n;
    if (n <= 0) return;

    /* Case 1: Single command */
    if (n == 1) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        }
        if (pid == 0) {  // Child
            /* Input redirection */
            if (inFile) {
                int infd = open(inFile, O_RDONLY);
                if (infd < 0) {_exit(1); }
                if (dup2(infd, STDIN_FILENO) == -1) {_exit(1); }
                close(infd);
            }
            /* Output redirection */
            if (outFile) {
                int outfd = open(outFile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (outfd < 0) { _exit(1); }
                if (dup2(outfd, STDOUT_FILENO) == -1) { _exit(1); }
                close(outfd);
            }

            execvp(vec->argvs[0][0], vec->argvs[0]);
            perror("execvp failed");
            _exit(1);
        }
        waitpid(pid, NULL, 0);
        return;
    }

    /* Case 2: Multiple commands (pipeline) */
    int pipefds[2 * (n - 1)];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe");
            return;
        }
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        }

        if (pid == 0) { // CHILD PROCESS 
            /* Connect STDIN to previous pipe (if not first cmd) */
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    _exit(1);
                }
            }

            /* Connect STDOUT to next pipe (if not last cmd) */
            if (i < n - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    _exit(1);
                }
            }

            /* Apply redirections to first/last command only */
            if (i == 0 && inFile) {
                int infd = open(inFile, O_RDONLY);
                if (infd < 0) { perror("open inFile"); _exit(1); }
                if (dup2(infd, STDIN_FILENO) == -1) { perror("dup2 in"); _exit(1); }
                close(infd);
            }
            if (i == n - 1 && outFile) {
                int outfd = open(outFile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (outfd < 0) { perror("open outFile"); _exit(1); }
                if (dup2(outfd, STDOUT_FILENO) == -1) { perror("dup2 out"); _exit(1); }
                close(outfd);
            }

            /* Close all pipe FDs in the child */
            for (int j = 0; j < 2 * (n - 1); j++) {
                close(pipefds[j]);
            }

            execvp(vec->argvs[i][0], vec->argvs[i]);
            perror("execvp failed");
            _exit(1);
        }
        // Parent continues to next command
    }

    /* Parent: close all pipes and wait for children */
    for (int j = 0; j < 2 * (n - 1); j++) {
        close(pipefds[j]);
    }
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
}

static void execute_looppipe(compiledCmd *cmd) {
   int n = cmd->loopLen;
   if (n <= 0)return;

   int in_fd = STDIN_FILENO;
   int pipefd[2];

   while(n != 0){
         if (pipe(pipefd) == -1) {
              perror("pipe");
              return;
         }
    
         pid_t pid = fork();
         if (pid < 0) {
              perror("fork");
              return;
         }
    
         if (pid == 0) { // Child
              dup2(in_fd, STDIN_FILENO);
              if (n > 1) {
                dup2(pipefd[1], STDOUT_FILENO);
              } else if (cmd->outFile) {
                int outfd = open(cmd->outFile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (outfd < 0) { perror("open outFile"); _exit(1); }
                dup2(outfd, STDOUT_FILENO);
                close(outfd);
              }
    
              close(pipefd[0]);
              execvp(cmd->inLoop.argvs[0][0], cmd->inLoop.argvs[0]);
              perror("execvp failed");
              _exit(1);
         } else { // Parent
              waitpid(pid, NULL, 0);
              close(pipefd[1]);
              in_fd = pipefd[0];
              n--;
         }
   }
}

int main(void) {
    //Parser Setup
    sparser_t sp;
    if(!initParser(&sp)){
        fprintf(stderr, "Failed to initialize parser\n");
        exit(0);
    }
    //Interactive Loop
    char *input = NULL;
    size_t bufsize = 0;
    
    while(1){
        fflush(stdout);
        printf("SUShell$ ");
        
        ssize_t Line;
        Line = getline(&input, &bufsize, stdin);
        //printf("Input is stored in Line variable: %s", input);

        if(Line == -1){
            printf("Exiting shell...\n");
            freeParser(&sp);
            exit(0);
        }
        rstrip_newline(input);
        if(is_empty_line(input)){
            continue; // if input is empty or only whitespace, prompt again
        }

        /*
        if (input[0] == '\0') {
            continue; // if input is empty, prompt again
        }*/

        compiledCmd cmd;
        if(!compileCommand(&sp, input, &cmd)){
            fprintf(stderr, "parse error\n");
            continue;
        }
        
        if (cmd.isQuit) {
            printf("Exiting shell...\n");
            freeCompiledCmd(&cmd);
            exit(0);
        }
        //Singe command or plain pipeline without loop
        int stream_fd = -1;

        /* BEFORE: if present, run and capture output.
            If no 'before' but there is cmd.inFile, open it to feed loop/after. */
        if (cmd.before.n > 0) {
            stream_fd = capture_pipeline_output(&cmd.before, cmd.inFile, -1);
        } else if (cmd.inFile) {
            stream_fd = open(cmd.inFile, O_RDONLY);
            if (stream_fd < 0) { perror("open inFile"); freeCompiledCmd(&cmd); continue; }
        }

        /* LOOP: if present, run N iterations, using current stream_fd as input */
        if (cmd.inLoop.n > 0 && cmd.loopLen > 0) {
            int new_fd = run_looppipe_and_get_fd(&cmd, (stream_fd==-1 ? STDIN_FILENO : stream_fd));
            if (stream_fd != -1 && stream_fd != STDIN_FILENO) close(stream_fd);
            stream_fd = new_fd;
        }

        /* AFTER: if present, feed stream_fd to it and direct to outFile/STDOUT.
            If AFTER is absent: write stream_fd to outFile/STDOUT directly. */
        if (cmd.after.n > 0) {
            /* send stream_fd to AFTER; AFTER may write to outFile or stdout */
            int out_fd = -1; /* let execute_ handle outFile */
            execute_pipeline_with_redirs_fd(&cmd.after, NULL, cmd.outFile,
                                            (stream_fd==-1? -1 : stream_fd), out_fd);
            if (stream_fd != -1 && stream_fd != STDIN_FILENO) close(stream_fd);
        } else {
            /* no AFTER: dump the final stream to outFile or stdout */
            int outfd = -1;
            if (cmd.outFile) {
                outfd = open(cmd.outFile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
                if (outfd < 0) { perror("open outFile"); if (stream_fd!=-1 && stream_fd!=STDIN_FILENO) close(stream_fd); freeCompiledCmd(&cmd); continue; }
            }
            char buf[4096]; ssize_t r;
            int sink = (cmd.outFile ? outfd : STDOUT_FILENO);
            int src  = (stream_fd==-1 ? STDIN_FILENO : stream_fd);
            while ((r = read(src, buf, sizeof buf)) > 0) write(sink, buf, r);
            if (cmd.outFile) close(outfd);
            if (stream_fd != -1 && stream_fd != STDIN_FILENO) close(stream_fd);
        }

        freeCompiledCmd(&cmd);
        //printCompiledCmd(&cmd);        

    }
    
    freeParser(&sp);
    free(input);
    return 0;
}
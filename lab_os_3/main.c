
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

void exitHandler(void)
{
    printf("--- [atexit] Process PID=%d is cleaning up and quitting. ---\n", getpid());
}

void sigintHandler(int signum)
{
    (void)signum;
    const char msg[] = "--- [signal] SIGINT received (Ctrl+C). Ignoring. ---\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

void sigtermHandler(int signum, siginfo_t *info, void *context)
{
    (void)info;
    (void)context;
    char msg[128];
    snprintf(
        msg, sizeof(msg), "--- [sigaction] SIGTERM (%d) received. Shutting down. ---\n", signum);
    write(STDOUT_FILENO, msg, strlen(msg));
    _exit(1);
}

int main(void)
{
    if (atexit(exitHandler) != 0)
    {
        perror("atexit");
        return 1;
    }

    if (signal(SIGINT, sigintHandler) == SIG_ERR)
    {
        perror("signal");
        return 1;
    }

    struct sigaction sigAction;
    memset(&sigAction, 0, sizeof(sigAction));
    sigAction.sa_sigaction = sigtermHandler;
    sigAction.sa_flags     = SA_SIGINFO;
    if (sigaction(SIGTERM, &sigAction, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    printf("Main: PID=%d, PPID=%d\n", getpid(), getppid());
    printf("Test SIGINT handler: press Ctrl+C in this terminal.\n");
    printf("Test SIGTERM handler: in another terminal run: kill %d\n\n", getpid());

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    else if (pid == 0)
    {
        printf("[child] Hello! PID=%d, PPID=%d\n", getpid(), getppid());
        printf("[child] Sleeping for 2 seconds...\n");
        sleep(2);
        printf("[child] Exiting with code 7.\n");
        exit(7);
    }
    else
    {
        FILE *file = fopen("pid.txt", "w");
        if (file)
        {
            fprintf(file, "%d\n", getpid());
            fclose(file);
        }

        printf("[parent] Hi! PID=%d, PPID=%d\n", getpid(), getppid());
        int status = 0;
        printf("[parent] Waiting for the child to finish...\n");
        if (wait(&status) == -1)
        {
            perror("wait");
            return 1;
        }

        if (WIFEXITED(status))
        {
            printf("[parent] Child exited with status %d.\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("[parent] Child was terminated by signal %d.\n", WTERMSIG(status));
        }
        else
        {
            printf("[parent] Child finished in an unexpected way.\n");
        }

        printf("[parent] Taking a short nap (12 seconds)...\n");
        sleep(12);
    }

    printf("Main: PID=%d is about to exit.\n", getpid());
    return 0;
}

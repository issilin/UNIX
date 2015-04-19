#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_CHILD_PROCESSES 512
#define MAX_PROCESS_ARGUMENTS 20

struct ChildProcess
{
    pid_t pid;
    char *args[MAX_PROCESS_ARGUMENTS];
    char type[10];
};

void ParseLine(char *args[], FILE *in, char type[10])
{
    char buffer[512];
    int numberReadSymbol = 0;
    int i = 0;
    char readSymbol;

    fscanf(in, "%s", type);

    for(i; (readSymbol = fgetc(in)) != '\n' && readSymbol != EOF; i++)
    {
        fscanf(in, "%s%n", buffer, &numberReadSymbol);
        args[i] = malloc(sizeof(char) * numberReadSymbol);
        memcpy(args[i], buffer, (size_t) numberReadSymbol);
    }
    args[i] = NULL;
}

int ParseConfig(struct ChildProcess programs[512], FILE *in)
{
    int i = 0;
    for(i; !feof(in); i++)
    {
        ParseLine(programs[i].args, in, programs[i].type);
    }
    return i-1;
}

void CreatePidFile()
{
    pid_t pid = getpid();
    char fileName[80];
    sprintf (fileName, "%s.%d", "/tmp/specFile", pid);
    FILE *specFile = fopen(fileName, "w+");
    if(specFile)
    {
        fprintf(specFile, "%d\n", pid);
        fclose(specFile);
    }
}

void DeletePidFile(pid_t pid)
{
    char fileName[80];
    sprintf (fileName, "%s.%d", "/tmp/specFile", pid);
    unlink(fileName);
}

void InitProcess(struct ChildProcess *child)
{
    pid_t pid = fork();
    switch(pid)
    {
        case -1:
            perror("error");
            exit(1);
        case 0:
            CreatePidFile();
            execv(child -> args[0], child -> args);
        default :
            child -> pid = pid;
    }
}

void InitWorkThread(struct ChildProcess programs[], int numberOfProcess)
{
    for(int i=0; i < numberOfProcess; i++)
    {
        InitProcess(&programs[i]);
    }
}

struct ChildProcess FindChildProcess(pid_t pid, struct ChildProcess programs[], int size)
{
    for(int i = 0; i < size; i++)
    {
        if (programs[i].pid == pid)
        {
            return programs[i];
        }
    }
}

int MonitorProc(struct ChildProcess programs[], int numberOfProcess)
{
    InitWorkThread(programs, numberOfProcess);
    int status;
    sigset_t sigset;
    siginfo_t siginfo;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGCHLD);

    sigprocmask(SIG_BLOCK, &sigset, NULL);

    pid_t cpid;
    while (errno != ECHILD)
    {
        sigwaitinfo(&sigset, &siginfo);
        if (siginfo.si_signo == SIGCHLD)
        {
            cpid = wait(&status);
            DeletePidFile(cpid);
            struct ChildProcess child = FindChildProcess(cpid, programs, MAX_CHILD_PROCESSES);
            if (strcmp(child.type, "restart\0") == 0)
            {
                InitProcess(&child);
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    FILE *in = fopen(argv[1], "r");
    struct ChildProcess programs[MAX_CHILD_PROCESSES];
    int numberOfProcess = ParseConfig(programs, in);
    if (in == NULL)
    {
        printf("%s\n", "config is empty");
        exit(0);
    }

    pid_t pid;

    pid = fork();

    int status;

    if (pid == -1)
    {
        printf("Error: Start Daemon failed (%s)\n", strerror(errno));

        return -1;
    }
    else if (!pid)
    {
        umask(0);

        setsid();

        chdir("/");

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        status = MonitorProc(programs, numberOfProcess);

        return status;
    }
    else
    {
        return 0;
    }
}
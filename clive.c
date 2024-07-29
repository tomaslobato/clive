#include <stdio.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int is_directory(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

int create_build_directory()
{
    struct stat st = {0};
    if (stat("build", &st) == -1)
    {
        if (mkdir("build", 0700) == -1)
        {
            fprintf(stderr, "Error creating build directory\n");
            return -1;
        }
    }
    return 0;
}

int IeventQueue = -1;
int IeventStatus = -1;

int main(int argc, char **argv)
{
    char *fullPath = NULL;
    char *basePath = NULL;
    char *token = NULL;
    int make_flag = 0;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "USAGE: clive [--make] PATH");
        exit(1);
    }

    // check --make flag
    if (argc == 3)
    {
        if (strcmp(argv[1], "--make") == 0)
        {
            make_flag = 1;
            fullPath = argv[2];
        }
        else
        {
            fprintf(stderr, "Unknown flag. USAGE: clive [--make] PATH\n");
            exit(1);
        }
    }
    // if there's no 3rd and 2nd arg is "--make" return error
    else
    {
        if (strcmp(argv[1], "--make") == 0)
        {
            fprintf(stderr, "USAGE: clive [--make] PATH\n");
            exit(1);
        }
        fullPath = argv[1];
    }

    if (is_directory(fullPath))
    {
        if (!make_flag)
        {
            fprintf(stderr, "Error: Directory provided without --make flag\n");
            exit(1);
        }
    }

    if (create_build_directory() == -1)
    {
        exit(1);
    }

    // get path
    basePath = (char *)malloc(sizeof(char) * (strlen(fullPath) + 1));
    strcpy(basePath, fullPath);

    // get last / token on path
    token = strtok(basePath, "/");
    while (token != NULL)
    {
        basePath = token;
        token = strtok(NULL, "/");
    }

    // inotify
    const struct inotify_event *watchEvent;

    IeventQueue = inotify_init();
    if (IeventQueue == -1)
    {
        fprintf(stderr, "error initializing inotify\n");
        exit(2);
    }

    IeventStatus = inotify_add_watch(IeventQueue, fullPath, IN_MODIFY);
    if (IeventStatus == -1)
    {
        fprintf(stderr, "error adding file to watchlist\n");
        exit(3);
    }

    char buffer[4096];
    int readLength;

    while (1)
    {
        printf("waiting for file changes\n");

        readLength = read(IeventQueue, buffer, sizeof(buffer));
        if (readLength == -1)
        {

            fprintf(stderr, "error reading event\n");
            exit(4);
        }

        for (
            char *bufPointer = buffer;
            bufPointer < buffer + readLength;
            bufPointer += sizeof(struct inotify_event) + watchEvent->len)
        {
            watchEvent = (const struct inotify_event *)bufPointer;

            if (watchEvent->mask & IN_MODIFY)
            {
                char buildCommand[256];
                char exCommand[256];

                char *dot = strrchr(basePath, '.');
                if (dot && strcmp(dot, ".c") == 0)
                {
                    *dot = '\0';
                }

                if (make_flag)
                {
                    sprintf(buildCommand, "make");
                }
                else
                {
                    sprintf(buildCommand, "gcc %s -o build/%s", fullPath, basePath);
                }

                printf("------------------\nRUNNING: %s\n", buildCommand);
                int result = system(buildCommand);

                if (result == 0 && !make_flag)
                {
                    char exCommand[512];
                    snprintf(exCommand, sizeof(exCommand), "build/%s", basePath);
                    printf("EXECUTING: %s\nLOGS:\n", exCommand);
                    system(exCommand);
                }
            }
        }
    }

    free(basePath);
    return 0;
}
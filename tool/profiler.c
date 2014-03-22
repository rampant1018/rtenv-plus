#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include "../include/task.h"

#define ERR_NO_IN 1
#define ERR_NO_OUT 2
#define ERR_OPEN_OUT 3
#define ERR_OPEN_FILE 4

#define ERR(err) (ERR_##err)

int main (int argc, char *argv[])
{
    char c;
    FILE *outfile;
    FILE *infile;
    const char *outname = NULL;
    const char *inname = NULL;

    // input struct
    struct task_control_block tmpTCB;
    struct user_thread_stack tmpStack;
    int tmpTime;

    while ((c = getopt(argc, argv, "o:d:")) != -1) {
        switch (c) {
            case 'd':
                if (optarg)
                    inname = optarg;
                else
                    error(ERR(NO_IN), EINVAL,
                          "-d option need a input directory.\n");
                break;
            case 'o':
                if (optarg)
                    outname = optarg;
                else
                    error(ERR(NO_OUT), EINVAL,
                          "-o option need a output file name.\n");
                break;
            default:
                ;
        }
    }

    infile = fopen(inname, "r");
    outfile = fopen(outname, "w");
    if(infile == NULL || outfile == NULL) {
        printf("File open error!\n");
        return -1;
    }
    else {
        while(!feof(infile)) {
            fread(&tmpTCB, 24, 1, infile);
            fread(&tmpStack, sizeof(struct user_thread_stack), 1, infile);
            fread(&tmpTime, sizeof(int), 1, infile);

            fprintf(outfile, "%5d :pid: %d, status: %d, priority: %d\n", tmpTime, tmpTCB.pid, tmpTCB.status, tmpTCB.priority);
        }
    }

    fclose(outfile);
    fclose(infile);

    return 0;
}

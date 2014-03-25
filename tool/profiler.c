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

#define MKTL(n, d) {.task_name=#n, .task_identifier=d}
#define TASK_COUNT (sizeof(task_list)/sizeof(task_list[0]))

int main (int argc, char *argv[])
{
    char c;
    int i;
    FILE *outfile;
    FILE *infile;
    const char *outname = NULL;
    const char *inname = NULL;

    // input struct
    struct task_control_block tmpTCB;
    struct user_thread_stack tmpStack;
    int tmpTime;
    int tmpCs;
    int tmpSysTick;

    // task structure
    struct task_t {
        char *task_name;
        char task_identifier;
    };
    const struct task_t task_list[] = {
        MKTL("first", '!'),
        MKTL("pathserver", '@'),
        MKTL("romdev_driver", '#'),
        MKTL("romfs_server", '$'),
        MKTL("serialout", '%'),
        MKTL("serialin", '^'),
        MKTL("rs232_xmit_msg_task", '&'),
        MKTL("shell_task", '*'),
    };

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

    char tmpTrash[32];

    infile = fopen(inname, "r");
    outfile = fopen(outname, "w");
    if(infile == NULL || outfile == NULL) {
        printf("File open error!\n");
        return -1;
    }
    else {
        // Header section
        fprintf(outfile, "$version\n$end\n");
        fprintf(outfile, "$comment\n$end\n");
        fprintf(outfile, "$timescale 1us $end\n");

        // Varaible definition section
        for(i = 0; i < TASK_COUNT; i++) {
            fprintf(outfile, "$var wire 1 %c %s $end\n", task_list[i].task_identifier, task_list[i].task_name);
        }

        // $dumpvars section
        fprintf(outfile, "$dumpvars\n");
        for(i = 0; i < TASK_COUNT; i++) {
            fprintf(outfile, "0%c\n", task_list[i].task_identifier);
        }
        fprintf(outfile, "$end\n");

        while(!feof(infile)) {
            fread(&tmpCs, sizeof(int), 1, infile);
            fread(tmpTrash, 4, 1, infile);
            fread(&tmpTCB.pid, 4, 1, infile);
            fread(&tmpTCB.status, 4, 1, infile);
            fread(&tmpTCB.priority, 4, 1, infile);
            fread(tmpTrash, 8, 1, infile);
            fread(&tmpStack, sizeof(struct user_thread_stack), 1, infile);
            fread(&tmpTime, sizeof(int), 1, infile);
            fread(&tmpSysTick, sizeof(int), 1, infile);
            float tmpTimeSec = tmpSysTick / 720000.0 + tmpTime;

            if(tmpTime == 0 && tmpSysTick == 719999) { // Skip the incorrent section
                continue;
            }

            //fprintf(outfile, "%5d %f:pid: %d, status: %d, priority: %d, ABI: %d\n", tmpCs, tmpTimeSec, tmpTCB.pid, tmpTCB.status, tmpTCB.priority, tmpStack.r7);
            fprintf(outfile, "#%d\n", (int)(tmpTimeSec * 1000000.0));
            for(i = 0; i < TASK_COUNT; i++) {
                if(tmpTCB.pid == i) {
                    fprintf(outfile, "1%c\n", task_list[i].task_identifier);
                }
                else {
                    fprintf(outfile, "0%c\n", task_list[i].task_identifier);
                }
            }
        }
    }

    fclose(outfile);
    fclose(infile);

    return 0;
}

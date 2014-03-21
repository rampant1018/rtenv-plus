#include <stdint.h>
#include "shell.h"
#include "string.h"
#include "syscall.h"
#include "file.h"
#include "clib.h"

extern int fdout;
extern int fdin;
extern struct task_control_block tasks[TASK_LIMIT];
extern size_t task_count;

const char next_line[3] = {'\n','\r','\0'};

typedef void cmdfunc(int, char *[]);

/* Structure for command handler. */
typedef struct {
    const char *name;
    cmdfunc *fptr;
    const char *desc;
} hcmd_entry;

#define MKCL(n, d) {.name=#n, .fptr=n ## _command, .desc=d}
#define CMD_COUNT (sizeof(cmd_list)/sizeof(cmd_list[0]))

const hcmd_entry cmd_list[] = {
    MKCL(echo, "Show words you input."),
    MKCL(help, "List all commands you can use."),
    MKCL(man, "Manual pager."),
    MKCL(ps, "List all the processes."),
    MKCL(xxd, "Make a hexdump."),
    MKCL(cat, "Print the file on the standard output."),
    MKCL(ls, "List directory contents.")
};

void process_command(char *cmd) 
{
    char *token[20];
    int i;
    int count = 0; // token count
    int p = 0; // token start index

    for(i = 0; cmd[i]; i++) { // process token split
        if(cmd[i] == ' ') {
            cmd[i] = '\0';
            token[count++] = &cmd[p];
            p = i + 1;
        }
    }
    token[count++] = &cmd[p]; // last token

    for(i = 0; i < CMD_COUNT; i++) { // process command
        if(!strcmp(cmd_list[i].name, token[0])) {
            cmd_list[i].fptr(count, token);
            return;
        }
    }

    fio_printf(fdout, "%s: command not found.\n\r", token[0]);
}

//ps
void ps_command(int argc, char* argv[])
{
	int task_i;

        fio_printf(fdout, " PID STATUS PRIORITY\n\r");
	for (task_i = 0; task_i < task_count; task_i++) {
		char task_info_pid[2];
		char task_info_status[2];
		char task_info_priority[3];

		task_info_pid[0]='0'+tasks[task_i].pid;
		task_info_pid[1]='\0';
		task_info_status[0]='0'+tasks[task_i].status;
		task_info_status[1]='\0';			

		itoa(tasks[task_i].priority, task_info_priority, 10);

                fio_printf(fdout, "  %s     %s       %s\n\r", task_info_pid, task_info_status, task_info_priority);
	}
}


//help

void help_command(int argc, char* argv[])
{
	int i;

        fio_printf(fdout, "This system has commands as follow\n\r");
	for (i = 0; i < CMD_COUNT; i++) {
            fio_printf(fdout, "%s: %s\n\r", cmd_list[i].name, cmd_list[i].desc);
	}
}

//echo
void echo_command(int argc, char* argv[])
{
	const int _n = 1; /* Flag for "-n" option. */
	int flag = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-n"))
			flag |= _n;
		else
			break;
	}

	for (; i < argc; i++) {
            fio_printf(fdout, "%s", argv[i]);
            if (i < argc - 1)
                fio_printf(fdout, " ");
	}

	if (~flag & _n)
            fio_printf(fdout, "\n\r");
}

//man
void man_command(int argc, char *argv[])
{
	int i;

	if (argc < 2)
		return;

	for (i = 0; i < CMD_COUNT && strcmp(cmd_list[i].name, argv[1]); i++)
		;

	if (i >= CMD_COUNT)
		return;

        fio_printf(fdout, "NAME: %s\n\rDESCRIPTION: %s\n\r", cmd_list[i].name, cmd_list[i].desc);
}

char hexof(int dec)
{
    const char hextab[] = "0123456789abcdef";

    if (dec < 0 || dec > 15)
        return -1;

    return hextab[dec];
}

char char_filter(char c, char fallback)
{
    if (c < 0x20 || c > 0x7E)
        return fallback;

    return c;
}

#define XXD_WIDTH 0x10

struct romfs_entry {
    uint32_t parent;
    uint32_t prev;
    uint32_t next;
    uint32_t isdir;
    uint32_t len;
    uint8_t name[PATH_MAX];
};

//xxd
void xxd_command(int argc, char *argv[])
{
    int readfd = -1;
    char buf[XXD_WIDTH];
    char ch;
    char chout[2] = {0};
    int pos = 0;
    int size;
    int i;

    if (argc == 1) { /* fallback to stdin */
        readfd = fdin;
    }
    else { /* open file of argv[1] */
        readfd = open(argv[1], 0);

        if (readfd < 0) { /* Open error */
            fio_printf(fdout, "xxd: %s: No such file or directory\r\n", argv[1]);
            return;
        }
    }

    lseek(readfd, sizeof(struct romfs_entry), SEEK_SET);
    while ((size = read(readfd, &ch, sizeof(ch))) && size != -1) {
        if (ch != -1 && ch != 0x04) { /* has something read */

            if (pos % XXD_WIDTH == 0) { /* new line, print address */
                for (i = sizeof(pos) * 8 - 4; i >= 0; i -= 4) {
                    chout[0] = hexof((pos >> i) & 0xF);
                    fio_printf(fdout, "%s", chout);
                }

                fio_printf(fdout, ":");
            }

            if (pos % 2 == 0) { /* whitespace for each 2 bytes */
                fio_printf(fdout, " ");
            }

            /* higher bits */
            chout[0] = hexof(ch >> 4);
            fio_printf(fdout, "%s", chout);

            /* lower bits*/
            chout[0] = hexof(ch & 0xF);
            fio_printf(fdout, "%s", chout);

            /* store in buffer */
            buf[pos % XXD_WIDTH] = ch;

            pos++;

            if (pos % XXD_WIDTH == 0) { /* end of line */
                fio_printf(fdout, "  ");

                for (i = 0; i < XXD_WIDTH; i++) {
                    chout[0] = char_filter(buf[i], '.');
                    fio_printf(fdout, "%s", chout);
                }

                fio_printf(fdout, "\r\n");
            }
        }
        else { /* EOF */
            break;
        }
    }

    if (pos % XXD_WIDTH != 0) { /* rest */
        /* align */
        for (i = pos % XXD_WIDTH; i < XXD_WIDTH; i++) {
            if (i % 2 == 0) { /* whitespace for each 2 bytes */
                fio_printf(fdout, " ");
            }
            fio_printf(fdout, "  ");
        }

        fio_printf(fdout, "  ");

        for (i = 0; i < pos % XXD_WIDTH; i++) {
            chout[0] = char_filter(buf[i], '.');
            fio_printf(fdout, "%s", chout);
        }

        fio_printf(fdout, "\r\n");
    }
}

#define CAT_BUFFER_SIZE 32
// cat
void cat_command(int argc, char *argv[])
{
    struct romfs_entry entry;
    int readfd = -1;
    char buf[CAT_BUFFER_SIZE];
    char ch;
    char chout[2] = {'\0', '\0'};
    int pos = 0;
    int size;
    int i;

    if (argc == 1) { /* fallback to stdin */
        readfd = fdin;
    }
    else { /* open file of argv[1] */
        readfd = open(argv[1], 0);

        if (readfd < 0) { /* Open error */
            fio_printf(fdout, "cat: %s: No such file or directory\r\n", argv[1]);
            return;
        }

        lseek(readfd, 0, SEEK_SET);
        read(readfd, &entry, sizeof(entry));
        if(entry.isdir == 1) {
            fio_printf(fdout, "cat: %s: Is not a regular file\r\n", argv[1]);
            return;
        }
    }

    lseek(readfd, sizeof(struct romfs_entry), SEEK_SET);
    while ((size = read(readfd, &ch, sizeof(ch))) && size != -1) {
        if (ch != -1 && ch != 0x04) { /* has something read */
            /* store in buffer */
            buf[pos % CAT_BUFFER_SIZE] = ch;
            pos++;

            if (pos % CAT_BUFFER_SIZE == 0) { /* buffer full */
                for (i = 0; i < CAT_BUFFER_SIZE; i++) {
                    chout[0] = buf[i];
                    fio_printf(fdout, "%s", chout);
                    if(buf[i] == '\n') {
                        fio_printf(fdout, "\r");
                    }
                }
            }
        }
        else { /* EOF */
            break;
        }
    }

    if (pos % CAT_BUFFER_SIZE != 0) { /* rest */
        for (i = 0; i < pos; i++) {
            chout[0] = buf[i];
            fio_printf(fdout, "%s", chout);
            if(buf[i] == '\n') {
                fio_printf(fdout, "\r");
            }
        }
    }
}

// ls
void ls_command(int argc, char *argv[])
{
    struct romfs_entry entry;
    int readfd = -1;
    int size;
    int i;

    if (argc == 1) { /* fallback to stdin */
        readfd = open("/", 0);
    }
    else { /* open file of argv[1] */
        readfd = open(argv[1], 0);

        if (readfd < 0) { /* Open error */
            fio_printf(fdout, "ls: %s: No such file or directory\r\n", argv[1]);
            return;
        }
        
        lseek(readfd, 0, SEEK_SET);
        read(readfd, &entry, sizeof(entry));
        if(entry.isdir != 1) {
            fio_printf(fdout, "ls: %s: Is not a directory\n\r", argv[1]);
            return;
        }
    }

    fio_printf(fdout, " Size     Name   isdir\n\r");

    char tmpout[32];
    lseek(readfd, sizeof(struct romfs_entry), SEEK_SET);
    while ((size = read(readfd, &entry, sizeof(entry))) && size != -1) {
        itoa(entry.len, tmpout, 10); // Process file len
        for(i = 4 - strlen(tmpout); i >= 0; i--) {
            fio_printf(fdout, " ");
        }
        fio_printf(fdout, "%s", tmpout);

        for(i = 8 - strlen((char *)entry.name); i >= 0; i--) { // Process file name
            fio_printf(fdout, " ");
        }
        fio_printf(fdout, "%s", entry.name);

        if(entry.isdir == 1) { // Process isdir
            fio_printf(fdout, "    [*]");
        }
        fio_printf(fdout, "\n\r");

        lseek(readfd, entry.len, SEEK_CUR);
    }
}

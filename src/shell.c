#include "shell.h"

/* Structure for command handler. */
typedef struct {
    const char *name;
    void (*func)(int, char *[]);
    const *char desc;
} hcmd_entry;

#define MKCL(n, d) {.name=#n, .func=n ## _command, .desc=d}

const hcmd_entry cmd_list[] = {
    MKCL(echo, "Show words you input."),
    MKCL(export, "Export environment variables."),
    MKCL(help, "List all commands you can use."),
    MKCL(history, "Show latest commands entered."),
    MKCL(man, "Manual pager."),
    MKCL(ps, "List all the processes."),
    MKCL(xxd, "Make a hexdump."),
    MKCL(cat, "Print the file on the standard output."),
    MKCL(ls, "List directory contents.")
};

void process_command(char *cmd) 
{

}

//export
void export_command(int argc, char *argv[])
{
	char *found;
	char *value;
	int i;

	for (i = 1; i < argc; i++) {
		value = argv[i];
		while (*value && *value != '=')
			value++;
		if (*value)
			*value++ = '\0';
		found = find_envvar(argv[i]);
		if (found)
			strcpy(found, value);
		else if (env_count < MAX_ENVCOUNT) {
			strcpy(env_var[env_count].name, argv[i]);
			strcpy(env_var[env_count].value, value);
			env_count++;
		}
	}
}

//ps
void ps_command(int argc, char* argv[])
{
	char ps_message[]="PID STATUS PRIORITY";
	int ps_message_length = sizeof(ps_message);
	int task_i;

	write(fdout, &ps_message , ps_message_length);
	write(fdout, &next_line , 3);

	for (task_i = 0; task_i < task_count; task_i++) {
		char task_info_pid[2];
		char task_info_status[2];
		char task_info_priority[3];

		task_info_pid[0]='0'+tasks[task_i].pid;
		task_info_pid[1]='\0';
		task_info_status[0]='0'+tasks[task_i].status;
		task_info_status[1]='\0';			

		itoa(tasks[task_i].priority, task_info_priority, 10);

		write(fdout, &task_info_pid , 2);
		write_blank(3);
			write(fdout, &task_info_status , 2);
		write_blank(5);
		write(fdout, &task_info_priority , 3);

		write(fdout, &next_line , 3);
	}
}


//help

void help_command(int argc, char* argv[])
{
	const char help_desp[] = "This system has commands as follow\n\r\0";
	int i;

	write(fdout, &help_desp, sizeof(help_desp));
	for (i = 0; i < CMD_COUNT; i++) {
		write(fdout, cmd_data[i].cmd, strlen(cmd_data[i].cmd) + 1);
		write(fdout, ": ", 3);
		write(fdout, cmd_data[i].description, strlen(cmd_data[i].description) + 1);
		write(fdout, next_line, 3);
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
		write(fdout, argv[i], strlen(argv[i]) + 1);
		if (i < argc - 1)
			write(fdout, " ", 2);
	}

	if (~flag & _n)
		write(fdout, next_line, 3);
}

//man
void man_command(int argc, char *argv[])
{
	int i;

	if (argc < 2)
		return;

	for (i = 0; i < CMD_COUNT && strcmp(cmd_data[i].cmd, argv[1]); i++)
		;

	if (i >= CMD_COUNT)
		return;

	write(fdout, "NAME: ", 7);
	write(fdout, cmd_data[i].cmd, strlen(cmd_data[i].cmd) + 1);
	write(fdout, next_line, 3);
	write(fdout, "DESCRIPTION: ", 14);
	write(fdout, cmd_data[i].description, strlen(cmd_data[i].description) + 1);
	write(fdout, next_line, 3);
}

void history_command(int argc, char *argv[])
{
	int i;

	for (i = cur_his + 1; i <= cur_his + HISTORY_COUNT; i++) {
		if (cmd[i % HISTORY_COUNT][0]) {
			write(fdout, cmd[i % HISTORY_COUNT], strlen(cmd[i % HISTORY_COUNT]) + 1);
			write(fdout, next_line, 3);
		}
	}
}

void write_blank(int blank_num)
{
	char blank[] = " ";
	int blank_count = 0;

	while (blank_count <= blank_num) {
		write(fdout, blank, sizeof(blank));
		blank_count++;
	}
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
            write(fdout, "xxd: ", 6);
            write(fdout, argv[1], strlen(argv[1]) + 1);
            write(fdout, ": No such file or directory\r\n", 31);
            return;
        }
    }

    lseek(readfd, sizeof(struct romfs_entry), SEEK_SET);
    while ((size = read(readfd, &ch, sizeof(ch))) && size != -1) {
        if (ch != -1 && ch != 0x04) { /* has something read */

            if (pos % XXD_WIDTH == 0) { /* new line, print address */
                for (i = sizeof(pos) * 8 - 4; i >= 0; i -= 4) {
                    chout[0] = hexof((pos >> i) & 0xF);
                    write(fdout, chout, 2);
                }

                write(fdout, ":", 2);
            }

            if (pos % 2 == 0) { /* whitespace for each 2 bytes */
                write(fdout, " ", 2);
            }

            /* higher bits */
            chout[0] = hexof(ch >> 4);
            write(fdout, chout, 2);

            /* lower bits*/
            chout[0] = hexof(ch & 0xF);
            write(fdout, chout, 2);

            /* store in buffer */
            buf[pos % XXD_WIDTH] = ch;

            pos++;

            if (pos % XXD_WIDTH == 0) { /* end of line */
                write(fdout, "  ", 3);

                for (i = 0; i < XXD_WIDTH; i++) {
                    chout[0] = char_filter(buf[i], '.');
                    write(fdout, chout, 2);
                }

                write(fdout, "\r\n", 3);
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
                write(fdout, " ", 2);
            }
            write(fdout, "  ", 3);
        }

        write(fdout, "  ", 3);

        for (i = 0; i < pos % XXD_WIDTH; i++) {
            chout[0] = char_filter(buf[i], '.');
            write(fdout, chout, 2);
        }

        write(fdout, "\r\n", 3);
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
            write(fdout, "cat: ", 6);
            write(fdout, argv[1], strlen(argv[1]) + 1);
            write(fdout, ": No such file or directory\r\n", 31);
            return;
        }

        lseek(readfd, 0, SEEK_SET);
        read(readfd, &entry, sizeof(entry));
        if(entry.isdir == 1) {
            write(fdout, "cat: ", 6);
            write(fdout, argv[1], strlen(argv[1]) + 1);
            write(fdout, ": Is not a regular file\n\r", 26);
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
                    write(fdout, chout, 2);
                    if(buf[i] == '\n') {
                        write(fdout, "\r", 2);
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
            write(fdout, chout, 2);
            if(buf[i] == '\n') {
                write(fdout, "\r", 2);
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
            write(fdout, "ls: ", 5);
            write(fdout, argv[1], strlen(argv[1]) + 1);
            write(fdout, ": No such file or directory\r\n", 31);
            return;
        }
        
        lseek(readfd, 0, SEEK_SET);
        read(readfd, &entry, sizeof(entry));
        if(entry.isdir != 1) {
            write(fdout, "ls: ", 5);
            write(fdout, argv[1], strlen(argv[1]) + 1);
            write(fdout, ": Is not a directory\n\r", 23);
            return;
        }
    }

    // print tag
    const char *tag_line = "  Size     Name   isdir\n\r";
    write(fdout, tag_line, strlen(tag_line) + 1);

    char tmpout[32];
    lseek(readfd, sizeof(struct romfs_entry), SEEK_SET);
    while ((size = read(readfd, &entry, sizeof(entry))) && size != -1) {
        itoa(entry.len, tmpout, 10); // Process file len
        for(i = 5 - strlen(tmpout); i >= 0; i--) {
            write(fdout, " ", 2);
        }
        write(fdout, tmpout, strlen(tmpout) + 1);

        for(i = 8 - strlen((char *)entry.name); i >= 0; i--) { // Process file name
            write(fdout, " ", 2);
        }
        write(fdout, entry.name, strlen((char *)entry.name) + 1);

        if(entry.isdir == 1) { // Process isdir
            write(fdout, "    [*]", 8);
        }
        write(fdout, "\n\r", 3);

        lseek(readfd, entry.len, SEEK_CUR);
    }
}

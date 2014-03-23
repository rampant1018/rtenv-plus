#ifndef SHELL_H
#define SHELL_H

void shell_task();
void process_command(char *cmd);

/* Command handlers. */
void echo_command(int argc, char *argv[]);
void help_command(int argc, char *argv[]);
void ps_command(int argc, char *argv[]);
void man_command(int argc, char *argv[]);
void xxd_command(int argc, char *argv[]);
void cat_command(int argc, char *argv[]);
void ls_command(int argc, char *argv[]);

#endif

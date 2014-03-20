#include "kconfig.h"
#include "kernel.h"
#include "stm32f10x.h"
#include "stm32_p103.h"
#include "RTOSConfig.h"

#include "syscall.h"

#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include "string.h"
#include "task.h"
#include "memory-pool.h"
#include "path.h"
#include "pipe.h"
#include "fifo.h"
#include "mqueue.h"
#include "block.h"
#include "romdev.h"
#include "event-monitor.h"
#include "romfs.h"

#define MAX_CMDNAME 19
#define MAX_ARGC 19
#define MAX_CMDHELP 1023
#define HISTORY_COUNT 8
#define CMDBUF_SIZE 64
#define MAX_ENVCOUNT 16
#define MAX_ENVNAME 15
#define MAX_ENVVALUE 63

/*Global Variables*/
char next_line[3] = {'\n','\r','\0'};
size_t task_count = 0;
char cmd[HISTORY_COUNT][CMDBUF_SIZE];
int cur_his=0;
int fdout;
int fdin;

void check_keyword();
void find_events();
int fill_arg(char *const dest, const char *argv);
void itoa(int n, char *dst, int base);
void write_blank(int blank_num);



/* Structure for environment variables. */
typedef struct {
	char name[MAX_ENVNAME + 1];
	char value[MAX_ENVVALUE + 1];
} evar_entry;
evar_entry env_var[MAX_ENVCOUNT];
int env_count = 0;


struct task_control_block tasks[TASK_LIMIT];


void serialout(USART_TypeDef* uart, unsigned int intr)
{
	int fd;
	char c;
	int doread = 1;
	mkfifo("/dev/tty0/out", 0);
	fd = open("/dev/tty0/out", 0);

	while (1) {
		if (doread)
			read(fd, &c, 1);
		doread = 0;
		if (USART_GetFlagStatus(uart, USART_FLAG_TXE) == SET) {
			USART_SendData(uart, c);
			USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
			doread = 1;
		}
		interrupt_wait(intr);
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
	}
}

void serialin(USART_TypeDef* uart, unsigned int intr)
{
	int fd;
	char c;
	mkfifo("/dev/tty0/in", 0);
	fd = open("/dev/tty0/in", 0);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	while (1) {
		interrupt_wait(intr);
		if (USART_GetFlagStatus(uart, USART_FLAG_RXNE) == SET) {
			c = USART_ReceiveData(uart);
			write(fd, &c, 1);
		}
	}
}

void greeting()
{
	int fdout = open("/dev/tty0/out", 0);
	char *string = "Hello, World!\n";
	while (*string) {
		write(fdout, string, 1);
		string++;
	}
}

void echo()
{
	int fdout;
	int fdin;
	char c;

	fdout = open("/dev/tty0/out", 0);
	fdin = open("/dev/tty0/in", 0);

	while (1) {
		read(fdin, &c, 1);
		write(fdout, &c, 1);
	}
}

void rs232_xmit_msg_task()
{
	int fdout;
	int fdin;
	char str[100];
	int curr_char;

	fdout = open("/dev/tty0/out", 0);
	fdin = mq_open("/tmp/mqueue/out", O_CREAT);
	setpriority(0, PRIORITY_DEFAULT - 2);

	while (1) {
		/* Read from the queue.  Keep trying until a message is
		 * received.  This will block for a period of time (specified
		 * by portMAX_DELAY). */
		read(fdin, str, 100);

		/* Write each character of the message to the RS232 port. */
		curr_char = 0;
		while (str[curr_char] != '\0') {
			write(fdout, &str[curr_char], 1);
			curr_char++;
		}
	}
}

void queue_str_task(const char *str, int delay)
{
	int fdout = mq_open("/tmp/mqueue/out", 0);
	int msg_len = strlen(str) + 1;

	while (1) {
		/* Post the message.  Keep on trying until it is successful. */
		write(fdout, str, msg_len);

		/* Wait. */
		sleep(delay);
	}
}

void queue_str_task1()
{
	queue_str_task("Hello 1\n", 200);
}

void queue_str_task2()
{
	queue_str_task("Hello 2\n", 50);
}

void serial_readwrite_task()
{
	int fdout;
	int fdin;
	char str[100];
	char ch;
	int curr_char;
	int done;

	fdout = mq_open("/tmp/mqueue/out", 0);
	fdin = open("/dev/tty0/in", 0);

	/* Prepare the response message to be queued. */
	memcpy(str, "Got:", 4);

	while (1) {
		curr_char = 4;
		done = 0;
		do {
			/* Receive a byte from the RS232 port (this call will
			 * block). */
			read(fdin, &ch, 1);

			/* If the byte is an end-of-line type character, then
			 * finish the string and inidcate we are done.
			 */
			if (curr_char >= 98 || ch == '\r' || ch == '\n') {
				str[curr_char] = '\n';
				str[curr_char+1] = '\0';
				done = -1;
			}
			/* Otherwise, add the character to the
			 * response string. */
			else
				str[curr_char++] = ch;
		} while (!done);

		/* Once we are done building the response string, queue the
		 * response to be sent to the RS232 port.
		 */
		write(fdout, str, curr_char+1 + 1);
	}
}

void serial_test_task()
{
	char put_ch[2]={'0','\0'};
	char hint[] =  USER_NAME "@" USER_NAME "-STM32:~$ ";
	int hint_length = sizeof(hint);
	char *p = NULL;

	fdout = mq_open("/tmp/mqueue/out", 0);
	fdin = open("/dev/tty0/in", 0);

	for (;; cur_his = (cur_his + 1) % HISTORY_COUNT) {
		p = cmd[cur_his];
		write(fdout, hint, hint_length);

		while (1) {
			read(fdin, put_ch, 1);

			if (put_ch[0] == '\r' || put_ch[0] == '\n') {
				*p = '\0';
				write(fdout, next_line, 3);
				break;
			}
			else if (put_ch[0] == 127 || put_ch[0] == '\b') {
				if (p > cmd[cur_his]) {
					p--;
					write(fdout, "\b \b", 4);
				}
			}
			else if (p - cmd[cur_his] < CMDBUF_SIZE - 1) {
				*p++ = put_ch[0];
				write(fdout, put_ch, 2);
			}
		}
		check_keyword();	
	}
}

/* Split command into tokens. */
char *cmdtok(char *cmd)
{
	static char *cur = NULL;
	static char *end = NULL;
	if (cmd) {
		char quo = '\0';
		cur = cmd;
		for (end = cmd; *end; end++) {
			if (*end == '\'' || *end == '\"') {
				if (quo == *end)
					quo = '\0';
				else if (quo == '\0')
					quo = *end;
				*end = '\0';
			}
			else if (isspace((int)*end) && !quo)
				*end = '\0';
		}
	}
	else
		for (; *cur; cur++)
			;

	for (; *cur == '\0'; cur++)
		if (cur == end) return NULL;
	return cur;
}

void check_keyword()
{
	char *argv[MAX_ARGC + 1] = {NULL};
	char cmdstr[CMDBUF_SIZE];
	int argc = 1;
	int i;

	find_events();
	fill_arg(cmdstr, cmd[cur_his]);
	argv[0] = cmdtok(cmdstr);
	if (!argv[0])
		return;

	while (1) {
		argv[argc] = cmdtok(NULL);
		if (!argv[argc])
			break;
		argc++;
		if (argc >= MAX_ARGC)
			break;
	}

	for (i = 0; i < CMD_COUNT; i++) {
		if (!strcmp(argv[0], cmd_data[i].cmd)) {
			cmd_data[i].func(argc, argv);
			break;
		}
	}
	if (i == CMD_COUNT) {
		write(fdout, argv[0], strlen(argv[0]) + 1);
		write(fdout, ": command not found", 20);
		write(fdout, next_line, 3);
	}
}

void find_events()
{
	char buf[CMDBUF_SIZE];
	char *p = cmd[cur_his];
	char *q;
	int i;

	for (; *p; p++) {
		if (*p == '!') {
			q = p;
			while (*q && !isspace((int)*q))
				q++;
			for (i = cur_his + HISTORY_COUNT - 1; i > cur_his; i--) {
				if (!strncmp(cmd[i % HISTORY_COUNT], p + 1, q - p - 1)) {
					strcpy(buf, q);
					strcpy(p, cmd[i % HISTORY_COUNT]);
					p += strlen(p);
					strcpy(p--, buf);
					break;
				}
			}
		}
	}
}

char *find_envvar(const char *name)
{
	int i;

	for (i = 0; i < env_count; i++) {
		if (!strcmp(env_var[i].name, name))
			return env_var[i].value;
	}

	return NULL;
}

/* Fill in entire value of argument. */
int fill_arg(char *const dest, const char *argv)
{
	char env_name[MAX_ENVNAME + 1];
	char *buf = dest;
	char *p = NULL;

	for (; *argv; argv++) {
		if (isalnum((int)*argv) || *argv == '_') {
			if (p)
				*p++ = *argv;
			else
				*buf++ = *argv;
		}
		else { /* Symbols. */
			if (p) {
				*p = '\0';
				p = find_envvar(env_name);
				if (p) {
					strcpy(buf, p);
					buf += strlen(p);
					p = NULL;
				}
			}
			if (*argv == '$')
				p = env_name;
			else
				*buf++ = *argv;
		}
	}
	if (p) {
		*p = '\0';
		p = find_envvar(env_name);
		if (p) {
			strcpy(buf, p);
			buf += strlen(p);
		}
	}
	*buf = '\0';

	return buf - dest;
}


void first()
{
	if (!fork()) setpriority(0, 0), pathserver();
	if (!fork()) setpriority(0, 0), romdev_driver();
	if (!fork()) setpriority(0, 0), romfs_server();
	if (!fork()) setpriority(0, 0), serialout(USART2, USART2_IRQn);
	if (!fork()) setpriority(0, 0), serialin(USART2, USART2_IRQn);
	if (!fork()) rs232_xmit_msg_task();
	if (!fork()) setpriority(0, PRIORITY_DEFAULT - 10), serial_test_task();

	setpriority(0, PRIORITY_LIMIT);

	mount("/dev/rom0", "/", ROMFS_TYPE, 0);

	while(1);
}

#define INTR_EVENT(intr) (FILE_LIMIT + (intr) + 15) /* see INTR_LIMIT */
#define INTR_EVENT_REVERSE(event) ((event) - FILE_LIMIT - 15)
#define TIME_EVENT (FILE_LIMIT + INTR_LIMIT)

int intr_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    return 1;
}

int time_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    int *tick_count = data;
    return task->stack->r0 == *tick_count;
}

/* System resources */
unsigned int stacks[TASK_LIMIT][STACK_SIZE];
char memory_space[MEM_LIMIT];
struct file *files[FILE_LIMIT];
struct file_request requests[TASK_LIMIT];
struct list ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */
struct event events[EVENT_LIMIT];


int main()
{
	//struct task_control_block tasks[TASK_LIMIT];
	struct memory_pool memory_pool;
	struct event_monitor event_monitor;
	//size_t task_count = 0;
	size_t current_task = 0;
	int i;
	struct list *list;
	struct task_control_block *task;
	int timeup;
	unsigned int tick_count = 0;

        // for sbrk()
        unsigned char heaps[HEAP_SIZE * TASK_LIMIT];
        unsigned char *program_break;
        unsigned char *previous_pb;

	SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

	init_rs232();
	__enable_irq();

    /* Initialize memory pool */
    memory_pool_init(&memory_pool, MEM_LIMIT, memory_space);

	/* Initialize all files */
	for (i = 0; i < FILE_LIMIT; i++)
		files[i] = NULL;

	/* Initialize ready lists */
	for (i = 0; i <= PRIORITY_LIMIT; i++)
		list_init(&ready_list[i]);

    /* Initialise event monitor */
    event_monitor_init(&event_monitor, events, ready_list);

        /* Initialize program_break */
        program_break = heaps; // program_break is located at last end location

	/* Initialize fifos */
	for (i = 0; i <= PATHSERVER_FD; i++)
		file_mknod(i, -1, files, S_IFIFO, &memory_pool, &event_monitor);

    /* Register IRQ events, see INTR_LIMIT */
	for (i = -15; i < INTR_LIMIT - 15; i++)
	    event_monitor_register(&event_monitor, INTR_EVENT(i), intr_release, 0);

	event_monitor_register(&event_monitor, TIME_EVENT, time_release, &tick_count);

    /* Initialize first thread */
	tasks[task_count].stack = (void*)init_task(stacks[task_count], &first);
	tasks[task_count].pid = 0;
	tasks[task_count].priority = PRIORITY_DEFAULT;
	list_init(&tasks[task_count].list);
	list_push(&ready_list[tasks[task_count].priority], &tasks[task_count].list);
	task_count++;

	while (1) {
		tasks[current_task].stack = activate(tasks[current_task].stack);
		tasks[current_task].status = TASK_READY;
		timeup = 0;

		switch (tasks[current_task].stack->r7) {
		case 0x1: /* fork */
			if (task_count == TASK_LIMIT) {
				/* Cannot create a new task, return error */
				tasks[current_task].stack->r0 = -1;
			}
			else {
				/* Compute how much of the stack is used */
				size_t used = stacks[current_task] + STACK_SIZE
					      - (unsigned int*)tasks[current_task].stack;
				/* New stack is END - used */
				tasks[task_count].stack = (void*)(stacks[task_count] + STACK_SIZE - used);
				/* Copy only the used part of the stack */
				memcpy(tasks[task_count].stack, tasks[current_task].stack,
				       used * sizeof(unsigned int));
				/* Set PID */
				tasks[task_count].pid = task_count;
				/* Set priority, inherited from forked task */
				tasks[task_count].priority = tasks[current_task].priority;
				/* Set return values in each process */
				tasks[current_task].stack->r0 = task_count;
				tasks[task_count].stack->r0 = 0;
				list_init(&tasks[task_count].list);
				list_push(&ready_list[tasks[task_count].priority], &tasks[task_count].list);
				/* There is now one more task */
				task_count++;
			}
			break;
		case 0x2: /* getpid */
			tasks[current_task].stack->r0 = current_task;
			break;
		case 0x3: /* write */
		    {
		        /* Check fd is valid */
		        int fd = tasks[current_task].stack->r0;
		        if (fd < FILE_LIMIT && files[fd]) {
		            /* Prepare file request, store reference in r0 */
		            requests[current_task].task = &tasks[current_task];
		            requests[current_task].buf =
		                (void*)tasks[current_task].stack->r1;
		            requests[current_task].size = tasks[current_task].stack->r2;
		            tasks[current_task].stack->r0 =
		                (int)&requests[current_task];

                    /* Write */
			        file_write(files[fd], &requests[current_task],
			                   &event_monitor);
			    }
			    else {
			        tasks[current_task].stack->r0 = -1;
			    }
			} break;
		case 0x4: /* read */
            {
		        /* Check fd is valid */
		        int fd = tasks[current_task].stack->r0;
		        if (fd < FILE_LIMIT && files[fd]) {
		            /* Prepare file request, store reference in r0 */
		            requests[current_task].task = &tasks[current_task];
		            requests[current_task].buf =
		                (void*)tasks[current_task].stack->r1;
		            requests[current_task].size = tasks[current_task].stack->r2;
		            tasks[current_task].stack->r0 =
		                (int)&requests[current_task];

                    /* Read */
			        file_read(files[fd], &requests[current_task],
			                  &event_monitor);
			    }
			    else {
			        tasks[current_task].stack->r0 = -1;
			    }
			} break;
		case 0x5: /* interrupt_wait */
			/* Enable interrupt */
			NVIC_EnableIRQ(tasks[current_task].stack->r0);
			/* Block task waiting for interrupt to happen */
			event_monitor_block(&event_monitor,
			                    INTR_EVENT(tasks[current_task].stack->r0),
			                    &tasks[current_task]);
			tasks[current_task].status = TASK_WAIT_INTR;
			break;
		case 0x6: /* getpriority */
			{
				int who = tasks[current_task].stack->r0;
				if (who > 0 && who < (int)task_count)
					tasks[current_task].stack->r0 = tasks[who].priority;
				else if (who == 0)
					tasks[current_task].stack->r0 = tasks[current_task].priority;
				else
					tasks[current_task].stack->r0 = -1;
			} break;
		case 0x7: /* setpriority */
			{
				int who = tasks[current_task].stack->r0;
				int value = tasks[current_task].stack->r1;
				value = (value < 0) ? 0 : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);
				if (who > 0 && who < (int)task_count) {
					tasks[who].priority = value;
					if (tasks[who].status == TASK_READY)
					    list_push(&ready_list[value], &tasks[who].list);
				}
				else if (who == 0) {
					tasks[current_task].priority = value;
				    list_unshift(&ready_list[value], &tasks[current_task].list);
				}
				else {
					tasks[current_task].stack->r0 = -1;
					break;
				}
				tasks[current_task].stack->r0 = 0;
			} break;
		case 0x8: /* mknod */
			tasks[current_task].stack->r0 =
				file_mknod(tasks[current_task].stack->r0,
				           tasks[current_task].pid,
				           files,
					       tasks[current_task].stack->r2,
					       &memory_pool,
					       &event_monitor);
			break;
		case 0x9: /* sleep */
			if (tasks[current_task].stack->r0 != 0) {
				tasks[current_task].stack->r0 += tick_count;
				tasks[current_task].status = TASK_WAIT_TIME;
			    event_monitor_block(&event_monitor, TIME_EVENT,
			                        &tasks[current_task]);
			}
			break;
		case 0xa: /* lseek */
            {
		        /* Check fd is valid */
		        int fd = tasks[current_task].stack->r0;
		        if (fd < FILE_LIMIT && files[fd]) {
		            /* Prepare file request, store reference in r0 */
		            requests[current_task].task = &tasks[current_task];
		            requests[current_task].buf = NULL;
		            requests[current_task].size = tasks[current_task].stack->r1;
		            requests[current_task].whence = tasks[current_task].stack->r2;
		            tasks[current_task].stack->r0 =
		                (int)&requests[current_task];

                    /* Read */
			        file_lseek(files[fd], &requests[current_task],
			                   &event_monitor);
			    }
			    else {
			        tasks[current_task].stack->r0 = -1;
			    }
			} break;
                case 0xb: /* sbrk */
                        if(program_break + tasks[current_task].stack->r0 >= heaps && program_break + tasks[current_task].stack->r0 < heaps + (HEAP_SIZE * TASK_LIMIT)) {
                                previous_pb = program_break;
                                program_break += tasks[current_task].stack->r0;
                                tasks[current_task].stack->r0 = (unsigned int)previous_pb;
                        }
                        else {
                                tasks[current_task].stack->r0 = -1;
                        }
                          
                        break;
		default: /* Catch all interrupts */
			if ((int)tasks[current_task].stack->r7 < 0) {
				unsigned int intr = -tasks[current_task].stack->r7 - 16;

				if (intr == SysTick_IRQn) {
					/* Never disable timer. We need it for pre-emption */
					timeup = 1;
					tick_count++;
					event_monitor_release(&event_monitor, TIME_EVENT);
				}
				else {
					/* Disable interrupt, interrupt_wait re-enables */
					NVIC_DisableIRQ(intr);
				}
				event_monitor_release(&event_monitor, INTR_EVENT(intr));
			}
		}

        /* Rearrange ready list and event list */
		event_monitor_serve(&event_monitor);

		/* Check whether to context switch */
		task = &tasks[current_task];
		if (timeup && ready_list[task->priority].next == &task->list)
		    list_push(&ready_list[task->priority], &tasks[current_task].list);

		/* Select next TASK_READY task */
		for (i = 0; list_empty(&ready_list[i]); i++);

		list = ready_list[i].next;
		task = list_entry(list, struct task_control_block, list);
		current_task = task->pid;
	}

	return 0;
}

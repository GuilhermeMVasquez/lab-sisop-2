#ifndef LIST_DRIVER_H
#define LIST_DRIVER_H

#include <linux/types.h>

typedef struct List {
	struct process_node *head;
	struct process_node *tail;
	int count;
	unsigned int max_messages;
	unsigned int max_message_size;
} List;

struct process_node {
	pid_t pid;
	char *name;
	struct queue *queue;
	struct process_node *next;
};

struct queue {
	char **messages;
	int head;
	int tail;
	int count;
};

void init_list(unsigned int max_messages, unsigned int max_message_size);
int register_process(pid_t pid, char *name);
int enqueue_message(pid_t pid, char *dest_name, char *msg);
int unregister_process(pid_t pid, char *name);
char *dequeue_message(pid_t pid);
void cleanup_process(struct process_node *process);
void cleanup_all_processes(void);

struct process_node *get_process_by_pid(pid_t pid);
struct process_node *get_process_by_name(char *name);

int check_process_messages(pid_t pid);

#endif
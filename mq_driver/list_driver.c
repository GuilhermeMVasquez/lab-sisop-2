#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/types.h>
#include "list_driver.h"

List *process_list = NULL;

void init_list(unsigned int max_messages, unsigned int max_message_size)
{
	if (process_list == NULL) {
		process_list = (List *)kmalloc(sizeof(List), GFP_KERNEL);
		process_list->head = NULL;
		process_list->tail = NULL;
		process_list->count = 0;
		process_list->max_messages = max_messages;
		process_list->max_message_size = max_message_size;

		printk(KERN_INFO "mq_driver.init_list: process list initialized with max_messages: %u, max_message_size: %u\n",
		       max_messages, max_message_size);
	}
}

int register_process(pid_t pid, char *name)
{
	struct process_node *new_process;

	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.register_process: process list not initialized\n");
		return -1;
	}

	if (get_process_by_pid(pid) != NULL) {
		printk(KERN_ALERT "mq_driver.register_process: process with pid %d already exists\n", (int)pid);
		return -1;
	}

	if (get_process_by_name(name) != NULL) {
		printk(KERN_ALERT "mq_driver.register_process: process with name %s already exists\n", name);
		return -1;
	}

	new_process = (struct process_node *)kmalloc(sizeof(struct process_node), GFP_KERNEL);

	new_process->pid = pid;

	new_process->name = (char *)kmalloc(strlen(name) + 1, GFP_KERNEL);
	strcpy(new_process->name, name);

	new_process->queue = (struct queue *)kmalloc(sizeof(struct queue), GFP_KERNEL);
	new_process->queue->messages = (char **)kmalloc(process_list->max_messages * sizeof(char *), GFP_KERNEL);
	new_process->queue->head = 0;
	new_process->queue->tail = 0;
	new_process->queue->count = 0;

	new_process->next = NULL;

	if (process_list->head == NULL) {
		process_list->head = new_process;
		process_list->tail = new_process;
	} else {
		process_list->tail->next = new_process;
		process_list->tail = new_process;
	}

	process_list->count++;

	printk(KERN_INFO "mq_driver.register_process: process with pid %d and name %s registered\n", (int)pid, name);

	return 0;
}

int enqueue_message(pid_t pid, char *dest_name, char *msg)
{
	struct process_node *dest_process;

	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.enqueue_message: process list not initialized\n");
		return -1;
	}

	if (get_process_by_pid(pid) == NULL) {
		printk(KERN_ALERT "mq_driver.enqueue_message: process with pid %d must be registered to send messages\n", (int)pid);
		return -1;
	}

	dest_process = get_process_by_name(dest_name);
	if (dest_process == NULL) {
		printk(KERN_ALERT "mq_driver.enqueue_message: destination process %s not found\n", dest_name);
		return -1;
	}

	if (strlen(msg) > process_list->max_message_size) {
		printk(KERN_ALERT "mq_driver.enqueue_message: message size exceeds maximum allowed size\n");
		return -1;
	}

	if (dest_process->queue->count >= process_list->max_messages) {
		printk(KERN_ALERT "mq_driver.enqueue_message: message queue for process %s full. Overwriting oldest message.\n",
		       dest_name);
	}

	if (dest_process->queue->tail == dest_process->queue->head && dest_process->queue->count > 0) {
		kfree(dest_process->queue->messages[dest_process->queue->head]);
		dest_process->queue->head = (dest_process->queue->head + 1) % process_list->max_messages;
		dest_process->queue->count--;
	}

	dest_process->queue->messages[dest_process->queue->tail] = (char *)kmalloc(strlen(msg) + 1, GFP_KERNEL);
	strcpy(dest_process->queue->messages[dest_process->queue->tail], msg);

	dest_process->queue->tail = (dest_process->queue->tail + 1) % process_list->max_messages;
	dest_process->queue->count++;

	printk(KERN_INFO "mq_driver.enqueue_message: message sent from pid %d to %s: %s\n", (int)pid, dest_name, msg);

	return 0;
}

int unregister_process(pid_t pid, char *name)
{
	struct process_node *process;
	struct process_node *prev;

	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.unregister_process: process list not initialized\n");
		return -1;
	}

	process = process_list->head;
	prev = NULL;
	while (process != NULL) {
		if (process->pid == pid && strcmp(process->name, name) == 0) {
			if (prev == NULL) {
				process_list->head = process->next;
			} else {
				prev->next = process->next;
			}

			if (process == process_list->tail) {
				process_list->tail = prev;
			}

			cleanup_process(process);

			process_list->count--;

			printk(KERN_INFO "mq_driver.unregister_process: process with pid %d and name %s unregistered\n", (int)pid,
			       name);

			return 0;
		}

		prev = process;
		process = process->next;
	}

	printk(KERN_ALERT "mq_driver.unregister_process: process with pid %d and name %s not found\n", (int)pid, name);

	return -1;
}

char *dequeue_message(pid_t pid)
{
	struct process_node *process;
	char *message;

	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.dequeue_message: process list not initialized\n");
		return NULL;
	}

	process = get_process_by_pid(pid);
	if (process == NULL) {
		printk(KERN_ALERT "mq_driver.dequeue_message: process with pid %d not found\n", (int)pid);
		return NULL;
	}

	if (process->queue->count <= 0) {
		printk(KERN_INFO "mq_driver.dequeue_message: no messages to read for process %d\n", (int)pid);
		return NULL;
	}

	message = process->queue->messages[process->queue->head];
	process->queue->head = (process->queue->head + 1) % process_list->max_messages;
	process->queue->count--;

	printk(KERN_INFO "mq_driver.dequeue_message: message read for process %d: %s\n", (int)pid, message);

	return message;
}

void cleanup_process(struct process_node *process)
{
	pid_t process_pid;

	if (process == NULL) {
		printk(KERN_ALERT "mq_driver.cleanup_process: process is NULL\n");
		return;
	}

	process_pid = process->pid;

	if (process->queue != NULL) {
		if (process->queue->messages != NULL) {
			while (process->queue->count > 0) {
				kfree(process->queue->messages[process->queue->head]);
				process->queue->head = (process->queue->head + 1) % process_list->max_messages;
				process->queue->count--;
			}

			kfree(process->queue->messages);

			printk(KERN_INFO "mq_driver.cleanup_process: process %d message queue cleaned up\n", (int)process_pid);
		}

		kfree(process->queue);
	}

	kfree(process->name);
	kfree(process);
}

void cleanup_all_processes(void)
{
	struct process_node *process;
	struct process_node *next;

	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.cleanup_all_processes: process list not initialized\n");
		return;
	}

	process = process_list->head;
	next = NULL;
	while (process != NULL) {
		next = process->next;
		cleanup_process(process);
		process = next;
	}

	kfree(process_list);
	process_list = NULL;

	printk(KERN_INFO "mq_driver.cleanup_all_processes: all processes cleaned up\n");
}

struct process_node *get_process_by_pid(pid_t pid)
{
	struct process_node *process = process_list->head;
	while (process != NULL) {
		if (process->pid == pid) {
			return process;
		}
		process = process->next;
	}

	return NULL;
}

struct process_node *get_process_by_name(char *name)
{
	struct process_node *process = process_list->head;
	while (process != NULL) {
		if (strcmp(process->name, name) == 0) {
			return process;
		}
		process = process->next;
	}

	return NULL;
}

int check_process_messages(pid_t pid)
{
	struct process_node *process;

	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.check_process_messages: process list not initialized\n");
		return -1;
	}

	process = get_process_by_pid(pid);
	if (process == NULL) {
		printk(KERN_ALERT "mq_driver.check_process_messages: process with pid %d not found\n", (int)pid);
		return -1;
	}

	printk(KERN_INFO "mq_driver.check_process_messages: process %d has %d messages\n", (int)pid, process->queue->count);

	return process->queue->count;
}

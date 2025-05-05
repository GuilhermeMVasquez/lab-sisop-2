#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include "list_driver.h"

List *process_list = NULL;

void init_list(signed int max_messages, unsigned int max_message_size)
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
	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.register_process: process list not initialized\n");
		return -1;
	}

	if (get_process_by_pid(pid) != NULL) {
		printk(KERN_ALERT "mq_driver.register_process: process with pid %d already exists\n", pid);
		return -1;
	}

	if (get_process_by_name(name) != NULL) {
		printk(KERN_ALERT "mq_driver.register_process: process with name %s already exists\n", name);
		return -1;
	}

	struct process_node *new_process = (struct process_node *)kmalloc(sizeof(struct process_node), GFP_KERNEL);

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

	printk(KERN_INFO "mq_driver.register_process: process with pid %d and name %s registered\n", pid, name);

	return (int)new_process->pid;
}

int enqueue_message(pid_t pid, char *dest_name, char *msg)
{
	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.enqueue_message: process list not initialized\n");
		return -1;
	}

	if (get_process_by_pid(pid) == NULL) {
		printk(KERN_ALERT "mq_driver.enqueue_message: process with pid %d must be registered to send messages\n", pid);
		return -1;
	}

	struct process_node *dest_process = get_process_by_name(dest_name);
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

	printk(KERN_INFO "mq_driver.enqueue_message: message sent from pid %d to %s: %s\n", pid, dest_name, msg);

	return 0;
}

int unregister_process(pid_t pid, char *name)
{
	if (process_list == NULL) {
		printk(KERN_ALERT "mq_driver.unregister_process: process list not initialized\n");
		return -1;
	}

	struct process_node *current = process_list->head;
	struct process_node *prev = NULL;
	while (current != NULL) {
		if (current->pid == pid && strcmp(current->name, name) == 0) {
			if (prev == NULL) {
				process_list->head = current->next;
			} else {
				prev->next = current->next;
			}

			if (current == process_list->tail) {
				process_list->tail = prev;
			}

			cleanup_process(current);

			process_list->count--;

			printk(KERN_INFO "mq_driver.unregister_process: process with pid %d and name %s unregistered\n", pid, name);

			return 0;
		}

		prev = current;
		current = current->next;
	}
}

struct process_node *get_process_by_pid(pid_t pid)
{
	struct process_node *current = process_list->head;
	while (current != NULL) {
		if (current->pid == pid) {
			return current;
		}
		current = current->next;
	}

	return NULL;
}

struct process_node *get_process_by_name(char *name)
{
	struct process_node *current = process_list->head;
	while (current != NULL) {
		if (strcmp(current->name, name) == 0) {
			return current;
		}
		current = current->next;
	}

	return NULL;
}

void cleanup_process(struct process_node *process)
{
	if (process == NULL) {
		printk(KERN_ALERT "mq_driver.cleanup_process: process is NULL\n");
		return;
	}

	int pid = (int)process->pid;

	if (process->queue != NULL) {
		if (process->queue->messages != NULL) {
			while (process->queue->count > 0) {
				kfree(process->queue->messages[process->queue->head]);
				process->queue->head = (process->queue->head + 1) % process_list->max_messages;
				process->queue->count--;
			}

			kfree(process->queue->messages);
		}

		kfree(process->queue);
	}

	kfree(process->name);
	kfree(process);
}
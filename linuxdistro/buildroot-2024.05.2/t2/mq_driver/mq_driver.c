#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#define DEVICE_NAME "mq_driver"
#define CLASS_NAME "mq_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Guilherme Vasquez, Henrique Zapella, Pedro Muller");
MODULE_DESCRIPTION("A simple process message queue driver");
MODULE_VERSION("1.0");

typedef struct List
{
	struct process_node *head;
	struct process_node *tail;
	int count;
} List;

struct process_node
{
	pid_t pid;
	char *name;
	struct queue *queue;
	struct process_node *next;
};

struct queue
{
	char **messages;
	int head;
	int tail;
	int count;
};

static unsigned int n_msg = 0;
static unsigned int max_msg_len = 0;

module_param(n_msg, uint, 0644);
module_param(max_msg_len, uint, 0644);

static int major_number;
static int number_opens = 0;
static struct class *mq_class = NULL;
static struct device *mq_device = NULL;

List *process_list = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

void init_list(void);
int register_process(pid_t pid, char *name);
int enqueue_message(pid_t pid, char *dest_name, char *msg);
int unregister_process(pid_t pid, char *name);
char *dequeue_message(pid_t pid);
void cleanup_process(struct process_node *process);
void cleanup_all_processes(void);

struct process_node *get_process_by_pid(pid_t pid);
struct process_node *get_process_by_name(char *name);

int check_process_messages(pid_t pid);

static int mq_driver_init(void)
{
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "mq_driver: failed to register a major number\n");

		return major_number;
	}

	printk(KERN_INFO "mq_driver: registered correctly with major number %d\n", major_number);

	mq_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mq_class))
	{
		unregister_chrdev(major_number, DEVICE_NAME);

		printk(KERN_ALERT "mq_driver: failed to register device class\n");

		return PTR_ERR(mq_class);
	}

	printk(KERN_INFO "mq_driver: device class registered correctly\n");

	mq_device = device_create(mq_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(mq_device))
	{
		class_destroy(mq_class);
		unregister_chrdev(major_number, DEVICE_NAME);

		printk(KERN_ALERT "mq_driver: failed to create the device\n");

		return PTR_ERR(mq_device);
	}

	printk(KERN_INFO "mq_driver: device class created correctly\n");

	if (n_msg <= 0 || max_msg_len <= 0)
	{
		printk(KERN_ALERT "mq_driver: not loaded. Pass two positive integers!\n");
		printk(KERN_ALERT "mq_driver: Example: modprobe mq_driver n_msg=5 max_msg_len=250\n");

		return PTR_ERR(0);
	}

	printk(KERN_INFO "mq_driver: n_msg: %u, max_msg_len: %u\n", n_msg, max_msg_len);

	init_list();

	printk(KERN_INFO "mq_driver: loaded.\n");

	return 0;
}

static void mq_driver_exit(void)
{
	cleanup_all_processes();

	device_destroy(mq_class, MKDEV(major_number, 0));
	class_unregister(mq_class);
	class_destroy(mq_class);
	unregister_chrdev(major_number, DEVICE_NAME);

	printk(KERN_INFO "mq_driver: goodbye.\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	number_opens++;

	printk(KERN_INFO "mq_driver: device opened %d time(s)\n", number_opens);
	printk("Process id: %d, name: %s\n", (int)task_pid_nr(current), current->comm);

	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "mq_driver: device successfully closed\n");

	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	char *message;

	int message_count = check_process_messages(task_pid_nr(current));

	if (message_count < 0)
	{
		printk(KERN_ALERT "mq_driver: failed to check process messages\n");

		return -EFAULT;
	}

	if (message_count == 0)
	{
		printk(KERN_INFO "mq_driver: no messages to read\n");

		return 0;
	}

	message = dequeue_message(task_pid_nr(current));
	size_t message_len = strlen(message) + 1;

	int error = copy_to_user(buffer, message, message_len);

	kfree(message);

	if (error == 0)
	{
		printk(KERN_INFO "mq_driver: sent %zu characters to the user\n", message_len);

		return message_len;
	}
	else
	{
		printk(KERN_INFO "mq_driver: failed to send %zu characters to the user\n", message_len);

		return -EFAULT;
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	if (strncmp(buffer, "/reg ", 5) == 0)
	{
		if (register_process(task_pid_nr(current), buffer + 5) != 0)
		{
			printk(KERN_ALERT "mq_driver: failed to register process\n");
			return -1;
		}
	}
	else if (strncmp(buffer, "/unreg ", 7) == 0)
	{
		if (unregister_process(task_pid_nr(current), buffer + 7) != 0)
		{
			printk(KERN_ALERT "mq_driver: failed to unregister process\n");
			return -1;
		}
	}
	else if (buffer[0] == '/')
	{
		int i = 0;
		while (buffer[i] != ' ' && buffer[i] != '\0')
		{
			i++;
		}

		char *dest_name;
		char *msg;

		dest_name = kmalloc(i, GFP_KERNEL);
		strncpy(dest_name, buffer + 1, i - 1);
		dest_name[i - 1] = '\0';

		msg = kmalloc(len - i, GFP_KERNEL);
		strncpy(msg, buffer + i + 1, len - i - 1);
		msg[len - i - 1] = '\0';

		if (enqueue_message(task_pid_nr(current), dest_name, msg) != 0)
		{
			printk(KERN_ALERT "mq_driver: failed to enqueue message\n");

			kfree(msg);
			kfree(dest_name);

			return -1;
		}

		kfree(dest_name);
	}
	else
	{
		printk(KERN_WARNING "mq_driver: invalid command format\n");

		return -1;
	}

	return len;
}

void init_list()
{
	if (process_list == NULL)
	{
		process_list = (List *)kmalloc(sizeof(List), GFP_KERNEL);
		process_list->head = NULL;
		process_list->tail = NULL;
		process_list->count = 0;

		printk(KERN_INFO "mq_driver.init_list: process list initialized\n");
	}
}

int register_process(pid_t pid, char *name)
{
	struct process_node *new_process;

	if (process_list == NULL)
	{
		printk(KERN_ALERT "mq_driver.register_process: process list not initialized\n");
		return -1;
	}

	if (get_process_by_pid(pid) != NULL)
	{
		printk(KERN_ALERT "mq_driver.register_process: process with pid %d already exists\n", (int)pid);
		return -1;
	}

	if (get_process_by_name(name) != NULL)
	{
		printk(KERN_ALERT "mq_driver.register_process: process with name %s already exists\n", name);
		return -1;
	}

	new_process = (struct process_node *)kmalloc(sizeof(struct process_node), GFP_KERNEL);

	new_process->pid = pid;

	new_process->name = (char *)kmalloc(strlen(name) + 1, GFP_KERNEL);
	strcpy(new_process->name, name);

	new_process->queue = (struct queue *)kmalloc(sizeof(struct queue), GFP_KERNEL);
	new_process->queue->messages = (char **)kmalloc(n_msg * sizeof(char *), GFP_KERNEL);
	new_process->queue->head = 0;
	new_process->queue->tail = 0;
	new_process->queue->count = 0;

	new_process->next = NULL;

	if (process_list->head == NULL)
	{
		process_list->head = new_process;
		process_list->tail = new_process;
	}
	else
	{
		process_list->tail->next = new_process;
		process_list->tail = new_process;
	}

	process_list->count++;

	printk(KERN_INFO "mq_driver.register_process: process with pid %d and name %s registered\n", (int)pid, name);

	return 0;
}

int enqueue_message(pid_t pid, char *dest_name, char *msg)
{
	struct process_node *pid_process;
	struct process_node *dest_process;

	if (process_list == NULL)
	{
		printk(KERN_ALERT "mq_driver.enqueue_message: process list not initialized\n");
		return -1;
	}

	pid_process = get_process_by_pid(pid);
	if (pid_process == NULL)
	{
		printk(KERN_ALERT "mq_driver.enqueue_message: process with pid %d must be registered to send messages\n", (int)pid);
		return -1;
	}

	dest_process = get_process_by_name(dest_name);
	if (dest_process == NULL)
	{
		printk(KERN_ALERT "mq_driver.enqueue_message: destination process %s not found\n", dest_name);
		return -1;
	}

	if (dest_process->pid == pid)
	{
		printk(KERN_ALERT "mq_driver.enqueue_message: cannot send message to self\n");
		return -1;
	}

	if (strlen(msg) >= max_msg_len)
	{
		printk(KERN_ALERT "mq_driver.enqueue_message: message size exceeds maximum allowed size\n");
		return -1;
	}

	if (dest_process->queue->count >= n_msg)
	{
		printk(KERN_ALERT "mq_driver.enqueue_message: message queue for process %s full. Overwriting oldest message.\n",
			   dest_name);
	}

	if (dest_process->queue->tail == dest_process->queue->head && dest_process->queue->count > 0)
	{
		kfree(dest_process->queue->messages[dest_process->queue->head]);
		dest_process->queue->head = (dest_process->queue->head + 1) % n_msg;
		dest_process->queue->count--;
	}

	dest_process->queue->messages[dest_process->queue->tail] = (char *)kmalloc(strlen(msg) + 1, GFP_KERNEL);
	strcpy(dest_process->queue->messages[dest_process->queue->tail], msg);

	dest_process->queue->tail = (dest_process->queue->tail + 1) % n_msg;
	dest_process->queue->count++;

	printk(KERN_INFO "mq_driver.enqueue_message: message sent from pid %d to %s: %s\n", (int)pid, dest_name, msg);

	return 0;
}

int unregister_process(pid_t pid, char *name)
{
	struct process_node *process;
	struct process_node *prev;

	if (process_list == NULL)
	{
		printk(KERN_ALERT "mq_driver.unregister_process: process list not initialized\n");
		return -1;
	}

	process = process_list->head;
	prev = NULL;
	while (process != NULL)
	{
		if (process->pid == pid && strcmp(process->name, name) == 0)
		{
			if (prev == NULL)
			{
				process_list->head = process->next;
			}
			else
			{
				prev->next = process->next;
			}

			if (process == process_list->tail)
			{
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

	if (process_list == NULL)
	{
		printk(KERN_ALERT "mq_driver.dequeue_message: process list not initialized\n");
		return NULL;
	}

	process = get_process_by_pid(pid);
	if (process == NULL)
	{
		printk(KERN_ALERT "mq_driver.dequeue_message: process with pid %d not found\n", (int)pid);
		return NULL;
	}

	if (process->queue->count <= 0)
	{
		printk(KERN_INFO "mq_driver.dequeue_message: no messages to read for process %d\n", (int)pid);
		return NULL;
	}

	message = process->queue->messages[process->queue->head];
	process->queue->head = (process->queue->head + 1) % n_msg;
	process->queue->count--;

	printk(KERN_INFO "mq_driver.dequeue_message: message read for process %d: %s\n", (int)pid, message);

	return message;
}

void cleanup_process(struct process_node *process)
{
	pid_t process_pid;

	if (process == NULL)
	{
		printk(KERN_ALERT "mq_driver.cleanup_process: process is NULL\n");
		return;
	}

	process_pid = process->pid;

	if (process->queue != NULL)
	{
		if (process->queue->messages != NULL)
		{
			while (process->queue->count > 0)
			{
				kfree(process->queue->messages[process->queue->head]);
				process->queue->head = (process->queue->head + 1) % n_msg;
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

	if (process_list == NULL)
	{
		printk(KERN_ALERT "mq_driver.cleanup_all_processes: process list not initialized\n");
		return;
	}

	process = process_list->head;
	next = NULL;
	while (process != NULL)
	{
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
	while (process != NULL)
	{
		if (process->pid == pid)
		{
			return process;
		}
		process = process->next;
	}

	return NULL;
}

struct process_node *get_process_by_name(char *name)
{
	struct process_node *process = process_list->head;
	while (process != NULL)
	{
		if (strcmp(process->name, name) == 0)
		{
			return process;
		}
		process = process->next;
	}

	return NULL;
}

int check_process_messages(pid_t pid)
{
	struct process_node *process;

	if (process_list == NULL)
	{
		printk(KERN_ALERT "mq_driver.check_process_messages: process list not initialized\n");
		return -1;
	}

	process = get_process_by_pid(pid);
	if (process == NULL)
	{
		printk(KERN_ALERT "mq_driver.check_process_messages: process with pid %d not found\n", (int)pid);
		return -1;
	}

	printk(KERN_INFO "mq_driver.check_process_messages: process %d has %d messages\n", (int)pid, process->queue->count);

	return process->queue->count;
}

module_init(mq_driver_init);
module_exit(mq_driver_exit);
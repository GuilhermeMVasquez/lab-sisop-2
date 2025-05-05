#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/string.h>
#include "list_driver.h"

#define DEVICE_NAME "mq_driver"
#define CLASS_NAME "mq_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Guilherme Vasquez, Henrique Zapella, Pedro Muller");
MODULE_DESCRIPTION("A simple process message queue driver");
MODULE_VERSION("1.0");

static int major_number;
static struct class *mq_class = NULL;
static struct device *mq_device = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
};

static unsigned int num_messages = 0;
static unsigned int max_message_size = 0;

module_param(num_messages, unsigned int, 0644);
module_param(max_message_size, unsigned int, 0644);

static int mq_driver_init(void)
{
	if (num_messages == 0 || max_message_size == 0) {
		printk(KERN_ALERT "mq_driver: not loaded. Pass two positive integers!\n");
		printk(KERN_ALERT "mq_driver: Example: modprobe mq_driver num_messages=5 max_message_size=250\n");
		return -EINVAL;
	}

	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0) {
		printk(KERN_ALERT "mq_driver: failed to register a major number\n");
		return major_number;
	}

	printk(KERN_INFO "mq_driver: registered correctly with major number %d\n", major_number);

	mq_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mq_class)) {
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "mq_driver: failed to register device class\n");
		return PTR_ERR(mq_class);
	}

	printk(KERN_INFO "mq_driver: device class registered correctly\n");

	mq_device = device_create(mq_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(mq_device)) {
		class_destroy(mq_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "mq_driver: failed to create the device\n");
		return PTR_ERR(mq_device);
	}

	printk(KERN_INFO "mq_driver: device class created correctly\n");

	if (create_list(num_messages, max_message_size) != 0) {
		device_destroy(mq_class, MKDEV(major_number, 0));
		class_destroy(mq_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "mq_driver: failed to create the list\n");
		return -ENOMEM;
	}

	init_list(num_messages, max_message_size);

	printk(KERN_INFO "mq_driver: loaded.\n");

	return 0;
}

static void mq_driver_exit(void)
{
	device_destroy(mq_class, MKDEV(major_number, 0));
	class_unregister(mq_class);
	class_destroy(mq_class);
	unregister_chrdev(major_number, DEVICE_NAME);

	printk(KERN_INFO "mq_driver: goodbye.\n");
}

module_init(mq_driver_init);
module_exit(mq_driver_exit);

static int dev_open(struct inode *inodep, struct file *filep);
{
}

static int dev_release(struct inode *inodep, struct file *filep)
{
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	if (strncmp(buffer, "/reg ", 5) == 0) {
		if (register_process(task_pid_nr(current), buffer + 5) != 0) {
			printk(KERN_ALERT "mq_driver: failed to register process\n");
		}
	} else if (strncmp(buffer, "/unreg ", 7) == 0) {
		if (unregister_process(task_pid_nr(current), buffer + 7) != 0) {
			printk(KERN_ALERT "mq_driver: failed to unregister process\n");
		}
	} else if (buffer[0] == '/') {
		int i = 0;
		while (buffer[i] != ' ' && buffer[i] != '\0') {
			i++;
		}

		char *dest_name = kmalloc(i, GFP_KERNEL);
		strncpy(dest_name, buffer + 1, i - 1);
		dest_name[i - 1] = '\0';

		char *msg = kmalloc(len - i, GFP_KERNEL);
		strncpy(msg, buffer + i + 1, len - i - 1);
		msg[len - i - 1] = '\0';

		if (enqueue_message(task_pid_nr(current), dest_name, msg) != 0) {
			printk(KERN_ALERT "mq_driver: failed to enqueue message\n");
			kfree(msg);
		}

		kfree(dest_name);
	} else {
		printk(KERN_WARNING "mq_driver: invalid command format\n");
	}
}

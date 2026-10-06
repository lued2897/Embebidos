#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define TAM_BUF 256

static unsigned long aperturas;
static DEFINE_MUTEX(candado);

static int p6_open(struct inode *inode, struct file *f)
{
	mutex_lock(&candado);
	aperturas++;
	mutex_unlock(&candado);

	pr_info("open (pid %d, proceso %s)\n", current->pid, current->comm);

	return 0;
}

static ssize_t p6_read(struct file *f, char __user *dst,
		       size_t n, loff_t *pos)
{
	char buf[TAM_BUF];
	int largo;

	mutex_lock(&candado);

	largo = scnprintf(buf, TAM_BUF, "aperturas: %lu\n", aperturas);

	mutex_unlock(&candado);

	return simple_read_from_buffer(dst, n, pos, buf, largo);
}

static ssize_t p6_write(struct file *f, const char __user *src,
			size_t n, loff_t *pos)
{
	char buf[TAM_BUF];
	size_t len;

	if (n >= TAM_BUF)
		return -EINVAL;

	if (copy_from_user(buf, src, n))
		return -EFAULT;

	buf[n] = '\0';

	len = n;

	if (len > 0 && buf[len - 1] == '\n')
		buf[--len] = '\0';

	if (len != 5 || memcmp(buf, "reset", 5) != 0)
		return -EINVAL;

	mutex_lock(&candado);
	aperturas = 0;
	mutex_unlock(&candado);

	pr_info("contador reiniciado (pid %d)\n", current->pid);

	return n;
}

static const struct file_operations p6_fops = {
	.owner = THIS_MODULE,
	.open = p6_open,
	.read = p6_read,
	.write = p6_write,
	.llseek = default_llseek,
};

static struct miscdevice p6_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "p6cnt",
	.fops = &p6_fops,
	.mode = 0666,
};

static int __init p6_init(void)
{
	int rc;

	aperturas = 0;

	rc = misc_register(&p6_dev);
	if (rc) {
		pr_err("misc_register fallo: %d\n", rc);
		return rc;
	}

	pr_info("/dev/p6cnt creado, minor %d\n", p6_dev.minor);

	return 0;
}

static void __exit p6_exit(void)
{
	unsigned long total;

	mutex_lock(&candado);
	total = aperturas;
	mutex_unlock(&candado);

	misc_deregister(&p6_dev);

	pr_info("total de aperturas: %lu\n", total);
	pr_info("/dev/p6cnt eliminado\n");
}

module_init(p6_init);
module_exit(p6_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("raspitinho");
MODULE_DESCRIPTION("Practica 6 FSE: contador de aperturas");
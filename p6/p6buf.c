#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#define TAM_BUF 256
static char buf[TAM_BUF];
static size_t largo;
static DEFINE_MUTEX(candado);
static int p6_open(struct inode *inode, struct file *f)
{
pr_info("open (pid %d, proceso %s)\n", current->pid, current->comm);
return 0;
}
static ssize_t p6_read(struct file *f, char __user *dst, size_t n, loff_t *pos)
{
ssize_t r;
mutex_lock(&candado);
r = simple_read_from_buffer(dst, n, pos, buf, largo);
mutex_unlock(&candado);
return r;
}
static ssize_t p6_write(struct file *f, const char __user *src, size_t n,
loff_t *pos)
{
if (n > TAM_BUF)
return -ENOSPC;
mutex_lock(&candado);
if (copy_from_user(buf, src, n)) {
mutex_unlock(&candado);
return -EFAULT;
}
largo = n;
mutex_unlock(&candado);
pr_info("write de %zu bytes (pid %d)\n", n, current->pid);
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
.name = "p6buf",
.fops = &p6_fops,
.mode = 0666,
};
static int __init p6_init(void)
{
int rc = misc_register(&p6_dev);
if (rc) {
pr_err("misc_register fallo: %d\n", rc);
return rc;
}
pr_info("/dev/p6buf creado, minor %d\n", p6_dev.minor);
return 0;
}
static void __exit p6_exit(void)
{
misc_deregister(&p6_dev);
pr_info("/dev/p6buf eliminado\n");
}
module_init(p6_init);
module_exit(p6_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("raspitinho");
MODULE_DESCRIPTION("Practica 6 FSE: dispositivo de caracteres virtual");
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/module.h>
static int __init hola_init(void)
{
    pr_info("modulo cargado\n");
    return 0;
}
    static void __exit hola_exit(void)
{
    pr_info("modulo descargado\n");
}
module_init(hola_init);
module_exit(hola_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("raspitinho");
MODULE_DESCRIPTION("Practica 6 FSE: modulo minimo");
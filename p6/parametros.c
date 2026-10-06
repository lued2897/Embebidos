#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
static char *nombre = "mundo";
module_param(nombre, charp, 0444);
MODULE_PARM_DESC(nombre, "Nombre al que se saluda");
static int veces = 1;
module_param(veces, int, 0644);
MODULE_PARM_DESC(veces, "Cuantas veces se saluda (1 a 5)");
static int __init parametros_init(void)
{
int i;
if (veces < 1 || veces > 5) {
pr_err("veces=%d fuera de rango (1 a 5)\n", veces);
return -EINVAL;
}
for (i = 0; i < veces; i++)
pr_info("hola %s (%d de %d)\n", nombre, i + 1, veces);
return 0;
}
static void __exit parametros_exit(void)
{
pr_info("adios %s, veces vale %d al descargar\n", nombre, veces);
}
module_init(parametros_init);
module_exit(parametros_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("raspitinho");
MODULE_DESCRIPTION("Practica 6 FSE: parametros de modulo");
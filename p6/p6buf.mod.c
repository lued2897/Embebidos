#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0x62129a31, "misc_register" },
	{ 0x92997ed8, "_printk" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x828ce6bb, "mutex_lock" },
	{ 0x528c709d, "simple_read_from_buffer" },
	{ 0x9618ede0, "mutex_unlock" },
	{ 0xfd874cb8, "misc_deregister" },
	{ 0xae353d77, "arm_copy_from_user" },
	{ 0x5f754e5a, "memset" },
	{ 0xd7bade89, "default_llseek" },
	{ 0x66af6230, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "41ADDEB8D1D5236A68E92D6");

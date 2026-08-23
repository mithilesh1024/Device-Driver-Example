#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
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

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const char ____versions[]
__used __section("__versions") =
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x10\x00\x00\x00\xe6\x6e\xab\xbc"
	"sscanf\0\0"
	"\x14\x00\x00\x00\x4b\x8d\xfa\x4d"
	"mutex_lock\0\0"
	"\x1c\x00\x00\x00\xda\x82\xc8\x8c"
	"device_remove_file\0\0"
	"\x18\x00\x00\x00\x3a\x6f\x9e\x01"
	"device_destroy\0\0"
	"\x10\x00\x00\x00\xba\x0c\x7a\x03"
	"kfree\0\0\0"
	"\x18\x00\x00\x00\x38\xf0\x13\x32"
	"mutex_unlock\0\0\0\0"
	"\x28\x00\x00\x00\xb3\x1c\xa2\x87"
	"__ubsan_handle_out_of_bounds\0\0\0\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x10\x00\x00\x00\xfd\xf9\x3f\x3c"
	"sprintf\0"
	"\x18\x00\x00\x00\x56\x3f\xf4\x50"
	"class_create\0\0\0\0"
	"\x20\x00\x00\x00\xfe\x43\xae\x58"
	"class_create_file_ns\0\0\0\0"
	"\x18\x00\x00\x00\x3f\x1f\x2f\x34"
	"class_destroy\0\0\0"
	"\x20\x00\x00\x00\x10\xdd\x7f\xbc"
	"class_remove_file_ns\0\0\0\0"
	"\x1c\x00\x00\x00\x63\xa5\x03\x4c"
	"random_kmalloc_seed\0"
	"\x18\x00\x00\x00\x1f\xc3\xc4\x85"
	"kmalloc_caches\0\0"
	"\x18\x00\x00\x00\x8f\xd7\x93\x9f"
	"kmalloc_trace\0\0\0"
	"\x18\x00\x00\x00\xb3\xfc\x49\xd5"
	"device_create\0\0\0"
	"\x1c\x00\x00\x00\x38\x8d\xe9\x0d"
	"device_create_file\0\0"
	"\x18\x00\x00\x00\xfb\x29\xf6\xc8"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "F941AA368ADDE18107C908B");

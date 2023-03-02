/*
 * hello_module.c
 *
 * This is an system call trace demo program for ARM32 architecture.
 *
 * Copyright (c) 2023 Leng Xujun <lengxujun2007@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>


static int __init hello_module_init(void)
{
	printk(KERN_INFO "Hello, module!\n");
	return 0;
}

static void __exit hello_module_exit(void)
{
	printk(KERN_INFO "Bye, module!\n");
}

module_init(hello_module_init);
module_exit(hello_module_exit);

MODULE_AUTHOR("Leng Xujun <lengxujun2007@126.com>");
MODULE_LICENSE("GPL");

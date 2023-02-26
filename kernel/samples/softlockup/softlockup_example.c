/*
 * An example for soft lockup task test.
 *
 * Copyright (c) 2023 Leng Xujun <lengxujun2007@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>


static struct task_struct *softlockup_task;


static int softlockup_task_fn(void *ignored)
{
	int ret = 0;

	while (!kthread_should_stop())
		asm("nop");

	return ret;
}


static int __init softlockup_task_demo_init(void)
{
	int ret = 0;

	softlockup_task = kthread_run(softlockup_task_fn, NULL, "softlockup_task");
	if (IS_ERR(softlockup_task)) {
		ret = PTR_ERR(softlockup_task);
		printk(KERN_ERR "%s: Failed to create kernel thread, ret = [%d]\n", __func__, ret);
	}

	printk(KERN_INFO "soft lockup task example module loaded.\n");

	return ret;
}

static void __exit softlockup_task_demo_exit(void)
{
	if (softlockup_task) {
		kthread_stop(softlockup_task);
		softlockup_task = NULL;
	}

	printk(KERN_INFO "soft lockup task example module exited.\n");
}

module_init(softlockup_task_demo_init);
module_exit(softlockup_task_demo_exit);

MODULE_AUTHOR("Leng Xujun <lengxujun2007@126.com>");
MODULE_LICENSE("GPL");

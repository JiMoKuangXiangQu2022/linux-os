/*
 * An example for hung task test.
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
#include <linux/delay.h>


static struct task_struct *hung_task;


static int hung_task_fn(void *ignored)
{
	int ret = 0;

	while (!kthread_should_stop())
		msleep((CONFIG_DEFAULT_HUNG_TASK_TIMEOUT + 8) * 1000);

	return ret;
}


static int __init hung_task_demo_init(void)
{
	int ret = 0;

	hung_task = kthread_run(hung_task_fn, NULL, "hung_task_demo");
	if (IS_ERR(hung_task)) {
		ret = PTR_ERR(hung_task);
		printk(KERN_ERR "%s: Failed to create kernel thread, ret = [%d]\n", __func__, ret);
	}

	printk(KERN_INFO "hung task example module loaded.\n");

	return ret;
}

static void __exit hung_task_demo_exit(void)
{
	if (hung_task) {
		kthread_stop(hung_task);
		hung_task = NULL;
	}

	printk(KERN_INFO "hung task example module exited.\n");
}

module_init(hung_task_demo_init);
module_exit(hung_task_demo_exit);

MODULE_AUTHOR("Leng Xujun <lengxujun2007@126.com>");
MODULE_LICENSE("GPL");

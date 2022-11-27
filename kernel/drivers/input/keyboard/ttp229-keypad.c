/*
 * Driver for the TTP229 matrix keypad.
 *
 * Copyright (c) 2019 Leng Xujun <lengxujun2007@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#define SAMPLE_FACILITY	0 			/* 0: timer sample; 1: work sample */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/kernel.h>
#if (SAMPLE_FACILITY == 0)
#include <linux/timer.h>
#elif (SAMPLE_FACILITY == 1)
#include <linux/workqueue.h>
#endif
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpio.h>


/*
 * sample configuration.
 */
#define SAMPLE_PERIOD	26			/* sample period, ms */
#define SAMPLE_LEVEL 	GPIO_HIGH 	/* sample bit on high level */
#define SAMPLE_CLK_T_US 100 		/* sample us of clock T, 
									   sample clock frequency = 1000000 / SAMPLE_CLK_T_US Hz */

/*
 * Simulate I2C bus using GPIOs.
 * Modify GPIOs according to actual hardware.
 */
#define TTP229_SCL 		RK30_PIN1_PD5 /* output */
#define TTP229_SDO 		RK30_PIN1_PD4 /* input/output */


struct ttp229_device {
	struct platform_device *pdev;
	
	struct input_dev *input_dev;
#if (SAMPLE_FACILITY == 0)
	struct timer_list input_timer;
#elif (SAMPLE_FACILITY == 1)
	struct delayed_work input_work;
#endif
	__u16 state;
};


static inline int ttp229_gpio_request(unsigned int gpio, const char *label)
{
	int result;

	result = gpio_request(gpio, label);
	if (result)
		pr_err("%s error, gpio = %d", __func__, gpio);
	
	return result;
}

static int ttp229_timing_init(struct ttp229_device *ttp229)
{
	int result;

	if (!ttp229)
		return -EINVAL;

	result = ttp229_gpio_request(TTP229_SCL, "ttp229-scl");
	if (result)
		goto scl_req_err;
	gpio_direction_output(TTP229_SCL, !SAMPLE_LEVEL);
	
	result = ttp229_gpio_request(TTP229_SDO, "ttp229-sdo");
	if (result)
		goto sdo_req_err;
	gpio_direction_output(TTP229_SDO, !SAMPLE_LEVEL);

	ttp229->state = 0xFFFF;

	return 0;

sdo_req_err:
	gpio_free(TTP229_SCL);
scl_req_err:
	return result;
}


static const struct ttp229_key_map {
	__u16 data;
	unsigned int code;
} key_hash_tb[] = {
	{ 0xFDFD, KEY_1  },  
	{ 0xFBFB, KEY_2  },  
	{ 0xF7F7, KEY_3  }, 
	{ 0xEFEF, KEY_4  }, 
	{ 0xDFDF, KEY_5  }, 
	{ 0xBFBF, KEY_6  }, 
	{ 0x7F7F, KEY_7  }, 
	{ 0xFEFF, KEY_8  },
#if 0	
	/* TODO: remain 8 keys */
	{ 0xFFFF, KEY_9  },  
	{ 0xFFFF, KEY_0  },  
	{ 0xFFFF, KEY_F1 }, 
	{ 0xFFFF, KEY_F2 }, 
	{ 0xFFFF, KEY_F3 }, 
	{ 0xFFFF, KEY_F4 }, 
	{ 0xFFFF, KEY_F5 }, 
	{ 0xFFFF, KEY_F6 },
#endif
};

static unsigned int ttp229_key_hash(__u16 data)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_hash_tb); i++)
		if (data == key_hash_tb[i].data)
			return key_hash_tb[i].code;
	return 0xFFFF;
}

static __u16 ttp229_sample(void)
{
	__u16 data = 0x0000;
	int i;

	gpio_direction_output(TTP229_SDO, SAMPLE_LEVEL);
	udelay(93);
	gpio_set_value(TTP229_SDO, !SAMPLE_LEVEL);
	udelay(10);

	gpio_direction_input(TTP229_SDO);
	for (i = 0; i < 16; ++i) {
		gpio_set_value(TTP229_SCL, SAMPLE_LEVEL);
		data |= (gpio_get_value(TTP229_SDO) << i);
		udelay(SAMPLE_CLK_T_US - 20);

		gpio_set_value(TTP229_SCL, !SAMPLE_LEVEL);;
		udelay(SAMPLE_CLK_T_US);
	}

	gpio_direction_output(TTP229_SDO, !SAMPLE_LEVEL);

	return data;
}

static void ttp229_key_report(struct ttp229_device *ttp229, __u16 new_state)
{
	if (ttp229->state == new_state) /* long tap not support now!!! */
		return;

	input_report_key(ttp229->input_dev, 
				ttp229_key_hash(new_state == 0xFFFF ? ttp229->state : new_state), 
				new_state == 0xFFFF ? 0 : 1);
	input_sync(ttp229->input_dev);

	ttp229->state = new_state;
}

#if (SAMPLE_FACILITY == 0)
static void ttp229_input_timer_fn(unsigned long data)
{
	struct ttp229_device *ttp229 = (struct ttp229_device *)data;

	ttp229_key_report(ttp229, ttp229_sample());
	mod_timer(&ttp229->input_timer, jiffies + msecs_to_jiffies(SAMPLE_PERIOD));
}
#elif (SAMPLE_FACILITY == 1)
static void ttp229_input_work(struct work_struct *work)
{
	struct delayed_work *input_work = to_delayed_work(work);
	struct ttp229_device *ttp229 = 
			container_of(input_work, struct ttp229_device, input_work);

	ttp229_key_report(ttp229, ttp229_sample());
	schedule_delayed_work(&ttp229->input_work, msecs_to_jiffies(SAMPLE_PERIOD));
}
#endif

static int ttp229_input_init(struct ttp229_device *ttp229)
{
	struct input_dev *input_dev;
	int i, result;

	input_dev = input_allocate_device();
	if (!input_dev) {
		result = -ENOMEM;
		dev_err(&ttp229->pdev->dev, "input_allocate_device error\n");
		goto input_alloc_err;
	}
	input_dev->name = "ttp229-keypad";
	input_dev->dev.parent = &ttp229->pdev->dev;
	for(i = 0; i < ARRAY_SIZE(key_hash_tb); i++)
		input_set_capability(input_dev, EV_KEY, key_hash_tb[i].code);
	__set_bit(EV_REP, input_dev->evbit);

	ttp229->input_dev = input_dev;
	result = input_register_device(ttp229->input_dev);
	if (result) {
		dev_err(&ttp229->pdev->dev, "input_register_device error\n");
		goto input_register_err;
	}

#if (SAMPLE_FACILITY == 0)
	setup_timer(&ttp229->input_timer, ttp229_input_timer_fn, (unsigned long)ttp229);
	mod_timer(&ttp229->input_timer, jiffies + msecs_to_jiffies(5));
#elif (SAMPLE_FACILITY == 1)
	INIT_DELAYED_WORK(&ttp229->input_work, ttp229_input_work);
	schedule_delayed_work(&ttp229->input_work, msecs_to_jiffies(5));
#endif

	return 0;

input_register_err:
	input_free_device(ttp229->input_dev);
input_alloc_err:
	return result;
}

static int ttp229_probe(struct platform_device *pdev)
{
	struct ttp229_device *ttp229;
	int result;

	ttp229 = kzalloc(sizeof(*ttp229), GFP_KERNEL);
	if (!ttp229) {
		result = -ENOMEM;
		dev_err(&pdev->dev, "out of memory\n");
		goto err_out;
	}

	ttp229->pdev = pdev;
	platform_set_drvdata(pdev, ttp229);

	result = ttp229_timing_init(ttp229);
	if (result)
		goto err_init;

	result = ttp229_input_init(ttp229);
	if (result)
		goto err_init;

    dev_info(&pdev->dev, "%s successful.\n", __func__);
	return 0;

err_init:
	kfree(ttp229);
err_out:
	dev_err(&pdev->dev, "%s failed, result = %d\n", __func__, result);
	return result;
}

static int ttp229_remove(struct platform_device *pdev)
{
	struct ttp229_device *ttp229 = platform_get_drvdata(pdev);

#if (SAMPLE_FACILITY == 0)
	del_timer_sync(&ttp229->input_timer);
#elif (SAMPLE_FACILITY == 1)
	cancel_delayed_work_sync(&ttp229->input_work);
#endif

	input_unregister_device(ttp229->input_dev);
	input_free_device(ttp229->input_dev);

	gpio_free(TTP229_SCL);
	gpio_free(TTP229_SDO);

	kfree(ttp229);

	return 0;
}

static struct platform_driver ttp229_keypad_driver = {
	.driver 	= {
		.name	= "ttp229-keypad",
		.owner	= THIS_MODULE,
	},
	.probe		= ttp229_probe,
	.remove		= __devexit_p(ttp229_remove),	
};

static int __init ttp229_keypad_init(void)
{
	return platform_driver_register(&ttp229_keypad_driver);
}

static void __exit ttp229_keypad_exit(void)
{
	platform_driver_unregister(&ttp229_keypad_driver);
}

module_init(ttp229_keypad_init);
module_exit(ttp229_keypad_exit);

MODULE_AUTHOR("Leng Xujun <lengxujun2007@126.com>");
MODULE_DESCRIPTION("TTP229 Matrix Keypad Driver");
MODULE_LICENSE("GPL");


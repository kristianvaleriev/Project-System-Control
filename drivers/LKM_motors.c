#include "linux/gpio/consumer.h"
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/uaccess.h>

#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/gpio.h>

struct gpio_desc* gpioLeft;
struct gpio_desc* gpioRight;

static ssize_t motor_write(struct file* file, const char __user* buf,
                           size_t count, loff_t* ppos)
{
    char* data = kzalloc(count, GFP_KERNEL);
    if (copy_from_user(data, buf, count)) {
        pr_err("copy from user\n");
        return -EFAULT;
    }

    data[count - 1] = '\0';
    pr_info("user data writen to driver: %s\n", data);

    for (size_t i = 0; i < count; i++)
    {
        if (data[i] >= 'a' && data[i] <= 'Z')
            data[i] -= 32;

        switch (data[i]) {
        case 'A':
            gpiod_set_value(gpioRight, GPIOD_OUT_HIGH);
            gpiod_set_value(gpioLeft, GPIOD_OUT_LOW);

            pr_info("turning left...\n");

            break;

        case 'D':
            gpiod_set_value(gpioRight, GPIOD_OUT_LOW);
            gpiod_set_value(gpioLeft, GPIOD_OUT_HIGH);

            pr_info("turning right...\n");

            break;

        case 'W':
            gpiod_set_value(gpioRight, GPIOD_OUT_HIGH);
            gpiod_set_value(gpioLeft, GPIOD_OUT_HIGH);

            pr_info("going forward...\n");

            break;

        case 'S':
            gpiod_set_value(gpioRight, GPIOD_OUT_LOW);
            gpiod_set_value(gpioLeft, GPIOD_OUT_LOW);

            pr_info("breaks...\n");

            break;
        }
    }

    kfree(data);

    *ppos += count;
    return count;
}

static ssize_t motor_read(struct file* file, char __user* buf,
                           size_t count, loff_t* ppos)
{
    pr_info("read fn called. Exiting...");
    return count;
}

static const struct file_operations motor_ops = {
    .owner = THIS_MODULE,
    .write = motor_write,
    .read  = motor_read,
};

static int motor_probe(struct platform_device* pdev)
{
    pr_info("entering probe...\n");

    gpioRight = devm_gpiod_get_index(&pdev->dev, NULL, 0, GPIOD_OUT_LOW);
    gpioLeft = devm_gpiod_get_index(&pdev->dev, NULL, 1, GPIOD_OUT_LOW);

    if (IS_ERR(gpioLeft)) {
        dev_err(&pdev->dev, "gpio get failed\n");
        return PTR_ERR(gpioLeft);
    }

    if (IS_ERR(gpioRight)) {
        dev_err(&pdev->dev, "gpio get failed\n");
        return PTR_ERR(gpioRight);
    }

    pr_info("successfully got gpios\n");

    struct miscdevice* motor_dev = kzalloc(sizeof *motor_dev, GFP_KERNEL);

    const char* name;
    of_property_read_string(pdev->dev.of_node, "label", &name);

    motor_dev->name = name;
    motor_dev->fops = &motor_ops;
    motor_dev->minor = MISC_DYNAMIC_MINOR;

    int status = misc_register(motor_dev);
    if (status) {
        pr_err("misc_register failed");
        return status;
    }

    platform_set_drvdata(pdev, motor_dev);

    pr_info("exiting probe\n");
    return 0;
}

static void motor_remove(struct platform_device* pdev)
{
    struct miscdevice* mdev = platform_get_drvdata(pdev);
    misc_deregister(mdev);

    pr_info("leaving remove fn");
}

static const struct of_device_id my_of_ids[] = {
    { .compatible = "kris,motor_robo"},
    {},
};
MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver motor_plat_driver = {
    .probe = motor_probe,
    .remove = motor_remove,
    .driver = {
        .name = "motor_car_driver",
        .owner = THIS_MODULE,
        .of_match_table = my_of_ids,
    },
};

module_platform_driver(motor_plat_driver);

MODULE_LICENSE("GPL");

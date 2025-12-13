#include "linux/init.h"
#include "linux/types.h"
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/uaccess.h>

#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>


static ssize_t motor_write(struct file* file, const char __user* buf,
                           size_t count, loff_t* ppos)
{
    char* data = kzalloc(count, GFP_KERNEL);
    if (copy_from_user(data, buf, count)) {
        pr_err("copy from user");
        return -EFAULT;
    }

    data[count - 1] = '\0';
    pr_info("msg rcved from user space: %s", data);

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
    pr_info("entering probe");

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

    pr_info("exiting probe");
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

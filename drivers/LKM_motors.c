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

static ssize_t motor_read(struct file* file, const char __user* buf,
                           size_t count, loff_t* ppos)
{
    return 1;
}

static const struct file_operations motor_ops = {
    .owner = THIS_MODULE,
    .write = motor_write,
    .read  = motor_read,
};

static int motor_probe(struct platform_device* pdev)
{
    pr_info("entering probe");

}

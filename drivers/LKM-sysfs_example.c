#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/printk.h>

static struct kobject* mymodule;
static int var = 0; // changable through /sys/kernel/m5-sys

static ssize_t var_show(struct kobject* kobj,
                        struct kobj_attribute* attr,
                        char* buf)
{
    return sprintf(buf, "%d\n", var);
}

static ssize_t var_store(struct kobject* kobj,
                         struct kobj_attribute* attr,
                         const char* buf, size_t count)
{
    sscanf(buf, "%d", &var);
    pr_info("val changed to: %d\n", var);
    return count;
}

static struct kobj_attribute var_attr = __ATTR(var, 0660, var_show, var_store);

static int __init m5_init(void)
{
    pr_info("initializing module...\n");

    mymodule = kobject_create_and_add("m5-sys", kernel_kobj);
    if (!mymodule)
        return -ENOMEM;

    int rc = sysfs_create_file(mymodule, &var_attr.attr);
    if (rc) {
        kobject_put(mymodule);
        pr_info("failed to create var file in /sys/kernel/m5-sys\n");
    }

    pr_info("successfuly created var file in /sys/kernel/m5-sys\n");

    return rc;
}

static void __exit m5_exit(void)
{
    pr_info("exit success\n");
    kobject_put(mymodule);
}

module_init(m5_init); 
module_exit(m5_exit); 
 
MODULE_LICENSE("GPL");

/**
 * dp-lkm.c - Test loadable kernel module.
 */

#include <linux/module.h>  
#include <linux/kernel.h>  
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hrebenarova"); 
MODULE_DESCRIPTION("test module, dp-hrebenarova");
MODULE_VERSION("0.1");

static int __init lkm_init(void)
{
    printk(KERN_INFO "[dp-lkm@hrebenarova] insmoded\n");
    return 0;
}

static void __exit lkm_exit(void)
{
    printk(KERN_INFO "[dp-lkm@hrebenarova] rmmoded\n");
}

module_init(lkm_init);
module_exit(lkm_exit);


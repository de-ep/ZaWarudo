#include <linux/module.h> // Needed by all modules 
#include <linux/printk.h> // Needed for pr_info() 

typedef enum {
    HYPERVISOR_NO_ERORR = 0,
} HypervisorError;


static int __init entry(void) {
    pr_info("ZaWarudo: Entry\n");
    HypervisorError err = HYPERVISOR_NO_ERORR;



    return 0;
}


static void __exit exit(void) {
    pr_info("ZaWarudo: Exit\n");
}


module_init(entry);
module_exit(exit);

MODULE_LICENSE("GPL v3");
MODULE_AUTHOR("deep");
MODULE_DESCRIPTION("A Simple Hypervisor");
#include <linux/module.h> // Needed by all modules 
#include <linux/printk.h> // Needed for pr_info() 
#include <linux/types.h>
#include <linux/string.h>

typedef enum {
    HYPERVISOR_NO_ERORR = 0,
    HYPERVISOR_GENERAL_ERROR,
    HYPERVISOR_CPU_UNSUPPORTED,
    HYPERVISOR_VMX_UNSUPPORTED,
} HypervisorError;


HypervisorError vmx_support(void) {
    /*
        23.6 DISCOVERING SUPPORT FOR VMX
            If CPUID.1:ECX.VMX[bit 5] = 1, then VMX operation is supported.
    */

    s32 ecx;

    asm (
        "cpuid"
        : "=c"(ecx)
        : "a"(1)
    );

    if ((ecx >> 5) & 1)
        return HYPERVISOR_NO_ERORR;
     
    return HYPERVISOR_VMX_UNSUPPORTED;
}

HypervisorError cpu_support(void) {
    /*
        Checking if intel CPU
        Table 3-17
        Executes the CPUID instruction with a value 0x00 in the EAX register, then reads the EBX, EDX, and ECX registers to determine if the BSP is “GenuineIntel.
        https://wiki.osdev.org/CPUID
    */

    s32 reg[3];
    char cpu_vendor_id[12];

    asm (
        "cpuid"
        : "=b"(reg[0]), "=d"(reg[1]), "=c"(reg[2]) 
        : "a"(0) 
    );

    memcpy(cpu_vendor_id, reg, 12);

    if (strcmp(cpu_vendor_id, "GenuineIntel"))
        return HYPERVISOR_CPU_UNSUPPORTED;

    return HYPERVISOR_NO_ERORR;
}


static int __init entry(void) {
    pr_info("ZaWarudo: Entry\n");
    HypervisorError err = HYPERVISOR_NO_ERORR;

    err = cpu_support();
    if(err) {  
        pr_info("ZaWarudo: CPU unsupported");
        return HYPERVISOR_CPU_UNSUPPORTED;    
    }

    err = vmx_support();
    if(err) {  
        pr_info("ZaWarudo: VMX unsupported");
        return HYPERVISOR_VMX_UNSUPPORTED;    
    }

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
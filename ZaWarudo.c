#include <linux/module.h> // Needed by all modules 
#include <linux/printk.h> // Needed for pr_info() 
#include <linux/types.h>
#include <linux/string.h>
#include <asm/msr.h>
#include <asm/page.h>

#define MSR_IA32_FEATURE_CONTROL 0x3A
#define MSR_IA32_VMX_CR0_FIXED0  0x486
#define MSR_IA32_VMX_CR0_FIXED1  0x487
#define MSR_IA32_VMX_CR4_FIXED0  0x488
#define MSR_IA32_VMX_CR4_FIXED1  0x489
#define MSR_IA32_VMX_BASIC       0x480

typedef enum {
    HYPERVISOR_NO_ERORR = 0,
    HYPERVISOR_GENERAL_ERROR,
    HYPERVISOR_CPU_UNSUPPORTED,
    HYPERVISOR_VMX_UNSUPPORTED,
    HYPERVISOR_FAILED_TO_ALLOCATE_MEMORY,
    HYPERVISOR_VMXON_FAILED
} HypervisorError;


HypervisorError vmxon(void) {
    /*
        23.7 ENABLING AND ENTERING VMX OPERATION
            Before system software can enter VMX operation, it enables VMX by setting CR4.VMXE[bit 13] = 1. 
            VMX operation is then entered by executing the VMXON instruction.
    
    */
    u64 cr4_vmxe = 1 << 13;
    u64 cr4;
    
    asm volatile (
        "mov %%cr4, %0"
        : "=r"(cr4)
    );

    u64 cr4_set = (cr4 >> 13) & 1;

    if (!cr4_set) {
        cr4 |= cr4_vmxe;

        asm volatile (
            "mov %0, %%cr4"
            :
            : "r"(cr4)
        );
    }


    /*
        VMXON is also controlled by the IA32_FEATURE_CONTROL MSR (MSR address 3AH). This MSR is cleared to zero
        when a logical processor is reset. The relevant bits of the MSR are:
        
        • Bit 0 is the lock bit. If this bit is clear, VMXON causes a general-protection exception. If the lock bit is set,
            WRMSR to this MSR causes a general-protection exception; the MSR cannot be modified until a power-up reset
            condition. System BIOS can use this bit to provide a setup option for BIOS to disable support for VMX. To   
            enable VMX support in a platform, BIOS must set bit 1, bit 2, or both (see below), as well as the lock bit.

        • Bit 1 enables VMXON in SMX operation. If this bit is clear, execution of VMXON in SMX operation causes a
            general-protection exception. Attempts to set this bit on logical processors that do not support both VMX
            operation (see Section 23.6) and SMX operation (see Chapter 6, “Safer Mode Extensions Reference,” in Intel®
            64 and IA-32 Architectures Software Developer’s Manual, Volume 2D) cause general-protection exceptions.

        • Bit 2 enables VMXON outside SMX operation. If this bit is clear, execution of VMXON outside SMX
            operation causes a general-protection exception. Attempts to set this bit on logical processors that do not
            support VMX operation (see Section 23.6) cause general-protection exceptions.
    */

    u64 ia32;

    rdmsrl(MSR_IA32_FEATURE_CONTROL, ia32);
    
    u64 ia32_set = ia32 & 1
    if (!ia32_set)
        msr_set_bit(MSR_IA32_FEATURE_CONTROL, 0);

    
    /*
        23.8 RESTRICTIONS ON VMX OPERATION
            In VMX operation, processors may fix certain bits in CR0 and CR4 to specific values and not support other values
             
            Software should consult the VMX capability MSRs IA32_VMX_CR0_FIXED0 and IA32_VMX_CR0_FIXED1 to determine how bits in CR0 are fixed (see Appendix A.7).
            
            For CR4, software should consult the VMX capability MSRs IA32_VMX_CR4_FIXED0 and IA32_VMX_CR4_FIXED1 (see Appendix A.8).


        A.7 VMX-FIXED BITS IN CR0
            If bit X is 1 in IA32_VMX_CR0_FIXED0, then that bit of CR0 is fixed to 1 in VMX operation.  
            if bit X is 0 in IA32_VMX_CR0_FIXED1, then that bit of CR0 is fixed to 0 in VMX operation. 


        A.8 VMX-FIXED BITS IN CR4
            If bit X is 1 in IA32_VMX_CR4_FIXED0, then that bit of CR4 is fixed to 1 in VMX operation.
            if bit X is 0 in IA32_VMX_CR4_FIXED1, then that bit of CR4 is fixed to 0 in VMX operation. 
    */

    u64 f0, f1, cr0;


    asm volatile (
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    rdmsrl(MSR_IA32_VMX_CR0_FIXED0, f0);
    rdmsrl(MSR_IA32_VMX_CR0_FIXED1, f1);    

    cr0 |= f0;
    cr0 &= f1;

    asm volatile (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
    );


    asm volatile(
        "mov %%cr4, %0"
        : "=r"(cr4)
    );

    rdmsrl(MSR_IA32_VMX_CR4_FIXED0, f0);
    rdmsrl(MSR_IA32_VMX_CR4_FIXED1, f1);

    cr4 |= f0;
    cr4 &= f1; 

    asm volatile(
        "mov %0, %%cr4"
        :
        : "r"(cr4)
    );


    /*
        24.11.5 VMXON Region
            Before executing VMXON, software allocates a region of memory (called the VMXON region)1 that the logical
            processor uses to support VMX operation. The physical address of this region (the VMXON pointer) is provided in
            an operand to VMXON.

            • The VMXON pointer must be 4-KByte aligned (bits 11:0 must be zero).
            • The VMXON pointer must not set any bits beyond the processor’s physical-address width
    
            Before executing VMXON, software should write the VMCS revision identifier (see Section 24.2) to the VMXON
            region. (Specifically, it should write the 31-bit VMCS revision identifier to bits 30:0 of the first 4 bytes of the VMXON
            region; bit 31 should be cleared to 0.) It need not initialize the VMXON region in any other way. Software should
            use a separate region for each logical processor and should not access or modify the VMXON region of a logical
            processor between execution of VMXON and VMXOFF on that logical processor. Doing otherwise may lead to unpre-
            dictable behavior (including behaviors identified in Section 24.11.1)
    */
    u8 vmxon_failed = 0;
    u64 vmcs_data;

    u64* vmxon_region = kzalloc(4096, GFP_KERNEL);
    if (vmxon_region == NULL)   
        return HYPERVISOR_FAILED_TO_ALLOCATE_MEMORY;    

    //If you have a logical address, the macro __pa() (defined in <asm/page.h>) will return its associated physical address.
    pa_vmxon_region = __pa(vmxon_region);

    //writing vmcs revision identifier
    rdmsrl(MSR_IA32_VMX_BASIC, vmcs_data);
    *(i32*)vmxon_region = (i32)vmcs_data;

    
    //executing vmxon
    asm volatile(
        "vmxon %[pa] \t\n
        setna %[vmxf]"
        : [vmxfa]"=r" (vmxon_failed)
        : [pa]"m"(pa_vmxon_region)
        : "cc"
    )

    if (vmxon_failed)
        return HYPERVISOR_VMXON_FAILED;

    
    return HYPERVISOR_NO_ERORR;
}

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
        pr_info("ZaWarudo: CPU unsupported\n");
        return HYPERVISOR_CPU_UNSUPPORTED;    
    }

    err = vmx_support();
    if(err) {  
        pr_info("ZaWarudo: VMX unsupported\n");
        return HYPERVISOR_VMX_UNSUPPORTED;    
    }

    err = vmxon();
    if(err) {  
        pr_info("ZaWarudo: vmxon failed\n");
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
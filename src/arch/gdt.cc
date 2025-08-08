export module arch.gdt;

import types;

export namespace arch {
    constexpr auto KERNEL_CS = 0x08;
    constexpr auto KERNEL_DS = 0x10;
    constexpr auto USER_CS   = 0x18;
    constexpr auto USER_DS   = 0x20;
    constexpr auto TSS_SEL   = 0x28;

    struct [[gnu::packed]] gdt_entry {
        u16 limit_low;
        u16 base_low;
        u8  base_middle;
        u8  access;
        u8  granularity;
        u8  base_high;
    };

    struct [[gnu::packed]] tss_descriptor {
        uint16_t limit_low;      // Limit bits 0-15
        uint16_t base_low;       // Base bits 0-15
        uint8_t  base_middle;    // Base bits 16-23
        uint8_t  type : 4;       // Type (should be 0x9 for available TSS)
        uint8_t  zero1 : 1;      // Must be 0
        uint8_t  dpl : 2;        // Descriptor Privilege Level
        uint8_t  present : 1;    // Present flag
        uint8_t  limit_high : 4; // Limit bits 16-19
        uint8_t  avl : 1;        // Available for software use
        uint8_t  zero2 : 2;      // Must be 0
        uint8_t  g : 1;          // Granularity
        uint8_t  base_high;      // Base bits 24-31
        uint32_t base_upper;     // Base bits 32-63 (upper 32 bits)
        uint32_t reserved;       // Must be 0
    } __attribute__((packed));

        struct [[gnu::packed]] gdt_pointer {
            u16 limit;
            u64 base;
        };

        // TSS structure for x86_64
        struct tss {
            uint32_t reserved0;
            uint64_t rsp0;      // Stack pointer for privilege level 0
            uint64_t rsp1;      // Stack pointer for privilege level 1
            uint64_t rsp2;      // Stack pointer for privilege level 2
            uint64_t reserved1;
            uint64_t ist[7];    // Interrupt Stack Table
            uint64_t reserved2;
            uint16_t reserved3;
            uint16_t iomap_base;
        } __attribute__((packed));


    alignas(16) gdt_entry gdt[6];
    alignas(16) tss tss[MAX_CPU];
    alignas(16) gdt_pointer gdtp;

    void 
    set_gdt_entry( int idx, u32 base, u32 limit, u8 access, u8 gran )
    {
        gdt[idx].limit_low    = limit & 0xFFFF;
        gdt[idx].base_low     = base & 0xFFFF;
        gdt[idx].base_middle  = (base >> 16) & 0xFF;
        gdt[idx].access       = access;
        gdt[idx].granularity  = ((limit >> 16) & 0x0F) | (gran & 0xF0);
        gdt[idx].base_high    = (base >> 24) & 0xFF;
    }
    
    void
    set_tss_entry( int idx, u64 base, u32 limit ) {
                // TSS descriptor takes two consecutive GDT entries
        tss_descriptor *tss_desc = (tss_descriptor *)&gdt[idx];
        
        // Set up the TSS descriptor
        tss_desc->limit_low = limit & 0xFFFF;
        tss_desc->base_low = base & 0xFFFF;
        tss_desc->base_middle = (base >> 16) & 0xFF;
        
        // Type field: 0x9 = Available 64-bit TSS
        tss_desc->type = 0x9;
        tss_desc->zero1 = 0;
        tss_desc->dpl = 0;  // Ring 0 only
        tss_desc->present = 1;
        
        tss_desc->limit_high = (limit >> 16) & 0xF;
        tss_desc->avl = 0;
        tss_desc->zero2 = 0;
        tss_desc->g = 0;  // Byte granularity
        
        tss_desc->base_high = (base >> 24) & 0xFF;
        tss_desc->base_upper = (base >> 32) & 0xFFFFFFFF;
        tss_desc->reserved = 0;
    }

    void 
    init_gdt() {
        set_gdt_entry(0, 0, 0, 0, 0);                  // Null
        set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);      // Kernel code
        set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xA0);      // Kernel data
        set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);      // User code (DPL=3)
        set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xA0);      // User data (DPL=3)
        set_tss_entry(5, (u64)tss, sizeof(tss));

        gdtp.limit = sizeof(gdt) + sizeof(tss_descriptor) - 1;
        gdtp.base  = reinterpret_cast<u64>(&gdt);

        asm( "lgdt %0"      : : "m"(gdtp) );
        asm( "mov %0, %%ds" : : "r"(0x10) );
        asm( "mov %0, %%es" : : "r"(0x10) );
        asm( "mov %0, %%fs" : : "r"(0x10) );
        asm( "mov %0, %%gs" : : "r"(0x10) );
        asm( "mov %0, %%ss" : : "r"(0x10) );  
        asm( "pushq %%rax"  : : "a"(0x08) );
        asm( "lea 1f(%%rip), %%rax    \n"
             "push %%rax              \n"
             "lretq                   \n"  
             "ltr %0                  \n"
             "1:                      \n" :: "r"(TSS_SEL): "rax" );  
    }
}

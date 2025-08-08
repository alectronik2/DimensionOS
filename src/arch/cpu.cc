export module arch.cpu;

import types;

export namespace arch {
    u32 
    get_id() {
        u32 eax, ebx, ecx, edx;
    
        eax = 1;
        __asm__ volatile(
            "cpuid"
            : "=b"(ebx), "=a"(eax), "=c"(ecx), "=d"(edx)
            : "a"(eax)
        );
        return (ebx >> 24) & 0xFF;  // initial APIC ID (processor/core ID)
    }

    void
    atomic_exchange( u64 *where, u64 value ) {
        asm volatile (
                "lock xchg %0, %1"
                : "+r" (value), "+m" (*where)
                : 
                : "memory"
            );
    }

    void 
    halt_cpu() {
        while( true ) {
            __asm__ volatile("hlt");
        }
    }

    inline uint64_t 
    read_msr(uint32_t msr) {
        uint32_t low, high;
        asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
        return ((uint64_t)high << 32) | low;
    }

    inline void 
    write_msr(uint32_t msr, uint64_t value) {
        uint32_t low = value & 0xFFFFFFFF;
        uint32_t high = value >> 32;
        asm volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high));
    }

    void
    enable_interrupts() {
        __asm__ volatile("sti");
    }

    void 
    disable_interrupts() {
        __asm__ volatile("cli");
    }
}
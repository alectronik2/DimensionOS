export module arch.lapic;

import types;
import arch.io;
import arch.cpu;
import arch.idt;
import lib.print;
import mm.pframe;
import sched;

// APIC Base MSR
#define IA32_APIC_BASE_MSR      0x1B

// Bits in the IA32_APIC_BASE MSR
#define APIC_GLOBAL_ENABLE      (1ULL << 11)
#define APIC_BASE_MASK          0xFFFFF000  // Bits 12–51 are base address

// The LAPIC MMIO base is usually at 0xFEE00000
#define LAPIC_BASE              lapic_base

#define LAPIC_TIMER_VECTOR     0x20    // Must match IDT entry
#define LAPIC_TIMER_MODE_ONESHOT   0x00000
#define LAPIC_TIMER_MODE_PERIODIC  0x20000
#define LAPIC_TIMER_MODE_TSC       0x40000

#define LAPIC_DIVIDE_BY_16     0x3

// LAPIC register offsets
#define LAPIC_ID                0x020
#define LAPIC_EOI               0x0B0
#define LAPIC_SVR               0x0F0
#define LAPIC_ESR               0x280
#define LAPIC_ICR_LOW           0x300
#define LAPIC_ICR_HIGH          0x310
#define LAPIC_TIMER             0x320
#define LAPIC_TIMER_INIT_CNT    0x380
#define LAPIC_TIMER_CUR_CNT     0x390
#define LAPIC_TIMER_DIV         0x3E0
#define LAPIC_LVT_TIMER   0x320
#define LAPIC_LVT_LINT0   0x350
#define LAPIC_LVT_LINT1   0x360
#define LAPIC_LVT_ERROR   0x370

#define LAPIC_ENABLE            0x100
#define SPURIOUS_VECTOR         0xFF  // Can be any vector from 0x10–0xFE

// LAPIC Timer modes
#define TIMER_MODE_ONE_SHOT     0x00000
#define TIMER_MODE_PERIODIC     0x20000
#define TIMER_MODE_TSC_DEADLINE 0x40000


#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

uint64_t lapic_base;

export namespace arch {
    // Write to LAPIC MMIO
    inline void lapic_write(uint32_t reg, uint32_t value) {
        volatile uint32_t* lapic = (volatile uint32_t*)LAPIC_BASE;
        lapic[reg / 4] = value;
    }

    // Read from LAPIC MMIO
    inline uint32_t lapic_read(uint32_t reg) {
        volatile uint32_t* lapic = (volatile uint32_t*)LAPIC_BASE;
        return lapic[reg / 4];
    }
    
    void io_wait() {
        // Use an unused port to cause slight delay
        outb(0x80, 0);
    }

    // Remap PIC and then mask it (disable)
    void disable_pic() {
        // Save current masks
        uint8_t mask1 = inb(PIC1_DATA);
        uint8_t mask2 = inb(PIC2_DATA);

        // Start initialization sequence
        arch::outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
        io_wait();
        arch::outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
        io_wait();

        // Remap PIC vectors to 0x20-0x2F (away from exceptions)
        arch::outb(PIC1_DATA, 0x20);
        io_wait();
        arch::outb(PIC2_DATA, 0x28);
        io_wait();

        // Tell Master PIC there is a slave at IRQ2
        arch::outb(PIC1_DATA, 0x04);
        io_wait();
        // Tell Slave PIC its cascade identity
        arch::outb(PIC2_DATA, 0x02);
        io_wait();

        // Set 8086/88 mode
        arch::outb(PIC1_DATA, ICW4_8086);
        io_wait();
        arch::outb(PIC2_DATA, ICW4_8086);
        io_wait();

        // Mask all interrupts (disable PIC)
        arch::outb(PIC1_DATA, 0xFF);
        arch::outb(PIC2_DATA, 0xFF);
    }

    void
    enable_lapic() {
        uint64_t apic_base = arch::read_msr(IA32_APIC_BASE_MSR);

        // Enable APIC by setting bit 11
        apic_base |= APIC_GLOBAL_ENABLE;

        // (Optional) Set base address to default if not already set
        apic_base &= ~APIC_BASE_MASK;
        apic_base |= LAPIC_BASE;

        arch::write_msr(IA32_APIC_BASE_MSR, apic_base);
    }

    void init_lapic_internal() {
        // Enable the Local APIC and set the spurious interrupt vector
        lapic_write(LAPIC_SVR, LAPIC_ENABLE | SPURIOUS_VECTOR);

        // Clear the Error Status Register by reading twice
        lapic_write(LAPIC_ESR, 0);
        lapic_read(LAPIC_ESR);
        lapic_write(LAPIC_ESR, 0);
        lapic_read(LAPIC_ESR);

        // Set the divide configuration (Divide by 16)
        lapic_write(LAPIC_TIMER_DIV, 0x3);

        // Set up the LAPIC Timer
        lapic_write(LAPIC_TIMER, TIMER_MODE_PERIODIC | 0x20);  // Vector 0x20, periodic mode
        lapic_write(LAPIC_TIMER_INIT_CNT, 10000000);           // Arbitrary initial count

        // Send EOI to clear any pending interrupts
        lapic_write(LAPIC_EOI, 0);
    }

    void 
    init_lapic_timer( uint32_t initial_count, bool periodic = true ) {
        // Set divide config (Divide by 16)
        lapic_write(LAPIC_TIMER_DIV, LAPIC_DIVIDE_BY_16);

        // Set vector and mode (periodic or one-shot)
        lapic_write(LAPIC_TIMER, 
            (periodic ? LAPIC_TIMER_MODE_PERIODIC : LAPIC_TIMER_MODE_ONESHOT) | LAPIC_TIMER_VECTOR);

        // Set initial count (this is how long the timer runs)
        lapic_write(LAPIC_TIMER_INIT_CNT, initial_count);
    }

    void 
    route_lapic_interrupts() {
        lapic_write(LAPIC_LVT_TIMER, 0xEF);  // Vector 0xEF for timer
        lapic_write(LAPIC_LVT_LINT0, 1 << 16);  // Mask LINT0 (if unused)
        lapic_write(LAPIC_LVT_LINT1, 1 << 16);  // Mask LINT1 (if unused)
        lapic_write(LAPIC_LVT_ERROR, 0xFE); // Vector 0xFE for error
    }


    void
    lapic_eoi( int no ) {
        lapic_write( LAPIC_EOI, no );
    }
    
    void
    lapic_timer_handler( arch::interrupt_context *ctx ) {
        static int timer_count = 0;
        if (++timer_count % 1000 == 0) {
            printk("[TIMER] Timer interrupt %d\n", timer_count);
        }
        sched::schedule_from_interrupt(ctx);
        lapic_eoi( 0 );
    }

    void 
    remap_lapic( uint64_t new_base ) {
        // Set new base and re-enable
        lapic_base &= ~APIC_BASE_MASK;
        lapic_base |= new_base;
        lapic_base |= APIC_GLOBAL_ENABLE;

        write_msr(IA32_APIC_BASE_MSR, lapic_base);
    }

    void
    init_lapic() {
        lapic_base = 0xFEE00000;

        enable_lapic();
        init_lapic_internal();
        disable_pic();

        route_lapic_interrupts();
        register_irq_handler( 0x20, lapic_timer_handler );

        init_lapic_timer( 100, true );
    }
}

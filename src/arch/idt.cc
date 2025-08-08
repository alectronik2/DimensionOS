export module arch.idt;

import types;
import lib.print;
import lib.string;
import arch.cpu;
import arch.io;

#include "idt_handlers.h"

typedef struct [[gnu::packed]] {
    u16 offset_lo;    ///< base address bits 0..15
    u16 selector;     ///< code segment selector, typically 0x08
    u8  ist;          ///< bits 0..2 holds Interrupt Stack Table offset, rest of bits zero
    u8  type_attr;    ///< types and attributes
    u16 offset_mid;   ///< base address bits 16..31
    u32 offset_hi;    ///< base address bits 32..63
    u32 zero;         ///< reserved
} idt_entry_t;

static idt_entry_t idt_table[256];

char* error_msgs[] = {
    "Divide by 0",
    "Reserved",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bounds range exceeded",
    "Invalid Opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 FPU error",
    "Alignment check",
    "Machine check",
    "SIMD Floating Point Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

export namespace arch {
    struct [[gnu::packed]] cpu_register_state  {
        u64 r15, r14, r13, r12, r11, r10, r9, r8, rdi, rsi, rbp, rdx, rcx, rbx, rax; 
    };

    struct [[gnu::packed]] interrupt_context {
        cpu_register_state regs; ///< general purpose registers which get pushed upon interrupt
        u64 int_no, err;      ///< interrupt number and the error code of the interrupt
        u64 rip, cs, rflags;  ///< Instruction pointer to see where the interrupt happened, code segments and flags
        u64 rsp, ss;          ///< Stack pointer and stack segment
    };

    struct [[gnu::packed]] idt_ptr {
        u16 size;         ///< size of whole idt
        u64 address;      ///< address to idt
    };

    void 
    register_interrupt_handler(size_t vec, void *handler, u8 ist, u8 type) {
        u64 p = (u64)handler;

        idt_table[vec].offset_lo = (u16)p;
        idt_table[vec].selector = 0x08;
        idt_table[vec].ist = ist;
        idt_table[vec].type_attr = type;
        idt_table[vec].offset_mid = (u16)(p >> 16);
        idt_table[vec].offset_hi = (u32)(p >> 32);
        idt_table[vec].zero = 0;
    }

    typedef void (irq_handler_t)( arch::interrupt_context *ctx );
    irq_handler_t *callbacks[ 256 ] = { 0 };


    void 
    init_idt() {
        for( size_t i = 0; i < 256; i++ )
            register_interrupt_handler(i, reinterpret_cast<void*>(handlers[i]), 0, 0x8e);

        idt_ptr idt_ptr = {
            sizeof(idt_table) - 1,
            (u64)idt_table
        };

        asm volatile (
            "lidt %0"
            :
            : "m" (idt_ptr)
        );

        memset( callbacks, 0, sizeof(callbacks) );
    }

    void
    lapic_unmask_irq( ulong irq ) {
        u8 mask = arch::inb( 0x21 );
        mask &= ~(1 || (irq - 0x20) );
        arch::outb( 0x21, mask );
    }

    void
    register_irq_handler( ulong no, irq_handler_t *handler ) {
        if( callbacks[no] )
            printk( "IRQ %i is already claimed by 0x%x (new: 0x%x)\n", no, handlers[no], handler );

        callbacks[no] = handler;

        printk( "[IRQ] Unmasking irq %i\n", no - 32 );
        lapic_unmask_irq( no - 32 );
    }
}

extern "C" void
handle_interrupt( arch::interrupt_context *ctx ) {
    u64 cr2;

    asm( "mov %%cr2, %0" : "=r"(cr2) );

    if( arch::callbacks[ctx->int_no] ) {
        arch::callbacks[ctx->int_no]( ctx );
    } else {
        printk( "Interrupt %i: %s | CR2: 0x%x\n", ctx->int_no, ctx->int_no < 32 ? error_msgs[ctx->int_no] : "IRQ", cr2 );
        debug::print_stacktrace(ctx->rip, ctx->regs.rbp);

        arch::halt_cpu(); // Halt the CPU after handling the interrupt
    }
}

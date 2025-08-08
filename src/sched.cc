export module sched;

import types;
import lib.string;
import lib.print;
import arch.gdt;
import mm.heap;
import arch.idt;

export namespace sched {
    enum task_state_t {
        TASK_READY = 0,
        TASK_RUNNING,
        TASK_BLOCKED,
        TASK_TERMINATED
    };

    struct task_context_t {
        uint64_t rax, rbx, rcx, rdx;
        uint64_t rsi, rdi, rbp, rsp;
        uint64_t r8, r9, r10, r11;
        uint64_t r12, r13, r14, r15;
        uint64_t rip, rflags;
        uint16_t cs, ss, ds, es, fs, gs;
        uint16_t padding;  // Add explicit padding for alignment
    } __attribute__((packed));

    struct task_t {
        uint32_t pid;
        task_state_t state;
        task_context_t context;
        void *stack_base;
        size_t stack_size;
        struct task_t *next;
    } __attribute__((packed));

    task_t *current_task = nullptr;
    task_t *task_queue = nullptr;
    uint32_t next_pid = 1;
    bool scheduler_ready = false;

    void
    print_task_queue();
    
    void
    init_kernel_task(task_t *task) {
        if (!task) {
            printk("[SCHED] ERROR: Cannot initialize null task\n");
            return;
        }
        
        // Check if the task address looks reasonable (not in low memory)
        if ((uint64_t)task < 0x1000) {
            printk("[SCHED] ERROR: Task address 0x%x looks invalid (too low)\n", task);
            return;
        }
        
        printk("[SCHED] Task structure sizes: task_t=%d, task_context_t=%d\n", 
               sizeof(task_t), sizeof(task_context_t));
        
        printk("[SCHED] Before memset: task=0x%x, end=0x%x\n", task, (uint64_t)task + sizeof(task_t));
        
        // Try to write to the memory first as a test
        volatile uint8_t *test_ptr = (volatile uint8_t*)task;
        printk("[SCHED] Testing memory write at 0x%x\n", test_ptr);
        *test_ptr = 0xFF;  // Test write
        uint8_t test_val = *test_ptr;  // Test read
        printk("[SCHED] Memory test result: wrote 0xFF, read 0x%x\n", test_val);
        
        memset(task, 0, sizeof(task_t));
        printk("[SCHED] After memset\n");
        
        task->pid = 0;  // Kernel task gets PID 0
        task->state = TASK_RUNNING;
        task->stack_base = nullptr;  // Kernel uses its own stack
        task->stack_size = 0;
        task->next = task;  // Point to itself initially
        
        printk("[SCHED] Basic fields set\n");
        
        // Initialize context to current CPU state
        task->context.rflags = 0x202;
        task->context.cs = 0x08;
        task->context.ss = 0x10;
        task->context.ds = task->context.es = task->context.fs = task->context.gs = 0x10;
        
        printk("[SCHED] Context initialized\n");
        
        // Set this as the task queue head
        task_queue = task;
        
        printk("[SCHED] Initialized kernel task at 0x%x as task queue head\n", task);
    }

    task_t*
    create_task( void *entry_point, void *user_stack, size_t stack_size ) {
        task_t *task = (task_t*)mm::kmalloc(sizeof(task_t));
        if (!task) {
            printk("[SCHED] Failed to allocate task\n");
            return nullptr;
        }
        
        memset(task, 0, sizeof(task_t));
        task->pid = next_pid++;
        task->state = TASK_READY;
        
        // Mark this as a new task that hasn't been interrupted yet
        task->context.rax = 0xDEADBEEF;  // Use a magic marker to identify new tasks
        task->stack_base = user_stack;
        task->stack_size = stack_size;
        
        task->context.rip = (uint64_t)entry_point;
        task->context.rsp = (uint64_t)user_stack + stack_size - 16;
        task->context.rflags = 0x202;
        task->context.cs = 0x08;
        task->context.ss = 0x10;
        task->context.ds = task->context.es = task->context.fs = task->context.gs = 0x10;
        
        printk("[SCHED] Task setup: entry=0x%x, rsp=0x%x, stack_base=0x%x\n", 
               task->context.rip, task->context.rsp, user_stack);
        
        if (!task_queue) {
            task->next = task;
            task_queue = task;
        } else {
            task_t *last = task_queue;
            while (last->next != task_queue) {
                last = last->next;
            }
            task->next = task_queue;
            last->next = task;
        }
        
        printk("[SCHED] Created task PID %d, entry=0x%x, stack=0x%x\n", 
               task->pid, entry_point, user_stack);
        
        return task;
    }

    extern "C" void switch_context(task_context_t *old_ctx, task_context_t *new_ctx);

    void
    save_interrupt_context(arch::interrupt_context *int_ctx, task_context_t *task_ctx) {
        task_ctx->rax = int_ctx->regs.rax;
        task_ctx->rbx = int_ctx->regs.rbx;
        task_ctx->rcx = int_ctx->regs.rcx;
        task_ctx->rdx = int_ctx->regs.rdx;
        task_ctx->rsi = int_ctx->regs.rsi;
        task_ctx->rdi = int_ctx->regs.rdi;
        task_ctx->rbp = int_ctx->regs.rbp;
        task_ctx->r8 = int_ctx->regs.r8;
        task_ctx->r9 = int_ctx->regs.r9;
        task_ctx->r10 = int_ctx->regs.r10;
        task_ctx->r11 = int_ctx->regs.r11;
        task_ctx->r12 = int_ctx->regs.r12;
        task_ctx->r13 = int_ctx->regs.r13;
        task_ctx->r14 = int_ctx->regs.r14;
        task_ctx->r15 = int_ctx->regs.r15;
        task_ctx->rip = int_ctx->rip;
        task_ctx->rsp = int_ctx->rsp;
        task_ctx->rflags = int_ctx->rflags;
        task_ctx->cs = int_ctx->cs;
        task_ctx->ss = int_ctx->ss;
        
        printk("[SCHED] Saving task context: RIP=0x%x, RSP=0x%x\n", int_ctx->rip, int_ctx->rsp);
    }

    void
    restore_interrupt_context(task_context_t *task_ctx, arch::interrupt_context *int_ctx) {
        int_ctx->regs.rax = task_ctx->rax;
        int_ctx->regs.rbx = task_ctx->rbx;
        int_ctx->regs.rcx = task_ctx->rcx;
        int_ctx->regs.rdx = task_ctx->rdx;
        int_ctx->regs.rsi = task_ctx->rsi;
        int_ctx->regs.rdi = task_ctx->rdi;
        int_ctx->regs.rbp = task_ctx->rbp;
        int_ctx->regs.r8 = task_ctx->r8;
        int_ctx->regs.r9 = task_ctx->r9;
        int_ctx->regs.r10 = task_ctx->r10;
        int_ctx->regs.r11 = task_ctx->r11;
        int_ctx->regs.r12 = task_ctx->r12;
        int_ctx->regs.r13 = task_ctx->r13;
        int_ctx->regs.r14 = task_ctx->r14;
        int_ctx->regs.r15 = task_ctx->r15;
        int_ctx->rip = task_ctx->rip;
        int_ctx->rsp = task_ctx->rsp;
        int_ctx->rflags = task_ctx->rflags;
        int_ctx->cs = task_ctx->cs;
        int_ctx->ss = task_ctx->ss;
        
        printk("[SCHED] Restoring task context: RIP=0x%x, RSP=0x%x\n", task_ctx->rip, task_ctx->rsp);
    }

    void
    schedule() {
        if (!current_task || !task_queue) return;
        
        task_t *next_task = current_task->next;
        if (!next_task) next_task = task_queue;
        
        if (next_task != current_task && next_task->state == TASK_READY) {
            task_t *prev_task = current_task;
            prev_task->state = TASK_READY;
            
            current_task = next_task;
            current_task->state = TASK_RUNNING;
            
            switch_context(&prev_task->context, &current_task->context);
        }
    }

    void
    schedule_from_interrupt(arch::interrupt_context *ctx) {
        if (!scheduler_ready) {
            return; // Silently ignore until scheduler is ready
        }
        
        if (!current_task) {
            printk("[SCHED] No current task\n");
            return;
        }
        
        if (!task_queue) {
            printk("[SCHED] No task queue\n");
            return;
        }
        
        task_t *next_task = current_task->next;
        if (!next_task) {
            printk("[SCHED] WARNING: current_task->next is null, using task_queue\n");
            next_task = task_queue;
        }
        
        if (!next_task) {
            printk("[SCHED] ERROR: No valid next task found\n");
            return;
        }
        
        printk("[SCHED] Current: PID %d (0x%x), Next: PID %d (0x%x)\n", 
               current_task->pid, current_task, next_task->pid, next_task);
        
        if (next_task != current_task && next_task->state == TASK_READY) {
            printk("[SCHED] Switching from PID %d to PID %d\n", 
                   current_task->pid, next_task->pid);
                   
            save_interrupt_context(ctx, &current_task->context);
            current_task->state = TASK_READY;
            
            current_task = next_task;
            current_task->state = TASK_RUNNING;
            
            // Check if this is a new task that hasn't run yet
            if (current_task->context.rax == 0xDEADBEEF) {
                printk("[SCHED] First-time scheduling task PID %d, preserving entry point 0x%x\n", 
                       current_task->pid, current_task->context.rip);
                // Clear the marker
                current_task->context.rax = 0;
            }
            
            restore_interrupt_context(&current_task->context, ctx);
        } else {
            if (next_task == current_task) {
                printk("[SCHED] Only one task in queue\n");
            } else {
                printk("[SCHED] Next task not ready, state: %d\n", next_task->state);
            }
        }
    }

    void
    yield() {
        // Simply enable interrupts and halt, allowing timer interrupt to schedule
        asm volatile("sti; hlt");
    }

    task_t*
    get_current_task() {
        return current_task;
    }

    void
    set_current_task(task_t *task) {
        if (!task) {
            printk("[SCHED] ERROR: Attempting to set null task as current\n");
            return;
        }
        
        printk("[SCHED] About to set current task to PID %d (at 0x%x)\n", task->pid, task);
        printk("[SCHED] Task state before: %d\n", task->state);
        
        current_task = task;
        printk("[SCHED] Set current_task pointer\n");
        
        task->state = TASK_RUNNING;
        printk("[SCHED] Set task state to RUNNING\n");
        
        printk("[SCHED] Successfully set current task\n");
    }

    void
    print_task_queue() {
        printk("[SCHED] Task queue:\n");
        if (!task_queue) {
            printk("  (empty)\n");
            return;
        }
        
        task_t *t = task_queue;
        int count = 0;
        do {
            printk("  PID %d: state=%d, next=0x%x\n", t->pid, t->state, t->next);
            if (t == current_task) printk("    ^ CURRENT\n");
            t = t->next;
            count++;
        } while (t != task_queue && count < 10);
        
        if (count >= 10) printk("  ... (truncated)\n");
    }

    void
    start_scheduler() {
        if (!current_task || !task_queue) {
            printk("[SCHED] ERROR: Cannot start scheduler without current task and queue\n");
            return;
        }
        
        scheduler_ready = true;
        printk("[SCHED] Scheduler started with %d task(s)\n", 
               task_queue ? 1 : 0); // TODO: count tasks properly
        print_task_queue();
    }
}
export module lib.spinlock;

import arch.cpu;

export class spinlock_t {
private:
    // Lock flag: 0 = unlocked, 1 = locked
    volatile int locked = 0;

public:
    spinlock_t() = default;

    inline void lock() {
        while (true) {
            int expected = 0;
            // xchg sets locked = 1 and returns old value in expected
            
            if (expected == 0) {
                // Successfully acquired lock
                break;
            }
            // Spin

            arch::halt_cpu();;
        }
    }

    inline void release() {
        asm volatile("" ::: "memory"); // Compiler barrier
        locked = 0;
    }

    // Non-copyable, non-movable
    spinlock_t(const spinlock_t&) = delete;
    spinlock_t& operator=(const spinlock_t&) = delete;
};
export module mm.pframe;

import types;
import arch.io;
import arch.simpleboot;
import arch.cpu;
import lib.print;
import lib.string;

constexpr auto HEAP_BASE = 0xFFFFFFFFF0002000UL;

u8    *bitmap;
size_t bitmap_size;
size_t total_memory;

inline void
set_page( size_t page ) {
    bitmap[page / 8] |= (1 << (page % 8));
}

inline void
clear_page( size_t page ) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static ulong heap_base = HEAP_BASE;

export namespace mm {
    constexpr auto PAGE_SIZE    = 4096; // 4 KiB pages  
    constexpr auto PGADDR_MASK  = ~0xFFF; // Mask for the page address 
    
    union pte_t {
    struct {
            bool    present       : 1;
            bool    writable      : 1;
            bool    user_access   : 1;
            bool    write_through : 1;
            bool    disable_cache : 1;
            bool    accessed      : 1;
            bool    dirty         : 1;
            bool    huge_page     : 1;
            bool    global        : 1;
            size_t  custom        : 3;
            size_t  addr          : 52;
        };
        size_t entry;
    };

    enum {
        P4_SHIFT = 39,
        P3_SHIFT = 30,
        P2_SHIFT = 21,
        P1_SHIFT = 12,
    };

    enum {
        PT_PRESENT  = 1 << 0, // Page present
        PT_RW       = 1 << 1, // Read/Write
        PT_USER     = 1 << 2, // User accessible
        PT_PWT      = 1 << 3, // Page Write-Through
        PT_PCD      = 1 << 4, // Page Cache Disable
        PT_ACCESSED = 1 << 5, // Page accessed
        PT_DIRTY    = 1 << 6, // Page dirty
        PT_PSE      = 1 << 7, // Page Size Extension (4 MiB page)
        PT_GLOBAL   = 1 << 8, // Global page
        PT_PAT      = 1 << 12, // Page Attribute Table
    };

    using p4_t = pte_t;
    using p3_t = pte_t;
    using p2_t = pte_t;
    using p1_t = pte_t;

    struct indexer_t {
        u64 p4_idx;
        u64 p3_idx;
        u64 p2_idx;
        u64 p1_idx;

        indexer_t( u64 virt_addr ) {
            this->p4_idx = (virt_addr >> P4_SHIFT) & 0x1FF;
            this->p3_idx = (virt_addr >> P3_SHIFT) & 0x1FF;
            this->p2_idx = (virt_addr >> P2_SHIFT) & 0x1FF;
            this->p1_idx = (virt_addr >> P1_SHIFT) & 0x1FF;
        }
    };

    physaddr_t
    phys_alloc_page( bool zeroed = true );

    void
    map_page( p4_t *dir, physaddr_t phys_addr, virtaddr_t virt_addr, u64 flags = PT_PRESENT | PT_RW | PT_USER );

    inline u64
    page_align_down( u64 addr ) {
        return addr & ~(PAGE_SIZE - 1);
    }

    inline u64
    page_align_up( u64 addr ) {
        return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }

    inline p4_t *
    get_current_page_dir() {
        p4_t *ret;
        __asm__ volatile( "mov %%cr3, %0" : "=a"(ret) );
        return ret;
    }

    void *
    heap_request_page() {
        ulong page  = phys_alloc_page();
        ulong ret   = heap_base;

        heap_base = HEAP_BASE;

        printk( "[HeapRequest] mapping 0x%lX to 0x%lX\n", page, heap_base );

        map_page( mm::get_current_page_dir(), page, heap_base, PT_PRESENT | PT_RW );
        heap_base += PAGE_SIZE;

        printk( "[HeapRequest] 0x%x\n", ret );
        return (void *)ret;
    }

    inline void
    phys_free_page( size_t base ) {
        clear_page( base / PAGE_SIZE );
    }

    inline void
    phys_free_range( size_t base, size_t size ) {
        for( size_t i = 0; i < size; i++ )
            phys_free_page( base + i * PAGE_SIZE );

        printk( "cleared range from 0x%x to 0x%x\n", base, size );
    }

    physaddr_t
    phys_alloc_page( bool zeroed ) {
        auto      *p     = bitmap;
        auto      *p_end = bitmap + bitmap_size;
        physaddr_t pg    = -1;

        while( p < p_end && *p == 0xFF )
            p++;

        if( p == p_end )
            panic( "Out of memory!" ); 

        for( auto i = 0; i < 8; i++ ) {
            if( !(*p & (1 << i)) ) {
                pg = (size_t)(((p - bitmap) * 8 + i)* PAGE_SIZE );
                break;
            }
        }

        if( pg == -1 )
            panic( "Failed to allocate page.\n" );

        set_page( pg / PAGE_SIZE );

        if( zeroed )
            memset( (void *)(pg), 0, PAGE_SIZE );

        return pg;
    }

    void
    phys_init_multiboot( multiboot_mmap_entry *mmap, int count ) {
        multiboot_mmap_entry *biggest_part = nullptr;
        size_t available_memory = 0;

        printk( "phys_init_multiboot: 0x%0x, count %d\n", (u64)mmap, count );

        for( auto i = 0; i < count; i++ ) {
            if( mmap[i].type == MULTIBOOT_MEMORY_AVAILABLE ) {
                available_memory += mmap[i].length;
                if( !biggest_part || mmap[i].length > biggest_part->length ) {
                    biggest_part = &mmap[i];
                }
            }
        }

        printk( "Total available memory: %d MB\n", available_memory / 1024 / 1024 );

        bitmap = (u8 *)biggest_part->base_addr + 0x100000; // hack for not crashing?? skip first MB. Guess easyboot places the kernel wrongly at 0x100000
        bitmap_size = available_memory / PAGE_SIZE / 8;
        biggest_part->base_addr += bitmap_size + 0x100000;

        printk( "Physical memory bitmap at 0x%0x, size %d bytes\n", (u64)bitmap, bitmap_size );
        memset( bitmap, 0xFF, bitmap_size );

        printk( "Kernel page table is at 0x%0x\n", (u64)get_current_page_dir() );

        if( biggest_part-> base_addr < 0x200000 )
            biggest_part->base_addr = 0x200000;
        phys_free_range( biggest_part->base_addr, biggest_part->length );

        auto p1 = phys_alloc_page();
        auto p2 = phys_alloc_page();
        printk( "[PhysMM] alloc1 = 0x%xl | alloc2 = 0x%xl\n", p1, p2 );
    }

    /*
     * Get a page table for the given index in the page directory.
     * If the table does not exist, it will be created with the given flags.
     * Returns a pointer to the page table.
     *
     * @param table The page directory or page table to get the table from.
     * @param index The index of the table to get.
     * @param flags Flags for the new page table (default: PT_PRESENT | PT_RW).
     * @return Pointer to the page table.
    */
    pte_t *
    get_table( p4_t *table, u16 index, int flags = PT_PRESENT | PT_RW ) {
        if( !table[index].present ) {
            table[index].entry       = phys_alloc_page();
            table[index].present     = true;
            table[index].writable    = (flags & PT_RW) != 0;
            table[index].user_access = (flags & PT_USER) != 0;
        }
        return (pte_t *)(table[index].entry & ~0xFFF);
    }

    /* 
     * Map a page into the specified page directory. 
     */
    void
    map_page( p4_t *dir, physaddr_t phys_addr, virtaddr_t virt_addr, u64 flags ) {
        indexer_t indexer( virt_addr );

        p4_t *p3 = get_table( dir, indexer.p4_idx, flags );
        p3_t *p2 = get_table( p3, indexer.p3_idx, flags );
        p2_t *p1 = get_table( p2, indexer.p2_idx, flags );

        p1[indexer.p1_idx].entry = (phys_addr & PGADDR_MASK) | flags; // Clear the lower 12 bits
    }

    void
    unmap_page( p4_t *dir, virtaddr_t virt_addr ) {
        indexer_t indexer( virt_addr );

        if( !dir[indexer.p4_idx].present )
            return;

        p3_t *p3 = (p3_t *)(dir[indexer.p4_idx].entry & PGADDR_MASK);
        if( !p3[indexer.p3_idx].present )
            return;

        p2_t *p2 = (p2_t *)(p3[indexer.p3_idx].entry & PGADDR_MASK);
        if( !p2[indexer.p2_idx].present )
            return;

        if( p2[indexer.p2_idx].huge_page ) {
            // If it's a huge page, we can just clear the entry
            p2[indexer.p2_idx].present = false; // Unmap the huge page
            return;
        }

        p1_t *p1 = (p1_t *)(p2[indexer.p2_idx].entry & PGADDR_MASK);
        if( !p1[indexer.p1_idx].present )
            return;

        p1[indexer.p1_idx].present = false; // Unmap the page
    }

    physaddr_t
    virt_to_phys( p4_t *dir, virtaddr_t virt_addr ) {
        indexer_t indexer( virt_addr );

        if( !dir[indexer.p4_idx].present )
            return -1;

        printk( "virt_to_phys: %lX -> P4[%d] = %lX\n", virt_addr, indexer.p4_idx, dir[indexer.p4_idx].entry );

        p3_t *p3 = (p3_t *)(dir[indexer.p4_idx].entry & PGADDR_MASK);
        if( !p3[indexer.p3_idx].present )
            return -1;

        printk( "virt_to_phys: %lX -> P3[%d] = %lX\n", virt_addr, indexer.p3_idx, p3[indexer.p3_idx].entry );

        p2_t *p2 = (p2_t *)(p3[indexer.p3_idx].entry & PGADDR_MASK);
        if( !p2[indexer.p2_idx].present )
            return -1;

        printk( "virt_to_phys: %lX -> P2[%d] = %lX | huge = %s\n", virt_addr, indexer.p2_idx, p2[indexer.p2_idx].entry, p2[indexer.p2_idx].huge_page ? "true" : "false" );

        if( p2[indexer.p2_idx].huge_page ) {
            // If it's a huge page, return the physical address directly
            return p2[indexer.p2_idx].entry & PGADDR_MASK; // Clear the lower 12 bits
        }

        p1_t *p1 = (p1_t *)(p2[indexer.p2_idx].entry & PGADDR_MASK);
        if( !p1[indexer.p1_idx].present )
            return -1;

        printk( "virt_to_phys: %lX -> P1[%d] = %lX\n", virt_addr, indexer.p1_idx, p1[indexer.p1_idx].entry );

        return p1[indexer.p1_idx].entry & PGADDR_MASK; // Clear the lower 12 bits
    }

    void
    test_mm() {
        p4_t *dir = get_current_page_dir();

        // Map a page at 0x1000 to physical address 0x2000
        /*map_page( dir, 0x200000, 0xFFFF1000000, PT_PRESENT | PT_RW | PT_USER );

        auto x = (u64 *)0xFFFF1000000;;
        *x = 0xDEADCAFEBABE; // Write to the mapped page
        printk( "Mapped value: 0x%lX\n", *x ); // Should print 42
 */
        printk( "Physical address of 0xFFFF1000000: 0x%lX\n", virt_to_phys( dir, 0xFFFF1000000 ) );
        printk( "Physical address of 0x100000: 0x%lX\n", virt_to_phys( dir, 0x100000 ) );
        printk( "Physical address of 0x201000: 0x%lX\n", virt_to_phys( dir, 0x201000 ) );
    }
}
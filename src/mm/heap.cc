export module mm.heap;

import types;
import arch.io;
import arch.cpu;
import lib.print;
import lib.spinlock;
import mm.pframe;

constexpr auto ROUND_NUM = 10;

typedef struct heap_header heap_header_t;
struct heap_header {
    ulong length;
    heap_header_t *next;
    heap_header_t *last;
    bool          is_free;

    heap_header_t *split( ulong size ) {
        if( size < ROUND_NUM )
            return nullptr;
    
        ulong split_length = this->length - size - sizeof(heap_header_t);
        if( split_length < ROUND_NUM )
            return nullptr;

        auto header = reinterpret_cast<heap_header_t *>((u8 *)this + size + sizeof(heap_header_t) );
        
        this->next->last = header;
        header->next     = this->next;
        this->next       = header;
        header->last     = this;

        header->length  = split_length;
        header->is_free = true;
        this->length    = size;

        if( this->last == this )
            this->last = header;

        return header;
    }
};

static heap_header_t *last_header;
static void *        heap_start;
static void *        heap_end;
static void *        heap_address;
static spinlock_t    kmalloc_lock;

export namespace mm {
    void combine_backward( heap_header_t *hdr );
    void combine_forward( heap_header_t *hdr );

    void 
    expand_heap( size_t size ) {
        kmalloc_lock.lock();

        if( size % ROUND_NUM ) {
            size -= (size % ROUND_NUM);
            size += ROUND_NUM;
        }

        auto pages = size / mm::PAGE_SIZE;
        auto header = reinterpret_cast<heap_header_t *>(heap_end);
    
        for( auto i = 0UL; i < pages; i++ ) {
            ulong page = mm::phys_alloc_page();
            mm::map_page( mm::get_current_page_dir(), page, reinterpret_cast<u64>(heap_end), mm::PT_PRESENT | mm::PT_RW );
            heap_end = reinterpret_cast<u8 *>(heap_end) + PAGE_SIZE;
        }

        header->is_free     = true;
        header->last        = last_header;
        last_header->next   = header;
        last_header         = header;
        header->next        = nullptr;
        header->length      = size - sizeof(heap_header_t);

        combine_backward( header );

        kmalloc_lock.release();
    }

    void *
    kmalloc( size_t size ) {
        void *ret = nullptr;

        if( !size )
            return nullptr;

        kmalloc_lock.lock();

        if( size % ROUND_NUM ) {
            size -= (size % ROUND_NUM);
            size += ROUND_NUM;
        }

        auto hdr = reinterpret_cast<heap_header_t *>(heap_start);
        while( true ) {
            if( hdr->is_free ) {
                if( hdr->length > size ) {
                    hdr->split( size );
                    hdr->is_free = false;
                    
                    ret = reinterpret_cast<void *>(hdr + sizeof(heap_header_t));
                    printk( "hdr at 0x%x\n", hdr );
                    goto finish;
                }   
                if( hdr->length == size ) {
                    hdr->is_free = false;

                    ret = reinterpret_cast<void *>(hdr + sizeof(heap_header_t));
                    goto finish; 
                }
            }

            hdr = hdr->next;
        }

        kmalloc_lock.release();
        expand_heap( size );
        return kmalloc( size );

    finish:
        kmalloc_lock.release();
        return ret;
    }

    void
    kfree( void *ptr ) {
        auto header = reinterpret_cast<heap_header_t *>(ptr) - 1;

        kmalloc_lock.lock();

        header->is_free = true;
        combine_forward( header );
        combine_backward( header );

        kmalloc_lock.release();
    }

    void
    combine_forward( heap_header_t *hdr ) {
        if( (hdr->next == nullptr) || !hdr->next->is_free )
            return;

        if( hdr->next == last_header )
            last_header = hdr;

        hdr->next = hdr->next->next;
        hdr->length = hdr->length + hdr->next->length + sizeof(heap_header_t);

        printk( "Combined forwards.\n" );
    }

    void
    combine_backward( heap_header_t *hdr ) {
        if( hdr->last != nullptr && hdr->last->is_free )
            combine_forward( hdr->last );

        printk( "Combined backwards.\n" );
    }

    void
    init_kmalloc( ulong pages = 10 ) {
        printk( "Initializing heap\n" );

        auto nu = mm::heap_request_page();
        printk( "[HEAP] starting at 0x%x\n", nu );

        if( nu == nullptr ) 
            panic( "Couldn't allocate page." );        

        void *iter = nu;
        for( ulong i = 0; i < pages - i; i++ ) {
            auto frame = mm::phys_alloc_page( true );
            mm::map_page( mm::get_current_page_dir(), reinterpret_cast<ulong>(frame), reinterpret_cast<ulong>(iter), mm::PT_PRESENT | mm::PT_RW );
            iter = (u8 *)iter + PAGE_SIZE;
        }

        

        heap_address = nu;
        ulong length = pages * PAGE_SIZE;

        heap_start  = heap_address;
        heap_end    = (void *)((u8 *)heap_start + length);

        heap_header_t *start_header = reinterpret_cast<heap_header_t *>(heap_address);
        start_header->length        = length - sizeof(heap_header_t);
        start_header->next          = nullptr;
        start_header->last          = nullptr;
        start_header->is_free       = true;

        last_header = start_header;
    }
}
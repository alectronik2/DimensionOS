import types;
import arch.io;
import arch.cpu;
import arch.gdt;
import arch.idt;
import arch.simpleboot;
import arch.lapic;
import arch.ps2;
import lib.print;
import lib.string;
import mm.pframe;
import mm.heap;
import sched;

u64 dbg_start = 0;
u64 dbg_end   = 0;

void
task1() {
  printk("[TASK1] Task1 started!\n");
  for(;;) {
    printk( "Task1\n" );
    sched::yield();
  }
}

void
task2() {
  printk("[TASK2] Task2 started!\n");
  for(;;) {
    printk( "Task2\n" );
    sched::yield();
  }
}

u8 stack1[4096];
u8 stack2[4096];

sched::task_t init_task;

extern "C" void
KernelMain( u32 magic, u64 addr ) {
    if( arch::get_id() != 0 ) {
        // Not the boot CPU, halt
        arch::halt_cpu();
    }

    printk( "Kernel started with magic: 0x%0x, addr: 0x%0llx\n", magic, addr );
    
    multiboot_tag *tag, *last;
    multiboot_mmap_entry *mmap;
    multiboot_tag_framebuffer *tagfb;

    multiboot_mmap_entry *tagmmap;
    size_t mmap_size;

    arch::init_gdt(); 
    arch::init_idt(); 
    
    auto size = ((multiboot_info *)addr)->total_size;
    printk( "Announced MBI size 0x%x\n", size );

    for( tag = (multiboot_tag *)(addr + 8), last = (multiboot_tag *) (addr + size);
         tag < last && tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (multiboot_tag *)((u8 *)tag + ((tag->size + 7) & ~7)) ) 
    {
        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
          printk ("Command line = %s\n",
                  ((multiboot_tag_cmdline *) tag)->string);
          break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
          printk ("Boot loader name = %s\n",
                  ((multiboot_tag_loader *) tag)->string);
          break;
        case MULTIBOOT_TAG_TYPE_MODULE:
          {
            char *mod_desc = ((multiboot_tag_module *) tag)->string;
            u64  mod_start = ((multiboot_tag_module *) tag)->mod_start;
            u64  mod_end   = ((multiboot_tag_module *) tag)->mod_end;

            if( !strcmp( mod_desc, "kernel.dbg" ) ) {
                dbg_start = mod_start;
                dbg_end   = mod_end;

                printk( "Debugging enabled, dbg module at 0x%X-0x%X.\n", dbg_start, dbg_end );
            } else {
                printk ("Module at 0x%0x-0x%0x. Command line %s\n", mod_start, mod_end, mod_desc );
            }
            
          break;
          }
        case MULTIBOOT_TAG_TYPE_MMAP:
          {
            tagmmap = ((multiboot_tag_mmap *) tag)->entries;
            mmap_size = tag->size / sizeof(multiboot_mmap_entry);

            printk ("mmap\n");
            for (mmap = ((multiboot_tag_mmap *) tag)->entries;
                 (u8 *) mmap < (u8 *) tag + tag->size;
                 mmap = (multiboot_mmap_entry *)((u64) mmap + ((multiboot_tag_mmap *)tag)->entry_size))
              printk (" base_addr = 0x%08x%8x,"
                      " length = 0x%08x%08x, type = 0x%0x %s, res = 0x%0x\n",
                      (unsigned) (mmap->base_addr >> 32),
                      (unsigned) (mmap->base_addr & 0xffffffff),
                      (unsigned) (mmap->length >> 32),
                      (unsigned) (mmap->length & 0xffffffff),
                      (unsigned) mmap->type,
                      mmap->type == MULTIBOOT_MEMORY_AVAILABLE ? "free" : (
                      mmap->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE ? "ACPI" : (
                      mmap->type == MULTIBOOT_MEMORY_NVS ? "ACPI NVS" : "used")),
                      (unsigned) mmap->reserved);

            mm::phys_init_multiboot( tagmmap, mmap_size );
          }
          break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
          {
            tagfb = (multiboot_tag_framebuffer *) tag;
            printk ("framebuffer\n");
            printk (" address 0x%08x%08x pitch %d\n",
                (unsigned) (tagfb->addr >> 32),
                (unsigned) (tagfb->addr & 0xffffffff),
                tagfb->pitch);
            printk (" width %d height %d depth %d bpp\n",
                tagfb->width,
                tagfb->height,
                tagfb->bpp);
            printk (" red channel:   at %d, %d bits\n",
                tagfb->red_field_pos,
                tagfb->red_mask_size);
            printk (" green channel: at %d, %d bits\n",
                tagfb->green_field_pos,
                tagfb->green_mask_size);
            printk (" blue channel:  at %d, %d bits\n",
                tagfb->blue_field_pos,
                tagfb->blue_mask_size);
            break;
          }
        case MULTIBOOT_TAG_TYPE_EFI64:
          printk ("EFI system table 0x%0x\n",
                  ((multiboot_tag_efi64 *) tag)->pointer);
          break;
        case MULTIBOOT_TAG_TYPE_EFI64_IH:
          printk ("EFI image handle 0x%0x\n",
                  ((multiboot_tag_efi64 *) tag)->pointer);
          break;
        case MULTIBOOT_TAG_TYPE_SMBIOS:
          printk ("SMBIOS table major %d minor %d\n",
                  ((multiboot_tag_smbios *) tag)->major,
                  ((multiboot_tag_smbios *) tag)->minor);
          break;
        case MULTIBOOT_TAG_TYPE_ACPI_OLD:
          printk ("ACPI table (1.0, old RSDP)");
          //dumpacpi ((u64)*((u32*)&((multiboot_tag_old_acpi *) tag)->rsdp[16]));
          break;
        case MULTIBOOT_TAG_TYPE_ACPI_NEW:
          printk ("ACPI table (2.0, new RSDP)");
          //dumpacpi (*((u64*)&((multiboot_tag_new_acpi *) tag)->rsdp[24]));
          break;
        /* additional, not in the original Multiboot2 spec */
        case MULTIBOOT_TAG_TYPE_EDID:
          printk ("EDID info\n");
          printk (" manufacturer ID %02x%02x\n",
            ((multiboot_tag_edid *) tag)->edid[8], ((multiboot_tag_edid *) tag)->edid[9]);
          printk (" EDID ID %02x%02x Version %d Rev %d\n",
            ((multiboot_tag_edid *) tag)->edid[10], ((multiboot_tag_edid *) tag)->edid[11],
            ((multiboot_tag_edid *) tag)->edid[18], ((multiboot_tag_edid *) tag)->edid[19]);
          printk (" monitor type %02x size %d cm x %d cm\n",
            ((multiboot_tag_edid *) tag)->edid[20], ((multiboot_tag_edid *) tag)->edid[21],
            ((multiboot_tag_edid *) tag)->edid[22]);
          break;
        case MULTIBOOT_TAG_TYPE_SMP:
          printk ("SMP supported\n");
          printk (" %d core(s)\n", ((multiboot_tag_smp*) tag)->num_cores);
          printk (" %d running\n", ((multiboot_tag_smp*) tag)->running_cores);
          printk (" %02x bsp id\n", ((multiboot_tag_smp*) tag)->bspid);
          break;
        case MULTIBOOT_TAG_TYPE_PARTUUID:
          printk ("Partition UUIDs\n");
          //printk (" boot "); dumpuuid(((multiboot_tag_partuuid*) tag)->bootuuid);
          if(tag->size >= 40) {
          // printk (" root "); dumpuuid(((multiboot_tag_partuuid*) tag)->rootuuid);
          }
          break;
        default:
          printk ("---unknown MBI tag, this shouldn't happen with Simpleboot/Easyboot!---\n");
        }

    }

    arch::init_lapic();

    sched::init_kernel_task( &init_task );
    sched::set_current_task( &init_task );

    mm::test_mm(); 
    mm::init_kmalloc();

    sched::create_task( (void *)&task1, stack1, 4096 );
    sched::create_task( (void *)&task2, stack2, 4096 );

    sched::start_scheduler();
    arch::enable_interrupts();

    auto a1 = mm::kmalloc( 16 );
    auto a2 = mm::kmalloc( 32 );
    mm::kfree( a1 );
    mm::kfree( a2 );
    auto a3 = mm::kmalloc( 16 );
    printk( "KMALLOC a1=0x%X | a2=0x%X | a3=0x%X\n", a1, a2, a3 );

    if( !arch::init_ps2() )
      panic("Failed to initialize PS2\n" );

    arch::halt_cpu();
}

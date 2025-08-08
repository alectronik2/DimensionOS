export module arch.simpleboot;

import types;

export {
    constexpr auto MULTIBOOT2_BOOTLOADER_MAGIC         = 0x36d76289;
    constexpr auto MULTIBOOT_MOD_ALIGN                 = 0x00001000;
    constexpr auto MULTIBOOT_INFO_ALIGN                = 0x00000008;

    enum {
        MULTIBOOT_TAG_ALIGN                 = 8,
        MULTIBOOT_TAG_TYPE_END              = 0,
        MULTIBOOT_TAG_TYPE_CMDLINE          = 1,
        MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME = 2,
        MULTIBOOT_TAG_TYPE_MODULE           = 3,
        MULTIBOOT_TAG_TYPE_MMAP             = 6,
        MULTIBOOT_TAG_TYPE_FRAMEBUFFER      = 8,
        MULTIBOOT_TAG_TYPE_EFI64            = 12,
        MULTIBOOT_TAG_TYPE_SMBIOS           = 13,
        MULTIBOOT_TAG_TYPE_ACPI_OLD         = 14,
        MULTIBOOT_TAG_TYPE_ACPI_NEW         = 15,
        MULTIBOOT_TAG_TYPE_EFI64_IH         = 20,
        MULTIBOOT_TAG_TYPE_EDID             = 256,
        MULTIBOOT_TAG_TYPE_SMP              = 257,
        MULTIBOOT_TAG_TYPE_PARTUUID         = 258,
    };

    struct multiboot_info {
        u32 total_size;
        u32 reserved;
    };

    struct multiboot_tag {
        u32 type;
        u32 size;
    };

    struct multiboot_tag_cmdline {
        u32 type;
        u32 size;
        char string[0];
    };

    struct multiboot_tag_loader {
        u32 type;
        u32 size;
        char string[0];
    };

    struct multiboot_tag_module {
        u32 type;
        u32 size;
        u32 mod_start;
        u32 mod_end;
        char string[0];
    };

    enum {
        MULTIBOOT_MEMORY_AVAILABLE          = 1,
        MULTIBOOT_MEMORY_RESERVED           = 2,
        MULTIBOOT_MEMORY_ACPI_RECLAIMABLE   = 3,
        MULTIBOOT_MEMORY_NVS                = 4,
        MULTIBOOT_MEMORY_BADRAM             = 5,
    };

    struct multiboot_mmap_entry {
        u64 base_addr;
        u64 length;
        u32 type;
        u32 reserved;
    };

    struct multiboot_tag_mmap {
        u32 type;
        u32 size;
        u32 entry_size;
        u32 reserved;
        multiboot_mmap_entry entries[0];
    };

    /* Framebuffer info (type 8) */
    struct multiboot_tag_framebuffer {
        u32 type;
        u32 size;
        u64 addr;
        u32 pitch;
        u32 width;
        u32 height;
        u8  bpp;
        u8  fb_type;    /* must be 1 */
        u16 reserved;
        u8  red_field_pos;
        u8  red_mask_size;
        u8  green_field_pos;
        u8  green_mask_size;
        u8  blue_field_pos;
        u8  blue_mask_size;
    };

    /* EFI 64-bit image handle pointer (type 12) */
    struct multiboot_tag_efi64 {
        u32 type;
        u32 size;
        u64 pointer;
    };

    /* SMBIOS tables (type 13) */   
    struct multiboot_tag_smbios {
        u32 type;
        u32 size;
        u8  major;
        u8  minor;
        u8  reserved[6];
        u8  tables[0];
    };

    /* ACPI old RSDP (type 14) */
    struct multiboot_tag_old_acpi {
        u32 type;
        u32 size;
        u8  rsdp[0];
    };

    /* ACPI new RSDP (type 15) */
    struct multiboot_tag_new_acpi {
        u32 type;
        u32 size;
        u8  rsdp[0];
    };

    /* EFI 64-bit image handle pointer (type 20) */
    struct multiboot_tag_efi64_ih {
        u32 type;
        u32 size;
        u64 pointer;
    };


    /* EDID supported monitor resolutions (type 256) */
    struct multiboot_tag_edid {
        u32 type;
        u32 size;
        u8  edid[0];
    };

    /* SMP supported (type 257) */
    struct multiboot_tag_smp {
        u32 type;
        u32 size;
        u32 num_cores;
        u32 running_cores;
        u32 bspid;
    };

    /* Partition UUIDs (type 258) */
    struct multiboot_tag_partuuid {
        u32 type;
        u32 size;
        u8  partuuid[16];
        u8  rootuuid[16];
    };
}

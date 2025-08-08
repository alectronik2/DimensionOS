# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NewKern is a 64-bit x86 operating system kernel written in modern C++23 using C++ modules. It's designed as a microkernel with basic functionality including memory management, scheduling, interrupt handling, and hardware abstraction.

## Architecture

### Module System
The kernel uses C++23 modules exclusively - no traditional headers. The module structure is:

- `types` - Core type definitions (u8, u16, u32, u64, etc.)
- `arch.*` - Architecture-specific components (CPU, GDT, IDT, I/O, LAPIC, PS/2)
- `lib.*` - Utility libraries (print, string, spinlock)
- `mm.*` - Memory management (heap, physical frame allocator)
- `sched` - Task scheduler

### Directory Structure
- `src/` - All source code organized by component
- `src/arch/` - x86-64 architecture specific code
- `src/lib/` - Kernel utility libraries  
- `src/mm/` - Memory management subsystem
- `contrib/` - Third-party components (stb_sprintf, stdarg)
- `utilities/` - Build tools (dependency generator)

### Key Components
- **Simpleboot**: Custom bootloader interface compatible with Multiboot2
- **Memory Management**: Physical frame allocator + kernel heap allocator
- **Scheduler**: Cooperative task scheduler with yield-based switching
- **Interrupts**: IDT setup with LAPIC for SMP support
- **Hardware**: PS/2 keyboard support, basic I/O abstraction

## Build System

### Core Commands
```bash
# Build kernel
make

# Build and create disk image
make os.img

# Run in QEMU
make run

# Clean build artifacts
make clean
```

### Build Process
1. `ruby utilities/gen-depends.rb > modules.mk` - Generates module dependencies
2. C++ modules compiled with `-fmodules` flag to `.o` files in `obj/`
3. Assembly files (like `idt_asm.asm`) compiled with NASM
4. Linked with custom `src/linker.ld` for high-memory kernel layout
5. Debug symbols extracted to `kernel.dbg`
6. Simpleboot creates bootable image with kernel + debug symbols

### Compiler Flags
- `-std=c++23 -fmodules` - C++23 with modules support
- `-ffreestanding -nostdlib` - Kernel environment
- `-mno-red-zone -fno-stack-protector` - x86-64 kernel safety
- `-Os` - Optimize for size

## Development Workflow

### Adding New Modules
1. Create `.cc` file in appropriate `src/` subdirectory
2. Use `export module name;` syntax
3. Import required modules at top of file
4. Run `make` to regenerate dependencies automatically

### Memory Management
- Physical memory managed by `mm::pframe` allocator
- Kernel heap managed by `mm::heap` with `kmalloc()/kfree()`
- All addresses are higher-half (0xFFFFFFFF80000000+)

### Debugging
- Kernel includes debug symbols in separate `kernel.dbg` module
- Use QEMU monitor commands for debugging
- Serial output goes to stdio when running with `make run`

### Task Management
- Cooperative multitasking via `sched::yield()`
- Tasks created with `sched::create_task(function, stack, size)`
- No preemptive scheduling currently implemented

## Key Conventions

### Naming
- Module names use dot notation: `arch.cpu`, `mm.heap`
- Types use underscore suffix: `task_t`, `physaddr_t`
- Functions use snake_case
- No traditional C-style headers - modules only

### Error Handling
- Use `panic()` for unrecoverable kernel errors
- No exceptions (disabled with `-fno-exceptions`)

### Memory Layout
- Kernel loaded at 0xFFFFFFFF80000000 (higher half)
- Stack grows downward from high memory
- Heap managed by kernel allocator
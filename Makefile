CXX 		= g++
NASM 		= nasm
LD 			= ld
OBJCOPY 	= llvm-objcopy

# ===========================================================================================================

CXXFLAGS   += -std=c++23 -fmodules -Os -fno-exceptions -fno-rtti -ffreestanding -nostdlib -mno-red-zone \
	 -fno-stack-protector -fno-omit-frame-pointer -Icontrib -nostdinc -Wno-write-strings -g

QEMUFLAGS  += -m 256 -accel kvm -smp 2 -cpu host -serial stdio -machine q35

# ===========================================================================================================

include modules.mk

OBJECTS += obj/arch/idt_asm.o

obj/%.o: src/%.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

obj/%.o: src/%.asm
	$(NASM) -felf64 -o $@ $<

obj_dirs:
	mkdir -p obj/{arch,lib,mm}

all: clean_modules_mk modules.mk obj_dirs kernel.elf

clean_modules_mk: 
	rm -f modules.mk

modules.mk: 
	ruby utilities/gen-depends.rb > modules.mk

kernel.elf: $(OBJECTS)
	$(LD) -Tsrc/linker.ld -nostdlib -o $@ $(OBJECTS) 
	$(OBJCOPY) --only-keep-debug $@ kernel.dbg

os.img: kernel.elf
	mv kernel.elf disk_root/kernel.elf
	mv kernel.dbg disk_root/kernel.dbg
	simpleboot disk_root os.img

run: os.img
	qemu-system-x86_64 -hda os.img $(QEMUFLAGS)

objects/%.o: %.asm
	nasm -f elf64 -o $@ $<

clean:
	rm -rf $(OBJECTS) kernel.elf modules.mk
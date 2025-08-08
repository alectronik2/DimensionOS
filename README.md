# My little microkernel
This is a pretty new project which is still in its very early stages. It should once become an usable microkernel as an alternative to the existing big kernels like Linux, knowing that it will take a long time to achieve this goal, if ever. The architecture should be minimalistic yet complete to show how tiny a working kernel can get. Currently I am working on the task switching code, which will later be extended to user mode. There is a rudimentary memory manager (kmalloc, page mapping) already in place and interrupt handling works.

# How to build
You need to place the easyboot executable from contrib/easyboot into your path (or compile it yourself if you like). Then `make all` should build the kernel and `make run` starts a QEMU session. The requirements for build environment are pretty basic, you only need g++, NASM and QEMU.

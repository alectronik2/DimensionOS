export module arch.io;

import types;

constexpr unsigned short PORT = 0x3F8; // COM1 port

export namespace arch {
    void 
    outb( u16 port, u8 value ) {
        asm volatile( "outb %0, %1" : : "a"(value), "Nd"(port) );
    }

    u8 
    inb( u16 port ) {
        u8 ret;
        asm volatile( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
        return ret;
    }

    int 
    is_transmit_empty() {
        return inb( PORT + 5 ) & 0x20;
    }

    void 
    write_serial( char a ) {
        while( !is_transmit_empty() );
        outb( PORT, a );
    }
}



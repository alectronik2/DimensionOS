export module lib.print;

import types;
import arch.io;
import arch.cpu;

#define PRINTF_SUPPORT_DECIMAL_SPECIFIERS 0
#define PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS 0
#define PRINTF_SUPPORT_WRITEBACK_SPECIFIER 1
#define PRINTF_SUPPORT_MSVC_STYLE_INTEGER_SPECIFIERS 1
#define PRINTF_SUPPORT_LONG_LONG 1

#define PRINTF_ALIAS_STANDARD_FUNCTION_NAMES_SOFT 0
#define PRINTF_ALIAS_STANDARD_FUNCTION_NAMES_HARD 0

#define PRINTF_INTEGER_BUFFER_SIZE 32
#define PRINTF_DECIMAL_BUFFER_SIZE 32
#define PRINTF_DEFAULT_FLOAT_PRECISION 6
#define PRINTF_MAX_INTEGRAL_DIGITS_FOR_DECIMAL 9
#define PRINTF_LOG10_TAYLOR_TERMS 4
#define PRINTF_CHECK_FOR_NUL_IN_FORMAT_SPECIFIER 1

#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOFLOAT

#include <stb_sprintf.h>

export {
    void
    printk( const char *fmt, ... ) {
        va_list va;
        char buf[1024];

        va_start( va, fmt );
        stbsp_vsnprintf( buf, 1024, fmt, va );
        va_end( va );

        char *p = buf;
        while( *p )
            arch::write_serial( *p++ );
    }

    void
    panic( const char *msg ) {
        printk( "Kernel panic: %s\n", msg );
        arch::halt_cpu();
    }
}

export namespace fb {
    void
    spin() {
        
    }
}

struct debug_symbol {
    uint64_t address;
    char *name;
};

export namespace debug {
    void
    print_stacktrace( u64 rip, u64 rbp ) {
        printk( "Stack trace:\n" );
        while( rbp ) {
            u64 ret = *(u64 *)(rbp + 8);
            printk( "  RIP: 0x%0llx\n", rip );
            printk( "  RBP: 0x%0llx\n", rbp );
            rip = *(u64 *)rbp;
            rbp = *(u64 *)(rbp);
        }
        printk( "End of stack trace.\n" );
    }
}
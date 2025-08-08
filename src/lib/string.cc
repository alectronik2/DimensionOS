export module lib.string;

import types;

export void 
memset( void* _dst, int val, size_t len )
{
    u8 *dst = (u8 *)_dst;
    u64 *ldst;
    u64 lval = (val & 0xFF) * (-1ul / 255); //the multiplier becomes 0x0101... of the same length as long

    if (len >= 16) //optimize only if it's worth it (limit is a guess)
    {
        while( (size_t)dst & LONG_MASK )
        {
            *dst++ = val;
            len--;
        }
        ldst = (u64*)dst;
        while( len > sizeof(long) )
        {
             *ldst++ = lval;
             len -= sizeof( long );
        }
        dst = (u8*)ldst;
    }
    while (len--)
        *dst++ = val;
}

export void *
memcpy( void * dest, const void * src, size_t n ) {
        u8         *pdest  = static_cast<u8 *>(dest);
        const u8   *psrc   = static_cast<const u8 *>(src);
    
        for( size_t i = 0; i < n; i++ ) 
            pdest[i] = psrc[i];      
    
        return dest;
    }

export u64 
strlen( char *str )
{
    int i = 0;

    while (str[i] != '\0') {
        i++;
    }

    return i;
}

export i64 
strcmp( char *s1, char *s2 )
{
    i64 len1 = strlen(s1);
    i64 len2 = strlen(s2);

    i64 diff = len2 - len1;

    if (diff != 0)
        return diff;
    
    for (int i = 0; i < len1; i++) {
        if (s1[i] != s2[i])
            return s2[i] - s1[i];
    }

    return 0;
}

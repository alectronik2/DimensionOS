export module types;

export {
    using u8            = unsigned char;
    using u16           = unsigned short;
    using u32           = unsigned int;
    using u64           = unsigned long long;

    using i8            = signed char;
    using i16           = signed short;
    using i32           = signed int;
    using i64           = signed long long;

    using f32           = float;
    using f64           = double;
    using f128          = long double;    

    using uint8_t       = unsigned char;
    using uint16_t      = unsigned short;
    using uint32_t      = unsigned int;
    using uint64_t      = unsigned long;

    using int8_t        = signed char;
    using int16_t       = signed short;
    using int32_t       = signed int;
    using int64_t       = signed long;

    using size_t        = unsigned long;
    using physaddr_t    = unsigned long;
    using virtaddr_t    = unsigned long;
    using ptrdiff_t     = signed long;

    using ulong         = unsigned long;

    constexpr auto LONG_MASK = (sizeof (unsigned long) - 1);

    constexpr auto MAX_CPU = 64;
}


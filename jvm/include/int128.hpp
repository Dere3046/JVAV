#ifndef INT128_HPP
#define INT128_HPP

#include <cstdint>
#include <cstdio>
#include <string>

#if defined(__SIZEOF_INT128__) && !defined(INT128_FORCE_SOFT)
    #define INT128_HAS_NATIVE 1
#else
    #define INT128_HAS_NATIVE 0
#endif

struct Int128 {
#if INT128_HAS_NATIVE
    __int128 v;

    Int128() : v(0) {}
    Int128(int x) : v(x) {}
    Int128(long x) : v(x) {}
    Int128(long long x) : v(x) {}
    Int128(unsigned int x) : v(x) {}
    Int128(unsigned long x) : v(x) {}
    Int128(unsigned long long x) : v(x) {}
    Int128(__int128 x) : v(x) {}
    Int128(uint64_t low, int64_t high) {
        v = ((__int128)high << 64) | low;
    }

    uint64_t low_u64()  const { return (uint64_t)v; }
    int64_t  high_s64() const { return (int64_t)(v >> 64); }

    bool operator==(const Int128& o) const { return v == o.v; }
    bool operator!=(const Int128& o) const { return v != o.v; }
    bool operator<(const Int128& o) const { return v < o.v; }
    bool operator<=(const Int128& o) const { return v <= o.v; }
    bool operator>(const Int128& o) const { return v > o.v; }
    bool operator>=(const Int128& o) const { return v >= o.v; }

    Int128 operator-() const { return Int128(-v); }
    Int128 operator+(const Int128& o) const { return Int128(v + o.v); }
    Int128 operator-(const Int128& o) const { return Int128(v - o.v); }
    Int128 operator*(const Int128& o) const { return Int128(v * o.v); }
    Int128 operator/(const Int128& o) const { return Int128(v / o.v); }
    Int128 operator%(const Int128& o) const { return Int128(v % o.v); }

    Int128& operator+=(const Int128& o) { v += o.v; return *this; }
    Int128& operator-=(const Int128& o) { v -= o.v; return *this; }
    Int128& operator*=(const Int128& o) { v *= o.v; return *this; }
    Int128& operator/=(const Int128& o) { v /= o.v; return *this; }
    Int128& operator%=(const Int128& o) { v %= o.v; return *this; }

    Int128& operator++() { ++v; return *this; }
    Int128 operator++(int) { return Int128(v++); }
    Int128& operator--() { --v; return *this; }
    Int128 operator--(int) { return Int128(v--); }

    Int128 operator&(const Int128& o) const { return Int128(v & o.v); }
    Int128 operator|(const Int128& o) const { return Int128(v | o.v); }
    Int128 operator^(const Int128& o) const { return Int128(v ^ o.v); }
    Int128 operator~() const { return Int128(~v); }
    Int128& operator&=(const Int128& o) { v &= o.v; return *this; }
    Int128& operator|=(const Int128& o) { v |= o.v; return *this; }
    Int128& operator^=(const Int128& o) { v ^= o.v; return *this; }

    Int128 operator<<(int shift) const { return Int128(v << shift); }
    Int128 operator>>(int shift) const { return Int128(v >> shift); }

    explicit operator bool() const { return v != 0; }
    explicit operator int() const { return (int)v; }
    explicit operator long() const { return (long)v; }
    explicit operator long long() const { return (long long)v; }
    explicit operator unsigned int() const { return (unsigned int)v; }
    explicit operator unsigned long() const { return (unsigned long)v; }
    explicit operator unsigned long long() const { return (unsigned long long)v; }

    std::string toString() const {
        if (v == 0) return "0";
        bool neg = v < 0;
        __int128 t = neg ? -v : v;
        char buf[64]; int n = 0;
        while (t > 0) { buf[n++] = '0' + (char)(t % 10); t /= 10; }
        if (neg) buf[n++] = '-';
        std::string s; s.resize(n);
        for (int i = 0; i < n; i++) s[i] = buf[n - 1 - i];
        return s;
    }

    std::string toHexString() const {
        if (v == 0) return "0x0";
        bool neg = v < 0;
        __int128 t = neg ? -v : v;
        char buf[32]; int n = 0;
        while (t > 0) {
            int d = (int)(t % 16);
            buf[n++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
            t /= 16;
        }
        std::string s = neg ? "-0x" : "0x";
        for (int i = n - 1; i >= 0; i--) s += buf[i];
        return s;
    }

#else
    /* -------- software emulation for 32-bit targets -------- */
    uint64_t low;
    int64_t  high;

    Int128() : low(0), high(0) {}
    Int128(int x) : low((uint64_t)(int64_t)x), high(x < 0 ? -1 : 0) {}
    Int128(long x) : low((uint64_t)x), high(x < 0 ? -1 : 0) {}
    Int128(long long x) : low((uint64_t)x), high(x < 0 ? -1 : 0) {}
    Int128(unsigned int x) : low(x), high(0) {}
    Int128(unsigned long x) : low(x), high(0) {}
    Int128(unsigned long long x) : low(x), high(0) {}
    Int128(uint64_t l, int64_t h) : low(l), high(h) {}

    uint64_t low_u64()  const { return low; }
    int64_t  high_s64() const { return high; }

    bool operator==(const Int128& o) const {
        return high == o.high && low == o.low;
    }
    bool operator!=(const Int128& o) const { return !(*this == o); }
    bool operator<(const Int128& o) const {
        if (high != o.high) return high < o.high;
        return low < o.low;
    }
    bool operator<=(const Int128& o) const { return *this < o || *this == o; }
    bool operator>(const Int128& o) const { return o < *this; }
    bool operator>=(const Int128& o) const { return o <= *this; }

    Int128 operator-() const {
        uint64_t l = ~low + 1;
        int64_t h = ~high + (low == 0 ? 1 : 0);
        return Int128(l, h);
    }

    Int128 operator+(const Int128& o) const {
        uint64_t l = low + o.low;
        int64_t h = high + o.high + (l < low ? 1 : 0);
        return Int128(l, h);
    }
    Int128 operator-(const Int128& o) const {
        return *this + (-o);
    }
    Int128& operator+=(const Int128& o) {
        uint64_t old_low = low;
        low += o.low;
        high += o.high + (low < old_low ? 1 : 0);
        return *this;
    }
    Int128& operator-=(const Int128& o) {
        *this += (-o);
        return *this;
    }

    Int128& operator++() {
        low++;
        if (low == 0) high++;
        return *this;
    }
    Int128 operator++(int) {
        Int128 t = *this;
        ++(*this);
        return t;
    }
    Int128& operator--() {
        if (low == 0) high--;
        low--;
        return *this;
    }
    Int128 operator--(int) {
        Int128 t = *this;
        --(*this);
        return t;
    }

    Int128 operator&(const Int128& o) const {
        return Int128(low & o.low, high & o.high);
    }
    Int128 operator|(const Int128& o) const {
        return Int128(low | o.low, high | o.high);
    }
    Int128 operator^(const Int128& o) const {
        return Int128(low ^ o.low, high ^ o.high);
    }
    Int128 operator~() const {
        return Int128(~low, ~high);
    }
    Int128& operator&=(const Int128& o) {
        low &= o.low; high &= o.high; return *this;
    }
    Int128& operator|=(const Int128& o) {
        low |= o.low; high |= o.high; return *this;
    }
    Int128& operator^=(const Int128& o) {
        low ^= o.low; high ^= o.high; return *this;
    }

    Int128 operator<<(int shift) const {
        if (shift <= 0) return *this;
        if (shift >= 128) return Int128(0, 0);
        if (shift >= 64) {
            return Int128(0, (int64_t)(low << (shift - 64)));
        }
        return Int128(low << shift,
                      (int64_t)(((uint64_t)high << shift) | (low >> (64 - shift))));
    }
    Int128 operator>>(int shift) const {
        if (shift <= 0) return *this;
        if (shift >= 128) return (high < 0) ? Int128(~0ULL, ~0LL) : Int128(0, 0);
        if (shift >= 64) {
            return Int128((uint64_t)(high >> (shift - 64)), high >> 63);
        }
        return Int128((low >> shift) | ((uint64_t)high << (64 - shift)),
                      high >> shift);
    }

    /* multiplication */
    Int128 operator*(const Int128& o) const {
        // (a_lo + a_hi*2^64) * (b_lo + b_hi*2^64)
        // = a_lo*b_lo + (a_lo*b_hi + a_hi*b_lo)*2^64  (drop a_hi*b_hi*2^128)
        uint64_t a_lo = low, b_lo = o.low;
        uint64_t a_hi = (uint64_t)high, b_hi = (uint64_t)o.high;

        // Split into 32-bit limbs
        uint64_t a0 = a_lo & 0xFFFFFFFFULL;
        uint64_t a1 = a_lo >> 32;
        uint64_t a2 = a_hi & 0xFFFFFFFFULL;
        uint64_t a3 = a_hi >> 32;
        uint64_t b0 = b_lo & 0xFFFFFFFFULL;
        uint64_t b1 = b_lo >> 32;
        uint64_t b2 = b_hi & 0xFFFFFFFFULL;
        uint64_t b3 = b_hi >> 32;

        uint64_t p0 = a0 * b0;
        uint64_t p1 = a0 * b1;
        uint64_t p2 = a1 * b0;
        uint64_t p3 = a0 * b2;
        uint64_t p4 = a1 * b1;
        uint64_t p5 = a2 * b0;
        uint64_t p6 = a1 * b2;
        uint64_t p7 = a2 * b1;
        uint64_t p8 = a3 * b0;
        uint64_t p9 = a0 * b3;

        uint64_t r0 = p0;
        uint64_t r1 = (p0 >> 32) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL);
        uint64_t r2 = (r1 >> 32) + (p1 >> 32) + (p2 >> 32) + (p3 & 0xFFFFFFFFULL)
                     + (p4 & 0xFFFFFFFFULL) + (p5 & 0xFFFFFFFFULL);
        uint64_t r3 = (r2 >> 32) + (p3 >> 32) + (p4 >> 32) + (p5 >> 32)
                     + (p6 & 0xFFFFFFFFULL) + (p7 & 0xFFFFFFFFULL) + (p8 & 0xFFFFFFFFULL)
                     + (p9 & 0xFFFFFFFFULL);

        uint64_t low64  = (r0 & 0xFFFFFFFFULL) | ((r1 & 0xFFFFFFFFULL) << 32);
        uint64_t high64 = (r2 & 0xFFFFFFFFULL) | ((r3 & 0xFFFFFFFFULL) << 32);
        return Int128(low64, (int64_t)high64);
    }

    Int128& operator*=(const Int128& o) {
        *this = *this * o;
        return *this;
    }

    /* division */
    Int128 operator/(const Int128& o) const {
        bool neg = false;
        Int128 a = *this, b = o;
        if (a < 0) { a = -a; neg = !neg; }
        if (b < 0) { b = -b; neg = !neg; }
        if (b == 0) return Int128(0); // div-by-zero returns 0

        if (b.high == 0) {
            // divisor fits in 64 bits
            uint64_t d = b.low;
            if (a.high == 0) {
                Int128 q(a.low / d, 0);
                return neg ? -q : q;
            }
            uint64_t q_high = (uint64_t)a.high / d;
            uint64_t r_high = (uint64_t)a.high % d;
            uint64_t q_low = 0;
            uint64_t rem = r_high;
            for (int i = 63; i >= 0; i--) {
                int msb = (int)((rem >> 63) & 1);
                rem = (rem << 1) | ((a.low >> i) & 1);
                if (msb || rem >= d) {
                    rem -= d;
                    q_low |= (1ULL << i);
                }
            }
            Int128 q(q_low, (int64_t)q_high);
            return neg ? -q : q;
        }

        // general 128/128 division – binary long division
        Int128 quot(0, 0), rem(0, 0);
        for (int i = 127; i >= 0; i--) {
            int rem_msb = (rem.high < 0) ? 1 : 0;
            rem = rem << 1;
            int bit = (i >= 64) ? (int)(((uint64_t)a.high >> (i - 64)) & 1)
                                : (int)((a.low >> i) & 1);
            if (bit) rem.low |= 1;
            if (rem_msb || rem >= b) {
                rem = rem - b;
                if (i >= 64) quot.high |= (1ULL << (i - 64));
                else quot.low |= (1ULL << i);
            }
        }
        return neg ? -quot : quot;
    }

    Int128& operator/=(const Int128& o) {
        *this = *this / o;
        return *this;
    }

    /* modulo */
    Int128 operator%(const Int128& o) const {
        bool neg = false;
        Int128 a = *this, b = o;
        if (a < 0) { a = -a; neg = true; }
        if (b < 0) { b = -b; }
        if (b == 0) return Int128(0);

        if (b.high == 0) {
            uint64_t d = b.low;
            if (a.high == 0) {
                Int128 r(a.low % d, 0);
                return neg ? -r : r;
            }
            uint64_t r_high = (uint64_t)a.high % d;
            uint64_t rem = r_high;
            for (int i = 63; i >= 0; i--) {
                int msb = (int)((rem >> 63) & 1);
                rem = (rem << 1) | ((a.low >> i) & 1);
                if (msb || rem >= d) rem -= d;
            }
            Int128 r(rem, 0);
            return neg ? -r : r;
        }

        Int128 rem(0, 0);
        for (int i = 127; i >= 0; i--) {
            int rem_msb = (rem.high < 0) ? 1 : 0;
            rem = rem << 1;
            int bit = (i >= 64) ? (int)(((uint64_t)a.high >> (i - 64)) & 1)
                                : (int)((a.low >> i) & 1);
            if (bit) rem.low |= 1;
            if (rem_msb || rem >= b) rem = rem - b;
        }
        return neg ? -rem : rem;
    }

    Int128& operator%=(const Int128& o) {
        *this = *this % o;
        return *this;
    }

    explicit operator bool() const { return low != 0 || high != 0; }
    explicit operator int() const { return (int)(int64_t)low; }
    explicit operator long() const { return (long)(int64_t)low; }
    explicit operator long long() const { return (long long)low; }
    explicit operator unsigned int() const { return (unsigned int)low; }
    explicit operator unsigned long() const { return (unsigned long)low; }
    explicit operator unsigned long long() const { return low; }

    std::string toString() const {
        if (low == 0 && high == 0) return "0";
        bool neg = high < 0;
        Int128 t = neg ? -(*this) : *this;
        char buf[64]; int n = 0;
        while (t > Int128(0)) {
            Int128 d = t / Int128(10);
            Int128 m = t % Int128(10);
            buf[n++] = '0' + (char)(int)(long long)m;
            t = d;
        }
        if (neg) buf[n++] = '-';
        std::string s; s.resize(n);
        for (int i = 0; i < n; i++) s[i] = buf[n - 1 - i];
        return s;
    }

    std::string toHexString() const {
        if (low == 0 && high == 0) return "0x0";
        bool neg = high < 0;
        Int128 t = neg ? -(*this) : *this;
        char buf[32]; int n = 0;
        while (t > Int128(0)) {
            Int128 d = t / Int128(16);
            Int128 m = t % Int128(16);
            int digit = (int)(long long)m;
            buf[n++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
            t = d;
        }
        std::string s = neg ? "-0x" : "0x";
        for (int i = n - 1; i >= 0; i--) s += buf[i];
        return s;
    }
#endif
};

#endif // INT128_HPP

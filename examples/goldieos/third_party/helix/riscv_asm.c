#include <stdint.h>

#ifdef PLATFORM_TYPE_WS63
// 快速绝对值函数
int32_t xmp3_FASTABS(int32_t x) {
    int32_t mask;
    asm volatile (
        "srai %0, %1, 31\n\t"   // mask = x >> 31 (算术右移，得到符号位扩展)
        "xor %0, %1, %0\n\t"    // result = x ^ mask
        "sub %0, %0, %0"        // result = result - mask
        : "=r" (mask)
        : "r" (x)
    );
    return mask;
}

// 32位乘法并移位 (通常用于定点数运算)
int32_t xmp3_MULSHIFT32(int32_t a, int32_t b) {
    int32_t ret;
    
    asm volatile (
        "mulh %0, %1, %2\n\t"   // 获取64位乘法结果的高32位
        : "=r" (ret)
        : "r" (a), "r" (b)
    );
    
    return ret;
}

// 带移位的32位乘法 (更通用的版本)
int32_t xmp3_MULSHIFT32_shift(int32_t a, int32_t b, int shift) {
    int64_t result = (int64_t)a * (int64_t)b;
    return (int32_t)(result >> shift);
}

// 64位乘加操作: result = a + (b * c)
int64_t MADD64(int64_t a, int32_t b, int32_t c) {
    int64_t result;
    int64_t product = (int64_t)b * (int64_t)c;
    result = a + product;
    return result;
}

// 64位算术右移
int64_t SAR64(int64_t value, int shift) {
    if (shift >= 64) {
        // 如果移位超过64位，返回符号扩展
        return (value < 0) ? -1 : 0;
    } else if (shift >= 32) {
        // 如果移位超过32位但小于64位
        int32_t hi = (int32_t)(value >> 32);
        int32_t shifted = hi >> (shift - 32);
        return (int64_t)shifted;
    } else if (shift > 0) {
        // 正常移位
        uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);
        int32_t hi = (int32_t)(value >> 32);
        
        uint32_t new_lo = (lo >> shift) | ((uint32_t)hi << (32 - shift));
        int32_t new_hi = hi >> shift;
        
        return ((int64_t)new_hi << 32) | new_lo;
    } else {
        // 不移位
        return value;
    }
}
#else

int32_t xmp3_FASTABS(int x) 
{
	//int sign;
	int32_t sign = x >> 31;
	//sign = x >> (sizeof(int) * 8 - 1);
	x ^= sign;
	x -= sign;

	return x;
}

int32_t xmp3_MULSHIFT32(int32_t a, int32_t b) {
       int64_t product = (int64_t)a * (int64_t)b;
    return (int32_t)(product >> 32);
}

int64_t MADD64(int64_t sum, int32_t x, int32_t y)
{
    int64_t product = (int64_t)x * (int64_t)y;
    return sum + product;
}


#if 0
int64_t SAR64(int64_t x, int n)
{
    uint32_t xLo = (uint32_t)(x & 0xFFFFFFFF);
    int32_t xHi = (int32_t)(x >> 32);
    
    if (n == 0) {
        return x;
    }
    else if (n < 32) {
        uint32_t newLo = (xLo >> n) | ((uint32_t)xHi << (32 - n));
        int32_t newHi = xHi >> n;
        return ((int64_t)newHi << 32) | newLo;
    }
    else if (n < 64) {
        int32_t shift = n - 32;
        uint32_t newLo = (uint32_t)(xHi >> shift);
        int32_t newHi = xHi >> 31;
        return ((int64_t)newHi << 32) | newLo;
    }
    else {
	return (int64_t)(xHi >> 31);
    }
}
#else
int64_t SAR64(int64_t x, int n)
{
	if (n <= 0) return x;
	   if (n >= 64) {
		   return (x < 0) ? -1 : 0;
	   }
	   
	   return x >> n;
}

#endif
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bfile.h"

uint16_t swap16(uint16_t x)
{
    return ((x & 0xFF00)>>8) | ((x & 0x00FF)<<8);
}

uint32_t swap32(uint32_t x)
{
    return ((x & 0xFF000000)>>24) | ((x & 0x00FF0000)>>8) |
    ((x & 0x0000FF00)<<8) | ((x & 0x000000FF)<<24);
}


void read_uint8(const uint8_t *bytes, int big_endian, void *out)
{
    *(uint8_t*)out = bytes[0];
}

void read_uint16(const uint8_t *bytes, int big_endian, void *out)
{
    uint16_t val;
    memcpy(&val,bytes,2);
    if (big_endian==ENDIAN_BIG)
    {
        val = swap16 (val);

    }
    *(uint16_t*)out = val;
}

void read_uint32(const uint8_t *bytes, int big_endian, void *out)
{
    uint32_t val;
    memcpy(&val,bytes,4);
    if (big_endian==ENDIAN_BIG)
    {
        val = swap32 (val);
    }
    *(uint32_t*)out = val;
}

void read_int8(const uint8_t *bytes, int big_endian, void *out)
{
    *(int8_t *)out = (int8_t)bytes[0];
}

void read_int16(const uint8_t *bytes, int big_endian, void *out)
{
    uint16_t val;
    memcpy(&val,bytes,2);
    if (big_endian == ENDIAN_BIG)
    {
        val = swap16 (val);
    }
    *(int16_t*)out = (int16_t) val;
}

void read_int32(const uint8_t *bytes, int big_endian, void *out)
{
    uint32_t val;
    memcpy(&val,bytes,4);
    if (big_endian == ENDIAN_BIG)
    {
        val = swap32 (val);
    }
    *(int32_t*)out = (int32_t) val;
}

type_info_t type_table[TYPE_COUNT] = {
    [TYPE_UINT8]  = {"uint8_t",  1, read_uint8},
    [TYPE_UINT16] = {"uint16_t", 2, read_uint16},
    [TYPE_UINT32] = {"uint32_t", 4, read_uint32},
    [TYPE_UINT64] = {"uint64_t", 8, NULL},       // 暂不实现
    [TYPE_INT8]   = {"int8_t",   1, read_int8},
    [TYPE_INT16]  = {"int16_t",  2, read_int16},
    [TYPE_INT32]  = {"int32_t",  4, read_int32},
    [TYPE_INT64]  = {"int64_t",  8, NULL},       // 暂不实现
    [TYPE_FLOAT]  = {"float",    4, NULL},       // 暂不实现
    [TYPE_DOUBLE] = {"double",   8, NULL},       // 暂不实现
    [TYPE_CHAR]   = {"char",     1, NULL},       // 特殊处理，读函数不走这里
};


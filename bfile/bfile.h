#ifndef BFILE_H
#define BFILE_H

#include <stdint.h>

#define MAX_FIELDS 32

typedef enum{
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT32,
    TYPE_UINT64,

    TYPE_INT8,
    TYPE_INT16,
    TYPE_INT32,
    TYPE_INT64,

    TYPE_FLOAT,
    TYPE_DOUBLE,

    TYPE_CHAR,
    
    TYPE_COUNT,
}field_type_t;

typedef enum{
    ENDIAN_LITTLE,
    ENDIAN_BIG,
}endian_t;

typedef void (*read_func_t)(const uint8_t *bytes, int big_endian, void *out);

typedef struct 
{
    const char *name;
    int size;
    read_func_t reader;
}type_info_t;


typedef struct 
{
    field_type_t type;
    char name[64];
    int array_size;
}field_def_t;

extern type_info_t type_table[TYPE_COUNT];

int parse_fields(char *fmt, field_def_t *fields, int max_fields);

#endif

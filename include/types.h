/* =============================================================================
 * SENG21213-OS :: Primitive types
 * File   : include/types.h
 * Purpose: Freestanding C environment does not provide stdint.h from glibc.
 *          Define our own integer types here.
 * ============================================================================*/
#ifndef TYPES_H
#define TYPES_H

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

typedef uint32_t            size_t;
typedef int32_t             ssize_t;
typedef uint8_t             bool;

#define true  1
#define false 0
#define NULL  ((void*)0)

#endif /* TYPES_H */

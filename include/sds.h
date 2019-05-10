#ifndef SDS_H_INCLUDED
#define SDS_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include "config.h"

// You can define customized sds allocator in place of defualt libc allocator
#define s_malloc  malloc
#define s_realloc realloc
#define s_free    free

#define SDS_MAX_PREALLOC (1024*1024)        // affect how much space to prealloc in sds_make_room_for
const char *SDS_NOINIT;

typedef char *sds;

// for strings with lenth 0 ~ 64
typedef struct __attribute__ ((__packed__)) sds_hdr_6
{
    unsigned char flag;     // 2 lsb for type, and 5 msb for length
    char buf[];
} sds_hdr_6;
// for strings with lenth  65 ~ 256
typedef struct __attribute__ ((__packed__)) sds_hdr_8
{
    uint8_t len;            // buf used
    uint8_t alloc;          // buf length, excluding null terminator
    unsigned char flag;
    char buf[];
} sds_hdr_8;
// for strings with length 257 ~ 65536 (max possible lenth)
typedef struct __attribute__ ((__packed__)) sds_hdr_16
{
    uint16_t len;           // buf used
    uint16_t alloc;         // buf length, excluding null terminator
    unsigned char flag;
    char buf[];
} sds_hdr_16;


#define SDS_TYPE_6  0
#define SDS_TYPE_8  1
#define SDS_TYPE_16 2

#define SDS_TYPE_MASK    3
#define SDS_TYPE_BITS    2

#define SDS_HDR(T, s)       ((sds_hdr_##T *)((s) - (sizeof(sds_hdr_##T))))
#define SDS_HDR_VAR(T, s)   sds_hdr_##T *sh = (sds_hdr_##T *)((s) - (sizeof(sds_hdr_##T)));

// Inline functions declarations
// Return the length of the sds string
static inline size_t sds_len(const sds s)
{
    unsigned char flag = s[-1];
    switch(flag & SDS_TYPE_MASK)
    {
        case SDS_TYPE_6:
            return flag >> SDS_TYPE_BITS;
        case SDS_TYPE_8:
            return SDS_HDR(8, s)->len;
        case SDS_TYPE_16:
            return SDS_HDR(16, s)->len;
    }
    return 0;
}

// Return the available space in the sds string
static inline size_t sds_avail(const sds s)
{
    unsigned char flag = s[-1];
    switch(flag & SDS_TYPE_MASK)
    {
        case SDS_TYPE_6:
            return 0;
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8, s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16, s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

// Increate the 'len' field of sds string 's' by 'incr'.
static inline void sds_incr_len(sds s, size_t incr)
{
    unsigned char type = s[-1] & SDS_TYPE_MASK;
    size_t len;
    // 's' cannot be SDS_TYPE_6 because before sds_incr_len, the 's' must haved changed to SDS_TYPE_8
    assert(type != SDS_TYPE_6);

    switch(type)
    {
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8, s);
            assert((incr > 0 && sh->alloc - sh->len >= incr));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16, s);
            assert((incr > 0 && sh->alloc - sh->len >= incr));
            len = (sh->len += incr);
            break;
        }
    }
    s[len] = '\0';
}


// function declarations
sds sds_new_len(const void *init, size_t init_len);
sds sds_new(const char *init);
sds sds_new_empty();
sds sds_dup(const sds s);
void sds_free(sds s);
sds sds_make_room_for(sds s, size_t add_len);
sds sds_cat_len(sds s, const void *t, size_t len);
sds sds_cat(sds s, const char *t);
sds sds_cat_vprintf(sds s, const char *fmt, va_list ap);
sds sds_cat_printf(sds s, const char *fmt, ...);
sds sds_copy_len(sds s, const char *t, size_t len);
sds sds_copy(sds s, const char *t);
sds sds_from_longlong(long long val);
void sds_trim(sds s, const char *cset);
void sds_range(sds s, ssize_t start ,ssize_t end);
void sds_tolower(sds s);
void sds_toupper(sds s);
int sds_cmp(const sds s1, const sds s2);


#endif // SDS_H_INCLUDED

/*
    SDS, a dynamic string lib.
    4-24, sds_test_main() added
    4-25, sds_req_type, sds_new_len
*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "config.h"
#include "sds.h"
#include "util.h"
#include "debug.h"



const char *SDS_NOINIT = "SDS_NOINIT";

// Return the sds header size according to the header type
static inline size_t sds_hdr_size(char type)
{
    switch(type & SDS_TYPE_MASK)
    {
        case SDS_TYPE_6:
            return sizeof(sds_hdr_6);
        case SDS_TYPE_8:
            return sizeof(sds_hdr_8);
        case SDS_TYPE_16:
            return sizeof(sds_hdr_16);
    }
    return 0; // just suppress warning.
}

// Return proper sds header flag, SDS_TYPE_6, SDS_TYPE_8, or
// SDS_TYPE_16, based on the string size
static inline char sds_req_type(size_t str_size)
{
    if (str_size <= (1 << 6))
        return SDS_TYPE_6;
    if (str_size <= (1 << 8))
        return SDS_TYPE_8;
    return SDS_TYPE_16;
}

//  Set the length of the sds string
static inline void sds_set_len(sds s, size_t new_len)
{
    unsigned char flag = s[-1];
    switch(flag & SDS_TYPE_MASK)
    {
        case SDS_TYPE_6: {
            unsigned char *fp = (unsigned char*)s - 1;
            *fp = new_len << SDS_TYPE_BITS | SDS_TYPE_6;
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR(8, s)->len = new_len;
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR(16, s)->len = new_len;
            break;
        }
    }
}

// Return the alloc field of the sds string
static inline size_t sds_get_alloc(sds s)
{
    unsigned char flag = s[-1];
    switch(flag & SDS_TYPE_MASK)
    {
        case SDS_TYPE_6:
            return flag >> SDS_TYPE_BITS; // The alloc for type 6 is its length, not 1 << 6 !!
        case SDS_TYPE_8: {
            return SDS_HDR(8, s)->alloc;
        }
        case SDS_TYPE_16: {
            return SDS_HDR(16, s)->alloc;
        }
    }
    return 0;
}

//  Set the alloc of the sds string
static inline void sds_set_alloc(sds s, size_t new_alloc)
{
    unsigned char flag = s[-1];
    switch(flag & SDS_TYPE_MASK)
    {
        case SDS_TYPE_6: // No alloc field, nothing to do
            break;
        case SDS_TYPE_8: {
            SDS_HDR(8, s)->alloc = new_alloc;
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR(16, s)->alloc = new_alloc;
            break;
        }
    }
}

// Return the total size of the allocation of the sds string 's'
size_t sds_get_total_alloc(const sds s)
{
    return sds_hdr_size(s[-1]) + sds_get_alloc(s) + 1;
}

//  Create a new sds string with specified content and length.
//  If init is NULL, the string is initailized with zero.
//  If init is SDS_NOINIT, the string is left uninitailized.
sds sds_new_len(const void *init, size_t init_len)
{
    char type = sds_req_type(init_len);
    int hdr_len = sds_hdr_size(type);

    void *sh = s_malloc(hdr_len + init_len + 1);
    if (sh == NULL) return NULL;

    sds s = (char*)sh + hdr_len;
    // init sds header
    switch(type)
    {
        case SDS_TYPE_6: {      // must add brackets for case, to suppress error
            SDS_HDR_VAR(6, s)
            sh->flag = (init_len << SDS_TYPE_BITS) | type;
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8, s);
            sh->len = init_len;
            sh->alloc = init_len;
            sh->flag = type;
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16, s);
            sh->len = init_len;
            sh->alloc = init_len;
            sh->flag = type;
            break;
        }
    }
    // init sds buf
    if (!init) {
        memset(s, 0, init_len + 1);
    } else if (init != SDS_NOINIT) {
        memcpy(s, init, init_len);
    }
    s[init_len] = '\0';
    return s;
}

// Create a new sds string from a null terminated C string
sds sds_new(const char *init)
{
    size_t init_len = (init == NULL) ? 0 : strlen(init);
    return sds_new_len(init, init_len);
}

// Create a new empty sds string.
sds sds_new_empty()
{
    return sds_new_len("", 0);
}
//  Duplicate an sds string
sds sds_dup(const sds s)
{
    return sds_new_len(s, sds_len(s));
}


// Free an sds string. No operation if 's' is NULL.
void sds_free(sds s)
{
    if (s == NULL) return;
    s_free((char*)s - sds_hdr_size(s[-1]));
}

sds sds_make_room_for(sds s, size_t add_len)
{
    if (sds_avail(s) >= add_len) return s;

    char new_type, old_type = s[-1] & SDS_TYPE_MASK;
    size_t new_len, old_len = sds_len(s);
    void *new_sh, *old_sh = (char*)s - sds_hdr_size(old_type);

    // determine the appropriate new length and associate type
    new_len = old_len + add_len;
    if (new_len < SDS_MAX_PREALLOC) {
        new_len *= 2;
    } else {
        new_len += SDS_MAX_PREALLOC;
    }
    new_type = sds_req_type(new_len);
    if (new_type == SDS_TYPE_6) {
        new_type = SDS_TYPE_8;
    }

    // realloc if new_type remains the same type
    size_t hdr_len = sds_hdr_size(new_type);

    if (new_type == old_type) {
        server_assert(new_type != SDS_TYPE_6);
        new_sh = s_realloc(old_sh, hdr_len + new_len + 1);
        if (new_sh == NULL) return NULL;

        s = (char*)new_sh + hdr_len;
        sds_set_alloc(s, new_len);

    } else { // or else malloc a new sds of a different type, and free the old one
        new_sh = s_malloc(hdr_len + new_len + 1);
        if (new_sh == NULL) return NULL;
        memcpy((char*)new_sh + hdr_len, s, old_len + 1); // copy buf
        s_free(old_sh);

        // set len, alloc and flag for the new sds string
        s = (char*)new_sh + hdr_len;
        s[-1] = new_type;   // must first set type, then len and alloc (THIS BUG TOOK ME 2 HOURS)
        sds_set_len(s, old_len);
        sds_set_alloc(s, new_len);
    }
    return s;
}

// Append the specified binary-safe string pointed by 't' of 'len' bytes to the
// end of the sds string 's'.
sds sds_cat_len(sds s, const void *t, size_t len)
{
    s = sds_make_room_for(s, len);  // first make enough space for 'len' bytes
    if (s == NULL) return NULL;

    size_t cur_len = sds_len(s);
    memcpy(s + cur_len, t, len);    // then copy 'len' bytes from 't' to 's'
    sds_set_len(s, cur_len + len);  // update lengh field of 's'
    s[cur_len + len] = '\0';

    return s;
}

// Append the null terminated C string 't' to the end of the sds string 's'
sds sds_cat(sds s, const char *t)
{
    return sds_cat_len(s, t, strlen(t));
}

// Append the specified sds string 't' to the end of the sds string 's'
sds sds_cat_sds(sds s, const sds t)
{
    return sds_cat_len(s, t, sds_len(t));
}

// Like sds_cat_printf but gets a va_list instead of being variadic
sds sds_cat_vprintf(sds s, const char *fmt, va_list ap)
{
    char static_buf[1024], *buf = static_buf;
    size_t buf_len = strlen(fmt) * 2;

    // Try to use static_buf for speed. If static_buf cannot hold, we resort to heap allocation
    if (buf_len > sizeof(static_buf)) {
        buf = s_malloc(buf_len);
        if (buf == NULL) return NULL;
    } else {
        buf_len = sizeof(static_buf);
    }

    va_list cpy;

    va_copy(cpy, ap);
    vsnprintf(buf, buf_len, fmt, cpy);
    va_end(cpy);

    while (1) {
        buf[buf_len - 2] = '\0';
        // format and print

        va_copy(cpy, ap);
        vsnprintf(buf, buf_len, fmt, cpy);
        va_end(cpy);
        // If '\0' at buf[buf_len -2] is changed ,that means the buf is still not big.
        // We double the size of the current buf and retry.
        if (buf[buf_len - 2] != '\0')
        {
            if (buf != static_buf) s_free(buf);
            buf_len *= 2;
            buf = s_malloc(buf_len);
            if (buf == NULL) return NULL;
            continue;
        }
        break;
    }

    s = sds_cat(s, buf);
    if (buf != static_buf) s_free(buf);
    return s;
}

// Append to the sds string 's' with a string obtained from a printf-like format
sds sds_cat_printf(sds s, const char *fmt, ...)
{
    sds t;

    va_list ap;
    va_start(ap, fmt);
    t = sds_cat_vprintf(s, fmt, ap);
    va_end(ap);

    return t;
}

// Destructively modify the sds string 's' to hold the binary-safe
// string pointed by 't' of 'len' bytes.
sds sds_copy_len(sds s, const char* t, size_t len)
{
    // make room if the 'alloc' is not enough for copy 'len' bytes
    if (sds_get_alloc(s) < len) {
        s = sds_make_room_for(s, len - sds_len(s));
        if (s == NULL) return NULL;
    }

    memcpy(s, t, len);
    s[len] = '\0';
    sds_set_len(s, len);

    return s;
}

// Destructively modify the sds string 's' to hold the
// null terminated string pointed by 't'
sds sds_copy(sds s, const char* t)
{
    return sds_copy_len(s, t, strlen(t));
}


// Construct an sds string from 'val' of type long long
sds sds_from_longlong(long long val)
{
    char buf[LEN_LL_TO_STR];
    size_t len = util_convert_ll_to_str(buf, val);
    return sds_new_len(buf, len);;

}

// Remove characters from both sides of the sds string 's' which are composed of
// just contiguous characters found in 'cset'.
void sds_trim(sds s, const char* cset)
{
    char *start = s;
    char *end = s + sds_len(s) - 1;

    char *sp = start, *ep = end;
    while( sp <= end && strchr(cset, *sp)) sp ++;
    while( ep >= sp && strchr(cset, *ep)) ep --;

    size_t len = (sp > ep) ? 0 : ((ep - sp) + 1);

    if (sp != start) memmove(start, sp, len);
    s[len] = '\0';
    sds_set_len(s, len);
}

// Turn the string into a smaller substring specified by the 'start' and 'end' indexes.
void sds_range(sds s, ssize_t start, ssize_t end) // Note use ssize_t here, BUG using size_t
{
    size_t new_len, old_len = sds_len(s);

    if (old_len == 0) return;
    // adjust start
    if (start < 0) {
        start = old_len + start;
        if (start < 0) start = 0;
    } else if (start > old_len) {
        s[0] = '\0';
        sds_set_len(s, 0);
        return;
    }
    // adjust end
    if (end < 0) {
        end = old_len + end;
        if (end < 0) {
            s[0] = '\0';
            sds_set_len(s, 0);
            return;
        }
    } else if (end > old_len) {
        end = old_len - 1;
    }
    // now both are in range [0, old_len]
    if (start > end) {
        s[0] = '\0';
        sds_set_len(s, 0);
        return;
    }
    new_len = end - start + 1;
    if (start) memmove(s, s + start, new_len);
    s[new_len] = '\0';
    sds_set_len(s, new_len);
}

//  Apply tolower() to every character of the sds string 's'.
void sds_tolower(sds s)
{
    size_t len = sds_len(s), i;
    for(i = 0; i < len; i ++) s[i] = tolower(s[i]);
}

//  Apply toupper() to every character of the sds string 's'.
void sds_toupper(sds s)
{
    size_t len = sds_len(s), i;
    for(i = 0; i < len; i ++) s[i] = toupper(s[i]);
}

//  Compare two sds strigns with memcmp().
//  Return positive if s1 > s2, negative if s1 < s2, or 0 if exactly the same.
int sds_cmp(const sds s1, const sds s2)
{
    size_t l1 = sds_len(s1), l2 = sds_len(s2);
    size_t min_len = (l1 < l2) ? l1 : l2;
    int cmp = memcmp(s1, s2, min_len);

    if (cmp == 0) return (l1 > l2) ? 1 : ((l1 < l2) ? -1 : 0);
    else return cmp;
}

// Print all debug info for sds string 's'.
void sds_debug_print(const sds s, int debug_content)
{
    char *types[] = {"SDS_TYPE_6 ", "SDS_TYPE_8 ", "SDS_TYPE_16"};
    char *type = types[s[-1] & SDS_TYPE_MASK];
    printf("type:%s  len:%4lu  free:%4lu  alloc:%4lu  total_allc:%4lu  buf[]:%s \n",
        type, sds_len(s), sds_avail(s), sds_get_alloc(s), sds_get_total_alloc(s), (debug_content ? s :  "..."));
}

/*
    Miscellaneous utility functions go here. 5.1
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "client.h"
#include "debug.h"
#include "util.h"
#include "log.h"


// Get random bytes of length 'len' at 'p'.
void util_get_random_bytes(unsigned char *p, size_t len)
{
    //  'buf' that holds random bytes. Its size is mutiple of 4 that is just bigger than 'len'
    size_t buf_len = (len + 3) >> 2 << 2;
    unsigned char buf[buf_len];

    FILE *fp = fopen("/dev/urandom", "r");
    if (fp != NULL) {
        size_t num_read = fread(&buf, 1, len, fp);
        if (num_read == len) {
            memcpy(p, buf, len);
            fclose(fp);
            return;    // OK
        }
    }

    for (size_t i = 0; i < buf_len; i += 4)
    {
        buf[i] = (uint32_t)random();
    }

    memcpy(p, buf, len);
}

// Get random char of lengh 'len' at 'p'.
void util_get_random_hex_chars(char *p, size_t len)
{
    char *char_set = "0123456789abcdef";

    util_get_random_bytes((unsigned char*)p, len);
    for (size_t i = 0; i < len; i ++)
        p[i] = char_set[p[i] & 0x0F];
}

// Return time elasped in millisecond since Unix 1970.1.1 0:0:0
long long util_get_time_in_millisecond()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// Convert a signed long long value to string at 'buf'. Returned int is its length.
// Note that the size of 'buf' must >= LEN_LL_TO_STR
int util_convert_ll_to_str(char *buf, long long val)
{
    unsigned long long v = (val > 0) ? val : -val;
    size_t len = 0;
    char *p = buf, aux;

    // Generate string representation in reverse order.
    do {
        *p ++ = '0' + (v % 10);
        v /= 10;
    } while (v);
    if (val < 0) *p ++ = '-';

    // Compute length and add null term
    len = p - buf;
    *p = '\0';

    // Reverse the string
    p --;
    while (buf < p) {
        aux = *buf;
        *buf = *p;
        *p = aux;
        buf ++;
        p --;
    }

    return len;
}






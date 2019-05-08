#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <stddef.h>
#include <unistd.h>

#include "client.h"

// math
void util_get_random_bytes(unsigned char *p, size_t len);
void util_get_random_hex_chars(char *p, size_t len);

// time
long long util_get_time_in_millisecond();

// conversion
#define LEN_LL_TO_STR 21
int util_convert_ll_to_str(char *buf, long long val);

#endif // UTIL_H_INCLUDED

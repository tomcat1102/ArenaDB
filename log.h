#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <syslog.h>
// log levels
#define LL_DEBUG    0
#define LL_VERBOSE  1
#define LL_NOTICE   2
#define LL_WARNING  3
#define LL_ERROR    4

#define LL_RAW      8
#define LL_MASK  0x07

// log font, colors, etc. See 'man console_codes'

// style code prefix
#define LPREFIX "\e["
// font style codes
#define LF_DEF          "0"
#define LF_BOLD         "1"
#define LF_HALF_BRIGHT  "2"
#define LF_UNDERSCORE   "4"
#define LF_NORM_INTENS  "22"
// foreground color codes
#define LC_BLACK        ";30"
#define LC_RED          ";31"
#define LC_GREEN        ";32"
#define LC_YELLOW       ";33"
#define LC_BLUE         ";34"
#define LC_MAGENTA      ";35"
#define LC_CYAN         ";36"
#define LC_WHITE        ";37"
// background color codes
#define LBC_BLACK       ";40"
#define LBC_RED         ";41"
#define LBC_GREEN       ";42"
#define LBC_YELLOW      ";43"
#define LBC_BLUE        ";44"
#define LBC_MAGENTA     ";45"
#define LBC_CYAN        ";46"
#define LBC_WHITE       ";47"

// log styles from different log levels. Currently only log style ERROR defined
#define LS_RESET     LPREFIX LF_DEF "m"
#define LS_DEBUG
#define LS_VERBOSE
#define LS_WARNING
#define LS_ERROR     LPREFIX LF_NORM_INTENS LC_RED "m"

void server_log_raw(int level, const char* msg);
void server_log(int level, const char* fmt, ...);

#endif // LOG_H_INCLUDED

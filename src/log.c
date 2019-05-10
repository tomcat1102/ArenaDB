/*
    ArenaDB log facitily 4.24. First line of code.
*/
#include <stdio.h>
#include <time.h>
#include <sys/time.h>   // gettimeofday(),
#include <sys/types.h>  // pid
#include <unistd.h>     // getpid(),
#include <stdarg.h>
#include "server.h"
#include "log.h"

/*
    Use server_log() if there is something useful to print, or save to log file.

    Use server_assert() if we are not 100% sure that the condition should pass when it must pass.
    Use server_panic() if there is fatal error that cannot be fixed.
    Both the above two function will terminate the server asap. See debug.c for more info.
*/

//static const int syslog_level_map[] = {LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING};
static const char syslog_char_map[] = ".-*#!";
// used when log long messages, better performance than server_log
void server_log_raw(int level, const char* msg)
{
    FILE *fp;
    int rawmode = level & LL_RAW;
    int log_to_stdout = (server.log_file[0] == '\0');

    level &= LL_MASK;   /* clear raw flag */
    if (level < server.log_verbosity) return;

    fp = log_to_stdout ? stdout : fopen(server.log_file, "a");
    if (!fp) return;

    if (rawmode) {
        fprintf(fp, "%s", msg);
    } else {
        char buf[64];
        int off;

        struct timeval tv;
        gettimeofday(&tv, NULL);

        struct tm *tm = localtime(&tv.tv_sec);
        off = strftime(buf, sizeof(buf), "%d %b %Y %H:%M:%S.", tm);
        snprintf(buf + off, sizeof(buf) - off, "%03d", (int)(tv.tv_usec/1000));

        //format: pid:role_char day month hour:minute:second.millisecond char_prompt msg
        //TODO role_char is always M:master
        //TODO better log appearance, I think the code here is a little bit messy!
        if (level == LL_ERROR) {
            fprintf(fp, "%d:%c %s %c "LS_ERROR"%s"LS_RESET"\n", (int)getpid(), 'M', buf, syslog_char_map[level], msg);
        } else {
            fprintf(fp, "%d:%c %s %c %s\n", (int)getpid(), 'M', buf, syslog_char_map[level], msg);
        }

    }
    fflush(fp);

    if (!log_to_stdout) fclose(fp);
    //TODO add syslog functionality
}
// log function with print-like capability
void server_log(int level, const char* fmt, ...)
{
    if ((level && LL_MASK) < server.log_verbosity) return;

    char msg[1024];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    server_log_raw(level, msg);
}

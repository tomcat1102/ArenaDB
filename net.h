#ifndef NET_H_INCLUDED
#define NET_H_INCLUDED

#include <stdarg.h>
#include "client.h"
#include "sds.h"

// Function delarations
void net_init();
void net_loop();
void net_client_reply_flush(client *c);
void net_client_reply_append_sds(client *c, sds val);
void net_client_reply_append_cstr(client *c, const char *cstr);
void net_client_reply_append_fmt(client *c, const char *fmt, ...);

#endif // NET_H_INCLUDED

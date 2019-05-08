#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include "obj.h"
#include "dict.h"
#include "command.h"

#define server_assert(cond) ( (cond)? (void)0 : (_server_assert(#cond, __FILE__, __LINE__)) )
#define server_panic(...) _server_panic(__FILE__, __LINE__, __VA_ARGS__)
void _server_assert(const char *cond_str, const char* path, int line);
void _server_panic(const char *path, int line, const char *msg, ...);
void server_debug_obj(arobj *o);
void server_debug_command(command *c);
void server_debug_dict(dict *d);
void server_debug_dict_get_stats(char *buf, size_t buf_size, dict *d);

#endif // DEBUG_H_INCLUDED

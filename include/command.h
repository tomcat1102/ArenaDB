#ifndef COMMAND_H_INCLUDED
#define COMMAND_H_INCLUDED

#include "client.h"

// Command execution result codes
#define C_OK    0
#define C_ERR   1

typedef void command_proc(client *c);

typedef struct command {
    int id;                 // command id
    char *name;             // command name
    command_proc *proc;     // command's callback procedure. The arguments to the procedure are stored in c->argv.
    int arity;              // number of argument needed // TODO some future command may have variadic number of argument
} command;

void command_dict_init();
void command_parse_client_args(client *c);
void command_process(client *c);

#endif // COMMAND_H_INCLUDED

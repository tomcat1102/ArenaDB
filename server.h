#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include <sys/types.h>
#include <sys/select.h>
#include "config.h"

#include "client.h"
#include "dict.h"
#include "db.h"

// ArenaDb server
typedef struct arena_server{
    // server meta info
    pid_t pid;          // server process id
    // commands
    dict *commands;     // dict for command look up
    // users
    client **clients;
    int num_clients;
    // net
    int port;
    char *ip;

    char *stdin_buf;    // Buf that holds input from local 'stdin'.
    // fds for select().
    int stdin_fd;
    int socket_fd;
    fd_set active_fds;

    // log
    char* log_file;
    int log_verbosity;

    // databases
    database *db;   //server can have 16 databases
    int num_db;
    // others
} arena_server;

extern arena_server server; // single global server instance

// Function declarations
void server_init();
#endif // SERVER_H_INCLUDED

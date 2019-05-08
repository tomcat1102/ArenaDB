#ifndef CLIENT_H_INCLUDED
#define CLIENT_H_INCLUDED

#include <sys/select.h>
#include "obj.h"
#include "db.h"
#include "sds.h"

#define CLIENT_BUF_SIZE 512
#define CLIENT_MAX_ARG  6

struct command; // Forward declaration, DO NOT REMOVE

typedef struct client{
    // Socket fd
    int fd;
    // Command
    struct command *cmd;   // current command
    int argc;       // number of arguments to the current command
    sds argv[CLIENT_MAX_ARG];      // TODO argument objects to the current command. 6 should be enough
    // Recv buf
    char recv_buf[CLIENT_BUF_SIZE];
    size_t recv_size; // size of received commands in 'recv_buf'
    // Reply buf
    char reply_buf[CLIENT_BUF_SIZE];
    size_t reply_size;  // current reply size in reply_buf.
    // Database
    database *db;   // the database currently SELECTed
} client;

// Function declaration
void client_init();
client *client_lookup(int fd);
void client_register(int fd);
void client_unregister(int fd);


#endif // CLIENT_H_INCLUDED

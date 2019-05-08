/*
    ArenaDB Client Implementation. 5.5
*/

#include "server.h"
#include "client.h"
#include "sds.h"
#include "debug.h"

// TODO currently we use select, so we're able to support maximum FD_SETSIZE clients
// Note that since we use fd to index 'clients' array, we don't use first 3 clients
// because the corresponding fds are 0, 1, 2, 3, which are reserved for stdin, stdout,
// stderr, and server listensing socket_fd.
void client_init()
{
    server.clients = malloc(sizeof(client*) * FD_SETSIZE);
    for(int i = 0; i < FD_SETSIZE; i ++) {
        server.clients[i] = NULL;
    }
    server.num_clients = 0;
}

// Lookup a client connection by 'fd'. Used after select() returns. See net.c
client *client_lookup(int fd)
{
    server_assert(fd >= 4 && fd < FD_SETSIZE);
    return server.clients[fd];
}

// Resiger the client to the system.
void client_register(int fd)
{
    client *c = malloc(sizeof(client));
    c->fd = fd;
    c->db = &server.db[0];
    server.clients[fd] = c;
    server.num_clients ++;

}

// Unregister the client from the system.
void client_unregister(int fd)
{
    client *c = server.clients[fd];
    server_assert(c != NULL);
    for (int i = 0; i < c->argc; i ++) {
        sds_free(c->argv[i]);
    }
    server.clients --;
}

/*
    ArenaDB network implementation. 5.3
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h> // socket()
#include <sys/types.h>
#include <netinet/in.h> // socketaddr_in(it contains a sin_addr)
#include <arpa/inet.h>  // inet_addr()
#include "config.h"
#include "server.h"
#include "client.h"
#include "obj.h"
#include "sds.h"
#include "debug.h"
#include "log.h"
#include "command.h"

// Fd set that select() listens to
static fd_set read_fds;



void net_init()
{
    //char *server_ip_str = "0.0.0.0";
    //char *server_port_str = "8888"; //TODO  read from server_config
    //server.port = atol(server_port_str);

    // 1. Open a Socket
    server.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.socket_fd == -1) {
        server_panic("Server failed to create socket (Error: %s)", strerror(errno));
        return;
    } else if (server.socket_fd != 3) {
        server_panic("Server socket_fd %d != 3", server.socket_fd);
        return;
    } else {
        server_log(LL_VERBOSE, "Server socket created with fd: %d", server.socket_fd);
    }

    // Set option to reused socket address
    int reuse = 1;
    if (setsockopt(server.socket_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == -1) {
        server_log(LL_VERBOSE, "Server socket set option 'SO_REUSEADDR' failed.");
    } else {
        server_log(LL_VERBOSE, "Server socket set option 'SO_REUSEADDR' ok.");
    }



    // 2. Bind to address & port
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(server.port);

    if (bind(server.socket_fd, (struct sockaddr*)&listen_addr, sizeof(struct sockaddr_in)) == -1) {
        server_panic("Server socket failed to bind (Error: %s)", strerror(errno));
        return;
    } else {
        char *ip_str = inet_ntoa(listen_addr.sin_addr);
        server_assert(strcmp(server.ip, ip_str) == 0); // make sure the right address is bind
        server_log(LL_NOTICE,"Server socket bind to address: %s port: %d", server.ip, server.port);
    }

    // 3. listen
    if (listen(server.socket_fd, 3) == -1) {
        server_panic("Server socket failed to listen (Error: %s)", strerror(errno));
        return;
    } else {
        server_log(LL_NOTICE, "Server socket is listening...");
    }

    // 4. Init fds for select()
    FD_ZERO(&read_fds);
    FD_ZERO(&server.active_fds);

    FD_SET(server.socket_fd, &server.active_fds);
    FD_SET(server.stdin_fd, &server.active_fds);
}


int net_loop()
{
    size_t bytes_read = 0;
    char fd_buf[2048];  // fd
    //size_t bytes_sent = 0;
    int num_avail_fd = 0; // number of available fd after select()

    //sprintf(fd_buf, "%d %d", 10, 20);
    //server_log(LL_DEBUG, "Select() monitoring fds: %s", fd_buf);

    // TODO -i enable iteractive server, this should be an option
    while(1) {
        read_fds = server.active_fds;
        // Print fds that will be monitored by select()
        for(int fd = 0, len = 0; fd < FD_SETSIZE; fd ++) {
            if (FD_ISSET(fd, &read_fds)) {
                len += snprintf(fd_buf + len, sizeof(fd_buf) - len, "%d ", fd);
            }
        }
        server_log(LL_DEBUG, "Select() monitoring fds: %s", fd_buf);

        // Select
        num_avail_fd = select(FD_SETSIZE, &read_fds, NULL, NULL, 0);  // block wait since timeout is set to 0.
        if (num_avail_fd <= 0) {              // so result shoud be >= 0 in this case of block waiting
            server_log(LL_ERROR, "Select() failed (Error %s)", strerror(errno));
            return 1;
        }

        // TODO Simulate a time consuming processing in server side. Will we loss some client connections while processing?
        //sleep(10);

        /*--------------------- PROCESS LOCAL STDIN INPUT IF NEEDED ----------------------------*/
        if (FD_ISSET(server.stdin_fd, &read_fds)) {
            num_avail_fd --;

            server_log(LL_DEBUG, "Select() ok. 'stdin' available for reading");
            server_log(LL_DEBUG, "Read() from 'stdin'");

            bytes_read = read(server.stdin_fd, server.stdin_buf, sizeof(server.stdin_buf));
            if (bytes_read == -1) {
                server_log(LL_ERROR, "Read() failed (Error %s)", strerror(errno));
                return 1;
            }

            // procces the stdin_buf
            if (server.stdin_buf[bytes_read - 1] == '\n') bytes_read --;// delete last char if it is a new line char, '\n'
            server.stdin_buf[bytes_read] = '\0'; // null-terminated command string
            server_log(LL_DEBUG, "Read() ok. Processing command <'%s', %ld>", server.stdin_buf, bytes_read);

            // executet command in the stdin_buf
            if (strcasecmp(server.stdin_buf, "exit") == 0) {
                server_log(LL_DEBUG, "Command 'exit'. Server exiting...");
                close(server.socket_fd);
                for(int client_fd = server.socket_fd + 1; client_fd < FD_SETSIZE; client_fd ++) {
                    if (FD_ISSET(client_fd, &read_fds)) {
                        close(client_fd);
                    }
                }
                return 0;
            } else {
                server_log(LL_DEBUG, "Unknown command <%s, %ld>", server.stdin_buf, bytes_read);
            }
        }
        if ( num_avail_fd == 0) continue;  // contine if no other fd available


        /*-------------------- PROCESS REMOET CLIENT NEW CONNECTION IF NEEDED----------------------*/
        if (FD_ISSET(server.socket_fd, &read_fds)) {
            num_avail_fd --;

            struct sockaddr_in client_addr;
            unsigned int addr_size = sizeof(struct sockaddr);
            char ip_buf[17];

            server_log(LL_DEBUG, "Select() ok. Server socket ready to accept() new connection");
            server_log(LL_VERBOSE, "Accept() new client connection");

            int client_fd = accept(server.socket_fd, (struct sockaddr*)&client_addr, &addr_size);
            if (client_fd == -1) {
                server_log(LL_VERBOSE, "Accept() failed (Error %s)", strerror(errno));
                continue;
            }

            strcpy(ip_buf, inet_ntoa(client_addr.sin_addr));
            server_log(LL_VERBOSE, "Accept() ok. New client from %s. Client fd: %d added",ip_buf, client_fd);
            client_register(client_fd);
            FD_SET(client_fd, &server.active_fds);
        }
        if ( num_avail_fd == 0) continue;  // contine if no other fd available

        /*------------------------- PROCESS REMOTE CLIENT COMMANDS IF ANY ----------------------------*/
        server_log(LL_DEBUG, "Select() ok. Now %d client fd%s available for recv()",
            num_avail_fd, (num_avail_fd > 1) ? "s": "");

        for(int client_fd = server.socket_fd + 1; client_fd < FD_SETSIZE; client_fd ++) {
            if (!FD_ISSET(client_fd, &read_fds))
                continue;

            server_log(LL_VERBOSE, "Recv() from client fd: %d", client_fd);
            client *c = client_lookup(client_fd);
            bytes_read = recv(client_fd, c->recv_buf, CLIENT_BUF_SIZE, 0);
            if (bytes_read == -1) {
                server_log(LL_VERBOSE, "Recv() failed (Error %s). Close client fd: %d", strerror(errno), client_fd);

                FD_CLR(client_fd, &server.active_fds);
                close(client_fd);
                continue;
            } else if (bytes_read == 0) {
                server_log(LL_VERBOSE, "Recv() none. Close client fd: %d", client_fd);

                FD_CLR(client_fd, &server.active_fds);
                close(client_fd);
                continue;
            }
            // Now we process command in client's recv_buf
            c->recv_buf[bytes_read] = '\0';
            c->recv_size = bytes_read;
            c->reply_size = 0;

            server_log(LL_VERBOSE, "Recv() ok <'%s', %ld>", c->recv_buf, bytes_read);
            command_process(c);

        }
    }
    return 0;
}

// TODO for all append functions. If reply_size reaches CLIENT_BUF_SIZE, we need to flush it and append remaining.
// Append to client reply buf with print-like format
void net_client_reply_append_fmt(client *c, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int l = vsnprintf(c->reply_buf + c->reply_size, CLIENT_BUF_SIZE - c->reply_size, fmt, ap);
    c->reply_size += l;
    va_end(ap);

}

// Append to client reply buf with string object.
void net_client_reply_append_string_obj(client *c, arobj *o)
{
    server_assert(o->type == OBJ_TYPE_STRING);

    int len;
    int enc = o->encoding;

    if (enc == OBJ_ENC_SDS || enc == OBJ_ENC_EMBSDS) {
        sds s = o->ptr;
        len = sds_len(s);
        strncpy(c->reply_buf + c->reply_size, s, len);
    } else if (enc == OBJ_ENC_INT) {
        len = sprintf(c->reply_buf + c->recv_size, "%ld", (long)o->ptr);
    }

    c->reply_size += len;
}

// Append to client reply buf with sds 'val'.
void net_client_reply_append_sds(client *c, sds val)
{
    size_t l = sds_len(val);
    strncpy(c->reply_buf + c->reply_size, val, l);
    c->reply_size += l;
}

// Append to client reply buf with c style string.
void net_client_reply_append_cstr(client *c, const char *cstr)
{
    size_t l = strlen(cstr);
    strncpy(c->reply_buf + c->reply_size, cstr, l);
    c->reply_size += l;
}

// Write reply to client 'c'.
void net_client_reply_flush(client *c)
{
    int client_fd = c->fd;
    size_t bytes_sent = send(client_fd, c->reply_buf, c->reply_size, 0);
    if (bytes_sent == -1) {
        server_log(LL_VERBOSE, "Send() failed. Close client fd: %d", client_fd);
        client_unregister(client_fd);
        FD_CLR(client_fd, &server.active_fds);
    } else if (bytes_sent == 0) {
        server_log(LL_VERBOSE, "Send() none. Close client fd: %d", client_fd);
        client_unregister(client_fd);
        FD_CLR(client_fd, &server.active_fds);
    } else {
        c->recv_size = 0;   // reset reply index.
        c->reply_buf[c->reply_size] = '\0';
        server_log(LL_VERBOSE, "Send() ok. <'%s', %ld>", c->reply_buf, bytes_sent);
    }
}


/*
// Command 'time'
if (strcasecmp(client_buf, "time") == 0) {
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    size_t len = snprintf(reply_buf, sizeof(reply_buf), "%s", asctime(local_time));
    reply_buf[len - 2] = '\0'; // Have to remove trailing '\n'

    // send result
    if ((bytes_sent = send(client_fd, reply_buf, len, 0)) == -1) {
        server_log(LL_VERBOSE, "Send() failed for command 'time'. Close client fd: %d", client_fd);
        FD_CLR(client_fd, &active_fds);
        close(client_fd);
    } else {
        server_log(LL_VERBOSE, "Send() ok <%s, %ld>", reply_buf, bytes_sent);
    }
}  // command 'set' 'key' 'value'
else if (strncasecmp(client_buf, "set", 3) == 0) {

    char *msg_0 = "set ok";
    char *msg_1 = "(error) set no key";
    char *msg_2 = "(error) set no val";
    char *msg_3 = "(error) key %s already exits";

    // parse key and value
    int ps = 3, pe;
    // skip white space before key
    for(; ps < bytes_read && isspace(client_buf[ps]); ps ++);
    if (ps == bytes_read) { send(client_fd, msg_1, strlen(msg_1), 0); continue; }
    // skip key content
    for(pe = ps; pe < bytes_read && !isspace(client_buf[pe]);  pe ++)
    if (pe == bytes_read) { send(client_fd, msg_2, strlen(msg_2), 0); continue; }
    // get key
    sds key = sds_new_len(client_buf + ps, pe - ps);
    // parse val
    for(ps = pe; ps < bytes_read && isspace(client_buf[ps]); ps ++);
    if (ps == bytes_read) { send(client_fd, msg_2, strlen(msg_2), 0); continue; }
    sds val = sds_new_len(client_buf + ps, bytes_read - ps);


    // add the entry
    server_log(LL_DEBUG, "Server add new entry (%s, %s)", key, val);
    if (dict_add_entry(server.db[0].d, key, val) == DICT_ERR) {
        int len = snprintf(reply_buf, sizeof(reply_buf), msg_3, key);
        send(client_fd, reply_buf, len, 0); continue;
    }

    // send ok replay
    if ((bytes_sent = send(client_fd, msg_0, strlen(msg_0), 0)) == -1) {
        server_log(LL_VERBOSE, "Send() failed for command 'time'. Close client fd: %d", client_fd);
        FD_CLR(client_fd, &active_fds);
        close(client_fd);
    } else {
        server_log(LL_VERBOSE, "Send() ok <%s, %ld>", msg_0, strlen(msg_0));
    }

} // command 'get'
else if (strncasecmp(client_buf, "get", 3) == 0) {

    char *msg_1 = "(error) get no key";
    char *msg_2 = "(erro) key %s not exists";

    // parse key
    int ps = 3, pe;
    // skip white space before key
    for(; ps < bytes_read && isspace(client_buf[ps]); ps ++);
    if (ps == bytes_read) { send(client_fd, msg_1, strlen(msg_1), 0); continue; }
    // skip key content
    for(pe = ps; pe < bytes_read && !isspace(client_buf[pe]);  pe ++)
    if (pe == bytes_read) { send(client_fd, msg_1, strlen(msg_1), 0); continue; }
    // get key
    sds key = sds_new_len(client_buf + ps, pe - ps);

    sds val = dict_fetch_value(server.db[0].d, key);
    if (val == NULL) {
        int len = snprintf(reply_buf, sizeof(reply_buf), msg_2, key);
        send(client_fd, reply_buf, len, 0); continue;
    }

    // send result
    if ((bytes_sent = send(client_fd, val, sds_len(val), 0)) == -1) {
        server_log(LL_VERBOSE, "Send() failed for command 'time'. Close client fd: %d", client_fd);
        FD_CLR(client_fd, &active_fds);
        close(client_fd);
    } else {
        server_log(LL_VERBOSE, "Send() ok <%s, %ld>", val, sds_len(val));
    }

} // Command 'exit'
else if (strcasecmp(client_buf, "exit") == 0) {
    server_log(LL_VERBOSE, "Client disconnected. Remove client fd: %d ok", client_fd);
    FD_CLR(client_fd, &active_fds);
    close(client_fd);
} // Unknown command
else {
    server_log(LL_DEBUG, "Unknown command <%s, %ld>", client_buf, bytes_read);
    int len = snprintf(reply_buf, sizeof(reply_buf), "(error) Unknown command '%s'", client_buf);
    if ((bytes_sent = send(client_fd, reply_buf, len, 0)) == -1) {
        server_log(LL_VERBOSE, "Send() failed <'Unknown command...'>. Close client fd: %d", client_fd);
        FD_CLR(client_fd, &active_fds);
        close(client_fd);
    } else {
        server_log(LL_VERBOSE, "Send() ok <%s, %ld>", reply_buf, bytes_sent);
    }
}
*/

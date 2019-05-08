/*
    ArenaDB some experiments go here. 5.2
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h> // socket()
#include <sys/types.h>
#include <netinet/in.h> // socketaddr_in(it contains a sin_addr)
#include <arpa/inet.h>  // inet_addr()
#include "config.h"
#include "server.h"
#include "sds.h"
#include "obj.h"
#include "util.h"
#include "debug.h"
#include "log.h"
#include "command.h"
#include "client.h"


int experiment_main()
{
    printf("client* size: %lu \n", sizeof(client*));

    return 0;
}

int experiment_server_main()
{
    int socket_fd;
    int stdin_fd = fileno(stdin);
    char server_ip[17];
    char *server_port_str = "8888";
    unsigned short server_port = atol(server_port_str);


    // 1. Open a Socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);  //
    if (socket_fd != -1) {
        server_log(LL_DEBUG, "Server socket created (fd: %d)", socket_fd);
    } else {
        server_log(LL_ERROR, "Server failed to create socket (Error: %s)", strerror(errno));
        return 1;
    }

    // 2. Bind to address & port
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(server_port);

    if (bind(socket_fd, (struct sockaddr*)&listen_addr, sizeof(struct sockaddr_in)) == -1) {
        server_log(LL_ERROR, "Server socket failed to bind (Error: %s)", strerror(errno));
        return 1;
    } else {
        char *ip_str = inet_ntoa(listen_addr.sin_addr);
        strcpy(server_ip, ip_str);
        server_log(LL_NOTICE,"Server socket bind to address:%s port:%d", server_ip, server_port);
    }

    // 3. listen
    if (listen(socket_fd, 3) == 0) {
        server_log(LL_NOTICE, "Server socket is listening...");
    } else {
        server_log(LL_ERROR, "Server socket failed to listen (Error: %s)", strerror(errno));
        return 1;
    }

    // 4. accept in select() way
    fd_set read_fds;    // fd set that holds fds to be listenned by select()
    fd_set active_fds;  // fd set that contains active fds we want to be listen

    FD_ZERO(&read_fds);
    FD_ZERO(&active_fds);

    FD_SET(socket_fd, &active_fds);
    FD_SET(stdin_fd, &active_fds);

    char stdin_buf[1024];
    char reply_buf[1024];   // buf that holds reply contents to clients
    char client_buf[1024];  // buf that holds commands from clients
    size_t bytes_read = 0;
    char fd_buf[2048];
    size_t bytes_sent = 0;
    int num_avail_fd = 0; // number of available fd after select()

    // TODO -i enable iteractive server, this should be an option
    while(1) {
        read_fds = active_fds;
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
        if (FD_ISSET(stdin_fd, &read_fds)) {
            num_avail_fd --;

            server_log(LL_DEBUG, "Select() ok. 'stdin' available for reading");
            server_log(LL_DEBUG, "Read() from 'stdin'");

            bytes_read = read(stdin_fd, stdin_buf, sizeof(stdin_buf));
            if (bytes_read == -1) {
                server_log(LL_ERROR, "Read() failed (Error %s)", strerror(errno));
                return 1;
            }

            // procces the stdin_buf
            if (stdin_buf[bytes_read - 1] == '\n') bytes_read --;// delete last char if it is a new line char, '\n'
            stdin_buf[bytes_read] = '\0'; // null-terminated command string
            server_log(LL_DEBUG, "Read() ok. Processing command <'%s', %ld>", stdin_buf, bytes_read);

            // executet command in the stdin_buf
            if (strcasecmp(stdin_buf, "exit") == 0) {
                server_log(LL_DEBUG, "Command 'exit'. Server exiting...");
                close(socket_fd);
                for(int client_fd = socket_fd + 1; client_fd < FD_SETSIZE; client_fd ++) {
                    if (FD_ISSET(client_fd, &read_fds)) {
                        close(client_fd);
                    }
                }
                server_log(LL_NOTICE, "Server exited");
                return 0;
            } else {
                server_log(LL_DEBUG, "Unknown command <%s, %ld>", stdin_buf, bytes_read);
            }
        }
        if ( num_avail_fd == 0) continue;  // contine if no other fd available


        /*-------------------- PROCESS REMOET CLIENT NEW CONNECTION IF NEEDED----------------------*/
        if (FD_ISSET(socket_fd, &read_fds)) {
            num_avail_fd --;

            struct sockaddr_in client_addr;
            unsigned int addr_size = sizeof(struct sockaddr);
            char ip_buf[17];

            server_log(LL_DEBUG, "Select() ok. Server socket ready to accept() new connection");
            server_log(LL_VERBOSE, "Accept() new client connection");

            int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &addr_size);
            if (client_fd == -1) {
                server_log(LL_VERBOSE, "Accept() failed (Error %s)", strerror(errno));
                continue;
            }

            strcpy(ip_buf, inet_ntoa(listen_addr.sin_addr));
            server_log(LL_VERBOSE, "Accept() ok. New client from %s. Client fd: %d added",ip_buf, client_fd);
            FD_SET(client_fd, &active_fds);
        }
        if ( num_avail_fd == 0) continue;  // contine if no other fd available

        /*------------------------- PROCESS REMOTE CLIENT COMMANDS IF ANY ----------------------------*/
        server_log(LL_DEBUG, "Select() ok. Now %d client fd%s available for recv()",
            num_avail_fd, (num_avail_fd > 1) ? "s": "");

        for(int client_fd = socket_fd + 1; client_fd < FD_SETSIZE; client_fd ++) {
            if (!FD_ISSET(client_fd, &read_fds))
                continue;

            server_log(LL_VERBOSE, "Recv() from client fd: %d", client_fd);
            bytes_read = recv(client_fd, client_buf, sizeof(client_buf), 0);
            if (bytes_read == -1) {
                server_log(LL_VERBOSE, "Recv() failed (Error %s). Close client fd: %d", strerror(errno), client_fd);

                FD_CLR(client_fd, &active_fds);
                close(client_fd);
                continue;
            } else if (bytes_read == 0) {
                server_log(LL_VERBOSE, "Recv() none. Close client fd: %d", client_fd);

                FD_CLR(client_fd, &active_fds);
                close(client_fd);
                continue;
            }
            // Now we process command in client_buf
            client_buf[bytes_read] = '\0';
            server_log(LL_VERBOSE, "Recv() ok <%s, %ld>", client_buf, bytes_read);
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
            }  // Command 'exit'
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
        }
    }
    return 0;
}

int experiment_client_main()
{
    // Seems that the client works. Now we turn off log and it should look better.
    extern arena_server server;
    server.log_verbosity = LL_ERROR;


    int stdin_fd = fileno(stdin);
    int socket_fd;

    // 1. open a socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        server_log(LL_ERROR, "Socket() failed. (Error %s)", strerror(errno));
        return 1;
    }
    server_log(LL_VERBOSE, "Socket() ok. socket fd: %d", socket_fd);

    // 2. Bind to address & port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("202.199.13.248");
    server_addr.sin_port = htons(8888);

    // 3. Connect to the server
    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) == -1) {
        server_log(LL_ERROR, "Connect() failed. (Error %s)", strerror(errno));
        printf("Error; %s \n", strerror(errno));
        fflush(stdout);
        return 1;
    }
    server_log(LL_NOTICE, "Connect() ok.", socket_fd);

    fd_set read_fds;    // client listens to 2 fds. One for stdin, the other for remote server
    fd_set active_fds;

    FD_ZERO(&read_fds);
    FD_ZERO(&active_fds);

    FD_SET(socket_fd, &active_fds);
    FD_SET(stdin_fd, &active_fds);

    // 4. Send & Receive data
    char buf[1024];
    size_t bytes_read = 0;
    size_t bytes_sent = 0;
    pid_t pid = getpid();

    int wait_result = 0;
    while (1) {
        if (!wait_result) {
            printf("127.0.0.1:%d> ", pid);fflush(stdout);
        }

        read_fds = active_fds;

        if (select(socket_fd + 1, &read_fds, NULL, NULL, NULL) <= 0) {
            server_log(LL_ERROR, "Select() failed. (Error %s)", strerror(errno));
            close(socket_fd);
            return 1;
        }

        // First process input from remote server if any
        if (FD_ISSET(socket_fd, &read_fds)) {
            wait_result = 0;
            server_log(LL_DEBUG, "Select() ok. Server sent data available, ready to recv()");

            bytes_read = recv(socket_fd, buf, sizeof(buf), 0);
            if (bytes_read == -1) {
                server_log(LL_NOTICE, "Recv() failed. Exiting");
                close(socket_fd);
                return 0;
            } else if (bytes_read == 0) {
                server_log(LL_NOTICE, "Recv() none. Exiting");
                close(socket_fd);
                return 0;
            } else {
                buf[bytes_read] = '\0';
                server_log(LL_VERBOSE, "Recv() ok <'%s', %ld>", buf, bytes_read);
                // TODO should have protocal! And here try to parse the result and print correctly
                printf("%s\n", buf);
            }
        }

        // Then process input from user input if any
        if (FD_ISSET(stdin_fd, &read_fds)) {
            server_log(LL_DEBUG, "Select() ok. stdin available, ready to read()");
            bytes_read = read(stdin_fd, buf, sizeof(buf));
            if (bytes_read == -1) {
                server_log(LL_ERROR, "Read() failed. (Error %s)", strerror(errno));
                close(socket_fd);
                return 1;
            }
            if (buf[bytes_read - 1] == '\n') bytes_read --; // delete last char if it is a new line char, '\n'
            buf[bytes_read] = '\0';

            server_log(LL_VERBOSE, "Read() ok. Send() <'%s', %ld>", buf, bytes_read);
            bytes_sent = send(socket_fd, buf, bytes_read, 0);
            if (bytes_sent == -1) {
                server_log(LL_NOTICE, "Send() failed. Please check your connectionn");
            } else if (bytes_sent != bytes_read) {
                server_log(LL_NOTICE, "Send() bad. Incomplete command sent. Please check your connectionn");
            } else {
                server_log(LL_VERBOSE, "Send() ok <'%s', %ld>", buf, bytes_sent);
                wait_result = 1;
            }
            // check for 'exit' command
            if (strcasecmp(buf, "exit") == 0) {
                server_log(LL_VERBOSE, "Command 'exit'. Client exiting...");
                close(socket_fd);
                server_log(LL_NOTICE, "Client exited");
                return 0;
            }
        }
    }

    return 0;
}


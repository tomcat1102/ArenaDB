#include <stdlib.h>
#include <stdio.h>
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
#include "log.h"


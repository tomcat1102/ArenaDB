/*
*   ArenaDB configuration. 5.4
*/

/*
*   This file contains some funtion to parse, load configs.
*   For more info, see config.h
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "server.h"
#include "config.h"
#include "log.h"

// Initialize server configurations
void config_init()
{
    // log
    server.log_file = ""; // log to stdout by default
    server.log_verbosity = LL_DEBUG; // TODO change LL_DEBUG to LL_NOTICE
    // net
    server.port = atol("8888");
    server.ip = "0.0.0.0";

    // databases
    server.db = NULL;
    server.num_db = CONFIG_PARAM_DB_NUM;


    // Overwrite the default init by configs from config file

    // Chech that server.num_db >= 0 && <= CONFIG_MAX_DB_NUM
}

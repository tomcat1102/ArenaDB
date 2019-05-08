#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "server.h"
#include "sds.h"
#include "dict.h"
#include "db.h"
#include "net.h"
#include "obj.h"
#include "command.h"
#include "net.h"
#include "log.h"
#include "util.h"

#ifdef CONFIG_BUILD_TEST
    #include "test.h"
#endif

#ifdef CONFIG_BUILD_BENCHMARK
    #include "benchmark.h"
#endif


arena_server server;


int main(int argc, char *argv[])
{
    if (argc > 1) {
        extern int experiment_server_main();
        extern int experiment_client_main();
        extern int experiment_main();

        if (strcasecmp(argv[1], "experiment_server") == 0) {
            config_init();
            return experiment_server_main();
        } else if (strcasecmp(argv[1], "experiment_client") == 0) {
            config_init();
            return experiment_client_main();
        } else if (strcasecmp(argv[1], "experiment_main") == 0) {
            config_init();
            return experiment_main();
        }
    }

    #ifdef CONFIG_BUILD_TEST
    if (argc > 1){
        if (strcasecmp(argv[1], "all_test") == 0) {
            if (argc != 2) {
                printf("Usage: ./ArenaDB \n");
            }
            // all tests go here....
            sds_test_main();
            dict_test_main();
            return 0;
        } else if (strcasecmp(argv[1], "sds_test") == 0) {
            if (argc != 2) {
                printf("Usage: ./ArenaDB sds_test \n");
                return 0;
            }
            return sds_test_main();
        } else if (strcasecmp(argv[1], "dict_test") == 0) {
            if (argc != 2) {
                printf("Usage: ./ArenaDB dict_test \n");
                return 0;
            }
            return dict_test_main();
        }
    }
    #endif // CONFIG_BUILD_TEST

    #ifdef CONFIG_BUILD_BENCHMARK
    if (argc > 1) {
        if (strcasecmp(argv[1], "dict_benchmark") == 0) {
            if (argc == 2) {
                return dict_benchmark_main(5000000);
            }

            if (argc > 3) {
                printf("Usage: ./ArenaDB dict_benchmark [count] \n");
                return 0;
            }

            long count = strtol(argv[2], NULL, 10);
            if (count <= 0) {
                printf("Usage: ./ArenaDB dict_benchmark [count] \n");
                printf(" Bad count \n");
                return 0;
            }
            return dict_benchmark_main(count);
        }
    }
    #endif // CONFIG_BUILD_BENCHMARK

    // Init server configuration
    config_init();
    // Init server itself
    server_log(LL_VERBOSE, "Server is starting...");
    server_init();
    server_log(LL_NOTICE, "Server started");

    net_init();
    // main loop in net.c
    net_loop();

    // server exit
    server_log(LL_VERBOSE, "Server exiting normally...");
    server_log(LL_NOTICE, "Server exited");
    return 0;
}

void server_init()
{
    server.pid = getpid();
    server.stdin_buf = malloc(1024);
    server.stdin_fd = fileno(stdin);

    db_init();
    client_init();

    // dict must be first inited
    dict_hash_seed_init();
    command_dict_init();

    obj_create_shared();

}

void server_exit()
{

}


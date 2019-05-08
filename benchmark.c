/*
    ArenaDB benchmark cases. 5.2
*/

/*
*   It's strongly recommend that you run the corresponding test case before the benchmark!
*/

#include "config.h"

#ifdef CONFIG_BUILD_BENCHMARK
#include <stdio.h>
#include <assert.h>
#include "dict.h"
#include "sds.h"
#include "util.h"
#include "debug.h"

/*----------------------------------DICT BENCHMARK-------------------------------------------*/
int dict_benchmark_main(long count)
{
    long long bm_start, bm_elapsed;     // start time and elapsed time of the benchmark
    long bm_count = 100;                // conut of item to process in the benchmark

    #define start_benchmark() do { \
        bm_start = util_get_time_in_millisecond(); \
    } while(0)

    #define end_benchmark(msg) do { \
        bm_elapsed = util_get_time_in_millisecond() - bm_start; \
        printf(msg": %ld items in %lld ms \n", bm_count, bm_elapsed); \
    } while(0)

    bm_count = count;
    printf("Dict benchmark with %ld entries \n", bm_count);
    dict_hash_seed_init();


    dict *d = dict_create(&sample_dict_type);


    start_benchmark();
    for(long i = 0; i < bm_count; i ++) {
        int ret_val = dict_add_entry(d, sds_from_longlong(i), (void*) i);
        assert(ret_val == DICT_OK);
    }
    end_benchmark("Inserting");

    start_benchmark();
    while(dict_is_rehashing(d)) {
        dict_rehash(d, 100);
    }
    end_benchmark("Waiting while rehashing in progress");

    start_benchmark();
    for(long i = 0; i < bm_count; i ++) {
        sds key = sds_from_longlong(i);
        dict_entry *de = dict_find(d, key);
        sds_free(key);

        assert(de != NULL);
        //assert(dict_get_signed_integer_val(de) == i); // This should PASS!
    }
    end_benchmark("Linear access of existing entries");

    start_benchmark();
    for(long i = 0; i < bm_count; i ++) {
        sds key = sds_from_longlong(i);
        dict_entry *de = dict_find(d, key);
        sds_free(key);

        assert(de != NULL);
    }
    end_benchmark("Linear access of existing entries");

    start_benchmark();
    for(long i = 0; i < bm_count; i ++) {
        sds key = sds_from_longlong(rand() % bm_count);
        dict_entry *de = dict_find(d, key);
        sds_free(key);

        assert(de != NULL);
    }
    end_benchmark("Random access of existing entries");

    start_benchmark();
    for(long i = 0; i < bm_count; i ++) {
        sds key = sds_from_longlong(rand() % bm_count);
        key[0] = 'X';
        dict_entry *de = dict_find(d, key);

        if (de != NULL)
        {
            printf("BUG KEY: %s \n", key);
            printf("entry val: %ld \n", dict_get_signed_integer_val(de));
            assert(de != NULL);
        }

        sds_free(key);

    }
    end_benchmark("Access missing");

    start_benchmark();
    for(long i = 0; i < bm_count; i ++) {
        sds key = sds_from_longlong(i);
        int ret = dict_delete(d, key);
        assert(ret == DICT_OK);
        // Change first number to letter
        key[0] += 17;
        ret = dict_add_entry(d, key, (void*)i);
        assert(ret == DICT_OK);
    }
    end_benchmark("Removing and adding");

    #define BUF_SIZE  4096
    char *buf = malloc(BUF_SIZE);
    server_debug_dict_get_stats(buf, BUF_SIZE, d);
    return 0;
}
#endif // CONFIG_BUILD_BENCHMARK


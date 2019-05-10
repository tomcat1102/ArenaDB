/*
    ArenaDB test cases. 5.2
*/

#include "config.h"

#ifdef  CONFIG_BUILD_TEST
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "sds.h"
#include "dict.h"
#include "test.h"

static int __failed_tests = 0;
static int __test_num = 0;

// Test helper defines
#define test_cond(descr, _c) do{ \
    __test_num ++; printf("%d - %s: ", __test_num, descr); \
    if (_c) printf(" PASSED\n"); else {printf("FAILED\n"); __failed_tests ++; } \
} while(0);

#define test_report() do{ \
    printf("#%2d tests, %d passed, %d failed\n", __test_num, \
            __test_num-__failed_tests, __failed_tests); \
    if (__failed_tests) { \
        printf("===WARNING=== Failed tests in: "__FILE__"\n"); \
    } \
} while(0);


/*----------------------------------DICT TEST-----------------------------------------------*/
int dict_test_main()
{
    char *keys[] = {"DB name", "DB author", "DB mail", "DB bug0", "DB bug1"};
    char *vals[] = {"ArenaDB", "tomcat1102", "905438066@qq.com", "bug 0:...", "bug 1:..."};

    // test dict_add_entry()
    dict *d = dict_create(&sample_dict_type);
    for(int i = 0; i < 5; i ++) dict_add_entry(d, sds_new(keys[i]), sds_new(vals[i]));
    test_cond("dict_add_entry()", dict_keys(d) == 5);

    // test dict_add_or_replace_entry() add
    test_cond("dict_add_or_replace() add",
        dict_add_or_replace_entry(d, sds_new("DB description"), sds_new("none")) == 1 &&
        dict_keys(d) == 6);

    // test dict_add_or_replace() replace
    test_cond("dict_add_or_replace() replace",
        dict_add_or_replace_entry(d, sds_new("DB description"),
        sds_new("A in-memory key-value store, reconstructed from Redis")) == 0 &&
        dict_keys(d) == 6);

    // test dict_unlink()
    dict_entry *de = dict_unlink(d, sds_new("DB bug0"));
    test_cond("dict_unlink()", de != NULL && dict_keys(d) == 5);

    // test dict_free_unlinked_entry
    test_cond("dict_free_unlinked_entry()", dict_free_unlinked_entry(d, de) == DICT_OK);

    // test dict_delete()
    test_cond("dict_delete()", dict_delete(d, sds_new("DB bug1")) == DICT_OK && dict_keys(d) == 4);

    // test dict_fetch_val()
    void *fetch_val = NULL;
    test_cond("dict_fetch_val()",
        (fetch_val = dict_fetch_value(d, sds_new("DB name"))) != NULL &&
        strncmp(fetch_val, "ArenaDB", 7) == 0);

    // test find();
    test_cond("dict_find()", dict_find(d, sds_new("DB name")) != NULL &&
        dict_find(d, sds_new("DB Name")) == NULL);

    // test iterator dict_next();
    dict_iterator *iter = dict_get_iterator(d);
    int num_entry = 0;
    while((de = dict_next(iter)) != NULL) {
        //printf("\t%-15s: %s \n", (sds)dict_get_key(de), (sds)dict_get_val(de));
        num_entry ++;
    }
    test_cond("dict_next(iter)", num_entry == 4);

    // test dict_free_iterator()
    test_cond("dict_free_iterator(iter)", dict_free_iterator(iter) == DICT_OK);

    test_report();
    return 0;
};

/*-----------------------------------SDS TEST-----------------------------------------------*/
int sds_test_main()
{
    assert(sizeof(sds_hdr_6) == 1);
    assert(sizeof(sds_hdr_8) == 3);
    assert(sizeof(sds_hdr_16) == 5);


    sds x = sds_new("foo"), y;

    test_cond("Create a string and obtain its length",
        sds_len(x) == 3 && memcmp(x, "foo\0", 4) == 0);

    sds_free(x);
    x = sds_new_len("foo", 2);


    test_cond("Create a string with specified length",
        sds_len(x) == 2 && memcmp(x, "fo\0", 3) == 0);

    x = sds_cat(x, "bar");
    test_cond("String concatenation",
        sds_len(x) == 5 && memcmp(x, "fobar\0", 6) == 0);

    x = sds_copy(x, "a");
    test_cond("sds_copy() against an originally longer string",
        sds_len(x) == 1 && memcmp(x, "a\0", 2) == 0);

    x = sds_copy(x, "0123456789012345678901234567890123456789012345678901234567890123456789");
    test_cond("sds_copy() against an originally shorter string",
        sds_len(x) == 70 &&
        memcmp(x, "0123456789012345678901234567890123456789012345678901234567890123456789\0", 71) == 0);

    sds_free(x);
    x = sds_cat_printf(sds_new_empty(), "%d %s", 123, "hello");
    test_cond("sds_cat_printf() seems working in the base case",
        sds_len(x) == 9 && memcmp(x, "123 hello\0", 10) == 0);

    sds_free(x);
    x = sds_new(" x ");
    sds_trim(x, " x");
    test_cond("sds_trim() works when all chars match",
        sds_len(x) == 0);

    sds_free(x);
    x = sds_new(" x ");
    sds_trim(x, " ");
    test_cond("sds_trim() works when a single char remains",
        sds_len(x) == 1 && memcmp(x, "x\0", 2) == 0);

    sds_free(x);
    x = sds_new("xxciaoyy");
    sds_trim(x, "xy");
    test_cond("sds_trim() correctly trims characters",
        sds_len(x) == 4 && memcmp(x, "ciao\0", 4) == 0);

    y = sds_dup(x); // y is now 'ciao'
    sds_range(y, 1, 1);

    test_cond("sds_range(..., 1, 1)",
        sds_len(y) == 1 && memcmp(y, "i\0", 2) == 0);

    sds_free(y);
    y = sds_dup(x);
    sds_range(y, 1, -1);
    test_cond("sds_range(..., 1, -1)",
        sds_len(y) == 3 && memcmp(y, "iao\0", 4) == 0);

    sds_free(y);
    y = sds_dup(x);
    sds_range(y, -2, -1);
    test_cond("sds_range(..., -2, -1)",
        sds_len(y) == 2 && memcmp(y, "ao\0", 2) == 0);

    sds_free(y);
    y = sds_dup(x);
    sds_range(y, 2, 1);
    test_cond("sds_range(..., 2, 1)",
        sds_len(y) == 0 && memcmp(y, "\0", 1) == 0);

    sds_free(y);
    y = sds_dup(x);
    sds_range(y, 1, 100);
    test_cond("sds_range(..., 1, 100)",
        sds_len(y) == 3 && memcmp(y, "iao\0", 4) == 0);

    sds_free(y);
    y = sds_dup(x);
    sds_range(y, 100, 100);
    test_cond("sds_range(..., 100, 100",
        sds_len(y) == 0 && memcmp(y, "\0", 1) == 0);

    sds_free(y);
    sds_free(x);
    x = sds_new("foo");
    y = sds_new("foa");
    test_cond("sds_cmp(foo, foa)", sds_cmp(x, y) > 0);

    sds_free(y);
    sds_free(x);
    x = sds_new("bar");
    y = sds_new("bar");
    test_cond("sds_cmp(bar, bar)", sds_cmp(x, y) == 0);

    sds_free(y);
    sds_free(x);
    x = sds_new("aar");
    y = sds_new("bar");
    test_cond("sds_cmp(aar, bar)", sds_cmp(x, y) < 0);

    sds_free(y);
    sds_free(x);
    x = sds_new("foobar");
    y = sds_new("foo");
    test_cond("sds_cmp(foobar, foo)", sds_cmp(x, y) > 0);

    {
        sds_free(y);
        sds_free(x);
        x = sds_new("X");
        test_cond("sds_new() free/len buffers", sds_len(x) == 1 && sds_avail(x) == 0);
        // sds_debug_print(x, 1);
        // Run the test a few times to hit the sds_hdr_8 & sds_hdr_16, each time with a increase of 'step'
        int step = 10, rounds = 15;
        char *p;
        for (int i = 0; i < rounds; i ++)
        {
            int old_len = sds_len(x);
            x = sds_make_room_for(x, step);
            // use assert() here to reduce debug output.
            assert(sds_len(x) == old_len);
            assert(sds_avail(x) >= step);
            p = x + old_len;
            for(int i = 0; i < step; i ++) {
                p[i] = '0' + i;
            }
            sds_incr_len(x, step);
            //sds_debug_print(x, (i <= 4) ? 1: 0);

        }
        #define NUM_50 "01234567890123456789012345678901234567890123456789"
        #define NUM_150 NUM_50 NUM_50 NUM_50
        test_cond("sds_make_room_for() content", memcmp("X"NUM_150"\0", x, 52) == 0);
        test_cond("sds_make_room_for() final length", sds_len(x) == 10 * rounds + 1);
        #undef NUM_50
        #undef NUM_150

        sds_free(x);
    }
    test_report();

    return 0;
}

#endif


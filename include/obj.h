#ifndef OBJ_H_INCLUDED
#define OBJ_H_INCLUDED

#include <stddef.h>

// Data types in ArenaDB

// All object in ArenaDB belongs to one of these types
#define OBJ_TYPE_STRING 0
//#define OBJ_TYPE_LIST   1
//#define OBJ_TYPE_SET    2
//#define OBJ_TPYE_ZSET   3
//#define OBJ_TYPE_HASH   4

// The low level data structures that implement the above object types are as follows.
// e.g. an object of type string can be implemented (encoded) as // TODO what ?
// See obj.c for more encoding information
#define OBJ_ENC_SDS     0   // encoding for long string. 'ptr' points to an sds string.
#define OBJ_ENC_EMBSDS  1   // encoding for short string. 'ptr' potins to an embeded sds string which is right after obj itself.
#define OBJ_ENC_INT     2   // encoding for int string . 'ptr' is used for storing an integer.
//#define OBJ_ENC_HASH    3   // encodign for hash. 'ptr' points to a dict type obj.

#define OBJ_SHARED_REFCOUNT INT_MAX

typedef struct {
    unsigned int type: 4;
    unsigned int encoding: 4;
    unsigned int lru: 24;
    int ref_count;
    void *ptr;
} arobj;

// Shared objects across server

#define OBJ_SHARED_INTEGERS     2049    // -1024, ..., -1, 0, 1, ... ,1024

struct shared_objs {
    arobj *integers[OBJ_SHARED_INTEGERS];
};
extern struct shared_objs shared;

// Function declarations
void obj_create_shared();
arobj *obj_create(int type, int encoding, void *ptr);
arobj *obj_create_string(const char *str, size_t len);
arobj *obj_create_string_from_ll(long long val);
arobj *obj_create_string_from_ll_withoption(long long val, int try_shared);
void obj_inc_ref();
void obj_dec_ref();


#endif // OBJ_H_INCLUDED

/*
    ArenaDB ojbect implementation. 5.4
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "limits.h"
#include "dict.h"
#include "sds.h"
#include "obj.h"
#include "debug.h"

static arobj *_obj_create_sds_string(const char *str, size_t len);
static arobj *_obj_create_embedded_sds_string(const char *str, size_t len);
static arobj *_obj_make_shared(arobj *o);
static void _obj_free_string(arobj *o);

// Global shared objects in the server.
struct shared_objs shared;

/* TODO to be continued, 5.8
static dict_type set_dict_type = {
    dict_sample_hash,                   // hash
    NULL,                               // key dup
    NULL,                               // val dup
    dict_sample_compare_sds_key,        // key compare
    dict_sample_free_sds,               // key destruct, remove a key from set
    NULL // val destruct, note there is no val in set, namely no val in dict_entry of set
}; */

// Create shared objects. Called in server_init().
void obj_create_shared()
{
    // create shared int objects
    int min_val = - (OBJ_SHARED_INTEGERS >> 1);
    for(int i = 0; i < OBJ_SHARED_INTEGERS; i ++) {
        shared.integers[i] = _obj_make_shared(obj_create_string_from_ll_withoption(min_val + i, 0));
    }
}

// Make object 'o' shared.
// Note that inc_ref() and dec_ref() won't touch an object's refcount if it is shared.
// Though no need to return the object 'o', doing this can make code look nicer.
static arobj *_obj_make_shared(arobj *o)
{
    server_assert(o->ref_count == 1);
    o->ref_count = OBJ_SHARED_REFCOUNT;
    return o;
}

// Create an object with specified 'type', 'encoding' and 'ptr'.
arobj *obj_create(int type, int encoding, void *ptr)
{
    arobj *o = malloc(sizeof(arobj));
    o->type = type;
    o->encoding = encoding;
    o->lru = 0;
    o->ref_count = 1;
    o->ptr = ptr;
    return o;
}

// Create a string object. Using SDSs or EMBSTR encoding depending on 'len'.
#define EMBSTR_MAX_LENGTH 44
arobj *obj_create_string(const char *str, size_t len)
{
    if (len < EMBSTR_MAX_LENGTH) {
        return _obj_create_embedded_sds_string(str, len);
    } else {
        return _obj_create_sds_string(str, len);
    }
}

// Create a sds string object from 'str' of length 'len'.
static arobj *_obj_create_sds_string(const char *str, size_t len)
{
    arobj *o = malloc(sizeof(arobj));

    o->type = OBJ_TYPE_STRING;
    o->encoding = OBJ_ENC_SDS;
    o->lru = 0;             // TODO future work. LRU mechanism for arobj.
    o->ref_count = 1;
    o->ptr = sds_new_len(str, len);

    return o;
}

// Create a embedded sds string object from 'str' of lengh 'len'.
// If the string is non-modifiable, for exmample, as a key, it's recommended that
// you use embeded str instead of sds str, since it is more compact and may run faster.
static arobj *_obj_create_embedded_sds_string(const char *str, size_t len)
{
    arobj *o = malloc(sizeof(arobj) + sizeof(sds_hdr_8) + len + 1);
    sds_hdr_8 *sh = (void*)(o + 1);
    // init obj
    o->type = OBJ_TYPE_STRING;
    o->encoding = OBJ_ENC_EMBSDS;
    o->lru = 0;
    o->ref_count = 1;
    o->ptr = sh + 1;
    // init embeded sds string
    sh->len = len;
    sh->alloc = len;
    sh->flag = SDS_TYPE_8;
    if (str == SDS_NOINIT) {
        sh->buf[len] = '\0';
    } else if (str) {
        memcpy(sh->buf, str, len);
        sh->buf[len] = '\0';
    } else {
        memset(sh->buf, 0, len + 1);
    }

    return o;
}

// Create a string obj with a long long 'val'.
arobj *obj_create_string_from_ll(long long val)
{
    return obj_create_string_from_ll_withoption(val, 1);
}

// Create or share a string obj with a long long 'val'.
//
// If try_shared is true, try to return an obj from shared integers if any.
// or create one if not in shared integer range.
//
// If try_shared not true, alwasy create a new string obj.
arobj *obj_create_string_from_ll_withoption(long long val, int try_shared)
{
    // try shared if in range
    if (try_shared) {
        int min = - (OBJ_SHARED_INTEGERS >> 1);
        int max = OBJ_SHARED_INTEGERS >> 1;
        if (min <= val && val <= max) {
            return shared.integers[val - min];
        }
    }

    // If the size of the pointer 'ptr' cannot hold the long long value, we revert
    // to string representation for the val. However, in 64bit machine, this can't
    // happen because in that case the pointer size equals to long long size.
    arobj *o;
    if (val >= LONG_MIN && val <= LONG_MAX) {
        o = obj_create(OBJ_TYPE_STRING, OBJ_ENC_INT, (void*)val);
    } else {
        o = obj_create(OBJ_TYPE_STRING, OBJ_ENC_SDS, sds_from_longlong(val));
    }
    return o;
}

// Duplicate a string object. New object's encoding is the same as the original's.
arobj *obj_dup_string(const arobj *o)
{
    server_assert(o->type == OBJ_TYPE_STRING);

    switch(o->type) {
    case OBJ_ENC_SDS:
        return _obj_create_sds_string(o->ptr, sds_len(o->ptr));
    case OBJ_ENC_EMBSDS:
        return _obj_create_embedded_sds_string(o->ptr, sds_len(o->ptr));
    case OBJ_ENC_INT:
        return obj_create(OBJ_TYPE_STRING, OBJ_ENC_INT, o->ptr);
    default:
        server_panic("Wrong string encoding");
        return NULL;
    }
}

// Increse reference count of obj 'o'
void obj_inc_ref(arobj *o)
{
    if (o->ref_count != OBJ_SHARED_REFCOUNT) o->ref_count ++;
}

// Decrease reference count of obj 'o' and free it if needed
void obj_dec_ref(arobj *o)
{
    // Cases most likely to happen
    if (o->ref_count > 1 && o->ref_count != OBJ_SHARED_REFCOUNT) {
        o->ref_count --;
        return;
    }
    // Cases sometimes happen
    if (o->ref_count == 1) {
        switch(o->type) {
            case OBJ_TYPE_STRING: _obj_free_string(o); break;
            //case OBJ_TYPE_LIST: ...; break;
            //case OBJ_TYPE_SET: ...; break;
            default: server_panic("Unknown object type"); break;
        }
        o->ref_count = -100;
        free(o);
        return;
    }
    // Cases rarely happen! Panic!
    if (o->ref_count <= 0) {
        server_panic("obj_dec_ref() bad ref_count %d", o->ref_count);
    }
}

static void _obj_free_string(arobj *o)
{
    if (o->encoding == OBJ_ENC_SDS) sds_free(o->ptr);
}



#ifndef DICT_H_INCLUDED
#define DICT_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

#define DICT_OK 0
#define DICT_ERR 1

// Dict entry that holds a key-value pair
typedef struct dict_entry
{
    void *key;
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    struct dict_entry *next;
} dict_entry;

// Dict type with methods for various dict operations
typedef struct dict_type
{
    uint64_t (*hash_func)(const void *key);
    void *(*key_dup_func)(const void *key);
    void *(*val_dup_func)(const void *val);
    int (*key_compare_func)(const void *key1, const void *key2);
    void (*key_destructor_func)(void *key);
    void (*val_destructor_func)(void *val);
} dict_type;

//  Dict hash table that holds meta data about dict entry table.
typedef struct dict_ht
{
    dict_entry **table;         // points to dict entry table
    unsigned long size;         // num of slots in the table, always power of 2
    unsigned long size_mask;    // size - 1
    unsigned long keys;         // num of dict entries(keys) used in the table
} dict_ht;

// Dict structure. Every dict has two dict_ht for rehashing.
typedef struct dict
{
    dict_type *type;
    dict_ht ht[2];
    long rehash_idx;
    unsigned long num_safe_iterators;
} dict;

// Safe iterator can modify dict while tranversing, whereas unsafe iterator cannot and
// use of it are checked against fingerprint if it ever modifies the dict.
typedef struct dict_iterator
{
    dict *d;                // the dict it refereces to
    int table;              // table index of the dict
    long index;             // slot index of the table
    dict_entry *entry;      // current entry in the slot that the iterator points to

    int safe;              // Is the iterator safe or not
    long long fingerprint; // Fingerprint for unsafe iterator
} dict_iterator;

// Initial size of every dict hash table
#define DICT_HT_INITIAL_SIZE 4
// Macros
#define dict_set_val(d, entry, _val_) do {\
    if ((d)->type->val_dup_func) \
        (entry)->v.val = (d)->type->val_dup_func(_val_); \
    else \
        (entry)->v.val = (_val_); \
    } while(0)

#define dict_set_signed_integer_val(entry, _val_)       ((entry)->v.s64 = _val_)
#define dict_set_unsigned_integer_val(entry, _val_)     ((entry)->v.u64 = _val_)
#define dict_set_double_val(entry, _val_)               ((entry)->v.d = _val_)

#define dict_free_val(d, entry) \
    if ((d)->type->val_destructor_func) \
        (d)->type->val_destructor_func((entry)->v.val)

#define dict_set_key(d, entry, _key_) do { \
    if ((d)->type->key_dup_func) \
        (entry)->key = (d)->type->key_dup_func(_key_); \
    else \
        (entry)->key = (_key_); \
    } while(0)

#define dict_free_key(d, entry)  \
    if ((d)->type->key_destructor_func) \
        (d)->type->key_destructor_func((entry)->key)

#define dict_compare_key(d, key1, key2) \
    (((d)->type->key_compare_func) ? \
        (d)->type->key_compare_func(key1, key2) : (key1) == (key2))

#define dict_hash_key(d, key)                   ((d)->type->hash_func(key))
#define dict_get_key(de)                        ((de)->key)
#define dict_get_val(de)                        ((de)->v.val)
#define dict_get_signed_integer_val(de)         ((de)->v.s64)
#define dict_get_unsigned_integer_val(de)       ((de)->v.u64)
#define dict_get_double_val(de)                 ((de)->v.d)
#define dict_size(d)                            ((d)->ht[0].size + (d)->ht[1].size)
#define dict_keys(d)                            ((d)->ht[0].keys + (d)->ht[1].keys)
#define dict_is_rehashing(d)                    ((d)->rehash_idx != -1)

// Function declarations
dict *dict_create(dict_type *type);
int dict_resize_to(dict *d, unsigned long new_size);
int dict_rehash(dict *d, int n);
dict_entry *dict_accommodate_key(dict *d, void *key, dict_entry **existing_entry);
int dict_add_entry(dict *d, void *key, void *val);
int dict_add_or_replace_entry(dict *d, void *key, void *val);
int dict_delete(dict *d, const void *key);
dict_entry *dict_unlink(dict *d, const void *key);
int dict_free_unlinked_entry(dict *d, dict_entry *de);
void *dict_fetch_value(dict *d, const void *key);
dict_entry *dict_find(dict *d, const void *key);
dict_iterator *dict_get_iterator(dict *d);
dict_iterator *dict_get_safe_iterator(dict *d);
dict_entry *dict_next(dict_iterator *iter);
int dict_free_iterator(dict_iterator *iter);
void dict_enable_resize();
void dict_disable_resize();
void dict_get_stats(char *buf, size_t buf_size, dict *d);
void dict_hash_seed_init();
// Some sample functions for building dict_type
uint64_t dict_sample_hash(const void *key);
uint64_t dict_sample_case_hash(const void *key);
int dict_sample_compare_sds_key(const void *key1, const void *key2);
int dict_sample_compare_sds_key_case(const void *key1, const void *key2);
void dict_sample_free_sds(void *val);
void dict_sample_free_obj(void *o);


extern dict_type sample_dict_type;
extern uint8_t dict_hash_seed[17];
extern uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
extern uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);


#endif // DICT_H_INCLUDED

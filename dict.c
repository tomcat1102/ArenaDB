/*
    ArenaDB dict implementation. 4-29 4-30
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include "config.h"
#include "dict.h"
#include "sds.h"
#include "util.h"
#include "debug.h"
#include "log.h"

// Hash seed for dict hash function.
// Initialized when server starts. Never modify it during server running.
uint8_t dict_hash_seed[17];

void dict_hash_seed_init()
{
    util_get_random_hex_chars((char*)dict_hash_seed, 16);

    dict_hash_seed[16] = '\0';  // now printable
    server_log(LL_DEBUG, "Dict hash seed: %s", (char*)dict_hash_seed);
}

// To enable/disable resize of hash tables, we can use dict_enable_resize() and
// dict_disable_resize().
//
// Note that even if resize is disabled, some hash tables may resize if the ratio between
// num of entries and slot in those table > dict_force_resize_ratio.
static int dict_can_resize = 1;
static int dict_force_resize_ratio = 5;

// Static function delarations
static void _dict_ht_reset(dict_ht *ht);
static int _dict_ht_clear(dict *d, dict_ht *ht, void (callback)(void));
static unsigned long _dict_next_power(unsigned long size);
static void _dict_rehash_1_step(dict *d);
static int _dict_expand_if_needed(dict *d);
static dict_entry *dict_generic_delete(dict *d, const void *key, int do_free);
static int _dict_ht_clear(dict *d, dict_ht *ht, void (callback)(void));

// Reset a dict hast table 'ht'.
static void _dict_ht_reset(dict_ht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->size_mask = 0;
    ht->keys = 0;
}

// Return next power of 2 that is just greater than 'size'
static unsigned long _dict_next_power(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;
    if (size == ULONG_MAX) return size;

    while (i < size) {
        i <<= 1;
    }
    return i;
}

// Create a new dict.
dict *dict_create(dict_type *type)
{
    server_assert(type != NULL);

    dict *d = malloc(sizeof(dict)); // malloc alwasy succeed
    _dict_ht_reset(&d->ht[0]);
    _dict_ht_reset(&d->ht[1]);
    d->type = type;
    d->rehash_idx = -1;
    d->num_safe_iterators = 0;
    return d;
}

// Resize dict 'd' to the specified 'new_size'. And begin incremental rehashing.
// Note that the hash table may expand as well as shrink based on 'new_size'.
int dict_resize_to(dict *d, unsigned long new_size)
{
    // Cannot resize if rehashing
    if (dict_is_rehashing(d)) // TODO 'd->ht[0].used >= new_size' is OK I think
        return DICT_ERR;

    unsigned long ht_size = _dict_next_power(new_size);
    // No need to resize to the original size
    if (ht_size == d->ht[0].size) return DICT_ERR;

    // Alloc new hash table
    dict_ht ht;
    ht.table = calloc(ht_size, sizeof(dict_entry*)); // Calloc to set table entries to zero (NULL)
    ht.size = ht_size;
    ht.size_mask = ht_size - 1;
    ht.keys = 0;
    // If this is the first allocation, just set the first hast table
    if (d->ht[0].table == NULL) {
        d->ht[0] = ht;
        return DICT_OK;
    }
    // Else prepare a second hast table for incremental rehashing
    d->ht[1] = ht;
    d->rehash_idx = 0;
    return DICT_OK;
}

// Perform a single step of rehashing if there is no safe_iterators bound to
// the dict 'd', otherwise the dict may be in inconsistent state if
// safe_iterators are used to modify the dict where the dict is rehashing.
//
//  Called every time by common lookup and update ops to gradually complete rehashing.
static void _dict_rehash_1_step(dict *d)
{
    if (d->num_safe_iterators == 0) dict_rehash(d, 1);
}

// Perform N step of incremental rehashing. Each step handles a rehashing for a slot.
// Return 1 if there are still keys to remove(rehash), otherwise 0.
int dict_rehash(dict *d, int n)
{
    int max_empty_slot = n * 10; // Max allowable empty slot to visit
    if (!dict_is_rehashing(d)) return 0;

    while (n -- && d->ht[0].keys != 0) {
        assert(d->rehash_idx <= d->ht[0].size); // Rehash_idx can't overflow
        // Find first non-empty slot, or return if reach max allowable empty slot
        dict_entry *de, *de_next;
        while ((de = d->ht[0].table[d->rehash_idx]) == NULL) {
            d->rehash_idx ++;
            if (--max_empty_slot == 0) return 1;
        }
        // Now remove all keys in the slot from ht[0] to ht[1]
        while(de) {
            de_next = de->next;

            unsigned long idx = dict_hash_key(d, de->key) & d->ht[1].size_mask;
            de->next = d->ht[1].table[idx];
            d->ht[1].table[idx] = de;
            de = de_next;
            // update metadata for both hash tables
            d->ht[0].keys --;
            d->ht[1].keys ++;
        }
        d->ht[0].table[d->rehash_idx] = NULL;
        d->rehash_idx ++;
    }
    // Check whether all keys in ht[0] have been rehashed. If so, swap the two tables
    if (d->ht[0].keys == 0) {
        free(d->ht[0].table);
        d->ht[0] = d->ht[1];
        _dict_ht_reset(&d->ht[1]);
        d->rehash_idx = -1; // rehashing ended
        return 0;
    }
    // More to rehash...
    return 1;
}

// Expand hash table if neeed.
// Called before adding new entries to table.
static int _dict_expand_if_needed(dict *d)
{
    // If rehashing, maybe expansion (or reduction) is in progress. Return.
    if (dict_is_rehashing(d)) return DICT_OK;
    // If hast table is empty, resize (expand) it to the initial size.
    if (d->ht[0].size == 0) return dict_resize_to(d, DICT_HT_INITIAL_SIZE);
    // If we need to expand and, we can expand or must expand, expand it !
    if (d->ht[0].keys >= d->ht[0].size && (dict_can_resize ||
        (d->ht[0].keys / d->ht[0].size) > dict_force_resize_ratio)) {
        return dict_resize_to(d, d->ht[0].keys * 2);
     }
    return DICT_OK;
}

// Provide a dict_entry to accommdate 'key'.
// If 'key' is not in the dict, the function return a new dict_entry to accommdate 'key'.
// If 'key' is in the dict, the function returns NULL and the entry that holds 'key' is
// returned through 'existing_entry'.
dict_entry *dict_accommodate_key(dict *d, void *key, dict_entry **existing_entry)
{
    if (dict_is_rehashing(d)) _dict_rehash_1_step(d);

    _dict_expand_if_needed(d);

    // Fist scan the slot in ht[0]
    unsigned long hash = dict_hash_key(d, key);
    unsigned long idx = hash & d->ht[0].size_mask;

    dict_entry *de = d->ht[0].table[idx];
    while(de) {
        if (key == de->key || dict_compare_key(d, key, de->key)) { // If match, always return
            if (existing_entry) *existing_entry = de;
            return NULL;
        }
        de = de->next;
    }
    if (!dict_is_rehashing(d)) { // If not rehashing, create a new entry in ht[0] and return it
        de = malloc(sizeof(dict_entry));
        de->next = d->ht[0].table[idx];
        d->ht[0].table[idx] = de;
        d->ht[0].keys ++;
        return de;
    }
    // Then scan the slot in ht[1] if rehashing
    idx = hash & d->ht[1].size_mask;
    de = d->ht[1].table[idx];
    while(de) {
        if (key == de->key || dict_compare_key(d, key, de->key)) {
            if (existing_entry) *existing_entry = de;
            return NULL;
        }
        de = de->next;
    }
    // If not found in ht[1], always create a new entry, insert it to ht[1] and return it.
    de = malloc(sizeof(dict_entry));
    de->next = d->ht[1].table[idx];
    d->ht[1].table[idx] = de;
    d->ht[1].keys ++;
    return de;
}

// Add an entry to dict 'd'.
int dict_add_entry(dict *d, void *key, void *val)
{
    dict_entry * new_entry = dict_accommodate_key(d, key, NULL);

    if (!new_entry) return DICT_ERR;

    dict_set_key(d, new_entry, key);
    dict_set_val(d, new_entry, val);

    return DICT_OK;
}

// If 'key' not exists, add a new entry to dict 'd'. Return 1.
// If 'key' exists, replace the value of the entry with 'val'. Return 0.
int dict_add_or_replace_entry(dict *d, void *key, void *val)
{
    dict_entry *new_entry, *existing_entry = NULL, anx_entry;

    new_entry = dict_accommodate_key(d, key, &existing_entry);
    if (new_entry) {
        dict_set_key(d, new_entry, key);
        dict_set_val(d, new_entry, val);
        return 1;
    }

    assert(existing_entry != NULL); // Existing_entry cannot be NULL if new_entry is NULL
    // Cannot free val directly since the val in existing_entry may be the same as 'val'
    anx_entry = *existing_entry;
    dict_set_val(d, existing_entry, val);
    dict_free_val(d, &anx_entry);
    return 0;
}

// Fetch value of the entry with 'key' in dict 'd'.
void *dict_fetch_value(dict *d, const void *key)
{
    dict_entry *de = dict_find(d, key);
    return de ? dict_get_val(de) : NULL;
}

// Find the entry with 'key' in dict 'd'.
dict_entry *dict_find(dict *d, const void *key)
{
    if (d->ht[0].keys + d->ht[1].keys == 0) return NULL;    // dict is empty
    if (dict_is_rehashing(d)) _dict_rehash_1_step(d);        // rehash if dict is rehashing and there are keys in dict

    unsigned long idx, hash = dict_hash_key(d, key);


    for(int table = 0; table <= 1; table ++) {
        idx = hash & d->ht[table].size_mask;
        //printf("dict_find(): table:%d, hash:%lu, idx:%lu, key:%s \n", table, hash, idx, (char*)key); //TODO printf
        dict_entry *de = d->ht[table].table[idx];
        while (de) {
            if (de->key == key || dict_compare_key(d, de->key, key)) {
                return de;
            }
            de = de->next;
        }
        if (!dict_is_rehashing(d)) return NULL;
    }
    return NULL;
}

// Delete the entry with 'key' from dict 'd'. Return DICT_OK if deteled, otherwise DICT_ERR.
int dict_delete(dict *d, const void *key)
{
    return dict_generic_delete(d, key, 1) ? DICT_OK : DICT_ERR;
}

// Unlink the entry with 'key' from dict 'd'. The returned entry may be freed later.
dict_entry * dict_unlink(dict *d, const void *key)
{
    return dict_generic_delete(d, key, 0);
}

// Free the entry 'de' that is unlinked from dict 'd'.
int dict_free_unlinked_entry(dict *d, dict_entry *de)
{
    if (de == NULL) return DICT_ERR;

    dict_free_key(d, de);
    dict_free_val(d, de);
    printf("de addr: 0x%lx \n", (unsigned long)de);
    fflush(stdout);
    free(de);

    return DICT_OK;
}

// Generic delele helper function for dict_delete() and dict_unlink().
// It searches and removes an entry from the dict 'd'.
static dict_entry *dict_generic_delete(dict *d, const void *key, int do_free)
{
    if (d->ht[0].keys == 0 && d->ht[1].keys == 0) return NULL;
    if (dict_is_rehashing(d)) _dict_rehash_1_step(d); // Perform 1 step rehashing if there are entries and the dict is rehashing.

    unsigned long idx, hash = dict_hash_key(d, key);
    dict_entry *de, *prev_de;
    for (int table = 0; table <= 1; table ++) {
        idx = hash & d->ht[table].size_mask;
        de = d->ht[table].table[idx];
        prev_de = NULL;
        while(de){
            if (de->key == key || dict_compare_key(d, de->key, key)) {
                // Unlink the entry from the entry chain in this slot
                if (prev_de) {
                    prev_de->next = de->next;
                } else {
                    d->ht[table].table[idx] = de->next;
                }
                // Free if 'free' is true
                if (do_free) {
                    dict_free_key(d, de);
                    dict_free_val(d, de);
                    free(de);
                }
                d->ht[table].keys --;
                return de;
            }

            prev_de = de;
            de = de->next;
        }
        if (!dict_is_rehashing(d)) break;
    }
    return NULL;
}

// Clear both hash tables and release the dict 'd'
void dict_release(dict *d)
{
    _dict_ht_clear(d, &d->ht[0], NULL);
    _dict_ht_clear(d, &d->ht[1], NULL);
    free(d);
}

// Clear(release) an entire hast table and its entries
static int _dict_ht_clear(dict *d, dict_ht *ht, void (callback)(void))
{
    if (ht->table == NULL) return DICT_OK;

    for (unsigned long i = 0; i < ht->size && ht->keys > 0; i ++) {
        dict_entry *de, *next_de;
        // Invoke user-defined callback funtion if any. Possibly for debug purposes or to display current info.
        if (callback && (i & 65535) == 0) callback();

        if ((de = ht->table[i]) == NULL) continue;
        while (de) {
            next_de = de->next;
            // free current entry
            dict_free_key(d, de);
            dict_free_val(d, de);
            free(de);

            de = next_de;
            ht->keys --;
        }
    }
    server_assert(ht->keys == 0); // Now there should be no keys in the hash table
    free(ht->table);
    _dict_ht_reset(ht);
    return DICT_OK; // Never fails
}

// Return fingerprint of the dict that is used to detect misuse of unsafe iterator
long long dict_fingerprint(dict *d)
{
    long long integers[6], hash = 0;

    integers[0] = (long)d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].keys;
    integers[3] = (long)d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].keys;
    // I don' know this. It's Tomas Wang's 64 bit integer hash.
    for(int i = 0; i < 6; i ++) {
        hash += integers[i];
        hash = (~hash) + (hash << 21);
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 21);
    }
    return hash;
}

// Get a unsafe iterator for dict 'd'
dict_iterator *dict_get_iterator(dict *d)
{
    dict_iterator *iter = malloc(sizeof(dict_iterator));

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    return iter;
}

// Get a safe iterator for dict 'd'
dict_iterator *dict_get_safe_iterator(dict *d)
{
    dict_iterator *iter = dict_get_iterator(d);

    iter->safe = 1;
    return iter;
}

dict_entry *dict_next(dict_iterator *iter)
{
    dict_entry *de = iter->entry;

    while (1)
    {
        if (de == NULL) {
            dict_ht * ht = &iter->d->ht[iter->table];
            // dict_next first called ?
            if (iter->index == -1 && iter->table == 0) {
                if (iter->safe) {
                    iter->d->num_safe_iterators ++;
                } else {
                    iter->fingerprint = dict_fingerprint(iter->d);
                }
            }
            // No entry in current slot, iterate to the next slot
            iter->index ++;

            if (iter->index >= (long)ht->size) {
                // No entry in first hash table, switch to the second if rehashing
                if (dict_is_rehashing(iter->d) && iter->table == 0) {
                    iter->table = 1;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break; // No such entry
                }
            }
            // Iterate from the begining of the next slot
            de = ht->table[iter->index];
        } else {
            iter->entry = de->next;
            return de;
        }
    }
    return NULL;
}

int dict_free_iterator(dict_iterator *iter)
{   // Update dict or check fingerprint if dict_next() has been called on 'iter'
    if (!(iter->index == -1 && iter->table == 0)) {
        if (iter->safe) {
            iter->d->num_safe_iterators --;
            server_assert(iter->d->num_safe_iterators >= 0);    // number of safe iterators must >= 0
        } else {                     // fingerprint must be the same before and after unsafe iteration
            server_assert(iter->fingerprint == dict_fingerprint(iter->d));
        }
    }
    free(iter);
    return DICT_OK;
}

void dict_enable_resize()
{
    dict_can_resize = 1;
}

void dict_disable_resize()
{
    dict_can_resize = 0;
}

// Some sample helper functions for constructing dict_type, like test_dict_type, db_dict_type, etc.
uint64_t dict_sample_hash(const void *key)
{
    return siphash((const uint8_t*)key, sds_len((const sds)key), dict_hash_seed);
}
uint64_t dict_sample_case_hash(const void *key)
{
    return siphash_nocase((const uint8_t*)key, sds_len((const sds)key), dict_hash_seed);
}
// Case sensitive care. Used in database for key-obj lookup. Return 1 if same, 0 otherwise
int dict_sample_compare_sds_key(const void *key1, const void *key2)
{
    int len_1, len_2;
    len_1 = sds_len((const sds)key1);
    len_2 = sds_len((const sds)key2);
    if (len_1 != len_2) return 0;
    return memcmp(key1, key2, len_1) == 0;
}
// Case insensitive compare. Used in command table for fast command loopup. Note it's non binary-safe.
int dict_sample_compare_sds_key_case(const void *key1, const void *key2)
{
    return strcasecmp(key1, key2) == 0;
}

void dict_sample_free_sds(void *val)
{
    sds_free(val);
}

void dict_sample_free_obj(void *o)
{
    if (o != NULL) obj_dec_ref(o);
}
// A sample dict type used for dict test and benchmark. See test.c and benchmark.c
dict_type sample_dict_type = {
    dict_sample_hash,               // key hash
    NULL,                           // key dup
    NULL,                           // val dup
    dict_sample_compare_sds_key,    // key compare
    dict_sample_free_sds,           // key destructor
    NULL,                           // val destructor
};





/*
    ArenaDB debug implementation. 5.4
*/

/*
*   Various debug functions go here. These functions can panic, assert, and print object
*   info of various kinds. The output all goes to log with log level being LL_DEBUG mostly.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "obj.h"
#include "sds.h"
#include "dict.h"
#include "command.h"
#include "log.h"

static size_t _server_debug_dict_get_ht_stats(char *buf, size_t buf_size, dict_ht *ht, int table_id);
static void server_debug_string(arobj *o);


// Though the names of these function, by default, should start with 'debug', I think
// its more approriate to call them 'server_xxxx'. And though server_xxx() functions
// should be put in server.c, I think it is somewhat better to put them here.
void _server_assert(const char *cond_str, const char* path, int line)
{
    // Make 'file_name' points to the last component of the 'path'
    const char *file_name = path + strlen(path);
    while(file_name >= path) {
        if (*file_name == '/') break;
        file_name --;
    }
    file_name ++; // skip last '/'

    // Log assertion info
    server_log(LL_ERROR, "==== ASSERTION FAILED ====");
    server_log(LL_ERROR, "In %s:%d, assertion '%s' not true", file_name, line, cond_str);
    server_log(LL_VERBOSE, "Server exiting...");

    exit(1);
}

void _server_panic(const char *path, int line, const char *msg, ...) {
    // Format panic message
    va_list ap;
    va_start(ap, msg);
    char fmt_msg[256];
    vsnprintf(fmt_msg, sizeof(fmt_msg), msg, ap);
    va_end(ap);

    // Make 'file_name' points to the last component of the 'path'
    const char *file_name = path + strlen(path);
    while(file_name >= path) {
        if (*file_name == '/') break;
        file_name --;
    }
    file_name ++; // skip last '/'
    // Log message
    server_log(LL_ERROR, "==== SERVER PANIC ====");
    server_log(LL_ERROR, "In %s:%d, panic %s", file_name, line, fmt_msg);

    exit(1);
}

static void server_debug_string(arobj *o)
{
    switch(o->encoding) {
        case OBJ_ENC_SDS:{
            sds s = o->ptr;
            server_log(LL_RAW, "(Debug) TYPE: string, ENC: sds, REF: %d, SDS_LEN: %d, SDS_AVAIL: %d \n",
             o->ref_count, sds_len(s), sds_avail(s));
             break;
        }
        case OBJ_ENC_EMBSDS:{
            sds s = o->ptr;
            server_log(LL_RAW, "(Debug) TYPE: string, ENC: embsds, REF: %d, SDS_LEN: %d, SDS_AVAIL: %d \n",
             o->ref_count, sds_len(s), sds_avail(s));
             break;
        }
        case OBJ_ENC_INT: {
            server_log(LL_RAW, "(Debug) TYPE: string, ENC: int, REF: %d, INT_VAL: %ld \n",
             o->ref_count, (long)o->ptr);
             break;
        }
    }
}

// Debug an arobj. Call lowlevel debug function for specific types.
void server_debug_obj(arobj *o)
{
    if (o == NULL) {
        server_log(LL_RAW, "server_debug_obj(): o is NULL! \n");
        return;
    }
    switch(o->type) {
    case OBJ_TYPE_STRING:
        server_debug_string(o); break;
    default:
        server_log(LL_DEBUG, "server_debug_obj() Unknown object type"); break;
    }
}

// Debug command 'c'.
void server_debug_command(command *c)
{
    if (c == NULL) {
        server_log(LL_RAW, "server_debug_command(): c is NULL! \n");
        return;
    }
    server_log(LL_RAW, "server_debug_command(): id: %d, name: %s, command_proc: cmd_%s(), arity: %d \n",
        c->id, c->name, c->name, c->arity);
}

// Debug dict 'd'. Print its hash tables.
void server_debug_dict(dict *d)
{
    if (d == NULL) {
        server_log(LL_RAW, "server_debug_dict(): d is NULL! \n");
        return;
    }

    server_log(LL_RAW, "server_debug_dict(): \n");
    server_log(LL_RAW, "rehash_idx: %ld \n", d->rehash_idx);

    dict_ht *ht;
    // print ht[0]
    ht = &d->ht[0];
    server_log(LL_RAW, "ht[0]: size=%lu, keys=%lu, table=%s \n", ht->size, ht->keys, (ht->table == NULL) ? "null" : "(below)");
    if (ht->table != NULL) {
        size_t size = ht->size;
        for(int i = 0; i < size; i ++) {
            server_log(LL_RAW, "slot %2d: ", i);
            dict_entry *de = ht->table[i];
            while(de) {
                server_log(LL_RAW, "%2s:%-2ld ", (char*)dict_get_key(de), dict_get_signed_integer_val(de));
                de = de->next;
            }
            putchar('\n');
        }
    }
    // print ht[1]
    ht = &d->ht[1];
    server_log(LL_RAW, "ht[1]: size=%lu, keys=%lu, table=%s \n", ht->size, ht->keys, (ht->table == NULL) ? "null" : "(below)");
    if (ht->table != NULL) {
        size_t size = ht->size;
        for(int i = 0; i < size; i ++) {
            server_log(LL_RAW, "slot %2d: ", i);
            dict_entry *de = ht->table[i];
            while(de) {
                server_log(LL_RAW, "%2s:%-2ld ", (char*)dict_get_key(de), dict_get_signed_integer_val(de));
                de = de->next;
            }
            putchar('\n');
        }
    }
    putchar('\n');
}

void server_debug_dict_get_stats(char *buf, size_t buf_size, dict *d)
{
    char *orig_buf = buf;
    size_t orig_buf_size = buf_size;

    size_t l = _server_debug_dict_get_ht_stats(buf, buf_size, &d->ht[0], 0);
    buf += l;
    buf_size -= l;
    if (dict_is_rehashing(d) && buf_size >= 0) {
        _server_debug_dict_get_ht_stats(buf, buf_size, &d->ht[1], 1);
    }
    if (orig_buf_size) orig_buf[orig_buf_size] = '\0';
}

static size_t _server_debug_dict_get_ht_stats(char *buf, size_t buf_size, dict_ht *ht, int table_id)
{
    if (ht->keys == 0) {
        return snprintf(buf, buf_size, "No stats available for empty hash table\n");
    }

    #define HISTO_LEN 50
    // Length histogram that displays the count of slot chain with increasing length.
    // The first count shows the count of slot chains with zero length (no entries).
    // The last count shows the count of slot chains with length more than 50.
    unsigned long histogram[HISTO_LEN];
    for(int i = 0; i < HISTO_LEN; i ++) histogram[i] = 0;

    unsigned long num_slots = 0;            // number of non-empty slot
    unsigned long num_entries;              // number of entries on current slot
    unsigned long max_entries = 0;          // max number of entries found in slot chains
    unsigned long total_entries = 0;        // total number of entries in the hash table
    for(int i = 0; i < ht->size; i ++) {
        dict_entry *de;
        if ((de = ht->table[i]) == NULL) { // empty slot
            histogram[0] ++;
            continue;
        }
        // slot with entries to count
        num_slots ++;
        num_entries = 0;
        while (de) {
            num_entries ++;
            de = de->next;
        }
        // Update histogram info
        histogram[(num_entries < HISTO_LEN) ? num_entries : (HISTO_LEN - 1)] ++;
        if (num_entries > max_entries) max_entries = num_entries;
        total_entries += num_entries;
    }

    size_t l = 0; // buf used for printing
    l += snprintf(buf + l, buf_size - l,
        "Hash table %d stats (%s): \n"
        " table size: %lu \n"
        " num of entries: %lu \n"
        " num of non-empty slots: %lu \n"
        " max entries per slot: %lu \n"
        " avg entries per slot (counted) : %.02f \n"
        " avg entries per slot (computed): %.02f \n"
        " Distribution of slot length: \n",
        table_id, (table_id == 0) ? "main hash table" : "rehashing table",
        ht->size, ht->keys, num_slots, max_entries,
        (float)total_entries/num_slots, (float)ht->keys/num_slots);

    for (int i = 0; i < HISTO_LEN; i ++) {
        if (l >= buf_size) break;
        l += snprintf(buf + l, buf_size - l,
            "  %s%d: %ld (%.02f%%) \n",
            (i == HISTO_LEN - 1) ? ">=" : "",
            i, histogram[i], ((float)histogram[i]/ht->size) * 100);
    }

    if (buf_size) buf[buf_size - 1] = '\0';
    return strlen(buf);
}


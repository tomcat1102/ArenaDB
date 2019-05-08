/*
    ArenaDB database implementation. 5.2
*/

/*
* A database is a key-value space in ArenaDB server. The server can have multiple databases.
* These database are used as look-up dictionaries. They store key-value entries with key being an sds
* string and value being an object.The object may be a string(sds string), a list ,a dict, etc.
*
* When a user issues commond "set name apple", a corresponding entries is inserted to default db.
* When a user issues commond "get name", a look-up is perfomed on the db, and "apple" is returned as expected.
*/
#include <stdio.h>
#include "server.h"
#include "dict.h"
#include "obj.h"
#include "db.h"
#include "debug.h"

// The dict type used for databases in ArenaDB server. Keys are sds string, val are also sds string
// TODO val should support other data types, in additon to sds.
dict_type db_dict_type = {
    dict_sample_hash,               // hash
    NULL,                           // key dup
    NULL,                           // val dup
    dict_sample_compare_sds_key,    // key compare
    dict_sample_free_sds,           // key destruct
    dict_sample_free_obj            // val destruct
};

void db_init()
{
    server.db = malloc(sizeof(database) * server.num_db);
    for(int i = 0; i < server.num_db; i ++) {
        server.db[i].d = dict_create(&db_dict_type);
    }
}

arobj *db_lookup_key(database *db, arobj *key)
{
    dict_entry *de = dict_find(db->d, key);
    if (de) {
        arobj *val = dict_get_val(de);
        return val;
    } else {
        return NULL;
    }
}




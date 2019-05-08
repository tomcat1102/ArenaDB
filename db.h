#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

#include "dict.h"

typedef struct database{
    dict *d;        // key-value space. key is always of type string
    int id;
} database;

// Function declarations
void db_init();

extern dict_type db_dict_type;
extern database *db;


#endif // DB_H_INCLUDED

/*
*   ArenaDB command interfaces. 5.4
*/

/*
*   All command interfaces are specified in this file. However, some of the
*   command implementations may be scatterd in several other files. For example,
*   the underlying implementation of 'set' command is in db.c and dict.c.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/select.h>
#include "server.h"
#include "obj.h"
#include "net.h"
#include "debug.h"
#include "util.h"
#include "log.h"

static command *command_lookup(sds cmd_name);
//static command *command_lookup_cstring(const char* cmd_cname);

static void cmd_get(client *c);
static void cmd_set(client *c);
static void cmd_del(client *c);
static void cmd_exist(client *c);
//static void cmd_hset(client *c);
static void cmd_time(client *c);
static void cmd_exit(client *c);

static command cmd_table[] = {
    // string commands
    {0, "get", cmd_get, 2},
    {0, "set", cmd_set, 3},
    {0, "del", cmd_del, 2},
    {0, "exist", cmd_exist, 2},
    // hash commands
    //{0, "hset", cmd_hset, 4}, ZIPLIST needed

    // miscellaneous commands
    {0, "exit", cmd_exit, 1},
    {0, "time", cmd_time, 1}   // TODO remove 'time' command. It's only for testing
};
// Dict type for command dict
dict_type cmd_dict_type = {
    dict_sample_case_hash,              // hash
    NULL,                               // key dup
    NULL,                               // val dup
    dict_sample_compare_sds_key_case,   // key compare
    dict_sample_free_sds,               // key destruct
    NULL                                // val destruct. Note static cmd_table[], no need to free val, aka command*.
};

// A dictionary that store all commands and is used for command lookup.
static dict* cmd_dict;

// Init the command dict
void command_dict_init()
{
    cmd_dict = dict_create(&cmd_dict_type);
    int num_cmd = (sizeof(cmd_table) / sizeof(command));

    for (long i = 0; i < num_cmd; i ++) {
        command *cmd = cmd_table + i;
        cmd->id = i;
        int ret = dict_add_entry(cmd_dict, sds_new(cmd->name), cmd);
        server_assert(ret == DICT_OK);
    }
}

// Lookup command with sds key 'cmd_name' in cmd_dict. Return the command if found.
static command *command_lookup(sds cmd_name)
{
    return dict_fetch_value(cmd_dict, cmd_name);
}
// Lookup command with C string 'name' in cmd_dict. Return the command if found.
/*
static command *command_lookup_cstring(const char* cmd_cname)
{
    sds cmd_name = sds_new(cmd_cname);
    command *cmd = dict_fetch_value(cmd_dict, cmd_name);
    sds_free(cmd_name);
    return cmd;
}*/

// Parse client command buf in client's recv buf
void command_parse_client_args(client *c)
{
    size_t cur = 0, end = c->recv_size;
    size_t arg_s = 0, arg_e = 0;

    int idx = 0;

    while (1) {
        while (cur < end && isspace(c->recv_buf[cur])) cur ++; // skip space before
        if (cur >= end) {
            break;
        } else {
            arg_s = cur;
        }

        while (cur < end && !isspace(c->recv_buf[cur])) cur ++; // skip arg content

        arg_e = cur;
        server_assert(idx < CLIENT_MAX_ARG);
        sds arg = sds_new_len(c->recv_buf + arg_s, arg_e - arg_s);
        c->argv[idx] = arg;
        idx ++;
    }

    c->argc = idx;

    printf("command_parse_client_args() argc: %d \n", idx);
    for(int i = 0; i < idx; i ++) {
        printf("arg %d: %s \n", i, c->argv[i]);
    }
}

void command_free_client_args(client *c)
{
    for(int i = 0; i < c->argc; i ++) {
        sds_free(c->argv[i]);
    }
    c->argc = 0;
}

// This function gets called when there command in client's recv_buf to process.
void command_process(client *c)
{
    // Parse recv_buf to get arguments
    command_parse_client_args(c);
    // Reply if not command
    if (c->argv[0] == NULL) {
        net_client_reply_append_cstr(c, "(error) no command.");
        net_client_reply_flush(c);
        return;
    }

    // Look up command 'argv[0]'
    c->cmd = command_lookup(c->argv[0]);
    // Reply if unknown command
    if (!c->cmd) {
        net_client_reply_append_fmt(c, "(error) unknown command <'%s', %lu>.", c->argv[0], sds_len(c->argv[0]));
        net_client_reply_flush(c);
        return;

    } // Reply if wrong number of argument provided
    else if (c->argc != c->cmd->arity) {
        net_client_reply_append_fmt(c, "(error) wrong argument count %d, %d needed.", c->argc, c->cmd->arity);
        net_client_reply_flush(c);
        return;
    }
    // execute now.
    c->cmd->proc(c);

    // Free parsed arguments if any
    command_free_client_args(c);
}

// 'Get' command: get key
static void cmd_get(client *c)
{

    arobj *obj = dict_fetch_value(c->db->d, c->argv[1]);

    if (obj == NULL) {
        net_client_reply_append_fmt(c, "(error) key '%s' not exists.", c->argv[1]);
        net_client_reply_flush(c);
    } else if (obj->type != OBJ_TYPE_STRING) {
        net_client_reply_append_cstr(c, "(error) wrong type, object not a string.");
        net_client_reply_flush(c);
    } else {
        net_client_reply_append_sds(c, obj->ptr);
        net_client_reply_flush(c);
    }
}

// 'Set' command: set key value
static void cmd_set(client *c)
{
    // Used already parsed args as key & val. Avoid sds_new().
    sds key_str = c->argv[1];
    sds val_str = c->argv[2];
    c->argv[1] = NULL; // must set NULL!
    c->argv[2] = NULL;

    arobj *val_obj = obj_create(OBJ_TYPE_STRING, OBJ_ENC_SDS, val_str);
    // add the entry !
    if (dict_add_entry(c->db->d, key_str, val_obj) == DICT_ERR) {
        server_log(LL_VERBOSE, "Server add new entry ('%s', '%s') failed. Already exists.", key_str, val_str);
        net_client_reply_append_fmt(c, "(error) key '%s' already exists. ", key_str);
        net_client_reply_flush(c);
    } else {
        server_log(LL_VERBOSE, "Server add new entry ('%s', '%s') ok", key_str, val_str);
        net_client_reply_append_cstr(c, "(ok)");
        net_client_reply_flush(c);
    }
}

// 'Del' command: del key
static void cmd_del(client *c)
{
    sds key_str = c->argv[1];
    dict_entry *de = dict_unlink(c->db->d, key_str);
    if (de == NULL) {
        server_log(LL_VERBOSE, "Server delete entry with key '%s' failed. No such key.", key_str);
        net_client_reply_append_fmt(c, "(error) key '%s' not exists.", key_str);
        net_client_reply_flush(c);
    } else {
        arobj* o = dict_get_val(de);
        server_assert(o->ref_count == 1);

        dict_free_unlinked_entry(c->db->d, de);

        server_log(LL_VERBOSE, "Server delete entry ('%s', ...) ok", key_str);
        net_client_reply_append_cstr(c, "(ok)");
        net_client_reply_flush(c);
    }
}

// 'Exist' command: exist key
static void cmd_exist(client *c)
{
    sds key_str = c->argv[1];
    if (dict_find(c->db->d, key_str) == NULL) {
        server_log(LL_VERBOSE, "Server entry with key '%s' not exists.", key_str);
        net_client_reply_append_cstr(c, "(no)");
        net_client_reply_flush(c);
    } else {
        server_log(LL_VERBOSE, "Server entry with key '%s' exists.", key_str);
        net_client_reply_append_cstr(c, "(yes)");
        net_client_reply_flush(c);
    }
}

// 'Hset' command: hset hash key value
// TODO Please implement hash structure using ZIPLIST
/*
static void cmd_hset(client *c)
{
    sds hash_str = c->argv[1];
    dict_etnry *new_de = NULL, *old_de = NULL;

    new_de = dict_accommodate_key(c->db->d, hash_str, &old_de);
    if (old_de != NULL) {     // target object exists
        arobj *hash = dict_get_val(old_de);
        if (hash->type != OBJ_TYPE_HASH) {
            net_client_reply_append_cstr(c, "(error) wrong type, object not a hash.");
            net_client_reply_flush(c);
            return;
        }



    } else {

    }
}*/

// 'Exit' command: exit
static void cmd_exit(client *c)
{
    int client_fd = c->fd;
    server_log(LL_VERBOSE, "Client disconnected. Remove client fd: %d ok", client_fd);
    FD_CLR(client_fd, &server.active_fds);
    close(client_fd);
}

// 'Time' command: time
static void cmd_time(client *c)
{
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);

    net_client_reply_append_cstr(c, asctime(local_time));
    net_client_reply_flush(c);
}







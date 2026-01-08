#ifndef ARGPARSE_HASH_H
#define ARGPARSE_HASH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARGPARSE_HASH_THRESHOLD 16
#define ARGPARSE_HASH_TABLE_SIZE 256
#define ARGPARSE_HASH_LOAD_FACTOR 0.75f

    typedef struct HashEntry HashEntry;
    typedef struct ArgHashTable ArgHashTable;
    typedef struct Argument Argument;
    typedef struct ArgParser ArgParser;

    struct HashEntry {
        char* key;              /* argument name (short or long) */
        Argument* argument;     /* pointer to original argument */
        struct HashEntry* next; /* for chaining */
    };

    struct ArgHashTable {
        HashEntry** buckets;    /* array of bucket pointers */
        size_t capacity;        /* total number of buckets */
        size_t size;            /* number of entries */
        uint32_t seed;          /* random seed for hash randomization */
    };

    ArgHashTable* argparse_hash_create_internal(void);
    void argparse_hash_destroy_internal(ArgHashTable* table);
    bool argparse_hash_insert_internal(ArgHashTable* table, const char* key, Argument* arg);
    Argument* argparse_hash_lookup_internal(ArgHashTable* table, const char* key);
    bool ensure_hash_table_built(ArgParser* parser);
    Argument* argparse_hash_find_argument(ArgParser* parser, const char* name);
    bool argparse_hash_is_argument(ArgParser* parser, const char* str);

#ifdef __cplusplus
}
#endif

#endif
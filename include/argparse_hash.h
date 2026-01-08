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

    /* @brief Collision chain node storing argument name-to-structure mappings. */
    typedef struct HashEntry HashEntry;

    /* @brief Fast hash table for argument lookup with auto-resizing and collision handling. */
    typedef struct ArgHashTable ArgHashTable;

    typedef struct Argument Argument;
    typedef struct ArgParser ArgParser;

    struct HashEntry {
        char* key;              /* Argument name (short or long) */
        Argument* argument;     /* Pointer to original Argument structure */
        struct HashEntry* next; /* Next entry in collision chain */
    };

    struct ArgHashTable {
        HashEntry** buckets;    /* Array of bucket pointers */
        size_t capacity;        /* Number of buckets (power of two) */
        size_t size;            /* Number of stored entries */
        uint32_t seed;          /* Random seed for hash randomization */
    };

    /**
     * @brief Creates and initializes a new hash table instance.
     * @return - Pointer to newly allocated ArgHashTable on success
     * @return - NULL on memory allocation failure (sets APE_MEMORY error)
    */
    ArgHashTable* argparse_hash_create_internal(void);

    /**
     * @brief Completely destroys a hash table and all its entries.
     * @param table Hash table to destroy (NULL-safe)
    */
    void argparse_hash_destroy_internal(ArgHashTable* table);

    /**
     * @brief Inserts or updates a key-argument mapping in the hash table.
     * @param table Target hash table (must not be NULL)
     * @param key Argument name (short or long form, e.g., "-v" or "--verbose")
     * @param arg Pointer to Argument structure (must not be NULL)
     * @return - true on success
     * @return - false on failure (sets appropriate error)
    */
    bool argparse_hash_insert_internal(ArgHashTable* table, const char* key, Argument* arg);

    /**
     * @brief Looks up an argument by its name in the hash table.
     * @param table Hash table to search (NULL-safe)
     * @param key Argument name to find
     * @return - Pointer to Argument structure if found
     * @return - NULL if not found or table is NULL
    */
    Argument* argparse_hash_lookup_internal(ArgHashTable* table, const char* key);

    /**
     * @brief Ensures the parser's hash table is built if threshold is reached.
     * @param parser Argument parser instance
     * @return - true if hash table is ready or not needed
     * @return - false on memory allocation failure
    */
    bool ensure_hash_table_built(ArgParser* parser);

    /**
     * @brief Primary lookup function that automatically selects search strategy.
     * @param parser Parser instance (NULL-safe)
     * @param name Argument name to find (e.g., "-h", "--help")
     * @return - Pointer to Argument structure if found
     * @return - NULL if not found or parser is NULL
    */
    Argument* argparse_hash_find_argument(ArgParser* parser, const char* name);

    /**
     * @brief Checks if a string corresponds to a registered argument name.
     * @param parser Parser instance (NULL-safe)
     * @param str String to check
     * @return - true if string matches any argument name (short or long)
     * @return - false otherwise
    */
    bool argparse_hash_is_argument(ArgParser* parser, const char* str);

#ifdef __cplusplus
}
#endif

#endif
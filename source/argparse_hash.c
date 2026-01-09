#include "argparse.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <sys/time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#endif

/* Return secure random seed, using portable generator. */
static uint32_t secure_random_seed(ArgHashTable* table) {
    uint32_t seed = 0;

    /* try OS-specific secure random */
#ifdef _WIN32
    /* Windows CryptGenRandom */
    HCRYPTPROV hCryptProv = 0;
    if (CryptAcquireContextW(&hCryptProv, NULL, NULL,
        PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptGenRandom(hCryptProv, sizeof(seed), (BYTE*)&seed)) {
            CryptReleaseContext(hCryptProv, 0);
            if (seed != 0) return seed;
        }
        CryptReleaseContext(hCryptProv, 0);
    }
#else
    /* Unix /dev/urandom */
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        if (fread(&seed, sizeof(seed), 1, urandom) == 1) {
            fclose(urandom);
            return seed;
        }
        fclose(urandom);
    }
#endif

    /* high-resolution timer combination */
    uint64_t highres_time = 0;

#ifdef _WIN32
    LARGE_INTEGER counter;
    if (QueryPerformanceCounter(&counter)) {
        highres_time = counter.QuadPart;
    }
    else {
        highres_time = GetTickCount64();
    }
#elif defined(__APPLE__)
    highres_time = mach_absolute_time();
#else
    /* POSIX clock_gettime, fallback to gettimeofday */
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        highres_time = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }
    else {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        highres_time = (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
    }
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    highres_time = (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
#endif
#endif

    /* mix multiple entropy sources */
    uintptr_t ptr_value = (uintptr_t)table;

    /* get process ID */
    int process_id =
#ifdef _WIN32
    (int)_getpid();
#else
        (int)getpid();
#endif

    /* get thread ID for additional entropy */
    uintptr_t thread_id = 0;

#ifdef _WIN32
    thread_id = (uintptr_t)GetCurrentThreadId();
#elif defined(__linux__)
    thread_id = (uintptr_t)syscall(SYS_gettid);
#elif defined(__APPLE__)
    thread_id = (uintptr_t)pthread_mach_thread_np(pthread_self());
#endif

    /* high-quality mixing */
    seed = (uint32_t)highres_time;
    seed ^= (uint32_t)(highres_time >> 32);
    seed ^= (uint32_t)ptr_value;
    seed ^= (uint32_t)(ptr_value >> 32);
    seed ^= (uint32_t)process_id;
    seed ^= (uint32_t)thread_id;

    /* MurmurHash3 finalizer for good avalanche */
    seed ^= seed >> 16;
    seed *= 0x85ebca6bU;
    seed ^= seed >> 13;
    seed *= 0xc2b2ae35U;
    seed ^= seed >> 16;

    /* ensure non-zero seed */
    return seed ? seed : 0xDEADBEEFU;
}

/* Security-enhanced FNV-1a hash with randomization. */
static uint32_t hash_string(const char* str, uint32_t seed) {
    if (!str) return 0;

    /* FNV-1a constants */
    const uint32_t FNV_PRIME = 16777619U;
    const uint32_t FNV_OFFSET_BASIS = 2166136261U;

    /* start with randomized basis */
    uint32_t hash = FNV_OFFSET_BASIS ^ seed;

    /* process full string */
    while (*str) {
        hash ^= (uint32_t)(*str++);
        hash *= FNV_PRIME;
    }

    /* final mixing for better distribution */
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    return hash;
}

ArgHashTable* argparse_hash_create_internal(void) {
    ArgHashTable* table = (ArgHashTable*)calloc(1, sizeof(ArgHashTable));
    if (!table) {
        APE_SET_MEMORY(NULL);
        return NULL;
    }

    /* initialize with default capacity */
    table->capacity = ARGPARSE_HASH_TABLE_SIZE;
    table->buckets = (HashEntry**)calloc(table->capacity, 
        sizeof(HashEntry*));

    if (!table->buckets) {
        APE_SET_MEMORY(NULL);
        free(table);
        return NULL;
    }

    /* Generate secure random seed */
    table->seed = secure_random_seed(table);
    table->size = 0;
    return table;
}

void argparse_hash_destroy_internal(ArgHashTable* table) {
    if (!table) return;

    /* free all entries */
    for (size_t i = 0; i < table->capacity; i++) {
        HashEntry* entry = table->buckets[i];

        while (entry) {
            HashEntry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(table->buckets);
    free(table);
}

/* Resize hash table when load factor exceeds threshold. */
static bool hash_table_resize(ArgHashTable* table) {
    /* clear any previous error state */
    argparse_error_clear();

    if (!table) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Cannot resize NULL hash table.");
        return false;
    }

    size_t new_capacity = table->capacity * 2;

    /* overflow check for safety */
    if (new_capacity < table->capacity) 
        return false;

    HashEntry** new_buckets = (HashEntry**)calloc(new_capacity, 
        sizeof(HashEntry*));

    if (!new_buckets) {
        APE_SET_MEMORY(NULL);
        return false;
    }

    /* rehash all existing entries */
    for (size_t i = 0; i < table->capacity; i++) {
        HashEntry* entry = table->buckets[i];

        while (entry) {
            HashEntry* next = entry->next;

            /* recompute hash with new capacity */
            uint32_t hash = hash_string(entry->key, table->seed);
            size_t new_index = hash & (new_capacity - 1);

            /* insert into new bucket */
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;

            entry = next;
        }
    }

    /* replace old buckets */
    free(table->buckets);
    table->buckets = new_buckets;

    table->capacity = new_capacity;
    return true;
}

bool argparse_hash_insert_internal(ArgHashTable* table, const char* key, Argument* arg) {
    /* clear any previous error state */
    argparse_error_clear();

    if (!table || !key || !arg) {
        APE_SET(APE_INTERNAL, EINVAL, key ? key : "(null)",
            "Invalid parameters to hash insertion.");
        return false;
    }

    /* check load factor and resize if needed */
    float load_factor = (float)table->size / (float)table->capacity;
    if (load_factor > ARGPARSE_HASH_LOAD_FACTOR) {
        if (!hash_table_resize(table))
            return false;
    }

    /* compute hash and bucket index */
    uint32_t hash = hash_string(key, table->seed);
    size_t index = hash & (table->capacity - 1);

    /* check for duplicates in this bucket */
    HashEntry* current = table->buckets[index];

    while (current) {
        /* update existing entry */
        if (strcmp(current->key, key) == 0) {
            current->argument = arg;
            return true;
        }

        current = current->next;
    }

    /* create new entry */
    HashEntry* new_entry = (HashEntry*)malloc(sizeof(HashEntry));
    if (!new_entry) {
        APE_SET_MEMORY(key);
        return false;
    }

    /* duplicate the key */
    new_entry->key = strdup(key);
    if (!new_entry->key) {
        APE_SET_MEMORY(key);
        free(new_entry);
        return false;
    }

    /* insert at head of bucket */
    new_entry->argument = arg;
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;
    table->size++;

    return true;
}

Argument* argparse_hash_lookup_internal(ArgHashTable* table, const char* key) {
    /* clear previous error, lookup failure is normal */
    argparse_error_clear();

    if (!table || !key) {
        APE_SET(APE_INTERNAL, EINVAL, key ? key : "(null)",
            "Invalid parameters to hash lookup.");
        return NULL;
    }

    /* compute hash and bucket index */
    uint32_t hash = hash_string(key, table->seed);
    size_t index = hash & (table->capacity - 1);

    /* search in bucket chain */
    HashEntry* entry = table->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0)
            return entry->argument;
        entry = entry->next;
    }

    return NULL;
}

bool ensure_hash_table_built(ArgParser* parser) {
    /* clear previous error */
    argparse_error_clear();

    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL,
            "Cannot build hash table for NULL parser.");
        return false;
    }

    if (parser->hash_table)
        return true;

    if (parser->argument_count < ARGPARSE_HASH_THRESHOLD)
        return false;

    parser->hash_table = argparse_hash_create_internal();

    if (!parser->hash_table) {
        APE_SET(APE_MEMORY, ENOMEM, NULL, "Failed to create hash table.");
        return false;
    }

    Argument* current = parser->arguments;

    while (current) {
        if (current->short_name)
            argparse_hash_insert_internal(parser->hash_table, current->short_name, current);
        if (current->long_name)
            argparse_hash_insert_internal(parser->hash_table, current->long_name, current);
        current = current->next;
    }

    parser->hash_enabled = true;
    return true;
}

Argument* argparse_hash_find_argument(ArgParser* parser, const char* name) {
    /* clear previous error */
    argparse_error_clear();

    if (!parser || !name || name[0] == '\0') {
        APE_SET(APE_INTERNAL, EINVAL, name ? name : "(null)",
            "Invalid parameters to argument lookup.");
        return NULL;
    }

    /* use hash table if enabled */
    if (parser->hash_enabled && parser->hash_table)
        return argparse_hash_lookup_internal(parser->hash_table, 
            name);

    /* fallback to linear search */
    Argument* arg = parser->arguments;

    while (arg) {
        if (arg->short_name && strcmp(arg->short_name, name) == 0) return arg;
        if (arg->long_name && strcmp(arg->long_name, name) == 0) return arg;
        arg = arg->next;
    }

    /* not found */
    return NULL;
}

bool argparse_hash_is_argument(ArgParser* parser, const char* str) {
    /* clear previous error */
    argparse_error_clear();

    if (!parser || !str || str[0] == '\0') {
        APE_SET(APE_INTERNAL, EINVAL, str ? str : "(null)",
            "Invalid parameters to argument check.");
        return false;
    }

    /* use hash table if enabled */
    if (parser->hash_enabled && parser->hash_table)
        return argparse_hash_lookup_internal(parser->hash_table, str) != NULL;

    /* fallback to linear search */
    Argument* arg = parser->arguments;

    while (arg) {
        if (arg->short_name && strcmp(arg->short_name, str) == 0) return true;
        if (arg->long_name && strcmp(arg->long_name, str) == 0) return true;
        arg = arg->next;
    }
    
    /* not an argument */
    return false;
}
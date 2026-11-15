#ifndef HASH_TABLE_H
#define HASH_TABLE_H
#include <stddef.h>
/* Entries store array indices, so growing a record array keeps lookups valid. */
typedef struct Node
{
    int key;
    size_t value;
    struct Node *next;
} Node;
typedef struct
{
    Node **buckets;
    size_t capacity, count;
} Hash;
void hash_free(Hash *h);
int hash_put(Hash *h,int key,size_t value);
int hash_find(const Hash *h,int key,size_t *value);
int hash_remove(Hash *h,int key);
#endif

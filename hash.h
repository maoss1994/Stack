#ifndef _HASH_H_
#define _HASH_H_

typedef struct hash_node_s hash_node_t;
struct hash_node_s {
        char *key;
        char *val;
        unsigned int hash_a;
        unsigned int hash_b;
};

typedef struct hash_table_s hash_table_t;
struct hash_table_s {
        int size;
        hash_node_t *table;
};

hash_table_t *hash_table_create(int size);
int hash_table_free(hash_table_t *table);
int hash_table_add(hash_table_t *table, char *key, char *val);
hash_node_t *hash_table_find(hash_table_t *table, char *key);

#endif // _HASH_H_

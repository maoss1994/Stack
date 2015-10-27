#include <stdlib.h>
#include <string.h>
#include "hash.h"

// BKDR hash function
unsigned int bkdr_hash(char *key, int size)
{
        unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
        unsigned int val  = 0;

        while (*key != '\0')
        {
                val = val * seed + (*key++);
        }

        return (val & 0x7FFFFFFF % size);
}

// AP hash function
unsigned int ap_hash(char *key)
{
        unsigned int val = 0;
        int i;

        for (i=0; *key != '\0'; i++)
        {
                if ((i & 1) == 0)
                {
                        val ^= ((val << 7) ^ (*key++) ^ (val >> 3));
                }
                else
                {
                        val ^= (~((val << 11) ^ (*key++) ^ (val >> 5)));
                }
        }

        return (val & 0x7FFFFFFF);
}

// DJB hash function
unsigned int djb_hash(char *key)
{
        unsigned int val = 5381;
        while (*key != '\0')
        {
                val += (val << 5) + (*key++);
        }

        return (val &0x7FFFFFFF);
}

hash_table_t *hash_table_create(int size)
{
        hash_table_t *t;
        hash_node_t  *n;

        t = (hash_table_t*) malloc(sizeof(hash_table_t));
        if (!t) {
                return NULL;
        }

        n = (hash_node_t*) malloc(size * sizeof(hash_node_t));
        if (!n) {
                free(t);
                return NULL;
        }

        memset(n, 0, size * sizeof(hash_node_t));

        t->size  = size;
        t->table = n;

        return t;
}

int hash_table_free(hash_table_t *table)
{
        free(table->table);

        return 0;
}

int hash_table_add(hash_table_t *table, char *key, char *val)
{
        unsigned int index  = bkdr_hash(key, table->size);
        unsigned int hash_a = ap_hash(key);
        unsigned int hash_b = djb_hash(key);

        hash_node_t *node = table->table + index;

        while (node->key != NULL)
        {
                node++;

                if (node >= table->table + table->size) {
                        node = table->table;
                }

                if (node == table->table + index) {
                        return -1;
                }
        }

        node->key    = key;
        node->val    = val;
        node->hash_a = hash_a;
        node->hash_b = hash_b;

        return 0;
}

hash_node_t *hash_table_find(hash_table_t *table, char *key)
{
        unsigned int index  = bkdr_hash(key, table->size);
        unsigned int hash_a = ap_hash(key);
        unsigned int hash_b = djb_hash(key);

        hash_node_t *node = table->table + index;

        while ((node->hash_a != hash_a) || node->hash_b != hash_b)
        {
                node++;

                if (node >= table->table + table->size) {
                        node = table->table;
                }

                if (node == table->table + index) {
                        return NULL;
                }
        }

        return node;
}

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdio.h>
#include "hash.h"

typedef struct config_s config_t;
struct config_s {
        // FILE *file_fd;
        char *file_name;

        // store config in a hash table
        hash_table_t *hash_table;
};

// for core
config_t *config_init(char *file_name);
int config_exit();

// function for other module
char *config_find(char *key);

#endif // _CONFIG_H_

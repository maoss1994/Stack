#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "hash.h"

#define DEFAULT_CONFIG_FILE "uns.conf"

#define CONFIG_TABLE_SIZE 100

#define CONFIG_BUFFER_SIZE 10 * 1024 // 10KB

int config_read();
int config_add(char *key, char *val);

static config_t config;

config_t *config_init(char *file_name)
{
        if (file_name == NULL) {
                config.file_name = DEFAULT_CONFIG_FILE;
        } else {
                config.file_name = file_name;
        }

        config.hash_table = hash_table_create(CONFIG_TABLE_SIZE);
        if (!config.hash_table) {
                return NULL;
        }

        if (config_read(config) == -1) {
                return NULL;
        }

        return &config;
}

int config_exit()
{
        hash_table_free(config.hash_table);
        free(config.hash_table);

        return 0;
}

enum config_state {
        s_key_start,
        s_key,
        s_key_end,
        s_space_after_key,
        s_val_start,
        s_val,
        s_val_end,
        s_space_after_val,
        s_comment_start,
        s_comment,
        s_comment_end,
        s_cr,
        s_lf,
        s_eof,
};

#define IS_SPACE(c) ((c) == ' ' || (c) == '\t')
#define IS_VCHAR(c) ((c) >  ' ')
#define IS_SHARP(c) ((c) == '#') 
#define IS_CR(c)    ((c) == '\r')
#define IS_LF(c)    ((c) == '\n')

int config_read()
{
        FILE *file_fd;
        char *buf, *p, ch, *key_start, *key_end, *val_start, *val_end;
        enum config_state state = s_key_start;
        int len;

        buf = (char*) malloc(CONFIG_BUFFER_SIZE * sizeof(char));
        if (!buf) {
                return -1;
        }

        file_fd = fopen(config.file_name, "r");
        if (!file_fd) {
                return -1;
        }

        len = fread(buf, sizeof(char), CONFIG_BUFFER_SIZE, file_fd);
        if (ferror(file_fd) || !feof(file_fd)) {
                return -1;
        }

        for (p = buf; p < buf + len; p++)
        {
                ch = *p;
                switch (state)
                {
                        
                case s_key_start:
                        // skip space before the line
                        if (IS_SPACE(ch)) {
                                break;
                        }

                        // begin with '#', that's comment
                        if (IS_SHARP(ch)) {
                                state = s_comment;
                                break;
                        }

                        // key
                        if (IS_VCHAR(ch)) {
                                key_start = p;
                                state     = s_key;
                                break;
                        }

                        // spcae line
                        if (IS_CR(ch)) {
                                state = s_cr;
                                break;
                        }

                        // spcae line
                        if (IS_LF(ch)) {
                                state = s_key_start;
                                break;
                        }

                        // error
                        return -1;

                case s_key:
                        // key
                        if (IS_VCHAR(ch)) {
                                break;
                        }
                        
                        // space after key
                        if (IS_SPACE(ch)) {
                                key_end = p;
                                state   = s_space_after_key;
                                break;
                        }
                        
                        // error
                        return -1;

                case s_comment:
                        // CR
                        if (IS_CR(ch)) {
                                state = s_cr;
                                break;
                        }

                        // LF
                        if (IS_LF(ch)) {
                                state = s_key_start;
                                break;
                        }

                        // comment
                        break;

                case s_space_after_key:
                        // space
                        if (IS_SPACE(ch)) {
                                break;
                        }

                        // value start
                        if (IS_VCHAR(ch)) {
                                val_start = p;
                                state     = s_val;
                                break;
                        }

                        // error
                        return -1;

                case s_val:
                        // value
                        if (IS_VCHAR(ch)) {
                                break;
                        }

                        // space after value
                        if (IS_SPACE(ch)) {
                                val_end  = p;
                                state    = s_space_after_val;

                                *key_end = '\0';
                                *val_end = '\0';
                                if (config_add(key_start, val_start) == -1) {
                                        return -1;
                                }

                                break;
                        }

                        // CR
                        if (IS_CR(ch)) {
                                val_end  = p;
                                state    = s_cr;

                                *key_end = '\0';
                                *val_end = '\0';
                                if (config_add(key_start, val_start) == -1) {
                                        return -1;
                                }

                                break;
                        }

                        // LF
                        if (IS_LF(ch)) {
                                val_end  = p;
                                state    = s_key_start;

                                *key_end = '\0';
                                *val_end = '\0';
                                if (config_add(key_start, val_start) == -1) {
                                        return -1;
                                }

                                break;
                        }
                        
                        // error
                        return -1;

                case s_space_after_val:
                        // space
                        if (IS_SPACE(ch)) {
                                break;
                        }

                        // CR
                        if (IS_CR(ch)) {
                                state = s_cr;
                                break;
                        }

                        // LF
                        if (IS_LF(ch)) {
                                state = s_key_start;
                                break;
                        }

                        // error
                        return -1;

                case s_cr:
                        // LF
                        if (IS_LF(ch)) {
                                state = s_key_start;
                                break;
                        }
        
                        // error
                        return -1;

                default:
                        return -1;

                }
        }

        fclose(file_fd);

        return 0;
}

int config_add(char *key, char *val)
{
        return hash_table_add(config.hash_table, key, val);
}

char *config_find(char *key)
{
        hash_node_t *p;

        p = hash_table_find(config.hash_table, key);
        if (!p) {
                return NULL;
        }

        return p->val;
}

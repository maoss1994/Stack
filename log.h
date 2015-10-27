#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

typedef struct log_s log_t;
struct log_s {
        FILE *file_fd;
        char *file_name;
        int   level;
};

// for core
log_t *log_init(char *file_name, int level);
int log_exit();

// function for other module
int log_error(char *module, char *msg);
int log_warn (char *module, char *msg);
int log_info (char *module, char *msg);
int log_debug(char *module, char *msg);

int logf_error(char *module, const char *format, ...);
int logf_warn (char *module, const char *format, ...);
int logf_info (char *module, const char *format, ...);
int logf_debug(char *module, const char *format, ...);

#endif // _LOG_H_

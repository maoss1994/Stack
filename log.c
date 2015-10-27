#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include "log.h"

#define DEFAULT_LOG_FILE "uns.log"

#define LOG_BUFFER_SIZE 1024

int log_msg(char *type, char *module, char *msg);

static log_t log;

log_t *log_init(char *file_name, int level)
{
        FILE *file_fd;

        if (file_name == NULL) {
                log.file_name = DEFAULT_LOG_FILE;
        } else {
                log.file_name = file_name;
        }

        if (level >= 0 && level <= 3) {
                log.level = level;
        } else {
                return NULL;
        }

        file_fd = fopen(log.file_name, "a+");
        if (!file_fd) {
                return NULL;
        } else {
                log.file_fd = file_fd;
        }

        return &log;
}

int log_exit()
{
        return fclose(log.file_fd);
}

#define LOG_MESSAGE         "[%s] [%s]\t%s -- %s\n"
#define LOG_TIME_FORMAT     "%Y-%m-%d %T"

int log_msg(char *type, char *module, char *msg)
{
        static char buf[LOG_BUFFER_SIZE];
        static char tm[30];
        static time_t t;
        FILE *file_fd = log.file_fd;
        
        if (!file_fd) {
                return -1;
        }

        t = time(NULL);
        strftime(tm, sizeof(tm), LOG_TIME_FORMAT, localtime(&t));

        sprintf(buf, LOG_MESSAGE, tm, module, type, msg);

        if (fwrite(buf, sizeof(char), strlen(buf), file_fd) < strlen(buf)) {
                return -1;
        }

        return 0;
}

// error level = 3
int log_error(char *module, char *msg)
{
        return log_msg("ERROR", module, msg);
}

int logf_error(char *module, const char *format, ...)
{
        static char buf[LOG_BUFFER_SIZE];

        va_list args;    
        va_start(args, format);
        vsprintf(buf, format, args);
        va_end(args);

        log_error(module, buf);

        return 0;
}

// warn level = 2
int log_warn(char *module, char *msg)
{
        if (log.level <= 2) {
                return log_msg("WARN ", module, msg);
        } else {
                return 0;
        }
}

int logf_warn(char *module, const char *format, ...)
{
        static char buf[LOG_BUFFER_SIZE];

        va_list args;    
        va_start(args, format);
        vsprintf(buf, format, args);
        va_end(args);

        log_warn(module, buf);

        return 0;
}

// info level = 1
int log_info(char *module, char *msg)
{
        if (log.level <= 1) {
                return log_msg("INFO ", module, msg);
        } else {
                return 0;
        }
}

int logf_info(char *module, const char *format, ...)
{
        static char buf[LOG_BUFFER_SIZE];

        va_list args;    
        va_start(args, format);
        vsprintf(buf, format, args);
        va_end(args);

        log_info(module, buf);

        return 0;
}

// debug level = 0
int log_debug(char *module, char *msg)
{
        if (log.level <= 0) {
                return log_msg("DEBUG", module, msg);
        } else {
                return 0;
        }
}

int logf_debug(char *module, const char *format, ...)
{
        static char buf[LOG_BUFFER_SIZE];

        va_list args;    
        va_start(args, format);
        vsprintf(buf, format, args);
        va_end(args);

        log_debug(module, buf);

        return 0;
}


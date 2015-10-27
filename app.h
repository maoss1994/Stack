#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <stdint.h>
#include <sys/types.h>
#include "config.h"
#include "queue.h"

typedef struct app_s app_t;
typedef struct packet_s packet_t;

typedef int (*app_input_fn)(app_t *app);
typedef int (*app_output_fn)(packet_t *pkg);

enum app_state {
        s_close,
        s_wait_request,
        s_connected,
};

struct app_s {
        int fd;

        // client infomation
        uint32_t in_addr;
        uint16_t in_port;

        enum app_state state;

        app_input_fn  input;
        app_output_fn output;

        queue_t queue;
};

typedef struct app_ctl_s app_ctl_t;
struct app_ctl_s {
        int   fd;
        char *port;
};

// for core
int app_init();
int app_exit();

// for other module
int app_send(packet_t *pkg);
int app_receive(packet_t *pkg);

// for client 
int app_add(app_t *app);

#endif // _APPLICATION_H_

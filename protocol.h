#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>
#include "queue.h"

typedef int (*send_up_fn)(packet_t *pkg);
typedef int (*send_down_fn)(packet_t *pkg);

typedef uint8_t ptc_id_t;

typedef struct ptc_s ptc_t;
struct ptc_s {
        ptc_id_t id;

        send_up_fn      up;
        send_down_fn    down;

        queue_t queue;
};

// for core
int ptc_init();
int ptc_exit();

// for other module

// for protocol
int ptc_add(ptc_t *ptc);


#endif // _PROTOCOL_H_

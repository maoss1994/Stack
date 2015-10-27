#ifndef _EVENT_H_
#define _EVENT_H_

#include <stdint.h>
#include "queue.h"

#define event_create(ev)        ((ev) = (event_t*) malloc(sizeof(event_t)))
#define tick_create(tc)         ((tc) = (tick_t*) malloc(sizeof(tick_t)))

#define EVENT_FLAG_ACTIVE       0x01U
#define EVENT_FLAG_READ         0x02U
#define EVENT_FLAG_WRITE        0x04U
#define EVENT_FLAG_ERROR        0x08U

#define set_event_active(ev)    ((ev)->flag |= EVENT_FLAG_ACTIVE)
#define set_event_read(ev)      ((ev)->flag |= EVENT_FLAG_READ)
#define set_event_write(ev)     ((ev)->flag |= EVENT_FLAG_WRITE)
#define set_event_error(ev)     ((ev)->flag |= EVENT_FLAG_ERROR)

#define unset_event_active(ev)  ((ev)->flag &= ~EVENT_FLAG_ACTIVE)
#define unset_event_read(ev)    ((ev)->flag &= ~EVENT_FLAG_READ)
#define unset_event_write(ev)   ((ev)->flag &= ~EVENT_FLAG_WRITE)
#define unset_event_error(ev)   ((ev)->flag &= ~EVENT_FLAG_ERROR)

typedef int dev_id_t;
typedef struct event_s event_t;

// event callback function
typedef int (*ev_cb_fn)(event_t *ev);

struct event_s {
        dev_id_t fd;

        // control flags
        int flag;

        ev_cb_fn input;
        ev_cb_fn output;

        queue_t queue;
};

typedef uint8_t ptc_id_t;
typedef struct tick_s tick_t;

// tick callback function
typedef int (*tc_cb_fn)(tick_t *tc);

struct tick_s {
        ptc_id_t ptc;

        int time;       // ms

        tc_cb_fn timeout;

        queue_t queue;
};

// for core
int event_init();
int event_exit();
int wait_select();

// for other module
int event_add(event_t *ev);
int event_delete(event_t *ev);
int tick_add(tick_t *tc);
int tick_add(tick_t *tc);
event_t *event_find_by_fd(int fd);

#endif // _EVENT_H_


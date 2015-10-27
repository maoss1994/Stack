#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/select.h>
#include "event.h"
#include "config.h"
#include "log.h"
#include "queue.h"

#define EVENT_ERROR(s) log_error("EVENT", (s))
#define EVENT_WARN(s)  log_warn ("EVENT", (s))
#define EVENT_INFO(s)  log_info ("EVENT", (s))
#define EVENT_DEBUG(s) log_debug("EVENT", (s))

#define is_event_active(ev)     ((ev)->flag & EVENT_FLAG_ACTIVE)
#define is_event_read(ev)       ((ev)->flag & EVENT_FLAG_READ)
#define is_event_write(ev)      ((ev)->flag & EVENT_FLAG_WRITE)
#define is_event_error(ev)      ((ev)->flag & EVENT_FLAG_ERROR)

#define TICK_PERIOD    (100 * 1e3) // 100 * 1e3 us = 100 ms

static queue_t *ev_list;
static queue_t *tc_list;

int find_max_fd();
int handle_event(fd_set *rfd, fd_set *wfd);
int handle_tick(struct timeval *tv);

int event_init()
{
        if (!queue_create(ev_list)) {
                EVENT_ERROR("Can not create event queue.");
                return -1;
        }

        queue_init(ev_list);

        if (!queue_create(tc_list)) {
                EVENT_ERROR("Can not create event queue.");
                return -1;
        }

        queue_init(tc_list);

        EVENT_INFO("Initialize the EVENT SYSTEM successed.");

        return 0;
}

int event_exit()
{
        event_t *e;
        tick_t *t;
        queue_t *q;

        for (q = ev_list->next; q != ev_list; q = q->next)
        {
                e = queue_data(q, event_t, queue);
                free(e);
        }

        for (q = tc_list->next; q != tc_list; q = q->next)
        {
                t = queue_data(q, tick_t, queue);
                free(t);
        }

        return 0;
}

int event_add(event_t *ev)
{
        queue_insert(ev_list, &ev->queue);

        EVENT_DEBUG("Successed to add a event.");

        return 0;
}

int event_delete(event_t *ev)
{
        queue_delete(&ev->queue);

        free(ev);

        return 0;
}

event_t *event_find_by_fd(int fd)
{
        event_t *ev;
        queue_t *q;

        for (q = ev_list->next; q != ev_list; q = q->next)
        {
                ev = queue_data(q, event_t, queue);
                if (ev->fd == fd) {
                        return ev;
                }
        }

        return NULL;
}

int tick_add(tick_t *tc)
{
        queue_insert(tc_list, &tc->queue);

        EVENT_DEBUG("Successed to add a tick.");

        return 0;
}

int tick_delete(tick_t *tc)
{
        queue_delete(&tc->queue);

        free(tc);

        return 0;
}

int find_max_fd()
{
        event_t *e;
        queue_t *q;
        int maxfd = 0;

        for (q = ev_list->next; q != ev_list; q = q->next)
        {
                e = queue_data(q, event_t, queue);
                if (e->fd > maxfd) {
                        maxfd = e->fd;
                }
        }

        return maxfd;
}

int handle_event(fd_set *rfd, fd_set *wfd)
{
        event_t *ev;
        queue_t *q;
        
        for (q = ev_list->next; q != ev_list; q = q->next)
        {
                ev = queue_data(q, event_t, queue);

                if (FD_ISSET(ev->fd, rfd)) {
                        ev->input(ev);
                }

                if (FD_ISSET(ev->fd, wfd)) {
                        ev->output(ev);
                }
        }

        return 0;
}

#define USEC_TO_MSEC(tc) ((tc) * 1000)

int handle_tick(struct timeval *tv)
{
        tick_t  *tc;
        queue_t *q;

        for (q = tc_list->next; q != tc_list; q = q->next)
        {
                tc->time -= USEC_TO_MSEC(tv->tv_usec);
                if (tc->time < 0) {
                        tc->timeout(tc);
                        tick_delete(tc);
                }
        }

        return 0;
}

int wait_select()
{
        static struct timeval tv;
        static fd_set readfd;
        static fd_set writefd;

        int selected;

        event_t *ev;
        queue_t *q;

        FD_ZERO(&readfd);
        FD_ZERO(&writefd);

        for (q = ev_list->next; q != ev_list; q = q->next)
        {
                ev = queue_data(q, event_t, queue);
                if (is_event_active(ev) && is_event_read(ev)) {
                        FD_SET(ev->fd, &readfd);
                }
                if (is_event_active(ev) && is_event_write(ev)) {
                        FD_SET(ev->fd, &writefd);
                }
        }

        if (tv.tv_usec <= 0) {
                tv.tv_sec  = 0;
                tv.tv_usec = TICK_PERIOD;
        }

        // selected = select(find_max_fd()+1, &readfd, &writefd, NULL, &tv);
        selected = select(find_max_fd()+1, &readfd, &writefd, NULL, NULL);

        if (selected == -1) {           // error
                EVENT_ERROR(strerror(errno));
        } else if (selected == 0) {     // tick
                handle_tick(&tv);
        } else {                        // IO evnet
                handle_event(&readfd, &writefd);
                handle_tick(&tv);
        }

        return 0;
}


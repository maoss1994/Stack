/*
 * device.c
 *
 * 1. Initialize the device module.
 * 2. Close the file descriptor and free all the device before exit.
 * 3. Call the device send their packet. 
 * 4. Send the frame to upper layer.
 * 5. Send packet to the device.
 * 6. Get input data from device
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "device.h"
#include "config.h"
#include "log.h"
#include "queue.h"
#include "event.h"
#include "app.h"
#include "packet.h"

#define DEVICE_ERROR(s) log_error("DEVICE", (s))
#define DEVICE_WARN(s)  log_warn ("DEVICE", (s))
#define DEVICE_INFO(s)  log_info ("DEVICE", (s))
#define DEVICE_DEBUG(s) log_debug("DEVICE", (s))

#define is_dev_read_available(dev)      ((dev)->state & DEVICE_STATE_READ_AVAILABLE)
#define is_dev_write_available(dev)     ((dev)->state & DEVICE_STATE_WRITE_AVAILABLE)

static int device_input(event_t *ev);
static int device_output(event_t *ev);
static int device_check_write();
static device_t *device_find_by_fd(int fd);

static queue_t *dev_list;
static queue_t *write_list;

extern int aquasent_init();

/* 
 * Initialize the device module.
 */
int device_init()
{
        if (!queue_create(dev_list)) {
                DEVICE_ERROR("Can not create device queue.");
                return -1;
        }

        queue_init(dev_list);

        if (!queue_create(write_list)) {
                DEVICE_ERROR("Can not create device queue.");
                return -1;
        }

        queue_init(write_list);

        DEVICE_INFO("Initialize the DEVICE MODULE successed.");

        if (aquasent_init() == -1) {
                return -1;
        }

        return 0;
}

/*
 * Close the file descriptor and free all the device before exit.
 */
int device_exit()
{
        device_t *dev;
        queue_t  *q;

        for(q = dev_list->next; q != dev_list; q = q->next)
        {
                dev = queue_data(q, device_t, queue);
                dev->exit();
                free(dev);
        }

        return 0;
}

/* 
 * the protocol module use this function to call the device send 
 * their packet.
 */
int device_send(packet_t *pkg)
{
        if (!pkg->dev) {
                return -1;
        }

        queue_insert_tail(write_list, &pkg->queue);

        device_check_write();

        return 0;
}

int device_add(device_t *dev)
{
        event_t *ev;

        if (!dev) {
                DEVICE_ERROR("Can not add an invalid device to the device list.");
                return -1;
        }

        queue_insert(dev_list, &dev->queue);

        if (!event_create(ev)) {
                DEVICE_ERROR("Can not alloc memory for a event.");
                return -1;
        }

        ev->fd     = dev->fd;
        ev->input  = device_input;
        ev->output = device_output;
        ev->flag   = 0;

        set_event_active(ev);
        set_event_read(ev);
        event_add(ev);

        DEVICE_DEBUG("Successed to add an device.");

        return 0;
}

/*
 * get input data from device
 */
static int device_input(event_t *ev)
{
        device_t *dev = device_find_by_fd(ev->fd);
        if (!dev) {
                DEVICE_ERROR("Can not find the device matched.");
                return -1;
        }

        return dev->input(dev);
}

/*
 * after read a fully frame, the driver call this function to send 
 * the frame to upper layer.
 */
int device_input_finish(packet_t *pkg)
{
        return app_send(pkg);
        // int i;
        // for (i = 0; i < pkg->len; i++) {
                // printf("%c", pkg->pdu[i]);
        // }
        // printf("\n");
        // return 0;
}

/*
 * send packet to the device.
 */
static int device_output(event_t *ev)
{
        device_t *dev = device_find_by_fd(ev->fd);
        if (!dev) {
                DEVICE_ERROR("Can not find the device matched.");
                return -1;
        }

        packet_t *pkg;
        queue_t  *q;

        for (q = write_list->next; q != write_list; q = q->next)
        {
                pkg = queue_data(q, packet_t, queue);
                if (pkg->dev == dev) {
                        dev->output(pkg);
                        break;
                }
        }

        return 0;
}

int device_output_finish(packet_t *pkg)
{
        event_t *ev;

        if (!pkg->dev) {
                return -1;
        }

        queue_delete(&pkg->queue);
        free(pkg);

        device_check_write();

        return 0;
}

int device_output_finish_part(packet_t *pkg)
{
        event_t *ev;

        if (!pkg->dev) {
                return -1;
        }

        unset_dev_write_available(pkg->dev);

        ev = event_find_by_fd(pkg->dev->fd);
        if (!ev) {
                return -1;
        }

        unset_event_write(ev);

        return 0;
}

/*
 * check there is any device ready to send their packet.
 */
static int device_check_write()
{
        packet_t *pkg;
        event_t  *ev;
        queue_t  *q;

        for (q = write_list->next; q != write_list; q = q->next)
        {
                pkg = queue_data(q, packet_t, queue);
                if (!is_dev_write_available(pkg->dev)) {
                        continue;
                }

                ev = event_find_by_fd(pkg->dev->fd);
                if (!ev) {
                        return -1;
                }

                set_event_write(ev);

                return 1;
        }

        return 0;
}

static device_t *device_find_by_fd(int fd)
{
        device_t *d;
        queue_t  *q;

        for (q = dev_list->next; q != dev_list; q = q->next)
        {
                d = queue_data(q, device_t, queue);
                if (d->fd == fd) {
                        return d;
                }
        }

        return NULL;
}

device_t *device_find_by_ip(ip_addr_t addr)
{
        device_t *d;
        queue_t  *q;

        for (q = dev_list->next; q != dev_list; q = q->next)
        {
                d = queue_data(q, device_t, queue);
                if (d->ip_addr == addr) {
                        return d;
                }
        }

        return NULL;
}

device_t *device_find_by_name(char *name)
{
        device_t *d;
        queue_t  *q;

        for (q = dev_list->next; q != dev_list; q = q->next)
        {
                d = queue_data(q, device_t, queue);
                if (d->name[0] == name[0] && d->name[1] == name[1]) {
                        return d;
                }
        }

        return NULL;
}

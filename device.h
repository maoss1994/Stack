#ifndef _DEIVCE_H_
#define _DEIVCE_H_

#include <stdint.h>
#include <sys/types.h>
#include "config.h"
#include "queue.h"

#define DEVICE_FLAG_UP          0x01U
#define DEVICE_FLAG_BROADCAST   0x02U
#define DEVICE_FLAG_DHCP        0x04U
#define DEVICE_FLAG_ETHARP      0x08U

#define DEVICE_STATE_READ_AVAILABLE     0x01U
#define DEVICE_STATE_WRITE_AVAILABLE    0x02U

#define set_dev_read_available(dev)     ((dev)->state |= DEVICE_STATE_READ_AVAILABLE)
#define set_dev_write_available(dev)    ((dev)->state |= DEVICE_STATE_WRITE_AVAILABLE)

#define unset_dev_read_available(dev)   ((dev)->state &= ~DEVICE_STATE_READ_AVAILABLE)
#define unset_dev_write_available(dev)  ((dev)->state &= ~DEVICE_STATE_WRITE_AVAILABLE)

typedef struct device_s device_t;
typedef struct packet_s packet_t;

typedef uint8_t ip_addr_t;
typedef uint8_t mac_addr_t;
typedef int (*input_fn)(device_t *dev);
typedef int (*output_fn)(packet_t *pkg);
typedef int (*dev_exit_fn)();

struct device_s {
        // file descriptor
        int fd;

        // device type
        char name[2];
        // device number connected
        // unsigned num;

        // IP address infomation
        ip_addr_t ip_addr;
        ip_addr_t netmask;
        ip_addr_t gateway;

        // MAC address infomation
        mac_addr_t mac_addr;

        // maximum transfer unit size
        unsigned int mtu;

        // device flag
        unsigned int flag;

        // device state
        unsigned int state;

        // function for io
        input_fn  input;
        output_fn output;

        // function for exit
        dev_exit_fn exit;

        queue_t queue;
};

// for core
int device_init();
int device_exit();

// for other module
int device_send(packet_t *pkg);
device_t *device_find_by_name(char *name);

// for devices
int device_add(device_t *device);
int device_input_finish(packet_t *pkg);
int device_output_finish(packet_t *pkg);

#endif // _DEIVCE_H_

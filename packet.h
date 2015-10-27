#ifndef _UNS_PACKET_H_
#define _UNS_PACKET_H_

#include <stdint.h>
#include "queue.h"
#include "device.h"
#include "app.h"
#include "protocol.h"

typedef struct packet_s packet_t;
struct packet_s {
        char *pdu;
        char *buf;
        int   len;
        int   tot_len;
        int   ref;

        // device
        device_t *dev;

        // mac header
        // mac_hdr_t mac_hdr;

        // ip header
        // ip_hdr_t  ip_hdr;

        // application
        app_t    *app;

        ptc_id_t up;
        ptc_id_t down;

        queue_t queue;
};

#endif // _UNS_PACKET_H_

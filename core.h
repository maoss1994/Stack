#ifndef _CORE_H_
#define _CORE_H_

#include "device.h"
#include "event.h"
#include "app.h"

typedef struct core_s core_t;
struct core_s {

        // config_t      *conf;
        // log_t         *log;
        // notice_t      *noti;

        device_ctl_t        *dev;
        event_ctl_t         *eve;
        app_ctl_t           *app;
        // application_t *app;
        // protocol_t    *ptc;
};

#endif // _CORE_H_

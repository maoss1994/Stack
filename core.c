#include <stdio.h>
#include <signal.h>
#include "config.h"
#include "log.h"
#include "event.h"
#include "device.h"
#include "app.h"

#define BUFFER_SIZE 1024

#define CORE_ERROR(s) log_error("CORE", (s))
#define CORE_WARN(s)  log_warn ("CORE", (s))
#define CORE_INFO(s)  log_info ("CORE", (s))
#define CORE_DEBUG(s) log_debug("CORE", (s))

#define CORE_SEND_NOTICE(r, m, d) notice_send(MODULE_CORE, (r), (m), (d))

void sighandler(int sig)
{
        CORE_INFO("Receive signal SIGINT from terminal to close the process.");
        // app_exit();
        device_exit();
        event_exit();
        app_exit();
        log_exit();

        exit(0);
}

int main(int argc, int argv)
{
        config_t   *config;
        log_t      *log;

        // handle some signal
        signal(SIGABRT, &sighandler);
        signal(SIGTERM, &sighandler);
        signal(SIGINT,  &sighandler);

        // read config
        config_init(NULL);

        // set log
        log_init(NULL, 0);

        // init event module
        if (event_init() == -1) {
                return -1;
        }

        // init device module
        if (device_init() == -1) {
                return -1;
        }

        // init app module
        if (app_init() == -1) {
                return -1;
        }

        for (;;)
        {
                wait_select();
        }

        return 0;
}

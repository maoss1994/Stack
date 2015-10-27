#include "protocol.h"
#include "config.h"
#include "log.h"
#include "queue.h"
#include "message.h"
#include "notice.h"

static queue_t *head;
static ptc_ctl_t ptc_ctl;
static packet_t packet;

#define PTC_ERROR(s) log_error("PROTOCOL", (s))
#define PTC_WARN(s)  log_warn ("PROTOCOL", (s))
#define PTC_INFO(s)  log_info ("PROTOCOL", (s))
#define PTC_DEBUG(s) log_debug("PROTOCOL", (s))

#define PTC_SEND_NOTICE(r, m, d) notice_send(MODULE_PROTOCOL, (r), (m), (d))

int ptc_notice_receive(int sender, int message, char *data);
ptc_t *ptc_find(ptc_id_t id);
int receive_device_pkg(packet_t *pkg);

extern int mac_init();

ptc_ctl_t *ptc_init()
{
        if (!queue_create(head)) {
                PTC_ERROR("Can not create protocol queue.");
                return NULL;
        }

        queue_init(head);

        ptc_ctl.notice_receive = ptc_notice_receive;

        mac_init();

        PTC_INFO("Initialize the PROTOCOL MODULE successed.");

        return &ptc_ctl;
}

int ptc_notice_receive(int sender, int message, char *data)
{
        switch(message)
        {
                case MSG_DEVICE_RECEIVE:
                {
                        PTC_DEBUG("Get data from device");
                        packet_t *pkg = (packet_t*) data;
                        return receive_device_pkg(pkg);
                }
        }
}

int receive_device_pkg(packet_t *pkg)
{
        ptc_t *p;
        while (pkg->up)
        {
                p = ptc_find(pkg->up);
                if (!p) {
                        return -1;
                }

                if (p->up(pkg) != 0) {
                        break;
                }
        }

        PTC_SEND_NOTICE(MODULE_APP, MSG_STACK_RECEIVE, *pkg);

        return 0;
}

ptc_t *ptc_find(ptc_id_t id)
{
        ptc_t   *p;
        queue_t *q;

        for (q = head->next; q != head; q = q->next)
        {
                p = queue_data(q, ptc_t, queue);
                if (p->id = id) {
                        return p;
                }
        }

        return NULL;
}

int ptc_add(ptc_t *ptc)
{
        if (!ptc) {
                PTC_ERROR("Can not add an invalid protocol to the protocol list.");
                return -1;
        }

        queue_insert_head(head, &ptc->queue);

        PTC_DEBUG("Successed to add an protocol.");

        return 0;
}


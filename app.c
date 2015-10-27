#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include "app.h"
#include "config.h"
#include "log.h"
#include "queue.h"
#include "event.h"
#include "packet.h"
#include "device.h"

#define APP_LISTEN_PORT     "socks_port"
#define APP_DEFAULT_PORT    "34567"

#define APP_ERROR(s) log_error("APP", (s))
#define APP_WARN(s)  log_warn ("APP", (s))
#define APP_INFO(s)  log_info ("APP", (s))
#define APP_DEBUG(s) log_debug("APP", (s))

#define SOCKS_VERSION   0x05
#define METHOD_DEFAULT  0x00
#define METHOD_ERROR    0xFF
#define CMD_CONNECT     0x01
#define CMD_BIND        0x02
#define CMD_UPD         0x03
#define ATYP_IPV4       0x01
#define ATYP_DOMAIN     0x03
#define ATYP_IPV6       0x04

#define REP_SUCCEEDED           0x00
#define REP_GENREAL_FAILURE     0x01
#define REP_CONNECTE_NOT_ALLOW  0x02
#define REP_NETWORK_UNREACHABLE 0x03
#define REP_HOST_UNREACHABLE    0x04
#define REP_CONNECTION_REFUSED  0x05
#define REP_TTL_EXPIRED         0x06
#define REP_CMD_NOT_SUPPORTED   0x07
#define REP_ADDR_NOT_SUPPORTED  0x08

typedef struct method_request_s method_req_t;
struct method_request_s {
        char ver;
        char nmethods;
        char method[255];
};

typedef struct method_response_s method_res_t;
struct method_response_s {
        char ver;
        char method;
};

typedef struct socks_request_s socks_req_t;
struct socks_request_s {
        char ver;
        char cmd;
        char rsv;
        char atyp;
        uint32_t addr;
        uint16_t port;
};

typedef struct socks_response_s socks_res_t;
struct socks_response_s {
        char ver;
        char rep;
        char rsv;
        char atyp;
        uint32_t addr;
        uint16_t port;
};

#define APP_HEADER_TYPE_DATA    0x00U
#define APP_HEADER_TYPE_NEW     0x01U
#define APP_HEADER_TYPE_CONNECT 0x02U
#define APP_HEADER_TYPE_CLOSE   0x03U

#define APP_HEADER_LENGTH       (sizeof(app_hdr_t))
#define APP_DATA_POINT(pkg)     ((pkg)->pdu + APP_HEADER_LENGTH)
#define APP_HEADER_POINT(pkg)   ((pkg)->pdu)

typedef struct app_hdr_s app_hdr_t;
struct app_hdr_s {
        uint8_t type;
};

static int create_and_bind(const char *port);
static int app_input(event_t *ev);
static int app_output(event_t *ev);
static int app_connect(int fd);
static int client_input(app_t *app);
static int client_output(packet_t *pkg);
static int app_check_write_queue();
static app_t *app_find_by_fd(int fd);
static int app_close(app_t *app);
static int app_output_finish(packet_t *pkg);
static int new_client_cli(packet_t *pkg);
static int new_client_ser(packet_t *pkg);
static int connect_client_cli(packet_t *pkg);
static int connect_client_ser(packet_t *pkg);
static int trans_data(packet_t *pkg);
static int close_client(packet_t *pkg);

static queue_t *app_list;
static queue_t *write_list;
static app_ctl_t app_ctl;
static app_t *client;

/*
 * The Application Module initialize function.
 * first, create two queue for clients and packet cache,
 * then, create a socket and listen a port that clients will connect,
 * last, add an event to the event queue, waiting clients connect
 * or client transfer data.
 */
int app_init()
{
        char *port;
        int   fd;
        event_t *ev;

        // create the application queue
        if (!queue_create(app_list)) {
                APP_ERROR("Can not create application queue.");
                return -1;
        }

        queue_init(app_list);

        // create the write packet cache queue
        if (!queue_create(write_list)) {
                APP_ERROR("Can not create write cache queue.");
                return -1;
        }

        queue_init(write_list);

        // find app listen port
        port = config_find(APP_LISTEN_PORT);
        if (port) {
                app_ctl.port = port;
        } else {
                app_ctl.port = APP_DEFAULT_PORT;
        }

        // create and bind the socket
        d = create_and_bind(app_ctl.port);
        if (fd == -1) {
                return -1;
        }
        app_ctl.fd = fd;

        // listen the socket port
        if (listen(fd, SOMAXCONN) == -1) {
                APP_ERROR(strerror(errno));
                return -1;
        }

        logf_info("APP", "Open socket with port %s successed.", port);

        // add event for clients connect
        if (!event_create(ev)) {
                APP_ERROR("Can not alloc memory for an event.");
                return -1;
        }

        ev->fd     = fd;
        ev->input  = app_input;
        ev->output = app_output;
        ev->flag   = 0;

        set_event_active(ev);
        set_event_read(ev);
        event_add(ev);

        APP_INFO("Initialize the APP MODULE successed.");

        return 0;
}

/*
 * free all the clients and packets in their queue.
 */
int app_exit()
{
        app_t    *app;
        queue_t  *q;
        packet_t *pkg;

        // free clients
        for (q = app_list->next; q != app_list; q = q->next)
        {
                app = queue_data(q, app_t, queue);
                close(app->fd);
                free(app);
        }

        // free packets
        for (q = write_list->next; q != write_list; q = q->next)
        {
                pkg = queue_data(q, packet_t, queue);
                free(pkg);
        }

        return 0;
}

/*
 * create and bind the socket.
 */
static int create_and_bind(const char *port)
{
        int fd, opt;
        struct addrinfo hints;
        struct addrinfo *res, *p;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags    = AI_PASSIVE;

        if (getaddrinfo(NULL, port, &hints, &res) != 0) {
                APP_ERROR(strerror(errno));
                return -1;
        }

        for (p = res; p != NULL; p = p->ai_next)
        {
                fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (fd == -1) {
                        continue;
                }

                // set option to reuse port quickly
                opt = SO_REUSEADDR;
                setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

                if (bind(fd, p->ai_addr, p->ai_addrlen) == 0) {
                        break;
                }

                APP_ERROR(strerror(errno));

                close(fd);
        }

        if (p == NULL) {
                APP_ERROR("Could not bind");
                return -1;
        }

        freeaddrinfo(res);

        return fd;
}

/*
 * when a file descriptor can read, the Event Module will call this function back.
 * while the fd is same as application fd, that means a new client connect,
 * use app_connect to accept it, else use the client_input function to
 * handle data transfer between two client.
 */
static int app_input(event_t *ev)
{
        // new client
        if (ev->fd == app_ctl.fd) {
                return app_connect(ev->fd);
        }

        // data transfer between clients
        app_t *app = app_find_by_fd(ev->fd);
        if (!app) {
                return -1;
        }

        return app->input(app);
}

/*
 * when a file descriptor can write, the Event Module will call this function back.
 * we could find the client by fd first, and then get the packet which is send
 * to this client from the write packet cache queue, and call function client_output
 * to send it.
 */
static int app_output(event_t *ev)
{
        // never send data throuth this fd
        if (ev->fd == app_ctl.fd) {
                return -1;
        }

        packet_t *pkg;
        queue_t  *q;

        // find client
        app_t *app = app_find_by_fd(ev->fd);
        if (!app) {
                return -1;
        }

        // find the first packet send to client
        for (q = write_list->next; q != write_list; q = q->next)
        {
                pkg = queue_data(q, packet_t, queue);
                if (pkg->app == app) {
                        app->output(pkg);
                        break;
                }
        }

        app_check_write_queue();

        return 0;
}

/*
 * check if there are any packet in the write packet cache queue,
 * if any, make the Event Module wait for their write event.
 */
static int app_check_write_queue()
{
        return 0;
}

/*
 * while an client connect, accept it and add it to the client queue.
 * we assume client connect throuth TCP/IP.
 */
static int app_connect(int fd)
{
        struct sockaddr_in client_addr;
        socklen_t addr_len;

        // alloc memory for client
        app_t *app = (app_t*) malloc(sizeof(app_t));
        if (!app) {
                APP_ERROR("Can not alloc memory for an client.");
                return -1;
        }

        addr_len = sizeof(struct sockaddr_in);

        // accept client
        app->fd  = accept(fd, (struct sockaddr*)&client_addr, &addr_len);

        if (app->fd == -1) {
                APP_ERROR(strerror(errno));
                free(app);
                return -1;
        }

        // save client infomation
        app->in_addr = client_addr.sin_addr.s_addr;
        app->in_port = client_addr.sin_port;
        app->state   = s_close;
        app->input   = client_input;
        app->output  = client_output;

        // add it to client queue
        if (app_add(app) == -1) {
                free(app);
                return -1;
        }

        APP_INFO("Connect an application.");

        return 0;
}

/*
 * add a client to the client queue,
 * and add an appropriate event to the event queue too.
 */
int app_add(app_t *app)
{
        event_t *ev;

        if (!app) {
                APP_ERROR("Can not add an invalid application to the application list.");
                return -1;
        }

        // add to client queue
        queue_insert(app_list, &app->queue);

        if (!event_create(ev)) {
                APP_ERROR("Can not alloc memory for a event.");
                return -1;
        }

        // fill the event and add it
        ev->fd     = app->fd;
        ev->input  = app_input;
        ev->output = app_output;
        ev->flag   = 0;

        // we always accept input
        set_event_active(ev);
        set_event_read(ev);

        if (event_add(ev) == -1) {
                return -1;
        }

        APP_DEBUG("Successed to add an application.");

        return 0;
}

#define TOTAL_HEADER_LENGTH 0
#define APP_MAX_LENGTH  1024

/*
 * if a client can read, we alloc a packet, then read and store
 * data in it.
 * accoding to state of client, do differently.
 */
static int client_input(app_t *app)
{
        ssize_t nread;

        // alloc packet
        packet_t *pkg = (packet_t*) malloc(sizeof(packet_t));
        if (!pkg) {
                return -1;
        }

        // because of the size limit of device, just read a fix
        // number of data.
        char *pdu = (char*) malloc(APP_MAX_LENGTH);
        if (!pdu) {
                free(pkg);
                return -1;
        }

        // fill the packet
        // TODO
        pkg->pdu     = pdu + TOTAL_HEADER_LENGTH;
        pkg->buf     = pdu;
        pkg->tot_len = APP_MAX_LENGTH;
        pkg->app     = app;
        pkg->up      = 0;
        pkg->down    = 1;

        nread = read(app->fd, APP_DATA_POINT(pkg), APP_MAX_LENGTH);

        if (nread == -1) {      // error
                APP_ERROR(strerror(errno));
                free(pdu);
                free(pkg);
                return -1;
        } else if (nread == 0) {// client close
                free(pdu);
                free(pkg);
                return app_close(app);
        } else {                // read data
                pkg->len = nread + APP_HEADER_LENGTH;
        }

        switch (app->state)
        {
                // init state
                case s_close:
                {
                        return new_client_cli(pkg);
                }

                // wait for request
                case s_wait_request:
                {
                        return connect_client_cli(pkg);
                }

                // data transfer
                case s_connected:
                {
                        app_hdr_t *app_hdr = (app_hdr_t*) (pkg->pdu - sizeof(app_hdr_t));

                        device_t *dev = device_find_by_name("AM");
                        if (!dev) {
                                return -1;
                        }

                        app_hdr->type = APP_HEADER_TYPE_DATA;

                        pkg->dev  = dev;

                        return device_send(pkg);
                }

                default: return -1;
        }
}

static int client_output(packet_t *pkg)
{
        ssize_t nwrite, tot_write = 0;
        ssize_t nleft = pkg->len;

        while (nleft > 0)
        {
                nwrite = write(pkg->app->fd, pkg->pdu + tot_write, nleft);
                if (nwrite == -1) {
                        if (errno == EINTR) {
                                continue;
                        } else {
                                APP_ERROR(strerror(errno));
                                return -1;
                        }
                } else if (nwrite == 0) {
                        return app_close(pkg->app);
                } else {
                        nleft     -= nwrite;
                        tot_write += nwrite;
                }
        }

        if (nleft == 0) {
                app_output_finish(pkg);
        }

        return tot_write;
}

/*
 * after sending data to client,
 * delete packet from cache queue,
 * and make the Event Module not wait for client's write event.
 */
static int app_output_finish(packet_t *pkg)
{
        event_t *ev;

        // not wait for client's write event
        ev = event_find_by_fd(pkg->app->fd);
        if (!ev) {
                return -1;
        }
        unset_event_write(ev);

        // delete packet
        queue_delete(&pkg->queue);
        free(pkg);

        // check if any other packets need to send
        app_check_write_queue();

        return 0;
}

/*
 * send data to application layer, use by other module,
 * of course, this layer can also use it.
 * there is a type field in application layer header,
 * different value means different method.
 * transfer data method is the way send data to client,
 * others will not reach client.
 */
int app_send(packet_t *pkg)
{
        app_hdr_t *app_hdr = (app_hdr_t*) pkg->pdu;

        switch (app_hdr->type)
        {
                // a client from other host call us to
                // connect the real remote server
                case APP_HEADER_TYPE_NEW:
                {
                        return new_client_ser(pkg);
                }

                // we asked other host to connect the real remote
                // server, it answer the result to us now
                case APP_HEADER_TYPE_CONNECT:
                {
                        return connect_client_ser(pkg);
                }

                // oh, a connection was closed, we should do that too.
                case APP_HEADER_TYPE_CLOSE:
                {
                        return close_client(pkg);
                }

                // transfer data
                case APP_HEADER_TYPE_DATA:
                {
                        return trans_data(pkg);
                }

                default: return -1;
        }
}

/*
 * accoding to the mapping between client port and tcp port,
 * just like NAT, we find the correct client, and tell the
 * Event Module wait for its write evnet, then call back
 * function client_output to send data.
 */
static int trans_data(packet_t *pkg)
{
        // TODO
        // no tcp protocol now, change this part in the future.
        queue_t *q;
        app_t   *app;
        event_t *ev;
                app = client;
                pkg->app  = app;
                pkg->pdu += sizeof(app_hdr_t);
                pkg->len -= sizeof(app_hdr_t);
                queue_insert_tail(write_list, &pkg->queue);
                ev = event_find_by_fd(app->fd);
                if (!ev) {
                        return -1;
                }
                set_event_write(ev);

        return 0;
}

/*
 * accoding to RFC 1928, in the socks5 protocol, the real client
 * will send a method select_request at first, we check that is it
 * valid, and make response to real client.
 */
static int new_client_cli(packet_t *pkg)
{
        // we use the same packet to send the response back,
        // so make these point to the same location.
        method_req_t *request  = (method_req_t*) APP_DATA_POINT(pkg);
        method_res_t *response = (method_res_t*) APP_DATA_POINT(pkg);

        // response application layer header
        app_hdr_t *app_hdr = (app_hdr_t*) APP_HEADER_POINT(pkg);

        // valid the socks version
        if (request->ver == SOCKS_VERSION) {
                response->method = METHOD_DEFAULT;
                pkg->app->state  = s_wait_request;
        } else {
                response->method = METHOD_ERROR;
        }

        // send data back to real client
        app_hdr->type = APP_HEADER_TYPE_DATA;

        pkg->len  = APP_HEADER_LENGTH + sizeof(method_res_t);

        return app_send(pkg);
}

/*
 * a client in other host ask us to connect the real remote server,
 * do it, and send response back.
 */
static int new_client_ser(packet_t *pkg)
{
        // we use the same packet to send the response back,
        // so make these point to the same location.
        socks_req_t *request  = (socks_req_t*) APP_DATA_POINT(pkg);
        socks_res_t *response = (socks_res_t*) APP_DATA_POINT(pkg);

        // response application layer header
        app_hdr_t *app_hdr = (app_hdr_t*) APP_HEADER_POINT(pkg);

        // set error response
        app_hdr->type = APP_HEADER_TYPE_CLOSE;
        response->rep = REP_GENREAL_FAILURE;

        // connect the real remote server
        struct sockaddr_in cin;

        // we assume the address type of socks 5 request is IPv4
        memset((void *)&cin, 0, sizeof(struct sockaddr_in));
        cin.sin_family      = AF_INET;
        cin.sin_addr.s_addr = request->addr;
        cin.sin_port        = request->port;

        int real_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (real_sock == -1) {
                APP_ERROR(strerror(errno));
                goto new_error;
        }

        if (connect(real_sock, (struct sockaddr*)&cin, sizeof(struct sockaddr_in)) == -1) {
                APP_ERROR(strerror(errno));
                close(real_sock);
                goto new_error;
        }

        // after connection, alloc a client and add it to client queue.
        // this client connect to real server, a client in other host
        // connect to real client, all the data transfer betwoon real client
        // and real server will throuth this two client in stack.
        // if connect failed, skip this part.
        app_t *app = (app_t*) malloc(sizeof(app_t));
        if (!app) {
                APP_ERROR(strerror(errno));
                close(real_sock);
                goto new_error;
        }

        // fill data infomation
        app->in_addr = cin.sin_addr.s_addr;
        app->in_port = cin.sin_port;
        app->fd      = real_sock;
        app->state   = s_connected;      // connected, can use for data transfer later
        app->input   = client_input;
        app->output  = client_output;

        if (app_add(app) == -1) {
                close(real_sock);
                free(app);
                return -1;
        }

        // we set error type above, if connect and alloc successed,
        // turn it to connect type, tell other host we connected,
        // else say error to them.
        app_hdr->type = APP_HEADER_TYPE_CONNECT;
        response->rep = REP_SUCCEEDED;

new_error:

        // fill the response
        response->ver  = SOCKS_VERSION;
        response->rsv  = 0x00;
        response->atyp = ATYP_IPV4;
        response->addr = 0xFFFFFFFF;
        response->port = 0xFFFF;

        // TODO
        // we don't have protocol now, so need to find device by ourself,
        // don't need in the future.
        device_t *dev = device_find_by_name("AM");
        if (!dev) {
                return -1;
        }

        pkg->dev = dev;
        pkg->pdu = (char*) app_hdr;

        client = app;

        return device_send(pkg);
}

/*
 * accoding to RFC 1928, real client will send a request including
 * real server address and port this time, but we just assume
 * the address type is IPv4, if other, it must be error.
 * then we send this request to other host, and wait for their response.
 */
static int connect_client_cli(packet_t *pkg)
{
        // we use the same packet to send request to other host
        // so make these point to the same location.
        socks_req_t *request  = (socks_req_t*) APP_DATA_POINT(pkg);
        socks_res_t *response = (socks_res_t*) APP_DATA_POINT(pkg);

        // application layer header
        app_hdr_t *app_hdr = (app_hdr_t*) APP_HEADER_POINT(pkg);

        // valid request
        int error_flag = 0;
        if (request->ver != SOCKS_VERSION) {
                APP_ERROR("Not supported socks protocol version.");
                error_flag = 1;
        } else if (request->cmd != CMD_CONNECT) {
                APP_ERROR("Not supported socks protocol command.");
                error_flag = 1;
        } else if (request->rsv) {
                APP_ERROR("Socks protocol format error.");
                error_flag = 1;
        }

        response->ver = SOCKS_VERSION;
        response->rsv = 0x00;
        // valid request failed, send error response to real client
        if (error_flag) {
                response->rep  = REP_GENREAL_FAILURE;
                response->atyp = ATYP_IPV4;
                response->addr = 0xFFFFFFFF;
                response->port = 0xFFFF;

                app_hdr->type = APP_HEADER_TYPE_DATA;
                pkg->len      = APP_HEADER_LENGTH + sizeof(socks_res_t);

                pkg->app->state = s_close;

                return app_send(pkg);
        // valid request successed, send to other host
        } else {
                // TODO
                device_t *dev = device_find_by_name("AM");
                if (!dev) {
                        return -1;
                }

                app_hdr->type = APP_HEADER_TYPE_NEW;

                pkg->dev = dev;
                pkg->len = APP_HEADER_LENGTH + sizeof(socks_res_t);

                return device_send(pkg);
        }
}

/*
 * the other side is connected, we should find the appropriate
 * client, and turn it to data transfer type.
 */
static int connect_client_ser(packet_t *pkg)
{
        app_t   *app;
        queue_t *q;

        for (q = app_list->next; q != app_list; q = q->next)
        {
                app = queue_data(q, app_t, queue);
                if (app->state == s_wait_request) {
                        pkg->app   = app;
                        app_hdr_t *app_hdr = (app_hdr_t*) pkg->pdu;
                        app_hdr->type = APP_HEADER_TYPE_DATA;
                        app->state = s_connected;
                        return app_send(pkg);
                }
        }

        return 0;
}

static int close_client(packet_t *pkg)
{
        return 0;
}

static app_t *app_find_by_fd(int fd)
{
        app_t   *app;
        queue_t *q;

        for (q = app_list->next; q != app_list; q = q->next)
        {
                app = queue_data(q, app_t, queue);
                if (app->fd == fd) {
                        return app;
                }
        }

        return NULL;
}

/*
 * three step, find and delete appropriate event first,
 * then delete and free client, ask the other side delete
 * the client last.
 */
static int app_close(app_t *app)
{
        if (!app) {
                return -1;
        }

        // delete event
        event_t *ev = event_find_by_fd(app->fd);
        if (!ev) {
                return -1;
        }

        if (event_delete(ev) == -1) {
                return -1;
        }

        // delete client
        queue_delete(&app->queue);

        free(app);

        // TODO
        // no tcp protocol now, change this part in the future.

        return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include "config.h"
#include "device.h"
#include "log.h"
#include "packet.h"

#define AQUASENT_BUFFER_SIZE(mtu) ((mtu) * 2 + 300)

#define AQUASENT_CONFIG_PORT      "aquasent_port"
#define AQUASENT_DEFAULT_PORT     "/dev/ttyUSB0"
#define AQUASENT_CONFIG_BAUD      "aquasent_baud"
#define AQUASENT_DEFAULT_BAUD     "115200"

#define AQUASENT_CONFIG_NAME      "aquasent_name"
#define AQUASENT_DEFAULT_NAME     "AM"
#define AQUASENT_CONFIG_MTU       "aquasent_mtu"
#define AQUASENT_DEFAULT_MTU      1024
#define AQUASENT_CONFIG_IP_ADDR   "aquasent_ip_addr"
#define AQUASENT_DEFAULT_IP_ADDR  0
#define AQUASENT_CONFIG_NETMASK   "aquasent_netmask"
#define AQUASENT_DEFAULT_NETMASK  0
#define AQUASENT_CONFIG_GATEWAY   "aquasent_gateway"
#define AQUASENT_DEFAULT_GATEWAY  0
#define AQUASENT_CONFIG_MAC_ADDR  "aquasent_mac_addr"
#define AQUASENT_DEFAULT_MAC_ADDR 0

#define AQUASENT_ERROR(s) log_error("AQUA", (s))
#define AQUASENT_WARN(s)  log_warn ("AQUA", (s))
#define AQUASENT_INFO(s)  log_info ("AQUA", (s))
#define AQUASENT_DEBUG(s) log_debug("AQUA", (s))

#define dbuf_space(b)           ((b)->buf + (b)->len)
#define dbuf_space_len(b)       ((b)->tot_len - (b)->len)

int aquasent_init();
int aquasent_exit();
int handle_mmoky();
int handle_mmtdn();
int handle_mmrxd();
int aquasent_open(char *port, char *baud);
int aquasent_flush(int fd, int flag);
int aquasent_input(device_t *d);
int aquasent_output(packet_t *pkg);
int byte_to_hex(char *hex, const unsigned char *src, size_t size);
int hex_to_byte(char *dst, const char *hex, size_t size);

enum aquasent_read_state {
        s_init,
        s_start,
        s_dollar,
        s_m,
        s_mm,

        s_mme,
        s_mmer,
        s_mmerr,
        s_comma_after_mmerr,
        s_mmerr_cmd,
        s_comma_after_mmerr_cmd,
        s_mmerr_code,
        s_cr_after_mmerr,

        s_mmt,
        s_mmtd,
        s_mmtdn,
        s_comma_after_mmtdn,
        s_mmtdn_result,
        s_comma_after_mmtdn_result,
        s_mmtdn_pn,
        s_cr_after_mmtdn,

        s_mmo,
        s_mmok,
        s_mmoky,
        s_comma_after_mmoky,
        s_mmoky_cmd,
        s_comma_after_mmoky_cmd,
        s_cr_after_mmoky,

        s_mmr,
        s_mmrx,
        s_mmrxd,
        s_comma_after_mmrxd,
        s_mmrxd_src,
        s_comma_after_mmrxd_src,
        s_mmrxd_dst,
        s_comma_after_mmrxd_dst,
        s_mmrxd_data,
        s_cr_after_mmrxd,
};

enum aquasent_write_state {
        s_ready,
        s_wait_mmoky,
        s_wait_mmtdn,
};

typedef struct dbuf_s dbuf_t;
struct dbuf_s {
        char *buf;
        int   len;
        int   tot_len;
};

typedef struct device_aquasent_s device_aquasent_t;
struct device_aquasent_s {
        char *port;
        char *baud;
};

// aquasent device info
static device_aquasent_t device_aquasent;
// read and write buffer
static dbuf_t rbuf;
static dbuf_t wbuf;
// aquasent device state
static enum aquasent_read_state read_state;
static enum aquasent_write_state write_state;
// cache for write packet
static packet_t *pkg_cache;

#define hex_to_num(c) ((unsigned char)(c>='A' ? c-'A'+10 : c-'0'))

static const char hex_table[] = 
{'0', '1', '2', '3',
 '4', '5', '6', '7',
 '8', '9', 'A', 'B',
 'C', 'D', 'E', 'F'};

int aquasent_init()
{
        char *c;
        int fd;
        device_t *d;

        // register aquasent modem to device module
        d = (device_t*) malloc(sizeof(device_t));
        if (!d) {
                return -1;
        }
 
        // cofnigure serail port
        c = config_find(AQUASENT_CONFIG_PORT);
        if (c) {
                device_aquasent.port = c;
        } else {
                device_aquasent.port = AQUASENT_DEFAULT_PORT;
        }

        // cofnigure serial baud
        c = config_find(AQUASENT_CONFIG_BAUD);
        if (c) {
                device_aquasent.baud = c;
        } else {
                device_aquasent.baud = AQUASENT_DEFAULT_BAUD;
        }

        // cofnigure device name
        c = config_find(AQUASENT_CONFIG_NAME);
        if (c) {
                d->name[0] = c[0];
                d->name[1] = c[1];
        } else {
                d->name[0] = 'A';
                d->name[1] = 'M';
        }

        // cofnigure mac adress infomation
        c = config_find(AQUASENT_CONFIG_MAC_ADDR);
        if (c) {
                d->mac_addr = atoi(c);
        } else {
                d->mac_addr = AQUASENT_DEFAULT_MAC_ADDR;
        }

        // cofnigure ip adress infomation
        c = config_find(AQUASENT_CONFIG_IP_ADDR);
        if (c) {
                d->ip_addr = atoi(c);
        } else {
                d->ip_addr = AQUASENT_DEFAULT_IP_ADDR;
        }
        c = config_find(AQUASENT_CONFIG_NETMASK);
        if (c) {
                d->netmask = atoi(c);
        } else {
                d->netmask = AQUASENT_DEFAULT_NETMASK;
        }
        c = config_find(AQUASENT_CONFIG_GATEWAY);
        if (c) {
                d->gateway = atoi(c);
        } else {
                d->gateway = AQUASENT_DEFAULT_GATEWAY;
        }

        // cofnigure mtu
        c = config_find(AQUASENT_CONFIG_MTU);
        if (c) {
                d->mtu = atoi(c);
        } else {
                d->mtu = AQUASENT_DEFAULT_MTU;
        }

        // open aquasent modem
        if ((fd = aquasent_open(device_aquasent.port, device_aquasent.baud)) == -1) {
                return -1;
        } else {
                logf_info("AQUA", "Open aquasent with port %s and baud rate %s successed.",
                        device_aquasent.port, device_aquasent.baud);
                d->fd = fd;
        }

        // flush serial buffer
        if (aquasent_flush(fd, 0) == -1) {
                AQUASENT_ERROR("Can not to flush serial data.");
                close(fd);
                return -1;
        }

        // map device function
        d->input  = aquasent_input;
        d->output = aquasent_output;
        d->exit   = aquasent_exit;
        d->flag   = 0;
        d->state  = 0;

        set_dev_read_available(d);
        set_dev_write_available(d);

        if (device_add(d) == -1) {
                close(fd);
                return -1;
        }

        // alloc read buffer space
        rbuf.tot_len = AQUASENT_BUFFER_SIZE(d->mtu);
        rbuf.len = 0;
        rbuf.buf = (char*) malloc(rbuf.tot_len);
        if (!rbuf.buf) {
                close(fd);
                return -1;
        }

        // alloc write buffer space
        wbuf.tot_len = AQUASENT_BUFFER_SIZE(d->mtu);
        wbuf.len = 0;
        wbuf.buf = (char*) malloc(wbuf.tot_len);
        if (!wbuf.buf) {
                close(fd);
                return -1;
        }

        AQUASENT_INFO("Initialize the AQUASENT MODULE successed.");

        return 0;
}

int aquasent_exit()
{
        return 0;
}

int aquasent_open(char *port, char *baud)
{
        struct termios tio;     
        int bd, fd, rv;

        if ((fd = open(port, O_RDWR)) == -1) {
                AQUASENT_ERROR("Can not to open aquasent.");
                return -1;
        }

        switch(atoi(baud))
        {
                case 9600:
                        bd = B9600;
                        break;
                case 19200:
                        bd = B19200;
                        break;
                case 38400:
                        bd = B38400;
                        break;
                case 115200:
                        bd = B115200;
                        break;
                default:
                        close(fd);
                        return -1;
        }

        memset(&tio,0,sizeof(tio));
        tio.c_iflag     = 0;
        tio.c_oflag     = 0;
        tio.c_cflag     = CS8|CREAD|CLOCAL;
        tio.c_lflag     = 0;
        tio.c_cc[VMIN]  = 1;
        tio.c_cc[VTIME] = 5;
        rv  = cfsetospeed(&tio, bd);
        rv |= cfsetispeed(&tio, bd);
        rv |= tcsetattr(fd, TCSANOW, &tio);
        if(rv) {
                AQUASENT_ERROR("Can not set parameter to aquasent.");
                close(fd);
                return -1;
        }
        
        return fd;
}

int aquasent_flush(int fd, int flag)
{
        return tcflush(fd, flag);
}

#define IS_DOLLAR(c)    ((c) == '$')
#define IS_COMMA(c)     ((c) == ',')
#define IS_NUMBER(c)    ((c) <= '9' && (c) >= '0')
#define IS_ALPHABET(c)  (((c) <= 'Z' && (c) >= 'A') || ((c) <= 'z' && (c) >= 'a'))
#define IS_CR(c)        ((c) == '\r')
#define IS_LF(c)        ((c) == '\n')
#define IS_CMD_CHAR(c)  ((c) == 'H' || (c) == 'T' || (c) == 'X' || (c) == 'D' ||        \
                         (c) == 'A' || (c) == 'C' || (c) == 'W' || (c) == 'R')
#define IS_VCHAR(c)     (IS_NUMBER(c) || IS_ALPHABET(c) || IS_COMMA(c))
#define IS_HEX(c)       (IS_NUMBER(c) || (c) == 'A' || (c) == 'B' || (c) == 'C' ||      \
                         (c) == 'D' || (c) == 'E' || (c) == 'F')

int aquasent_input(device_t *d)
{
        int i, p;
        char ch;

        int len = read(d->fd, rbuf.buf + rbuf.len, rbuf.tot_len - rbuf.len);
        if (len == -1) {
                return -1;
        }

        p = rbuf.len;
        i = 0;
        while (i < len)
        {
                ch = rbuf.buf[p++];
                i++;
        
                switch(read_state)
                {
                        case s_init:
                        if (IS_DOLLAR(ch)) {
                                read_state = s_dollar;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_dollar:
                        if (ch == 'M') {
                                read_state = s_m;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_m:
                        if (ch == 'M') {
                                read_state = s_mm;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mm:
                        switch (ch)
                        {
                                case 'E':
                                        read_state = s_mme;
                                        break;
                                case 'O':
                                        read_state = s_mmo;
                                        break;
                                case 'R':
                                        read_state = s_mmr;
                                        break;
                                case 'T':
                                        read_state = s_mmt;
                                        break;
                                default:
                                        goto error;
                                        break;
                        }
                                rbuf.len++;
                        break;

                        case s_mme:
                        if (ch == 'R') {
                                read_state = s_mmer;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmer:
                        if (ch == 'R') {
                                read_state = s_mmerr;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmerr:
                        if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmerr;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmerr:
                        if (IS_CMD_CHAR(ch)) {
                                read_state = s_mmerr_cmd;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmerr_cmd:
                        if (IS_CMD_CHAR(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmerr_cmd;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmerr_cmd:
                        if (IS_NUMBER(ch)) {
                                read_state = s_mmerr_code;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmerr_code:
                        if (IS_NUMBER(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_CR(ch)) {
                                read_state = s_cr_after_mmerr;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_cr_after_mmerr:
                        if (IS_LF(ch)) {
                                read_state = s_init;
                                rbuf.len = 0;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmo:
                        if (ch == 'K') {
                                rbuf.len++;
                                read_state = s_mmok;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmok:
                        if (ch == 'Y') {
                                rbuf.len++;
                                read_state = s_mmoky;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmoky:
                        if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmoky;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmoky:
                        if (IS_CMD_CHAR(ch)) {
                                read_state = s_mmoky_cmd;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmoky_cmd:
                        if (IS_CMD_CHAR(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmoky_cmd;
                                rbuf.len++;
                                break;
                        } else if (IS_CR(ch)) {
                                read_state = s_cr_after_mmoky;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmoky_cmd:
                        if (IS_VCHAR(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_CR(ch)) {
                                read_state = s_cr_after_mmoky;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_cr_after_mmoky:
                        if (IS_LF(ch)) {
                                read_state = s_init;
                                rbuf.len = 0;
                                handle_mmoky();
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmr:
                        if (ch == 'X') {
                                read_state = s_mmrx;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        // TODO
                        // $MMRXA
                        case s_mmrx:
                        if (ch == 'D') {
                                read_state = s_mmrxd;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmrxd:
                        if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmrxd;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmrxd:
                        if (IS_NUMBER(ch)) {
                                read_state = s_mmrxd_src;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmrxd_src:
                        if (IS_NUMBER(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmrxd_src;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmrxd_src:
                        if (IS_NUMBER(ch)) {
                                read_state = s_mmrxd_dst;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }
 
                        case s_mmrxd_dst:
                        if (IS_NUMBER(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmrxd_dst;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmrxd_dst:
                        if (IS_HEX(ch)) {
                                read_state = s_mmrxd_data;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmrxd_data:
                        if (IS_HEX(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_CR(ch)) {
                                read_state = s_cr_after_mmrxd;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_cr_after_mmrxd:
                        if (IS_LF(ch)) {
                                read_state = s_init;
                                rbuf.len++;
                                handle_mmrxd();
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmt:
                        if (ch == 'D') {
                                read_state = s_mmtd;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmtd:
                        if (ch == 'N') {
                                read_state = s_mmtdn;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmtdn:
                        if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmtdn;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmtdn:
                        if (IS_NUMBER(ch)) {
                                read_state = s_mmtdn_result;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmtdn_result:
                        if (IS_NUMBER(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_COMMA(ch)) {
                                read_state = s_comma_after_mmtdn_result;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_comma_after_mmtdn_result:
                        if (IS_NUMBER(ch)) {
                                read_state = s_mmtdn_pn;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }

                        case s_mmtdn_pn:
                        if (IS_NUMBER(ch)) {
                                rbuf.len++;
                                break;
                        } else if (IS_CR(ch)) {
                                read_state = s_cr_after_mmtdn;
                                rbuf.len++;
                                break;
                        } else {
                                goto error;
                        }
 
                        case s_cr_after_mmtdn:
                        if (IS_LF(ch)) {
                                read_state = s_init;
                                rbuf.len = 0;
                                handle_mmtdn();
                                break;
                        } else {
                                goto error;
                        }
                        
                }
        }
        return 0;

error:
        read_state = s_init;
        printf("error\n");
        return -1;
}

int byte_to_hex(char *hex, const unsigned char *src, size_t size)
{
        int i;
        for (i = 0; i < size; i++)
        {
                hex[i*2]   = hex_table[src[i] >> 4];
                hex[i*2+1] = hex_table[src[i] & 0x0f];
        }

        return 0;
}

#define CMD_HHTXD_HEADER "$HHTXD,0,0,0,"
#define CMD_HHTXD_HEADER_LENGTH strlen(CMD_HHTXD_HEADER)

int aquasent_output(packet_t *pkg)
{
        if (write_state != s_ready) {
                return -1;
        }

        strncpy(wbuf.buf, CMD_HHTXD_HEADER, CMD_HHTXD_HEADER_LENGTH);
        byte_to_hex(wbuf.buf+CMD_HHTXD_HEADER_LENGTH, pkg->pdu, pkg->len);
        strncpy(wbuf.buf+CMD_HHTXD_HEADER_LENGTH+pkg->len*2, "\r\n", 2);

        int nwrite = 0, len;
        int nleft  = CMD_HHTXD_HEADER_LENGTH + pkg->len * 2 + 2;

        while (nleft > 0)
        {
                len = write(pkg->dev->fd, wbuf.buf, nleft);

                if (len == -1) {
                        return -1;
                } else if (len == 0) {
                        break;
                } else {
                        nleft  -= len;
                        nwrite += len;
                }
        }

        write_state = s_wait_mmoky;

        pkg_cache = pkg;

        return device_output_finish_part(pkg);
}

int handle_mmoky()
{
        int i;
        for (i = 0; rbuf.buf[i] != ','; i++)
                ;
        i++;

        if (strncmp(rbuf.buf + i, "MMRXD", 5)) {
                write_state = s_wait_mmtdn;
        }

        return 0;
}

int handle_mmtdn()
{
        write_state = s_ready;

        set_dev_write_available(pkg_cache->dev);

        device_output_finish(pkg_cache);

        pkg_cache = NULL;

        return 0;
}

int handle_mmrxd()
{
        packet_t *pkg = (packet_t*) malloc(sizeof(packet_t));
        if (!pkg) {
                AQUASENT_ERROR("Can not alloc memory for the packet.");
                return -1;
        }

        char *pdu = (char*) malloc(rbuf.len / 2);
        if (!pdu) {
                AQUASENT_ERROR("Can not alloc memory for the packet.");
                return -1;
        }
        pkg->pdu  = pdu;
        pkg->up   = 1;
        pkg->down = 0;

        int comma_num = 0;
        int i;
        for (i = 0; comma_num < 3; i++)
        {
                if (IS_COMMA(rbuf.buf[i])) {
                        comma_num++;
                }
        }

        hex_to_byte(pdu, rbuf.buf + i, rbuf.len - i - 2);
        pkg->len = (rbuf.len - i - 2) / 2;
        pkg->tot_len = pkg->len;
        pkg->ref = 1;

        rbuf.len = 0;

        return device_input_finish(pkg);
}

int hex_to_byte(char *dst, const char *hex, size_t size)
{
        int i = 0;
        for (i = 0; i < size; i+=2)
        {
                dst[i/2] = (hex_to_num(hex[i]) << 4) + hex_to_num(hex[i+1]);
        }
        return 0;
}


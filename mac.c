#include <stdint.h>
#include <string.h>
#include "protocol.h"
#include "packet.h"
#include "mac.h"

#define MAC_BROCAST_ADDRESS 0U

int mac_input(packet_t *pkg);
int mac_output(packet_t *pkg);
int mac_checksum(char *data, size_t len, crc32_t crc);

int mac_init()
{
        ptc_t *ptc = (ptc_t*) malloc(sizeof(ptc_t));
        if (!ptc) {
                return -1;
        }

        ptc->id   = MAC_PROTOCOL_ID;
        ptc->up   = mac_input;
        ptc->down = mac_output;

        ptc_add(ptc);

        return 0;
}

int mac_exit()
{
}

int mac_input(packet_t *pkg)
{
        mac_hdr_t *hdr = (mac_hdr_t*) pkg->pdu;

        pkg->mac_hdr.src = hdr->src;
        pkg->mac_hdr.dst = hdr->dst;
        pkg->mac_hdr.up  = hdr->up;
        pkg->mac_hdr.crc = hdr->crc;
        // pkg->up          = hdr->up;
        pkg->up          = 0;
        pkg->down        = MAC_PROTOCOL_ID;
        

        if (mac_checksum(pkg->pdu + MAC_HEADER_LENGTH, pkg->len - MAC_HEADER_LENGTH, pkg->mac_hdr.crc) == -1) {
                return -1;
        }

        pkg->pdu += MAC_HEADER_LENGTH;
        pkg->len -= MAC_HEADER_LENGTH;

        return 0;
}

int mac_checksum(char *data, size_t len, crc32_t crc)
{
        return 0;
}

int mac_output(packet_t *pkg)
{
        static mac_hdr_t hdr;
        
        hdr.src = 1;
        hdr.dst = MAC_BROCAST_ADDRESS;
        hdr.up  = pkg->up;

        memcpy(pkg->pdu + MAC_HEADER_LENGTH, &hdr, MAC_HEADER_LENGTH);

        pkg->up   = MAC_PROTOCOL_ID;
        pkg->down = 0;

        return 0;
}


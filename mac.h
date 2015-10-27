#ifndef _MAC_H_
#define _MAC_H_

#include <stdint.h>

#define MAC_PROTOCOL_ID   1U

#define MAC_HEADER_LENGTH (1+1+1+4)

typedef uint8_t  ptc_id_t;
typedef uint8_t  mac_addr_t;
typedef uint32_t crc32_t;

typedef struct mac_hdr_s mac_hdr_t;
struct mac_hdr_s {
        mac_addr_t src;
        mac_addr_t dst;
        ptc_id_t   up;
        crc32_t    crc;
};

#endif

#ifndef __RTP_H
#define __RTP_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAYLOAD_MAX 1461

#define MAX_WINDOW 20000
#define MAX_RETRY 50
#define RETRANSIMISSION_TIME 100 // ms

// flags in the rtp header
typedef enum RtpHeaderFlag {
    RTP_SYN = 0b0001,
    RTP_ACK = 0b0010,
    RTP_FIN = 0b0100,
} rtp_header_flag_t;

typedef struct __attribute__((__packed__)) RtpHeader {
    uint32_t seq_num;  // Sequence number
    uint16_t length;   // Length of data; 0 for SYN, ACK, and FIN packets
    uint32_t checksum; // 32-bit CRC
    uint8_t flags;     // See at `RtpHeaderFlag`
} rtp_header_t;

typedef struct __attribute__((__packed__)) RtpPacket {
    rtp_header_t rtp;          // header
    char payload[PAYLOAD_MAX]; // data
} rtp_packet_t;

#define RTP_PACKET_LEN(p) (sizeof(rtp_header_t) + (p)->rtp.length)

uint32_t RtpGetChecksum(rtp_packet_t *packet)
{
    packet->rtp.checksum = 0;
    return compute_checksum(packet, RTP_PACKET_LEN(packet));
}

bool RtpBadChecksum(rtp_packet_t *packet)
{
    uint32_t c = packet->rtp.checksum;
    return c != RtpGetChecksum(packet);
}

// send RTP packet using UDP
void RtpSendPacket(int sock, const struct sockaddr_in *dstAddr, 
                   const rtp_packet_t *packet)
{
    ssize_t r;
    do {
        r = sendto(sock, packet, RTP_PACKET_LEN(packet), 0, 
                   (struct sockaddr *)dstAddr, sizeof(*dstAddr));
        if (r == -1 && errno != EINTR)
        {
            LOG_FATAL("RtpSendPacket: %d\n", errno);
        }
    } 
    while (r == -1);
    if (r != RTP_PACKET_LEN(packet))
    {
        LOG_FATAL("RtpSendPacket");
    }
}

// send RTP command packet
void RtpSendEmptyPacket(int sock, const struct sockaddr_in *dstAddr,
                        rtp_packet_t *packet, 
                        uint32_t seq_num, uint8_t flags)
{
    packet->rtp.seq_num = seq_num;
    packet->rtp.length = 0;
    packet->rtp.flags = flags;
    packet->rtp.checksum = RtpGetChecksum(packet);
    RtpSendPacket(sock, dstAddr, packet);
}

void RtpSendDataPacket(int sock, const struct sockaddr_in *dstAddr,
                       rtp_packet_t *packet,
                       uint32_t seq_num, uint16_t length)
{
    packet->rtp.seq_num = seq_num;
    packet->rtp.length = length;
    packet->rtp.flags = 0;
    packet->rtp.checksum = RtpGetChecksum(packet);
    RtpSendPacket(sock, dstAddr, packet);
}

// return true when received uncorrupted data
bool RtpReceivePacket(int sock, const struct sockaddr_in *srcAddr, 
                      rtp_packet_t *packet)
{
    socklen_t addrlen = sizeof(*srcAddr);
    ssize_t r;
    do 
    {
        r = recvfrom(sock, packet, sizeof(rtp_packet_t), 0,
                     (struct sockaddr *)srcAddr, &addrlen);
        if (r == -1)
        {
            if (errno != EINTR)
            {
                LOG_FATAL("RtpReceivePacket");
            }
            continue;
        }
        if (r != RTP_PACKET_LEN(packet) || RtpBadChecksum(packet))
        {
            return false;
        }
        return true;
    }
    while (true);
}

#ifdef __cplusplus
}
#endif

#endif // __RTP_H

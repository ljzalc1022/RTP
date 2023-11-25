#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>

#include "mylib.h"
#include "rtp.h"
#include "util.h"

#define CONNECTION_WAIT 2000 // ms

int window;
int mode;

struct sockaddr_in dstAddr;
int sock;

static rtp_packet_t sndPacket, rcvPacket;
static rtp_packet_t snd_buffer[MAX_WINDOW];
static bool acked[MAX_WINDOW];

static int snd_nxt;
static int snd_una;

static int file;

void SendPendingData()
{
    int n;

    while (snd_nxt - snd_una < window)
    {
        n = Rio_readn(file, sndPacket.payload, PAYLOAD_MAX);
        if (n == 0) break;
        RtpSendDataPacket(sock, &dstAddr, &sndPacket, snd_nxt, n);
        snd_buffer[snd_nxt % window] = sndPacket;
        acked[snd_nxt++ % window] = false;
    }
}

void DataTransimission()
{
    int i;

    // fill in initial window
    SendPendingData();
    if (snd_una == snd_nxt)
    {
        // empty file
        return;
    }

    // timer init
    int tfd;
    struct itimerspec ts;
    tfd = Timerfd_create(CLOCK_MONOTONIC, 0);
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = RETRANSIMISSION_TIME / 1000;
    ts.it_value.tv_nsec = (RETRANSIMISSION_TIME % 1000) * 1000000;
    Timerfd_settime(tfd, 0, &ts, NULL);

    // epoll init
    struct epoll_event ev;
    int epfd = Epoll_create(2);
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    Epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    Epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev);

    while (true)
    {
        Epoll_wait(epfd, &ev, 1, -1);
        if (ev.data.fd == sock)
        {
            if (RtpReceivePacket(sock, NULL, &rcvPacket) &&
                rcvPacket.rtp.flags == RTP_ACK)
            {
                if (mode == 0)
                {
                    // GBN receive ACK
                    if (rcvPacket.rtp.seq_num <= snd_una) continue;
                    snd_una = rcvPacket.rtp.seq_num;
                }
                else if (mode == 1)
                {
                    // SR receive ACK
                    if (rcvPacket.rtp.seq_num < snd_una) continue;
                    acked[rcvPacket.rtp.seq_num % window] = true;
                    while (acked[snd_una % window] && 
                           snd_una < snd_nxt)
                    {
                        snd_una++;
                    }
                }
                Timerfd_settime(tfd, 0, &ts, NULL);
                SendPendingData();
                if(snd_una == snd_nxt)
                {
                    // all data sent and acked
                    break;
                }
            }
        }
        else if (ev.data.fd == tfd)
        {
            // timeout
            if (mode == 0)
            {
                // GBN retransmission
                for (i = snd_una; i < snd_nxt; i++)
                {
                    RtpSendPacket(sock, &dstAddr, 
                                    &snd_buffer[i % window]);
                }
            }
            else if (mode == 1)
            {
                // SR retransmission
                for (i = snd_una; i < snd_nxt; i++)
                {
                    if (acked[i % window]) continue;
                    RtpSendPacket(sock, &dstAddr, 
                                    &snd_buffer[i % window]);
                }
            }
            Timerfd_settime(tfd, 0, &ts, NULL); // re-arm
        }
    }

    Close(tfd);
    Close(epfd);
}

int main(int argc, char **argv) {
    if (argc != 6) {
        LOG_FATAL("Usage: ./sender [receiver ip] [receiver port] [file path] "
                  "[window size] [mode]\n");
    }

    // your code here
    // parse parameters
    char *rcverIP;
    uint16_t rcverPort;
    char *filepath;
    rcverIP = argv[1];
    rcverPort = atoi(argv[2]);
    filepath = argv[3];
    window = atoi(argv[4]);
    mode = atoi(argv[5]);

    int i, r;

    LOG_DEBUG("sock init\n");
    sock = Socket(AF_INET, SOCK_DGRAM, 0);
    memset(&dstAddr, 0, sizeof(dstAddr));
    dstAddr.sin_family = AF_INET;
    inet_pton(AF_INET, rcverIP, &dstAddr.sin_addr);
    dstAddr.sin_port = htons(rcverPort);

    LOG_DEBUG("epoll init\n");
    struct epoll_event ev;
    int epfd = Epoll_create(1);
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    Epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);

    LOG_DEBUG("connecting\n");

    LOG_DEBUG("first handshake\n");
    RtpSendEmptyPacket(sock, &dstAddr, &sndPacket, 
                       snd_nxt = rand(), RTP_SYN);

    LOG_DEBUG("second handshake\n");
    for (i = 0; i < MAX_RETRY; i++)
    {
        r = Epoll_wait(epfd, &ev, 1, RETRANSIMISSION_TIME);
        if (r > 0)
        {
            if (RtpReceivePacket(sock, NULL, &rcvPacket))
            {
                if (rcvPacket.rtp.flags == (RTP_SYN | RTP_ACK) && 
                    rcvPacket.rtp.seq_num == snd_nxt + 1)
                {
                    break;
                }
            }
        }
        RtpSendEmptyPacket(sock, &dstAddr, &sndPacket, 
                           snd_nxt, RTP_SYN);
    }
    if (i == MAX_RETRY)
    {
        LOG_DEBUG("second handshake failed\n");
        goto cleanup;
    }

    LOG_DEBUG("third handshake\n");
    RtpSendEmptyPacket(sock, &dstAddr, &sndPacket,
                       snd_nxt + 1, RTP_ACK);
    do
    {
        r = Epoll_wait(epfd, &ev, 1, CONNECTION_WAIT);
        if (r > 0)
        {
            if (RtpReceivePacket(sock, NULL, &rcvPacket))
            {
                if (rcvPacket.rtp.flags == (RTP_SYN | RTP_ACK) && 
                    rcvPacket.rtp.seq_num == snd_nxt + 1)
                {
                    RtpSendEmptyPacket(sock, &dstAddr, &sndPacket,
                                       snd_nxt + 1, RTP_ACK);
                }
            }
        }
    }
    while(r);

    LOG_DEBUG("start data transimission\n");

    file = Open(filepath, O_RDONLY);
    snd_nxt++;
    snd_una = snd_nxt;
    DataTransimission();
    Close(file);

    LOG_DEBUG("closing connection\n");

    LOG_DEBUG("first handwave\n");
    RtpSendEmptyPacket(sock, &dstAddr, &sndPacket, snd_nxt, RTP_FIN);

    LOG_DEBUG("second handwave\n");
    for (i = 0; i < MAX_RETRY; i++)
    {
        r = Epoll_wait(epfd, &ev, 1, RETRANSIMISSION_TIME);
        if (r > 0)
        {
            if (RtpReceivePacket(sock, NULL, &rcvPacket))
            {
                if (rcvPacket.rtp.flags == (RTP_FIN | RTP_ACK) && 
                    rcvPacket.rtp.seq_num == snd_nxt)
                {
                    LOG_DEBUG("close succeed\n");
                    break;
                }
            }
        }
        RtpSendEmptyPacket(sock, &dstAddr, &sndPacket, 
                           snd_nxt, RTP_FIN);
    }
    if (i == MAX_RETRY) LOG_DEBUG("close timeout\n");

cleanup:
    Close(sock);
    Close(epfd);
    LOG_DEBUG("Sender: exiting...\n");
    return 0;
}

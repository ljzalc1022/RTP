#include "fcntl.h"
#include "stdbool.h"
#include "string.h"
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mylib.h"
#include "rtp.h"
#include "util.h"

#define WAIT_TIME 5000 // ms
#define CLOSE_WAIT 2000 // ms

static rtp_packet_t rcvPacket;
static rtp_packet_t sndPacket;
static rtp_packet_t rcv_buffer[MAX_WINDOW];
static bool received[MAX_WINDOW];

int main(int argc, char **argv) {
    if (argc != 5) {
        LOG_FATAL("Usage: ./receiver [listen port] [file path] [window size] "
                  "[mode]\n");
    }
    uint16_t lstPort = atoi(argv[1]);
    char * filePath = argv[2];
    int window = atoi(argv[3]);
    int mode = atoi(argv[4]);

    // your code here
    int i, r;
    int y;
    int sock;
    struct sockaddr_in lstAddr;
    struct sockaddr_in srcAddr;

    sock = Socket(AF_INET, SOCK_DGRAM, 0);
    memset(&lstAddr, 0, sizeof(lstAddr));
    lstAddr.sin_family = AF_INET;
    lstAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    lstAddr.sin_port = htons(lstPort);
    Bind(sock, (struct sockaddr *)&lstAddr, sizeof(lstAddr));

    // timer init
    int tfd;
    struct itimerspec ts;
    struct itimerspec ts0;
    tfd = Timerfd_create(CLOCK_MONOTONIC, 0);
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = WAIT_TIME / 1000;
    ts.it_value.tv_nsec = (WAIT_TIME % 1000) * 1000000;
    ts0.it_interval.tv_sec = 0;
    ts0.it_interval.tv_nsec = 0;
    ts0.it_value.tv_sec = 0;
    ts0.it_value.tv_nsec = 0;
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

    LOG_DEBUG("Receiver: connecting\n");

    LOG_DEBUG("Receiver: first handshake\n");
    while (true)
    {
        Epoll_wait(epfd, &ev, 1, -1);
        if (ev.data.fd == tfd)
        {
            LOG_DEBUG("Receiver: no SYN received in 5s\n");
            goto cleanup;
        }
        if(RtpReceivePacket(sock, &srcAddr, &rcvPacket))
        {
            if (rcvPacket.rtp.flags == RTP_SYN)
            {
                y = rcvPacket.rtp.seq_num;
                break;
            }
        }
    }
    Timerfd_settime(tfd, 0, &ts0, NULL);

    LOG_DEBUG("Receiver: second handshake\n");
    RtpSendEmptyPacket(sock, &srcAddr, &sndPacket,
                       y + 1, RTP_ACK | RTP_SYN);
    
    LOG_DEBUG("Receiver: third handshake\n");
    for (i = 0; i < MAX_RETRY; i++)
    {
        r = Epoll_wait(epfd, &ev, 1, RETRANSIMISSION_TIME);
        if (r > 0)
        {
            if (RtpReceivePacket(sock, &srcAddr, &rcvPacket))
            {
                if (rcvPacket.rtp.flags == RTP_ACK &&
                    rcvPacket.rtp.seq_num == y + 1)
                {
                    LOG_DEBUG("Receiver: connected\n");
                    break;
                }
            }
        }
        RtpSendEmptyPacket(sock, &srcAddr, &sndPacket,
                           y + 1, RTP_ACK | RTP_SYN);
    }
    if (i == MAX_RETRY)
    {
        LOG_DEBUG("third handshake failed\n");
        goto cleanup;
    }

    LOG_DEBUG("Receiver: transmissing data\n");
    
    int file = Open3(filePath, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    while (true)
    {
        r = Epoll_wait(epfd, &ev, 1, WAIT_TIME);
        if (r == 0)
        {
            LOG_DEBUG("Receiver: timeout\n");
            Close(file);
            goto cleanup;
        }
        if (RtpReceivePacket(sock, &srcAddr, &rcvPacket))
        {
            int seq_num = rcvPacket.rtp.seq_num;
            if (rcvPacket.rtp.flags == 0)
            {
                if (seq_num <= y - window || seq_num > y + window) continue;
                if (mode == 0)
                {
                    if (seq_num == y + 1) 
                    {
                        ++y;
                        Rio_writen(file, rcvPacket.payload, rcvPacket.rtp.length);
                    }
                    RtpSendEmptyPacket(sock, &srcAddr, &sndPacket, y + 1, RTP_ACK);
                }
                else if (mode == 1)
                {
                    if (seq_num > y && !received[seq_num % window])
                    {
                        received[seq_num % window] = true;
                        rcv_buffer[seq_num % window] = rcvPacket;
                        while (received[(y + 1) % window])
                        {
                            received[++y % window] = false;
                            Rio_writen(file, rcv_buffer[y % window].payload,
                                       rcv_buffer[y % window].rtp.length);
                        }
                    }
                    RtpSendEmptyPacket(sock, &srcAddr, &sndPacket, 
                                       seq_num, RTP_ACK);
                }
            }
            else if (rcvPacket.rtp.flags == RTP_FIN)
            {
                if (seq_num == y + 1) break;
            }
        }
    }
    Close(file);

    LOG_DEBUG("Receiver: closing connection\n");

    LOG_DEBUG("second handwave\n");
    RtpSendEmptyPacket(sock, &srcAddr, &sndPacket, y + 1, RTP_ACK | RTP_FIN);
    do 
    {
        r = Epoll_wait(epfd, &ev, 1, CLOSE_WAIT);
        if (r > 0)
        {
            if (RtpReceivePacket(sock, &srcAddr, &rcvPacket) &&
                rcvPacket.rtp.flags == RTP_FIN && 
                rcvPacket.rtp.seq_num == y + 1)
            {
                RtpSendEmptyPacket(sock, &srcAddr, &sndPacket,
                                   y + 1, RTP_ACK | RTP_FIN);
            }
        }
    }
    while(r);

cleanup:
    Close(epfd);
    Close(sock);
    LOG_DEBUG("Receiver: exiting...\n");
    return 0;
}

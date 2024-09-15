# RTP (Reliable Transport Protocol)

RTP is a reliable transport layer built on top of UDP which can resist 

- corrupted packets (not maliciously tampered)
- duplicated packets
- lost packets

RTP can work under two modes

1. Accumulated ACK - the receiver sends back ACK with a number representing the next expected packets with the minimium sequence number
2. Selected ACK - the receiver sends back ACK for each packets received.

RTP works with a constant congestion window specifying the maxium number of flying (unacknowledged) packets.

> This project is an assignment of the Computer Network course of PKU in 2023
>
> The RTP protocol is designed and the test suits are provided by the course
>
> The implementation is developed indepently by the author

## File Structure

- `CMakeLists.txt` - This project is built by `CMake`



- `src/sender.c` - The implementation of a RTP sender
- `src/receiver.c` - The implementation of a RTP receiver
- `src/rtp.h` - The data structure and some helper functions of RTP 
- `src/test.cpp` - provided by the course for local tests
- `src/util.c, src/util.h` - provided by the course for checksum computation



- `lib/CMakeLists.txt` 

- `lib/mylib.c, lib/mylib.h` - error handling of system calls
- `lib/rio.c, lib/rio.h` - robust IO implementation

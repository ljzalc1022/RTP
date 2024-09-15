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

## How to try it out 

This project aims to operate on the Linux system and is tested on Ubuntu 22.04.

To build this project 

```bash
mkdir build && cd build 
cmake ..
cmake --build .
```

This should produce two executables `sender` and `receiver` under the `build` directory.

To use RTP to send a file over a reliable connection

```bash
./receiver <listen-port> <file-path> <window-size> <mode>  # start receiver first
./sender <receiver-ip> <receiver-port> <file-path> <windows-size> <mode> 
```

Remarks
- the maxium acceptable window is 20000 (configured in `src/rtp.h`)
- The `window-size` and `mode` arguments should be the same for the protocol to function correctly

### To run the official test

To run the official test suits (which injects duplicated packets, packet loss, etc.), first download the official receiver and sender

```bash
git submodule init
git submodule update
```

Then build the project and run the local test

```bash
./rtp_test_all
```

# TCP Client-Server Chat in C

A two-person academic project implementing a terminal-based chat application over TCP/IP in C. The client and server use POSIX sockets; the server handles multiple clients with `poll()` and a linked list of active connections.

## Features

- Register a nickname and list connected users
- Send direct messages, broadcast messages, and echo messages
- Query a user's connection information
- Handle multiple clients concurrently with `poll()`
- Resolve IPv4 and IPv6 addresses through `getaddrinfo()`

## Build and run

```sh
make
./server 8080
```

In another terminal, connect one or more clients:

```sh
./client 127.0.0.1 8080
```

Available commands include `/nick <name>`, `/who`, `/whois <name>`, `/msg <name> <message>`, `/msgall <message>`, and `/quit`.

## Project scope

This portfolio copy contains the final submitted implementation, which reached the user and messaging features (course milestone 2). Authentication, message history, file storage, and file transfer were not completed and are not presented as implemented features.

## Authors

- Nathan Rigal — [@pavardinho](https://github.com/pavardinho)
- Adrien de Chivré — [@dridrileloulou](https://github.com/dridrileloulou)

Developed as a team project at ENSEIRB-MATMECA. Course handouts, starter templates, intermediate drafts, and generated binaries are intentionally omitted from this portfolio copy.

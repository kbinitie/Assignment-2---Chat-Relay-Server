# SP26 CPSC 380 Programming Assignment 2 – Multi-Client Chat Server Using Threads and Sockets

## Contributors
Brent Matthew Ortizo  
Student ID: 2452997  
Email: ortizo@chapman.edu  

Kayode Binitie  
Student ID: 2461327  
Email: binitie@chapman.edu  

---

## Description
This project implements a **multi-client chat server** using **TCP sockets and POSIX threads**. The server accepts connections from multiple clients and relays messages between them. Each message sent by a client is broadcast to all other connected clients in real time.

Clients communicate with the server using **newline-terminated text messages**. The server processes incoming data, reconstructs complete messages, and distributes them to other clients using the required broadcast format.

---

## Design Approach
The chat server is implemented as a **concurrent TCP server** using a **thread-per-client architecture**.

When a new client connects:

1. The server accepts the connection using `accept()`.
2. A **unique client ID** is assigned.
3. A **new pthread** is created to handle communication with that client.

Each client thread continuously reads data from its socket using `recv()` and processes the incoming data stream character-by-character. Messages are accumulated until a newline (`\n`) is encountered, which signals a complete message.

Once a complete message is received, the server broadcasts it to all other connected clients using the required format:

```
<client_id>: <message>\n
```

The client who originally sent the message does **not receive its own broadcast**.

---

## Message Handling and Network I/O
Network programs cannot assume that a single `recv()` call returns an entire message. The server therefore correctly handles:

- **Partial reads**
- **Multiple messages received in a single read**
- **Messages arriving in fragments**

Incoming bytes are stored in an accumulation buffer until a newline character is detected. Only then is the message considered complete and broadcast to other clients.

The assignment requires a maximum message length of **512 bytes (excluding the newline)**. If more than 512 bytes are received before a newline is encountered, the message is discarded until the next newline appears.

---

## Concurrency and Client Management
The server supports multiple clients simultaneously using **POSIX threads (`pthread`)**. Each client connection runs in its own thread, allowing multiple clients to send messages concurrently.

A global client list stores the sockets and IDs of active clients. Access to this shared data structure is protected using a **pthread mutex** to prevent race conditions when clients connect, disconnect, or when messages are broadcast.

The server supports up to **20 simultaneous clients**, which exceeds the assignment requirement of supporting at least 10 clients.

---

## Client Disconnect Handling
Clients disconnect when their TCP connection closes (for example when `nc` exits or a terminal window is closed).

When a disconnect occurs:

- `recv()` returns `0`
- The server removes the client from the active client list
- The associated socket is closed
- The client thread terminates

The server continues running and servicing any remaining connected clients.

---

## Compilation

```
gcc chatserver.c -o chatserver -pthread
```

---

## Execution

```
./chatserver <port>
```

Example:

```
./chatserver 5000
```

Clients can connect using **netcat**:

```
nc localhost 5000
```

Multiple terminal windows can be opened to simulate multiple chat participants.

---

## Assumptions
- Clients send messages terminated by newline characters (`\n`).
- Messages longer than **512 bytes before a newline** are discarded.
- The server limits connections to **20 simultaneous clients**.
- Threads are detached so that resources are automatically reclaimed when client threads terminate.

---

## Collaboration and References
This project was implemented individually using **Git** for version control.

The **man7 Linux manual pages** were used as references for system calls and networking functions including:

- `socket()`
- `bind()`
- `listen()`
- `accept()`
- `recv()`
- `send()`
- `pthread_create()`
- `pthread_detach()`

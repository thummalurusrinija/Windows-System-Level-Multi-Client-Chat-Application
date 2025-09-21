# Project Working Explanation

This document provides a comprehensive explanation of the chat server project, its components, and how it works. It is designed to help you explain the project effectively in an interview.

---

## Project Overview

This project is a multi-client chat application implemented in C++. It consists of a server and client programs that communicate over TCP sockets. The server manages multiple client connections, handles messaging (public and private), and supports server commands for administration.

---

## Key Components

### 1. Server (`server.cpp`)

- **Socket Setup:** The server creates a TCP socket, binds to a port (default 8080), and listens for incoming client connections.
- **Client Management:** Uses a vector of client objects to track connected clients, each with a socket, username, IP address, and connection status.
- **Concurrency:** Accepts client connections and handles each client in a separate thread for concurrent communication.
- **Commands:** Supports server console commands such as:
  - `list`: Lists all connected clients with their usernames and IPs.
  - `kick <username>`: Disconnects a specific client.
  - `stop` or `quit`: Shuts down the server and disconnects all clients.
  - `broadcast <message>`: Sends a message to all clients.
  - `help`: Shows available commands.
- **Client Commands:** Supports client commands like `/list`, `/pm <username> <message>`, `/quit`, and `/help`.
- **Message Handling:** Broadcasts public messages to all clients and supports private messaging between clients.
- **Thread Safety:** Uses mutexes to protect shared data structures like the client list and console output.

### 2. Client (`client.cpp`)

- Connects to the server using TCP sockets.
- Sends username upon connection.
- Supports sending public messages and commands like `/list`, `/pm`, `/quit`, and `/help`.
- Receives and displays messages from the server.

### 3. Supporting Files

- `server_manager.*` and `config_manager.*`: Handle server-to-server communication and configuration management.
- `interserver_protocol.*`: Defines message types and protocols for server communication.

---

## How It Works

1. **Server Startup:**
   - The server initializes Winsock (on Windows), creates a listening socket, and starts accepting client connections.
   - Each client connection is handled in a separate thread.

2. **Client Connection:**
   - Upon connecting, the client sends a username.
   - The server checks for duplicate usernames and either accepts or rejects the connection.
   - The server sends a welcome message and instructions to the client.

3. **Messaging:**
   - Clients can send public messages broadcasted to all other clients.
   - Private messages can be sent using the `/pm` command.
   - The server logs all chat messages.

4. **Server Commands:**
   - The server operator can enter commands in the server console to manage clients and the server.
   - Commands include listing clients, kicking users, broadcasting messages, and stopping the server.

5. **Client Commands:**
   - Clients can use commands to list users, send private messages, quit, or get help.

---

## How to Run

- Build the project using the provided `Makefile` or `build.bat`.
- Run `server.exe` to start the server.
- Run `client.exe` to start a client and connect to the server.
- Use the server console to manage the server and clients.
- Use client commands to interact with other users.

---

## Important Notes

- The server supports up to 50 clients by default.
- Usernames must be unique.
- The server handles graceful shutdown and client disconnections.
- Thread safety is ensured using mutexes.

---

## Summary

This project demonstrates network programming, concurrency, and command parsing in C++. It provides a functional chat server with administrative controls and client interaction features, suitable for real-time communication.

---

Feel free to ask if you need a presentation slide or a more detailed explanation on any part of the project.

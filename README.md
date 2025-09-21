# Chat Application - System-Level Project

## Overview
This project is a system-level chat application implemented in C++ for Windows. It consists of a server and client programs that communicate over TCP sockets using the Windows Sockets API. The application supports multiple clients connecting to the server, public chat messages, private messaging, and basic chat commands.

## Features
- Multi-client chat server supporting up to 50 clients.
- Clients can send public messages visible to all connected users.
- Private messaging between clients using the `/pm <username> <message>` command.
- Basic client commands:
  - `/list` - List all online users.
  - `/pm <username> <message>` - Send a private message.
  - `/quit` - Disconnect from the server.
  - `/help` - Show available commands.
- Server logs client connections, disconnections, and chat activity.
- Thread-safe handling of client connections using C++17 and atomic variables.

## Project Structure
- `server.cpp` - Main server application entry point.
- `client.cpp` - Main client application entry point.
- `server_manager.cpp/h` - Server-side connection and client management.
- `config_manager.cpp` - Configuration management for server settings.
- `interserver_protocol.cpp/h` - Protocol definitions for inter-server communication (if applicable).
- `Makefile` and `build.bat` - Build scripts for compiling server and client executables.
- `README.md` - This file.

## Build Instructions
The project uses g++ with C++17 standard and requires Windows Sockets library (`ws2_32`).

To build the project, run the following command in the project root directory:

```bash
.\build.bat
```

This will compile and generate `server.exe` and `client.exe`.

## Running the Application
1. Start the server:

```bash
.\server.exe
```

The server listens on port 8080 by default.

2. Start one or more clients in separate terminals:

```bash
.\client.exe
```

3. Enter a unique username when prompted.

4. Use chat commands or send messages:
   - `/list` to see online users.
   - `/pm <username> <message>` to send private messages.
   - `/quit` to disconnect.
   - `/help` to see all commands.

## Notes
- The server currently uses default settings if `server_config.txt` is missing.
- Usernames must be unique; duplicate usernames will be rejected.
- The application is designed for Windows and uses Windows-specific socket libraries.


## Contact
For questions or contributions, please contact - thummalurusrinija@gmail.com

#ifndef INTERSERVER_PROTOCOL_H
#define INTERSERVER_PROTOCOL_H

#include <string>
#include <vector>
#include <map>
#include <chrono>

// Server-to-server message types
enum class ServerMessageType {
    // Handshake and connection
    SERVER_HANDSHAKE = 100,
    SERVER_HANDSHAKE_ACK = 101,
    SERVER_REGISTER = 102,
    SERVER_REGISTER_ACK = 103,
    SERVER_DISCONNECT = 104,

    // Message forwarding
    MSG_FORWARD_PUBLIC = 200,
    MSG_FORWARD_PRIVATE = 201,
    MSG_FORWARD_BROADCAST = 202,

    // User management
    USER_JOIN_SERVER = 300,
    USER_LEAVE_SERVER = 301,
    USER_LIST_REQUEST = 302,
    USER_LIST_RESPONSE = 303,

    // Server management
    SERVER_STATUS_REQUEST = 400,
    SERVER_STATUS_RESPONSE = 401,
    SERVER_LIST_REQUEST = 402,
    SERVER_LIST_RESPONSE = 403,

    // Error handling
    ERROR_INVALID_MESSAGE = 500,
    ERROR_AUTHENTICATION_FAILED = 501,
    ERROR_SERVER_FULL = 502,
    ERROR_SERVER_NOT_FOUND = 503
};

// Base message structure for server-to-server communication
struct ServerMessage {
    ServerMessageType type;
    std::string server_id;        // ID of the sending server
    std::string target_server_id; // ID of the target server (empty for broadcast)
    std::chrono::system_clock::time_point timestamp;
    std::string payload;          // Message content

    ServerMessage(ServerMessageType t, const std::string& server, const std::string& payload = "")
        : type(t), server_id(server), target_server_id(""), timestamp(std::chrono::system_clock::now()), payload(payload) {}

    ServerMessage(ServerMessageType t, const std::string& server, const std::string& target, const std::string& payload)
        : type(t), server_id(server), target_server_id(target), timestamp(std::chrono::system_clock::now()), payload(payload) {}
};

// Server information structure
struct ServerInfo {
    std::string server_id;
    std::string server_name;
    std::string host;
    int port;
    int max_clients;
    int current_clients;
    std::chrono::system_clock::time_point last_seen;
    bool is_connected;

    ServerInfo() : port(0), max_clients(0), current_clients(0), is_connected(false) {}

    ServerInfo(const std::string& id, const std::string& name, const std::string& h, int p)
        : server_id(id), server_name(name), host(h), port(p), max_clients(50), current_clients(0),
          last_seen(std::chrono::system_clock::now()), is_connected(true) {}
};

// User information for cross-server communication
struct NetworkUser {
    std::string username;
    std::string server_id;
    std::string server_name;
    std::chrono::system_clock::time_point join_time;
    bool is_online;

    NetworkUser() : is_online(false) {}

    NetworkUser(const std::string& user, const std::string& server, const std::string& server_name)
        : username(user), server_id(server), server_name(server_name),
          join_time(std::chrono::system_clock::now()), is_online(true) {}
};

// Protocol constants
const int DEFAULT_INTERSERVER_PORT = 8081;
const int MAX_SERVERS_PER_NETWORK = 100;
const int SERVER_TIMEOUT_SECONDS = 300; // 5 minutes
const int HANDSHAKE_TIMEOUT_SECONDS = 30;

// Message serialization functions
std::string serializeServerMessage(const ServerMessage& msg);
ServerMessage deserializeServerMessage(const std::string& data);
std::string serializeServerInfo(const ServerInfo& info);
ServerInfo deserializeServerInfo(const std::string& data);

// Utility functions
std::string generateServerId();
std::string getCurrentTimestamp();
bool isServerTimeout(const std::chrono::system_clock::time_point& last_seen);

#endif // INTERSERVER_PROTOCOL_H

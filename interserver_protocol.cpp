#include "interserver_protocol.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

// Message serialization functions
std::string serializeServerMessage(const ServerMessage& msg) {
    std::stringstream ss;

    // Format: TYPE|SERVER_ID|TARGET_SERVER_ID|TIMESTAMP|PAYLOAD
    ss << static_cast<int>(msg.type) << "|"
       << msg.server_id << "|"
       << msg.target_server_id << "|"
       << std::chrono::duration_cast<std::chrono::seconds>(
           msg.timestamp.time_since_epoch()).count() << "|"
       << msg.payload;

    return ss.str();
}

ServerMessage deserializeServerMessage(const std::string& data) {
    std::stringstream ss(data);
    std::string token;
    std::vector<std::string> tokens;

    // Split by '|'
    while (std::getline(ss, token, '|')) {
        tokens.push_back(token);
    }

    if (tokens.size() < 5) {
        throw std::runtime_error("Invalid message format");
    }

    ServerMessage msg(
        static_cast<ServerMessageType>(std::stoi(tokens[0])),
        tokens[1],
        tokens[2],
        tokens[4]
    );

    // Parse timestamp
    auto timestamp_seconds = std::chrono::seconds(std::stoll(tokens[3]));
    msg.timestamp = std::chrono::system_clock::time_point(timestamp_seconds);

    return msg;
}

std::string serializeServerInfo(const ServerInfo& info) {
    std::stringstream ss;

    // Format: ID|NAME|HOST|PORT|MAX_CLIENTS|CURRENT_CLIENTS|LAST_SEEN|CONNECTED
    ss << info.server_id << "|"
       << info.server_name << "|"
       << info.host << "|"
       << info.port << "|"
       << info.max_clients << "|"
       << info.current_clients << "|"
       << std::chrono::duration_cast<std::chrono::seconds>(
           info.last_seen.time_since_epoch()).count() << "|"
       << (info.is_connected ? "1" : "0");

    return ss.str();
}

ServerInfo deserializeServerInfo(const std::string& data) {
    std::stringstream ss(data);
    std::string token;
    std::vector<std::string> tokens;

    // Split by '|'
    while (std::getline(ss, token, '|')) {
        tokens.push_back(token);
    }

    if (tokens.size() < 8) {
        throw std::runtime_error("Invalid server info format");
    }

    ServerInfo info(tokens[0], tokens[1], tokens[2], std::stoi(tokens[3]));
    info.max_clients = std::stoi(tokens[4]);
    info.current_clients = std::stoi(tokens[5]);

    // Parse timestamp
    auto timestamp_seconds = std::chrono::seconds(std::stoll(tokens[6]));
    info.last_seen = std::chrono::system_clock::time_point(timestamp_seconds);

    info.is_connected = (tokens[7] == "1");

    return info;
}

// Utility functions
std::string generateServerId() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    std::string id = "SERVER_";
    for (int i = 0; i < 8; ++i) {
        id += alphanum[dis(gen)];
    }

    return id;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool isServerTimeout(const std::chrono::system_clock::time_point& last_seen) {
    auto now = std::chrono::system_clock::now();
    auto duration = now - last_seen;
    return std::chrono::duration_cast<std::chrono::minutes>(duration).count() > SERVER_TIMEOUT_SECONDS;
}

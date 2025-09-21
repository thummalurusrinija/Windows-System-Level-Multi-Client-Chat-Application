#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include "interserver_protocol.h"

// Server configuration structure
struct ServerConfig {
    // Basic server settings
    std::string server_id;
    std::string server_name;
    int port;
    int max_clients;
    bool enable_interserver_communication;

    // Inter-server communication settings
    int interserver_port;
    std::string network_password; // For server authentication
    std::vector<std::string> allowed_servers; // List of allowed server IDs

    // Network settings
    std::string network_name;
    std::map<std::string, ServerInfo> known_servers;

    // Feature flags
    bool enable_user_sync;
    bool enable_message_forwarding;
    bool enable_server_commands;

    ServerConfig() : port(8080), max_clients(50), enable_interserver_communication(false),
                     interserver_port(DEFAULT_INTERSERVER_PORT), enable_user_sync(true),
                     enable_message_forwarding(true), enable_server_commands(true) {}
};

// Configuration manager class
class ConfigManager {
private:
    ServerConfig config;
    std::string config_file;

public:
    ConfigManager(const std::string& config_file = "server_config.txt");

    // Load and save configuration
    bool loadConfig();
    bool saveConfig();

    // Getters
    const ServerConfig& getConfig() const { return config; }
    ServerConfig& getConfig() { return config; }

    // Setters
    void setServerId(const std::string& id) { config.server_id = id; }
    void setServerName(const std::string& name) { config.server_name = name; }
    void setPort(int p) { config.port = p; }
    void setMaxClients(int max) { config.max_clients = max; }
    void setInterserverPort(int port) { config.interserver_port = port; }
    void setNetworkPassword(const std::string& password) { config.network_password = password; }

    // Server management
    void addKnownServer(const ServerInfo& server);
    void removeKnownServer(const std::string& server_id);
    bool isServerAllowed(const std::string& server_id) const;

    // Utility functions
    std::string getConfigSummary() const;
    void generateDefaultConfig();
};

// Default configuration values
const std::string DEFAULT_SERVER_NAME = "ChatServer";
const std::string DEFAULT_NETWORK_NAME = "ChatNetwork";
const std::string DEFAULT_CONFIG_FILENAME = "server_config.txt";

#endif // SERVER_CONFIG_H

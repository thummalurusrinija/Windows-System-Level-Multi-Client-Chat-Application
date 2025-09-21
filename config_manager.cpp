#include "server_config.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

ConfigManager::ConfigManager(const std::string& config_file)
    : config_file(config_file) {
    generateDefaultConfig();
    loadConfig();
}

bool ConfigManager::loadConfig() {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cout << "Warning: Could not open config file " << config_file << ", using defaults\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key, value;

        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            // Remove whitespace
            key.erase(key.find_last_not_of(" \t") + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            if (key == "server_id") {
                config.server_id = value;
            } else if (key == "server_name") {
                config.server_name = value;
            } else if (key == "port") {
                config.port = std::stoi(value);
            } else if (key == "max_clients") {
                config.max_clients = std::stoi(value);
            } else if (key == "interserver_port") {
                config.interserver_port = std::stoi(value);
            } else if (key == "network_password") {
                config.network_password = value;
            } else if (key == "network_name") {
                config.network_name = value;
            } else if (key == "enable_interserver_communication") {
                config.enable_interserver_communication = (value == "true");
            } else if (key == "enable_user_sync") {
                config.enable_user_sync = (value == "true");
            } else if (key == "enable_message_forwarding") {
                config.enable_message_forwarding = (value == "true");
            } else if (key == "enable_server_commands") {
                config.enable_server_commands = (value == "true");
            }
        }
    }

    file.close();
    return true;
}

bool ConfigManager::saveConfig() {
    std::ofstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not save config file " << config_file << std::endl;
        return false;
    }

    file << "server_id=" << config.server_id << std::endl;
    file << "server_name=" << config.server_name << std::endl;
    file << "port=" << config.port << std::endl;
    file << "max_clients=" << config.max_clients << std::endl;
    file << "interserver_port=" << config.interserver_port << std::endl;
    file << "network_password=" << config.network_password << std::endl;
    file << "network_name=" << config.network_name << std::endl;
    file << "enable_interserver_communication=" << (config.enable_interserver_communication ? "true" : "false") << std::endl;
    file << "enable_user_sync=" << (config.enable_user_sync ? "true" : "false") << std::endl;
    file << "enable_message_forwarding=" << (config.enable_message_forwarding ? "true" : "false") << std::endl;
    file << "enable_server_commands=" << (config.enable_server_commands ? "true" : "false") << std::endl;

    file.close();
    return true;
}

void ConfigManager::addKnownServer(const ServerInfo& server) {
    config.known_servers[server.server_id] = server;
}

void ConfigManager::removeKnownServer(const std::string& server_id) {
    config.known_servers.erase(server_id);
}

bool ConfigManager::isServerAllowed(const std::string& server_id) const {
    // If no allowed servers list is specified, allow all
    if (config.allowed_servers.empty()) {
        return true;
    }

    return std::find(config.allowed_servers.begin(), config.allowed_servers.end(), server_id) != config.allowed_servers.end();
}

std::string ConfigManager::getConfigSummary() const {
    std::stringstream ss;
    ss << "=== Server Configuration ===\n";
    ss << "Server ID: " << config.server_id << "\n";
    ss << "Server Name: " << config.server_name << "\n";
    ss << "Port: " << config.port << "\n";
    ss << "Max Clients: " << config.max_clients << "\n";
    ss << "Inter-server Port: " << config.interserver_port << "\n";
    ss << "Network Name: " << config.network_name << "\n";
    ss << "Inter-server Communication: " << (config.enable_interserver_communication ? "Enabled" : "Disabled") << "\n";
    ss << "User Sync: " << (config.enable_user_sync ? "Enabled" : "Disabled") << "\n";
    ss << "Message Forwarding: " << (config.enable_message_forwarding ? "Enabled" : "Disabled") << "\n";
    ss << "Server Commands: " << (config.enable_server_commands ? "Enabled" : "Disabled") << "\n";
    ss << "Known Servers: " << config.known_servers.size() << "\n";
    return ss.str();
}

void ConfigManager::generateDefaultConfig() {
    if (config.server_id.empty()) {
        config.server_id = generateServerId();
    }

    if (config.server_name.empty()) {
        config.server_name = DEFAULT_SERVER_NAME;
    }

    if (config.network_name.empty()) {
        config.network_name = DEFAULT_NETWORK_NAME;
    }
}


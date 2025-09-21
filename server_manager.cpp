#include "server_manager.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// ServerManager implementation
ServerManager::ServerManager(ConfigManager& config)
    : config_manager(config), running(false), total_messages_sent(0), total_messages_received(0) {
    start_time = std::chrono::system_clock::now();
}

ServerManager::~ServerManager() {
    stop();
}

bool ServerManager::start() {
    if (running) {
        return true;
    }

    running = true;
    network_thread = std::thread(&ServerManager::networkLoop, this);

    logNetworkMessage("Server manager started");
    return true;
}

void ServerManager::stop() {
    if (!running) {
        return;
    }

    running = false;
    message_cv.notify_all();

    if (network_thread.joinable()) {
        network_thread.join();
    }

    // Disconnect all connections
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex));
    connections.clear();

    logNetworkMessage("Server manager stopped");
}

bool ServerManager::connectToServer(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex));

    // Check if already connected
    for (const auto& pair : connections) {
        if (pair.second->getHost() == host && pair.second->getPort() == port) {
            return true;
        }
    }

    auto connection = std::make_unique<InterServerConnection>(this, host, port);
    if (connection->connect()) {
        connections[connection->getServerId()] = std::move(connection);
        logNetworkMessage("Connected to server: " + host + ":" + std::to_string(port));
        return true;
    }

    return false;
}

bool ServerManager::disconnectFromServer(const std::string& server_id) {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex));

    auto it = connections.find(server_id);
    if (it != connections.end()) {
        it->second->disconnect();
        connections.erase(it);
        logNetworkMessage("Disconnected from server: " + server_id);
        return true;
    }

    return false;
}

bool ServerManager::isConnectedToServer(const std::string& server_id) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex));
    return connections.find(server_id) != connections.end();
}

bool ServerManager::sendMessage(const ServerMessage& message) {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex));

    bool sent = false;
    for (const auto& pair : connections) {
        if (pair.second->sendMessage(message)) {
            sent = true;
        }
    }

    if (sent) {
        total_messages_sent++;
    }

    return sent;
}

bool ServerManager::broadcastMessage(const ServerMessage& message) {
    return sendMessage(message);
}

void ServerManager::processMessage(const ServerMessage& message) {
    total_messages_received++;

    switch (message.type) {
        case ServerMessageType::SERVER_HANDSHAKE:
            handleHandshake(message);
            break;
        case ServerMessageType::SERVER_REGISTER:
            handleServerRegister(message);
            break;
        case ServerMessageType::MSG_FORWARD_PUBLIC:
        case ServerMessageType::MSG_FORWARD_PRIVATE:
        case ServerMessageType::MSG_FORWARD_BROADCAST:
            handleMessageForward(message);
            break;
        case ServerMessageType::USER_JOIN_SERVER:
        case ServerMessageType::USER_LEAVE_SERVER:
            handleUserSync(message);
            break;
        case ServerMessageType::SERVER_STATUS_REQUEST:
            handleServerStatus(message);
            break;
        default:
            logNetworkMessage("Unknown message type received: " + std::to_string(static_cast<int>(message.type)));
            break;
    }
}

void ServerManager::discoverServers() {
    // TODO: Implement server discovery
    logNetworkMessage("Server discovery not yet implemented");
}

void ServerManager::registerWithServer(const std::string& server_id) {
    // TODO: Implement server registration
    logNetworkMessage("Server registration not yet implemented");
}

void ServerManager::unregisterFromServer(const std::string& server_id) {
    // TODO: Implement server unregistration
    logNetworkMessage("Server unregistration not yet implemented");
}

std::vector<ServerInfo> ServerManager::getConnectedServers() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex));
    std::vector<ServerInfo> servers;

    for (const auto& pair : connections) {
        ServerInfo info;
        info.server_id = pair.first;
        info.host = pair.second->getHost();
        info.port = pair.second->getPort();
        info.is_connected = pair.second->isConnected();
        info.last_seen = pair.second->getLastActivity();
        servers.push_back(info);
    }

    return servers;
}

std::string ServerManager::getNetworkStatus() const {
    std::stringstream ss;
    ss << "Network Status:\n";
    ss << "Connected servers: " << connections.size() << "\n";
    ss << "Total messages sent: " << total_messages_sent << "\n";
    ss << "Total messages received: " << total_messages_received << "\n";

    auto uptime = std::chrono::system_clock::now() - start_time;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime).count();
    ss << "Uptime: " << minutes << " minutes\n";

    return ss.str();
}

void ServerManager::networkLoop() {
    while (running) {
        cleanupDeadConnections();

        // Process message queue
        std::unique_lock<std::mutex> lock(message_mutex);
        message_cv.wait_for(lock, std::chrono::seconds(1));

        while (!message_queue.empty()) {
            ServerMessage msg = message_queue.front();
            message_queue.pop();
            lock.unlock();

            processMessage(msg);

            lock.lock();
        }
    }
}

void ServerManager::handleHandshake(const ServerMessage& message) {
    logNetworkMessage("Handshake received from: " + message.server_id);
}

void ServerManager::handleServerRegister(const ServerMessage& message) {
    logNetworkMessage("Server registration from: " + message.server_id);
}

void ServerManager::handleMessageForward(const ServerMessage& message) {
    logNetworkMessage("Message forwarded: " + message.payload);
}

void ServerManager::handleUserSync(const ServerMessage& message) {
    logNetworkMessage("User sync: " + message.payload);
}

void ServerManager::handleServerStatus(const ServerMessage& message) {
    logNetworkMessage("Server status request from: " + message.server_id);
}

void ServerManager::cleanupDeadConnections() {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex));

    auto it = connections.begin();
    while (it != connections.end()) {
        auto timeout = std::chrono::minutes(5);
        if (std::chrono::system_clock::now() - it->second->getLastActivity() > timeout) {
            logNetworkMessage("Connection timeout: " + it->first);
            it = connections.erase(it);
        } else {
            ++it;
        }
    }
}

void ServerManager::logNetworkMessage(const std::string& message) {
    std::cout << "[NETWORK] " << message << std::endl;
}

// InterServerConnection implementation
InterServerConnection::InterServerConnection(ServerManager* mgr, const std::string& host, int port)
    : manager(mgr), host(host), port(port), connected(false), connection_socket(INVALID_SOCKET) {
    server_id = host + ":" + std::to_string(port);
}

InterServerConnection::~InterServerConnection() {
    disconnect();
}

bool InterServerConnection::connect() {
    #ifdef _WIN32
        connection_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    #else
        connection_socket = socket(AF_INET, SOCK_STREAM, 0);
    #endif

    if (connection_socket == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        close(connection_socket);
        return false;
    }

    if (::connect(connection_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        close(connection_socket);
        return false;
    }

    connected = true;
    updateActivity();

    // Start receive thread
    receive_thread = std::thread(&InterServerConnection::receiveLoop, this);

    return performHandshake();
}

void InterServerConnection::disconnect() {
    if (!connected) {
        return;
    }

    connected = false;

    if (receive_thread.joinable()) {
        receive_thread.join();
    }

    if (connection_socket != INVALID_SOCKET) {
        close(connection_socket);
        connection_socket = INVALID_SOCKET;
    }
}

bool InterServerConnection::sendMessage(const ServerMessage& message) {
    if (!connected) {
        return false;
    }

    std::lock_guard<std::mutex> lock(socket_mutex);

    std::string serialized = serializeServerMessage(message);
    int result = send(connection_socket, serialized.c_str(), serialized.length(), 0);

    return result != SOCKET_ERROR;
}

void InterServerConnection::receiveLoop() {
    char buffer[4096];

    while (connected) {
        int bytes = recv(connection_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
            break;
        }

        buffer[bytes] = '\0';
        std::string data(buffer);

        try {
            ServerMessage message = deserializeServerMessage(data);
            updateActivity();
            // TODO: Add to message queue for processing
        } catch (const std::exception& e) {
            std::cerr << "Error deserializing message: " << e.what() << std::endl;
        }
    }

    connected = false;
}

bool InterServerConnection::performHandshake() {
    // TODO: Implement handshake protocol
    return true;
}

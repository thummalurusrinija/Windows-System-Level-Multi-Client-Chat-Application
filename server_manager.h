#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <queue>
#include <condition_variable>
#include "interserver_protocol.h"
#include "server_config.h"

// Cross-platform socket includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define close closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

// Forward declarations
class InterServerConnection;

// Server manager class to handle server-to-server communication
class ServerManager {
private:
    std::map<std::string, std::unique_ptr<InterServerConnection>> connections;
    std::mutex connections_mutex;
    std::atomic<bool> running;
    std::thread network_thread;
    std::queue<ServerMessage> message_queue;
    std::mutex message_mutex;
    std::condition_variable message_cv;

    // Server information
    std::string server_id;
    std::string server_name;
    ConfigManager& config_manager;

    // Network statistics
    std::atomic<int> total_messages_sent;
    std::atomic<int> total_messages_received;
    std::chrono::system_clock::time_point start_time;

public:
    ServerManager(ConfigManager& config);
    ~ServerManager();

    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running; }

    // Connection management
    bool connectToServer(const std::string& host, int port);
    bool disconnectFromServer(const std::string& server_id);
    bool isConnectedToServer(const std::string& server_id) const;

    // Message handling
    bool sendMessage(const ServerMessage& message);
    bool broadcastMessage(const ServerMessage& message);
    void processMessage(const ServerMessage& message);

    // Server discovery
    void discoverServers();
    void registerWithServer(const std::string& server_id);
    void unregisterFromServer(const std::string& server_id);

    // Information and statistics
    std::vector<ServerInfo> getConnectedServers() const;
    std::string getNetworkStatus() const;
    int getTotalMessagesSent() const { return total_messages_sent; }
    int getTotalMessagesReceived() const { return total_messages_received; }

private:
    // Network thread function
    void networkLoop();

    // Message processing
    void handleHandshake(const ServerMessage& message);
    void handleServerRegister(const ServerMessage& message);
    void handleMessageForward(const ServerMessage& message);
    void handleUserSync(const ServerMessage& message);
    void handleServerStatus(const ServerMessage& message);

    // Utility functions
    void cleanupDeadConnections();
    void logNetworkMessage(const std::string& message);
    std::string getServerId() const { return server_id; }
};

// Individual server connection handler
class InterServerConnection {
private:
    SOCKET connection_socket;
    std::string server_id;
    std::string host;
    int port;
    std::atomic<bool> connected;
    std::thread receive_thread;
    ServerManager* manager;

    std::chrono::system_clock::time_point last_activity;
    std::mutex socket_mutex;

public:
    InterServerConnection(ServerManager* mgr, const std::string& host, int port);
    ~InterServerConnection();

    bool connect();
    void disconnect();
    bool isConnected() const { return connected; }

    bool sendMessage(const ServerMessage& message);
    std::string getServerId() const { return server_id; }
    std::string getHost() const { return host; }
    int getPort() const { return port; }

    void updateActivity() { last_activity = std::chrono::system_clock::now(); }
    std::chrono::system_clock::time_point getLastActivity() const { return last_activity; }

private:
    void receiveLoop();
    bool performHandshake();
};

#endif // SERVER_MANAGER_H

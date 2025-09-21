#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include "interserver_protocol.h"
#include "server_config.h"
#include "server_manager.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
// #pragma comment(lib, "ws2_32.lib") // Not needed for g++, use -lws2_32 in linker
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

class ChatServer {
private:
    struct Client {
        SOCKET socket;
        std::string username;
        std::string ip_address;
        std::chrono::system_clock::time_point join_time;
        bool active;

        Client(SOCKET s, const std::string& ip)
            : socket(s), ip_address(ip), join_time(std::chrono::system_clock::now()), active(true) {}
    };

    // Server components
    SOCKET server_socket;
    std::vector<std::unique_ptr<Client>> clients;
    std::mutex clients_mutex;
    std::mutex cout_mutex;
    int port;
    int max_clients;
    bool running;

    // Server-to-server communication
    ConfigManager config_manager;
    std::unique_ptr<ServerManager> server_manager;
    
    // Message types for protocol
    enum MessageType {
        MSG_JOIN = 1,
        MSG_LEAVE = 2,
        MSG_CHAT = 3,
        MSG_LIST_USERS = 4,
        MSG_PRIVATE = 5,
        MSG_SERVER_INFO = 6
    };
    
public:
    ChatServer(int p = 8080, int max_c = 50) : port(p), max_clients(max_c), running(false) {
        #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        #endif
    }
    
    ~ChatServer() {
        stop();
        #ifdef _WIN32
        WSACleanup();
        #endif
    }
    
    bool start() {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET) {
            logError("Failed to create socket");
            return false;
        }
        
        // Allow socket reuse
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
                      (char*)&opt, sizeof(opt)) < 0) {
            logError("setsockopt failed");
            return false;
        }
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            logError("Bind failed");
            return false;
        }
        
        if (listen(server_socket, 5) == SOCKET_ERROR) {
            logError("Listen failed");
            return false;
        }
        
        running = true;
        logInfo("Chat server started on port " + std::to_string(port));
        logInfo("Maximum clients: " + std::to_string(max_clients));
        
        // Accept connections in a separate thread
        std::thread accept_thread(&ChatServer::acceptConnections, this);
        accept_thread.detach();
        
        return true;
    }
    
    void stop() {
        running = false;
        if (server_socket != INVALID_SOCKET) {
            close(server_socket);
            server_socket = INVALID_SOCKET;
        }
        
        // Disconnect all clients
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto& client : clients) {
            if (client->active) {
                std::string kick_msg = "Server is shutting down. You have been disconnected.\n";
                send(client->socket, kick_msg.c_str(), kick_msg.length(), 0);
                close(client->socket);
                client->active = false;
            }
        }
        clients.clear();
    }
    
    void runConsole() {
        std::string command;
        logInfo("Server console started. Type 'help' for commands.");

        while (running && std::getline(std::cin, command)) {
            if (command == "help") {
                showHelp();
            } else if (command == "status") {
                showStatus();
            } else if (command == "list") {
                listClients();
            } else if (command.substr(0, 9) == "broadcast") {
                if (command.length() > 10) {
                    broadcastMessage("[SERVER]: " + command.substr(10), nullptr);
                }
            } else if (command == "stop" || command == "quit") {
                logInfo("Shutting down server...");
                stop();
                break;
            } else if (command.substr(0, 4) == "kick") {
                if (command.length() > 5) {
                    std::string username = command.substr(5);
                    // Trim leading and trailing spaces
                    username.erase(0, username.find_first_not_of(" \t\n\r\f\v"));
                    username.erase(username.find_last_not_of(" \t\n\r\f\v") + 1);
                    kickUser(username);
                }
            } else if (command.substr(0, 7) == "connect") {
                if (command.length() > 8) {
                    connectToServer(command.substr(8));
                }
            } else if (command == "servers") {
                listServers();
            } else if (command == "network") {
                showNetworkStatus();
            } else if (command.substr(0, 8) == "sendmsg ") {
                if (command.length() > 8) {
                    sendServerMessage(command.substr(8));
                }
            } else {
                logError("Unknown command. Type 'help' for available commands.");
            }
        }
    }

private:
    void acceptConnections() {
        while (running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            
            SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
            if (client_socket == INVALID_SOCKET) {
                if (running) {
                    logError("Accept failed");
                }
                continue;
            }
            
            // Check max clients
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                if (clients.size() >= static_cast<std::vector<std::unique_ptr<Client>>::size_type>(max_clients)) {
                    std::string msg = "Server full. Try again later.\n";
                    send(client_socket, msg.c_str(), msg.length(), 0);
                    close(client_socket);
                    continue;
                }
            }
            
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            logInfo("New connection from " + client_ip);
            
            // Create client and start handler thread
            auto client = std::make_unique<Client>(client_socket, client_ip);
            std::thread client_thread(&ChatServer::handleClient, this, std::move(client));
            client_thread.detach();
        }
    }
    
    void handleClient(std::unique_ptr<Client> client) {
        char buffer[1024];
        
        // Welcome message and username prompt
        std::string welcome = "=== Welcome to ChatServer ===\nEnter your username: ";
        send(client->socket, welcome.c_str(), welcome.length(), 0);
        
        // Get username
        int bytes = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            close(client->socket);
            return;
        }
        
        buffer[bytes] = '\0';
        client->username = std::string(buffer);
        // Remove newline characters
        client->username.erase(std::remove(client->username.begin(), 
                              client->username.end(), '\n'), client->username.end());
        client->username.erase(std::remove(client->username.begin(), 
                              client->username.end(), '\r'), client->username.end());
        
        if (client->username.empty()) {
            client->username = "Anonymous_" + std::to_string(client->socket);
        }
        
        // Check for duplicate usernames
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (const auto& existing : clients) {
                if (existing->username == client->username && existing->active) {
                    std::string error = "Username already taken. Connection closed.\n";
                    send(client->socket, error.c_str(), error.length(), 0);
                    close(client->socket);
                    return;
                }
            }
            clients.push_back(std::move(client));
        }
        
        Client* client_ptr = clients.back().get();
        logInfo("User '" + client_ptr->username + "' joined from " + client_ptr->ip_address);
        
        // Send join confirmation and instructions
        std::string instructions = 
            "\n=== Successfully joined chat ===\n"
            "Commands:\n"
            "  /list - Show online users\n"
            "  /pm <username> <message> - Private message\n"
            "  /quit - Leave chat\n"
            "  /help - Show this help\n"
            "Just type to send public messages\n\n";
        send(client_ptr->socket, instructions.c_str(), instructions.length(), 0);
        
        // Notify other users
        broadcastMessage("*** " + client_ptr->username + " joined the chat ***", client_ptr);
        
        // Main message loop
        while (running && client_ptr->active) {
            bytes = recv(client_ptr->socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                break;
            }
            
            buffer[bytes] = '\0';
            std::string message(buffer);
            message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
            message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());
            
            if (message.empty()) continue;
            
            processMessage(client_ptr, message);
        }
        
        // Client disconnected
        logInfo("User '" + client_ptr->username + "' disconnected");
        broadcastMessage("*** " + client_ptr->username + " left the chat ***", client_ptr);
        
        client_ptr->active = false;
        close(client_ptr->socket);
        
        // Remove from clients list
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
                     [client_ptr](const std::unique_ptr<Client>& c) {
                         return c.get() == client_ptr;
                     }), clients.end());
    }
    
    void processMessage(Client* sender, const std::string& message) {
        if (message[0] == '/') {
            // Handle commands
            std::istringstream iss(message);
            std::string command;
            iss >> command;
            
            if (command == "/quit") {
                std::string goodbye = "Goodbye!\n";
                send(sender->socket, goodbye.c_str(), goodbye.length(), 0);
                sender->active = false;
            } else if (command == "/list") {
                sendUserList(sender);
            } else if (command == "/help") {
                sendHelp(sender);
            } else if (command == "/pm") {
                std::string target, pm_message;
                iss >> target;
                std::getline(iss, pm_message);
                if (!pm_message.empty()) {
                    pm_message = pm_message.substr(1); // Remove leading space
                    sendPrivateMessage(sender, target, pm_message);
                }
            } else {
                std::string error = "Unknown command. Type /help for available commands.\n";
                send(sender->socket, error.c_str(), error.length(), 0);
            }
        } else {
            // Regular chat message
            std::string formatted_message = getCurrentTime() + " [" + sender->username + "]: " + message;
            broadcastMessage(formatted_message, sender);
            logChat(sender->username, message);
        }
    }
    
    void broadcastMessage(const std::string& message, Client* exclude) {
        std::string full_message = message + "\n";
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        for (auto& client : clients) {
            if (client->active && client.get() != exclude) {
                send(client->socket, full_message.c_str(), full_message.length(), 0);
            }
        }
    }
    
    void sendPrivateMessage(Client* sender, const std::string& target, const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        for (auto& client : clients) {
            if (client->active && client->username == target) {
                std::string pm = "[PRIVATE from " + sender->username + "]: " + message + "\n";
                send(client->socket, pm.c_str(), pm.length(), 0);
                
                std::string confirmation = "[PRIVATE to " + target + "]: " + message + "\n";
                send(sender->socket, confirmation.c_str(), confirmation.length(), 0);
                return;
            }
        }
        
        std::string error = "User '" + target + "' not found.\n";
        send(sender->socket, error.c_str(), error.length(), 0);
    }
    
    void sendUserList(Client* sender) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::string user_list = "\n=== Online Users ===\n";
        
        for (const auto& client : clients) {
            if (client->active) {
                user_list += "- " + client->username + " (" + client->ip_address + ")\n";
            }
        }
        user_list += "Total: " + std::to_string(clients.size()) + " users\n\n";
        
        send(sender->socket, user_list.c_str(), user_list.length(), 0);
    }
    
    void sendHelp(Client* sender) {
        std::string help = 
            "\n=== Chat Commands ===\n"
            "/list - Show online users\n"
            "/pm <username> <message> - Send private message\n"
            "/quit - Leave the chat\n"
            "/help - Show this help\n"
            "Just type normally to send public messages\n\n";
        send(sender->socket, help.c_str(), help.length(), 0);
    }
    
    void showHelp() {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "\n=== Server Console Commands ===\n";
        std::cout << "help      - Show this help\n";
        std::cout << "status    - Show server status\n";
        std::cout << "list      - List connected clients\n";
        std::cout << "broadcast <message> - Send message to all clients\n";
        std::cout << "kick <username> - Disconnect a user\n";
        std::cout << "stop/quit - Shutdown server\n";
        std::cout << "\n=== Server-to-Server Commands ===\n";
        std::cout << "connect <host:port> - Connect to another server\n";
        std::cout << "servers   - List connected servers\n";
        std::cout << "network   - Show network status\n";
        std::cout << "sendmsg <message> - Send message to all connected servers\n\n";
    }
    
    void showStatus() {
        std::lock_guard<std::mutex> lock1(clients_mutex);
        std::lock_guard<std::mutex> lock2(cout_mutex);
        
        std::cout << "\n=== Server Status ===\n";
        std::cout << "Port: " << port << "\n";
        std::cout << "Active clients: " << clients.size() << "/" << max_clients << "\n";
        std::cout << "Server running: " << (running ? "Yes" : "No") << "\n\n";
    }
    
    void listClients() {
        std::lock_guard<std::mutex> lock1(clients_mutex);
        std::lock_guard<std::mutex> lock2(cout_mutex);
        
        std::cout << "\n=== Connected Clients ===\n";
        if (clients.empty()) {
            std::cout << "No clients connected\n\n";
            return;
        }
        
        for (const auto& client : clients) {
            if (client->active) {
                auto duration = std::chrono::system_clock::now() - client->join_time;
                auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
                std::cout << "- " << client->username << " (" << client->ip_address 
                         << ") - Connected " << minutes << " mins ago\n";
            }
        }
        std::cout << "\n";
    }
    
    void kickUser(const std::string& username) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        for (auto& client : clients) {
            if (client->active && client->username == username) {
                std::string kick_msg = "You have been kicked from the server.\n";
                send(client->socket, kick_msg.c_str(), kick_msg.length(), 0);
                client->active = false;
                close(client->socket);
                logInfo("Kicked user: " + username);
                return;
            }
        }
        
        std::lock_guard<std::mutex> cout_lock(cout_mutex);
        std::cout << "User '" << username << "' not found.\n";
    }
    
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "[%H:%M:%S]");
        return ss.str();
    }
    
    void logInfo(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << getCurrentTime() << " [INFO] " << message << "\n";
    }
    
    void logError(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << getCurrentTime() << " [ERROR] " << message << "\n";
    }
    
    void logChat(const std::string& username, const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << getCurrentTime() << " [CHAT] " << username << ": " << message << "\n";
    }

    // Server-to-server communication methods
    void connectToServer(const std::string& server_info) {
        // Parse host:port
        size_t colon_pos = server_info.find(':');
        if (colon_pos == std::string::npos) {
            logError("Invalid server format. Use: host:port");
            return;
        }

        std::string host = server_info.substr(0, colon_pos);
        int port = std::atoi(server_info.substr(colon_pos + 1).c_str());

        if (port <= 0 || port > 65535) {
            logError("Invalid port number");
            return;
        }

        // Initialize server manager if not already done
        if (!server_manager) {
            server_manager = std::make_unique<ServerManager>(config_manager);
        }

        if (server_manager->connectToServer(host, port)) {
            logInfo("Successfully connected to server " + host + ":" + std::to_string(port));
        } else {
            logError("Failed to connect to server " + host + ":" + std::to_string(port));
        }
    }

    void listServers() {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "\n=== Connected Servers ===\n";

        if (!server_manager) {
            std::cout << "No server manager initialized\n\n";
            return;
        }

        auto servers = server_manager->getConnectedServers();
        if (servers.empty()) {
            std::cout << "No servers connected\n\n";
            return;
        }

        for (const auto& server : servers) {
            std::cout << "- " << server.host << ":" << server.port
                     << " (Connected " << std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now() - server.last_seen).count() << ")\n";
        }
        std::cout << "\n";
    }

    void showNetworkStatus() {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "\n=== Network Status ===\n";

        if (!server_manager) {
            std::cout << "Server manager not initialized\n";
            std::cout << "Total connected servers: 0\n\n";
            return;
        }

        auto servers = server_manager->getConnectedServers();
        std::cout << "Total connected servers: " << servers.size() << "\n";

        if (!servers.empty()) {
            std::cout << "Server list:\n";
            for (const auto& server : servers) {
                std::cout << "  - " << server.host << ":" << server.port << "\n";
            }
        }
        std::cout << "\n";
    }

    void sendServerMessage(const std::string& message) {
        if (!server_manager) {
            logError("Server manager not initialized");
            return;
        }

        ServerMessage msg(ServerMessageType::MSG_FORWARD_PUBLIC, config_manager.getConfig().server_id, message); if (server_manager->broadcastMessage(msg)) {
            logInfo("Message sent to all connected servers: " + message);
        } else {
            logError("Failed to send message to servers");
        }
    }
};

int main(int argc, char* argv[]) {
    int port = 8080;
    int max_clients = 50;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-p" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::string(argv[i]) == "-m" && i + 1 < argc) {
            max_clients = std::atoi(argv[++i]);
        } else if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  -p <port>     Set server port (default: 8080)\n";
            std::cout << "  -m <max>      Set max clients (default: 50)\n";
            std::cout << "  -h, --help    Show this help\n";
            return 0;
        }
    }
    
    try {
        ChatServer server(port, max_clients);
        
        if (!server.start()) {
            std::cerr << "Failed to start server\n";
            return 1;
        }
        
        server.runConsole();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
         

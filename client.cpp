
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
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

class ChatClient {
private:
    SOCKET client_socket;
    std::atomic<bool> connected;
    std::atomic<bool> running;
    std::string server_host;
    int server_port;
    std::string username;
    
public:
    ChatClient(const std::string& host = "127.0.0.1", int port = 8080) 
        : server_host(host), server_port(port), connected(false), running(false) {
        #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        #endif
    }
    
    ~ChatClient() {
        disconnect();
        #ifdef _WIN32
        WSACleanup();
        #endif
    }
    
    bool connect() {
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Error: Failed to create socket\n";
            return false;
        }
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        
        // Convert hostname to IP if necessary
        if (inet_addr(server_host.c_str()) == INADDR_NONE) {
            struct hostent* he = gethostbyname(server_host.c_str());
            if (he == nullptr) {
                std::cerr << "Error: Could not resolve hostname " << server_host << "\n";
                close(client_socket);
                return false;
            }
            server_addr.sin_addr = *((struct in_addr*)he->h_addr);
        } else {
            server_addr.sin_addr.s_addr = inet_addr(server_host.c_str());
        }
        
        std::cout << "Connecting to " << server_host << ":" << server_port << "...\n";
        
        if (::connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Error: Failed to connect to server\n";
            close(client_socket);
            return false;
        }
        
        connected = true;
        running = true;
        std::cout << "Connected successfully!\n";
        
        return true;
    }
    
    void disconnect() {
        running = false;
        connected = false;
        if (client_socket != INVALID_SOCKET) {
            close(client_socket);
            client_socket = INVALID_SOCKET;
        }
    }
    
    void run() {
        if (!connected) {
            std::cerr << "Error: Not connected to server\n";
            return;
        }
        
        // Start message receiving thread
        std::thread receive_thread(&ChatClient::receiveMessages, this);
        
        // Handle user input in main thread
        handleUserInput();
        
        // Wait for receive thread to finish
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
    }
    
    void sendMessage(const std::string& message) {
        if (!connected || message.empty()) {
            return;
        }
        
        std::string msg = message + "\n";
        int result = send(client_socket, msg.c_str(), msg.length(), 0);
        if (result == SOCKET_ERROR) {
            std::cerr << "Error: Failed to send message\n";
            disconnect();
        }
    }

private:
    void receiveMessages() {
        char buffer[1024];
        
        while (running && connected) {
            int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                if (running) {
                    std::cout << "\nConnection to server lost.\n";
                }
                disconnect();
                break;
            }
            
            buffer[bytes] = '\0';
            
            // Print received message, handling multiple messages in one buffer
            std::string received(buffer);
            size_t pos = 0;
            std::string line;
            
            while ((pos = received.find('\n')) != std::string::npos) {
                line = received.substr(0, pos);
                if (!line.empty()) {
                    // Clear current input line and print message
                    std::cout << "\r" << std::string(80, ' ') << "\r";
                    std::cout << line << std::endl;
                    std::cout << "> " << std::flush;
                }
                received.erase(0, pos + 1);
            }
            
            // Handle remaining text without newline
            if (!received.empty()) {
                std::cout << "\r" << std::string(80, ' ') << "\r";
                std::cout << received << std::flush;
                std::cout << "\n> " << std::flush;
            }
        }
    }
    
    void handleUserInput() {
        std::string input;
        
        // Show prompt
        std::cout << "> " << std::flush;
        
        while (running && connected && std::getline(std::cin, input)) {
            if (input.empty()) {
                std::cout << "> " << std::flush;
                continue;
            }
            
            // Handle local commands
            if (input == "/quit" || input == "/exit") {
                std::cout << "Disconnecting...\n";
                sendMessage("/quit");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                disconnect();
                break;
            } else if (input == "/help") {
                showLocalHelp();
            } else if (input == "/clear") {
                clearScreen();
            } else if (input == "/ping") {
                auto start = std::chrono::high_resolution_clock::now();
                sendMessage("ping");
                // Simple ping implementation - in a real app you'd measure server response
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                std::cout << "Local response time: " << duration.count() << "ms\n";
            } else {
                // Send message to server
                sendMessage(input);
            }
            
            if (connected && running) {
                std::cout << "> " << std::flush;
            }
        }
    }
    
    void showLocalHelp() {
        std::cout << "\n=== Local Client Commands ===\n";
        std::cout << "/quit, /exit  - Disconnect from server\n";
        std::cout << "/help         - Show this help\n";
        std::cout << "/clear        - Clear screen\n";
        std::cout << "/ping         - Test connection\n";
        std::cout << "\nServer commands (sent to server):\n";
        std::cout << "/list         - Show online users\n";
        std::cout << "/pm <user> <message> - Private message\n";
        std::cout << "Just type normally to send public messages\n\n";
    }
    
    void clearScreen() {
        #ifdef _WIN32
        system("cls");
        #else
        system("clear");
        #endif
        
        std::cout << "=== Chat Client ===\n";
        std::cout << "Connected to " << server_host << ":" << server_port << "\n";
        std::cout << "Type /help for commands\n\n";
    }
};

void showUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -h <host>     Server hostname/IP (default: 127.0.0.1)\n";
    std::cout << "  -p <port>     Server port (default: 8080)\n";
    std::cout << "  --help        Show this help\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << "                    # Connect to localhost:8080\n";
    std::cout << "  " << program_name << " -h 192.168.1.100   # Connect to specific IP\n";
    std::cout << "  " << program_name << " -p 9999            # Connect to different port\n";
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 8080;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-h" && i + 1 < argc) {
            host = argv[++i];
        } else if (std::string(argv[i]) == "-p" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                std::cerr << "Error: Invalid port number. Must be 1-65535\n";
                return 1;
            }
        } else if (std::string(argv[i]) == "--help") {
            showUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Error: Unknown option " << argv[i] << "\n";
            showUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "=== Chat Client ===\n";
    std::cout << "Attempting to connect to " << host << ":" << port << "\n\n";
    
    try {
        ChatClient client(host, port);
        
        if (!client.connect()) {
            std::cerr << "Failed to connect to server. Make sure the server is running.\n";
            return 1;
        }
        
        std::cout << "\n=== Welcome to the Chat! ===\n";
        std::cout << "Type /help for available commands\n";
        std::cout << "Type /quit to disconnect\n\n";
        
        client.run();
        
    } catch (const std::exception& e) {
        std::cout<<"Error: " << e.what() << "\n";
        return 1;   
    }
        
}
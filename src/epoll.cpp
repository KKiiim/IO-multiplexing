#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <fstream>
#include <cassert>

#define NUM_FILES 10
#define BUFFER_SIZE 1024
#define SOCKET_PATH "/tmp/epoll_socket"
#define OUTPUT_FILE "/tmp/epollOutput.txt"

int clientSockets[NUM_FILES];
std::ofstream outputFile;

void* writeThread(void* arg) {
    int fileIndex = *((int*)arg);
    assert(fileIndex >=0 && fileIndex <10);
    std::string data = "This is epoll test file";

    int clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error creating client socket" << std::endl;
        pthread_exit(NULL);
    }
  
    struct sockaddr_un serverAddr;
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, SOCKET_PATH, sizeof(serverAddr.sun_path) - 1);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error connecting to server socket" << std::endl;
        close(clientSocket);
        pthread_exit(NULL);
    }
  
    // Send data to the epoll process
    ssize_t bytesSent = send(clientSocket, data.c_str(), data.length(), 0);
    if (bytesSent == -1) {
        std::cerr << "Error sending data to the epoll process" << std::endl;
    } else {
        std::cout << "Data sent to the epoll process: " << data << std::endl;
    }
  
    close(clientSocket);
    pthread_exit(NULL);
}

int main() {
    // Create server socket
    int serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error creating server socket" << std::endl;
        return 1;
    }

    struct sockaddr_un serverAddr;
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, SOCKET_PATH, sizeof(serverAddr.sun_path) - 1);

    // Bind the server socket to the socket path
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error binding server socket to the socket path" << std::endl;
        close(serverSocket);
        return 1;
    }

    // Start listening for incoming connections
    if (listen(serverSocket, NUM_FILES) == -1) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        close(serverSocket);
        return 1;
    }
  
    // Create threads
    pthread_t threads[NUM_FILES];
    for (int i = 0; i < NUM_FILES; i++) {
        int* fileIndex = new int;
        *fileIndex = i;
        if (pthread_create(&threads[i], NULL, writeThread, (void*)fileIndex) != 0) {
            std::cerr << "Error creating thread for file " << i << std::endl;
            return 1;
        }
    }
  
    // Create epoll instance
    int epollFd = epoll_create1(0);
    if (epollFd == -1) {
        std::cerr << "Error creating epoll instance" << std::endl;
        close(serverSocket);
        return 1;
    }
  
    // Add server socket to epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = serverSocket;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverSocket, &event) == -1) {
        std::cerr << "Error adding server socket to epoll" << std::endl;
        close(serverSocket);
        close(epollFd);
        return 1;
    }
  
    // Open the output file
    outputFile.open(OUTPUT_FILE);
    if (!outputFile.is_open()) {
        std::cerr << "Error opening output file" << std::endl;
        close(serverSocket);
        close(epollFd);
        return 1;
    }
  
    // Use epoll for IO multiplexing
    struct epoll_event events[NUM_FILES + 1];
    while (true) {
        int ready = epoll_wait(epollFd, events, NUM_FILES + 1, -1);
        if (ready == -1) {
            std::cerr << "Error in epoll_wait" << std::endl;
            break;
        }
      
        for (int i = 0; i < ready; i++) {
            if (events[i].data.fd == serverSocket) {
                //处理新的客户端连接
                int clientSocket = accept(serverSocket, NULL, NULL);
                if (clientSocket == -1) {
                    std::cerr << "Error accepting client connection" << std::endl;
                    continue;
                }
              
                // Add the client socket to epoll
                event.events = EPOLLIN;
                event.data.fd = clientSocket;
                if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSocket, &event) == -1) {
                    std::cerr << "Error adding client socket to epoll" << std::endl;
                    close(clientSocket);
                    continue;
                }
            } else {
                // Handle data received from client
                int clientSocket = events[i].data.fd;
                char buffer[BUFFER_SIZE];
                ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::cout << "Received data: " << buffer << std::endl;
                  
                    // Write data to the output file
                    outputFile << buffer << std::endl;
                    outputFile.flush();
                } else if (bytesRead == 0) {
                    // Client closed the connection
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientSocket, NULL);
                    close(clientSocket);
                } else {
                    std::cerr << "Error receiving data from client" << std::endl;
                }
            }
        }
    }
  
    // Close server socket and output file
    close(serverSocket);
    outputFile.close();
  
    // Cleanup client sockets
    for (int i = 0; i < NUM_FILES; i++) {
        if (clientSockets[i] != 0) {
            close(clientSockets[i]);
        }
    }
  
    return 0;
}
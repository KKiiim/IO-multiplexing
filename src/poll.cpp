#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define NUM_FILES 10
#define BUFFER_SIZE 1024
#define SOCKET_PATH "/tmp/poll_socket"
#define OUTPUT_FILE "/tmp/pollOutput.txt"

int clientSockets[NUM_FILES];
std::ofstream outputFile;

void *writeThread(void *arg) {
  int fileIndex = *((int *)arg);
  assert(fileIndex >= 0 && fileIndex < 10);
  std::string data = "This is poll test file";

  int clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (clientSocket == -1) {
    std::cerr << "Error creating client socket" << std::endl;
    pthread_exit(NULL);
  }

  struct sockaddr_un serverAddr;
  serverAddr.sun_family = AF_UNIX;
  strncpy(serverAddr.sun_path, SOCKET_PATH, sizeof(serverAddr.sun_path) - 1);

  if (connect(clientSocket, (struct sockaddr *)&serverAddr,
              sizeof(serverAddr)) == -1) {
    std::cerr << "Error connecting to server socket" << std::endl;
    close(clientSocket);
    pthread_exit(NULL);
  }

  // Send data to the select process
  ssize_t bytesSent = send(clientSocket, data.c_str(), data.length(), 0);
  if (bytesSent == -1) {
    std::cerr << "Error sending data to the select process" << std::endl;
  } else {
    std::cout << "Data sent to the select process: " << data << std::endl;
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
  if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      -1) {
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
    int *fileIndex = new int;
    *fileIndex = i;
    if (pthread_create(&threads[i], NULL, writeThread, (void *)fileIndex) !=
        0) {
      std::cerr << "Error creating thread for file " << i << std::endl;
      return 1;
    }
  }

  // Set up the pollfd array
  struct pollfd fds[NUM_FILES + 1];
  fds[0].fd = serverSocket;
  fds[0].events = POLLIN;
  for (int i = 0; i < NUM_FILES; i++) {
    fds[i + 1].fd = clientSockets[i];
    fds[i + 1].events = POLLIN;
  }

  // Open the output file
  outputFile.open(OUTPUT_FILE);
  if (!outputFile.is_open()) {
    std::cerr << "Error opening output file" << std::endl;
    close(serverSocket);
    return 1;
  }

  // Use poll for IO multiplexing
  while (true) {
    int ready = poll(fds, NUM_FILES + 1, -1);
    if (ready == -1) {
      std::cerr << "Error in poll" << std::endl;
      break;
    }

    if (fds[0].revents & POLLIN) {
      int clientSocket = accept(serverSocket, NULL, NULL);
      if (clientSocket == -1) {
        std::cerr << "Error accepting client connection" << std::endl;
        continue;
      }

      // Add the client socket to the array
      for (int i = 0; i < NUM_FILES; i++) {
        if (clientSockets[i] == 0) {
          clientSockets[i] = clientSocket;
          fds[i + 1].fd = clientSocket;
          fds[i + 1].events = POLLIN;
          break;
        }
      }
    }

    // Check for data on client sockets
    for (int i = 0; i < NUM_FILES; i++) {
      if (fds[i + 1].revents & POLLIN) {
        int clientSocket = clientSockets[i];
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
          close(clientSocket);
          clientSockets[i] = 0;
          fds[i + 1].fd = -1;
          fds[i + 1].events = 0;
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
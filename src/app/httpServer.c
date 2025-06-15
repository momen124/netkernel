#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CONN 10
#define BUFFER_SIZE 4096
#define MAX_PATH 256

// Function to parse headers and extract Cookie and DNT
void parse_headers(char* request, char* cookie, char* dnt) {
    cookie[0] = '\0';
    dnt[0] = '\0';
    char* line = strtok(request, "\r\n");
    while (line) {
        if (strstr(line, "Cookie:")) {
            sscanf(line, "Cookie: %[^\r\n]", cookie);
        } else if (strstr(line, "DNT:")) {
            sscanf(line, "DNT: %[^\r\n]", dnt);
        }
        line = strtok(NULL, "\r\n");
    }
}

// Function to serve static file
int serve_static_file(int client_fd, const char* path) {
    FILE* file = fopen(path + 1, "r"); // Skip leading '/'
    if (!file) {
        char response[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
        write(client_fd, response, strlen(response));
        return -1;
    }

    char response_header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    write(client_fd, response_header, strlen(response_header));

    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        write(client_fd, buffer, bytes);
    }
    fclose(file);
    return 0;
}

// Thread function to handle client requests
void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char buffer[BUFFER_SIZE] = {0};
    read(client_fd, buffer, BUFFER_SIZE);

    // Parse request line
    char method[16], path[MAX_PATH], version[16];
    sscanf(buffer, "%s %s %s", method, path, version);

    // Parse headers
    char cookie[256] = {0}, dnt[256] = {0};
    parse_headers(buffer, cookie, dnt);

    // Log headers (for GDPR simulation)
    printf("Request: %s %s\nCookie: %s\nDNT: %s\n", method, path, cookie, dnt);

    // Handle GET request
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            serve_static_file(client_fd, "/index.html");
        } else {
            serve_static_file(client_fd, path);
        }
    } else {
        char response[] = "HTTP/1.1 501 Not Implemented\r\nContent-Type: text/plain\r\n\r\nMethod not supported";
        write(client_fd, response, strlen(response));
    }

    close(client_fd);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    // Listen
    if (listen(server_fd, MAX_CONN) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int* client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (*client_fd < 0) {
            perror("Accept failed");
            free(client_fd);
            continue;
        }

        // Create thread to handle client
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client_fd) != 0) {
            perror("Thread creation failed");
            close(*client_fd);
            free(client_fd);
        }
        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}
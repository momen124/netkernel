/* This is an HTTP/1.1 server that listens on port 8080, handles GET requests,
 * parses Cookie and DNT headers (for GDPR simulation), and serves static files.
 * It uses threads to handle multiple clients concurrently, like a library with
 * multiple librarians serving visitors. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Define constants for server configuration */
#define PORT 8080         /* Port number the server listens on (like a phone number) */
#define MAX_CONN 10       /* Maximum queued connections (like people waiting in line) */
#define BUFFER_SIZE 4096  /* Size of buffer for reading requests (4KB) */
#define MAX_PATH 256      /* Maximum length of file paths (e.g., /index.html) */

/* Function to parse HTTP headers and extract Cookie and DNT (Do Not Track) values.
 * Takes the request string and stores Cookie/DNT in provided buffers.
 * Like reading a letter to find specific notes (e.g., "Cookie: session=abc123"). */
void parse_headers(char* request, char* cookie, char* dnt) {
    /* Initialize cookie and dnt as empty strings (null-terminated) */
    cookie[0] = '\0';
    dnt[0] = '\0';
    
    /* Split request into lines using \r\n (HTTP line separator) */
    char* line = strtok(request, "\r\n");
    
    /* Loop through each line until none remain */
    while (line) {
        /* Check if line contains "Cookie:" */
        if (strstr(line, "Cookie:")) {
            /* Extract everything after "Cookie: " into cookie buffer */
            sscanf(line, "Cookie: %[^\r\n]", cookie);
        }
        /* Check if line contains "DNT:" */
        else if (strstr(line, "DNT:")) {
            /* Extract everything after "DNT: " into dnt buffer */
            sscanf(line, "DNT: %[^\r\n]", dnt);
        }
        /* Get the next line */
        line = strtok(NULL, "\r\n");
    }
}

/* Function to serve a static file (e.g., index.html) to the client.
 * Takes the client socket (client_fd) and file path (e.g., /index.html).
 * Returns 0 on success, -1 if file not found.
 * Like a librarian handing over a book or saying "Book not found." */
int serve_static_file(int client_fd, const char* path) {
    /* Open the file in read mode, skipping the leading '/' (e.g., /index.html -> index.html) */
    FILE* file = fopen(path + 1, "r");
    
    /* If file doesn't exist, send a 404 response */
    if (!file) {
        /* Define 404 error response (HTTP status and message) */
        char response[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
        /* Send response to client */
        write(client_fd, response, strlen(response));
        /* Return failure */
        return -1;
    }

    /* Define success response header (200 OK, content type HTML) */
    char response_header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    /* Send header to client */
    write(client_fd, response_header, strlen(response_header));

    /* Buffer to read file contents */
    char buffer[BUFFER_SIZE];
    size_t bytes;
    /* Read file in chunks and send to client */
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        write(client_fd, buffer, bytes);
    }
    
    /* Close the file */
    fclose(file);
    /* Return success */
    return 0;
}

/* Thread function to handle a single client request.
 * Takes a pointer to the client socket (arg) and processes the HTTP request.
 * Like a librarian serving one visitor, reading their request, and responding. */
void* handle_client(void* arg) {
    /* Extract client socket from argument and free the allocated memory */
    int client_fd = *(int*)arg;
    free(arg);

    /* Buffer to store the client's HTTP request */
    char buffer[BUFFER_SIZE] = {0};
    /* Read request from client (e.g., "GET / HTTP/1.1...") */
    read(client_fd, buffer, BUFFER_SIZE);

    /* Parse the first line of the request (e.g., "GET /index.html HTTP/1.1") */
    char method[16], path[MAX_PATH], version[16];
    sscanf(buffer, "%s %s %s", method, path, version);

    /* Buffers to store Cookie and DNT headers */
    char cookie[256] = {0}, dnt[256] = {0};
    /* Parse headers from request */
    parse_headers(buffer, cookie, dnt);

    /* Log request details to console (for debugging and GDPR simulation) */
    printf("Request: %s %s\nCookie: %s\nDNT: %s\n", method, path, cookie, dnt);

    /* Handle GET requests */
    if (strcmp(method, "GET") == 0) {
        /* If path is "/" or "/index.html", serve index.html */
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            serve_static_file(client_fd, "/index.html");
        }
        /* Otherwise, try to serve the requested file (e.g., /test.html) */
        else {
            serve_static_file(client_fd, path);
        }
    }
    /* For non-GET methods (e.g., POST, PUT), send 501 error */
    else {
        char response[] = "HTTP/1.1 501 Not Implemented\r\nContent-Type: text/plain\r\n\r\nMethod not supported";
        write(client_fd, response, strlen(response));
    }

    /* Close the client connection */
    close(client_fd);
    /* End the thread */
    return NULL;
}

/* Main function: Sets up the server, listens for connections, and spawns threads.
 * Like the head librarian opening the library and assigning librarians to visitors. */
int main() {
    /* Server socket file descriptor */
    int server_fd;
    /* Server address structure */
    struct sockaddr_in server_addr;

    /* Create a TCP socket (AF_INET for IPv4, SOCK_STREAM for TCP) */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    /* Check for socket creation failure */
    if (server_fd < 0) {
        perror("Socket creation failed"); /* Print error */
        exit(1); /* Exit program */
    }

    /* Configure server address */
    server_addr.sin_family = AF_INET;            /* Use IPv4 */
    server_addr.sin_addr.s_addr = INADDR_ANY;    /* Listen on all interfaces (e.g., localhost) */
    server_addr.sin_port = htons(PORT);          /* Set port to 8080 (htons converts to network byte order) */

    /* Bind socket to address/port */
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    /* Listen for incoming connections, with a queue of up to MAX_CONN */
    if (listen(server_fd, MAX_CONN) < 0) {
        perror("Listen failed");
        exit(1);
    }

    /* Confirm server is running */
    printf("Server listening on port %d...\n", PORT);

    /* Infinite loop to accept client connections */
    while (1) {
        /* Client address structure and size */
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        /* Allocate memory for client socket (passed to thread) */
        int* client_fd = malloc(sizeof(int));
        /* Accept a client connection */
        *client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        /* Check for accept failure */
        if (*client_fd < 0) {
            perror("Accept failed");
            free(client_fd); /* Free memory */
            continue; /* Skip to next iteration */
        }

        /* Create a thread to handle the client */
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client_fd) != 0) {
            perror("Thread creation failed");
            close(*client_fd); /* Close client socket */
            free(client_fd);   /* Free memory */
        }
        /* Detach thread to clean up automatically when done */
        pthread_detach(thread);
    }

    /* Close server socket (unreachable due to infinite loop) */
    close(server_fd);
    return 0;
}
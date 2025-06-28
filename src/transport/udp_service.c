/* udp_service.c: A simple UDP service for handling client and server datagrams.
 * Server mode echoes received datagrams; client mode sends/receives datagrams.
 * Like a librarian at a dropbox for quick notes. Complements netkernel tools for
 * UDP-based protocols like DNS. */

/* Include standard libraries for sockets, I/O, and networking */
#include <stdio.h>      /* For printf, perror, fprintf */
#include <stdlib.h>     /* For exit, malloc, free */
#include <string.h>     /* For memcpy, memset */
#include <unistd.h>     /* For close */
#include <sys/socket.h> /* For socket, bind, sendto, recvfrom */
#include <netinet/in.h> /* For sockaddr_in */
#include <arpa/inet.h>  /* For inet_pton, inet_ntop */

/* Buffer size for datagrams */
#define BUFFER_SIZE 1024
/* Default port */
#define DEFAULT_PORT 7000

/* Server function */
void run_server(int port) {
    /* Create UDP socket */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Set socket options */
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Set up server address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    /* Bind socket */
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(1);
    }

    printf("UDP server listening on port %d\n", port);

    /* Handle datagrams */
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    while (1) {
        /* Receive datagram */
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr*)&client_addr, &client_len);
        if (bytes < 0) {
            perror("Receive failed");
            continue;
        }

        buffer[bytes] = '\0';
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Received from %s:%d: %s\n", client_ip, ntohs(client_addr.sin_port), buffer);

        /* Echo back */
        if (sendto(sockfd, buffer, bytes, 0,
                   (struct sockaddr*)&client_addr, client_len) < 0) {
            perror("Send failed");
        } else {
            printf("Sent to %s:%d: %s\n", client_ip, ntohs(client_addr.sin_port), buffer);
        }
    }

    close(sockfd);
}

/* Client function */
void run_client(const char *server_ip, int port) {
    /* Create UDP socket */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Set up server address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP");
        close(sockfd);
        exit(1);
    }

    /* Send datagram */
    char *message = "Hello, UDP world!\n";
    if (sendto(sockfd, message, strlen(message), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Send failed");
        close(sockfd);
        exit(1);
    }

    printf("Sent to %s:%d: %s\n", server_ip, port, message);

    /* Receive response */
    char buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                             (struct sockaddr*)&from_addr, &from_len);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        char from_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, from_ip, sizeof(from_ip));
        printf("Received from %s:%d: %s\n", from_ip, ntohs(from_addr.sin_port), buffer);
    } else if (bytes < 0) {
        perror("Receive failed");
    }

    close(sockfd);
}

/* Main function: Entry point of the UDP service */
int main(int argc, char *argv[]) {
    /* Check command-line arguments */
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s server [port]\n", argv[0]);
        fprintf(stderr, "       %s client <server_ip> [port]\n", argv[0]);
        fprintf(stderr, "Example: %s server 7000\n", argv[0]);
        fprintf(stderr, "         %s client 127.0.0.1 7000\n", argv[0]);
        exit(1);
    }

    /* Pointers to arguments */
    char *mode = argv[1];
    int port = (argc == 3 && strcmp(mode, "server") == 0) ? atoi(argv[2]) :
               (argc == 4 && strcmp(mode, "client") == 0) ? atoi(argv[3]) : DEFAULT_PORT;
    char *server_ip = (strcmp(mode, "client") == 0) ? argv[2] : NULL;

    /* Run in server or client mode */
    if (strcmp(mode, "server") == 0) {
        run_server(port);
    } else if (strcmp(mode, "client") == 0) {
        if (!server_ip) {
            fprintf(stderr, "Client mode requires server IP\n");
            exit(1);
        }
        run_client(server_ip, port);
    } else {
        fprintf(stderr, "Invalid mode: use 'server' or 'client'\n");
        exit(1);
    }

    return 0;
}
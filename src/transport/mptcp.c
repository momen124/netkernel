/* mptcp.c: A simple MPTCP client-server program that demonstrates Multipath TCP
 * by transferring data over multiple network paths. Server echoes client messages;
 * client sends and receives messages. Like a librarian sending books via multiple
 * delivery trucks. Requires Linux kernel with MPTCP support. */

/* Include standard libraries for sockets, I/O, and networking */
#include <stdio.h>      /* For printf, perror, fprintf */
#include <stdlib.h>     /* For exit, malloc, free */
#include <string.h>     /* For memcpy, memset */
#include <unistd.h>     /* For close, read, write */
#include <sys/socket.h> /* For socket, bind, listen, accept, connect */
#include <netinet/in.h> /* For sockaddr_in */
#include <arpa/inet.h>  /* For inet_pton, inet_ntop */
#include <netinet/tcp.h> /* For TCP_MULTIPATH_ENABLE */

/* Buffer size for messages */
#define BUFFER_SIZE 1024
/* Default port */
#define DEFAULT_PORT 5000

/* Enable MPTCP on a socket */
int enable_mptcp(int sockfd) {
    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_MULTIPATH_ENABLE, &enable, sizeof(enable)) < 0) {
        perror("Failed to enable MPTCP");
        return -1;
    }
    return 0;
}

/* Server function */
void run_server(int port) {
    /* Create socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Enable MPTCP */
    if (enable_mptcp(sockfd) < 0) {
        close(sockfd);
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

    /* Listen for connections */
    if (listen(sockfd, 5) < 0) {
        perror("Listen failed");
        close(sockfd);
        exit(1);
    }

    printf("MPTCP server listening on port %d\n", port);

    /* Accept and handle clients */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Connected to client %s\n", client_ip);

        /* Handle client */
        char buffer[BUFFER_SIZE];
        ssize_t bytes;
        while ((bytes = read(client_sock, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = '\0';
            printf("Received: %s\n", buffer);
            write(client_sock, buffer, bytes); /* Echo back */
        }

        if (bytes < 0) {
            perror("Read failed");
        }

        close(client_sock);
        printf("Disconnected from client %s\n", client_ip);
    }

    close(sockfd);
}

/* Client function */
void run_client(const char *server_ip, int port) {
    /* Create socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Enable MPTCP */
    if (enable_mptcp(sockfd) < 0) {
        close(sockfd);
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

    /* Connect to server */
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sockfd);
        exit(1);
    }

    printf("Connected to server %s:%d\n", server_ip, port);

    /* Send message */
    char *message = "Hello, MPTCP world!\n";
    if (write(sockfd, message, strlen(message)) < 0) {
        perror("Send failed");
        close(sockfd);
        exit(1);
    }

    /* Receive echo */
    char buffer[BUFFER_SIZE];
    ssize_t bytes = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Received: %s\n", buffer);
    } else if (bytes < 0) {
        perror("Receive failed");
    }

    close(sockfd);
}

/* Main function: Entry point of the MPTCP program */
int main(int argc, char *argv[]) {
    /* Check command-line arguments */
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s server [port]\n", argv[0]);
        fprintf(stderr, "       %s client <server_ip> [port]\n", argv[0]);
        fprintf(stderr, "Example: %s server 5000\n", argv[0]);
        fprintf(stderr, "         %s client 127.0.0.1 5000\n", argv[0]);
        exit(1);
    }

    /* Pointers to arguments */
    char *mode = argv[1];
    int port = (argc == 3 && strcmp(mode, "server") == 0) ? atoi(argv[2]) :
               (argc == 4 && strcmp(mode, "client") == 0) ? atoi(argv[3]) : DEFAULT_PORT;
    char *server_ip = (strcmp(mode, "client") == 0) ? argv[2] : NULL;

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
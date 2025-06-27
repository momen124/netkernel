/* tcp_engine.c: A modular TCP engine for handling client and server connections
 * with customizable callbacks. Supports server (echoes data) and client (sends/
 * receives data) modes. Like a librarian managing a central desk for letters.
 * Integrates with netkernel tools for TCP-based protocols. */

/* Include standard libraries for sockets, I/O, and networking */
#include <stdio.h>      /* For printf, perror, fprintf */
#include <stdlib.h>     /* For exit, malloc, free */
#include <string.h>     /* For memcpy, memset */
#include <unistd.h>     /* For close, read, write */
#include <sys/socket.h> /* For socket, bind, listen, accept, connect */
#include <netinet/in.h> /* For sockaddr_in */
#include <arpa/inet.h>  /* For inet_pton, inet_ntop */

/* Buffer size for messages */
#define BUFFER_SIZE 1024
/* Default port */
#define DEFAULT_PORT 6000

/* TCP engine structure with callback functions */
struct tcp_engine {
    void (*on_accept)(int client_sock, struct sockaddr_in *client_addr); /* Called on new connection */
    void (*on_receive)(int client_sock, char *data, ssize_t len);        /* Called on data received */
    void (*on_send)(int client_sock, char *data, ssize_t len);           /* Called before sending data */
};

/* Default callback: Log client connection */
void default_on_accept(int client_sock, struct sockaddr_in *client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("Connected to client %s:%d\n", client_ip, ntohs(client_addr->sin_port));
}

/* Default callback: Log and echo received data */
void default_on_receive(int client_sock, char *data, ssize_t len) {
    data[len] = '\0';
    printf("Received: %s\n", data);
    write(client_sock, data, len); /* Echo back */
}

/* Default callback: Log sent data */
void default_on_send(int client_sock, char *data, ssize_t len) {
    data[len] = '\0';
    printf("Sending: %s\n", data);
}

/* Initialize TCP engine with default callbacks */
void tcp_engine_init(struct tcp_engine *engine) {
    engine->on_accept = default_on_accept;
    engine->on_receive = default_on_receive;
    engine->on_send = default_on_send;
}

/* Server function */
void run_server(int port, struct tcp_engine *engine) {
    /* Create socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

    /* Listen for connections */
    if (listen(sockfd, 5) < 0) {
        perror("Listen failed");
        close(sockfd);
        exit(1);
    }

    printf("TCP engine server listening on port %d\n", port);

    /* Accept and handle clients */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        /* Call accept callback */
        engine->on_accept(client_sock, &client_addr);

        /* Handle client */
        char buffer[BUFFER_SIZE];
        ssize_t bytes;
        while ((bytes = read(client_sock, buffer, sizeof(buffer) - 1)) > 0) {
            /* Call receive callback */
            engine->on_receive(client_sock, buffer, bytes);
        }

        if (bytes < 0) {
            perror("Read failed");
        }

        close(client_sock);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Disconnected from client %s\n", client_ip);
    }

    close(sockfd);
}

/* Client function */
void run_client(const char *server_ip, int port, struct tcp_engine *engine) {
    /* Create socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

    /* Connect to server */
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sockfd);
        exit(1);
    }

    printf("Connected to server %s:%d\n", server_ip, port);

    /* Send message */
    char *message = "Hello, TCP engine!\n";
    engine->on_send(sockfd, message, strlen(message));
    if (write(sockfd, message, strlen(message)) < 0) {
        perror("Send failed");
        close(sockfd);
        exit(1);
    }

    /* Receive response */
    char buffer[BUFFER_SIZE];
    ssize_t bytes = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        engine->on_receive(sockfd, buffer, bytes);
    } else if (bytes < 0) {
        perror("Receive failed");
    }

    close(sockfd);
}

/* Main function: Entry point of the TCP engine */
int main(int argc, char *argv[]) {
    /* Check command-line arguments */
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s server [port]\n", argv[0]);
        fprintf(stderr, "       %s client <server_ip> [port]\n", argv[0]);
        fprintf(stderr, "Example: %s server 6000\n", argv[0]);
        fprintf(stderr, "         %s client 127.0.0.1 6000\n", argv[0]);
        exit(1);
    }

    /* Pointers to arguments */
    char *mode = argv[1];
    int port = (argc == 3 && strcmp(mode, "server") == 0) ? atoi(argv[2]) :
               (argc == 4 && strcmp(mode, "client") == 0) ? atoi(argv[3]) : DEFAULT_PORT;
    char *server_ip = (strcmp(mode, "client") == 0) ? argv[2] : NULL;

    /* Initialize TCP engine */
    struct tcp_engine engine;
    tcp_engine_init(&engine);

    /* Run in server or client mode */
    if (strcmp(mode, "server") == 0) {
        run_server(port, &engine);
    } else if (strcmp(mode, "client") == 0) {
        if (!server_ip) {
            fprintf(stderr, "Client mode requires server IP\n");
            exit(1);
        }
        run_client(server_ip, port, &engine);
    } else {
        fprintf(stderr, "Invalid mode: use 'server' or 'client'\n");
        exit(1);
    }

    return 0;
}
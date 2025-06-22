/* tls_openssl.c: A simple TLS server using OpenSSL that listens for client
 * connections, performs TLS handshakes, and exchanges encrypted messages.
 * Like a librarian at a secure desk exchanging secret notes. Requires OpenSSL
 * and a server certificate/key. */

/* Include standard libraries for sockets, I/O, and OpenSSL */
#include <stdio.h>      /* For printf, perror, fprintf */
#include <stdlib.h>     /* For exit, malloc, free */
#include <string.h>     /* For memcpy, memset */
#include <unistd.h>     /* For close */
#include <sys/socket.h> /* For socket, bind, listen, accept */
#include <netinet/in.h> /* For sockaddr_in */
#include <arpa/inet.h>  /* For inet_addr */
#include <openssl/ssl.h> /* For SSL/TLS functions */
#include <openssl/err.h> /* For error handling */

/* Buffer size for messages */
#define BUFFER_SIZE 1024
/* Default port */
#define DEFAULT_PORT 8443

/* Initialize OpenSSL */
void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

/* Create SSL context */
SSL_CTX *create_context() {
    const SSL_METHOD *method = TLS_server_method(); /* Modern TLS versions */
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    return ctx;
}

/* Configure SSL context with certificate and key */
void configure_context(SSL_CTX *ctx, const char *cert_file, const char *key_file) {
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the certificate\n");
        exit(1);
    }
}

/* Main function: Entry point of the TLS server */
int main(int argc, char *argv[]) {
    /* Check command-line arguments */
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <cert_file> <key_file>\n", argv[0]);
        fprintf(stderr, "Example: %s 8443 server.crt server.key\n", argv[0]);
        exit(1);
    }

    /* Pointers to arguments */
    int port = atoi(argv[1]);
    char *cert_file = argv[2];
    char *key_file = argv[3];

    /* Initialize OpenSSL */
    init_openssl();
    SSL_CTX *ctx = create_context();
    configure_context(ctx, cert_file, key_file);

    /* Create TCP socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        SSL_CTX_free(ctx);
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
        SSL_CTX_free(ctx);
        exit(1);
    }

    /* Listen for connections */
    if (listen(sockfd, 5) < 0) {
        perror("Listen failed");
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(1);
    }

    printf("TLS server listening on port %d\n", port);

    /* Accept and handle clients */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        /* Create SSL structure */
        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            fprintf(stderr, "SSL creation failed\n");
            close(client_sock);
            continue;
        }

        /* Assign socket to SSL */
        SSL_set_fd(ssl, client_sock);

        /* Perform TLS handshake */
        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client_sock);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("TLS connection established with %s\n", client_ip);

        /* Handle client communication */
        char buffer[BUFFER_SIZE];
        int bytes;
        while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = '\0';
            printf("Received: %s\n", buffer);

            /* Send response */
            char *response = "Hello, secure world!\n";
            SSL_write(ssl, response, strlen(response));
        }

        if (bytes < 0) {
            ERR_print_errors_fp(stderr);
        }

        /* Cleanup client connection */
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_sock);
        printf("TLS connection closed with %s\n", client_ip);
    }

    /* Cleanup */
    close(sockfd);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
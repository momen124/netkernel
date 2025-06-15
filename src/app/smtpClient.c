/* smtp_client.c: A simple SMTP client that sends an email to an SMTP server.
 * It connects to the server, sends SMTP commands (HELO, MAIL FROM, RCPT TO, DATA),
 * and delivers an email with a subject and body. Like a postal worker delivering
 * a letter to a post office by following protocol steps. */

/* Include standard libraries for sockets, strings, and I/O */
#include <stdio.h>      /* For printf, perror, fprintf (printing) */
#include <stdlib.h>     /* For exit, malloc, free (memory and control) */
#include <string.h>     /* For strlen, strcpy, strcat (string operations) */
#include <unistd.h>     /* For write, read, close (socket operations) */
#include <sys/socket.h> /* For socket, connect (socket functions) */
#include <netinet/in.h> /* For sockaddr_in (socket address) */
#include <arpa/inet.h>  /* For inet_pton, htons (network conversions) */
#include <netdb.h>      /* For gethostbyname (DNS lookup) */

/* Buffer size for reading server responses and sending commands */
#define BUFFER_SIZE 1024

/* Function to send an SMTP command and read the server’s response */
int send_command(int sockfd, const char *command, char *response, size_t response_size) {
    /* Send the command to the server (add \r\n for SMTP protocol) */
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "%s\r\n", command);
    if (write(sockfd, cmd, strlen(cmd)) < 0) {
        perror("Failed to send command");
        return -1;
    }
    printf("Sent: %s", cmd);

    /* Read the server’s response */
    ssize_t bytes = read(sockfd, response, response_size - 1);
    if (bytes <= 0) {
        perror("Failed to read response");
        return -1;
    }
    response[bytes] = '\0'; /* Null-terminate the response */
    printf("Received: %s", response);

    /* Check if the response starts with a success code (e.g., 250) */
    if (strncmp(response, "250", 3) != 0 && strncmp(response, "220", 3) != 0 && 
        strncmp(response, "354", 3) != 0) {
        fprintf(stderr, "Unexpected response: %s", response);
        return -1;
    }
    return 0;
}

/* Main function: Entry point of the SMTP client */
int main(int argc, char *argv[]) {
    /* Check for correct number of command-line arguments */
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <server> <port> <from> <to> <subject> <body>\n", argv[0]);
        fprintf(stderr, "Example: %s smtp.example.com 587 sender@example.com recipient@example.com \"Test Email\" \"Hello, this is a test.\"\n");
        exit(1);
    }

    /* Pointers to command-line arguments */
    char *server = argv[1];  /* SMTP server (e.g., smtp.example.com) */
    int port = atoi(argv[2]); /* Port (e.g., 587) */
    char *from = argv[3];    /* Sender email */
    char *to = argv[4];      /* Recipient email */
    char *subject = argv[5]; /* Email subject */
    char *body = argv[6];    /* Email body */

    /* Create a TCP socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Resolve the server’s hostname to an IP address */
    struct hostent *host = gethostbyname(server);
    if (!host) {
        fprintf(stderr, "Failed to resolve %s\n", server);
        close(sockfd);
        exit(1);
    }

    /* Set up server address structure */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    /* Connect to the SMTP server */
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(1);
    }
    printf("Connected to %s:%d\n", server, port);

    /* Buffer for server responses */
    char response[BUFFER_SIZE];

    /* Read initial server greeting (220) */
    if (read(sockfd, response, sizeof(response) - 1) <= 0) {
        perror("Failed to read greeting");
        close(sockfd);
        exit(1);
    }
    response[strlen(response)] = '\0';
    printf("Received: %s", response);

    /* Send SMTP commands */
    /* HELO: Introduce ourselves to the server */
    if (send_command(sockfd, "HELO localhost", response, sizeof(response)) < 0) {
        close(sockfd);
        exit(1);
    }

    /* MAIL FROM: Specify sender */
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "MAIL FROM:<%s>", from);
    if (send_command(sockfd, cmd, response, sizeof(response)) < 0) {
        close(sockfd);
        exit(1);
    }

    /* RCPT TO: Specify recipient */
    snprintf(cmd, sizeof(cmd), "RCPT TO:<%s>", to);
    if (send_command(sockfd, cmd, response, sizeof(response)) < 0) {
        close(sockfd);
        exit(1);
    }

    /* DATA: Start email content */
    if (send_command(sockfd, "DATA", response, sizeof(response)) < 0) {
        close(sockfd);
        exit(1);
    }

    /* Send email headers and body */
    snprintf(cmd, sizeof(cmd), 
             "From: %s\r\nTo: %s\r\nSubject: %s\r\n\r\n%s\r\n.\r\n", 
             from, to, subject, body);
    if (write(sockfd, cmd, strlen(cmd)) < 0) {
        perror("Failed to send email data");
        close(sockfd);
        exit(1);
    }
    printf("Sent: %s", cmd);
    if (read(sockfd, response, sizeof(response) - 1) <= 0) {
        perror("Failed to read DATA response");
        close(sockfd);
        exit(1);
    }
    response[strlen(response)] = '\0';
    printf("Received: %s", response);
    if (strncmp(response, "250", 3) != 0) {
        fprintf(stderr, "Failed to send email: %s", response);
        close(sockfd);
        exit(1);
    }

    /* QUIT: End session */
    if (send_command(sockfd, "QUIT", response, sizeof(response)) < 0) {
        close(sockfd);
        exit(1);
    }

    /* Close the socket */
    close(sockfd);
    printf("Email sent successfully!\n");

    return 0;
}
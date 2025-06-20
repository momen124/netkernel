/* bgp_sim.c: A simple BGP simulator that establishes a BGP session with a peer over TCP,
 * sends an OPEN message to negotiate parameters, receives the peer’s OPEN, and sends a
 * KEEPALIVE to maintain the session. Like a librarian negotiating a book-sharing agreement
 * with another library, exchanging contracts and check-ins. Uses TCP port 179 by default. */

/* Include standard libraries for sockets, networking, and I/O */
#include <stdio.h>      /* For printf, perror, fprintf (printing) */
#include <stdlib.h>     /* For exit, malloc, free (memory and control) */
#include <string.h>     /* For memcpy, memset (memory operations) */
#include <unistd.h>     /* For close, read, write (socket operations) */
#include <sys/socket.h> /* For socket, connect, bind, listen, accept */
#include <netinet/in.h> /* For sockaddr_in, in_addr (IP addresses) */
#include <arpa/inet.h>  /* For inet_pton, htons (network conversions) */

/* BGP message types (per RFC 4271) */
#define BGP_OPEN 1
#define BGP_UPDATE 2
#define BGP_NOTIFICATION 3
#define BGP_KEEPALIVE 4

/* Buffer size for BGP messages */
#define BUFFER_SIZE 4096

/* BGP message header (19 bytes) */
struct bgp_header {
    uint8_t marker[16]; /* All 1s (0xFF) for synchronization */
    uint16_t length;    /* Message length (including header) */
    uint8_t type;       /* Message type (OPEN, UPDATE, etc.) */
};

/* BGP OPEN message (variable length, minimum 29 bytes) */
struct bgp_open {
    struct bgp_header header; /* Common header */
    uint8_t version;         /* BGP version (4) */
    uint16_t my_as;          /* My Autonomous System number */
    uint16_t hold_time;      /* Hold time in seconds (e.g., 180) */
    uint32_t bgp_id;         /* BGP Identifier (IP address as integer) */
    uint8_t opt_param_len;   /* Length of optional parameters */
    /* Optional parameters follow (none for simplicity) */
};

/* Function to initialize BGP header */
void init_bgp_header(struct bgp_header *hdr, uint16_t length, uint8_t type) {
    memset(hdr->marker, 0xFF, 16); /* Set marker to all 1s */
    hdr->length = htons(length);    /* Network byte order */
    hdr->type = type;
}

/* Function to send a BGP message */
int send_bgp_message(int sockfd, void *msg, size_t len) {
    if (write(sockfd, msg, len) != (ssize_t)len) {
        perror("Failed to send BGP message");
        return -1;
    }
    printf("Sent BGP message (type %u, length %zu)\n", ((struct bgp_header*)msg)->type, len);
    return 0;
}

/* Function to receive and validate a BGP message */
int receive_bgp_message(int sockfd, unsigned char *buffer, size_t buf_size) {
    ssize_t len = read(sockfd, buffer, buf_size);
    if (len <= 0) {
        perror("Failed to receive BGP message");
        return -1;
    }
    struct bgp_header *hdr = (struct bgp_header*)buffer;
    if (len < sizeof(struct bgp_header) || ntohs(hdr->length) != len) {
        fprintf(stderr, "Invalid BGP message length\n");
        return -1;
    }
    printf("Received BGP message (type %u, length %zd)\n", hdr->type, len);
    return len;
}

/* Main function: Entry point of the BGP simulator */
int main(int argc, char *argv[]) {
    /* Check for correct number of command-line arguments */
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <local_as> <peer_ip> <peer_as> <peer_port>\n", argv[0]);
        fprintf(stderr, "Example: %s 65001 127.0.0.1 65002 179\n", argv[0]);
        exit(1);
    }

    /* Pointers to command-line arguments */
    char *local_as_str = argv[1]; /* Local AS number (e.g., 65001) */
    char *peer_ip = argv[2];     /* Peer IP address (e.g., 127.0.0.1) */
    char *peer_as_str = argv[3]; /* Peer AS number (e.g., 65002) */
    char *peer_port_str = argv[4]; /* Peer port (e.g., 179) */

    /* Parse arguments */
    uint16_t local_as = atoi(local_as_str);
    uint16_t peer_as = atoi(peer_as_str);
    int peer_port = atoi(peer_port_str);

    /* Create TCP socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Set up peer address */
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    if (inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr) <= 0) {
        perror("Invalid peer IP address");
        close(sockfd);
        exit(1);
    }

    /* Connect to peer */
    if (connect(sockfd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("Connection to peer failed");
        close(sockfd);
        exit(1);
    }
    printf("Connected to peer %s:%d\n", peer_ip, peer_port);

    /* Prepare BGP OPEN message */
    struct bgp_open open_msg;
    memset(&open_msg, 0, sizeof(open_msg));
    init_bgp_header(&open_msg.header, sizeof(struct bgp_open), BGP_OPEN);
    open_msg.version = 4;                    /* BGP-4 */
    open_msg.my_as = htons(local_as);        /* Local AS */
    open_msg.hold_time = htons(180);         /* 3 minutes */
    open_msg.bgp_id = htonl(0xC0A80001);     /* 192.168.0.1 (hardcoded for simplicity) */
    open_msg.opt_param_len = 0;              /* No optional parameters */

    /* Send OPEN message */
    if (send_bgp_message(sockfd, &open_msg, sizeof(open_msg)) < 0) {
        close(sockfd);
        exit(1);
    }

    /* Receive peer’s OPEN message */
    unsigned char buffer[BUFFER_SIZE];
    int len = receive_bgp_message(sockfd, buffer, sizeof(buffer));
    if (len < 0 || ((struct bgp_header*)buffer)->type != BGP_OPEN) {
        fprintf(stderr, "Failed to receive valid OPEN message\n");
        close(sockfd);
        exit(1);
    }
    struct bgp_open *peer_open = (struct bgp_open*)buffer;
    printf("Received OPEN: AS %u, Hold Time %u, BGP ID %x\n",
           ntohs(peer_open->my_as), ntohs(peer_open->hold_time), ntohl(peer_open->bgp_id));

    /* Send KEEPALIVE message */
    struct bgp_header keepalive;
    init_bgp_header(&keepalive, sizeof(struct bgp_header), BGP_KEEPALIVE);
    if (send_bgp_message(sockfd, &keepalive, sizeof(keepalive)) < 0) {
        close(sockfd);
        exit(1);
    }

    /* Close socket */
    close(sockfd);
    printf("BGP session simulation completed\n");
    return 0;
}
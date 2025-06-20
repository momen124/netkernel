/* icmp_diag.c: A simple ICMP diagnostic tool that sends Echo Requests (like ping) to a
 * target IP and receives Echo Replies, calculating round-trip time (RTT). It uses raw
 * sockets to craft ICMP packets. Like a librarian sending test letters and timing replies.
 * Requires root privileges (sudo). */

/* Include standard libraries for sockets, networking, time, and I/O */
#include <stdio.h>      /* For printf, perror, fprintf (printing) */
#include <stdlib.h>     /* For exit, malloc, free (memory and control) */
#include <string.h>     /* For memcpy, memset (memory operations) */
#include <unistd.h>     /* For close, usleep (socket and timing) */
#include <sys/socket.h> /* For socket, sendto, recvfrom */
#include <sys/time.h>   /* For gettimeofday (RTT calculation) */
#include <netinet/in.h> /* For sockaddr_in, in_addr (IP addresses) */
#include <arpa/inet.h>  /* For inet_pton, htons (network conversions) */
#include <netinet/ip.h> /* For iphdr (IP header) */
#include <netinet/ip_icmp.h> /* For icmphdr (ICMP header) */

/* Buffer size for ICMP packets */
#define BUFFER_SIZE 1024
/* Default number of pings */
#define DEFAULT_COUNT 4
/* ICMP payload size (includes timestamp) */
#define PAYLOAD_SIZE 56

/* Compute checksum for ICMP packet */
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

/* Main function: Entry point of the ICMP diagnostic tool */
int main(int argc, char *argv[]) {
    /* Check command-line arguments */
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <target_ip> [count]\n", argv[0]);
        fprintf(stderr, "Example: %s 8.8.8.8 4\n", argv[0]);
        exit(1);
    }

    /* Pointers to arguments */
    char *target_ip = argv[1]; /* Target IP (e.g., 8.8.8.8) */
    int count = (argc == 3) ? atoi(argv[2]) : DEFAULT_COUNT; /* Number of pings */

    /* Create raw socket for ICMP */
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Set up target address */
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, target_ip, &target_addr.sin_addr) <= 0) {
        perror("Invalid target IP");
        close(sockfd);
        exit(1);
    }

    /* Packet buffer */
    unsigned char packet[BUFFER_SIZE];
    struct icmphdr *icmp_hdr = (struct icmphdr *)packet;
    unsigned char *payload = packet + sizeof(struct icmphdr);

    /* Statistics */
    int sent = 0, received = 0;
    double min_rtt = 1e6, max_rtt = 0, sum_rtt = 0;

    printf("PING %s (%s): %d data bytes\n", target_ip, target_ip, PAYLOAD_SIZE);

    /* Send <count> ICMP Echo Requests */
    for (int seq = 1; seq <= count; seq++) {
        /* Clear packet */
        memset(packet, 0, sizeof(packet));

        /* Set ICMP header */
        icmp_hdr->type = ICMP_ECHO;     /* Echo Request */
        icmp_hdr->code = 0;
        icmp_hdr->un.echo.id = htons(getpid()); /* Unique ID */
        icmp_hdr->un.echo.sequence = htons(seq);
        icmp_hdr->checksum = 0; /* Set after payload */

        /* Set payload (timestamp + filler) */
        struct timeval *tv = (struct timeval *)payload;
        gettimeofday(tv, NULL);
        for (int i = sizeof(struct timeval); i < PAYLOAD_SIZE; i++) {
            payload[i] = 'A'; /* Fill with 'A' */
        }

        /* Compute checksum */
        icmp_hdr->checksum = checksum(packet, sizeof(struct icmphdr) + PAYLOAD_SIZE);

        /* Send packet */
        if (sendto(sockfd, packet, sizeof(struct icmphdr) + PAYLOAD_SIZE, 0,
                   (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
            perror("Send failed");
            continue;
        }
        sent++;

        /* Receive reply */
        unsigned char reply[BUFFER_SIZE];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        struct timeval start, end;
        gettimeofday(&start, NULL);

        ssize_t len = recvfrom(sockfd, reply, sizeof(reply), 0,
                               (struct sockaddr*)&from_addr, &from_len);
        gettimeofday(&end, NULL);

        if (len < 0) {
            perror("Receive failed");
            continue;
        }

        /* Skip IP header (assume 20 bytes, no options) */
        struct icmphdr *reply_icmp = (struct icmphdr *)(reply + 20);
        if (len < 20 + sizeof(struct icmphdr) || reply_icmp->type != ICMP_ECHOREPLY ||
            ntohs(reply_icmp->un.echo.id) != getpid() ||
            ntohs(reply_icmp->un.echo.sequence) != seq) {
            continue; /* Not our reply */
        }

        /* Calculate RTT */
        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                     (end.tv_usec - start.tv_usec) / 1000.0;
        min_rtt = (rtt < min_rtt) ? rtt : min_rtt;
        max_rtt = (rtt > max_rtt) ? rtt : max_rtt;
        sum_rtt += rtt;
        char from_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, from_ip, sizeof(from_ip));
        printf("%ld bytes from %s: icmp_seq=%d ttl=64 time=%.2f ms\n",
               len - 20, from_ip, seq, rtt);
        received++;

        /* Wait 1 second between pings */
        usleep(1000000);
    }

    /* Close socket */
    close(sockfd);

    /* Print statistics */
    if (sent > 0) {
        double avg_rtt = sum_rtt / received;
        double loss = 100.0 * (sent - received) / sent;
        printf("\n--- %s ping statistics ---\n", target_ip);
        printf("%d packets sent, %d packets received, %.1f%% packet loss\n",
               sent, received, loss);
        if (received > 0) {
            printf("round-trip min/avg/max = %.2f/%.2f/%.2f ms\n",
                   min_rtt, avg_rtt, max_rtt);
        }
    }

    return 0;
}
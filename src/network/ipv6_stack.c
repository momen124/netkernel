/* ipv6_stack.c: A simple IPv6 stack implementation that sends ICMPv6 Echo Requests
 * (like ping6) to a target IPv6 address and receives Echo Replies, calculating
 * round-trip time (RTT). It uses raw sockets for ICMPv6 packets. Like a librarian
 * sending test letters with a new address format (IPv6). Requires root privileges
 * (sudo) and an IPv6-enabled interface. */

/* Include standard libraries for sockets, networking, time, and I/O */
#include <stdio.h>      /* For printf, perror, fprintf (printing) */
#include <stdlib.h>     /* For exit, malloc, free (memory and control) */
#include <string.h>     /* For memcpy, memset (memory operations) */
#include <unistd.h>     /* For close, usleep (socket and timing) */
#include <sys/socket.h> /* For socket, sendto, recvfrom */
#include <sys/time.h>   /* For gettimeofday (RTT calculation) */
#include <netinet/in.h> /* For sockaddr_in6, in6_addr (IPv6 addresses) */
#include <arpa/inet.h>  /* For inet_pton, htons (network conversions) */
#include <netinet/icmp6.h> /* For icmp6_hdr (ICMPv6 header) */
#include <net/if.h>     /* For ifreq, if_nametoindex (interface index) */

/* Buffer size for IPv6 packets */
#define BUFFER_SIZE 1024
/* Default number of pings */
#define DEFAULT_COUNT 4
/* ICMPv6 payload size (includes timestamp) */
#define PAYLOAD_SIZE 56

/* Compute checksum for ICMPv6 packet (includes pseudo-header) */
unsigned short checksum(void *b, int len, struct in6_addr *src, struct in6_addr *dst) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    /* Pseudo-header for ICMPv6 checksum */
    unsigned char pseudo[40];
    memset(pseudo, 0, sizeof(pseudo));
    memcpy(pseudo, src, 16);           /* Source IPv6 address */
    memcpy(pseudo + 16, dst, 16);      /* Destination IPv6 address */
    pseudo[32] = 0; pseudo[33] = 0; pseudo[34] = 0; pseudo[35] = len >> 8;
    pseudo[36] = len & 0xFF;           /* Length */
    pseudo[39] = IPPROTO_ICMPV6;       /* Next header (ICMPv6) */

    /* Sum pseudo-header */
    for (int i = 0; i < 40; i += 2) {
        sum += (pseudo[i] << 8) + pseudo[i + 1];
    }

    /* Sum packet */
    for (; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }

    /* Fold 32-bit sum to 16 bits */
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

/* Main function: Entry point of the IPv6 stack */
int main(int argc, char *argv[]) {
    /* Check command-line arguments */
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <interface> <target_ipv6> [count]\n", argv[0]);
        fprintf(stderr, "Example: %s eth0 2001:4860:4860::8888 4\n", argv[0]);
        exit(1);
    }

    /* Pointers to arguments */
    char *if_name = argv[1];     /* Interface (e.g., eth0) */
    char *target_ip = argv[2];   /* Target IPv6 (e.g., 2001:4860:4860::8888) */
    int count = (argc == 4) ? atoi(argv[3]) : DEFAULT_COUNT; /* Number of pings */

    /* Get interface index */
    int if_index = if_nametoindex(if_name);
    if (if_index == 0) {
        perror("Invalid interface");
        exit(1);
    }

    /* Create raw socket for ICMPv6 */
    int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Bind socket to interface */
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, if_name, strlen(if_name) + 1) < 0) {
        perror("Bind to interface failed");
        close(sockfd);
        exit(1);
    }

    /* Set up target address */
    struct sockaddr_in6 target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin6_family = AF_INET6;
    target_addr.sin6_scope_id = if_index;
    if (inet_pton(AF_INET6, target_ip, &target_addr.sin6_addr) <= 0) {
        perror("Invalid target IPv6");
        close(sockfd);
        exit(1);
    }

    /* Get source IPv6 address (hardcoded for simplicity; ideally use getifaddrs) */
    struct in6_addr src_addr;
    inet_pton(AF_INET6, "::1", &src_addr); /* Use loopback or configure dynamically */

    /* Packet buffer */
    unsigned char packet[BUFFER_SIZE];
    struct icmp6_hdr *icmp_hdr = (struct icmp6_hdr *)packet;
    unsigned char *payload = packet + sizeof(struct icmp6_hdr);

    /* Statistics */
    int sent = 0, received = 0;
    double min_rtt = 1e6, max_rtt = 0, sum_rtt = 0;

    printf("PING6 %s (%s): %d data bytes\n", target_ip, target_ip, PAYLOAD_SIZE);

    /* Send <count> ICMPv6 Echo Requests */
    for (int seq = 1; seq <= count; seq++) {
        /* Clear packet */
        memset(packet, 0, sizeof(packet));

        /* Set ICMPv6 header */
        icmp_hdr->icmp6_type = ICMP6_ECHO_REQUEST; /* Echo Request */
        icmp_hdr->icmp6_code = 0;
        icmp_hdr->icmp6_id = htons(getpid());      /* Unique ID */
        icmp_hdr->icmp6_seq = htons(seq);
        icmp_hdr->icmp6_cksum = 0;                 /* Set after payload */

        /* Set payload (timestamp + filler) */
        struct timeval *tv = (struct timeval *)payload;
        gettimeofday(tv, NULL);
        for (int i = sizeof(struct timeval); i < PAYLOAD_SIZE; i++) {
            payload[i] = 'A'; /* Fill with 'A' */
        }

        /* Compute checksum */
        icmp_hdr->icmp6_cksum = checksum(packet, sizeof(struct icmp6_hdr) + PAYLOAD_SIZE,
                                         &src_addr, &target_addr.sin6_addr);

        /* Send packet */
        if (sendto(sockfd, packet, sizeof(struct icmp6_hdr) + PAYLOAD_SIZE, 0,
                   (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
            perror("Send failed");
            continue;
        }
        sent++;

        /* Receive reply */
        unsigned char reply[BUFFER_SIZE];
        struct sockaddr_in6 from_addr;
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

        /* Validate ICMPv6 reply */
        struct icmp6_hdr *reply_icmp = (struct icmp6_hdr *)reply;
        if (len < sizeof(struct icmp6_hdr) || reply_icmp->icmp6_type != ICMP6_ECHO_REPLY ||
            ntohs(reply_icmp->icmp6_id) != getpid() ||
            ntohs(reply_icmp->icmp6_seq) != seq) {
            continue; /* Not our reply */
        }

        /* Calculate RTT */
        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                     (end.tv_usec - start.tv_usec) / 1000.0;
        min_rtt = (rtt < min_rtt) ? rtt : min_rtt;
        max_rtt = (rtt > max_rtt) ? rtt : max_rtt;
        sum_rtt += rtt;
        char from_ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &from_addr.sin6_addr, from_ip, sizeof(from_ip));
        printf("%ld bytes from %s: icmp_seq=%d time=%.2f ms\n",
               len, from_ip, seq, rtt);
        received++;

        /* Wait 1 second between pings */
        usleep(1000000);
    }

    /* Close socket */
    close(sockfd);

    /* Print statistics */
    if (sent > 0) {
        double avg_rtt = received > 0 ? sum_rtt / received : 0;
        double loss = 100.0 * (sent - received) / sent;
        printf("\n--- %s ping6 statistics ---\n", target_ip);
        printf("%d packets sent, %d packets received, %.1f%% packet loss\n",
               sent, received, loss);
        if (received > 0) {
            printf("round-trip min/avg/max = %.2f/%.2f/%.2f ms\n",
                   min_rtt, avg_rtt, max_rtt);
        }
    }

    return 0;
}
/* firewall.c: A simple firewall that captures IPv4 packets on a specified interface,
 * applies filtering rules (e.g., allow TCP port 80, deny ICMP), and logs actions.
 * Uses raw sockets to inspect Ethernet, IP, and TCP/UDP headers. Like a librarian
 * checking letters for approved senders or labels. Requires root privileges (sudo). */

/* Include standard libraries for sockets, networking, and I/O */
#include <stdio.h>      /* For printf, perror, fprintf */
#include <stdlib.h>     /* For exit, malloc, free */
#include <string.h>     /* For memcpy, memset */
#include <unistd.h>     /* For close */
#include <sys/socket.h> /* For socket, recvfrom */
#include <netinet/in.h> /* For sockaddr_in, in_addr */
#include <arpa/inet.h>  /* For inet_ntop */
#include <netinet/ip.h> /* For iphdr */
#include <netinet/tcp.h> /* For tcphdr */
#include <netinet/udp.h> /* For udphdr */
#include <net/ethernet.h> /* For ether_header */
#include <linux/if_packet.h> /* For sockaddr_ll */
#include <net/if.h>     /* For if_nametoindex */

/* Buffer size for packets */
#define BUFFER_SIZE 65536
/* Maximum rules */
#define MAX_RULES 10

/* Rule structure */
struct firewall_rule {
    uint8_t protocol; /* IPPROTO_TCP, IPPROTO_UDP, IPPROTO_ICMP */
    uint16_t port;    /* Destination port (0 for any) */
    uint32_t src_ip;  /* Source IP (0 for any) */
    int allow;        /* 1 = allow, 0 = deny */
};

/* Parse and apply rules */
int apply_rules(unsigned char *packet, ssize_t len, struct firewall_rule *rules, int num_rules) {
    struct ether_header *eth_hdr = (struct ether_header *)packet;
    if (ntohs(eth_hdr->ether_type) != ETH_P_IP) {
        return 1; /* Allow non-IP packets (e.g., ARP) */
    }

    struct iphdr *ip_hdr = (struct iphdr *)(packet + sizeof(struct ether_header));
    if (len < sizeof(struct ether_header) + sizeof(struct iphdr)) {
        return 0; /* Deny malformed packets */
    }

    uint16_t port = 0;
    if (ip_hdr->protocol == IPPROTO_TCP) {
        struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + sizeof(struct ether_header) + (ip_hdr->ihl * 4));
        if (len >= sizeof(struct ether_header) + (ip_hdr->ihl * 4) + sizeof(struct tcphdr)) {
            port = ntohs(tcp_hdr->dest);
        }
    } else if (ip_hdr->protocol == IPPROTO_UDP) {
        struct udphdr *udp_hdr = (struct udphdr *)(packet + sizeof(struct ether_header) + (ip_hdr->ihl * 4));
        if (len >= sizeof(struct ether_header) + (ip_hdr->ihl * 4) + sizeof(struct udphdr)) {
            port = ntohs(udp_hdr->dest);
        }
    }

    char src_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_hdr->saddr, src_ip_str, sizeof(src_ip_str));

    for (int i = 0; i < num_rules; i++) {
        if ((rules[i].protocol == ip_hdr->protocol || rules[i].protocol == 0) &&
            (rules[i].port == port || rules[i].port == 0) &&
            (rules[i].src_ip == ip_hdr->saddr || rules[i].src_ip == 0)) {
            printf("%s %s packet (proto %u, port %u, src %s)\n",
                   rules[i].allow ? "Allowed" : "Denied",
                   ip_hdr->protocol == IPPROTO_TCP ? "TCP" :
                   ip_hdr->protocol == IPPROTO_UDP ? "UDP" : "ICMP",
                   port, src_ip_str);
            return rules[i].allow;
        }
    }

    printf("Allowed packet (no matching rule, proto %u, port %u, src %s)\n",
           ip_hdr->protocol, port, src_ip_str);
    return 1; /* Allow by default if no rule matches */
}

/* Main function: Entry point of the firewall */
int main(int argc, char *argv[]) {
    /* Check command-line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        fprintf(stderr, "Example: %s eth0\n", argv[0]);
        exit(1);
    }

    /* Pointer to interface name */
    char *if_name = argv[1];

    /* Get interface index */
    int if_index = if_nametoindex(if_name);
    if (if_index == 0) {
        perror("Invalid interface");
        exit(1);
    }

    /* Create raw socket */
    int sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Bind socket to interface */
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = if_index;
    sa.sll_protocol = htons(ETH_P_ALL);
    if (bind(sockfd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(1);
    }

    /* Define firewall rules (hardcoded for simplicity) */
    struct firewall_rule rules[MAX_RULES] = {
        { IPPROTO_TCP, 80, 0, 1 },   /* Allow TCP port 80 (HTTP) */
        { IPPROTO_ICMP, 0, 0, 0 },   /* Deny all ICMP */
        { IPPROTO_UDP, 53, 0, 1 },   /* Allow UDP port 53 (DNS) */
    };
    int num_rules = 3;

    printf("Firewall started on %s\n", if_name);

    /* Packet buffer */
    unsigned char packet[BUFFER_SIZE];

    /* Capture and filter packets */
    while (1) {
        ssize_t len = recvfrom(sockfd, packet, sizeof(packet), 0, NULL, NULL);
        if (len < 0) {
            perror("Receive failed");
            continue;
        }

        /* Apply rules */
        apply_rules(packet, len, rules, num_rules);
    }

    /* Close socket (unreachable in this loop) */
    close(sockfd);
    return 0;
}
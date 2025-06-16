/* arp_sim.c: A simple ARP simulator that sends an ARP request to resolve an IP address
 * to a MAC address on a local network. It uses raw sockets to craft and send Ethernet
 * frames containing ARP packets and captures replies. Like a librarian shouting, "Who has
 * this IP?" and listening for a MAC address reply. Requires root privileges (sudo). */

/* Include standard libraries for sockets, network interfaces, and I/O */
#include <stdio.h>         /* For printf, perror, fprintf (printing) */
#include <stdlib.h>        /* For exit, malloc, free (memory and control) */
#include <string.h>        /* For memcpy, memset (memory operations) */
#include <unistd.h>        /* For close (socket operations) */
#include <sys/socket.h>    /* For socket (raw socket creation) */
#include <sys/ioctl.h>     /* For ioctl (interface configuration) */
#include <net/if.h>        /* For ifreq (interface details) */
#include <netinet/in.h>    /* For sockaddr_in, in_addr (IP addresses) */
#include <net/ethernet.h>  /* For ether_header (Ethernet frame) */
#include <arpa/inet.h>     /* For inet_pton, htons (network conversions) */
#include <linux/if_packet.h> /* For sockaddr_ll (raw socket addressing) */
#include <netinet/if_ether.h> /* For arphdr, ether_arp (ARP packet structures) */

/* Buffer size for Ethernet frames */
#define BUFFER_SIZE 1514 /* Standard MTU for Ethernet */

/* Function to get the MAC address and interface index of a network interface */
int get_interface_info(const char *if_name, unsigned char *mac, int *if_index) {
    /* Create a socket for ioctl operations */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed for ioctl");
        return -1;
    }

    /* Structure to query interface details */
    struct ifreq ifr;
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    /* Get interface index */
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        perror("Failed to get interface index");
        close(sockfd);
        return -1;
    }
    *if_index = ifr.ifr_ifindex;

    /* Get MAC address */
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("Failed to get MAC address");
        close(sockfd);
        return -1;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6); /* Copy 6 bytes of MAC address */

    close(sockfd);
    return 0;
}

/* Main function: Entry point of the ARP simulator */
int main(int argc, char *argv[]) {
    /* Check for correct number of command-line arguments */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <interface> <target_ip>\n", argv[0]);
        fprintf(stderr, "Example: %s eth0 192.168.1.1\n", argv[0]);
        exit(1);
    }

    /* Pointers to command-line arguments */
    char *if_name = argv[1]; /* Network interface (e.g., eth0) */
    char *target_ip = argv[2]; /* Target IP address (e.g., 192.168.1.1) */

    /* Variables for interface info */
    unsigned char src_mac[6]; /* Source MAC address */
    int if_index; /* Interface index */
    if (get_interface_info(if_name, src_mac, &if_index) < 0) {
        exit(1);
    }
    printf("Interface %s: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           if_name, src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);

    /* Create a raw socket for Ethernet frames */
    int sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (sockfd < 0) {
        perror("Raw socket creation failed");
        exit(1);
    }

    /* Buffer for Ethernet frame */
    unsigned char frame[BUFFER_SIZE];
    memset(frame, 0, sizeof(frame));

    /* Pointer to Ethernet header */
    struct ether_header *eth_hdr = (struct ether_header *)frame;
    /* Set destination MAC to broadcast (FF:FF:FF:FF:FF:FF) */
    memset(eth_hdr->ether_dhost, 0xFF, 6);
    /* Set source MAC */
    memcpy(eth_hdr->ether_shost, src_mac, 6);
    /* Set frame type to ARP */
    eth_hdr->ether_type = htons(ETH_P_ARP);

    /* Pointer to ARP packet (after Ethernet header) */
    struct ether_arp *arp_pkt = (struct ether_arp *)(frame + sizeof(struct ether_header));
    /* Set ARP header fields */
    arp_pkt->arp_hrd = htons(ARPHRD_ETHER); /* Hardware type: Ethernet */
    arp_pkt->arp_pro = htons(ETH_P_IP);     /* Protocol type: IP */
    arp_pkt->arp_hln = 6;                   /* Hardware address length (MAC) */
    arp_pkt->arp_pln = 4;                   /* Protocol address length (IP) */
    arp_pkt->arp_op = htons(ARPOP_REQUEST); /* Operation: ARP request */

    /* Set sender MAC and IP */
    memcpy(arp_pkt->arp_sha, src_mac, 6);
    struct in_addr src_ip;
    /* Assume source IP is 192.168.1.100 (hardcoded for simplicity; use ioctl for dynamic IP) */
    if (inet_pton(AF_INET, "192.168.1.100", &src_ip) <= 0) {
        perror("Invalid source IP");
        close(sockfd);
        exit(1);
    }
    memcpy(arp_pkt->arp_spa, &src_ip, 4);

    /* Set target MAC to zero (unknown) and target IP */
    memset(arp_pkt->arp_tha, 0, 6);
    struct in_addr tgt_ip;
    if (inet_pton(AF_INET, target_ip, &tgt_ip) <= 0) {
        perror("Invalid target IP");
        close(sockfd);
        exit(1);
    }
    memcpy(arp_pkt->arp_tpa, &tgt_ip, 4);

    /* Set up socket address for sending */
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = if_index;
    sa.sll_halen = 6;
    memcpy(sa.sll_addr, eth_hdr->ether_dhost, 6);

    /* Send ARP request */
    if (sendto(sockfd, frame, sizeof(struct ether_header) + sizeof(struct ether_arp), 0,
               (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("Failed to send ARP request");
        close(sockfd);
        exit(1);
    }
    printf("Sent ARP request for %s\n", target_ip);

    /* Receive ARP reply */
    while (1) {
        ssize_t len = recv(sockfd, frame, sizeof(frame), 0);
        if (len < 0) {
            perror("Failed to receive ARP reply");
            close(sockfd);
            exit(1);
        }

        /* Check if it’s an ARP reply */
        if (ntohs(eth_hdr->ether_type) == ETH_P_ARP &&
            ntohs(arp_pkt->arp_op) == ARPOP_REPLY &&
            memcmp(arp_pkt->arp_spa, &tgt_ip, 4) == 0) {
            printf("ARP reply: %s is at %02x:%02x:%02x:%02x:%02x:%02x\n",
                   target_ip,
                   arp_pkt->arp_sha[0], arp_pkt->arp_sha[1], arp_pkt->arp_sha[2],
                   arp_pkt->arp_sha[3], arp_pkt->arp_sha[4], arp_pkt->arp_sha[5]);
            break;
        }
    }

    /* Close the socket */
    close(sockfd);
    return 0;
}
/* ethernet.c: A simple Ethernet frame manipulation program that sends a custom Ethernet
 * frame with a specified destination MAC address and payload, and captures incoming frames
 * on a given interface. It uses raw sockets to craft and send frames and display received
 * frames’ details (MAC addresses, payload). Like a librarian sending and reading custom
 * postcards on the network. Requires root privileges (sudo). */

/* Include standard libraries for sockets, network interfaces, and I/O */
#include <stdio.h>         /* For printf, perror, fprintf (printing) */
#include <stdlib.h>        /* For exit, malloc, free (memory and control) */
#include <string.h>        /* For memcpy, memset, sscanf (memory and string operations) */
#include <unistd.h>        /* For close (socket operations) */
#include <sys/socket.h>    /* For socket (raw socket creation) */
#include <sys/ioctl.h>     /* For ioctl (interface configuration) */
#include <net/if.h>        /* For ifreq (interface details) */
#include <netinet/in.h>    /* For sockaddr_in (IP addresses, if needed) */
#include <net/ethernet.h>  /* For ether_header (Ethernet frame) */
#include <linux/if_packet.h> /* For sockaddr_ll (raw socket addressing) */

/* Buffer size for Ethernet frames (standard MTU + some padding) */
#define BUFFER_SIZE 1514
/* Custom EtherType for our frames (0x1234, not used by standard protocols) */
#define CUSTOM_ETHERTYPE 0x1234

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

/* Function to parse a MAC address string (e.g., "00:1A:2B:3C:4D:5E") into bytes */
int parse_mac(const char *mac_str, unsigned char *mac) {
    int values[6];
    if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        fprintf(stderr, "Invalid MAC address format: %s\n", mac_str);
        return -1;
    }
    for (int i = 0; i < 6; i++) {
        mac[i] = (unsigned char)values[i];
    }
    return 0;
}

/* Main function: Entry point of the Ethernet program */
int main(int argc, char *argv[]) {
    /* Check for correct number of command-line arguments */
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <interface> <dest_mac> <payload>\n", argv[0]);
        fprintf(stderr, "Example: %s eth0 00:1A:2B:3C:4D:5E \"Hello\"\n", argv[0]);
        exit(1);
    }

    /* Pointers to command-line arguments */
    char *if_name = argv[1];    /* Network interface (e.g., eth0) */
    char *dest_mac_str = argv[2]; /* Destination MAC (e.g., 00:1A:2B:3C:4D:5E) */
    char *payload = argv[3];    /* Custom payload (e.g., Hello) */

    /* Variables for interface info and MAC addresses */
    unsigned char src_mac[6];   /* Source MAC address */
    unsigned char dest_mac[6];  /* Destination MAC address */
    int if_index;               /* Interface index */
    if (get_interface_info(if_name, src_mac, &if_index) < 0) {
        exit(1);
    }
    if (parse_mac(dest_mac_str, dest_mac) < 0) {
        exit(1);
    }
    printf("Interface %s: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           if_name, src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
    printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5]);

    /* Create a raw socket for Ethernet frames */
    int sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("Raw socket creation failed");
        exit(1);
    }

    /* Buffer for Ethernet frame */
    unsigned char frame[BUFFER_SIZE];
    memset(frame, 0, sizeof(frame));

    /* Pointer to Ethernet header */
    struct ether_header *eth_hdr = (struct ether_header *)frame;
    /* Set destination and source MAC addresses */
    memcpy(eth_hdr->ether_dhost, dest_mac, 6);
    memcpy(eth_hdr->ether_shost, src_mac, 6);
    /* Set custom EtherType */
    eth_hdr->ether_type = htons(CUSTOM_ETHERTYPE);

    /* Pointer to payload (after Ethernet header) */
    unsigned char *frame_payload = frame + sizeof(struct ether_header);
    /* Copy payload into frame */
    size_t payload_len = strlen(payload);
    if (payload_len > BUFFER_SIZE - sizeof(struct ether_header)) {
        fprintf(stderr, "Payload too large\n");
        close(sockfd);
        exit(1);
    }
    memcpy(frame_payload, payload, payload_len);

    /* Set up socket address for sending */
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = if_index;
    sa.sll_halen = 6;
    memcpy(sa.sll_addr, dest_mac, 6);

    /* Send Ethernet frame */
    size_t frame_len = sizeof(struct ether_header) + payload_len;
    if (sendto(sockfd, frame, frame_len, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("Failed to send Ethernet frame");
        close(sockfd);
        exit(1);
    }
    printf("Sent Ethernet frame with payload: %s\n", payload);

    /* Receive Ethernet frames */
    printf("Listening for incoming frames with EtherType 0x%04X...\n", CUSTOM_ETHERTYPE);
    while (1) {
        ssize_t len = recv(sockfd, frame, sizeof(frame), 0);
        if (len < 0) {
            perror("Failed to receive frame");
            close(sockfd);
            exit(1);
        }

        /* Check if it’s our custom EtherType */
        if (ntohs(eth_hdr->ether_type) == CUSTOM_ETHERTYPE) {
            printf("Received frame:\n");
            printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                   eth_hdr->ether_shost[0], eth_hdr->ether_shost[1], eth_hdr->ether_shost[2],
                   eth_hdr->ether_shost[3], eth_hdr->ether_shost[4], eth_hdr->ether_shost[5]);
            printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                   eth_hdr->ether_dhost[0], eth_hdr->ether_dhost[1], eth_hdr->ether_dhost[2],
                   eth_hdr->ether_dhost[3], eth_hdr->ether_dhost[4], eth_hdr->ether_dhost[5]);
            /* Ensure payload is null-terminated for printing */
            frame_payload[len - sizeof(struct ether_header)] = '\0';
            printf("Payload: %s\n", frame_payload);
            break; /* Exit after one matching frame (for simplicity) */
        }
    }

    /* Close the socket */
    close(sockfd);
    return 0;
}
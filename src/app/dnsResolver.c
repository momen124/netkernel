/* dns_resolver.c: A simple DNS resolver that translates domain names (e.g., google.com)
 * into IP addresses using getaddrinfo(). It takes a domain name as a command-line argument
 * and prints all resolved IPv4 and IPv6 addresses. Like a librarian looking up a book’s
 * location (IP) from its title (domain). */

/* Include standard libraries for DNS resolution, string handling, and I/O */
#include <stdio.h>      /* For printf, perror (printing messages and errors) */
#include <stdlib.h>     /* For exit, free (program control and memory management) */
#include <string.h>     /* For strlen, strcmp (string operations) */
#include <netdb.h>      /* For getaddrinfo, freeaddrinfo, gai_strerror (DNS functions) */
#include <sys/socket.h> /* For socket-related types (AF_INET, AF_INET6) */
#include <arpa/inet.h>  /* For inet_ntop (converting binary IPs to strings) */

/* Main function: Entry point of the DNS resolver */
int main(int argc, char *argv[]) {
    /* Check if a domain name was provided as a command-line argument */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <domain_name>\n", argv[0]);
        exit(1);
    }

    /* Pointer to the domain name (e.g., "google.com") from argv[1] */
    char *domain = argv[1];
    printf("Resolving domain: %s\n", domain);

    /* Structure to specify hints for getaddrinfo (e.g., IPv4/IPv6, TCP/UDP) */
    struct addrinfo hints;
    /* Pointer to the linked list of resolved addresses */
    struct addrinfo *results;
    /* Initialize hints to zero for safety */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow both IPv4 (AF_INET) and IPv6 (AF_INET6) */
    hints.ai_socktype = SOCK_STREAM; /* Use TCP (could be SOCK_DGRAM for UDP) */

    /* Perform DNS lookup: getaddrinfo returns a linked list of addresses */
    int status = getaddrinfo(domain, NULL, &hints, &results);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(status));
        exit(1);
    }

    /* Loop through the linked list of results */
    struct addrinfo *current;
    char ip_str[INET6_ADDRSTRLEN]; /* Buffer for IP address as string (supports IPv6) */
    for (current = results; current != NULL; current = current->ai_next) {
        /* Pointer to the IP address in the address structure */
        void *addr;
        char *ip_version;

        /* Extract the IP address based on family (IPv4 or IPv6) */
        if (current->ai_family == AF_INET) { /* IPv4 */
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)current->ai_addr;
            addr = &(ipv4->sin_addr);
            ip_version = "IPv4";
        } else if (current->ai_family == AF_INET6) { /* IPv6 */
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)current->ai_addr;
            addr = &(ipv6->sin6_addr);
            ip_version = "IPv6";
        } else {
            continue; /* Skip unknown address families */
        }

        /* Convert binary IP to string (e.g., 172.217.167.78) */
        inet_ntop(current->ai_family, addr, ip_str, sizeof(ip_str));
        printf("%s: %s\n", ip_version, ip_str);
    }

    /* Free the linked list to avoid memory leaks */
    freeaddrinfo(results);

    return 0;
}
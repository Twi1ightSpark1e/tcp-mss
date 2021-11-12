#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include <netinet/tcp.h>

#include <sys/param.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <unistd.h>

int handle_opts(int argc, char* argv[], bool* is_server, char** client_addr) {
    char* argv0 = argv[0];

    bool clientflag = false;
    int index;
    int c;
    opterr = 0;

    while ((c = getopt(argc, argv, "hsc:")) != -1) {
        switch (c) {
            case 'h':
                printf("Usage: %s [-h] [-s | -c <IP>]\n", argv0);
                exit(0);
            case 's':
                if (clientflag != false) {
                    fputs("ti cho ebanulsa", stderr);
                    return 2;
                }
                *is_server = true;
                break;
            case 'c':
                if (*is_server != false) {
                    fputs("ti cho ebanulsa", stderr);
                    return 2;
                }
                clientflag = true;
                strcpy(*client_addr, optarg);
                break;
            case '?':
                if (optopt == 'c') fprintf(stderr, "Option `-%c` requires an argument.\n", optopt);
                else if (isprint(optopt)) fprintf(stderr, "Unknown option `-%c`.\n", optopt);
                else fprintf(stderr, "Unknown option character `\\x%x`.\n", optopt);
                return 1;
            default:
                abort();
        }
    }

    if (!*is_server && !clientflag) {
        fprintf(stderr, "Usage: %s [-h] [-s | -c <IP>]\n", argv0);
        return 1;
    }

    return 0;
}

int get_address_family(const char* addr) {
    char buf[16];
    if (inet_pton(AF_INET, addr, buf)) return AF_INET;
    if (inet_pton(AF_INET6, addr, buf)) return AF_INET6;
    return -1;
}

int server_mode() {
    int v4_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (v4_sock_fd == -1) {
        fputs("Could not create AF_INET socket.", stderr);
        return 3;
    }
    int v6_sock_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (v6_sock_fd == -1) {
        fputs("Could not create AF_INET6 socket.", stderr);
        return 3;
    }

    struct sockaddr_in v4_server;
    v4_server.sin_family = AF_INET;
    v4_server.sin_addr.s_addr = INADDR_ANY;
    v4_server.sin_port = 3123;
    if (bind(v4_sock_fd, (struct sockaddr*)&v4_server, sizeof(v4_server)) < 0) {
        fputs("Could not bind IPv4 socket.", stderr);
        return 4;
    }
    listen(v4_sock_fd, 1);
    puts("Started listening on 0.0.0.0:3123");

    struct sockaddr_in6 v6_server;
    v6_server.sin6_family = AF_INET6;
    v6_server.sin6_addr = in6addr_any;
    v6_server.sin6_port = htons(3123);
    v6_server.sin6_flowinfo = 0;
    if (bind(v6_sock_fd, (struct sockaddr*)&v6_server, sizeof(v6_server)) < 0) {
        fputs("Could not bind IPv6 socket.", stderr);
        return 4;
    }
    listen(v6_sock_fd, 1);
    puts("Started listening on [::]:3123");

    fd_set fds;
    while (true) {
        FD_ZERO(&fds);
        FD_SET(v4_sock_fd, &fds);
        FD_SET(v6_sock_fd, &fds);

        int code = select(MAX(v4_sock_fd, v6_sock_fd) + 1, &fds, NULL, NULL, NULL);
        if (code < 1) {
            fprintf(stderr, "Something wrong happened... `select` returned %d", code);
            return 5;
        }

        if (FD_ISSET(v4_sock_fd, &fds)) {
            struct sockaddr_in client_name;
            socklen_t c = sizeof(client_name);
            int client_fd = accept(v4_sock_fd, (struct sockaddr*)&client_name, &c);

            char client_addr[INET_ADDRSTRLEN];
            const char* _ = inet_ntop(AF_INET, &client_name.sin_addr, client_addr, sizeof(client_addr));
            // inet_ntoa(client_name.sin_addr);
            printf("Incoming connectiong from %s:%d...", client_addr, client_name.sin_port);

            int mss;
            socklen_t len = sizeof(mss);
            getsockopt(client_fd, IPPROTO_TCP, TCP_MAXSEG, &mss, &len);
            printf("TCP MSS is %d.\n", mss);

            close(client_fd);
            continue;
        }

        if (FD_ISSET(v6_sock_fd, &fds)) {
            struct sockaddr_in6 client_name;
            socklen_t c = sizeof(client_name);
            int client_fd = accept(v6_sock_fd, (struct sockaddr*)&client_name, &c);

            char client_addr[INET6_ADDRSTRLEN];
            const char* _ = inet_ntop(AF_INET6, &client_name.sin6_addr, client_addr, sizeof(client_addr));
            printf("Incoming connectiong from [%s]:%d...", client_addr, client_name.sin6_port);

            int mss;
            socklen_t len = sizeof(mss);
            getsockopt(client_fd, IPPROTO_TCP, TCP_MAXSEG, &mss, &len);
            printf("TCP MSS is %d.\n", mss);

            close(client_fd);
            continue;
        }
    }

    return 0;
}

int client_mode(const char* client_addr) {
    int af_type = get_address_family(client_addr);
    if (af_type < 0) {
        fputs("Given IP address is not a valid IPv4 nor IPv6 address.", stderr);
        return 3;
    }

    int sock = socket(af_type, SOCK_STREAM, 0);
    if (sock < 0) {
        fputs("Could not create socket.", stderr);
        return 4;
    }

    struct sockaddr *server = NULL;
    socklen_t server_len;
    if (af_type == AF_INET) {
        size_t size = sizeof(struct sockaddr_in);
        struct sockaddr_in* server_v4 = (struct sockaddr_in*)malloc(size);
        inet_pton(AF_INET, client_addr, &server_v4->sin_addr);
        server_v4->sin_family = AF_INET;
        server_v4->sin_port = 3123;
        server = (struct sockaddr*)server_v4;
        server_len = size;
    } else if (af_type == AF_INET6) {
        size_t size = sizeof(struct sockaddr_in6);
        struct sockaddr_in6* server_v6 = (struct sockaddr_in6*)malloc(size);
        inet_pton(AF_INET6, client_addr, &server_v6->sin6_addr);
        server_v6->sin6_family = AF_INET6;
        server_v6->sin6_port = htons(3123);
        server = (struct sockaddr*)server_v6;
        server_len = size;
    }

    if (connect(sock, server, server_len) < 0) {
        fputs("Could not connect to server.", stderr);
        return 5;
    }

    printf("Connected to %s:3123...", client_addr);

    //char buf[1];
    //read(sock, buf, 1);

    int mss;
    socklen_t len = sizeof(mss);
    getsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, &mss, &len);
    printf("TCP MSS is %d.\n", mss);

    close(sock);
    return 0;
}

int main(int argc, char* argv[]) {
    char *client_addr = (char*)malloc(40 * sizeof(char));
    bool is_server = false;

    int handle_args = handle_opts(argc, argv, &is_server, &client_addr);
    if (handle_args != 0) return handle_args;

    int code = 0;
    if (is_server) code = server_mode();
    else code = client_mode(client_addr);

    return code;
}

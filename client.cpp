#include <iostream>
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <thread>
#include <string>
#include <signal.h>
#include <stdio.h>

void handle_server(int sock) {
    while (true) {
        uint32_t len;
        int res = recv(sock, &len, sizeof(len), 0);
        if (res > 0 && res != sizeof(len)) {
            std::cout << "Handle server error" << std::endl;
            return;
        }
        if (res <= 0)
            return;
        len = ntohl(len);

        char text[1024];
        res = recv(sock, text, len, 0);
        if (res > 0 && res != len) {
            std::cout << "Handle server error" << std::endl;
            return;
        }
        if (res <= 0)
            return;
        text[len] = '\0';
        std::cout << text << std::endl;
    }
}

void sig_handler(int sig) {
    if (sig == SIGINT) {
    }
}

int main(int argc, char **argv) {
    if (argc < 5) {
        std::cout << "Thieu tham so" << std::endl;
        return -1;
    }

    int sock;
    struct sockaddr_in address; 
    struct sockaddr_in serv_addr; 

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { 
        std::cout << "Socket creation error" << std::endl;
        return -1; 
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 
                                                      
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(atoi(argv[2])); 
                                                                     
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) { 
        std::cout << "Invalid address/ Address not supported" << std::endl;
        return -1; 
    } 

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 

    socklen_t len = sizeof(address);
    getsockname(sock, (struct sockaddr *)&address, &len);
    std::cout << "IP: " << std::hex << ntohl(address.sin_addr.s_addr) << std::endl;
    std::cout << "port: " << std::dec << ntohs(address.sin_port) << std::endl;

    // Connection setup
    uint32_t kind = 0;
    kind = htonl(kind);
    send(sock, &kind, sizeof(kind), 0);

    uint32_t send_ip;
    if (inet_pton(AF_INET, argv[3], &send_ip) <= 0) { 
        std::cout << "Invalid address/ Address not supported" << std::endl;
        return -1; 
    } 
    uint16_t send_port = htons(atoi(argv[4]));

    send(sock, &send_ip, sizeof(send_ip), 0);
    send(sock, &send_port, sizeof(send_port), 0);
    // End connection setup

    // Set up Ctrl C
    //  signal(SIGINT, sig_handler);
    // End set up Ctrl C

    std::thread thread([sock]() { handle_server(sock); });

    while (true) {
        std::string s;
        std::getline(std::cin, s);

        kind = 1;
        kind = htonl(kind);
        send(sock, &kind, sizeof(kind), 0);

        uint32_t len = s.length();
        uint32_t text_len = htonl(len);
        send(sock, &text_len, sizeof(text_len), 0);
        send(sock, s.c_str(), len, 0);
    }

    kind = 2;
    kind = htonl(kind);
    send(sock, &kind, sizeof(kind), 0);
    shutdown(sock, SHUT_WR);

    thread.join();
    
    return 0;
}

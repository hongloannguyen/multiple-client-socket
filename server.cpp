#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>

#define PORT 8000

#define MAX_THREAD_COUNT 32

#define THREAD_NONE 0
#define THREAD_RUNNING 1
#define THREAD_TERMINATED 2

struct Registry {
	uint32_t ip;
	uint16_t port;
	int socket;
};

std::mutex mutex;
Registry registry_list[MAX_THREAD_COUNT];
std::thread threads[MAX_THREAD_COUNT];
int thread_status[MAX_THREAD_COUNT];

struct HandleClient {
	int thread_index;
	int socket;
	uint32_t ip;
	uint16_t port;
};

int find_client(uint32_t ip, uint16_t port) {
    std::unique_lock<std::mutex> guard(mutex);
    for (int i = 0; i < MAX_THREAD_COUNT; i++)
        if (thread_status[i] == THREAD_RUNNING) {
            Registry r = registry_list[i];
            if (r.ip == ip && r.port == port)
                return r.socket;
        }
    return -1;
}


void handle_client(HandleClient client) {
    uint32_t send_ip;
    uint16_t send_port;

	while (true) {
		uint32_t kind;
		int res = recv(client.socket, &kind, sizeof(kind), 0);
		if (res > 0 && res != sizeof(kind)) {
			std::cout << "Connection setup error" << std::endl;
			break;
		}

        if (res <= 0)
            break;

        kind = ntohl(kind);
		if (kind == 0) {
			Registry registry;	
			registry.ip = client.ip;
			registry.port = client.port;
			registry.socket = client.socket;
            {
                std::unique_lock<std::mutex> guard(mutex);
                registry_list[client.thread_index] = registry;
            }

            res = recv(client.socket, &send_ip, sizeof(send_ip), 0);
            if (res != sizeof(send_ip)) {
                std::cout << "Connection setup error: send_ip" << std::endl;
                break;
            }

            res = recv(client.socket, &send_port, sizeof(send_port), 0);
            if (res != sizeof(send_port)) {
                std::cout << "Connection setup error: send_port" << std::endl;
                break;
            }
		}
		else if (kind = 1) {
            uint32_t len;   
            int res = recv(client.socket, &len, sizeof(len), 0);
            if (res != sizeof(len)) {
                std::cout << "Data exchange error: len" << std::endl;
                break;
            }
            len = ntohl(len);

            char text[1024];
            res = recv(client.socket, text, len, 0);
            if (res != len) {
                std::cout << "Data exchange error: text" << std::endl;
                break;
            }

            uint32_t text_len = htonl(len);
            text[len] = '\0';
            
            int send_socket = find_client(send_ip, send_port);
            if (send_socket == -1) {
                std::cout << "Can not find client" << std::endl;
                break;
            }

            send(send_socket, &text_len, sizeof(text_len), 0);
            send(send_socket, text, len, 0);
		}
		else if (kind = 2) {
            std::cout << "Connection teardown" << std::endl;
            shutdown(client.socket, SHUT_WR);
			break;
		}
		else {
			std::cout << "Kind not recognizable" << std::endl;
			break;
		}
	}	

    std::unique_lock<std::mutex> lk(mutex);
	thread_status[client.thread_index] = THREAD_TERMINATED;
}


int main() {
	int server_fd;

	// Creating socket file descriptor 
    	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
        	std::cout << "socket failed" << std::endl;
        	exit(EXIT_FAILURE); 
    	} 

	int opt = 1;
	// Forcefully attaching socket to the port 8080 
    	int res = setsockopt(
		server_fd, SOL_SOCKET, 
		SO_REUSEADDR | SO_REUSEPORT, 
                &opt, sizeof(opt));

	if (res) { 
        	std::cout << "setsockopt" << std::endl;
        	exit(EXIT_FAILURE); 
    	} 

	struct sockaddr_in address;
	address.sin_family = AF_INET; 
    	address.sin_addr.s_addr = INADDR_ANY; 
    	address.sin_port = htons(PORT); 
       
    	// Forcefully attaching socket to the port 8080 
    	res = bind(
		server_fd, 
		(struct sockaddr *)&address, sizeof(address));
	if (res < 0) { 
        	std::cout << "bind failed" << std::endl;
        	exit(EXIT_FAILURE); 
    	} 

	if (listen(server_fd, 10) < 0) { 
        	std::cout << "listen" << std::endl;
        	exit(EXIT_FAILURE); 
    	} 

	int addrlen = sizeof(address);
	while (true) {
		int new_socket = accept(
			server_fd, (struct sockaddr *)&address,  
			(socklen_t *)&addrlen);

		if (new_socket < 0) { 
			std::cout << "Accept error" << std::endl;
			exit(EXIT_FAILURE); 
		} 

		HandleClient client;
		client.socket = new_socket;
		client.ip = address.sin_addr.s_addr;
		client.port = address.sin_port;

        std::unique_lock<std::mutex> guard(mutex);
		for (int i = 0; i < MAX_THREAD_COUNT; i++) {
			if (thread_status[i] == THREAD_TERMINATED) {
				threads[i].join();
				thread_status[i] = THREAD_NONE;
			}
			if (thread_status[i] == THREAD_NONE) {
				client.thread_index = i;
				threads[i] = std::thread(
					[client]() { 
						handle_client(client);
					});	
                thread_status[i] = THREAD_RUNNING;
				break;
			}
		}
	}
	
	return 0;
}

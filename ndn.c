#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


typedef struct{
	char ip[16];
	int tcp_port;
	int socket_listening;
	struct NodeID *next;
}NodeID;


typedef struct{
	char ip[16];
	int tcp_port;
	int socket_listening;
	NodeID external;
	NodeID safeguard;
	NodeID *internal;
	int numInternals;
	int capacityInternals;
}NodeData;

void init_node(int cache_size, char *ip, int tcp_port, char *reg_ip, int reg_udp);

int init_socket_listening(int port);

int main(int argc, char *argv[]){

    char command[256];
    if (argc < 4) {
        printf("Usage: %s cache IP TCP [regIP] [regUDP]\n", argv[0]);
        return 1;
    }
	NodeID my_id;
	strncpy(my_id.ip, argv[2], sizeof(my_id.ip));
    my_id.tcp_port = atoi(argv[3]);
    char reg_server_ip[16];      // Registration server IP
    int reg_server_port;         // Registration server UDP port
    int cache_size;              // Cache size

  
    if (argc >= 5) {
        strncpy(reg_server_ip, argv[4], sizeof(reg_server_ip));
    } else {
        strncpy(reg_server_ip, "193.136.138.142", sizeof(reg_server_ip));
    }
    
    if (argc >= 6) {
        reg_server_port = atoi(argv[5]);
    } else {
        reg_server_port = 59000;
    }
    
    
    printf("Configuração:\n Cache: %d\n IP: %s\n TCP: %d\n RegIP: %s\n RegUDP: %d\n", 
        cache_size, my_id.ip, my_id.tcp_port, reg_server_ip, reg_server_port);


    // Initialize node
    init_node(cache_size, my_id.ip, my_id.tcp_port, reg_server_ip, reg_server_port);
	return 0;


}

void init_node(int cache_size, char *ip, int tcp_port, char *reg_ip, int reg_udp) {

   init_socket_listening(tcp_port);
   
}

int init_socket_listening(int port) {
    int sock;
    struct sockaddr_in server_addri;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }
    
    if (listen(sock, 5) < 0) {
        perror("listen");
        close(sock);
        return -1;
    }
    
    return sock;
}

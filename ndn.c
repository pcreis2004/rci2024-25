#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>  
#include <ctype.h>
#include <time.h>

#define BUFFER_SIZE 128

typedef struct NodeID {
    char ip[16];
    int tcp_port;
    int socket_fd;  // Socket de conexão com este nó
    int safe_sent;
} NodeID;

typedef struct {
    char ip[16];           // IP do nó atual
    int tcp_port;          // Porto TCP do nó atual
    int socket_listening;  // Socket de escuta para conexões entrantes
    NodeID vzext;          // Vizinho externo
    NodeID vzsalv;         // Vizinho de salvaguarda
    NodeID *intr;          // Lista de vizinhos internos
    int numInternals;      // Número atual de vizinhos internos
    int capacityInternals; // Capacidade alocada para vizinhos internos
    char **objects;        // Lista de objetos armazenados
    int numObjects;
    char **cache;          // Cache de objetos
    int cacheSize;
    int currentCacheSize;
} NodeData;

void init_node(NodeData *myNode, int cache_size, char *reg_ip, int reg_udp);
int init_socket_listening(int port, char *ip);
int handle_command(char *command, NodeData *myNode, char *ip, int port);
int djoin(NodeData *myNode, char *connectIP, int connectTCP);
int join(char *net,char *ip, int port,NodeData *myNode);
int connect_to_node(char *ip, int port);
// void handle_entry_response(NodeData *myNode, char *buffer);
void add_internal_neighbor(NodeData *myNode, NodeID neighbor);
void show_topology(NodeData *myNode);

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s cache IP TCP [regIP] [regUDP]\n", argv[0]);
        return 1;
    }

    NodeData my_node;
    strncpy(my_node.ip, argv[2], sizeof(my_node.ip) - 1);
    my_node.ip[sizeof(my_node.ip) - 1] = '\0';
    my_node.tcp_port = atoi(argv[3]);

    char reg_server_ip[16];
    int reg_server_port;
    int cache_size = atoi(argv[1]);

    if (argc >= 5) {
        strncpy(reg_server_ip, argv[4], sizeof(reg_server_ip) - 1);
        reg_server_ip[sizeof(reg_server_ip) - 1] = '\0';
    } else {
        strncpy(reg_server_ip, "193.136.138.142", sizeof(reg_server_ip) - 1);
    }

    if (argc >= 6) {
        reg_server_port = atoi(argv[5]);
    } else {
        reg_server_port = 59000;
    }

    printf("Configuração:\n Cache: %d\n IP: %s\n TCP: %d\n RegIP: %s\n RegUDP: %d\n",
           cache_size, my_node.ip, my_node.tcp_port, reg_server_ip, reg_server_port);

    printf("Inicializando nó...\n");

    init_node(&my_node, cache_size, reg_server_ip, reg_server_port);

    printf("NDN Node started. Enter commands:\n");

    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(STDIN_FILENO, &master_fds);         // Adicionar stdin para comandos de usuário
    FD_SET(my_node.socket_listening, &master_fds);//Adicionar socket de listening
    int max_fd = my_node.socket_listening; 

    // Adicionar sockets de conexão existentes ao master_fds
    if (my_node.vzext.socket_fd > 0) {
        FD_SET(my_node.vzext.socket_fd, &master_fds);
        if (my_node.vzext.socket_fd > max_fd) max_fd = my_node.vzext.socket_fd;
    }

    for (int i = 0; i < my_node.numInternals; i++) {
        if (my_node.intr[i].socket_fd > 0) {
            FD_SET(my_node.intr[i].socket_fd, &master_fds);
            if (my_node.intr[i].socket_fd > max_fd) max_fd = my_node.intr[i].socket_fd;
        }
    }

    char command[BUFFER_SIZE];
    
    while (1) {
    read_fds = master_fds;
    printf("> ");
    fflush(stdout);
    
    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
        perror("select");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    
    // Verificar todas as conexões para atividade
    for (int i = 0; i <= max_fd; i++) {
        if (FD_ISSET(i, &read_fds)) {
            // Entrada do usuário no terminal
            if (i == STDIN_FILENO) {
                if (fgets(command, sizeof(command), stdin) != NULL) {
                    command[strcspn(command, "\n")] = '\0';  // Remover \n
                    int fd = handle_command(command, &my_node, reg_server_ip, reg_server_port);
                    if (fd != 0) {
                        FD_SET(fd, &master_fds);
                        if (max_fd < fd) {
                            max_fd = fd;
                        }
                    }
                }
            }
            // Nova conexão no socket de escuta
            else if (i == my_node.socket_listening) {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int new_fd = accept(my_node.socket_listening, (struct sockaddr *)&client_addr, &addrlen);
                
                if (new_fd == -1) {
                    perror("accept");
                } else {
                    FD_SET(new_fd, &master_fds);
                    if (new_fd > max_fd) max_fd = new_fd;
                    printf("Nova conexão aceita: FD %d\n", new_fd);
                }
            }
            
            // Dados recebidos de uma conexão existente
            else {
                memset(buffer, 0, BUFFER_SIZE);
                int n = read(i, buffer, BUFFER_SIZE - 1);
                
                if (n <= 0) {
                    if (n == 0) {
                        printf("Conexão fechada no FD %d\n", i);
                        // Tratar terminação de sessão conforme o protocolo
                    } else {
                        perror("read");
                    }
                    close(i);
                    FD_CLR(i, &master_fds);
                } else {
                    buffer[n] = '\0';  // Garantir terminação da string
                    printf("Recebido do FD %d: %s", i, buffer);
                    
                    // Processamento das mensagens do protocolo
                    if (strncmp(buffer, "ENTRY", 5) == 0) {
                        // printf("recebi uma mensagem de entry lol\n");
                        // Processar mensagem ENTRY
                        char ip[16];
                        int tcp_port;
                        
                        if (sscanf(buffer + 6, "%15s %d", ip, &tcp_port) == 2) {
                            NodeID new_node;
                            strncpy(new_node.ip, ip, sizeof(new_node.ip) - 1);
                            new_node.ip[sizeof(new_node.ip) - 1] = '\0';
                            new_node.tcp_port = tcp_port;
                            new_node.socket_fd = i;
                            
                            // Verificar se o nó atual tem vizinho externo
                            if (my_node.vzext.tcp_port == -1) {
                                // Se não tiver vizinho externo, o nó que está se juntando se torna o vizinho externo
                                strncpy(my_node.vzext.ip, new_node.ip, sizeof(my_node.vzext.ip) - 1);
                                my_node.vzext.ip[sizeof(my_node.vzext.ip) - 1] = '\0';
                                my_node.vzext.tcp_port = new_node.tcp_port;
                                my_node.vzext.socket_fd = i;
                                
                                // printf("Novo vizinho externo definido: %s:%d\n", my_node.vzext.ip, my_node.vzext.tcp_port);
                                
                                // Adicionar como vizinho interno
                                // printf("Adicionar vizinho interno %s --> %d\n", ip, tcp_port);
                                add_internal_neighbor(&my_node, new_node);
                                
                                // Primeiro, enviar mensagem SAFE
                                char safe_msg[BUFFER_SIZE];
                                snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", 
                                         my_node.vzext.ip, my_node.vzext.tcp_port);
                                         
                                if (send(i, safe_msg, strlen(safe_msg), 0) < 0) {
                                    perror("send SAFE");
                                } else {
                                    // printf("\t\tMensagem enviada para fd: %d ---> %s", i, safe_msg);
                                }
                                
                                // Introduzir um pequeno atraso para garantir que as mensagens sejam processadas separadamente
                                usleep(1);  // 100ms
                                
                                // Depois, enviar mensagem ENTRY
                                char entry_msg[BUFFER_SIZE];
                                snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", 
                                         my_node.ip, my_node.tcp_port);
                                
                                if (send(i, entry_msg, strlen(entry_msg), 0) < 0) {
                                    perror("send ENTRY");
                                } else {
                                    // printf("\t\tMensagem enviada para fd: %d ---> %s", i, entry_msg);
                                }
                            } else {
                                // Adicionar como vizinho interno apenas se não for o vizinho externo
                                
                                    
                                    // printf("Adicionar vizinho interno %s --> %d\n", ip, tcp_port);
                                    add_internal_neighbor(&my_node, new_node);
                                    
                                    // Enviar mensagem SAFE com o vizinho externo
                                    char safe_msg[BUFFER_SIZE];
                                    snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", 
                                             my_node.vzext.ip, my_node.vzext.tcp_port);
                                             
                                    if (send(i, safe_msg, strlen(safe_msg), 0) < 0) {
                                        perror("send SAFE");
                                    } else {
                                        // printf("\t\tMensagem enviada para fd: %d ---> %s", i, safe_msg);
                                    }
                                
                            }
                        }
                    }
                    else if (strncmp(buffer, "SAFE", 4) == 0) {
                        // Processar mensagem SAFE
                        // printf("\t\tMensagem de safe a ser processada\n");
                        char ip[16];
                        int tcp_port;
                        if (sscanf(buffer + 5, "%15s %d", ip, &tcp_port) == 2) {
                            // Atualizar nó de salvaguarda
                            strncpy(my_node.vzsalv.ip, ip, sizeof(my_node.vzsalv.ip) - 1);
                            my_node.vzsalv.ip[sizeof(my_node.vzsalv.ip) - 1] = '\0';
                            my_node.vzsalv.tcp_port = tcp_port;
                            // printf("Nó de salvaguarda atualizado: %s:%d\n", 
                                //    my_node.vzsalv.ip, my_node.vzsalv.tcp_port);
                        }
                    }
                }
            }
        }
    }
}
    return 0;
}

void init_node(NodeData *myNode, int cache_size, char *reg_ip, int reg_udp) {
    // Inicializar o nó com seu próprio ID como vizinho externo (inicialmente)
    
    memset(&myNode->vzext,0,sizeof(NodeID));
    myNode->vzext.tcp_port=-1;
    myNode->vzext.socket_fd=-1;
    myNode->vzext.safe_sent=0;
    // Inicializar vizinho de salvaguarda como não definido
    memset(&myNode->vzsalv, 0, sizeof(NodeID));
    myNode->vzsalv.tcp_port=-1;
    myNode->vzsalv.socket_fd = -1;
    myNode->vzsalv.safe_sent=0;
    // Inicializar lista de vizinhos internos
    myNode->intr = malloc(10 * sizeof(NodeID));  // Capacidade inicial
    myNode->numInternals = 0;
    myNode->capacityInternals = 10;
    
    // Inicializar cache e objetos
    myNode->cacheSize = cache_size;
    myNode->currentCacheSize = 0;
    myNode->cache = malloc(cache_size * sizeof(char*));
    
    myNode->objects = malloc(100 * sizeof(char*));  // Capacidade inicial
    myNode->numObjects = 0;
    
    // Inicializar socket de escuta
    myNode->socket_listening = init_socket_listening(myNode->tcp_port, myNode->ip);
}

int init_socket_listening(int port, char *ip) {
    struct addrinfo hints, *res;
    int fd, errcode;
    char port_str[6];

    sprintf(port_str, "%d", port);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    
    // Permitir reutilização do endereço
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((errcode = getaddrinfo(ip, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
        exit(1);
    }

    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(res);
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(fd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Nó escutando no porto %s...\n", port_str);
    return fd;
}

int handle_command(char *command, NodeData *myNode, char *ip, int port) {
    // printf("Comando recebido: %s\n", command);

    char cmd[20];
    char arg1[100], arg2[100];
    
    // Extrair comando e argumentos
    int args = sscanf(command, "%19s %99s %99s", cmd, arg1, arg2);
    
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        printf("Comandos disponíveis:\n");
        printf("  join (j) net - Entrada na rede net\n");
        printf("  direct join (dj) conIP conTCP - Entrada ligando ao nó especificado\n");
        printf("  create (c) name - Criação de um objeto com nome name\n");
        printf("  delete (dl) name - Remoção do objeto com nome name\n");
        printf("  retrieve (r) name - Pesquisa do objeto com nome name\n");
        printf("  show topology (st) - Visualização dos vizinhos\n");
        printf("  show names (sn) - Visualização dos nomes dos objetos guardados\n");
        printf("  show interest table (si) - Visualização da tabela de interesses\n");
        printf("  leave (l) - Saída da rede\n");
        printf("  exit (x) - Fecha a aplicação\n");
    } 
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "x") == 0) {
        printf("Saindo...\n");
        exit(0);
    } 
    else if (strcmp(cmd, "direct") == 0 && args >= 3) {
        if (strcmp(arg1, "join") == 0 || strcmp(arg1, "j") == 0) {
            int port;
            if (sscanf(arg2, "%d", &port) == 1) {
                printf("Uso correto: direct join conIP conTCP\n");
            } else {
                printf("Conectando ao nó %s:%s...\n", arg1, arg2);
                djoin(myNode, arg1, atoi(arg2));
                return 1;
            }
        }
    }
    else if ((strcmp(cmd, "dj") == 0 || strcmp(cmd, "direct_join") == 0) && args >= 3) {
        // printf("Conectando ao nó %s:%s...\n", arg1, arg2);
        int fd = djoin(myNode, arg1, atoi(arg2));
        return fd;
    }
    else if (strcmp(cmd, "show") == 0 && args >= 2) {
        if (strcmp(arg1, "topology") == 0 || strcmp(arg1, "st") == 0) {
            show_topology(myNode);
        }
    }
    else if (strcmp(cmd,"j")==0)
    {
        int fd = join(arg1,ip,port,myNode);
        return fd;
    }
    
    else if (strcmp(cmd, "st") == 0) {
        show_topology(myNode);
    }
    else {
        printf("Comando desconhecido: %s\n", command);
    }
    return 0;
}

int join(char *net, char *ip, int port,NodeData *myNode) {
    // conectar ao servidor
    struct addrinfo hints, *res;
    socklen_t addrlen;
    struct sockaddr addr;
    int fd, errcode;
    ssize_t n;
    
    fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (fd == -1) /*error*/ exit(1);
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    
    char buffer[1024]; // Increased buffer size to handle multiple lines
    sprintf(buffer, "%d", port);
    
    errcode = getaddrinfo(ip, buffer, &hints, &res);
    if (errcode != 0) return -1;
    
    char mensagem[20];
    snprintf(mensagem, sizeof(mensagem), "NODES %s\n", net);
    
    n = sendto(fd, mensagem, strlen(mensagem), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) /*error*/ exit(1);
    
    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
    if (n == -1) return -1;
    
    buffer[n] = '\0';
    printf("> Nó connectado à net %s\n", net);
    
    // Parse the response to get IP addresses and ports
    char *line = strtok(buffer, "\n");
    
    // Skip first line (NODESLIST 100)
    if (line != NULL) {
        line = strtok(NULL, "\n");
    }
    
    // Count how many servers we have
    int server_count = 0;
    char *servers[100]; // Assuming maximum 100 servers
    
    while (line != NULL && server_count < 100) {
        servers[server_count++] = line;
        line = strtok(NULL, "\n");
    }
    // If no servers available, return error
    if (server_count == 0) {
        char msgServer[BUFFER_SIZE];
        snprintf(msgServer, sizeof(msgServer), "REG %s %s %d\n", net,myNode->ip,myNode->tcp_port);
        // printf("A mensagem enviada foi %s", msgServer);
        n = sendto(fd, msgServer, strlen(msgServer), 0, res->ai_addr, res->ai_addrlen);
        // printf("Nó registado no servidor\n");
        if(n == -1) /*error*/ exit(1);
        // Store the selected IP and port in variables
        // (You might want to modify the function parameters to pass these back)
        // Example: strcpy(output_ip, selected_ip); *output_port = selected_port;
        close(fd);
        return 0;
    }
    
    // Choose a random server
    srand(time(NULL));
    int random_index = rand() % server_count;
    char selected_server[128];
    strcpy(selected_server, servers[random_index]);
    
    // Extract IP and port from the selected server
    char selected_ip[64];
    int selected_port;
    sscanf(selected_server, "%s %d", selected_ip, &selected_port);
    
    // printf("Randomly selected server: %s:%d\n", selected_ip, selected_port);


    int filed = djoin(myNode,selected_ip,selected_port);

    char msgServer[BUFFER_SIZE];
    snprintf(msgServer, sizeof(msgServer), "REG %s %s %d\n", net,myNode->ip,myNode->tcp_port);
    n = sendto(fd, msgServer, strlen(msgServer), 0, res->ai_addr, res->ai_addrlen);
    // printf("Nó registado no servidor\n");
    if (n == -1) /*error*/ exit(1);
    printf("> Nó registado na net %s\n", net);

    // Store the selected IP and port in variables
    // (You might want to modify the function parameters to pass these back)
    // Example: strcpy(output_ip, selected_ip); *output_port = selected_port;
    freeaddrinfo(res);

    close(fd);
    return filed;
}

int djoin(NodeData *myNode, char *connectIP, int connectTCP) {
    if (strcmp(connectIP, "0.0.0.0") == 0) {
        printf("Criando rede com nó raiz (%s:%d)\n", myNode->ip, myNode->tcp_port);
        
        // Inicializar o nó como raiz
        strncpy(myNode->vzext.ip, myNode->ip, sizeof(myNode->vzext.ip));
        myNode->vzext.tcp_port = myNode->tcp_port;
        myNode->vzext.socket_fd = -1;
        
        // Como é raiz, o nó de salvaguarda é ele mesmo
        strncpy(myNode->vzsalv.ip, myNode->ip, sizeof(myNode->vzsalv.ip));
        myNode->vzsalv.tcp_port = myNode->tcp_port;
        myNode->vzsalv.socket_fd = -1;
        
        myNode->numInternals = 0;
        
        return 0;
    } else {
        // Conectar ao nó externo especificado
        int sockfd = connect_to_node(connectIP, connectTCP);
        if (sockfd < 0) {
            printf("Falha ao conectar a %s:%d\n", connectIP, connectTCP);
            return -1;
        }
        
        printf("Conectado a %s:%d (socket: %d)\n", connectIP, connectTCP, sockfd);
        
        // Atualizar vizinho externo
        strncpy(myNode->vzext.ip, connectIP, sizeof(myNode->vzext.ip) - 1);
        myNode->vzext.ip[sizeof(myNode->vzext.ip) - 1] = '\0';
        myNode->vzext.tcp_port = connectTCP;
        myNode->vzext.socket_fd = sockfd;
        myNode->vzext.safe_sent = 0;
        
        // Enviar mensagem ENTRY
        char entry_msg[BUFFER_SIZE];
        snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", myNode->ip, myNode->tcp_port);
        
        if (send(sockfd, entry_msg, strlen(entry_msg), 0) < 0) {
            perror("send ENTRY");
            return -1;
        }
        
        // printf("\t\tMensagem enviada para o fd:%d ---> %s",sockfd,entry_msg);
        // printf("\t\tAguardando msg de SAFE . . . \n");
        
        // A resposta SAFE será tratada no loop principal que lê as mensagens recebidas
        return sockfd;
    }
}


int connect_to_node(char *ip, int port) {
    struct addrinfo hints, *res;
    int sockfd, errcode;
    char port_str[6]; // Enough for a 5-digit port number plus null terminator
    
    // Convert port integer to string
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    // Initialize hints structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    
    // Get address information
    if ((errcode = getaddrinfo(ip, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        return -1;
    }
    
    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }
    
    // Connect to the server
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    
    // Free the address info structure
    freeaddrinfo(res);
    
    return sockfd;
}


void add_internal_neighbor(NodeData *myNode, NodeID neighbor) {
    // Verificar se já temos esse vizinho
    for (int i = 0; i < myNode->numInternals; i++) {
        if (strcmp(myNode->intr[i].ip, neighbor.ip) == 0 && 
            myNode->intr[i].tcp_port == neighbor.tcp_port) {
            printf("Vizinho %s:%d já existe na lista\n", neighbor.ip, neighbor.tcp_port);
            return;
        }
    }
    neighbor.safe_sent=0;
    // Verificar se precisamos expandir o array
    if (myNode->numInternals >= myNode->capacityInternals) {
        myNode->capacityInternals *= 2;
        myNode->intr = realloc(myNode->intr, myNode->capacityInternals * sizeof(NodeID));
        if (!myNode->intr) {
            perror("realloc");
            exit(1);
        }
    }
    
    // Adicionar o novo vizinho
    myNode->intr[myNode->numInternals] = neighbor;
    myNode->numInternals++;
    
    // printf("Vizinho interno adicionado: %s:%d\n", neighbor.ip, neighbor.tcp_port);
}

void show_topology(NodeData *myNode) {
    printf("\n=== Topologia Atual ===\n");
    printf("ID: %s:%d\n", myNode->ip, myNode->tcp_port);
    printf("Vizinho Externo: %s:%d\n", myNode->vzext.ip, myNode->vzext.tcp_port);
    printf("Vizinho de Salvaguarda: %s:%d\n", 
           strlen(myNode->vzsalv.ip) > 0 ? myNode->vzsalv.ip : "Não definido", 
           myNode->vzsalv.tcp_port);
    
    printf("Vizinhos Internos (%d):\n", myNode->numInternals);
    for (int i = 0; i < myNode->numInternals; i++) {
        printf("  %d. %s:%d\n", i+1, myNode->intr[i].ip, myNode->intr[i].tcp_port);
    }
    printf("=====================\n\n");
}

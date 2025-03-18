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
#include <time.h>
#include "lib.h"

/* Função: init_node
   Inicializa a estrutura de dados de um nó da rede com as suas variáveis internas,
   cache, lista de objetos e socket de escuta.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData a ser inicializada.
    - cache_size: tamanho máximo da cache do nó.
   Retorno:
    - Descritor do socket de escuta criado para o nó.
*/

int init_node(NodeData *myNode, int cache_size) {
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
    return myNode->socket_listening;
}

/* Função: init_socket_listening
   Cria e configura um socket TCP para escuta em um IP e porto específicos.
   Define as opções do socket e associa-o ao endereço.
   Parâmetros:
    - port: número do porto em que o socket vai escutar.
    - ip: endereço IP associado ao socket.
   Retorno:
    - Descritor do socket de escuta pronto para aceitar conexões.
*/

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

/* Função: free_node_memory
   Liberta a memória alocada dinamicamente pela estrutura NodeData,
   incluindo listas de vizinhos, cache, objetos e encerra os sockets abertos.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData cujos recursos serão libertados.
   Retorno:
    - Nenhum.
*/


void free_node_memory(NodeData *myNode) {
    // Free the internal neighbors array
    if (myNode->intr != NULL) {
        free(myNode->intr);
        myNode->intr = NULL;
    }
    
    // Free the objects array and its contents
    if (myNode->objects != NULL) {
        for (int i = 0; i < myNode->numObjects; i++) {
            if (myNode->objects[i] != NULL) {
                free(myNode->objects[i]);
            }
        }
        free(myNode->objects);
        myNode->objects = NULL;
    }
    
    // Free the cache array and its contents
    if (myNode->cache != NULL) {
        for (int i = 0; i < myNode->currentCacheSize; i++) {
            if (myNode->cache[i] != NULL) {
                free(myNode->cache[i]);
            }
        }
        free(myNode->cache);
        myNode->cache = NULL;
    }
    
    // Close sockets
    if (myNode->socket_listening > 0) {
        close(myNode->socket_listening);
    }
    
    if (myNode->vzext.socket_fd > 0) {
        close(myNode->vzext.socket_fd);
    }
    
    // Close all internal neighbors' sockets
    for (int i = 0; i < myNode->numInternals; i++) {
        if (myNode->intr[i].socket_fd > 0) {
            close(myNode->intr[i].socket_fd);
        }
    }
}
/* Função: connect_to_node
   Estabelece uma conexão TCP com outro nó, a partir de um IP e porto dados.
   Parâmetros:
    - ip: endereço IP do nó de destino.
    - port: número do porto TCP do nó de destino.
   Retorno:
    - Descritor do socket conectado em caso de sucesso, ou -1 em caso de erro.
*/


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

/* Função: add_internal_neighbor
   Adiciona um vizinho interno à lista de vizinhos do nó, verificando duplicações
   e realocando espaço se necessário.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData onde será adicionado o vizinho.
    - neighbor: estrutura NodeID com as informações do vizinho a ser adicionado.
   Retorno:
    - Nenhum.
*/


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

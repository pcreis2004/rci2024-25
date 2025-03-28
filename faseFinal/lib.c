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
#include <stdbool.h>
#include "lib.h"

/* Função: removeInternal
   Remove um vizinho interno da lista ao detetar o encerramento da ligação.
   Parâmetros:
    - mynode: ponteiro para a estrutura NodeData do nó atual.
    - fdClosed: descritor de socket que foi fechado.
*/

void removeInternal(NodeData *mynode, int fdClosed) {
    for (int i = 0; i < 10; i++) {
        if (fdClosed == mynode->intr[i].socket_fd) {
            mynode->intr[i].tcp_port = -1;
            mynode->intr[i].socket_fd = -1;
            mynode->intr[i].ip[0] = '\0';
            mynode->numInternalsReal--;
        }
    }
}

/* Função: promoteRandomInternalToExternal
   Promove um vizinho interno aleatório a vizinho externo, e envia-lhe uma mensagem ENTRY.
   Parâmetros:
    - mynode: ponteiro para a estrutura NodeData do nó atual.
    - master_fds: conjunto de descritores usados pela função select.
    - max_fd: ponteiro para o maior descritor de socket atual.
*/
void promoteRandomInternalToExternal(NodeData *mynode, fd_set *master_fds, int *max_fd) {
    srand(time(NULL));
    int i;
    do {
        i = rand() % mynode->numInternals;
    } while (mynode->intr[i].socket_fd == -1);

    mynode->vzext = mynode->intr[i];

    sendEntryMessage(mynode, mynode->vzext.socket_fd);
    FD_SET(mynode->vzext.socket_fd, master_fds);

    updateMaxFD(master_fds, max_fd);
}
/* Função: fallbackToSafeguard
   Conecta ao nó de salvaguarda e atualiza o vizinho externo com essa ligação.
   Envia mensagem ENTRY e SAFE conforme necessário.
   Parâmetros:
    - mynode: ponteiro para a estrutura NodeData do nó atual.
    - master_fds: conjunto de descritores usados pela função select.
    - max_fd: ponteiro para o maior descritor de socket atual.
   Retorno:
    - 0 em caso de sucesso, -1 em caso de falha.
*/
int fallbackToSafeguard(NodeData *mynode, fd_set *master_fds, int *max_fd) {
    int sockfd = connect_to_node(mynode->vzsalv.ip, mynode->vzsalv.tcp_port);
    if (sockfd < 0) {
        printf("Falha ao conectar a %s:%d\n", mynode->vzsalv.ip, mynode->vzsalv.tcp_port);
        return -1;
    }

    mynode->vzext.socket_fd = sockfd;
    strcpy(mynode->vzext.ip, mynode->vzsalv.ip);
    mynode->vzext.tcp_port = mynode->vzsalv.tcp_port;

    sendEntryMessage(mynode, sockfd);
    FD_SET(sockfd, master_fds);
    updateMaxFD(master_fds, max_fd);
    sendSafeToAllInternals(mynode);
    return 0;
}


/* Função: sendEntryMessage
   Envia uma mensagem ENTRY ao vizinho externo especificado.
   Parâmetros:
    - mynode: ponteiro para a estrutura NodeData do nó atual.
    - socket_fd: descritor da socket onde a mensagem será enviada.
*/
void sendEntryMessage(NodeData *mynode, int socket_fd) {
    char entry_msg[BUFFER_SIZE];
    snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", mynode->ip, mynode->tcp_port);
    writeFull(socket_fd, entry_msg);
}


/* Função: sendSafeToAllInternals
   Envia uma mensagem SAFE a todos os vizinhos internos.
   Parâmetros:
    - mynode: ponteiro para a estrutura NodeData do nó atual.
*/
void sendSafeToAllInternals(NodeData *mynode) {
    char safe_msg[BUFFER_SIZE];
    snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", mynode->vzext.ip, mynode->vzext.tcp_port);

    for (int i = 0; i < mynode->numInternals; i++) {
        if (mynode->intr[i].socket_fd != -1) {
            writeFull(mynode->intr[i].socket_fd, safe_msg);
        }
    }
}


/* Função: writeFull
   Garante o envio completo de uma mensagem através de uma socket.
   Parâmetros:
    - socket_fd: descritor da socket.
    - msg: mensagem a ser enviada.
*/
void writeFull(int socket_fd, const char *msg) {
    ssize_t nleft = strlen(msg), nwritten;
    const char *ptr = msg;
    while (nleft > 0) {
        nwritten = write(socket_fd, ptr, nleft);
        if (nwritten <= 0) {
            perror("Erro ao escrever");
            exit(1);
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
}


/* Função: updateMaxFD
   Atualiza o valor do maior descritor de socket (max_fd) usado no conjunto master_fds.
   Parâmetros:
    - master_fds: conjunto de descritores usados pela função select.
    - max_fd: ponteiro para o maior descritor a ser atualizado.
*/
void updateMaxFD(fd_set *master_fds, int *max_fd) {
    *max_fd = STDIN_FILENO;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, master_fds) && i > *max_fd) {
            *max_fd = i;
        }
    }
}

/* Função: cleanNeighboors
   Limpa todos os vizinhos (externo, salvaguarda e internos), fecha as sockets associadas,
   remove-as do conjunto master_fds, e reinicializa os contadores.
   Parâmetros:
    - my_node: ponteiro para a estrutura NodeData do nó atual.
    - master_fds: conjunto de descritores usados pela função select.
   Retorno:
    - Novo valor de max_fd após limpeza.
*/


int cleanNeighboors(NodeData *my_node, fd_set *master_fds) {
    printf("A limpar tudo\n");


    if (my_node->vzext.socket_fd >= 0) {
        FD_CLR(my_node->vzext.socket_fd, master_fds);
        my_node->vzext.tcp_port = -1;
        my_node->vzext.socket_fd = -1;
        strcpy(my_node->vzext.ip, "");
    }
    if (my_node->vzsalv.socket_fd >= 0) {
        FD_CLR(my_node->vzsalv.socket_fd, master_fds);
        my_node->vzsalv.tcp_port = -1;
        my_node->vzsalv.socket_fd = -1;
        strcpy(my_node->vzsalv.ip, "");
    }
    for (int i = 0; i < 10; i++) {
        if (my_node->intr[i].socket_fd >= 0) {
            FD_CLR(my_node->intr[i].socket_fd, master_fds);
            my_node->intr[i].tcp_port = -1;
            my_node->intr[i].socket_fd = -1;
            my_node->intr[i].ip[0] = '\0';
            my_node->numInternalsReal--;
            printf("Vizinho interno %d limpo\n", i);
        }
    }
    my_node->numInternals = 0;
    my_node->numInternalsReal = 0;
    // Recalcular max_fd
    int new_max_fd = STDIN_FILENO;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, master_fds)) {
            new_max_fd = i;

        }
    }
    return new_max_fd;
}
/* Função: meterTudoAzero
   Reinicializa todas as variáveis internas do nó, incluindo vizinhos e contadores.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData a ser reinicializada.
   Retorno:
    - 0 em caso de sucesso.

*/
int meterTudoAzero(NodeData *myNode) {

   

    myNode->vzext.espera=0;
    myNode->vzext.fechado=0;
    myNode->vzext.resposta=0;
    
    myNode->vzsalv.espera=0;
    myNode->vzsalv.fechado=0;
    myNode->vzsalv.resposta=0;
    
    for (int i = 0; i < 10; i++)
    {
        myNode->intr[i].espera=0;
        myNode->intr[i].fechado=0;
        myNode->intr[i].resposta=0;
    }
    

    
    myNode->nodes_em_espera=0;
    myNode->flagoriginretrieve=0;
    myNode->interface_retrieve=0;

    return 0;
}

/* Função add_to_cache
   Adiciona um objeto à cache do nó, verificando se já existe e se há espaço.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
    - name: nome do objeto a ser adicionado à cache.
   Retorno:
    - 1 em caso de sucesso, 0 se o objeto já existe, -1 se a cache está cheia.
*/
int add_to_cache(NodeData *myNode, char *name) {



    // Verificar se o objeto já está na cache
    for (int i = 0; i < myNode->cacheSize * 100; i+=100) {
        if (strncmp(&myNode->cache[i], name, 100) == 0) {
            printf("Objeto '%s' já está na cache.\n", name);
            return 0; // Já existe, não precisa adicionar
        }
    }

    // Procurar um espaço vazio para adicionar
    for (int i = 0; i < myNode->cacheSize*100; i+=100) {
        if (myNode->cache[i] == '\0') { // Verifica se o espaço está vazio
            strncpy(&myNode->cache[i], name, 100);
            myNode->cache[i+99] = '\0'; // Garantir terminação
            if(myNode->indexCacheFI==-1){
                myNode->indexCacheFI=i;
            }
            printf("Objeto '%s' adicionado à cache na posição %d.\n", name, i);
            return 1; // Sucesso
        }
    }

    myNode->cache[myNode->indexCacheFI*100] = '\0'; // Limpa o espaço na cache
    myNode->indexCacheFI++;
    if(myNode->indexCacheFI >= myNode->cacheSize){

        myNode->indexCacheFI=0; // Reinicia o índice

    }
    
    add_to_cache(myNode, name); // Tenta adicionar novamente

    return 0; // Falha ao adicionar
}

/* Função: printCache
   Imprime o conteúdo da cache do nó.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
   Retorno:
    - 0 em caso de sucesso.

*/

int printCache(NodeData *myNode) {
    printf("Cache:\n");
    for (int i = 0; i < myNode->cacheSize; i++) {
        char *entry = myNode->cache + (i * 100);
        
        if (entry[0] != '\0') { // Só imprime se houver algo armazenado
            printf(" - %s\n", entry);
        }
    }
    return 0;
}


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
    myNode->flagoriginretrieve=0;
    myNode->interface_retrieve=0;
    myNode->objectfound=0;
    myNode->nodes_em_espera=0;
    
    myNode->indexCacheFI=-1;

    memset(&myNode->vzext,0,sizeof(NodeID));
    myNode->vzext.tcp_port=-1;
    myNode->vzext.socket_fd=-1;
    myNode->vzext.safe_sent=0;
    myNode->vzext.resposta=0;
    myNode->vzext.espera=0;
    myNode->vzext.fechado=0;    

    // Inicializar vizinho de salvaguarda como não definido
    memset(&myNode->vzsalv, 0, sizeof(NodeID));
    myNode->vzsalv.tcp_port=-1;
    myNode->vzsalv.socket_fd = -1;
    myNode->vzsalv.safe_sent=0;
    myNode->vzsalv.resposta=0;
    myNode->vzsalv.espera=0;
    myNode->vzsalv.fechado=0;

    // Inicializar lista de vizinhos internos
    myNode->intr = malloc(10 * sizeof(NodeID));  // Capacidade inicial
    for (int i = 0; i < 10; i++)
    {
        memset(&myNode->intr[i], 0, sizeof(NodeID));
        myNode->intr[i].tcp_port = -1;
        myNode->intr[i].socket_fd = -1;
        myNode->intr[i].safe_sent = 0;
        myNode->intr[i].ip[0] = '\0';
        myNode->intr[i].resposta=0;
        myNode->intr[i].espera=0;
        myNode->intr[i].fechado=0;
    }
    myNode->numInternalsReal = 0;
    myNode->numInternals = 0;
    myNode->capacityInternals = 10;
    
    // Inicializar cache e objetos
    myNode->cacheSize = cache_size;
    
    myNode->cache = calloc(cache_size, 100);   // cacheSize blocos de 100 chars
    myNode->objects = calloc(10, 100);          // 10 blocos de 100 chars


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
    // Associar o socket ao endereço
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(res);
        exit(1);
    }

    freeaddrinfo(res);
    // Definir o socket para escuta
    if (listen(fd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Nó à escuta no porto %s...\n", port_str);
    return fd;
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
    char port_str[6]; 
    
    // Converter int para string
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    // Inicializar a estrutura hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    
    // Informação do endereço
    if ((errcode = getaddrinfo(ip, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        return -1;
    }
    
    // Cria a scoket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }
    
    // Conectar ao servidor
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    
    // Libertar a memória alocada para a estrutura addrinfo
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
        if (strcmp(myNode->intr[i].ip, neighbor.ip) == 0 && myNode->intr[i].tcp_port == neighbor.tcp_port) {
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
    if (myNode->numInternals == 0)
    {
        myNode->intr[0] = neighbor;
        myNode->numInternalsReal++;
        if (myNode->numInternalsReal > myNode->numInternals)
    {
        myNode->numInternals=myNode->numInternalsReal;
    }
            printf("Vizinho interno adicionado: %s:%d\n", neighbor.ip, neighbor.tcp_port);
            return;

    }
    
    // Adicionar o vizinho interno à lista
    for (int i = 0; i < 10; i++)
    {
        if (myNode->intr[i].socket_fd == -1)
        {
            myNode->intr[i] = neighbor;

            myNode->numInternalsReal++;
            if (myNode->numInternalsReal > myNode->numInternals)
    {
        myNode->numInternals=myNode->numInternalsReal;
    }
            printf("Vizinho interno adicionado: %s:%d\n", neighbor.ip, neighbor.tcp_port);
            return;
        }
        
    }
    
}

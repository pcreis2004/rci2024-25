#ifndef LIB_H
#define LIB_H

#define BUFFER_SIZE 128


typedef struct NodeID {
    char ip[16];    // IP do nó
    int tcp_port;   // Porto TCP do nó
    int socket_fd;  // Socket de conexão com este nó
    int safe_sent;  // Flag para indicar se a mensagem SAFE foi enviada

    int resposta;   // Flag para indicar se o nó é o de resposta
    int espera;     // Flag para indicar se o nó está a aguardar resposta
    int fechado;     // Flag para indicar se o nó está fechado

} NodeID;

typedef struct {
    int flagoriginretrieve;   // Flag para indicar se o nó é o nó que invocou o retrieve
    int interface_retrieve;  // Flag para indicar se o nó está aguardando resposta de outro nó
    int flaginit;            // Flag para indicar se o nó foi inicializado

    int objectfound;    // Flag para indicar se o objeto foi encontrado

    int nodes_em_espera; // Número de nós em espera

    char ip[16];
    char net[4];           // IP do nó atual
    int tcp_port;          // Porto TCP do nó atual
    int socket_listening;  // Socket de escuta para conexões entrantes
    NodeID vzext;          // Vizinho externo
    NodeID vzsalv;         // Vizinho de salvaguarda
    NodeID *intr;          // Lista de vizinhos internos
    int numInternals;      // Número máximo de vizinhos internos que já tivemos
    int numInternalsReal;  // Número real de vizinhos internos
    int capacityInternals; // Capacidade alocada para vizinhos internos
    
    char *objects;        // Vetor de objetos armazenados
    int numObjects;         // Número de objetos armazenados
    
    char *cache;          // Cache de objetos
    int cacheSize;          // Tamanho da cache

    int indexCacheFI;       // Índice do "First in" na cache

} NodeData;

int handleLeave(NodeData *mynode,int fdClosed, fd_set *master_fds,int *max_fd);
void removeInternal(NodeData *mynode, int fdClosed);
void promoteRandomInternalToExternal(NodeData *mynode, fd_set *master_fds, int *max_fd);
int fallbackToSafeguard(NodeData *mynode, fd_set *master_fds, int *max_fd);
void sendEntryMessage(NodeData *mynode, int socket_fd);
void sendSafeToAllInternals(NodeData *mynode);
void writeFull(int socket_fd, const char *msg);
void updateMaxFD(fd_set *master_fds, int *max_fd);
int cleanNeighboors(NodeData *my_node, fd_set *master_fds);
int meterTudoAzero(NodeData *myNode);
int add_to_cache(NodeData *myNode, char *object);
int printCache(NodeData *myNode);
int init_node(NodeData *myNode, int cache_size);
int init_socket_listening(int port, char *ip);
int connect_to_node(char *ip, int port);
void add_internal_neighbor(NodeData *myNode, NodeID neighbor);

#endif // LIB_H
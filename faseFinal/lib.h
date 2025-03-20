#ifndef LIB_H
#define LIB_H

#define BUFFER_SIZE 128


typedef struct NodeID {
    char ip[16];
    int tcp_port;
    int socket_fd;  // Socket de conexão com este nó
    int safe_sent;
} NodeID;

typedef struct {
    int flaginit;
    char ip[16];
    char net[4];           // IP do nó atual
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

int handleLeave(NodeData *mynode,int fdClosed);
int cleanNeighboors(NodeData *my_node, fd_set *master_fds);
int init_node(NodeData *myNode, int cache_size);
int init_socket_listening(int port, char *ip);
void free_node_memory(NodeData *myNode);
int connect_to_node(char *ip, int port);
void add_internal_neighbor(NodeData *myNode, NodeID neighbor);

#endif // LIB_H
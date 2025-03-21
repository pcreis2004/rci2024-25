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

int handleLeaveAntigo(NodeData *mynode, int fdClosed, fd_set *master_fds) {
    // Check if the closed connection was with the external neighbor
    if (fdClosed == mynode->vzext.socket_fd) {
        printf("External neighbor disconnected. Need to connect to safeguard node and update topology.\n");
        
        // Clear the external neighbor socket
        FD_CLR(mynode->vzext.socket_fd, master_fds);
        close(mynode->vzext.socket_fd);
        
        // Check if this node is its own safeguard (special case)
        if (strcmp(mynode->vzsalv.ip, mynode->ip) == 0 && mynode->vzsalv.tcp_port == mynode->tcp_port) {
            printf("This node is its own safeguard.\n");
            
            // First check if the closed connection was also an internal neighbor and remove it
            for (int i = 0; i < mynode->numInternals; i++) {
                if (fdClosed == mynode->intr[i].socket_fd) {
                    printf("It was also an internal neighbor, removing from internal list.\n");
                    
                    // Remove this internal neighbor
                    for (int j = i; j < mynode->numInternals - 1; j++) {
                        mynode->intr[j] = mynode->intr[j + 1];
                    }
                    mynode->numInternals--;
                    break;
                }
            }
            
            // If we still have internal neighbors, choose one as the new external
            if (mynode->numInternals > 0) {
                printf("Selecting a random internal neighbor as new external neighbor.\n");
                
                // Pick a random internal neighbor as the new external
                srand(time(NULL));
                int random_index = rand() % mynode->numInternals;
                
                // Set this neighbor as the new external
                mynode->vzext.tcp_port = mynode->intr[random_index].tcp_port;
                mynode->vzext.socket_fd = mynode->intr[random_index].socket_fd;
                strcpy(mynode->vzext.ip, mynode->intr[random_index].ip);
                mynode->vzext.safe_sent = 0;
                
                // Remove this neighbor from the internal list
                for (int j = random_index; j < mynode->numInternals - 1; j++) {
                    mynode->intr[j] = mynode->intr[j + 1];
                }
                mynode->numInternals--;
                
                // Send ENTRY message to the new external neighbor
                char entry_msg[BUFFER_SIZE];
                snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", mynode->ip, mynode->tcp_port);
                
                if (send_message(mynode->vzext.socket_fd, entry_msg) < 0) {
                    printf("Failed to send ENTRY message to new external neighbor.\n");
                    return -1;
                }
                
                // Send SAFE messages to all remaining internal neighbors
                printf("Sending SAFE messages to all internal neighbors.\n");
                char safe_msg[BUFFER_SIZE];
                snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", mynode->vzext.ip, mynode->vzext.tcp_port);
                
                for (int i = 0; i < mynode->numInternals; i++) {
                    if (send_message(mynode->intr[i].socket_fd, safe_msg) < 0) {
                        printf("Failed to send SAFE message to internal neighbor %d.\n", i);
                        // Continue with other neighbors even if one fails
                    }
                }
            } else {
                printf("No internal neighbors left. This node becomes standalone.\n");
                // This node becomes its own external neighbor
                mynode->vzext.tcp_port = mynode->tcp_port;
                strcpy(mynode->vzext.ip, mynode->ip);
                mynode->vzext.socket_fd = -1; // No actual connection to self
                mynode->vzext.safe_sent = 0;
                cleanNeighboors(mynode, master_fds);
            }
        } else {
            // This node is not its own safeguard, so connect to the safeguard node
            printf("Connecting to safeguard node: %s:%d\n", mynode->vzsalv.ip, mynode->vzsalv.tcp_port);
            
            // Establish connection to safeguard node
            int new_sock = connect_to_node(mynode->vzsalv.ip, mynode->vzsalv.tcp_port);
            if (new_sock < 0) {
                printf("Failed to connect to safeguard node. Network may be partitioned.\n");
                return -1;
            }
            
            // Add the new connection to the select set
            FD_SET(new_sock, master_fds);
            
            // Update external neighbor to be the safeguard
            mynode->vzext.tcp_port = mynode->vzsalv.tcp_port;
            strcpy(mynode->vzext.ip, mynode->vzsalv.ip);
            mynode->vzext.socket_fd = new_sock;
            mynode->vzext.safe_sent = 0;
            
            // Send ENTRY message to the new external neighbor (safeguard)
            ssize_t nleft,nwritten;
                char *ptr;
                char entry_msg[BUFFER_SIZE];
                
                snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", mynode->ip, mynode->tcp_port);
                
                ptr=entry_msg;
                nleft=strlen(entry_msg);
                
                while (nleft>0)
                {
                    nwritten=write(mynode->vzext.socket_fd,ptr,nleft);
                    if(nwritten<=0)/*error*/exit(1);
                    nleft-=nwritten;
                    ptr+=nwritten;
                }
            printf("Mensagem enviada para o nó %s:%d --- > %s",mynode->vzsalv.ip,mynode->vzsalv.tcp_port,entry_msg);
            // Send SAFE messages to all internal neighbors
            printf("Sending SAFE messages to all internal neighbors.\n");
            char safe_msg[BUFFER_SIZE];
            snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", mynode->vzext.ip, mynode->vzext.tcp_port);
            
            for (int i = 0; i < mynode->numInternals; i++) {
                if (send_message(mynode->intr[i].socket_fd, safe_msg) < 0) {
                    printf("Failed to send SAFE message to internal neighbor %d.\n", i);
                    // Continue with other neighbors even if one fails
                }
            }
        }
    } else {
        // This was an internal neighbor that disconnected
        for (int i = 0; i < mynode->numInternals; i++) {
            if (fdClosed == mynode->intr[i].socket_fd) {
                printf("Internal neighbor disconnected, removing from internal list.\n");
                
                // Remove this internal neighbor
                FD_CLR(mynode->intr[i].socket_fd, master_fds);
                close(mynode->intr[i].socket_fd);
                
                // Shift array elements to fill the gap
                for (int j = i; j < mynode->numInternals - 1; j++) {
                    mynode->intr[j] = mynode->intr[j + 1];
                }
                mynode->numInternals--;
                break;
            }
        }
    }
    
    return 0;
}

// Helper function to send a message reliably
int send_message(int sockfd, const char *message) {
    ssize_t nleft, nwritten;
    const char *ptr = message;
    nleft = strlen(message);
    
    while (nleft > 0) {
        nwritten = write(sockfd, ptr, nleft);
        if (nwritten <= 0) {
            // Handle error
            perror("DEU MERDA");
            return -1;
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    
    return 0;
}


int handleLeave(NodeData *mynode,int fdClosed, fd_set *master_fds){

    if (fdClosed == mynode->vzext.socket_fd)
    {
        printf("Foi um vizinho externo, Tens que mandar um Entry para o de Salvaguarda e um Safe para todos os internos\n");

        if (strcmp(mynode->vzsalv.ip,mynode->ip)==0 && mynode->vzsalv.tcp_port == mynode->tcp_port)
        {
            printf("CALMAAAAA\nO teu vizinho de Salvaguarda eras tu, ");
            

            for (int i = 0; i < mynode->numInternals; i++)
            {
                if (fdClosed == mynode->intr[i].socket_fd)
                {
                    printf("era também um vizinho interno, remove-o da lista de internos,");
                    //remove intr

                    mynode->intr[i].tcp_port = -1;
                    mynode->intr[i].socket_fd = -1;
                    mynode->intr[i].ip[0] = '\0';
                    mynode->numInternals--;

                }
                
            }

            if (mynode->numInternals!=0)
            {
                

                printf(" vais ter que escolher um interno para se tornar o teu externo, mandar lhe entry e enviar SAfe para os outros internos\n");
                //random interno

                srand(time(NULL));
                int random_index = -1;
                while (mynode->intr[random_index].socket_fd==-1)
                {
                    random_index = rand() % mynode->numInternals;
                }
                
                mynode->vzext.tcp_port = mynode->intr[random_index].tcp_port;
                mynode->vzext.socket_fd = mynode->intr[random_index].socket_fd;
                strcpy(mynode->vzext.ip,mynode->intr[random_index].ip);
                mynode->vzext.safe_sent=mynode->intr[random_index].safe_sent;
                
                // Enviar mensagem ENTRY
                
                //int sockfd = connect_to_node(mynode->vzext.ip, mynode->vzext.tcp_port);


                ssize_t nleft,nwritten;
                char *ptr;
                char entry_msg[BUFFER_SIZE];
                
                snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", mynode->ip, mynode->tcp_port);
                
                ptr=entry_msg;
                nleft=strlen(entry_msg);
                
                while (nleft>0)
                {
                    nwritten=write(mynode->vzext.socket_fd,ptr,nleft);
                    if(nwritten<=0)/*error*/exit(1);
                    nleft-=nwritten;
                    ptr+=nwritten;
                }

                printf("Enviar Safe para todos os nós internos\n");

                for (int i = 0; i < mynode->numInternals; i++)
                {
                    if (mynode->intr[i].socket_fd != mynode->vzext.socket_fd)
                    {
                        ssize_t nleft,nwritten;
                        char *ptr;
                        char safe_msg[BUFFER_SIZE];
                        
                        snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", mynode->vzext.ip, mynode->vzext.tcp_port);
                        
                        ptr=safe_msg;
                        nleft=strlen(safe_msg);
                        
                        while (nleft>0)
                        {
                            nwritten=write(mynode->intr[i].socket_fd,ptr,nleft);
                            if(nwritten<=0)/*error*/exit(1);
                            nleft-=nwritten;
                            ptr+=nwritten;
                        }
                    }
                    
                }
                
                
                printf("Vou sair");
                return 0;
                
            }else{
                printf("Como não tens internos tu és o teu próprio externo\n");
                //EXTERNO == teu id
                cleanNeighboors(mynode,master_fds);
                return 0;
            }
            
        }
        
        //mandar entry para o de salvaguarda e safe aos internos 

        ssize_t nleft,nwritten;
        char *ptr;
        char entry_msg[BUFFER_SIZE];

        snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", mynode->ip, mynode->tcp_port);

        ptr=entry_msg;
        nleft=strlen(entry_msg);
        
        while (nleft>0)
        {
            nwritten=write(mynode->vzext.socket_fd,ptr,nleft);
            if(nwritten<=0)/*error*/exit(1);
            nleft-=nwritten;
            ptr+=nwritten;
        }

        printf("Enviar Safe para todos os nós internos\n");
        for (int i = 0; i < mynode->numInternals; i++)
        {
            if (mynode->intr[i].socket_fd != mynode->vzext.socket_fd)
            {
                ssize_t nleft,nwritten;
                char *ptr;
                char safe_msg[BUFFER_SIZE];
                
                snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", mynode->vzext.ip, mynode->vzext.tcp_port);
                
                ptr=safe_msg;
                nleft=strlen(safe_msg);
                
                while (nleft>0)
                {
                    nwritten=write(mynode->intr[i].socket_fd,ptr,nleft);
                    if(nwritten<=0)/*error*/exit(1);
                    nleft-=nwritten;
                    ptr+=nwritten;
                }
            }
            
        }

    }else{

        for (int i = 0; i < mynode->numInternals; i++)
        {
            if (fdClosed == mynode->intr[i].socket_fd)
            {
                printf("Foi um vizinho interno, remove-o só da lista de internos\n");

                mynode->intr[i].tcp_port = -1;
                mynode->intr[i].socket_fd = -1;
                mynode->intr[i].ip[0] = '\0';
                mynode->numInternals--;
            }
            
        }
    }
    


    return 0;
}


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

    for (int i = 0; i < my_node->numInternals; i++) {
        if (my_node->intr[i].socket_fd >= 0) {
            FD_CLR(my_node->intr[i].socket_fd, master_fds);
            my_node->intr[i].tcp_port = -1;
            my_node->intr[i].socket_fd = -1;
            my_node->intr[i].ip[0] = '\0';
        }
    }

    my_node->numInternals = 0;

    // Recalcular max_fd
    int new_max_fd = STDIN_FILENO;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, master_fds)) {
            new_max_fd = i;

        }
    }
    // printf("O novo max fd é %d e o fd da socket de listening é %d",new_max_fd,my_node->socket_listening);
    return new_max_fd;
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

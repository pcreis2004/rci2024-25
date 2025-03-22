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

#include "commands.h"
#include "lib.h"

/* Função: handle_command
   Interpreta e executa comandos inseridos pelo utilizador, incluindo entrada na rede,
   criação de objetos, visualização de topologia, entre outros.
   Parâmetros:
    - command: string contendo o comando inserido.
    - myNode: ponteiro para a estrutura de dados do nó atual.
    - ip: IP do registrador ou do nó atual (usado para comandos como 'join').
    - port: porto associado ao IP acima.
   Retorno:
    - Valor inteiro correspondente ao descritor de socket em alguns comandos,
      ou 0 caso nenhum descritor seja criado.
*/

int handle_command(char *command, NodeData *myNode, char *ip, int port) {
    printf("Comando recebido: %s\n", command);

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
        // free_node_memory(myNode);
        exit(0);
    } 
    else if (strcmp(cmd, "direct") == 0 && args >= 3) {
        if (strcmp(arg1, "join") == 0 || strcmp(arg1, "j") == 0) {
            int port;
            if (sscanf(arg2, "%d", &port) == 1) {
                printf("Uso correto: direct join conIP conTCP\n");
            } else {
                printf("Conectando ao nó %s:%s...\n", arg1, arg2);
                djoin(myNode, arg1, atoi(arg2),myNode->cacheSize);
                return 1;
            }
        }
    }
    else if ((strcmp(cmd, "dj") == 0 || strcmp(cmd, "direct_join") == 0) && args >= 3) {
        int fd = djoin(myNode, arg1, atoi(arg2),myNode->cacheSize);
        return fd;
    }
    else if (strcmp(cmd, "show") == 0 && args >= 2) {
        if (strcmp(arg1, "topology") == 0 || strcmp(arg1, "st") == 0) {
            show_topology(myNode);
        }
    }
    else if (strcmp(cmd,"j")==0)
    {
        int fd = join(arg1,ip,port,myNode,myNode->cacheSize);
        strcpy(myNode->net,arg1);
        return fd;
    }
    
    else if (strcmp(cmd, "st") == 0) {
        show_topology(myNode);
    }else if (strcmp(cmd,"l")==0)
    {
        int fd = leave(myNode,ip,port);
        return fd;
    }
    


    else {
        printf("Comando desconhecido: %s\n", command);
    }
    return 0;
}

int leave(NodeData *myNode, char *serverIp,int serverPort){
    socklen_t addrlen;
    struct sockaddr addr;
    struct addrinfo hints, *res;
    int fd, errcode;
    ssize_t n;
    char buffer[BUFFER_SIZE];

    if(strcmp(myNode->net,"xxx")!=0){
    fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (fd == -1) /*error*/ exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    
    sprintf(buffer, "%d", serverPort);

    errcode = getaddrinfo(serverIp, buffer, &hints, &res);
    if (errcode != 0) return -1;


    snprintf(buffer, sizeof(buffer), "UNREG %s %s %d\n", myNode->net, myNode->ip, myNode->tcp_port);
    printf("\t\t\t%s\n",buffer);


    n = sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) /*error*/ exit(1);

    n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
    if (n == -1) return -1;

    buffer[n]='\0';
    //IMPRIMIR AQUI PARA DEBUG
   
    close(fd);
    }
    close(myNode->vzext.socket_fd);
    printf("socket %d fechada\n",myNode->vzext.socket_fd);

    close(myNode->vzsalv.socket_fd);
    printf("socket %d fechada\n",myNode->vzsalv.socket_fd);

    for (int i = 0; i < myNode->numInternals; i++)
    {   
        if (myNode->intr[i].socket_fd != myNode->vzext.socket_fd)
        {
            close(myNode->intr[i].socket_fd);
            printf("socket %d fechada\n",myNode->intr[i].socket_fd);    
        }
        
    }
    printf("Nó saiu da rede\n");
    return -2;
}



/* Função: join
   Liga o nó atual a uma rede lógica através do registrador,
   registando o nó e tentando ligar-se a um nó aleatório existente, se houver.
   Parâmetros:
    - net: nome da rede lógica.
    - ip: IP do registrador.
    - port: porto UDP do registrador.
    - myNode: ponteiro para a estrutura de dados do nó a ser ligado.
    - cache_size: tamanho da cache do nó.
   Retorno:
    - Descritor de socket criado pela ligação ao nó externo,
      ou 0 caso o nó se registe diretamente sem ligação externa.
*/
int join(char *net, char *ip, int port,NodeData *myNode,int cache_size) {
    // conectar ao servidor
    struct addrinfo hints, *res;
    socklen_t addrlen;
    struct sockaddr addr;
    int fd, errcode;
    ssize_t n;
    
    //inicializar nó antes de o connectar ao servidor
   
    if (myNode->flaginit==0)
    {
        int listening = init_node(myNode,cache_size);
        if(listening==-1) exit(1);
    }
    


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
    printf("Nó connectado à net %s\n", net);
    
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

        n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
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

    myNode->flaginit=1;
    int filed = djoin(myNode,selected_ip,selected_port,myNode->cacheSize);
    if (filed==-1)
    {
        myNode->flaginit=0;
        close(fd);
        close(filed);
        freeaddrinfo(res);
        return 0;
    }
    myNode->flaginit=0;

    // printf("o file descas é este lol [%d]\n",filed);
    char msgServer[BUFFER_SIZE];
    snprintf(msgServer, sizeof(msgServer), "REG %s %s %d\n", net,myNode->ip,myNode->tcp_port);
    n = sendto(fd, msgServer, strlen(msgServer), 0, res->ai_addr, res->ai_addrlen);
    // printf("Nó registado no servidor\n");
    if (n == -1) /*error*/ exit(1);
    printf("Nó registado na net %s\n\n", net);

    n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
    if (n == -1) return -1;

    // printf("Estou aqui seu boi do caralho\n");
    buffer[n]='\0';
    // if (strcmp(buffer,"OKREG")==0)
    // {
    //     printf("»»%s\n",buffer);
    // }else{
    //     printf("Fora daquele nó estranho %s\n",buffer);
    // }
    // printf("");
    // Store the selected IP and port in variables
    // (You might want to modify the function parameters to pass these back)
    // Example: strcpy(output_ip, selected_ip); *output_port = selected_port;
    freeaddrinfo(res);

    close(fd);
    return filed;
}

/* Função: djoin
   Conecta diretamente a um nó especificado, atualizando os dados de vizinhança
   e enviando a mensagem de entrada ENTRY.
   Parâmetros:
    - myNode: ponteiro para a estrutura do nó local.
    - connectIP: IP do nó ao qual se deseja conectar.
    - connectTCP: porto TCP do nó de destino.
    - cache_size: tamanho da cache do nó local.
   Retorno:
    - Descritor de socket da ligação estabelecida, ou -1 em caso de falha.
*/
int djoin(NodeData *myNode, char *connectIP, int connectTCP, int cache_size) {
    printf("O seu connectIP é %s\n E o seu flaginit é %d\n", connectIP,myNode->flaginit);
    ssize_t nleft,nwritten;
    char *ptr;
    if (strcmp(connectIP, "0.0.0.0") == 0 && myNode->flaginit == 0) {

        printf("Criando rede com nó raiz (%s:%d)\n", myNode->ip, myNode->tcp_port);

        int fd = init_node(myNode, cache_size);
        
        // myNode->flaginit=1;
        
        // printf("Agora a flag init é %d\n",myNode->flaginit);
        return fd; //Devolve o nó criado
    } else if(myNode->flaginit==1){
        // Conectar ao nó externo especificado
        int sockfd = connect_to_node(connectIP, connectTCP);
        if (sockfd < 0) {
            printf("Falha ao conectar a %s:%d\n", connectIP, connectTCP);
            return -1;
        }
        
        // printf("Conectado a %s:%d (socket: %d)\n", connectIP, connectTCP, sockfd);
        
        // Atualizar vizinho externo
        strncpy(myNode->vzext.ip, connectIP, sizeof(myNode->vzext.ip) - 1);
        myNode->vzext.ip[sizeof(myNode->vzext.ip) - 1] = '\0';
        myNode->vzext.tcp_port = connectTCP;
        myNode->vzext.socket_fd = sockfd;
        myNode->vzext.safe_sent = 0;
        
        // Enviar mensagem ENTRY
        char entry_msg[BUFFER_SIZE];
        snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", myNode->ip, myNode->tcp_port);
        ptr=entry_msg;
        nleft=strlen(entry_msg);
        
        while (nleft>0)
        {
            nwritten=write(sockfd,ptr,nleft);
            // printf("\t\t\tmensagem enviada %s\n",ptr);
            if(nwritten<=0)/*error*/exit(1);
            nleft-=nwritten;
            ptr+=nwritten;
        }
        
        printf("Ligado na Socket:%d ao nó %s %d\n\n",sockfd,connectIP,connectTCP);
        // printf("\t\tAguardando msg de SAFE . . . \n");
        
        // A resposta SAFE será tratada no loop principal que lê as mensagens recebidas
        return sockfd;
    }else
    {
        printf("Comando inválido ou nó ainda não inicializado, dê djoin ao ip 0.0.0.0\n");
        return 0;
    }
    
}

/* Função: show_topology
   Exibe no terminal a topologia atual do nó, incluindo vizinho externo,
   vizinho de salvaguarda e a lista de vizinhos internos.
   Parâmetros:
    - myNode: ponteiro para a estrutura de dados do nó cuja topologia será apresentada.
   Retorno:
    - Nenhum.
*/
void show_topology(NodeData *myNode) {
    if (myNode->flaginit==1)
    {
    printf("\n=== Topologia Atual ===\n");
    printf("ID: %s:%d\n", myNode->ip, myNode->tcp_port);
    
    printf("Vizinho Externo: %s:%d\n", 
        strlen(myNode->vzext.ip) > 0 ? myNode->vzext.ip : "Não definido", 
        myNode->vzext.tcp_port);

    printf("Vizinho de Salvaguarda: %s:%d\n", 
           strlen(myNode->vzsalv.ip) > 0 ? myNode->vzsalv.ip : "Não definido", 
           myNode->vzsalv.tcp_port);
    
    printf("Vizinhos Internos (%d):\n", myNode->numInternalsReal);
    if (myNode->numInternalsReal!=0)
    {
        
        for (int i = 0; i < myNode->numInternals; i++) {
            if (myNode->intr[i].socket_fd != -1)
            {
                printf("  %d. %s:%d\n", i+1, myNode->intr[i].ip, myNode->intr[i].tcp_port);
            }
            
        }
    }
    printf("=====================\n\n");
    }else{
        printf("=====================\n\n");
        printf("Nó ainda não inicializado\n\n");
        printf("=====================\n\n");
    
    }
    
    
}

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
    
    // Verificar o tipo de comando
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
        printf("  cache - Visualização da cache\n");
        printf("  leave (l) - Saída da rede\n");
        printf("  exit (x) - Fecha a aplicação\n");
    } 
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "x") == 0) {
        printf("Saindo...\n");
        leave(myNode, ip, port);
        free_node_memory(myNode);
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
        if (fd!=0)
        {
            strcpy(myNode->net,arg1);
            if (fd==-1)
            {
                fd=0;
            }
            
        }
    
        
        return fd;
    }
    
    else if (strcmp(cmd, "st") == 0) {
        show_topology(myNode);
    }else if (strcmp(cmd,"l")==0)
    {
		if (myNode->flaginit==0) {
			printf("Não pode dar leave sem ter o nó\n");
			return 0;
		
		}
        int fd = leave(myNode,ip,port);
        return fd;
    }else if (strcmp(cmd,"c")== 0)
    {
        
        int fd = create(myNode,arg1);
        return fd;
        printf("CREATE\n");

    }else if (strcmp(cmd,"r")== 0)
    {   
        
        printf("RETRIEVE\n");
        int fd = retrieve(myNode, arg1);
        return fd;
        
    }else if (strcmp(cmd,"sn")==0)
    {
        
        show_names(myNode);
        printf("Show names\n");
    }else if (strcmp(cmd,"si")== 0)
    {
        
        printf("SHOW INTEREST TABLE\n");
    }else if (strcmp(cmd,"cache")==0)
    {
        
        printf("CACHE\n");
        printCache(myNode);
    }else if (strcmp(cmd,"dl")==0)
    {
        
        int fd = delete(myNode,arg1);
        return fd;
        printf("DELETE\n");
    
    }else if (strcmp(cmd,"si")==0)
    {
        printf("SHOW INTEREST TABLE\n");
        table_interest(myNode,arg1);
        
    }else {
        printf("Comando desconhecido: %s\n", command);
    }
    return 0;
}


/* Função: free_node_memory
   Liberta a memória alocada para os dados do nó, incluindo vizinhos e objetos.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
   Retorno:
    - Nenhum.
*/
void free_node_memory(NodeData *myNode) {
    free(myNode->intr);
    free(myNode->objects);
    free(myNode->cache);
    
}



/* Função: retrieve
   Envia uma mensagem de interesse para o nó externo e vizinhos internos,
   aguardando resposta sobre a existência do objeto.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
    - name: nome do objeto a ser recuperado.
   Retorno:
    - 0 em caso de sucesso.
*/
int retrieve(NodeData *myNode, char *name) {
    printf("RETRIEVE\n");

    myNode->objectfound = 0;
    myNode->flagoriginretrieve=1; //Identifica nó origem

    char msg[BUFFER_SIZE];

    sprintf(msg,"INTEREST %s\n",name);

    // Enviar mensagem de interesse para o vizinho externo
    if (myNode->vzext.socket_fd == -1) {
        printf("Nó sem vizinho externo\n");
    }else{
        writeFull(myNode->vzext.socket_fd, msg);
        myNode->vzext.espera=1;
        myNode->vzext.fechado=0;
        myNode->nodes_em_espera++;
    }
    
    // Enviar mensagem de interesse para os vizinhos internos
    for (int i = 0; i < 10; i++)
    {
        if (myNode->intr[i].socket_fd != -1 && myNode->intr[i].socket_fd != myNode->vzext.socket_fd)
        {
            writeFull(myNode->intr[i].socket_fd, msg);
            myNode->intr[i].espera=1;
            myNode->intr[i].fechado=0;
            myNode->nodes_em_espera++;
        }
        
    }
    // Verificar se há vizinhos para enviar a mensagem
    if (myNode->nodes_em_espera==0)
    {
        printf("Nenhum vizinho para enviar a mensagem\n");
        
    }
    
    myNode->interface_retrieve=1;


    table_interest(myNode,name);    
    
    return 0;
}


/* Função: table_interest
   Mostra a tabela de interesses do nó, incluindo vizinho externo e internos.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
    - name: nome do objeto de interesse.
   Retorno:
    - Nenhum.
*/
void table_interest(NodeData *myNode, char *name) {

    printf("\n\n");
    printf("TABELA DE INTERESSES\n");
    
    if (myNode->nodes_em_espera!=0){

        printf("%s ",name);
    
        printf("%d:",myNode->vzext.socket_fd);
        if (myNode->vzext.resposta == 1)
        {
            printf("resposta ");
            
        }else if (myNode->vzext.espera == 1)
        {
            printf("espera ");
        
        }else if (myNode->vzext.fechado == 1)
        {
            printf("fechado ");
          
        }
        
        for (int i = 0; i < 10; i++)
        {
            if (myNode->intr[i].socket_fd != -1 && myNode->intr[i].socket_fd != myNode->vzext.socket_fd)
            {
                    printf("%d:",myNode->intr[i].socket_fd);
                
                if (myNode->intr[i].resposta == 1)
                {
                    printf("resposta ");
                    
                }else if (myNode->intr[i].espera == 1)
                {
                    printf("espera ");
                
                }else if (myNode->intr[i].fechado == 1)
                {
                    printf("fechado ");
                  
                }
            }
        }
        
    }
    printf("\n\n");
    
    

}



/*Função handle_interest
  Processa a mensagem de interesse recebida, verificando se o objeto está na cache
  ou se deve ser enviado para os vizinhos internos e externo.
  Parâmetros:
   - myNode: ponteiro para a estrutura NodeData do nó atual.
   - name: nome do objeto de interesse.
   - fd: descritor de socket do nó que enviou a mensagem.
  Retorno:
   - 1 se o objeto foi encontrado, 0 caso contrário.

*/
int handle_interest(NodeData *myNode, char *name, int fd){

    myNode->objectfound = 0;

  

    char buffer[BUFFER_SIZE];
    // 1. Verificar que nó enviou a mensagem
    if (myNode->interface_retrieve==0)
    {
        myNode->interface_retrieve=1;
        if (myNode->vzext.socket_fd == fd) {
            myNode->vzext.resposta=1;
        }else{
            for (int i = 0; i < myNode->numInternals; i++)
            {
                if (myNode->intr[i].socket_fd == fd)
                {
                    myNode->intr[i].resposta=1;
                }
                
            }
        
        }
        
        if (myNode->vzext.resposta != 1)
        {
            myNode->vzext.resposta=0;
        }

        
        for (int i = 0; i < 10; i++)
        {
            if (myNode->intr[i].resposta != 1)
            {
                myNode->intr[i].resposta=0;
            }
        }
        

        // 2. Procurar o objeto no vetor `objects`
        for (int i = 0; i < 10 * 100; i += 100) {
            if (myNode->objects[i] != '\0') {
                if (strncmp(&myNode->objects[i], name, 100) == 0) {
                    printf("Objeto '%s' encontrado no índice %d.\n", name, i / 100);
                    sprintf(buffer,"OBJECT %s\n",name);

                    table_interest(myNode,name);
                    send_response(myNode,buffer);
                    
                    return 1; // Objeto encontrado
                }
            }
        }
        
    
        
        
        snprintf(buffer, sizeof(buffer), "INTEREST %s\n", name);
        // 3. Enviar mensagem de interesse para o vizinho externo
        if (myNode->vzext.socket_fd != -1 && myNode->vzext.socket_fd != fd && myNode->vzext.resposta == 0) {
            writeFull(myNode->vzext.socket_fd, buffer);
            myNode->vzext.espera=1;
            myNode->vzext.fechado=0;
            myNode->nodes_em_espera++;
        }
        // 4. Enviar mensagem de interesse para os vizinhos internos
        for (int i = 0; i < 10; i++) {
            if (myNode->intr[i].socket_fd != -1 && myNode->intr[i].socket_fd != myNode->vzext.socket_fd && myNode->intr[i].resposta == 0 && myNode->intr[i].socket_fd != fd) {
                writeFull(myNode->intr[i].socket_fd, buffer);
                myNode->intr[i].espera=1;
                myNode->intr[i].fechado=0;
                myNode->nodes_em_espera++;
            }
        }
        table_interest(myNode,name);
        
        if (myNode->nodes_em_espera==0)
        {
            if (myNode->flagoriginretrieve==0)
            {
                
                snprintf(buffer, sizeof(buffer), "NOOBJECT %s\n", name);
                send_response(myNode,buffer);
            }
            
            return 0;
        }
        
        return 0;
    }
    

    return 0;
}


/*Função handle_object
  Processa a mensagem de objeto recebida, verificando se o objeto foi encontrado
  e atualizando a tabela de interesses.
  Parâmetros:
   - myNode: ponteiro para a estrutura NodeData do nó atual.
   - name: nome do objeto encontrado.
   - fd: descritor de socket do nó que enviou a mensagem.
  Retorno:
   - 1 se o objeto foi encontrado, 0 caso contrário.

*/
int handle_object(NodeData *myNode, char *name, int fd){

    //Ver de onde veio o objeto e colocar esse nó como fechado

    if (myNode->vzext.socket_fd == fd && myNode->vzext.resposta == 0) {
        myNode->vzext.fechado=1;
        myNode->vzext.espera=0;
        myNode->objectfound=1;
        myNode->nodes_em_espera--;
        table_interest(myNode,name);
    }else{
        for (int i = 0; i < myNode->numInternals; i++)
        {
            if (myNode->intr[i].socket_fd == fd && myNode->intr[i].resposta == 0)
            {
                myNode->intr[i].fechado=1;
                myNode->intr[i].espera=0;
                myNode->objectfound=1;
                myNode->nodes_em_espera--;
                table_interest(myNode,name);
            }
            
        }
    
    }
    // Guardar o nome na cache
    int cache = add_to_cache(myNode,name);
    if (cache == -1)
    {
        printf("Cache cheia\n");
        
    }
    //ver se ainda tem nós em espera
    if (myNode->nodes_em_espera==0)
    {
            
            
            if (myNode->objectfound == 0)
            {
                if (myNode->flagoriginretrieve==1)
                {
                    meterTudoAzero(myNode);
                    return 3;
                }
                
                char buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer), "NOOBJECT %s\n", name);
        
        
                
                send_response(myNode,buffer);
                myNode->interface_retrieve=0;
                return 0;
            }else if (myNode->objectfound == 1)
            {
                if (myNode->flagoriginretrieve==1)
                {
                    meterTudoAzero(myNode);
                    return 2;
                }
                
                char buffer[BUFFER_SIZE];
                
                snprintf(buffer, sizeof(buffer), "OBJECT %s\n", name);
    
                send_response(myNode,buffer);
                myNode->interface_retrieve=0;
                return 0;
                
            }
            
            
        }



    return 0;
}


/*Função handle_noobject
  Processa a mensagem de objeto não encontrado recebida, verificando se o objeto foi encontrado
  e atualizando a tabela de interesses.
  Parâmetros:
   - myNode: ponteiro para a estrutura NodeData do nó atual.
   - name: nome do objeto não encontrado.
   - fd: descritor de socket do nó que enviou a mensagem.
  Retorno:
   - 1 se o objeto foi encontrado, 0 caso contrário.

*/
int handle_noobject(NodeData *myNode, char *name, int fd){

    //Ver de onde veio o objeto e colocar esse nó como fechado
 
    

    if (myNode->vzext.socket_fd == fd && myNode->vzext.resposta == 0) {
        myNode->vzext.fechado=1;
        myNode->vzext.espera=0;
        myNode->nodes_em_espera--;
        table_interest(myNode,name);
    }else{
        for (int i = 0; i < myNode->numInternals; i++)
        {
            if (myNode->intr[i].socket_fd == fd && myNode->intr[i].resposta == 0)
            {
                myNode->intr[i].fechado=1;
                myNode->intr[i].espera=0;
                myNode->nodes_em_espera--;
                table_interest(myNode,name);
            }
            
        }
    
    }

    // Guardar o nome na cache
    if (myNode->nodes_em_espera==0)
    {
        
        
        if (myNode->objectfound == 0)
        {
            if (myNode->flagoriginretrieve==1)
            {
                meterTudoAzero(myNode);
                return 3;
            }
            
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "NOOBJECT %s\n", name);
    
    
            
            send_response(myNode,buffer);
            myNode->interface_retrieve=0;
            return 0;
        }else if (myNode->objectfound == 1)
        {
            if (myNode->flagoriginretrieve==1)
            {
                meterTudoAzero(myNode);
                return 2;
            }
            
            char buffer[BUFFER_SIZE];
            
            snprintf(buffer, sizeof(buffer), "OBJECT %s\n", name);

            send_response(myNode,buffer);
            myNode->interface_retrieve=0;
            return 0;
            
        }
        
        
    }

    

    return 0;
}


/*Função send_response
  Envia a resposta para o nó que fez a requisição, seja ele externo ou interno.
  Parâmetros:
   - myNode: ponteiro para a estrutura NodeData do nó atual.
   - msg: mensagem a ser enviada.
  Retorno:
   - 0 em caso de sucesso, 1 se não houver resposta.
*/
int send_response(NodeData *myNode, char *msg){

    //Encontrar o nó de resposta e enviar mensagem

    if (myNode->vzext.resposta == 1) {
        writeFull(myNode->vzext.socket_fd, msg);
        meterTudoAzero(myNode);
        return 0;
        
    }

    for (int i = 0; i < 10; i++)
    {
        if (myNode->intr[i].resposta == 1)
        {
            writeFull(myNode->intr[i].socket_fd, msg);
            meterTudoAzero(myNode);
            return 0;
        }
        
    }
    

    return 1;
}


/* Função: show_topology
   Exibe a topologia do nó, incluindo vizinho externo e internos.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
   Retorno:
    - Nenhum.
*/
void show_names(NodeData *myNode) {
    printf("Objetos armazenados:\n");

    int found = 0;
    for (int i = 0; i < 10 * 100; i += 100) {
        if (myNode->objects[i] != '\0') {  // bloco ocupado
            printf(" - %s\n", &myNode->objects[i]);
            found = 1;
        }
    }

    if (!found) {
        printf(" (nenhum objeto armazenado)\n");
    }
}


/* Função: create
   Cria um novo objeto no vetor de objetos do nó, se houver espaço disponível.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
    - name: nome do objeto a ser criado.
   Retorno:
    - 0 em caso de sucesso, -1 se o vetor de objetos estiver cheio.
*/
int create(NodeData *myNode, char *name) {
    for (int i = 0; i < 10 * 100; i += 100) {
        if (myNode->objects[i] == '\0') {  // bloco livre
            strncpy(&myNode->objects[i], name, 100);
            myNode->objects[i + 99] = '\0'; // garantir terminação
            myNode->numObjects++;
            printf("Objeto criado com sucesso: %s\n", &myNode->objects[i]);
            return 0;
        }
    }

    printf("Erro: vetor de objetos cheio.\n");
    return -1;
}


/* Função: delete
   Remove um objeto do vetor de objetos do nó, se encontrado.
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
    - name: nome do objeto a ser removido.
   Retorno:
    - 0 em caso de sucesso, -1 se o objeto não for encontrado.
*/
int delete(NodeData *myNode, char *name) {
    for (int i = 0; i < 10 * 100; i += 100) {
        if (strncmp(&myNode->objects[i], name, 100) == 0) {  // Encontrou o objeto
            myNode->objects[i] = '\0'; // Marcar como vazio
            myNode->numObjects--;
            printf("Objeto removido com sucesso: %s\n", name);
            return 0;
        }
    }

    printf("Erro: Objeto '%s' não encontrado.\n", name);
    return 0;
}


/* Função: leave
   Responsável por remover o nó da rede NDN, enviando uma mensagem UNREG ao servidor de registo
   e fechando todos os sockets abertos (externo, salvaguarda e internos).
   Parâmetros:
    - myNode: ponteiro para a estrutura NodeData do nó atual.
    - serverIp: endereço IP do servidor de registo.
    - serverPort: porto do servidor de registo.
   Retorno:
    - -2 indicando que o nó saiu da rede com sucesso.
*/
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


    // Configurar timeout para recvfrom (4 segundos)
    struct timeval timeout;
    timeout.tv_sec = 4;  // Segundos
    timeout.tv_usec = 0; // Microssegundos

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Erro ao configurar timeout");
        close(fd);
        return -1;
    }

    n = sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) /*error*/ exit(1);

    n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
    if (n == -1) return -1;

    buffer[n]='\0';
   
    close(fd);
    }

    if(myNode->flaginit==1){
		close(myNode->vzext.socket_fd);

		close(myNode->vzsalv.socket_fd);

    for (int i = 0; i < 10; i++)
    {   
		if (myNode->intr[i].socket_fd != myNode->vzext.socket_fd)
			{
				close(myNode->intr[i].socket_fd);
			}
        
		}
	}
    printf("Nó saiu da rede\n");
    strcpy(myNode->net, "xxx");

    return -2;
}
/* Função: handleLeave
   Processa o evento de saída de um vizinho (nó externo ou interno), atualizando a estrutura
   do nó conforme necessário (promovendo internos ou ativando o nó de salvaguarda).
   Parâmetros:
    - mynode: ponteiro para a estrutura NodeData do nó atual.
    - fdClosed: descritor do socket que foi fechado.
    - master_fds: conjunto de descritores gerido pelo select.
    - max_fd: ponteiro para o maior descritor de ficheiro atual.
   Retorno:
    - 0 em caso de processamento normal.
    - Resultado de fallbackToSafeguard, se aplicável.
*/
int handleLeave(NodeData *mynode, int fdClosed, fd_set *master_fds, int *max_fd) {
    printf("A procesar o leave\n");
    // Verificar se foi o externo
    if (fdClosed == mynode->vzext.socket_fd) {
        
        // Verificar se somos o nó de salvaguarda de nós próprios
        if (strcmp(mynode->vzsalv.ip, mynode->ip) == 0 && mynode->vzsalv.tcp_port == mynode->tcp_port) {
            
            // Verificar se o nó externo que saiu também era interno
            removeInternal(mynode, fdClosed);

            // Verificar se ainda tens internos
            if (mynode->numInternalsReal > 0) {
                promoteRandomInternalToExternal(mynode, master_fds, max_fd);
                sendSafeToAllInternals(mynode);
            } else {
                cleanNeighboors(mynode, master_fds);
                return 0;
            }
        } else {
            //Se não for um nó de salvaguarda de nós próprios enviar Entry para o de salvaguarda e Safe para todos os internos
            return fallbackToSafeguard(mynode, master_fds, max_fd);
        }
    } else {
        //Foi um vizinho interno
        removeInternal(mynode, fdClosed);
    }

    return 0;
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
    

    if (strcmp(myNode->net,"xxx")==0)
    {
        
        fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
        if (fd == -1) /*error*/ exit(1);
        
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET; // IPv4
        hints.ai_socktype = SOCK_DGRAM; // UDP socket
        
        char buffer[1024]; 
        sprintf(buffer, "%d", port);
        
        errcode = getaddrinfo(ip, buffer, &hints, &res);
        if (errcode != 0) return -1;
        
        char mensagem[20];
        snprintf(mensagem, sizeof(mensagem), "NODES %s\n", net);
        
        // Configurar timeout para recvfrom (4 segundos)
        struct timeval timeout;
        timeout.tv_sec = 4;  // Segundos
        timeout.tv_usec = 0; // Microssegundos

        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("Erro ao configurar timeout");
            close(fd);
            return -1;
        }

        n = sendto(fd, mensagem, strlen(mensagem), 0, res->ai_addr, res->ai_addrlen);
        if (n == -1) /*error*/ exit(1);
        
        addrlen = sizeof(addr);
        n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
        if (n == -1) return -1;
        
        buffer[n] = '\0';
        printf("Nó connectado à net %s\n", net);
        
        
        char *line = strtok(buffer, "\n");
        
        // Saltar a primeira linha
        if (line != NULL) {
            line = strtok(NULL, "\n");
        }
        
        // Contar as linhas
        int server_count = 0;
        char *servers[100]; // Assumindo no máximo 100 linhas
        
        while (line != NULL && server_count < 100) {
            servers[server_count++] = line;
            line = strtok(NULL, "\n");
        }
        // Se não houver servidores disponíveis, registrar o nó
        if (server_count == 0) {
            char msgServer[BUFFER_SIZE];
            snprintf(msgServer, sizeof(msgServer), "REG %s %s %d\n", net,myNode->ip,myNode->tcp_port);
            
            n = sendto(fd, msgServer, strlen(msgServer), 0, res->ai_addr, res->ai_addrlen);
            
            if(n == -1) /*error*/ exit(1);
    
            n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
            
            close(fd);
            return -1;
        }
        
        // Escolher um servidor aleatório
        srand(time(NULL));
        int random_index = rand() % server_count;
        char selected_server[128];
        strcpy(selected_server, servers[random_index]);
        
        // Extrair IP e porta do servidor selecionado
        char selected_ip[64];
        int selected_port;
        sscanf(selected_server, "%s %d", selected_ip, &selected_port);
        
    
        myNode->flaginit=1;
        int filed = djoin(myNode,selected_ip,selected_port,myNode->cacheSize);
        if (filed==-1)
        {
            printf("upssss");
            strcpy(myNode->net,"xxx");
            myNode->flaginit=0;
            close(fd);
            close(filed);
            freeaddrinfo(res);
            return 0;
        }
        myNode->flaginit=0;
    
        char msgServer[BUFFER_SIZE];
        snprintf(msgServer, sizeof(msgServer), "REG %s %s %d\n", net,myNode->ip,myNode->tcp_port);
        n = sendto(fd, msgServer, strlen(msgServer), 0, res->ai_addr, res->ai_addrlen);
        printf("Nó registado no servidor\n");
        if (n == -1) /*error*/ exit(1);
        printf("Nó registado na net %s\n\n", net);
    
        n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, &addr, &addrlen);
        if (n == -1) return -1;
    
        buffer[n]='\0';
        
        freeaddrinfo(res);
    
        close(fd);
        return filed;
    }
    printf("Já está numa rede\n");
    return 0;
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
    
    ssize_t nleft,nwritten;
    char *ptr;
    if (strcmp(connectIP, "0.0.0.0") == 0 && myNode->flaginit == 0) {

        printf("Criando rede com nó raiz (%s:%d)\n", myNode->ip, myNode->tcp_port);

        int fd = init_node(myNode, cache_size);
        
        
        return fd; //Devolve o nó criado
    }else if(strcmp(connectIP, myNode->ip) == 0 && connectTCP == myNode->tcp_port){
        printf("Não é possível connectar ao próprio nó\n");
        return -1;


    }else if(myNode->vzext.socket_fd>0){
        printf("Já está conectado a um nó externo, ou seja já estás numa rede de nós\n");
        return -1;


    }else if(myNode->flaginit==1){
        // Conectar ao nó externo especificado
        int sockfd = connect_to_node(connectIP, connectTCP);
        if (sockfd < 0) {
            printf("Falha ao conectar a %s:%d\n", connectIP, connectTCP);
            return -1;
        }
        
        
        // Atualizar vizinho externo
        strncpy(myNode->vzext.ip, connectIP, sizeof(myNode->vzext.ip) - 1);
        myNode->vzext.ip[sizeof(myNode->vzext.ip) - 1] = '\0';
        myNode->vzext.tcp_port = connectTCP;
        myNode->vzext.socket_fd = sockfd;
        myNode->vzext.safe_sent = 0;
        myNode->vzext.resposta=0;
        // Enviar mensagem ENTRY
        char entry_msg[BUFFER_SIZE];
        snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", myNode->ip, myNode->tcp_port);
        ptr=entry_msg;
        nleft=strlen(entry_msg);
        
        while (nleft>0)
        {
            nwritten=write(sockfd,ptr,nleft);
            if(nwritten<=0)/*error*/exit(1);
            nleft-=nwritten;
            ptr+=nwritten;
        }
        
        printf("Ligado na Socket:%d ao nó %s %d\n\n",sockfd,connectIP,connectTCP);
        
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
    if (strcmp(myNode->net,"xxx")!=0)
    {
        printf("NET --> %s\n",myNode->net);
    }
    printf("ID: %s:%d\n", myNode->ip, myNode->tcp_port);
    
    printf("Vizinho Externo: %s:%d\n", 
        strlen(myNode->vzext.ip) > 0 ? myNode->vzext.ip : "Não definido", 
        myNode->vzext.tcp_port);

    printf("Vizinho de Salvaguarda: %s:%d\n", 
           strlen(myNode->vzsalv.ip) > 0 ? myNode->vzsalv.ip : "Não definido", 
           myNode->vzsalv.tcp_port);
    
    printf("Vizinhos Internos (%d):\n", myNode->numInternalsReal);
        int nInterno = 1;
        for (int i = 0; i < 10; i++) {
            if (myNode->intr[i].socket_fd != -1)
            {
                printf("  %d. %s:%d\n", nInterno, myNode->intr[i].ip, myNode->intr[i].tcp_port);
                nInterno++;
            }
            
        }
        
    
    printf("=====================\n\n");
    }else{
        printf("=====================\n\n");
        printf("Nó ainda não inicializado\n\n");
        printf("=====================\n\n");
    
    }
    
    
}

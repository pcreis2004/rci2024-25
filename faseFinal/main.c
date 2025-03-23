/******************************************************************************
 * (c) 2024-2025 Projeto AED - Grupo 94
 * Last modified: 25 / 10 / 2024 - (17:30)
 *
 * NAME
 *   main.c
 *
 * DESCRIPTION
 *   Projeto: Redes de Dados Identificados por Nome (NDN)
 *   Funções principais do projeto.
 *
 *   Este ficheiro contém a função principal que:
 *     - Lê os argumentos de arranque do programa (cache, IP, TCP, [regIP], [regUDP])
 *     - Inicializa a estrutura do nó e a sua configuração
 *     - Cria o socket de escuta e gere as conexões com outros nós
 *     - Multiplexa a entrada do utilizador e dos sockets com `select()`
 *     - Processa comandos de utilizador e mensagens da rede (e.g. ENTRY, SAFE)
 *     - Gere as ligações e atualiza os vizinhos internos e externos
 *
 * COMMENTS
 *   Pedro Reis e Gonçalo Alexandre trabalharam neste projeto.
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>  
#include <time.h>
#include <stdbool.h>

# include "lib.h"
# include "commands.h"



int main(int argc, char *argv[]) {

    if (argc < 4) {
        printf("Usage: %s cache IP TCP [regIP] [regUDP]\n", argv[0]);
        return 1;
    }

    NodeData my_node;
    strncpy(my_node.ip, argv[2], sizeof(my_node.ip) - 1);
    my_node.ip[sizeof(my_node.ip) - 1] = '\0';
    my_node.tcp_port = atoi(argv[3]);
    strcpy(my_node.net,"xxx");
    char reg_server_ip[16];
    int reg_server_port;
    int cache_size = atoi(argv[1]);
    if (argc >= 5) {
        strncpy(reg_server_ip, argv[4], sizeof(reg_server_ip) - 1);
        reg_server_ip[sizeof(reg_server_ip) - 1] = '\0';
    } else {
        strncpy(reg_server_ip, "193.136.138.142", sizeof(reg_server_ip) - 1);
        reg_server_ip[sizeof(reg_server_ip) - 1] = '\0';  // <- adicionado para evitar warning
    }

    if (argc >= 6) {
        reg_server_port = atoi(argv[5]);
    } else {
        reg_server_port = 59000;
    }

    printf("Configuração:\n Cache: %d\n IP: %s\n TCP: %d\n RegIP: %s\n RegUDP: %d\n",
           cache_size, my_node.ip, my_node.tcp_port, reg_server_ip, reg_server_port);


    printf("NDN Node started. Enter commands:\n\n");

    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(STDIN_FILENO, &master_fds);         // Adicionar stdin para comandos de usuário
    int max_fd = STDIN_FILENO; 


    char command[BUFFER_SIZE];
    my_node.socket_listening=-1;
    my_node.flaginit=0;
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
						// printf("O valor de fd é %d, o valor de my_node.socket_listening é %d e o valor de my_node.flaginit é %d\n",fd,my_node.socket_listening,my_node.flaginit);
                        // printf("O max_fd é %d\n",max_fd);
                        if(my_node.socket_listening!=-1 && my_node.flaginit==0){    
                            FD_SET(my_node.socket_listening, &master_fds);//Adicionar socket de listening
                            my_node.flaginit=1;
                            if (max_fd < my_node.socket_listening) {
                                max_fd = my_node.socket_listening;
                            }
                        }
                        if (fd != 0 && fd != -1 && fd != -2) {
                            FD_SET(fd, &master_fds);
                            if (max_fd < fd) {
                                max_fd = fd;
                            }
                        }

                        if (fd == -2) {

                            max_fd = cleanNeighboors(&my_node, &master_fds);
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
                            close(i);
                            FD_CLR(i, &master_fds);

                            int new_max_fd = STDIN_FILENO;
                            
                            for (int i = 0; i < FD_SETSIZE; i++) {
                                if (FD_ISSET(i, &master_fds)) {
                                    new_max_fd = i;
                    
                                }
                            }
                            
                            max_fd=new_max_fd;
                            
                            if (handleLeave(&my_node,i,&master_fds,&max_fd)!=0)
                            {
                                perror("handle leave");
                                exit(1);
                            }
                            
                            // Tratar terminação de sessão conforme o protocolo
                        } else {
                            perror("read");
                        }
                    } else {
                        buffer[n] = '\0';  // Garantir terminação da string

                    char *message = buffer;
                    char *next_message;

                    //ciclo que lê todas as mensagens no buffer    
                    while ((next_message = strchr(message, '\n')) != NULL) {
                        *next_message = '\0';  // muda o \n para marcar a mensagem como lida
                        
                        
                        
                        // Processamento das mensagens do protocolo
                            
                            // Processar mensagem ENTRY
                        if (strncmp(message, "ENTRY", 5) == 0) {
                            char ip[16];
                            int tcp_port;
                            printf("A processar mensagem de entry\n");
                            if (sscanf(message + 6, "%15s %d", ip, &tcp_port) == 2) {
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
                                    
                                    
                                    // Adicionar como vizinho interno
                                    add_internal_neighbor(&my_node, new_node);
                                    
                                    // Primeiro, enviar mensagem SAFE
                                    ssize_t nleft,nwritten;
                                    char safe_msg[BUFFER_SIZE],*ptr;
                                    snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n",my_node.vzext.ip, my_node.vzext.tcp_port);
                                      
                                    ptr=safe_msg;
                                    nleft=strlen(safe_msg);
                                    printf("Enviar para a socket %d que é um nó interno %s:%d e agora é o externo a mensagem --> %s",i,my_node.vzext.ip,my_node.vzext.tcp_port,safe_msg);
                                    
                                    while (nleft>0)
                                    {
                                        nwritten=write(i,ptr,nleft);
                                        if(nwritten<=0)/*error*/exit(1);
                                        nleft-=nwritten;
                                        ptr+=nwritten;
                                    }                                    
                                    
                                    
                                    // Depois, enviar mensagem ENTRY
                                    char entry_msg[BUFFER_SIZE];
                                    snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", 
                                            my_node.ip, my_node.tcp_port);
                                    
                                            ptr=entry_msg;
                                            nleft=strlen(entry_msg);
                                            
                                            while (nleft>0)
                                            {
                                                nwritten=write(i,ptr,nleft);
                                                if(nwritten<=0)/*error*/exit(1);
                                                nleft-=nwritten;
                                                ptr+=nwritten;
                                            }
                                } else {
                                    // Adicionar como vizinho interno apenas se não for o vizinho externo
                                    add_internal_neighbor(&my_node, new_node);
                                    
                                    ssize_t nleft,nwritten;       
                                    // Enviar mensagem SAFE com o vizinho externo
                                    char safe_msg[BUFFER_SIZE],*ptr;
                                    printf("Enviar para a socket %d que é um nó interno %s:%d e agora é o externo a mensagem --> %s",i,my_node.vzext.ip,my_node.vzext.tcp_port,safe_msg);
                                    snprintf(safe_msg, sizeof(safe_msg), "SAFE %s %d\n", 
                                            my_node.vzext.ip, my_node.vzext.tcp_port);
                                            
                                            ptr=safe_msg;
                                            nleft=strlen(safe_msg);
                                            
                                            while (nleft>0)
                                            {
                                                nwritten=write(i,ptr,nleft);
                                                if(nwritten<=0)/*error*/exit(1);
                                                nleft-=nwritten;
                                                ptr+=nwritten;
                                            }
                                    
                                }
                            }
                        }
                            // Processar mensagem SAFE
                        else if (strncmp(message, "SAFE", 4) == 0) {
                            printf("A processar mensagem de safe\n");
                            char ip[16];
                            int tcp_port;
                            if (sscanf(message + 5, "%15s %d", ip, &tcp_port) == 2) {
                                // Atualizar nó de salvaguarda
                                strncpy(my_node.vzsalv.ip, ip, sizeof(my_node.vzsalv.ip) - 1);
                                my_node.vzsalv.ip[sizeof(my_node.vzsalv.ip) - 1] = '\0';
                                my_node.vzsalv.tcp_port = tcp_port;
                                my_node.vzsalv.socket_fd = i;
                            }
                        }
                    message = next_message + 1;
                    }
                    }
                }
            }
        }
    }
    return 0;
}

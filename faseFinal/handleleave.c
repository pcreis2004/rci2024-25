
int handleLeave(NodeData *mynode,int fdClosed, fd_set *master_fds, int *max_fd){
    //Verificar se foi o externo
    if (fdClosed == mynode->vzext.socket_fd)
    {
        printf("Foi um vizinho externo, Tens que mandar um Entry para o de Salvaguarda e um Safe para todos os internos\n");

        //Vamos verificar se somos o nó de salvaguarda de nós próprios
        if (strcmp(mynode->vzsalv.ip,mynode->ip)==0 && mynode->vzsalv.tcp_port == mynode->tcp_port)
        {
            printf("CALMAAAAA\nO teu vizinho de Salvaguarda eras tu próprio ");
            
            //Verificar se o nó externo que saiu tbm era interno
            for (int i = 0; i < mynode->numInternals; i++)
            {
                if (fdClosed == mynode->intr[i].socket_fd)
                {
                    printf("era também um vizinho interno, remove-o da lista de internos,");
                    //remove intr

                    printf("Vizinho %d removido\n",mynode->intr[i].tcp_port);
                    mynode->intr[i].tcp_port = -1;
                    mynode->intr[i].socket_fd = -1;
                    mynode->intr[i].ip[0] = '\0';
                    mynode->numInternalsReal--;
                }
                
            }

            //Verificar se ainda tens internos
            if (mynode->numInternalsReal!=0)
            {
                
                printf(" vais ter que escolher um interno para se tornar o teu externo, mandar lhe entry e enviar SAfe para os outros internos\n");
                //random interno

                srand(time(NULL));
                int random_index = rand() % mynode->numInternals;
                while (mynode->intr[random_index].socket_fd==-1)
                {
                    random_index = rand() % mynode->numInternals;
                }

                mynode->vzext.tcp_port = mynode->intr[random_index].tcp_port;
                mynode->vzext.safe_sent=mynode->intr[random_index].safe_sent;
                // Enviar mensagem ENTRY
                

                //Possível problema aqui
                // int sockfd = connect_to_node(mynode->vzext.ip, mynode->vzext.tcp_port);
                // if (sockfd < 0) {
                //     printf("Falha ao conectar a %s:%d\n", mynode->vzext.ip, mynode->vzext.tcp_port);
                //     return -1;
                // }

                // mynode->vzext.socket_fd = sockfd;
                mynode->vzext.socket_fd = mynode->intr[random_index].socket_fd;

                

                ssize_t nleft,nwritten;
                char *ptr;
                char entry_msg[BUFFER_SIZE];
                
                snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", mynode->ip, mynode->tcp_port);
                
                ptr=entry_msg;
                nleft=strlen(entry_msg);
                printf("Enviar Entry para o nó externo\n");

                while (nleft>0)
                {
                    nwritten=write(mynode->vzext.socket_fd,ptr,nleft);
                    if(nwritten<=0)/*error*/exit(1);
                    nleft-=nwritten;
                    ptr+=nwritten;
                }
                 FD_SET(mynode->vzext.socket_fd, master_fds);
        
                int new_max_fd = STDIN_FILENO;
                for (int i = 0; i < FD_SETSIZE; i++) {
                    if (FD_ISSET(i, master_fds)) {
                        new_max_fd = i;
        
                    }
                }
                *max_fd=new_max_fd;

                printf("Enviar Safe para todos os nós internos\n");

                for (int i = 0; i < mynode->numInternals; i++)
                {
                    if (/*mynode->intr[i].socket_fd != mynode->vzext.socket_fd && */mynode->intr[i].socket_fd != -1)
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
        //Não és o salvaguarda de ti próprio
        //mandar entry para o de salvaguarda e safe aos internos 
        int sockfd = connect_to_node(mynode->vzsalv.ip, mynode->vzsalv.tcp_port);
        if (sockfd < 0) {
            printf("Falha ao conectar a %s:%d\n", mynode->vzsalv.ip, mynode->vzext.tcp_port);
            return -1;
        }
        mynode->vzext.socket_fd=sockfd;
        strcpy(mynode->vzext.ip,mynode->vzsalv.ip);
        mynode->vzext.tcp_port=mynode->vzsalv.tcp_port;

        
        printf("Enviar Safe para todos os nós internos\n");
        for (int i = 0; i < mynode->numInternals; i++)
        {
            if (mynode->intr[i].socket_fd != mynode->vzext.socket_fd && mynode->intr[i].socket_fd != -1)
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
        ssize_t nleft,nwritten;
        char *ptr;
        char entry_msg[BUFFER_SIZE];

        snprintf(entry_msg, sizeof(entry_msg), "ENTRY %s %d\n", mynode->ip, mynode->tcp_port);
        printf("Enviar para a socket %d que era o nó de salvaguarda %s:%d e agora é o externo a mensagem --> %s",sockfd,mynode->vzext.ip,mynode->vzext.tcp_port,entry_msg);
        ptr=entry_msg;
        nleft=strlen(entry_msg);
        
        while (nleft>0)
        {
            nwritten=write(mynode->vzext.socket_fd,ptr,nleft);
            if(nwritten<=0)/*error*/exit(1);
            nleft-=nwritten;
            ptr+=nwritten;
        }
        FD_SET(mynode->vzext.socket_fd, master_fds);

        int new_max_fd = STDIN_FILENO;
        for (int i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, master_fds)) {
                new_max_fd = i;

            }
        }
        *max_fd=new_max_fd;

    
    }else{

        for (int i = 0; i < mynode->numInternals; i++)
        {
            if (fdClosed == mynode->intr[i].socket_fd)
            {
                printf("Foi um vizinho interno, remove-o só da lista de internos\n");

                mynode->intr[i].tcp_port = -1;
                mynode->intr[i].socket_fd = -1;
                mynode->intr[i].ip[0] = '\0';
                mynode->numInternalsReal--;
            }
            
        }
    }
    


    return 0;
}



#ifndef COMMANDS_H
#define COMMANDS_H

#include "lib.h"

int handle_command(char *command, NodeData *myNode, char *ip, int port);
int leave(NodeData *myNode, char *serverIp,int serverPort);
int handleLeave(NodeData *mynode,int fdClosed, fd_set *master_fds,int *max_fd);
int join(char *net, char *ip, int port, NodeData *myNode, int cache_size);
int djoin(NodeData *myNode, char *connectIP, int connectTCP, int cache_size);
void show_topology(NodeData *myNode);

#endif

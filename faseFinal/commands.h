#ifndef COMMANDS_H
#define COMMANDS_H

#include "lib.h"

int handle_command(char *command, NodeData *myNode, char *ip, int port);
void free_node_memory(NodeData *myNode);
int retrieve(NodeData *myNode, char *name);
int handle_interest(NodeData *myNode, char *name, int fd);
int handle_object(NodeData *myNode, char *name, int fd);
int handle_noobject(NodeData *myNode, char *name, int fd);
int send_response(NodeData *myNode, char *msg);
void show_names(NodeData *myNode);
int create(NodeData *myNode, char *name);
int delete(NodeData *myNode, char *name);
void table_interest(NodeData *myNode, char *name);
int leave(NodeData *myNode, char *serverIp,int serverPort);
int handleLeave(NodeData *mynode,int fdClosed, fd_set *master_fds,int *max_fd);
int join(char *net, char *ip, int port, NodeData *myNode, int cache_size);
int djoin(NodeData *myNode, char *connectIP, int connectTCP, int cache_size);
void show_topology(NodeData *myNode);

#endif


#ifndef _EXT_EPOLL_DATA_H_
#define _EXT_EPOLL_DATA_H_

#include "setup.h"
#include <sys/epoll.h>

#define GET_TYPE   1
#define POST_TYPE  2

#define REQUEST_NOT_COMPLETED   0
#define REQUEST_COMPLETED       1

// NOT UNION ! 
typedef struct ext_epoll_data {
  //epoll_data_t data;      // usual (union) epoll_data ( need it ?)
  int status;				// completed / not completed
  int sfd;					// socket fd
  int type;					// type of connection (GET_TYPE, POST_TYPE)
  char *header;            	// header of request
  FILE *fp;					// a file which the server has to send to client for its request
  //char *filename;         // actually for POST requests when file size is big (but post requests are NOT implemented)
  size_t content_length;	// for POST requests
} ext_epoll_data_t;

struct Node {
  ext_epoll_data_t data;
  struct Node * next;
};

struct List {
  struct Node *head;
};

typedef struct Node Node_t;
typedef struct List List_t;

List_t * list_new();
void list_delete(List_t *l);
Node_t *find_node(List_t *l, int fd);
int insert_node(List_t *l, ext_epoll_data_t data);
void remove_node(List_t *l, int fd);

#endif // _EXT_EPOLL_DATA_H_
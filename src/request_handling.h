
#ifndef _REQUEST_HANDLING_H_
#define _REQUEST_HANDLING_H_

#include "setup.h"
#include "ext_epoll_data.h"

// handle a request from a client
int handle(char *request, int sfd, List_t *list);


#endif // _REQUEST_HANDLING_H_

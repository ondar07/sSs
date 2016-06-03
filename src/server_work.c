
#include "request_handling.h"
#include "log.h"
#include "ext_epoll_data.h"
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>

#define BUFSIZE 1024

#define MEM_ZERO(ptr, size) memset((ptr), '\0', size * sizeof(char));

// for each connection the web-server keeps structure (with data for connection)
// and these structures are kept as elements of singly linked list
// (allocate memory for @list in start_server() function)
// (List_t, Node_t types are declared in ext_epoll_data.h)
List_t *list;

//
// set O_NONBLOCK flag on the descriptor
//
// @sfd -- socket descriptor
// return: 0 if success, else -1
static int make_socket_non_blocking(int sfd) {
  int flags, s;

  flags = fcntl(sfd, F_GETFL, 0);  // get file access mode and file status flags
  if (flags == -1)
    goto error;

  flags |= O_NONBLOCK;
  s = fcntl(sfd, F_SETFL, flags);  // set new status flags for the socket descriptor
  if (s == -1)
    goto error;

  return 0;

error:
  perror("[make_socket_non_blocking] fcntl");
  return -1;
}

#if 0
//#ifdef FORK_MODEL

// in Linux a process after running becomes ZOMBIE
// and this process will remain in the memory
//   (if be precise then its task_struct will not be returned to slab allocator)
// in case if the parent process wished to know about it
void zombieProcessesHandler() {
  // WNOHANG flag allows to return immediately if no child has exited
  // (without this flag calling (main) process will be suspended)
  while(waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

void start_server() {
  int listenSocketID;

  listenSocketID = create_and_bind_listen_socket();

  //
  if (listen(listenSocketID, MAXCONNECTIONS) == -1) {
    perror("listen socket");
    exit(1);
  }

  //
  {
    int acceptedSocketID;
    struct sockaddr_storage incoming_addr;  // address info of incoming connection
    socklen_t addrlen = sizeof(incoming_addr);
    char *buf = NULL;
    struct sigaction sa;
    pid_t childpid;

    {
      sa.sa_handler = zombieProcessesHandler;
      sigemptyset(&sa.sa_mask);
      sa.sa_flags = SA_RESTART;
      if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
      }
    }

    while (1) {
      if ((acceptedSocketID = accept(listenSocketID, (struct sockaddr *) &incoming_addr, &addrlen)) < 0) {
        PRINT("error: accept");
        continue;
      }

      if ((childpid = fork()) == 0) {
        // child process executes

        close(listenSocketID);
        buf = (char *)malloc(BUFSIZE * sizeof(char));

        recv(acceptedSocketID, buf, BUFSIZE, 0);
        PRINT("%s\n", buf);
        if (send(acceptedSocketID, "hello world\n", 12, 0) == -1)
          perror("send (acceptedSocketID");
        close(acceptedSocketID);
        free(buf);
        exit(0);
        // child process terminates
      } if (childpid < 0) {
        // failure
        perror("fork() failed");
      }

      close(acceptedSocketID);  // the parent no longer needs this descriptor
    }
  }
  close(listenSocketID);
}
#endif


#define CHECK(s, res, errmsg) if((s = res) < 0) { perror(errmsg); exit(-1); }

//
// handle all new incoming connections (by using accept() function)
//
static int new_connections_handling(int listenSocketID, int efd) {
  struct epoll_event event;
  int status;
  int infd;                                   // socket desciptor of a new connection

  while (1) {
    struct sockaddr in_addr;                  // address of a new connection
    socklen_t in_len;

    in_len = sizeof(in_addr);

    // accept a new connection
    // this function extracts the first connection request on the queue of pending connections for the listening socket,
    //    and creates a new socket descriptor for this connection and returns it (also, fills @in_addr struct)
    infd = accept(listenSocketID, &in_addr, &in_len); // this function does NOT BLOCK the caller,
                                                      // because 1) we call this function when epoll has reported about incoming connections
                                                      // or 2) we have processed all connections, so =>
    // => If the socket (@listenSocketID) is marked nonblocking (it's OUR CASE) and 
    // NO PENDING connections are present on the queue, accept() fails with the
    // error EAGAIN or EWOULDBLOCK.
    if (infd == -1) {
      if ((errno == EAGAIN) ||
          (errno == EWOULDBLOCK))
      {
        // we have processed all incoming connections
        break;
      }
      else {
        PRINT("[new_connections_handling] ERROR: accept!\n");
        return -1;
      }
    }

#ifdef DEBUG
    PRINT("Accepted connection on descriptor %d\n", infd);
#endif

    // make the incoming socket non-blocking
    status = make_socket_non_blocking(infd);
    if (status == -1) {
      PRINT("[new_connections_handling]make_socket_non_blocking %d", infd);
      goto error;
    }

    // add it to the list of fds to monitor
    event.data.fd = infd;

    // set types of events for this epoll_event structure
    //    EPOLLIN  -- for reading
    //    EPOLLET  -- Edge Trigged events
    //    EPOLLOUT -- descriptor is READY TO RECEIVE DATA (to write new chunk of data to client)
    //                                                    (especially if the size of file is large)
    //event.events = EPOLLIN | EPOLLOUT | EPOLLET;   // NOT WORKS PROPERLY!!!!!!!!!!!!!!

    // we use level-triggered for client descriptors
    event.events = EPOLLIN | EPOLLOUT;

    // add @infd to @efd (epoll descriptor)
    // and associate such @event with @infd
    status = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
    if (status == -1) {
      PRINT("[new_connections_handling]epoll_ctl %d", infd);
      goto error;
    }
  }

  return 0;

error:
  close(infd);
  return -1;
}

// This function adds @temporary_buf content to end of @big_buf (strcat)
// If @big_buf memory is not enought for it, 
//    reallocation for @big_buf will be done
// @big_buf_max_len -- potential maximum size of @big_buf
static char *strcat_buf(char *big_buf, char *temporary_buf, size_t *big_buf_max_len) {
  size_t buf_len = strnlen(temporary_buf, BUFSIZE);
  size_t actual_big_buf_len = strnlen(big_buf, *big_buf_max_len);

  if ( actual_big_buf_len + buf_len >= *big_buf_max_len ) {
    // if @big_buf memory is not enough
    // reallocate @big_buf, increasing its size by BUFSIZE
    (*big_buf_max_len) += BUFSIZE + 1;
    // TODO: CHECK
    big_buf = realloc(big_buf, *big_buf_max_len);
    memset(big_buf + actual_big_buf_len, '\0', *big_buf_max_len - actual_big_buf_len);
#ifdef DEBUG
    PRINT("[strcat_buf]realloc(); now big_buf_max_len=%zu\n", *big_buf_max_len);
#endif
  }

  big_buf = strncat(big_buf, temporary_buf, BUFSIZE);
  //
  big_buf[*big_buf_max_len - 1] = '\0';
  return big_buf;
}

//
// This function is caused by event_in_handling() (when req != NULL)
// and by event_out_handling (req == NULL)
//
// It causes handle() function
// if it returns -1, it means that the original request was NOT COMPLETED yet (some chunks of requested resource remained)
// else (if it returns 0) we should close connection on this socket (it removes this descriptor from epoll set of monitored fds)
//
static int call_request_handling(char *req, int fd) {
#ifdef DEBUG
  PRINT("================================================================================\n");
  PRINT("================================================================================\n");
  PRINT("request on sfd=%d\n", fd);
#endif

  if (req) {
    // we can see @req
#ifdef DEBUG
    //PRINT("%s\n", req);
    //PRINT("This is END of request\n\n");
#endif
  }

  // handling of this request
  // see: request_handling.c
  if (handle(req, fd, list) < 0) {
    // do NOT CLOSE this connection
    // wait new data on this socket (for new chunks)
    return -1;
  }

  // an original request was processed fully
  // so connection on this socket will be closed

  PRINT("Closed connection on descriptor %d\n", fd);
  // Closing the descriptor will make epoll remove it
  //   from the set of descriptors which are monitored
  if (close(fd) < 0)
    PRINT("[events_handling]ERROR: close fd=%d\n", fd);
  PRINT("\n\n");

  return 0;
}


// 
// handle events[i] element (events[i].data.fd descriptor):
//   1. read available data completely (edge-triggered mode)
//   2. handle the request on this descriptor (events[i].data.fd)
//
// @events -- a set of monitored descriptors
// @i      -- the sequence number of element in @events array, which is to be processed
static int event_in_handling(struct epoll_event *events, int i) {
  char *big_buf;
  size_t big_buf_max_len = BUFSIZE;    // the current size of @big_buf (it can increase)

  big_buf = (char *)malloc(BUFSIZE * sizeof(char));
  MEM_ZERO(big_buf, BUFSIZE);

#ifdef DEBUG
  PRINT("[events_handling] from sfd=%d\n", events[i].data.fd);
#endif

  // We have data on the fd waiting to be read. 
  //   We must read available data completely,
  //   as we are running in edge-triggered mode
  //   and won't get a notification again for the same data
  while (1) {
    ssize_t count;      // a number of read bytes
    char buf[BUFSIZE];

    MEM_ZERO(buf, BUFSIZE);
    
    //count = read(events[i].data.fd, buf, BUFSIZE);
    count = recv(events[i].data.fd, buf, BUFSIZE, 0);
    if (count == -1) {
      // earlier we set incoming socket descriptor O_NONBLOCK
      // so: 
      //   If errno == EAGAIN, 
      //      that means we have read all data
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      // else 
      //    there is another error
      PRINT("[events_handling]ERROR: read from fd=%d\n", events[i].data.fd);
    }
    else if (count == 0) {
      // end of data
      // the remote has closed the connection
      break;
    }

    big_buf = strcat_buf(big_buf, buf, &big_buf_max_len);
  }

  // now the following function processes the @request
  // and send (if http @request will be correct) only header
  // (the requested resource will be sent by chunks (if it so large) in event_out_handling() )
  call_request_handling(big_buf, events[i].data.fd);
  if (big_buf)
    free(big_buf);
  return 0;
}

// 
// This function sends new chunks of requested resource into socket to client
//
// When is this function used?
//    A client made a request earlier.
//    but the web-server did NOT SATISFY a request FULLY yet:
//       1) the web-server only processed a request and send a http-header (for the first time)
//       2) or a size of the requested resource is VERY LARGE, 
//          so the web-server has divided it into chunks and send each chunk, when client is ready to receive
//    So client waits new chunk of data (it is ready to receive data, so we get EPOLLOUT event)
//    And we should send new data to client
// 
// call_request_handling() function will close connection if the request is processed fully
// 
static int event_out_handling(struct epoll_event *events, int i) {
  Node_t *node;

  node = find_node(list, events[i].data.fd);

  if (node == NULL) {
    // for cases, when a connection was set
    // and client is not ready to write into socket a request yet
    //
    return -1;
  }

#ifdef DEBUG
  PRINT("EPOLLOUT on %d\n", events[i].data.fd);
  PRINT("[events_handling] from sfd=%d\n", events[i].data.fd);
#endif

  call_request_handling(NULL, events[i].data.fd);

  return 0;
}

static int event_handling(struct epoll_event *events, int i) {
  if (events[i].events & EPOLLIN) {
#ifdef DEBUG
    PRINT("EPOLLIN on %d\n", events[i].data.fd);
#endif
    return event_in_handling(events, i);
  }
  else if ((events[i].events & EPOLLOUT) && !(events[i].events & EPOLLIN) ) {
    return event_out_handling(events, i);
  } else {
    PRINT("[event_handling]ERROR: UNKNOWN EVENT\n");
  }
}

void start_server() {
  int listenSocketID, status;
  int efd;    // epoll descriptor to watch events
  struct epoll_event event;
  struct epoll_event *events; // for descriptors

  // create_and_bind_listen_socket(): see in setup.c
  listenSocketID = create_and_bind_listen_socket();
  
  CHECK(status, make_socket_non_blocking(listenSocketID), "make socket non-blocking");

  CHECK(status, listen(listenSocketID, MAXCONNECTIONS), "listen");

  // create epoll descriptor
  // it returns a file descriptor referring to the new epoll instance in @efd
  CHECK(efd, epoll_create1(0), "epoll_create1");

  // assign what event we need to monitor
  event.data.fd = listenSocketID;
  event.events = EPOLLIN | EPOLLOUT | EPOLLET; // watch just incoming(EPOLLIN) and Edge Trigged(EPOLLET) events

  // add the listening socket to watch for input events in an edge-triggered mode
  CHECK(status, epoll_ctl(efd, EPOLL_CTL_ADD, listenSocketID, &event), "epoll_ctl");

  // storage array for incoming events from epoll_wait(events)
  // and maximum events count could be MAXEVENTS
  events = (struct epoll_event *)calloc(MAXEVENTS, sizeof(struct epoll_event));
  if (events == NULL) {
    PRINT("[start_server]ERROR: out of memory for events array!\n");
    goto out_of_memory;
  }

  // create a list
  // (see ext_epoll_data.c)
  list = list_new();
  if (!list) {
    PRINT("[start_server]ERROR: out of memory for list!\n");
    free(events);
    goto out_of_memory;
  }

  // The event loop
  while (1) {
      int n, i;

      // wait for events on @efd (the thread remains blocked waiting for events)
      // available events will be stored in @events array
      // (-1) -- wait indefinitely
      // @n   -- number of ready descriptors
      n = epoll_wait(efd, events, MAXEVENTS, -1);
      for (i = 0; i < n; i++) {
          if ((events[i].events & EPOLLERR) ||
              (events[i].events & EPOLLHUP)
             )
          {
            #ifdef DEBUG
            PRINT("ERROR in wait\n");
            #endif
            
            Node_t *node;
            // An error (the connection was broken, for example) has occured on this fd, or the socket is not
            //   ready for reading

            // we should delete this node from list (moreover, remove_node() free memory of @node)
            node = find_node(list, events[i].data.fd);
            if (node != NULL) {
              if (node->data.header != NULL)
                free(node->data.header);
              if (node->data.fp != NULL) {
                PRINT("close fd %p\n", node->data.fp);
                if (fclose(node->data.fp) < 0) {
                  PRINT("ERROR: fclose with %p on socketfd=%d\n", node->data.fp, events[i].data.fd);
                }
              }
              remove_node(list, events[i].data.fd);
            }

            // closing the descriptor automatically removes it from the watched set of epoll instance @efd
            if (events[i].data.fd) {
              close(events[i].data.fd);
            }
            continue;
          }
          else if (listenSocketID == events[i].data.fd) {
            // We have a notification on the listening socket
            //   => one or more incoming connections
            new_connections_handling(listenSocketID, efd);
            continue;
          }
          else {

            // We have data on the fd (events[i].data.fd) waiting to be read. Read and
            //   display it. We must read whatever data is available
            //   completely, as we are running in edge-triggered mode
            //   and WILL NOT get a notification again for the same data 

            // (NOW LEVEL-TRIGGERED), but we try to read data completely too
            event_handling(events, i);
          }
      }
  }

  // free memory
  free(events);
  list_delete(list);

out_of_memory:
  close(efd);
  close(listenSocketID);
}

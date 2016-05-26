
#include "request_handling.h"
#include "log.h"
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>

#define BUFSIZE 512

// @listenSocketID -- socket descriptor which was received from create_and_bind_listen_socket() function
// return: 0 if success, else -1
static int make_socket_non_blocking(int listenSocketID) {
  int flags, s;

  flags = fcntl(listenSocketID, F_GETFL, 0);  // get file access mode and file status flags
  if (flags == -1)
    goto error;

  flags |= O_NONBLOCK;
  s = fcntl(listenSocketID, F_SETFL, flags);  // set new status flags for the socket descriptor
  if (s == -1)
    goto error;

  return 0;

error:
  perror("[make_socket_non_blocking] fcntl");
  return -1;
}

#ifdef FORK_MODEL
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

static int new_connections_handling(int listenSocketID, struct epoll_event *event, int efd) {
  int status;
  int infd;                                 // socket desciptor of a new connection

  while (1) {
    struct sockaddr in_addr;                  // address of a new connection
    socklen_t in_len;
#ifdef DEBUG
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
#endif

    in_len = sizeof(in_addr);

    // accept a new connection
    // this function extracts the first connection request on the queue of pending connections for the listening socket,
    //    and creates a new socket descriptor for this connection and returns it
    infd = accept(listenSocketID, &in_addr, &in_len); // this function does NOT BLOCK the caller, because ...
    // If the socket is marked nonblocking (it's OUR CASE) and 
    // NO PENDING connections are present on the queue, accept() fails with the
    // error EAGAIN or EWOULDBLOCK.
    if (infd == -1) {
      if ((errno == EAGAIN) ||
          (errno == EWOULDBLOCK))
      {
        // We have processed all incoming connections
        break;
      }
      else {
        perror("[new_connections_handling]accept");
        return -1;
      }
    }

#ifdef DEBUG
    status = getnameinfo(&in_addr, in_len,
                          hbuf, sizeof(hbuf),
                          sbuf, sizeof(sbuf),
                          NI_NUMERICHOST | NI_NUMERICSERV);
    if (status == 0) {
      PRINT("Accepted connection on descriptor %d "
                "(host=%s, port=%s)\n", infd, hbuf, sbuf);
    }
#endif

    // Make the incoming socket non-blocking
    status = make_socket_non_blocking(infd);
    if (status == -1) {
      PRINT("[new_connections_handling]make_socket_non_blocking %d", infd);
      goto error;
    }

    // Add it to the list of fds to monitor
    event->data.fd = infd;
    // set types of events for this epoll_event structure
    //    EPOLLIN -- for reading
    //    EPOLLET -- Edge Trigged events
    event->events = EPOLLIN | EPOLLET;
    // add @infd to @efd (epoll descriptor)
    // and associate such @event with @infd
    status = epoll_ctl(efd, EPOLL_CTL_ADD, infd, event);
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
    (*big_buf_max_len) += BUFSIZE;
    big_buf = realloc(big_buf, *big_buf_max_len);
    memset(big_buf + actual_big_buf_len, 0, *big_buf_max_len - actual_big_buf_len);
#ifdef DEBUG
    PRINT("[strcat_buf]realloc(); now big_buf_max_len=%zu\n", *big_buf_max_len);
#endif
  }

  big_buf = strncat(big_buf, temporary_buf, BUFSIZE);
  return big_buf;
}

// 
static int events_handling(struct epoll_event *events, int i) {
  int done = 0;
  char *big_buf = (char *)malloc(BUFSIZE * sizeof(char));
  size_t big_buf_max_len = BUFSIZE;    // this variable 

  memset(big_buf, 0, BUFSIZE);

#ifdef DEBUG
  PRINT("[events_handling] from %d\n", i);
#endif
  // We have data on the fd waiting to be read. Read and
  //   display it. We must read whatever data is available
  //   completely, as we are running in edge-triggered mode
  //   and won't get a notification again for the same
  //   data
  while (1) {
    ssize_t count;
    char buf[BUFSIZE];

    count = read(events[i].data.fd, buf, BUFSIZE);
    if (count == -1) {
      // If errno == EAGAIN, 
      //    that means we have read all data
      if (errno == EAGAIN) {
        done = 1;
        break;
      }
      // else 
      //    there is another error
      PRINT("[events_handling]ERROR: read from fd=%d\n", events[i].data.fd);
      done = 1; 
    }
    else if (count == 0) {
      // End of data
      // The remote has closed the connection
      done = 1;
      break;
    }

    big_buf = strcat_buf(big_buf, buf, &big_buf_max_len);
  }

  // TODO: create a separate function ?
  if (done) {
    PRINT("================================================================================\n");
    PRINT("================================================================================\n");
    PRINT("%s", big_buf);

    // handling of this request
    // see: request_handling.c
    handle(big_buf, events[i].data.fd);

    PRINT("Closed connection on descriptor %d\n", events[i].data.fd);

    // Closing the descriptor will make epoll remove it
    //   from the set of descriptors which are monitored
    if (close(events[i].data.fd) < 0)
      PRINT("[events_handling]ERROR: close fd=%d\n", events[i].data.fd);
    PRINT("\n\n");
  }

  free(big_buf);
  return 0;
}


void start_server() {
  int listenSocketID, status;
  int efd;    // epoll descriptor to watch events
  struct epoll_event event;
  struct epoll_event *events;

  listenSocketID = create_and_bind_listen_socket();
  
  CHECK(status, make_socket_non_blocking(listenSocketID), "make socket non-blocking");

  CHECK(status, listen(listenSocketID, MAXCONNECTIONS), "listen");

  // create epoll descriptor
  CHECK(efd, epoll_create1(0), "epoll_create1");

  event.data.fd = listenSocketID;
  event.events = EPOLLIN | EPOLLET; // watch just incoming(EPOLLIN) and Edge Trigged(EPOLLET) events
  CHECK(status, epoll_ctl(efd, EPOLL_CTL_ADD, listenSocketID, &event), "epoll_ctl");

  // storage array for incoming events from epoll_wait(events)
  // and maximum events count could be MAXEVENTS
  events = (struct epoll_event *)calloc(MAXEVENTS, sizeof(event));
  if (events == NULL) {
    perror("[]ERROR: out of memory for events array\n");
    goto out_of_memory;
  }

  // The event loop
  while (1) {
      int n, i;

      // wait for events on @efd
      // available events will be stored in @events array
      // (-1) -- wait indefinitely
      // @n   -- number of ready descriptors
      n = epoll_wait(efd, events, MAXEVENTS, -1);
      for (i = 0; i < n; i++) {
          if ((events[i].events & EPOLLERR) ||
              (events[i].events & EPOLLHUP) ||
              (!(events[i].events & EPOLLIN)))
          {
              // An error has occured on this fd, or the socket is not
              //   ready for reading (why were we notified then?)
            fprintf(stderr, "epoll error\n");
            close(events[i].data.fd);
            continue;
          }
          else if (listenSocketID == events[i].data.fd) {
            // We have a notification on the listening socket
            //   => one or more incoming connections
            new_connections_handling(listenSocketID, &event, efd);
            continue;
          }
          else {
            // We have data on the fd waiting to be read. Read and
            //   display it. We must read whatever data is available
            //   completely, as we are running in edge-triggered mode
            //   and won't get a notification again for the same
            //   data
            events_handling(events, i);
          }
      }
  }

  free(events);

out_of_memory:
  close(efd);
  close(listenSocketID);
}

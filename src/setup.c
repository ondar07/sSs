
#include "setup.h"
#include <fcntl.h>

#define BUF_SIZE 256

// allocate memory for @srv_option and
// set @srv_option with a value of @option_val
// return pointer to srv_option if success
// else NULL
static char * set_server_option(char *srv_option, const char *option_val) {
  size_t option_length = strlen(option_val) + 1;

  srv_option = (char *)malloc(option_length * sizeof(char));
  if (!srv_option) {
    PRINT("[init_server] ERROR: out of memory for %s\n", option_val);
    return NULL;
  }
  memset((void *)srv_option, 0, option_length);
  memcpy(srv_option, option_val, strlen(option_val));

  return srv_option;
}

//
static int fopen_mime_file() {
  const char *mime_file = "./src/mime.types";

  // TODO: this function should mmap() (man mmap) @mime_file
  // it increases PERFORMANCE of the server!
  srv_settings.mime_file = fopen(mime_file, "r");
  if (srv_settings.mime_file == NULL) {
    // TODO: error handling
    PRINT("ERROR: mime_file\n");
    return -1;
  }

  return 0;
}

// read @config_file and set some settings of the server
int init_server(const char *config_file) {
  FILE *f;
  char option[BUF_SIZE];
  char option_value[BUF_SIZE];
  char *res;

  // 1.
  if (NULL == (f = fopen(config_file, "r"))) {
    perror("[init_server] ERROR: cannot open config file!\n");
    return -1;
  }

  while (EOF != fscanf(f, "%s ", option)) {
    if (EOF == fscanf(f, "%s\n", option_value)) {
      PRINT("[init_server]value for %s option is NOT SPECIFIED\n", option);
      goto error;
    }
    if ( !strcmp(option, "PORT")) {

      res = set_server_option(srv_settings.port, option_value);
      srv_settings.port = res;
      #ifdef DEBUG
      PRINT("[init_server] DEBUG: port=%s\n",  srv_settings.port);
      #endif
      goto res_handling;
    } else if (!strcmp(option, "WWWROOT")) {
      res = set_server_option(srv_settings.wwwroot, option_value);
      srv_settings.wwwroot = res;
      #ifdef DEBUG
      PRINT("[init_server] DEBUG: wwwroot=%s\n",  srv_settings.wwwroot);
      #endif
      goto res_handling;
    } else if (!strcmp(option, "GENERATED_HTMLS_DIR")) {
      res = set_server_option(srv_settings.generated_htmls_dir, option_value);
      srv_settings.generated_htmls_dir = res;
      #ifdef DEBUG
      PRINT("[init_server] DEBUG: generated_htmls_dir=%s\n",  srv_settings.generated_htmls_dir);
      #endif
      goto res_handling;
    }

    PRINT("[init_server]%s option IS NOT KNOWN\n", option);
    continue;

res_handling:
    if (!res)
      goto error;
  }

  // 2.
  if ( fopen_mime_file() < 0)
    goto error; 

  // 3. close descriptors
  fclose(f);
  return 0;

error:
  fclose(f);
  return -1;
}

void deinit_server() {
  free(srv_settings.port);
  free(srv_settings.wwwroot);
  free(srv_settings.generated_htmls_dir);
  fclose(srv_settings.mime_file);
}

//
// (prepare for connection) 
// fill some structures
//
// @addr    -- IP addr or "www.mysite.com" (but we call with NULL, it means local host)
// @service -- "http" or port number
//
// returns: struct addrinfo
//
static struct addrinfo *resolve_server_addr_and_port(const char *addr, const char *service) {
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;    // getaddrinfo function returns a list of structures
                                // this var (servinfo) is a pointer to a head of the list

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;      // return IPv4 and IPv6 choices
  hints.ai_socktype = SOCK_STREAM;  // stream TCP socket
  hints.ai_flags = AI_PASSIVE;      // this flag allows to assign IP of THIS LOCAL host for structures of socket

  // this function returns in @servinfo a pointer to the list of addrinfo structures
  if ((status = getaddrinfo(addr, service, &hints, &servinfo)) != 0) {
    perror("[resolve_server_addr_and_port]ERROR: getaddrinfo\n");
    exit(-1);
  }
  return servinfo;
}

// 
// returns: socket descriptor for ListenSocket
int create_and_bind_listen_socket() {
  struct addrinfo *servinfo;    // getaddrinfo function returns a list of structures
  int listenSocketID;
  int reuse_addr = 1;   // this is option value to reuse addr
  struct addrinfo *p;

  // we call with NULL, it means local host
#define SRVNODE NULL
  servinfo = resolve_server_addr_and_port(SRVNODE, PORT);
#undef SRVNODE

  // find possible result from servinfo list
  // and create a socket for connecting to server
  for (p = servinfo; p != NULL; p = p->ai_next) {
    listenSocketID = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (-1 == listenSocketID) {
      perror("[create_and_bind_listen_socket]createListenSocket");
      continue;
    }

    // sometimes after server reboot, socket may continue to be at the kernel
    // if you don't wait, you could call setcokopt()
    // ( use port again )
    if (setsockopt(listenSocketID, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1) {
      perror("[create_and_bind_listen_socket]setsockopt(reuse addr)");
      exit(1);
    }

    // bind listenSocket with listened port
    // (ai_addr field has been filled with needed address info by getaddrinfo() earlier)
    if(bind(listenSocketID, p->ai_addr, (int)p->ai_addrlen) == 0) {
      // it managed to get a successful binding
      break;
    } else {
      // close this failed descriptor
      // and consider next element of the list
      close(listenSocketID);
    }
  }

  if (NULL == p) {
    perror("[create_and_bind_listen_socket]ERROR: cannot bind");
    exit(-1);
  }

  // free the list of structures
  freeaddrinfo(servinfo);
  
  return listenSocketID;
}

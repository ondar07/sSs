
#ifndef _SETUP_H_
#define _SETUP_H_

#include "log.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

// for printing some additional information
//#define DEBUG

#define FALSE 0
#define TRUE  1

#define PORT (srv_settings.port)
#define MAXEVENTS 128
#define MAXCONNECTIONS 128   // max number of connections for listening
#define WWWROOT (srv_settings.wwwroot)		// wwwroot dir
#define MIME_FILE (srv_settings.mime_file)
#define GENERATED_HTMLS (srv_settings.generated_htmls_dir)
#define ICONS_FOR_TYPES ("icons_for_types")
#define DB_NAME "wwwroot/icons_for_types/icons_for_types.db"
#define WWWROOT_PAGE "index.html"

typedef struct _server_settings {
  char *port;
  char *wwwroot;
  char *generated_htmls_dir;
  char *icons_db_path;        // relative to current directory of server
  FILE *mime_file;
} server_settings;

server_settings srv_settings;


// configure the server
// @config_file -- a path to config file for server
int init_server(const char *config_file);
void deinit_server();

// returns: socket descriptor for ListenSocket
int create_and_bind_listen_socket();



#endif // _SETUP_H_


#include "setup.h"
 
extern void start_server();

// you may specify a path to a config file for the server
// or use default "config" file in this directory
int main(int argc, char **argv) {
  const char *config_file_path = (argc > 1) ? argv[1] : "./config";

  if (init_server(config_file_path) < 0)
    return -1;

  start_server();

  deinit_server();
  return 0;
}

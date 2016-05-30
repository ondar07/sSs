
#include "setup.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

extern void start_server();

#define USE_LOG_FILE

#ifdef USE_LOG_FILE

FILE *logfp;

#define LOG_FILE_NAME "LOGS"

int open_log_file() {
  // see this descriptor in log.h
  logfp = fopen(LOG_FILE_NAME, "w+");
  if (logfp == NULL) {
    printf("ERROR: cannot open log file\n");
    exit(-1);
  }
  return 0;
}
#endif

// you may specify a path to a config file for the server
// or use default "config" file in this directory
int main(int argc, char **argv) {      
  /* Our process ID and Session ID */
  pid_t pid, sid;
        
  /* Fork off the parent process */
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  /* If we got a good PID, then
      we can exit the parent process. */
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* Change the file mode mask */
  umask(0);

  /* Open any logs here */        
#ifdef USE_LOG_FILE
  open_log_file();
#endif
  

  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
    /* Log the failure */
    exit(EXIT_FAILURE);
  }

  /* Change the current working directory */
  #if 0
  if ((chdir("/")) < 0) {
    /* Log the failure */
    exit(EXIT_FAILURE);
  }
  #endif
 
  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
        
  /* Daemon-specific initialization goes here */
        
  /* The Big Loop */
  //while (1)
  {
    /* Do some task here ... */
    const char *config_file_path = (argc > 1) ? argv[1] : "config";

    if (init_server(config_file_path) < 0)
      return -1;

    start_server();

    deinit_server();
  }
  return 0;
}

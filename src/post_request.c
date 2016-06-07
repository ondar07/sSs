
#include "setup.h"
#include "ext_epoll_data.h"


extern int read_word_from_req_into_buf(char *original_req, char *buf, size_t *cur_pos, size_t max);
extern ssize_t send_warning_msg(char *message, int socket_fd);

#define MEM_ZERO(ptr, size) memset(ptr, '\0', sizeof(char) * size)


//
//
static char *get_value_from_req(int sfd, char *header, char *value_type, size_t value_length) {
  char *value_ptr;
  char *value;
  size_t cur_pos = 0;

  value_ptr = strstr(header, value_type);
  if (!value_ptr) {
    send_warning_msg("incorrect post request\n", sfd);
    return NULL;
  }

  // now get the value 
  value_ptr += strlen(value_type);
  value = (char *) malloc(sizeof(char) * value_length);
  if (!value) {
#ifdef DEBUG
    PRINT("[get_boundary_value]OUT of memory\n");
#endif
    send_warning_msg("please, try later\n", sfd);
    return NULL;
  }

  MEM_ZERO(value, value_length);

  if (read_word_from_req_into_buf(value_ptr, value, &cur_pos, value_length) < 0) {
    free(value);
#ifdef DEBUG
    PRINT("[get_boundary_value]read_word_from_req_into_buf\n");
#endif
    send_warning_msg("please, try later\n", sfd);
    return NULL;
  }

  return value;
}



//
//
static char *get_boundary_value(int sfd, char *header) {

#define BOUNDARY_LENGTH 128
#define BOUNDARY_SIGN "boundary="

  return get_value_from_req(sfd, header, BOUNDARY_SIGN, BOUNDARY_LENGTH);

#undef BOUNDARY_LENGTH
#undef BOUNDARY_SIGN
}

//
//
static size_t get_content_length(int sfd, char *header) {
  char *cont_len;
  long long length;

#define CONTENT_LENGTH 64
#define CONTENT_LENGTH_SIGN "Content-Length: "

  cont_len = get_value_from_req(sfd, header, CONTENT_LENGTH_SIGN, CONTENT_LENGTH);
  if (!cont_len) {
    return -1;
  }
  length = strtoll(cont_len, NULL, 0);

  free(cont_len);

#undef CONTENT_LENGTH
#undef CONTENT_LENGTH_SIGN

  return length;
}


//
//
static int restore_cur_dir(char *cur_dir_path) {
  if (chdir(cur_dir_path) < 0) {
#ifdef DEBUG
    PRINT("[restore_cur_dir]ERROR on chdir %s (errno=%d)\n", cur_dir_path, errno);
#endif
    return -1;
  }
  return 0;
}

//
// change current directory to one, which is specified in POST request
// and return current dir (to restore dir later)
static char *ch_resource_dir(int sfd, char *header) {
  char *dir_path;  // where to store a file
  char *cwd;

#define DIR_PATH_LENGTH 1024
#define DIR_PATH_SIGN "POST "

  dir_path = get_value_from_req(sfd, header, DIR_PATH_SIGN, DIR_PATH_LENGTH);
  if (!dir_path) {
#ifdef DEBUG
    PRINT("[open_file]ERROR: get dir_path\n");
#endif
    return NULL;
  }


#define CUR_DIR_LENGTH 1024
  cwd = (char *) malloc(sizeof(char) * CUR_DIR_LENGTH);
  if (!cwd) {
#ifdef DEBUG
    PRINT("[open_file]ERROR: out of memory for cwd\n");
#endif
    goto free_dir_path;
  }
  MEM_ZERO(cwd, CUR_DIR_LENGTH);

  // 1. get current directory
  if (getcwd(cwd, CUR_DIR_LENGTH) == NULL) {
    PRINT("[open_file]ERROR: current working dir\n");
    goto free_dir_path;
  }

  // 2. change dir
  if (chdir(WWWROOT) < 0) {
#ifdef DEBUG
    PRINT("[ch_resource_dir]chdir WWWROOT\n");
#endif
    free(cwd);
    cwd = NULL;
    goto free_dir_path;
  }

  // if path is only "/"
  // it will mean that we should create a file in WWWROOT directory
  if (strcmp(dir_path, "/") != 0) {
    // all resource pathes begin with '/'
    // so we must ignore first '/' symbol
    if (chdir(dir_path + 1) < 0) {
#ifdef DEBUG
      PRINT("[ch_resource_dir]chdir %s\n", dir_path + 1);
#endif
      // restore
      if (restore_cur_dir(cwd) < 0) {
        send_warning_msg("Please, try later\n", sfd);
      }
      free(cwd);
      cwd = NULL;
      goto free_dir_path;
    }
  }

free_dir_path:
  if (dir_path)
    free(dir_path);
  return cwd;

#undef DIR_PATH_LENGTH
#undef DIR_PATH_SIGN
#undef CUR_DIR_LENGTH
}

//
// get filename from @request (it is placed after filename)
static char *get_filename(char *request, int sfd) {
  char *filename = NULL;
  char *ptr;    // pointer to " symbol at the end of @filename

#define FILENAME_LENGTH 128
#define FILENAME_SIGN "filename=\""

  filename = get_value_from_req(sfd, request, FILENAME_SIGN, FILENAME_LENGTH);
  if (!filename) {
#ifdef DEBUG
    PRINT("[get_filename] ERROR: out of memory for filename\n");
#endif
    return NULL;
  }

  // we should remove ' " ' symbol at the end
  if ((ptr = strstr(filename, "\"")) != NULL) {
    *ptr = '\0';
  }

#undef FILENAME_LENGTH
#undef FILENAME_SIGN

  return filename;
}

#define CRLFCRLF "\r\n\r\n"

static FILE *save_data(char *request, char *boundary, int sfd, Node_t *node) {
  FILE *fp = NULL;
  char *start;      // the beginning of the file
  char *end;
  size_t data_length;
  size_t i;

  if (node->data.fp == NULL) {
    char *filename;

    filename = get_filename(request, sfd);
    if (!filename) {
      send_warning_msg("Incorrect post request (or try later, please)\n", sfd);
      return NULL;
    }
    // for the first time, so
    // open a file (create if it doesn't exist yet)
    // mode "a" to write at the end of the file
    fp = fopen(filename, "a");
  }

  // find beginning of the data
  if ((start = strstr(request, CRLFCRLF)) == NULL) {
    goto error_so_close;
  }
  if ((end = strstr(start, boundary)) == NULL) {
    goto error_so_close;
  }
  end -= (strlen("--") + strlen(CRLFCRLF) + 1);

  data_length = (size_t)(end - start);
  // save a file
  if (fwrite(start, sizeof(char), data_length, fp) < 0) {
#ifdef DEBUG
    PRINT("[] ERROR \n");
#endif
    send_warning_msg("Cannot save a file\n", sfd);
    goto error_so_close;
  }

  //
  return fp;

error_so_close:
  fclose(fp);
  return NULL;
}

#undef CRLFCRLF

//
// return NULL, to close connection
static FILE *open_file_and_save_data(char *request, char *boundary, int sfd, Node_t *node) {
  char *cur_dir = NULL;
  FILE *fp = NULL;

  if (node->data.fp != NULL) {
    // it means that a file is open yet
    // in previous cases
    // so we don't need to change dir
    goto save_file;
  }
  // open dir
  cur_dir = ch_resource_dir(sfd, node->data.header);
  if (cur_dir == NULL) {
    send_warning_msg("Cannot find this directory (please, try later)\n", sfd);
    return NULL;
  }

save_file:
  // save file data
  if ( (fp = save_data(request, boundary, sfd, node)) == NULL ) {
    send_warning_msg("Incorrect post request (or try later, please)\n", sfd);
    return NULL;
  }

  if (cur_dir == NULL) {
    // it means that we don't change dir, so goto to return_fp
    goto return_fp;
  }


  // if we had changed dir, then we should restore it
restore_dir:
  if (restore_cur_dir(cur_dir) < 0) {
    send_warning_msg("ERROR on the server: cannot restore current directory\n", sfd);
    return NULL;
  }

return_fp:
  if (node->data.fp == NULL) {
    node->data.fp = fp;
  }
  return node->data.fp;
}


//
// @request           -- data of file
// @node->data.header -- header of POST request (it is filled for the first time in handle())
//
// if this function returns 0, the connection will be closed
// -1 => the connection will be kept open yet
int recv_file(char *request, int sfd, Node_t *node) {
  char *boundary;
  long long content_length;
  int res = -1;
  FILE *fp;

  if (node->data.status == REQUEST_COMPLETED) {
    return 0;
  }

  if (!request) {
    // 
    return -1;
  }

  boundary = get_boundary_value(sfd, node->data.header);
  if (!boundary) {
    return 0;
  }
  content_length = get_content_length(sfd, node->data.header);
  if (content_length < 0) {
    res = 0;
    goto free_boundary;
  }

#ifdef DEBUG
  PRINT("[recv_file]start of handling\n");
  PRINT("Boundary=%s\n", boundary);
  PRINT("Content-Length=%lld\n", content_length);
  PRINT("[recv_file]end of handling\n");
#endif
  

  if ((fp = open_file_and_save_data(request, boundary, sfd, node)) == NULL) {
    // to close connection
    res = 0;
    goto free_boundary;
  }
  node->data.fp = fp;
  
  // check is it end of request
  if (node->data.content_length == 0) {
    // for the first time
    node->data.content_length += strlen(node->data.header);
  }
  node->data.content_length += strlen(request);

  if (node->data.content_length >= content_length) {
    // it means that we save all data
    // so we can close connection
    res = 0;
#ifdef DEBUG
    PRINT("[recv_file]DEBUG: end of file");
#endif
  }
  
free_boundary:
  if (boundary)
    free(boundary);
  if (res == 0) {
    send_warning_msg("File added successfully.", sfd );
    if (node->data.fp != NULL) {
      fclose(node->data.fp);
    }
    return 0;
  }

  return -1;
}

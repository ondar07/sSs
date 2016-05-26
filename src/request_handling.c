
#include "request_handling.h"
#include <dirent.h>


#define GET_REQUEST      0
#define HEAD_REQUEST     1
#define UNKNOWN_REQUEST -1

#define MEM_ZERO(ptr, size) memset((ptr), '\0', size * sizeof(char));

static void handle_http_GET(char *request, size_t *cur_pos, int sfd);

// this function reads word, skipping '\t', ' ', '\n', '\r' 
// @original_req -- a pointer to original request string
// @buf          -- a pointer to buf, where read word should be copied
// @*cur_pos     -- current position in @original_req, from which function starts to read
//                  also the function saves new position (next word position in @original_req) in it
// @max          -- maximum number chars to read (max length of @buf)
//
// return:
//    if SUCCESS
//      0
//    else
//      -1
static int read_word_from_req_into_buf(char *original_req, char *buf, size_t *cur_pos, size_t max) {
  size_t read_chars_count = 0;
  size_t i;
  size_t original_req_len = strlen(original_req);

  if (*cur_pos >= original_req_len) {
    //PRINT("[analyze_and_copy_request]start >= length of original_req string\n");
    return -1;
  }

  for (i = *cur_pos; i < original_req_len; i++) {
    if (original_req[i] != '\t' && original_req[i] != ' ' && original_req[i] != '\n' && original_req[i] != '\r') {
      if (read_chars_count < max - 1) {
        buf[read_chars_count++] = original_req[i];
      }
    }
    else
      break;
  }
  buf[read_chars_count] = '\0';

  // find next word beginning
  i++;
  for ( ; i < original_req_len; i++) {
    if ( original_req[i] != '\t' && original_req[i] != ' ' && original_req[i] != '\n' && original_req[i] != '\r')
      break;
  }

  *cur_pos = i;
  return 0;
}

// @cur_pos -- current position in @request
// return:
//    request type (GET, HEAD or UNKOWN)
#define METHOD_TYPE_LENGTH 5
static int get_request_type(char *request, size_t *cur_pos) {
  char requestMethodType[METHOD_TYPE_LENGTH];  // buffer where method type will be saved by read_word_from_req_into_buf() function

  memset(requestMethodType, 0, METHOD_TYPE_LENGTH);

  // this function expects that cur_pos == 0
  // to read @request from beginning (because http type is located at the beginning)
  if (*cur_pos != 0) {
    PRINT("[get_request_type]function should be called with cur_pos == 0, but now cur_pos = %zu", *cur_pos);
    return UNKNOWN_REQUEST;
  }

  if (read_word_from_req_into_buf(request, requestMethodType, cur_pos, METHOD_TYPE_LENGTH) < 0) {
    return UNKNOWN_REQUEST;
  }

  if (strcmp("GET", requestMethodType) == 0) {
    return GET_REQUEST;
  }
  else if (strcmp("HEAD", requestMethodType) == 0) {
    return HEAD_REQUEST;
  } else {
    return UNKNOWN_REQUEST;
  }
}
#undef METHOD_TYPE_LENGTH

// What should server do with a request?
// 1. find out a type of request (GET / HEAD / etc)
// 2. handle the request and see if the needed resource exists
// 3. read its content
// 4. send this content correct file type
void handle(char *request, int sfd) {
  int request_type;
  size_t cur_pos = 0;   // current position (offset) in @request for next reading
                        // at the beginning it equal to 0

  request_type = get_request_type(request, &cur_pos);

  switch(request_type) {

    case GET_REQUEST :
#ifdef DEBUG
      PRINT("GET request on sfd=%d\n", sfd);
#endif
      handle_http_GET(request, &cur_pos, sfd);
      break;
  
    case HEAD_REQUEST :
      
#ifdef DEBUG
      PRINT("HEAD request on sfd=%d\n", sfd);
#endif
      ;// TODO: send a header
      break;

    default :
#ifdef DEBUG
      PRINT("UNKNOWN_REQUEST\n");
      //TODO: send ("400 Bad Request\n", sfd);
#endif
  }
}


#define HTTP_1_0  0
#define HTTP_1_1  1

#define FILE_NAME_LENGTH    200
#define PATH_LENGTH         1000
#define EXTENSION_LENGTH    10
#define MIME_LENGTH         200
#define HTTP_VERSION_LENGTH 20


// return:
//    int value (see definition above) for HTTP/1.1 and HTTP/1.0
//    -1, otherwise
static int get_http_version(char *request, size_t *cur_pos) {
  char *http_version = (char *)malloc(HTTP_VERSION_LENGTH * sizeof(char));
  int version_type = -1;

  MEM_ZERO(http_version, HTTP_VERSION_LENGTH);
  if (read_word_from_req_into_buf(request, http_version, cur_pos, FILE_NAME_LENGTH) < 0) {
    PRINT("ERROR: [get_http_version]couldn't read http version\n");
    return -1;
  }

  if (strcmp("HTTP/1.1" , http_version) == 0) {
    #ifdef DEBUG
    PRINT("HTTP/1.1\n");
    #endif
    version_type = HTTP_1_1;
  } else if (strcmp("HTTP/1.0" , http_version) == 0) {
    #ifdef DEBUG
    PRINT("HTTP/1.0\n");
    #endif
    version_type = HTTP_1_0;
  }

  free(http_version);
  return version_type;
}

// if file extension is represented in @filename, 
// this function will read it and save it into @extension
// return:
//    if extension is represented
//      0
//    else
//      -1
int get_extension(char *filename, char *extension, size_t max_extension_length) {
  int is_there_extension = FALSE;
  size_t i;
  size_t read_chars_count = 0;
  size_t filename_length = strlen(filename);

  for (i = 0; i < filename_length; i++ ) {
    if (filename[i] == '.') {
      i++; // should read from next char (extension without '.')
      is_there_extension = TRUE;
      break;
    }
  }

  if(!is_there_extension)
    goto NOT_extension;

  
  for ( ; i < filename_length; i++ ) {
    if(read_chars_count < max_extension_length) {
        extension[read_chars_count++] = filename[i];
    }
  }

  extension[read_chars_count] = '\0';

  #ifdef DEBUG
  PRINT("[get_extension]Extension =%s\n", extension);
  #endif
  return 0;

NOT_extension:
  #ifdef DEBUG
  PRINT("[get_extension]Extension is NOT represented\n");
  #endif
  return -1;
}

// offset относительно position
// position can be
//   SEEK_SET (beginning of a file)
//   SEEK_CUR (current file position indicator)
//   SEEK_END (end of file)
#define SET_FILE_POSITION(filestream, offset, position)   \
  do {                                                    \
    if (fseek((filestream), (offset), (position)) < 0) {  \
      PRINT("ERROR: in fseek \n");                        \
    }                                                     \
  } while(0)

// check if @extension is supported
// if it is supported, @mime_type will be filled by its mime type and return 0
// else return -1
static int check_mime_support(char *extension, char *mime_type) {

  // TODO: this function should use mmaped file pointer
  // also see TODO in setup.c (function fopen_mime_file)

#define FILE_LINE_LENGTH (MIME_LENGTH + EXTENSION_LENGTH)
  char *line; // TODO: if mmaped file pointer is used, this var will not be needed
  char *mimetype;
  char *ext;
  int support = -1; // -1 means "not supported", 0 means "supported"

  if (strcmp(extension, "") == 0) {
    #ifdef DEBUG
    PRINT("[check_mime_support]extension is empty string\n");
    PRINT("it's likely to be a dir\n");
    #endif
    return -1;
  }

  line = (char *)malloc( (FILE_LINE_LENGTH) * sizeof(char)); // TODO: if mmaped file pointer is used, this var will not be needed
  mimetype = (char *)malloc(MIME_LENGTH * sizeof(char));
  ext = (char *)malloc(EXTENSION_LENGTH * sizeof(char));


  memset(line, '\0', FILE_LINE_LENGTH );
  memset(mimetype,'\0',MIME_LENGTH);
  memset(ext, '\0', EXTENSION_LENGTH);

  // see MIME_FILE in setup.h
  SET_FILE_POSITION(MIME_FILE, 0, SEEK_SET);
  while (fgets(line, FILE_LINE_LENGTH, MIME_FILE) != NULL) {
    size_t cur_pos = 0;   // read 'mime.types' file from beginning

    if ( line[0] == '#' ) {
      // it is comment
      memset(line, '\0', FILE_LINE_LENGTH );
      continue;
    }

    // read mimetype from 'mime.types' one line
    // and store it into @mimetype
    // also after this function @cur_pos stores a start position of extension in @line
    read_word_from_req_into_buf(line, mimetype, &cur_pos, MIME_LENGTH);

    // read extension into @ext
    if (read_word_from_req_into_buf(line, ext, &cur_pos, EXTENSION_LENGTH) < 0) {
      // it means that an extension is not presented in 'mime.types' file
      continue;
    }

    if ( strcmp(ext, extension) == 0 ) {
      // mimetype contains  'application/andrew-inset', for example
      memcpy(mime_type, mimetype, strlen(mimetype));
      support = 0;
      break;
    }

    memset(line, '\0', FILE_LINE_LENGTH );
    memset(mimetype,'\0',MIME_LENGTH);
    memset(ext, '\0', EXTENSION_LENGTH);
  }

#undef FILE_LINE_LENGTH

  free(line);
  free(mimetype);
  free(ext);
  PRINT("[check_mime_support]%s\n", mime_type);
  return support;
}

// send @bytes with @length
// on socket @socket_fd
static ssize_t send_bytes(char *bytes, size_t length, int socket_fd) {
  ssize_t bytes_sent;

  // flags is 0!
  bytes_sent = send(socket_fd, bytes, length, 0);

#ifdef DEBUG
  //PRINT("\nSEND msg: %s\n", bytes);
#endif

  // TODO: analyze errno!
  if (bytes_sent == -1)
    PRINT("[events_handling]ERROR: cannot send a reply to client with fd=%d\n", socket_fd);
      
  return bytes_sent;
}

static ssize_t send_warning_msg(char *message, int socket_fd) {
  size_t msg_length = strlen(message);
  return send_bytes(message, msg_length, socket_fd);
}

//
// form header and send it to client
//
// @status_code -- 200 if resource is available
//              -- 404 if resource is NOT found
// 
// 
static ssize_t send_header(char *http_version, char *status_code, char *content_type, long content_length, int socket) {
  char *content_head = "\r\nContent-Type: ";
  char *server_head = "\r\nServer: sSs";
  char *length_head = "\r\nContent-Length: ";
  //char *date_head = "\r\nDate: ";
  char contentLength[64];
  
  char *header_end = "\r\n\n";  // 
  char *message;
  ssize_t res;

  //time_t rawtime;

  //time ( &rawtime );

  sprintf(contentLength, "%ld", content_length);

  message = (char *)malloc( (
    strlen(http_version) + 1 +  // "+1" means a ' ' space after @http_version (for example, "HTTP/1.1 ")
    strlen(content_head) +
    strlen(server_head) +
    strlen(length_head) +
    strlen(status_code) +
    strlen(content_type) +
    strlen(contentLength) +
    strlen(header_end)) * sizeof(char) );


  if (!message) {
    PRINT("[send_header]ERROR: out of memory for message header\n");
    return -1;
  }

  strcpy(message, http_version);
  strcat(message, " ");

  strcat(message, status_code);

  strcat(message, content_head);
  strcat(message, content_type);
  strcat(message, server_head);
  strcat(message, length_head);
  strcat(message, contentLength);
  strcat(message, header_end);

  res = send_bytes(message, strlen(message), socket);
  PRINT("\nHEADER:\n%s", message);
  free(message);

  return res;
}

//
//
//
static void send_file(FILE *fp, int sfd) {
  char current_char;
  ssize_t bytes_sent;
  ssize_t bytes_read;
  ssize_t total_bytes_sent = 0;

  while ( (bytes_read = fread(&current_char, sizeof(char), 1, fp) ) != 0 ) {
    bytes_sent = send_bytes(&current_char, bytes_read, sfd);
    if (bytes_sent == -1) {
      PRINT("ERROR: [send_file] \n");
    }
    total_bytes_sent += bytes_sent;
  }
#ifdef DEBUG
  PRINT("total bytes sent = %zu\n", total_bytes_sent);
#endif
}

//
// find out file (@fp) size, set file position indicator to the beginning of @fp stream
// and return it
//
long get_file_size(FILE *fp) {
  long filesize = 0;

  SET_FILE_POSITION(fp, 0, SEEK_END);
  filesize = ftell(fp);

  rewind(fp); // set file position indicator to the beginning of @fp

  return filesize;
}

#define REGULAR_FILE      1
#define DIRECTORY         2
#define UNKOWN_FILE_TYPE  3

//
//
//
static int get_type_of_file(const char *path) {
  struct stat statbuf;

  if (access(path, F_OK) == -1) {
    // file doesn't exists
    return -1;
  }

  if (stat(path, &statbuf) < 0) {
    // some errors
    // for example, EACCES (permission denied)
    return -1;
  }

  if (S_ISREG(statbuf.st_mode))
    return REGULAR_FILE;
  if (S_ISDIR(statbuf.st_mode))
    return DIRECTORY;
  return UNKOWN_FILE_TYPE;
}

//
//
static void send_response_for_reg_file(char *file_path, char *http_version, char *content_type, int socket_fd) {
  FILE *fp;
  long content_length;

  fp = fopen(file_path, "rb");
  if ( fp == NULL ) {
    PRINT("Unable to open file %s\n", file_path);
    // TODO: send header with code 404
    send_warning_msg("404 file not found", socket_fd);
    return;
  }

  // 2. content-length
  content_length = get_file_size(fp);
  if (content_length < 0) {
    PRINT("[get_file_size]ERROR: ftell returned -1");
    send_warning_msg("ERROR: File size is zero\n", socket_fd);
    goto close_file;
  }

  // 3. send header
  // content_type = "application/octet-stream" for usual strings
  if (send_header(http_version, "200 OK", content_type, content_length, socket_fd) == -1) {
    send_warning_msg("ERROR: server problem with sending header\n", socket_fd);
    goto close_file;
  }

  // 4. read file and send it
  send_file(fp, socket_fd);

close_file:
  fclose(fp);

}


// see html_generation_for_dir.c
extern int need_to_generate_html_for_dir(char *dir_path, char *generated_html_name);
extern int generate_html_for_dir(char *dir_path, char *generated_html_name, char *dir_name);
extern char *generate_html_name(char *dir_name);

//
// this function send response to requests for directories
//
// @dir_path -- directory path (relative to WWWROOT dir)
// @dir_name -- directory name
static void send_response_for_dir(char *dir_path, char *dir_name, char *http_version, int socket_fd) {
  char *generated_html_name;

  // see in html_generation_for_dir.c
  // if success returns pointer to file name
  // else NULL
  generated_html_name = generate_html_name(dir_name);
  if (!generated_html_name) {
    send_warning_msg("Error on the server. Try later, please\n", socket_fd);
    return;
  }

#ifdef DEBUG
  PRINT("generated_html_name=%s\n", generated_html_name);
#endif

  // 1. check if 
  if (need_to_generate_html_for_dir(dir_path, generated_html_name) == 0)
    goto send_html_file;

  // 2.
  if (generate_html_for_dir(dir_path, generated_html_name, dir_name) < 0) {
    send_warning_msg("ERROR with dir ", socket_fd);
    send_warning_msg(dir_name, socket_fd);
    goto free_name;
  }

  // 3. send it
send_html_file:
  send_response_for_reg_file(generated_html_name, http_version, "text/html", socket_fd);

free_name:
  free(generated_html_name);
}


//
//
static void send_response(char *http_version, char *filename, char *content_type, int socket_fd) {
  char *full_file_path; // not full; relative to WWWROOT
  
  int file_type;

#ifdef DEBUG
  PRINT("[send_response] filename=%s\n", filename);
#endif

#define FULL_FILE_PATH_LENGTH (FILE_NAME_LENGTH + strlen(WWWROOT) + 1)
  full_file_path = (char *)malloc(FULL_FILE_PATH_LENGTH * sizeof(char));
  if (!full_file_path) {
    PRINT("[send_response](char *)joke_path is NULL\n");
    return;
  }

  memset(full_file_path, 0, FULL_FILE_PATH_LENGTH);
  full_file_path = strncat(full_file_path, WWWROOT, FULL_FILE_PATH_LENGTH);
  if ( filename[0] != '/' && strcmp(filename, "/") != 0 ) {
    full_file_path = strncat(full_file_path, "/", 1);
  }
  full_file_path = strncat(full_file_path, filename, FILE_NAME_LENGTH);

#ifdef DEBUG
  PRINT("[send_response] full_filename_path=%s\n", full_file_path);
#endif

  // define @filename type (by using full_file_path)
  file_type = get_type_of_file(full_file_path);

  switch (file_type) {
    case REGULAR_FILE :
      send_response_for_reg_file(full_file_path, http_version, content_type, socket_fd);
      break;
    case DIRECTORY :
      send_response_for_dir(full_file_path, filename, http_version, socket_fd);
      break;
    case UNKOWN_FILE_TYPE:
      send_warning_msg("file type is not supported\n", socket_fd);
      break;
    default :
      // couldn't define file type or 
      send_warning_msg("404 file not found", socket_fd);
  }
  
  free(full_file_path);
}


static void handle_http_GET(char *request, size_t *cur_pos, int sfd) {
  char *filename = (char *)malloc(FILE_NAME_LENGTH * sizeof(char));

  char *extension = (char *)malloc(EXTENSION_LENGTH * sizeof(char));
  char *mime = (char *)malloc(MIME_LENGTH * sizeof(char));

  int http_version;


  MEM_ZERO(filename, FILE_NAME_LENGTH);

  MEM_ZERO(extension, EXTENSION_LENGTH);
  MEM_ZERO(mime, MIME_LENGTH);
  

  if ( read_word_from_req_into_buf(request, filename, cur_pos, FILE_NAME_LENGTH) < 0 ) {
    PRINT("[handle_http_GET]couldn't read filename in request\n");
    return;
  }

  // 
  if ( (http_version = get_http_version(request, cur_pos)) < 0 ) {
    send_warning_msg("501 Not Implemented", sfd);
    return;
  }

  if (strcmp(filename, "/") == 0) {
    // TODO
    strcpy(filename, WWWROOT_PAGE);
  }
  
  if ( get_extension(filename, extension, EXTENSION_LENGTH) < 0 ) {
    PRINT("File extension isn't represented\n");
    //send_warning_msg("400 Bad Request\n", sfd);
  }

  // 
  if ( check_mime_support(extension, mime) < 0 )
  {
    PRINT("Mime not supported\n");
  }

send_response:
  send_response("HTTP/1.1", filename, mime, sfd);


free_buffers:
  free(filename);
  free(mime);
  free(extension);
}

#undef FILE_NAME_LENGTH
#undef PATH_LENGTH
#undef EXTENSION_LENGTH
#undef MIME_LEGNTH
#undef HTTP_VERSION_LENGTH

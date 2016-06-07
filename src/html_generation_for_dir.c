#include "setup.h"

#include <dirent.h>

//
// generate html name for @dir_name
// (for example: my_dir/another_dir -> my_dir_another_dir)
// 
// return:
//    if SUCCESS
//      a pointer to allocated memory, which contains the generated name
//    else
//       NULL
char *generate_html_name(char *dir_name) {
  char *generated_html_name;
  size_t i;
  size_t dir_name_length = strlen(dir_name);

#define GEN_HTML_NAME_LENGTH (strlen(GENERATED_HTMLS) + strlen(dir_name) + 1)
  generated_html_name = (char *)malloc( GEN_HTML_NAME_LENGTH * sizeof(char) );

  if (!generated_html_name) {
    PRINT("[generate_html_name]: out of memory for name\n");
    return NULL;
  }

  memset(generated_html_name, '\0', GEN_HTML_NAME_LENGTH );
  strcat(generated_html_name, GENERATED_HTMLS);
  for (i = 0; i < dir_name_length; i++) {
    if (i == (dir_name_length - 1) && dir_name[i] == '/') {
      // if the last symbol is '/'
      // we shouldn't copy this symbol into @generated_html_name
      break;
    }
    if (i > 0 && dir_name[i] == '/') {
      // this @dir_name (dir_path) can contain names of other directories
      // so we replace '/' symbols with '_'
      char symbol = '_';
      strncat(generated_html_name, &symbol, sizeof(char));
      continue;
    }
    strncat(generated_html_name, dir_name + i, sizeof(char));
  }
  return generated_html_name;
}

#undef GEN_HTML_NAME_LENGTH 

//
// this function is used by send_response_for_dir() function
// it checks if 
//
//
int need_to_generate_html_for_dir(char *dir_path, char *generated_html_name) {
  return 1;
  
#if 0
  char *temp_html_name;
  struct stat dir_stat;
  struct stat html_file_stat;
  int need = 0;


  // check if corresponding html exists already 
  if (access(temp_html_name, F_OK) == -1) {
    // html file for this dir doesn't exists
    // so we need to generate it
    need = 1;
  }

free:
  free(temp_html_name);
  return need;
#endif
}

static int does_file_exist(char *file_path) {
  if (access(file_path, F_OK) == -1) {
    // file doesn't exists
    return FALSE;
  }
  return TRUE;
}

static void print_html_header(FILE *fp) {
  fprintf(fp, "<!DOCTYPE html>\n");
  fprintf(fp, "<html>\n");
  fprintf(fp, "<body>\n");
  fprintf(fp, "<form enctype=\"multipart/form-data\" method=\"post\">\n");
  fprintf(fp, "<p><input type=\"file\" name=\"f\">\n");
  fprintf(fp, "<input type=\"submit\" value=\"Send file\"></p>\n");
  fprintf(fp, "</form>\n");
}

static void print_html_end(FILE *fp) {
  fprintf(fp, "</body>\n");
  fprintf(fp, "</html>\n");
}

// see get_icon_path_from_db.c
extern char * get_icon_path(char *dir_path, char *filename);

//
// 
//
// @dir_path -- for example, (WWWROOT)/my_dir/another_dir
// @dir_name -- my_dir/another_dir (a path, relative to (WWWROOT) )
//
// returns 0 if success (-1 else)
int generate_html_for_dir(char *dir_path, char *generated_html_name, char *dir_name) {
  DIR *dp;
  FILE *fp;
  struct dirent *ep;
  int res = 0;

  if ( (dp = opendir(dir_path) ) == NULL ) {
    PRINT("Couldn't open the directory %s\n", dir_path);
    return -1;
  }

  #ifdef DEBUG
  PRINT("generated_html_name =%s\n", generated_html_name);
  #endif
  // create a file for @generated_html_name
  fp = fopen(generated_html_name, "w+");
  if (fp == NULL) {
    PRINT("[generate_html_for_dir]ERROR: file %s not created\n", generated_html_name);
    res = -1;
    goto close_dir;
  }

  print_html_header(fp);

  fprintf(fp, "<ul>\n");

  // read each entry in @dir_path
  while ( (ep = readdir(dp)) != NULL ) {
    if (strcmp(ep->d_name, ".") == 0) {
      continue;
    }

    fprintf(fp, "<li>");

    // get icon path
    char *icon_path = get_icon_path(dir_path, ep->d_name);

    if (icon_path) {
      #ifdef DEBUG
      PRINT("%s\n", dir_path);
      PRINT("[generate_html_for_dir] icCCCon_path=%s, for %s\n", icon_path, ep->d_name);
      #endif

      // set <img > tag with icon path

      // see ICONS_FOR_TYPES in setup.h
      if (ICONS_FOR_TYPES[0] != '/') {
        // we should set '/' symbol at the beginning
        // it allows to find icons in WWWROOT directory
        // else browser will try to find icons in current (uri) directory
        // and for nested directories we cannot find icons
        fprintf(fp, "<img src=/%s", ICONS_FOR_TYPES);
      }
      
      if ( ICONS_FOR_TYPES[strlen(ICONS_FOR_TYPES) - 1] != '/' ) {
        fprintf(fp, "/");
      }
      fprintf(fp, "%s height= \"40\" width= \"40 \" > \t", icon_path);

      // free icon path
      free(icon_path);
    } else {
      #ifdef DEBUG
      PRINT("[generate_html_for_dir]icon_path is NULL for %s\n", ep->d_name);
      #endif
    }

#ifdef DEBUG
    PRINT("DIR_NAME %s\n", dir_name);
#endif

    // write <a href=" "> for entry
    
    fprintf(fp, "<a href=\"%s", dir_name);
    if ( dir_name[strlen(dir_name) - 1] != '/' ) {
      fprintf(fp, "/");
    }
    fprintf(fp, "%s\">%s</a></li>\n", ep->d_name, ep->d_name);

  }

  fprintf(fp, "</ul>\n");
  print_html_end(fp);

  if (fclose(fp) != 0) {
    PRINT("[generate_html_for_dir]ERROR: fclose %s!\n", generated_html_name);
    res = -1;
  }

close_dir:
  if ( closedir(dp) < 0 ) {
    PRINT("[generate_html_for_dir]ERROR: closedir %s!\n", dir_path);
    res = -1;
  }

  return res;
}

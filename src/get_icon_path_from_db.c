
#include "setup.h"
#include <sqlite3.h>


// 
#define ICON_PATH_COLUMN_NAME path_to_icon
#define EXTENSION_COLUMN_NAME extension

int is_dir(const char *file_path) {
    struct stat statbuf;

    if (access(file_path, F_OK) == -1) {
        // file doesn't exists
        return -1;
    }

    if (stat(file_path, &statbuf) < 0) {
        // some errors
        // for example, EACCES (permission denied)
        return -1;
    }

    return S_ISDIR(statbuf.st_mode);
}

//
//
char * get_ext_in_filename(char *dir_path, char *filename) {
    size_t i;
    char *file_path;
#define FULL_FILE_PATH_LENGTH (strlen(dir_path) + strlen(filename) + strlen("/") + 1)

    file_path = (char *)malloc(FULL_FILE_PATH_LENGTH * sizeof(char));
    if (!file_path) {
#ifdef DEBUG
        PRINT("[get_ext_in_filename]ERROR: out of memory\n");
#endif
        return NULL;
    }

    // file file_path using @dir_path and @filename

    memset(file_path, '\0', FULL_FILE_PATH_LENGTH);
    file_path = strcat(file_path, dir_path);
    if (dir_path[strlen(dir_path) - 1] != '/')
        strcat(file_path, "/");
    file_path = strcat(file_path, filename);

    // check if this file is directory
    if (is_dir(file_path)) {

        // don't remember free allocated memory
        free(file_path);
        
        // @dir is "dir"
        char *dir = (char *)malloc( (strlen("dir") + 1) * sizeof(char));
        dir = strcpy(dir, "dir");
        dir[strlen(dir)] = '\0';
        return dir;
    }

    free(file_path);

    // strstr() returns a pointer to the start of substring (".") in @filename if the substring is found
    if (strstr(filename, ".") != NULL) {
        // beginning of file extension is replaced after ".", so +1 (sizeof(char)) 
        return strstr(filename, ".") + sizeof(char);
    }
    return NULL;
}

char * get_icon_path(char *dir_path, char *filename) {
    char *ext;
    sqlite3 *db;    // this structure defines db handle
    char *err_msg = 0;
    sqlite3_stmt *res;  // represents a single SQL statement (statement handle)
    char sql[] = "SELECT path_to_icon FROM Icons WHERE extension = ?";
    int step;
    char *icon_path = NULL;
    int rc;
    
    ext = get_ext_in_filename(dir_path, filename);
    if (!ext) {
#ifdef DEBUG
        PRINT("[get_icon_path] ext is NULL\n");
#endif
        return NULL;
    }
#ifdef DEBUG
    PRINT("EXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXT %s for %s\n", ext, filename);
#endif

    //return NULL;

    // open a new database connection
    rc = sqlite3_open(DB_NAME, &db);
    if (rc != SQLITE_OK) {

        // the connection with db was NOT established
        
        // sqlite3_errmsg() function returns a description of the error
        PRINT("[get_icon_path]ERROR: Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        goto free_memory;
    }

    // before a SQL statement is executed, 
    // it must be first compiled into a byte-code with one of the sqlite3_prepare* functions.
    // ( sqlite3_prepare() function is deprecated )
    // 
    // @sql -- the SQL statement to be compiled
    // -1   -- maximum length of the SQL statement measured in bytes
    //        (-1 means that the SQL string to be read up to the first zero terminator which is the end of the string here)
    // @res -- statement handle, it will point to the precompiled statement if the sqlite3_prepare_v2() runs successfully.
    // 0 (the last parameter) is a pointer to the unused portion of the SQL statement
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(res, 1, ext, strlen(ext), NULL);
    } else {
        PRINT("[get_icon_path]ERROR: failed to execute statement: %s\n", sqlite3_errmsg(db));
        goto free_memory;
    }
    
    // run the SQL statement
    // after that SQLITE_ROW return code indicates that there is another row ready.
    // Our SQL statement returns only one row of data, therefore, we call this function only once.
    step = sqlite3_step(res);
    
    if (step != SQLITE_DONE && step != SQLITE_ROW) {
        // there's probably a problem
        #ifdef DEBUG
        PRINT("ERROR in sqlite3_step!\n");
        #endif 
        sqlite3_finalize(res);
        sqlite3_close(db);
        goto free_memory;
    }
    PRINT("STEP returrrrrrrrrrrrrrrrrn %d\n", step);

    if (step == SQLITE_DONE) {
        // SELECT return 0 rows
#ifdef DEBUG
        PRINT("SELECT return 0 rows!\n");
#endif 
        sqlite3_finalize(res);
        sqlite3_close(db);
        goto free_memory;
    } 
    else
    if (step == SQLITE_ROW) {
        printf("SSSSQQQQLITE RAW=%s\n ", sqlite3_column_text(res, 0));

// strings returned by sqlite3_column_text() and sqlite3_column_text16(), even empty strings, are always zero-terminated
#define COL_VALUE_LEN (strlen(sqlite3_column_text(res, 0)) + 1)

        icon_path = (char *)malloc(COL_VALUE_LEN * sizeof(char));
        if (icon_path == NULL) {
            sqlite3_finalize(res);
            sqlite3_close(db);
            goto free_memory;
        }

        memset(icon_path, '\0', COL_VALUE_LEN);
        strcat(icon_path, sqlite3_column_text(res, 0));
    }

    // destroys the prepared statement object
    sqlite3_finalize(res);
    sqlite3_close(db);

#ifdef DEBUG
    if(icon_path)
    PRINT("FRRRRRRRRRRRRROM SQLITE: %s for %s\n", icon_path, filename);
#endif
free_memory:
    if (strcmp(ext, "dir") == 0)
        free(ext);
    return icon_path;
}



#include <stdio.h>
#include <stdlib.h>
#include "ext_epoll_data.h"
#include "log.h"

// create a new list
List_t * list_new() {
  List_t *list;

  list = (List_t *)malloc(sizeof(List_t));
  list->head = NULL;
  return list;
}

// remove all elements of @l and @l
void list_delete(List_t *l) {
  Node_t *a, *b;

  if(!l)
    return;
  a = l->head;
  while(a) {
    b = a->next;
    free(a);
    a = b;
  }
  free(l);
}

static int is_empty(List_t *l) {
  return !l->head;
}


// find element in @l with data.fd being equal to @fd
Node_t *find_node(List_t *l, int fd) {
  Node_t *cur;

  if( !l || is_empty(l) )
    return NULL;
  for(cur = l->head; cur != NULL; cur = cur->next) {
    if(cur->data.sfd == fd)
      return cur;
  }

  return NULL;
}


// insert a new Node with @data
// if element with @data.fd exists already,
// this function will NOT insert and return -1 
//
// return 0 if successful insertion
int insert_node(List_t *l, ext_epoll_data_t data) {
  Node_t *n;

  // find element
  n = find_node(l, data.sfd);
  if (n != NULL) {
    PRINT("[insert] element with fd=%d exists already in list\n", data.sfd);
    return -1;
  }

  // if element doesn't exist yet
  n = (Node_t *)malloc(sizeof(Node_t));
  if (n == NULL) {
    PRINT("[insert] out of memory for element with fd=%d \n", data.sfd);
    return -1;
  }
  n->data = data;
  // if insert first element l->head == NULL
  // so this code will be correct in any way
  n->next = l->head;
  l->head = n;
  return 0;
}

// remove all elements with this @fd
void remove_node(List_t *l, int fd) {
  Node_t *n, *cur, *prev;

  // find element
  n = find_node(l, fd);
  if (!n)
    return;

  // remove it from list
  prev = l->head;
  for (cur = l->head; cur != NULL; cur = cur->next) {
    if (cur == n) {
      if (n == l->head) {
        // if this element is single in the list
        // l->head will be NULL (it means it will be empty)
        l->head = n->next;
      } else {
        prev->next = n->next;
      }
      free(n);
      // in the list only one element with this fd may exist
      // so break
      break;
    }

    prev = cur;
  }
}
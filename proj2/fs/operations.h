#ifndef FS_H
#define FS_H
#include "state.h"
#include "../stack.h"

#define FALSE 0
#define TRUE 1
#define CREATE 0
#define DELETE 0
#define MOVE 1

void init_fs();
void destroy_fs();
int is_dir_empty(DirEntry *dirEntries);
int create(char *name, type nodeType);
int delete(char *name);
int lookup(char *name);
int lookup_aux(char *name, Stack stack, int flag);
int move(char* orig, char* dest);
void print_tecnicofs_tree(FILE *fp);
void unlock(Stack stack);
int rdlock(int inumber);
int wrlock(int inumber);

#endif /* FS_H */

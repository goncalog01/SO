#include "operations.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

extern inode_t inode_table[INODE_TABLE_SIZE];

/* Given a path, fills pointers with strings for the parent path and child
 * file name
 * Input:
 *  - path: the path to split. ATENTION: the function may alter this parameter
 *  - parent: reference to a char*, to store parent path
 *  - child: reference to a char*, to store child file name
 */
void split_parent_child_from_path(char * path, char ** parent, char ** child) {

	int n_slashes = 0, last_slash_location = 0;
	int len = strlen(path);

	// deal with trailing slash ( a/x vs a/x/ )
	if (path[len-1] == '/') {
		path[len-1] = '\0';
	}

	for (int i=0; i < len; ++i) {
		if (path[i] == '/' && path[i+1] != '\0') {
			last_slash_location = i;
			n_slashes++;
		}
	}

	if (n_slashes == 0) { // root directory
		*parent = "";
		*child = path;
		return;
	}

	path[last_slash_location] = '\0';
	*parent = path;
	*child = path + last_slash_location + 1;

}


/*
 * Initializes tecnicofs and creates root node.
 */
void init_fs() {
	inode_table_init();
	
	/* create root inode */
	int root = inode_create(T_DIRECTORY);
	pthread_rwlock_unlock(&inode_table[root].rwlock);
	
	if (root != FS_ROOT) {
		printf("failed to create node for tecnicofs root\n");
		exit(EXIT_FAILURE);
	}
}


/*
 * Destroy tecnicofs and inode table.
 */
void destroy_fs() {
	inode_table_destroy();
}


/*
 * Checks if content of directory is not empty.
 * Input:
 *  - entries: entries of directory
 * Returns: SUCCESS or FAIL
 */

int is_dir_empty(DirEntry *dirEntries) {
	if (dirEntries == NULL) {
		return FAIL;
	}
	  
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		if (dirEntries[i].inumber != FREE_INODE) {
			return FAIL;
		}
	}
	   
	return SUCCESS;
}


/*
 * Looks for node in directory entry from name.
 * Input:
 *  - name: path of node
 *  - entries: entries of directory
 * Returns:
 *  - inumber: found node's inumber
 *  - FAIL: if not found
 */
int lookup_sub_node(char *name, DirEntry *entries) {
	if (entries == NULL) {
		return FAIL;
	}

	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (entries[i].inumber != FREE_INODE && strcmp(entries[i].name, name) == 0) {
			return entries[i].inumber;
        }
    }

	return FAIL;
}


/*
 * Creates a new node given a path.
 * Input:
 *  - name: path of node
 *  - nodeType: type of node
 * Returns: SUCCESS or FAIL
 */
int create(char *name, type nodeType){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	Stack stack = STACKinit(STACK_SIZE);
	/* use for copy */
	type pType;
	union Data pdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup_aux(parent_name, stack, CREATE);

	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n",
		        name, parent_name);
		unlock(stack);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n",
		        name, parent_name);
		unlock(stack);
		return FAIL;
	}

	if (lookup_sub_node(child_name, pdata.dirEntries) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n",
		       child_name, parent_name);
		unlock(stack);
		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
	
	child_inumber = inode_create(nodeType);
	
	if (child_inumber != FAIL)
		STACKpush(stack, child_inumber);
	else {
		printf("failed to create %s in %s, couldn't allocate inode\n",
		        child_name, parent_name);
		unlock(stack);
		return FAIL;
	}

	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);
		unlock(stack);
		return FAIL;
	}
	unlock(stack);
	return SUCCESS;
}


/*
 * Deletes a node given a path.
 * Input:
 *  - name: path of node
 * Returns: SUCCESS or FAIL
 */
int delete(char *name){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	Stack stack = STACKinit(STACK_SIZE);
	/* use for copy */
	type pType, cType;
	union Data pdata, cdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup_aux(parent_name, stack, DELETE);

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n",
		        child_name, parent_name);
		unlock(stack);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n",
		        child_name, parent_name);
		unlock(stack);
		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries);

	if (child_inumber != FAIL) {
		wrlock(child_inumber);
		STACKpush(stack, child_inumber);
	}
	else {
		printf("could not delete %s, does not exist in dir %s\n",
		       name, parent_name);
		unlock(stack);
		return FAIL;
	}

	inode_get(child_inumber, &cType, &cdata);

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n",
		       name);
		unlock(stack);
		return FAIL;
	}

	/* remove entry from folder that contained deleted node */

	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		unlock(stack);
		return FAIL;
	}

	if (inode_delete(child_inumber) == FAIL) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
		unlock(stack);
		return FAIL;
	}
	unlock(stack);
	return SUCCESS;
}

/*
 * Lookup for a given path.
 * Input:
 *  - name: path of node
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 */
int lookup(char *name) {
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;
	Stack stack = STACKinit(STACK_SIZE);

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* get root inode data */
	inode_get(current_inumber, &nType, &data);

	char *path = strtok_r(full_path, delim, &saveptr);

	if (rdlock(current_inumber)) {
			unlock(stack);
			exit(EXIT_FAILURE);
		}
	else
		STACKpush(stack, current_inumber);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		inode_get(current_inumber, &nType, &data);
		if (rdlock(current_inumber)) {
			unlock(stack);
			exit(EXIT_FAILURE);
		}
		else
			STACKpush(stack, current_inumber);
		path = strtok_r(NULL, delim, &saveptr);
	}

	unlock(stack);

	return current_inumber;
}

/*
 * Lookup that write locks the last inode from the path
 * Input:
 *  - name: path of node
 *  - stack: stack to store locks
 *  - flag: delete/create = 0, move = 1
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 */
int lookup_aux(char *name, Stack stack, int flag) {
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT, previous_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* get root inode data */
	inode_get(current_inumber, &nType, &data);

	char *path = strtok_r(full_path, delim, &saveptr);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		inode_get(current_inumber, &nType, &data);
		/* if flag = move and the inumber is already in the stack, it skips to prevent deadlocks */
		if (flag && STACKcontains(stack, previous_inumber));
		else if (rdlock(previous_inumber)) {
			unlock(stack);
			exit(EXIT_FAILURE);
		}
		else
			STACKpush(stack, previous_inumber);
		previous_inumber = current_inumber;
		path = strtok_r(NULL, delim, &saveptr);
	}

	/* if flag = move and the inumber is already in the stack, it skips to prevent deadlocks */
	if (flag && STACKcontains(stack, previous_inumber));
	else if (wrlock(previous_inumber)) {
		unlock(stack);
		exit(EXIT_FAILURE);
	}
	else
		STACKpush(stack, previous_inumber);

	return current_inumber;
}

/*
 * If the input is valid, moves the child from orig to dest
 * Input:
 *  - orig: original path of the child to be moved
 *  - dest: new path to where the child will be moved
 * Returns: SUCCESS or FAIL
 */
int move(char* orig, char* dest) {
	int n = strcmp(dest, orig);
	Stack stack = STACKinit(STACK_SIZE);
	int dest_parent_inumber, orig_parent_inumber, orig_child_inumber;
	char *dest_parent_name, *dest_child_name, *orig_parent_name, *orig_child_name, dest_name_copy[MAX_FILE_NAME], orig_name_copy[MAX_FILE_NAME];
	type pType;
	union Data pdata;
	char* common_path = strstr(dest, orig);

	if (common_path == dest && n != 0) {
		unlock(stack);
		fprintf(stderr, "Error: %s is an invalid destination path\n", dest);
		return FAIL;
	}

	strcpy(dest_name_copy, dest);
	split_parent_child_from_path(dest_name_copy, &dest_parent_name, &dest_child_name);

	strcpy(orig_name_copy, orig);
	split_parent_child_from_path(orig_name_copy, &orig_parent_name, &orig_child_name);

	if (n > 0) {
		if ((orig_parent_inumber = lookup_aux(orig_parent_name, stack, MOVE)) == FAIL) {
			unlock(stack);
			fprintf(stderr, "Error: %s does not exist\n", orig_parent_name);
			return FAIL;
		}
		if ((dest_parent_inumber = lookup_aux(dest_parent_name, stack, MOVE)) == FAIL) {
			unlock(stack);
			fprintf(stderr, "Error: %s does not exist\n", dest_parent_name);
			return FAIL;
		}
	}
	else if (n < 0) {
		if ((dest_parent_inumber = lookup_aux(dest_parent_name, stack, MOVE)) == FAIL) {
			unlock(stack);
			fprintf(stderr, "Error: %s does not exist\n", dest_parent_name);
			return FAIL;
		}
		if ((orig_parent_inumber = lookup_aux(orig_parent_name, stack, MOVE)) == FAIL) {
			unlock(stack);
			fprintf(stderr, "Error: %s does not exist\n", orig_parent_name);
			return FAIL;
		}
	}
	else
		return SUCCESS;

	inode_get(dest_parent_inumber, &pType, &pdata);

	if (lookup_sub_node(dest_child_name, pdata.dirEntries) != FAIL) {
		unlock(stack);
		fprintf(stderr, "Error: %s already exists\n", dest);
		return FAIL;
	}

	inode_get(orig_parent_inumber, &pType, &pdata);

	if ((orig_child_inumber = lookup_sub_node(orig_child_name, pdata.dirEntries)) == FAIL) {
		unlock(stack);
		fprintf(stderr, "Error: %s does not exist\n", orig);
		return FAIL;
	}
	else {
		wrlock(orig_child_inumber);
		STACKpush(stack, orig_child_inumber);
	}

	if (dir_reset_entry(orig_parent_inumber, orig_child_inumber) == FAIL) {
		unlock(stack);
		fprintf(stderr, "Error: %s does not exist\n", orig);
		return FAIL;
	}

	if (dir_add_entry(dest_parent_inumber, orig_child_inumber, dest_child_name) == FAIL) {
		dir_add_entry(orig_parent_inumber, orig_child_inumber, orig_child_name);
		unlock(stack);
		fprintf(stderr, "Error: destiny parent directory full\n");
		return FAIL;
	}

	unlock(stack);

	return SUCCESS;
}

/*
 * Prints tecnicofs tree.
 * Input:
 *  - fp: pointer to output file
 */
void print_tecnicofs_tree(FILE *fp){
	inode_print_tree(fp, FS_ROOT, "");
}

/*
 * Unlocks all the locks in the stack and frees all the memory associated with it
 * Input:
 *   - stack: pointer to the stack
 */
void unlock(Stack stack) {
	int inumber;

	while((inumber = STACKpop(stack)) != FAIL)
		if (pthread_rwlock_unlock(&inode_table[inumber].rwlock) != 0) {
			fprintf(stderr, "Error: failed to unlock\n");
			STACKfree(stack);
			exit(EXIT_FAILURE);
		}

	STACKfree(stack);
}

int rdlock(int inumber) {
	if (pthread_rwlock_rdlock(&inode_table[inumber].rwlock) != 0) {
		fprintf(stderr, "Error: failed to lock\n");
		return FAIL;
	}
	return SUCCESS;
}

int wrlock(int inumber) {
	if (pthread_rwlock_wrlock(&inode_table[inumber].rwlock) != 0) {
		fprintf(stderr, "Error: failed to lock\n");
		return FAIL;
	}
	return SUCCESS;
}
#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

int sockfd;
socklen_t clilen, servlen;
struct sockaddr_un cli_addr, serv_addr;

/*
 * Sends the command to the server
 * Inputs:
 *   - command
 */
int tfsSend(char *command) {
  int n, res;
  if (sendto(sockfd, command, MAX_INPUT_SIZE, 0, (struct sockaddr *)&serv_addr, servlen) != MAX_INPUT_SIZE){ 
    return FAIL;
  }
  n = recvfrom(sockfd, &res, sizeof(int), 0, 0, 0);
  if (n < 0) return FAIL;
  if (res == ABORT) {
    fprintf(stderr, "Fatal error: server shutdown\n");
    exit(EXIT_FAILURE);
  }
  return res;
}

/*
 * Creates a new node given a path
 * Inputs:
 *   - filename: path of node
 *   - nodeType: type of node
 * Returns:
 *   - SUCCESS or FAIL
 */
int tfsCreate(char *filename, char nodeType) {
  char command[MAX_INPUT_SIZE];
  if (sprintf(command, "c %s %c", filename, nodeType) <= 0) return FAIL;
  return tfsSend(command);
}

/*
 * Deletes a node given a path
 * Inputs:
 *   - path
 * Returns:
 *   - SUCCESS or FAIL
 */
int tfsDelete(char *path) {
  char command[MAX_INPUT_SIZE];
  if (sprintf(command, "d %s", path) <= 0) return FAIL;
  return tfsSend(command);
}

/*
 * Moves a directory/file to a diferent path
 * Inputs:
 *   - from: original path
 *   - to: new path
 * Returns:
 *   - SUCCESS or FAIL
 */
int tfsMove(char *from, char *to) {
  char command[MAX_INPUT_SIZE];
  if (sprintf(command, "m %s %s", from, to) <= 0) return FAIL;
  return tfsSend(command);
}

/*
 * Looks up for a path
 * Inputs:
 *   - path
 * Returns:
 *   - SUCCESS or FAIL
 */
int tfsLookup(char *path) {
  char command[MAX_INPUT_SIZE];
  if (sprintf(command, "l %s", path) <= 0) return FAIL;
  return tfsSend(command);
}

/*
 * Prints the tree to a file
 * Inputs:
 *   - outputfile: file to where the tree will be printed
 * Returns:
 *   - SUCCESS or FAIL
 */
int tfsPrint(char *outputfile) {
  char command[MAX_INPUT_SIZE];
  if (sprintf(command, "p %s", outputfile) <= 0) return FAIL;
  return tfsSend(command);
}

/*
 * Creates the client socket
 * Inputs:
 *   - path of the socket
 * Returns:
 *   - SUCCESS or FAIL
 */
int tfsMount(char * sockPath) {
  
  if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) return FAIL;
  
  bzero((char *) &cli_addr, sizeof(cli_addr));
  cli_addr.sun_family = AF_UNIX;
  mkstemp(cli_addr.sun_path);
  clilen = sizeof(cli_addr.sun_family) + strlen(cli_addr.sun_path);
  
  if (bind(sockfd, (struct sockaddr *) &cli_addr, clilen) < 0) return FAIL;

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, sockPath);
  servlen = sizeof(serv_addr.sun_family) + strlen(serv_addr.sun_path);
  return SUCCESS;
}

/*
 * Closes and deletes the client socket name
 * Returns:
 *   - SUCCESS or FAIL
 */
int tfsUnmount() {
  close(sockfd);
  if (unlink(cli_addr.sun_path) != 0) return FAIL;
  return SUCCESS;
}

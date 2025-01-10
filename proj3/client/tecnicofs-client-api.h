#ifndef API_H
#define API_H

#include "../server/tecnicofs-api-constants.h"

#define FAIL -1
#define SUCCESS 0
#define ABORT -2

int tfsSend(char* command);
int tfsCreate(char *path, char nodeType);
int tfsDelete(char *path);
int tfsLookup(char *path);
int tfsMove(char *from, char *to);
int tfsPrint(char *outputfile);
int tfsMount(char* serverName);
int tfsUnmount();

#endif /* CLIENT_H */

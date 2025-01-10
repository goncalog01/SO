#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "fs/operations.h"

#define MAX_INPUT_SIZE 100
#define FALSE 0
#define TRUE 1

int numberThreads;
pthread_t *tid;
int sockfd;

/*
 * Receives a command and executes it
 * Input:
 *   - command
 * Returns:
 *   - SUCCESS or FAIL
 */
int applyCommand(char *command) {
    char token, type;
    char name[MAX_INPUT_SIZE], dest[MAX_INPUT_SIZE];
    int res, numTokens = sscanf(command, "%c %s %s", &token, name, dest);
    
    type = dest[0];

    if (numTokens < 2) {
        fprintf(stderr, "Error: invalid command in Queue\n");
        exit(EXIT_FAILURE);
    }

    switch (token) {
        case 'c':
            switch (type) {
                case 'f':
                    res = create(name, T_FILE);
                    printf("Create file: %s\n", name);
                    break;
                case 'd':
                    res = create(name, T_DIRECTORY);
                    printf("Create directory: %s\n", name);
                    break;
                default:
                    fprintf(stderr, "Error: invalid node type\n");
                    res = FAIL;
            }
            break;
        case 'l':
            res = lookup(name);
            if (res >= 0)
                printf("Search: %s found\n", name);
            else
                printf("Search: %s not found\n", name);
            break;
        case 'd':
            res = delete(name);
            printf("Delete: %s\n", name);
            break;
        case 'm':
            res = move(name, dest);
            printf("Move: %s to %s\n", name, dest);
            break;
        case 'p':
            res = print_tecnicofs_tree(name);
            printf("Tecnicofs tree printed to %s\n", name);
            break;
        default: { /* error */
            fprintf(stderr, "Error: command to apply\n");
            res = FAIL;
        }
    }
    return res;
}

/*
 * Sets the socket address
 * Inputs:
 *   - path: socket path
 *   - addr: socket address
 * Returns:
 *   - address length
 */
int setSockAddrUn(char *path, struct sockaddr_un *addr) {

    if (addr == NULL)
        return 0;

    bzero((char*)addr, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strcpy(addr->sun_path, path);

    return SUN_LEN(addr);
}

/*
 * Infinite loop: gets a command from the client, executes it and returns its result
 */
void *processInput(void* arg) {
    socklen_t addrlen = sizeof(struct sockaddr_un);
    size_t output_size = sizeof(int);
    struct sockaddr_un client_addr;
    char in_buffer[MAX_INPUT_SIZE];
    int c, output;

    while (TRUE) {
        c = recvfrom(sockfd, in_buffer, sizeof(in_buffer) - 1, 0, (struct sockaddr*)&client_addr, &addrlen);
        if (c <= 0) continue;

        /* in case the client doesn't end the message with '\0' */
        in_buffer[c] = '\0';

        output = applyCommand(in_buffer);

        sendto(sockfd, &output, output_size, 0, (struct sockaddr*)&client_addr, addrlen);

        if (output == ABORT) exit(EXIT_FAILURE);
    }
}

/*
 * Frees the pthread_t array
 * Input:
 *   - Array: array of pthread_t
 */
void freeThreadArray() {
    if (tid)
        free(tid);
}

/*
 * Creates the thread pool
 */
void create_threads(int sockfd) {
    tid = (pthread_t*) malloc(numberThreads * sizeof(pthread_t));
    
    if (tid == NULL) {
        fprintf(stderr, "Error: pthread_t array allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numberThreads; i++)
        if (pthread_create(&tid[i], NULL, processInput, (void*)&sockfd) != 0) {
            fprintf(stderr, "Error: Thread creation failed.\n");
            exit(EXIT_FAILURE);
        }
}

/*
 * Waits for all threads to finish
 */
void join_threads() {
    for (int i = 0; i < numberThreads; i++)
        if (pthread_join(tid[i], NULL) != 0) {
            fprintf(stderr, "Error: Thread joining failed.\n");
            exit(EXIT_FAILURE);
        }
}


int main(int argc, char* argv[]) {
    struct sockaddr_un server_addr;
    socklen_t addrlen;
    char *path;

    if (argc != 3) {
        fprintf(stderr,"Error : Invalid input.\n");
        fprintf(stderr, "Input should be:\n./tecnicofs numthreads socketname\n");
        exit(EXIT_FAILURE);
    }

    numberThreads = atoi(argv[1]);

    if (numberThreads <= 0) {
        fprintf(stderr, "Error: invalid number of threads.\nThread number is an integer >= 1\n");
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Error: can't open socket\n");
        exit(EXIT_FAILURE);
    }

    path = argv[2];

    unlink(path);

    addrlen = setSockAddrUn(path, &server_addr);
    if (bind(sockfd, (struct sockaddr*)&server_addr, addrlen) < 0) {
        fprintf(stderr, "Error: bind error\n");
        exit(EXIT_FAILURE);
    }

    /* init filesystem */
    init_fs();

    create_threads(sockfd);

    join_threads();

    /* close and delete the socket name, despite the program never ending */
    close(sockfd);
    if (unlink(path) != 0) exit(EXIT_FAILURE);

    /* release allocated memory */
    destroy_fs();
    freeThreadArray();
    exit(EXIT_SUCCESS);
}

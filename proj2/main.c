#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <pthread.h>
#include <sys/time.h>
#include "stack.h"

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

/*used in conversion between seconds and microseconds (1 sec = 10e6 usec) */
#define CONVERSION 1000000

#define FALSE 0
#define TRUE 1
#define COMMENT "#"

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberThreads, numberCommands, count, EOFflag, prodptr, consptr;
pthread_t *tid;
pthread_mutex_t mutex;
pthread_cond_t podeProd, podeCons;

/*
 * frees the memory allocated to struct timeval.
 * Input: 
 *   - startTime: pointer to struct timeval that holds starting time
 *   - endTime: pointer to struct timeval that holds finishing time
 */
void freeTimer(struct timeval* startTime, struct timeval* endTime) {
    if (startTime != NULL && endTime != NULL) {
        free(startTime);
        free(endTime);
    }
}

/*
 * frees the pthread_t array.
 * Input:
 *   - Array: array of pthread_t
 */
void freeThreadArray() {
    if (tid)
        free(tid);
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
}

void cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (pthread_cond_wait(cond, mutex) != 0) {
        fprintf(stderr, "Error: pthread_cond_wait failed\n");
        exit(EXIT_FAILURE);
    }
}

void cond_signal(pthread_cond_t *cond) {
    if (pthread_cond_signal(cond) != 0) {
        fprintf(stderr, "Error: pthread_cond_signal failed\n");
        exit(EXIT_FAILURE);
    }
}

void cond_broadcast(pthread_cond_t *cond) {
    if (pthread_cond_broadcast(cond) != 0) {
        fprintf(stderr, "Error: pthread_cond_broadcast failed\n");
        exit(EXIT_FAILURE);
    }
}

void mutex_lock(pthread_mutex_t *mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        fprintf(stderr, "Error: pthread_mutex_lock failed\n");
        exit(EXIT_FAILURE);
    }
}

void mutex_unlock(pthread_mutex_t *mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        fprintf(stderr, "Error: pthread_mutex_unlock failed\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Reads a command from the input file
 * Input:
 *   - pointer to the input file
 * Returns:
 *   - command: if fgets is successful
 *   - NULL: when it reaches EOF
 */
char* produce(FILE *inputFilePtr) {
    char* line = (char*) malloc(MAX_INPUT_SIZE * sizeof(char));

    if (!line) {
        fprintf(stderr, "Error: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    if (fgets(line, sizeof(char) * MAX_INPUT_SIZE, inputFilePtr)) {
        char token;
        char name[MAX_INPUT_SIZE], destOrType[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %s", &token, name, destOrType);

        /* perform minimal validation */
        if (numTokens < 1) {
            free(line);
            return NULL;
        }
        switch (token) {
            case 'c':
                if(numTokens != 3) {
                    free(line);
                    errorParse();
                    exit(EXIT_FAILURE);
                }
                return line;
            
            case 'l':
                if(numTokens != 2) {
                    free(line);
                    errorParse();
                    exit(EXIT_FAILURE);
                }
                return line;
            
            case 'd':
                if(numTokens != 2) {
                    free(line);
                    errorParse();
                    exit(EXIT_FAILURE);
                }
                return line;

            case 'm':
                if(numTokens != 3) {
                    free(line);
                    errorParse();
                    exit(EXIT_FAILURE);
                }
                return line;
            
            case '#':
                free(line);
                return COMMENT;
            
            default: { /* error */
                free(line);
                errorParse();
                exit(EXIT_FAILURE);
            }
        }
    }
    else {
        free(line);
        return NULL;
    }
}

/*
 * Producer: while EOF is not found, gets a command from the file and puts it in the task queue
 * Input:
 *   - pointer to the input file
 */
void insertCommand(FILE* inputFilePtr) {
    while (TRUE) {
        mutex_lock(&mutex);
        char* item = produce(inputFilePtr);
        if (item == NULL) break;
        else if (*item == *COMMENT) { mutex_unlock(&mutex); continue;}
        while (count == MAX_COMMANDS)
            cond_wait(&podeProd, &mutex);
        strcpy(inputCommands[prodptr], item);
        free(item);
        prodptr++;
        if (prodptr == MAX_COMMANDS)
            prodptr = 0;
        count++;
        cond_signal(&podeCons);
        mutex_unlock(&mutex);
    }
    EOFflag = TRUE;
    cond_broadcast(&podeCons);
    mutex_unlock(&mutex);
}

/*
 * Receives a command and executes it
 * Input:
 *   - command to be executed
 */
void applyCommand(char *command) {

    char token, type;
    char name[MAX_INPUT_SIZE], dest[MAX_INPUT_SIZE];
    int numTokens = sscanf(command, "%c %s %s", &token, name, dest);
    
    type = dest[0];
    free(command);

    if (numTokens < 2) {
        fprintf(stderr, "Error: invalid command in Queue\n");
        exit(EXIT_FAILURE);
    }

    int searchResult;
    switch (token) {
        case 'c':
            switch (type) {
                case 'f':
                    create(name, T_FILE);
                    printf("Create file: %s\n", name);
                    break;
                case 'd':
                    create(name, T_DIRECTORY);
                    printf("Create directory: %s\n", name);
                    break;
                default:
                    fprintf(stderr, "Error: invalid node type\n");
                    exit(EXIT_FAILURE);
            }
            break;
        case 'l':
            searchResult = lookup(name);
            if (searchResult >= 0)
                printf("Search: %s found\n", name);
            else
                printf("Search: %s not found\n", name);
            break;
        case 'd':
            delete(name);
            printf("Delete: %s\n", name);
            break;
        case 'm':
            move(name, dest);
            printf("Move: %s to %s\n", name, dest);
            break;
        default: { /* error */
            fprintf(stderr, "Error: command to apply\n");
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Consumer: while there are commands in the task queue, removes them and executes them
 */
void *removeCommand(void *param) {
    while (TRUE) {
        char* item = (char*) malloc(sizeof(char) * MAX_INPUT_SIZE);

        if (!item) {
            fprintf(stderr, "Error: memory allocation failed\n");
            exit(EXIT_FAILURE);
        }

        mutex_lock(&mutex);
        while (count == 0) {
            if (EOFflag) {
                mutex_unlock(&mutex);
                free(item);
                pthread_exit(NULL);
            }
            cond_wait(&podeCons, &mutex);
        }
        if(count == 0) continue;
        strcpy(item, inputCommands[consptr]);
        consptr++;
        if (consptr == MAX_COMMANDS)
            consptr = 0;
        count--;
        cond_signal(&podeProd);
        mutex_unlock(&mutex);
        applyCommand(item);
    }
    return NULL;
}

/*
 * Creates the thread pool
 */
void create_threads() {
    tid = (pthread_t*) malloc(numberThreads * sizeof(pthread_t));
    
    if (tid == NULL) {
        fprintf(stderr, "Error: pthread_t array allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numberThreads; i++)
        if (pthread_create(&tid[i], NULL, removeCommand, NULL) != 0) {
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

/*
 * Initiates all mutexes and conditions
 */ 
void init_all() {

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        fprintf(stderr, "Error: failed to initialize mutex\n");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_cond_init(&podeCons, NULL) != 0) {
        fprintf(stderr, "Error: failed to initialize cond\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&podeProd, NULL) != 0) {
        fprintf(stderr, "Error: failed to initialize cond\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Destroys all mutexes and conditions
 */ 
void destroy_all() {

    if (pthread_mutex_destroy(&mutex) != 0) {
        fprintf(stderr, "Error: failed to destroy mutex\n");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_cond_destroy(&podeCons) != 0) {
        fprintf(stderr, "Error: failed to destroy cond\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_destroy(&podeProd) != 0) {
        fprintf(stderr, "Error: failed to destroy cond\n");
        exit(EXIT_FAILURE);
    }
}

struct timeval* start_timer() {
    struct timeval *startTimePTR = (struct timeval*) malloc(sizeof(struct timeval));

    if (startTimePTR)
        gettimeofday(startTimePTR,NULL);
    else {
        fprintf(stderr, "Error: Time allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return startTimePTR;
}

float end_timer(struct timeval *startTimePTR) {
    int timeInMicroSec;
    float timeInSec;
    struct timeval *endTimePTR = (struct timeval*) malloc(sizeof(struct timeval));

    if (endTimePTR)
        gettimeofday(endTimePTR,NULL);
    else {
        fprintf(stderr, "Error: Time allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    
    timeInMicroSec = ((endTimePTR->tv_sec - startTimePTR->tv_sec) * CONVERSION + (endTimePTR->tv_usec - startTimePTR->tv_usec));
    timeInSec = timeInMicroSec /(float)CONVERSION ;

    freeTimer(startTimePTR, endTimePTR);

    return timeInSec;
}

FILE *openFile(char *filename, char *rw) {
    FILE *filePtr = fopen(filename, rw);

    if (filePtr == NULL) {
        fprintf(stderr, "Error: %s does not exist \n", filename);
        exit(EXIT_FAILURE);
    }

    return filePtr;
}

void closeFile(FILE *file, char *filename) {
    if (fclose(file) != 0) {
        fprintf(stderr, "Error: Could not close %s\n", filename);
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char* argv[]) {
    char inFile[MAX_FILE_NAME], outFile[MAX_FILE_NAME];
    FILE *outFilePtr, *inputFilePtr;
    struct timeval *startTimePTR;
    float timeInSec;

    if (argc != 4) {
        fprintf(stderr,"Error : Invalid input.\n");
        fprintf(stderr, "Input should be:\n./tecnicofs inputfile outputfile numthreads\n");
        exit(EXIT_FAILURE);
    }

    strcpy(inFile, argv[1]);
    strcpy(outFile, argv[2]);
    numberThreads = atoi(argv[3]);

    if (numberThreads <= 0) {
        fprintf(stderr, "Error: invalid number of threads.\nThread number is an integer >= 1\n");
        exit(EXIT_FAILURE);
    }

    /* init filesystem */
    init_fs();

    init_all();

    inputFilePtr = openFile(inFile, "r");

    startTimePTR = start_timer();

    create_threads();
    insertCommand(inputFilePtr);
    join_threads();

    closeFile(inputFilePtr, inFile);
        
    timeInSec = end_timer(startTimePTR);
    
    fprintf(stdout, "TecnicoFS completed in %.4f seconds.\n", timeInSec);

    outFilePtr = openFile(outFile, "w");

    print_tecnicofs_tree(outFilePtr);

    closeFile(outFilePtr, outFile);

    /* release allocated memory */
    destroy_fs();
    freeThreadArray();
    exit(EXIT_SUCCESS);
}

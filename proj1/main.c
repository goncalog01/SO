#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <pthread.h>
#include <sys/time.h>
#include "lock.h"

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

/* maximum size of the syncStrategy string */
#define MAX_SYNC_NAME 7

/*used in conversion between seconds and microseconds (1 sec = 10e6 usec) */
#define CONVERSION 1000000

int numberThreads = 0;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;
pthread_t *tid;

/*
 * frees the memory allocated to pthread_mutex_t or pthread_rwlock_t.
 */
void freeLocks(){
    switch(lockType){
        case MUTEX:
            pthread_mutex_destroy(sync_lock.mutex_lock);
            free(sync_lock.mutex_lock);
            break;
        case RWLOCK:
            pthread_rwlock_destroy(sync_lock.rw_lock);
            free(sync_lock.rw_lock);
            break;
        default:
            return;
    }
}

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
void freeThreadArray(pthread_t *Array) {
    if (lockType != NOSYNC && Array)
        free(Array);
}

int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    if(numberCommands > 0){
        numberCommands--;
        return inputCommands[headQueue++];  
    }
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput(FILE *inputFilePtr){
    char line[MAX_INPUT_SIZE];
    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), inputFilePtr)) {
        char token, type;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %c", &token, name, &type);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'l':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
}

void *applyCommands(void *Ptr){
    if (lockType!= NOSYNC && pthread_mutex_lock(&command_lock) != 0) {
        fprintf(stderr, "Error: Failed to lock mutex.\n");
        exit(EXIT_FAILURE);
    }
    while (numberCommands > 0){
        const char* command = removeCommand();

        if (lockType!= NOSYNC && pthread_mutex_unlock(&command_lock) != 0) {
            fprintf(stderr, "Error: Failed to unlock mutex.\n");
            exit(EXIT_FAILURE);
        }

        if (command == NULL){
            continue;
        }

        char token, type;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s %c", &token, name, &type);
        if (numTokens < 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }

        int searchResult;
        switch (token) {
            case 'c':
                switch (type) {
                    case 'f':
                        lock(WRITE);
                        printf("Create file: %s\n", name);
                        create(name, T_FILE);
                        unlock();
                        break;
                    case 'd':
                        lock(WRITE);
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY);
                        unlock();
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                lock(READ);
                searchResult = lookup(name);
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                unlock();
                break;
            case 'd':
                lock(WRITE);
                printf("Delete: %s\n", name);
                delete(name);
                unlock();
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}

/*
 * creates the threads if the syncstrategy isn't nosync
 */
void create_threads() {
    if (lockType == NOSYNC) {
        applyCommands(NULL);
        return;
    }
    else {
        int i;
        pthread_t *tid = (pthread_t*)malloc(numberThreads * sizeof(pthread_t));
        
        if(tid == NULL){
            fprintf(stderr, "Error: pthread_t array allocation failed.\n");
            exit(EXIT_FAILURE);
        }

        for (i = 0;i < numberThreads; i++)
            if (pthread_create(&tid[i], NULL, applyCommands, NULL) != 0) {
                fprintf(stderr, "Error: Thread creation failed.\n");
                exit(EXIT_FAILURE);
            }

        for (i = 0;i < numberThreads; i++)
            if (pthread_join(tid[i], NULL) != 0) {
                fprintf(stderr, "Error: Thread joining failed.\n");
                exit(EXIT_FAILURE);
        }
    }

}


int main(int argc, char* argv[]) {
    char inFile[MAX_FILE_NAME], outFile[MAX_FILE_NAME], syncStrategy[MAX_SYNC_NAME];
    FILE *outFilePtr,*inputFilePtr;
    struct timeval *startTimePTR, *endTimePTR;
    int timeInMicroSec;
    float timeInSec;

    if (argc != 5){
        fprintf(stderr,"Error : Invalid input.\n");
        fprintf(stderr, "Input should be:\n./tecnicofs inputfile outputfile numthreads syncstrategy\n");
        exit(EXIT_FAILURE);
    }
    strcpy(inFile,argv[1]);
    strcpy(outFile,argv[2]);
    strcpy(syncStrategy,argv[4]);
    numberThreads = atoi(argv[3]);
    

    if (numberThreads <= 0) {
        fprintf(stderr, "Error: invalid number of threads.\nThread number is an integer >= 1\n");
        exit(EXIT_FAILURE);
    }
    
    if(!strcmp("nosync",syncStrategy)){
        if(numberThreads > 1){
            fprintf(stderr, "Error: invalid number of threads for nosync.\n");
            exit(EXIT_FAILURE);
        }
        else {
            lockType = NOSYNC;
            sync_lock.mutex_lock = NULL;
            sync_lock.rw_lock = NULL;
        }
    }
    else if(!strcmp("mutex",syncStrategy)){
        sync_lock.mutex_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        if(sync_lock.mutex_lock == NULL){
            fprintf(stderr,"Error: Mutex allocation failed.\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_init(sync_lock.mutex_lock,NULL) != 0){
            fprintf(stderr,"Error: mutex initialization failed.\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_init(&command_lock,NULL) != 0){
            fprintf(stderr,"Error: mutex initialization failed.\n");
            exit(EXIT_FAILURE);
        }
        lockType = MUTEX;

    }
    else if(!strcmp("rwlock",syncStrategy)){
        sync_lock.rw_lock = (pthread_rwlock_t *)malloc(sizeof(pthread_rwlock_t));
        if(sync_lock.rw_lock == NULL){
            fprintf(stderr,"Error: rwlock allocation failed.\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_rwlock_init(sync_lock.rw_lock,NULL) != 0){
            fprintf(stderr,"Error: rwlock initialization failed.\n");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_init(&command_lock,NULL) != 0){
            fprintf(stderr,"Error: mutex initialization failed.\n");
            exit(EXIT_FAILURE);
        }
        lockType = RWLOCK;
    }
    else{
        fprintf(stderr,"Error: invalid syncstrategy.\n");
        exit(EXIT_FAILURE);
    }

    /* init filesystem */

    init_fs();

    /* process input and print tree */
    inputFilePtr = fopen(inFile,"r");

    if (inputFilePtr == NULL){
        fprintf(stderr, "Error: %s does not exist \n",inFile);
        exit(EXIT_FAILURE);
    }
    

    processInput(inputFilePtr);

    if(fclose(inputFilePtr) != 0){
        fprintf(stderr, "Error: Could not close %s\n",inFile);
        exit(EXIT_FAILURE);
    }

    startTimePTR = (struct timeval*) malloc(sizeof(struct timeval));
    if (startTimePTR)
        gettimeofday(startTimePTR,NULL);
    else {
        fprintf(stderr, "Error: Time allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    create_threads();
        
    endTimePTR = (struct timeval*) malloc(sizeof(struct timeval));
    if (endTimePTR)
        gettimeofday(endTimePTR,NULL);
    else {
        fprintf(stderr, "Error: Time allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    
    timeInMicroSec = ((endTimePTR->tv_sec - startTimePTR->tv_sec) * CONVERSION + (endTimePTR->tv_usec - startTimePTR->tv_usec));
    timeInSec = timeInMicroSec /(float)CONVERSION ;
    
    fprintf(stdout,"TecnicoFS completed in %.4f seconds.\n",timeInSec);

    outFilePtr = fopen(outFile,"w");

    if (outFilePtr == NULL){
        fprintf(stderr, "Error: %s does not exist \n",outFile);
        exit(EXIT_FAILURE);
    }

    print_tecnicofs_tree(outFilePtr);

    if(fclose(outFilePtr) != 0){
        fprintf(stderr, "Error: Could not close %s\n",outFile);
        exit(EXIT_FAILURE);
    }

    /* release allocated memory */
    destroy_fs();
    freeLocks();
    freeTimer(startTimePTR, endTimePTR);
    freeThreadArray(tid);
    pthread_mutex_destroy(&command_lock);
    exit(EXIT_SUCCESS);
}

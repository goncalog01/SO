#include <stdio.h>
#include <stdlib.h>
#include "lock.h"

void lock(int rw_flag) {
    switch (lockType) {
        case MUTEX:
            if (pthread_mutex_lock(sync_lock.mutex_lock) != 0) {
                fprintf(stderr, "Error: Failed to lock mutex.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case RWLOCK:
            if (rw_flag == READ) {
                if (pthread_rwlock_rdlock(sync_lock.rw_lock) != 0) {
                    fprintf(stderr, "Error: Failed to lock rwlock.\n");
                    exit(EXIT_FAILURE);
                }
            }
            else {
                if (pthread_rwlock_wrlock(sync_lock.rw_lock) != 0) {
                    fprintf(stderr, "Error: Failed to lock rwlock.\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;
        default:
            break;
    }
}


void unlock() {
    switch (lockType) {
        case MUTEX:
            if (pthread_mutex_unlock(sync_lock.mutex_lock) != 0) {
                fprintf(stderr, "Error: Failed to unlock mutex.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case RWLOCK:
            if (pthread_rwlock_unlock(sync_lock.rw_lock) != 0) {
                fprintf(stderr, "Error: Failed to unlock rwlock.\n");
                exit(EXIT_FAILURE);
                }
            break;
        default:
            break;
    }
}
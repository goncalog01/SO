#ifndef __LOCK_H__
#define __LOCK_H_

#include <pthread.h>

#define READ 0
#define WRITE 1

enum syncStrat{NOSYNC,MUTEX,RWLOCK};


union Lock{
    pthread_mutex_t *mutex_lock;
    pthread_rwlock_t *rw_lock;
};


enum syncStrat lockType;
union Lock sync_lock;
pthread_mutex_t command_lock;

void lock(int rw_flag);
void unlock();

#endif
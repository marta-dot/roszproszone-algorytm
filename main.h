#ifndef MAINH
#define MAINH
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "util.h"
/* boolean */
#define TRUE 1
#define FALSE 0
#define SEC_IN_STATE 1
#define STATE_CHANGE_PROB 10

#define ROOT 0

extern int rank;
extern int size;
typedef enum {InRun, IWait, InSend, InFinish, InSection} state_t;
extern state_t stan;
extern pthread_t threadKom, threadMon;

extern pthread_mutex_t stateMut;

//zegar lamporta
extern int lamportClock;
extern pthread_mutex_t clockMut;
void incrementClock(int recived_ts);

//ack count
extern int ackCount;
extern pthread_mutex_t ackMut;

//typ babcia/studentka
extern char processType;

//słoiki i konfitury
extern int p; //słoik
extern int k; //konfitura
extern pthread_mutex_t csMut;

extern volatile int waitingForCS;  // czy proces czeka na wejście do CS
extern pthread_mutex_t waitingMut;

//wielkość sekcji krytycznej
// extern const int csCapacity;

#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

// makro println - to samo co debug, ale wyświetla się zawsze
#define println(FORMAT,...) printf("%c[%d;%dm [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ##__VA_ARGS__, 27,0,37);

void changeState( state_t );

#endif
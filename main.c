#include "main.h"
#include "watek_glowny.h"
#include "watek_komunikacyjny.h"

int rank, size;
state_t stan=InRun;
pthread_t threadKom, threadMon;
pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;

int lamportClock = 0;
pthread_mutex_t clockMut = PTHREAD_MUTEX_INITIALIZER;

int ackCount = 0;
pthread_mutex_t ackMut = PTHREAD_MUTEX_INITIALIZER;

char processType;

int p = 0, k = 0;
pthread_mutex_t csMut = PTHREAD_MUTEX_INITIALIZER;

volatile int waitingForCS = 0;
pthread_mutex_t waitingMut = PTHREAD_MUTEX_INITIALIZER;

void finalizuj()
{
    pthread_mutex_destroy( &stateMut);
    /* Czekamy, aż wątek potomny się zakończy */
    println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(threadKom,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}
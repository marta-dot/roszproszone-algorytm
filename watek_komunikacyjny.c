#include "main.h"
#include "watek_komunikacyjny.h"
#include "util.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
    MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
    /* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( stan!=InFinish ) {
	debug("czekam na recv");
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        incrementClock(pakiet.ts);

        switch (status.MPI_TAG) {
            case FINISH: 
                changeState(InFinish);
                break;
                
            case REQUEST: 
                debug("Dostałem REQUEST od %d (ts: %d)", pakiet.src, pakiet.ts);
                addRequestToQueue(&pakiet);

                // Odsyłamy ACK
                packet_t ack;
                pthread_mutex_lock(&clockMut);
                lamportClock++;
                ack.ts = lamportClock;
                pthread_mutex_unlock(&clockMut);
                ack.src = rank;
                sendPacket(&ack, pakiet.src, ACK);
                break;

            case ACK: 
                debug("Dostałem ACK od %d", pakiet.src);
                pthread_mutex_lock(&ackMut);
                ackCount++;
                int should_enter = (ackCount == size - 1) && amIFirstInQueue();
                pthread_mutex_unlock(&ackMut);
                
                if (should_enter) {
                    enterCS();
                }
                break;

            case RELEASE:
                debug("Dostałem RELEASE od %d", pakiet.src);
                incrementClock(pakiet.ts);
                removeRequestFromQueue(pakiet.src);

                if (ackCount == size - 1 && amIFirstInQueue() && stan != InSection) {
                    enterCS();
                }

                break;
                
            default:
                break;
        }
    }
}

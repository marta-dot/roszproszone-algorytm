#include "main.h"
#include "watek_komunikacyjny.h"
#include "util.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
    MPI_Status status;
    packet_t pakiet;
    int flag;

    while (stan != InFinish) {
        // debug("Czekam na recv");
        MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        incrementClock(pakiet.ts);

        switch (status.MPI_TAG) {
            case FINISH:
                // debug("Otrzymałem FINISH - kończę");
                changeState(InFinish);
                break;

            case REQUEST:
                // debug("Otrzymałem REQUEST od %d (ts: %d)", pakiet.src, pakiet.ts);
                addRequestToQueue(&pakiet);

                // Odsyłamy ACK
                packet_t ack;
                pthread_mutex_lock(&clockMut);
                lamportClock++;
                ack.ts = lamportClock;
                pthread_mutex_unlock(&clockMut);
                ack.src = rank;
                ack.type = processType;
                sendPacket(&ack, pakiet.src, ACK);
                break;

            case ACK:
                // debug("Otrzymałem ACK od %d", pakiet.src);
                pthread_mutex_lock(&stateMut);
                ackCount++;
                // debug("Liczba ACK: %d/%d", ackCount, size-1);
                pthread_mutex_unlock(&stateMut);
                break;

            case RELEASE:
                // debug("Otrzymałem RELEASE od %d", pakiet.src);
                removeRequestFromQueue(pakiet.src);
                break;

            case UPDATE:
                // debug("Otrzymałem UPDATE od %d. Nowy stan: p=%d, k=%d", pakiet.src, pakiet.p_val, pakiet.k_val);
                pthread_mutex_lock(&csMut);
                p = pakiet.p_val;
                k = pakiet.k_val;
                pthread_mutex_unlock(&csMut);
                break;

            default:
                // debug("Nieznany typ wiadomości: %d", status.MPI_TAG);
                break;
        }

        // czy można wejść do sekcji krytycznej
        if (stan != InSection && canEnterCS()) {
            // debug("Mogę wejść do sekcji krytycznej!");
            changeState(InSection);
            enterCS();
        }
    }
    return NULL;
}
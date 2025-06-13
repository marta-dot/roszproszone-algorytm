#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    srandom(rank);

    while (stan != InFinish) {
        switch(stan) {
            case InRun:
                debug("Chcę wejść do sekcji krytycznej - wysyłam REQUESTy");

                // Przygotuj żądanie
                packet_t req;
                req.type = processType;
                req.src = rank;

                pthread_mutex_lock(&clockMut);
                lamportClock++;
                req.ts = lamportClock;
                pthread_mutex_unlock(&clockMut);

                // Ustaw flagę oczekiwania
                pthread_mutex_lock(&waitingMut);
                waitingForCS = 1;
                pthread_mutex_unlock(&waitingMut);

                // Zresetuj licznik ACK
                pthread_mutex_lock(&stateMut);
                ackCount = 0;
                pthread_mutex_unlock(&stateMut);

                // Dodaj własne żądanie do kolejki
                addRequestToQueue(&req);

                // Wyślij REQUEST do wszystkich innych procesów
                for (int i = 0; i < size; i++) {
                    if (i != rank) {
                        sendPacket(&req, i, REQUEST);
                    }
                }

                changeState(IWait);
                debug("Wysłałem REQUESTy, teraz czekam na odpowiedzi");
                break;

            case IWait:
                // Czekamy na ACKi i sprawdzamy w wątku komunikacyjnym
                break;

            case InSection:
                // Obsługiwane przez enterCS()
                break;

            case InSend:
                // Stan pośredni
                break;

            default:
                break;
        }

        sleep(2 + random() % 3); // losowy czas między próbami wejścia do CS
    }
}
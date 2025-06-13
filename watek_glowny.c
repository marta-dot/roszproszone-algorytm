#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    srandom(rank);

    while (stan != InFinish) {
        switch(stan) {
            case InRun:
                debug("Zmieniam stan na wysyłanie");
                changeState( InSend );

                packet_t req;
                req.type = processType;

                if(req.type == 'B'){
                    debug("proces BABCIA");
                } else {
                    debug("STUDENTKA");
                }

                pthread_mutex_lock(&clockMut);
                lamportClock++;
                req.ts = lamportClock;
                pthread_mutex_unlock(&clockMut);
                req.src = rank;

                addRequestToQueue(&req);

                for (int i = 0; i < size; i++) {
                    if (i != rank)
                        sendPacket(&req, i, REQUEST);
                }

                changeState(IWait);
                debug("Skończyłem wysyłać, teraz czekam");
                break;

            case IWait:
                
                break;

            case InSection:
                // też nic — sterowane przez `startKomWatek()`
                break;

            case InSend:
                // też nic — ten stan jest pośredni
                break;

            default:
                break;
        }

        sleep(1); // delikatne odciążenie CPU
    }
}

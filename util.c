#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

// #define REQUEST 1
// #define RELEASE 2
// #define ACK 3
// #define UPDATE 4
// #define FINISH 5

//kolejka żądań
#define MAX_QUEUE 100
packet_t requestQueue[MAX_QUEUE];
int queueSize = 0;
pthread_mutex_t queueMut = PTHREAD_MUTEX_INITIALIZER;

//struktura nazw typów
struct tagNames_t{
    const char *name;
    int tag;
} tagNames[] = { { "request ", REQUEST }, {"release", RELEASE }, {"ack", ACK },{"update", UPDATE }, { "finish", FINISH}};

const char const *tag2string( int tag )
{
    for (int i=0; i <sizeof(tagNames)/sizeof(struct tagNames_t);i++) {
        if ( tagNames[i].tag == tag )  return tagNames[i].name;
    }
    return "<unknown>";
}

/* tworzy typ MPI_PAKIET_T
*/
void inicjuj_typ_pakietu()
{
    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    int       blocklengths[NITEMS] = {1,1,1,1};
    MPI_Datatype typy[NITEMS] = {MPI_INT, MPI_INT, MPI_INT,MPI_CHAR};

    MPI_Aint     offsets[NITEMS];
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
    offsets[3] = offsetof(packet_t, type);


    MPI_Type_create_struct(NITEMS, blocklengths, offsets, typy, &MPI_PAKIET_T);

    MPI_Type_commit(&MPI_PAKIET_T);
}

/* opis patrz util.h */
void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    pkt->src = rank;
    MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    debug("Wysyłam %s do %d\n", tag2string( tag), destination);
    if (freepkt) free(pkt);
}

void changeState( state_t newState )
{
    pthread_mutex_lock( &stateMut );
    if (stan==InFinish) {
        pthread_mutex_unlock( &stateMut );
        return;
    }
    stan = newState;
    pthread_mutex_unlock( &stateMut );
}

//funkcje do obslugi kolejki

int comparePackets(const void *a, const void *b){
    const packet_t *pa = (const packet_t *)a;
    const packet_t *pb = (const packet_t *)b;

    if(pa->ts != pb->ts)
        return pa->ts - pb->ts;
    return pa->src - pb->src;
}

void addRequestToQueue(packet_t *pkt){
    pthread_mutex_lock(&queueMut);

    // Sprawdź czy żądanie już nie istnieje w kolejce
    for (int i = 0; i < queueSize; i++) {
        if (requestQueue[i].src == pkt->src) {
            pthread_mutex_unlock(&queueMut);
            debug("Żądanie od %d już jest w kolejce", pkt->src);
            return;
        }
    }

    if (queueSize < MAX_QUEUE) {
        requestQueue[queueSize++] = *pkt;
        qsort(requestQueue, queueSize, sizeof(packet_t), comparePackets);
        debug("Dodałem do kolejki REQUEST od %d (ts = %d), rozmiar kolejki: %d",
              pkt->src, pkt->ts, queueSize);

        // Debug: wyświetl całą kolejkę
        debug("Kolejka żądań:");
        for (int i = 0; i < queueSize; i++) {
            debug("  [%d] src=%d, ts=%d", i, requestQueue[i].src, requestQueue[i].ts);
        }
    } else {
        debug("BŁĄD: Kolejka żądań jest pełna!");
    }

    pthread_mutex_unlock(&queueMut);
}

void removeRequestFromQueue(int src){
    pthread_mutex_lock(&queueMut);
    for (int i = 0; i< queueSize; i++){
        if(requestQueue[i].src == src){
            for(int j = i;j <queueSize-1; j++){
                requestQueue[j] = requestQueue[j+1];
            }
            queueSize--;
            break;

        }
    }
    pthread_mutex_unlock(&queueMut);
}

int amIFirstInQueue(){
    pthread_mutex_lock(&queueMut);
    int first = (queueSize>0 && requestQueue[0].src == rank);
    pthread_mutex_unlock(&queueMut);
    return first;
}

//zegar lamporta

void incrementClock(int recived_ts){
    pthread_mutex_lock(&clockMut);
    if(lamportClock < recived_ts)
        lamportClock = recived_ts; // wybieramy większą wartość
    lamportClock++;
    pthread_mutex_unlock(&clockMut);
}

// //sekcja krytyczna
void enterCS(){
    debug("Wchodzę do sekcji krytycznej");

    sleep(3); // symulacja pracy w sekcji krytycznej

    pthread_mutex_lock(&csMut);
    if (processType == 'B') {
        p++; // babcia dodaje słoik
    } else {
        k++; // studentka dodaje konfiturę
    }
    debug("Babcie: %d słoików, Studentki: %d konfitur", p, k);
    pthread_mutex_unlock(&csMut);

    // Przygotuj i wyślij RELEASE
    packet_t release;
    release.src = rank;
    release.type = processType;

    pthread_mutex_lock(&clockMut);
    lamportClock++;
    release.ts = lamportClock;
    pthread_mutex_unlock(&clockMut);

    debug("Wysyłam RELEASE do wszystkich procesów");

    // Wyślij RELEASE do wszystkich innych procesów
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&release, i, RELEASE);
        }
    }

    // Bezpiecznie usuń własne żądanie i zresetuj stan
    pthread_mutex_lock(&stateMut);
    removeRequestFromQueue(rank);
    ackCount = 0;

    pthread_mutex_lock(&waitingMut);
    waitingForCS = 0;
    pthread_mutex_unlock(&waitingMut);

    stan = InRun;
    pthread_mutex_unlock(&stateMut);

    debug("Wyszedłem z sekcji krytycznej");
}

int canEnterCS() {
    int canEnter = 0;

    pthread_mutex_lock(&waitingMut);
    if (!waitingForCS) {
        pthread_mutex_unlock(&waitingMut);
        return 0; // Jeśli nie czekamy na CS, to nie możemy wejść
    }
    pthread_mutex_unlock(&waitingMut);

    pthread_mutex_lock(&queueMut);
    pthread_mutex_lock(&stateMut);

    if (ackCount >= (size - 1) &&
        queueSize > 0 &&
        requestQueue[0].src == rank) {
        canEnter = 1;
        debug("Mogę wejść do CS: ackCount=%d/%d, pierwszyWKolejce=TAK",
              ackCount, size-1);
    } else {
        debug("Nie mogę wejść do CS: ackCount=%d/%d, pierwszyWKolejce=%s, queueSize=%d",
              ackCount, size-1,
              (queueSize > 0 && requestQueue[0].src == rank) ? "TAK" : "NIE",
              queueSize);
    }

    pthread_mutex_unlock(&stateMut);
    pthread_mutex_unlock(&queueMut);

    return canEnter;
}
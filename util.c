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
    int       blocklengths[NITEMS] = {1,1,1,1,1,1};
    MPI_Datatype typy[NITEMS] = {MPI_INT, MPI_INT, MPI_INT,MPI_CHAR, MPI_INT, MPI_INT};

    MPI_Aint     offsets[NITEMS];
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
    offsets[3] = offsetof(packet_t, type);
    offsets[4] = offsetof(packet_t, p_val);
    offsets[5] = offsetof(packet_t, k_val);


    MPI_Type_create_struct(NITEMS, blocklengths, offsets, typy, &MPI_PAKIET_T);

    MPI_Type_commit(&MPI_PAKIET_T);
}


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

    // Czy już istnieje
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

        //wyświetla całą kolejkę
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
void enterCS() {
    debug("Wszedłem do sekcji krytycznej. Wykonuję akcję.");

    int czas_pracy_us = 1000000 + (random() % 9000000);

    debug("Czas produkcji/konsumpcji: %.2f sekundy.", (float)czas_pracy_us / 1000000.0);

    usleep(czas_pracy_us);

    pthread_mutex_lock(&csMut);
    if (processType == 'B') {
        p--;
        k++;
        debug("Babcia wymieniła słoik na konfiturę.");
    } else { // Studentka
        k--;
        p++;
        debug("Studentka wymieniła konfiturę na słoik.");
    }
    debug("STAN PO ZMIANIE: Słoiki: %d, Konfitury: %d", p, k);

    // UPDATE ze zmienionym stanem
    debug("Zaktualizowany stan zasobów.");
    packet_t update_pkt;
    update_pkt.src = rank;
    update_pkt.p_val = p;
    update_pkt.k_val = k;

    pthread_mutex_lock(&clockMut);
    lamportClock++;
    update_pkt.ts = lamportClock;
    pthread_mutex_unlock(&clockMut);

    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&update_pkt, i, UPDATE);
        }
    }
    pthread_mutex_unlock(&csMut);

    // release
    giveUpTurn();

    debug("Wyszedłem z sekcji krytycznej po udanej operacji.");
}

int canEnterCS() {
    // czy czekamy na CS i czy mamy wszystkie ACK
    pthread_mutex_lock(&waitingMut);
    if (!waitingForCS) {
        pthread_mutex_unlock(&waitingMut);
        return 0;
    }
    pthread_mutex_unlock(&waitingMut);

    pthread_mutex_lock(&stateMut);
    int allAcksReceived = (ackCount >= (size - 1));
    pthread_mutex_unlock(&stateMut);

    if (!allAcksReceived) {
        return 0;
    }

    // Sprawdzenie pozycji w kolejce
    int my_position = -1;
    pthread_mutex_lock(&queueMut);
    for (int i = 0; i < queueSize; i++) {
        if (requestQueue[i].src == rank) {
            my_position = i;
            break;
        }
    }

    int am_i_in_top_group = (my_position != -1 && my_position < csCapacity);

    if (!am_i_in_top_group) {
        pthread_mutex_unlock(&queueMut);
        return 0; // Nie załapliśmy się
    }

    // Załapaliśmy się
    // ile procesów danego typu jest w grupie wejściowej (top `csCapacity`)
    int babcie_w_grupie = 0;
    int studentki_w_grupie = 0;
    for(int i = 0; i < csCapacity && i < queueSize; i++) {
        if (requestQueue[i].type == 'B') {
            babcie_w_grupie++;
        } else {
            studentki_w_grupie++;
        }
    }
    pthread_mutex_unlock(&queueMut);

    // czy wystarczy?
    int resource_available = 0;
    pthread_mutex_lock(&csMut);
    if (processType == 'B' && p >= babcie_w_grupie) {
        resource_available = 1;
    } else if (processType == 'S' && k >= studentki_w_grupie) {
        resource_available = 1;
    }
    pthread_mutex_unlock(&csMut);

    if (resource_available) {
        // wchodzimy do cs
        debug("Jestem w top %d i są zasoby dla %d babć i %d studentek. WCHODZĘ.", csCapacity, babcie_w_grupie, studentki_w_grupie);
        return 1;
    } else {
        // nie wchodzimy do cs
        debug("Jestem w top %d, ale BRAK zasobów dla grupy. Zwalniam kolejkę.", csCapacity);
        giveUpTurn(); // robimy release
        return 0;
    }
}

void giveUpTurn() {
    debug("Zwalniam kolejkę i resetuję stan.");

    //  RELEASE do wszystkich
    packet_t release;
    release.src = rank;
    release.type = processType;

    pthread_mutex_lock(&clockMut);
    lamportClock++;
    release.ts = lamportClock;
    pthread_mutex_unlock(&clockMut);

    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&release, i, RELEASE);
        }
    }

    removeRequestFromQueue(rank);

    pthread_mutex_lock(&stateMut);
    ackCount = 0; // reset licznika
    pthread_mutex_unlock(&stateMut);

    pthread_mutex_lock(&waitingMut);
    waitingForCS = 0;
    pthread_mutex_unlock(&waitingMut);

    changeState(InRun); // od początku
}
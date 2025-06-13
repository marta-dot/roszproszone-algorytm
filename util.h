#ifndef UTILH
#define UTILH
#include "main.h"

/* typ pakietu */
typedef struct {
    int ts;       /* timestamp (zegar lamporta */
    int src;     // źródło
    int data;     /* przykładowe pole z danymi; można zmienić nazwę na bardziej pasującą */
    char type; // 'B' - babcia || 'S' - studentka
} packet_t;
/* packet_t ma trzy pola, więc NITEMS=3. Wykorzystane w inicjuj_typ_pakietu */
#define NITEMS 4


/* Typy wiadomości */
// #define APP_PKT 1
// #define FINISH 2
#define REQUEST 1
#define RELEASE 2
#define ACK 3
#define UPDATE 4
#define FINISH 5


extern MPI_Datatype MPI_PAKIET_T;
void inicjuj_typ_pakietu();

/* wysyłanie pakietu, skrót: wskaźnik do pakietu (0 oznacza stwórz pusty pakiet), do kogo, z jakim typem */
void sendPacket(packet_t *pkt, int destination, int tag);

int comparePackets(const void *a, const void *b);
void addRequestToQueue(packet_t *pkt);
void removeRequestFromQueue(int src);
int amIFirstInQueue();
void incrementClock(int recived_ts);
void enterCS();

#endif

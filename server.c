#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define FILE 10
#define POLTRONE 20
#define PORT 5587


typedef struct {
/* Struttura che rappresenta un posto
    @param fila     [int]: L'ID della fila
    @param poltrona [int]: L'ID della poltrona
    @param occupato [bool]: 0 = Posto libero, 1 = Posto occupato
    @param codice   [int]: Il codice assegnato alla prenotazione
*/
    int fila;
    int poltrona;
    bool occupato;
    char codice[11];
} Posto;

struct sockaddr_in server_addr;
Posto posti[FILE][POLTRONE];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void gencode(int fila, int poltrona) {
    char code[11];
    const char ALFANUM[36] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < 10; i++) {
        code[i] = ALFANUM[rand()%36];
    }
    code[10] = '\0';
    strcpy(posti[fila][poltrona].codice, code);
}

void* client_handler(void* cs) {
    int client_s = *((int*)cs);
    free(cs);
    char packet[0xFF];

    while(true) {
        ssize_t msg_len = recv(client_s, packet, sizeof(packet), 0);
        if (msg_len == 0) {
            close(client_s);
            pthread_exit(NULL);
        }

        if (packet[0] == 97) { // 'a' = PRENOTARE
            int fila_req = packet[1];
            int poltrona_req = packet[2];

            pthread_mutex_lock(&lock);
            if (fila_req >= FILE || poltrona_req >= POLTRONE) {
                printf("Posto inesistente!\n");
            } else if (posti[fila_req][poltrona_req].occupato == false) {
                posti[fila_req][poltrona_req].occupato = true;
                gencode(fila_req, poltrona_req);
                send(client_s, posti[fila_req][poltrona_req].codice, 10, 0); // Manda il codice di prenotazione al client
            } else {
                send(client_s, "XXXXXXXXXX", 10, 0);
                printf("Posto occupato!\n");
            }
            fflush(stdout);
            pthread_mutex_unlock(&lock);

        } else if(packet[0] == 98) { // 'b' = DISDIRE
            char codice[11];
            bool trovato = false; // FIX: inizializzo a false se non viene trovato
            for (int i = 0; i<10; i++) {
                codice[i] = packet[i+1];
            }
            codice[10] = '\0';
        
            pthread_mutex_lock(&lock);
            
            // FIX: Migliorato il ciclo for per far sì che si fermi quando trovato viene impostato a true e non cicli inutilmente
            for (int i = 0; i < FILE && !trovato; i++) {
                for (int j = 0; j < POLTRONE && !trovato; j++) {
                    if (strcmp(posti[i][j].codice, codice) == 0) {
                        memset(posti[i][j].codice, 0, 10);
                        posti[i][j].occupato = false;
                        printf("Prenotazione con codice %s cancellata\n", codice);
                        send(client_s, "OK", 10, 0); // Manda il codice di prenotazione al client
                        trovato = true;
                    }
                }
            }
            pthread_mutex_unlock(&lock);

            if (!trovato) {
                printf("Prenotazione non esistente!\n");
                send(client_s, "NO", 10, 0);
            }

        } else if(packet[0] == 115) { // s: manda mappa dei posti aggiornata al client
            char mappa[FILE*POLTRONE];

            pthread_mutex_lock(&lock);
            for(int i = 0; i < FILE; i++){
                for(int j = 0; j < POLTRONE; j++){
                    mappa[i*POLTRONE + j] = posti[i][j].occupato ? '1' : '0';
                }
            }
            send(client_s, mappa, FILE*POLTRONE, 0);
            pthread_mutex_unlock(&lock);
        }
        memset(packet, 0, 0xFF);
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int s = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // Permette di riavviare il socket velocemente

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Errore durante il binding del socket!");
        exit(EXIT_FAILURE);
    }

    if (listen(s, 30) < 0) { // Mette il server in ascolto, massimo 30 client in attesa
        perror("Errore durante il listening del socket!");
        exit(EXIT_FAILURE);
    }

    // Accetta le richieste di connessione da parte dei client
    while(true) {
        pthread_t thread;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_s = accept(s, (struct sockaddr*)&client_addr, &client_len);
        int* client_s_cpy = (int*)malloc(sizeof(int));
        *client_s_cpy = client_s;

        // FIX: controllo errore su pthread_create
        if(pthread_create(&thread, NULL, client_handler, (void*)client_s_cpy) < 0){
            perror("Thread create\n");
            exit(EXIT_FAILURE);
        }
        pthread_detach(thread); // FIX: aggiungo il detach così libero la memoria quando il thread termina
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
    }

    return 0;
}

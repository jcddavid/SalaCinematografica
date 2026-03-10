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


#define FILE_CINEMA 10
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
Posto posti[FILE_CINEMA][POLTRONE];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void salva_stato_server() {
    FILE *f = fopen("stato_cinema.dat", "wb");
    if (f != NULL) {
        // Scrive l'intera matrice in un colpo solo in formato binario
        fwrite(posti, sizeof(Posto), FILE_CINEMA * POLTRONE, f);
        fclose(f);
    } else {
        perror("Errore nel salvataggio del file di stato");
    }
}

void* client_handler(void* cs) {
    int client_s = *((int*)cs);
    free(cs);
    char packet[0xFF];

    while(true) {
        ssize_t msg_len = recv(client_s, packet, sizeof(packet), 0);
        if (msg_len <= 0) {
            close(client_s);
            pthread_exit(NULL);
        }

        if (packet[0] == 97) { // 'a' = PRENOTARE
            int fila_req = packet[1];
            int poltrona_req = packet[2];
            int n_posti = packet[3]; 

            pthread_mutex_lock(&lock);
            bool disponibili = true;

            if (fila_req >= FILE_CINEMA || poltrona_req + n_posti > POLTRONE || n_posti <= 0) {
                disponibili = false;
            } else {
                for (int k = 0; k < n_posti; k++) {
                    if (posti[fila_req][poltrona_req + k].occupato) {
                        disponibili = false;
                        break;
                    }
                }
            }

            if (disponibili) {
                char code[11];
                const char ALFANUM[36] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                for (int i = 0; i < 10; i++) {
                    code[i] = ALFANUM[rand()%36];
                }
                code[10] = '\0';

                for (int k = 0; k < n_posti; k++) {
                    posti[fila_req][poltrona_req + k].occupato = true;
                    strcpy(posti[fila_req][poltrona_req + k].codice, code);
                }
                
                salva_stato_server(); // Salva su disco dopo la prenotazione
                send(client_s, code, 10, 0);
            } else {
                send(client_s, "XXXXXXXXXX", 10, 0);
                printf("Richiesta negata: posti già occupati o fuori limite!\n");
            }
            fflush(stdout);
            pthread_mutex_unlock(&lock);

        } else if(packet[0] == 98) { // 'b' = DISDIRE
            char codice[11];
            for (int i = 0; i<10; i++) {
                codice[i] = packet[i+1];
            }
            codice[10] = '\0';
        
            pthread_mutex_lock(&lock);
            int posti_liberati = 0;
            
            for (int i = 0; i < FILE_CINEMA; i++) {
                for (int j = 0; j < POLTRONE; j++) {
                    if (posti[i][j].occupato && strcmp(posti[i][j].codice, codice) == 0) {
                        memset(posti[i][j].codice, 0, 10);
                        posti[i][j].occupato = false;
                        posti_liberati++;
                    }
                }
            }
            
            if (posti_liberati > 0) {
                salva_stato_server(); // Salva su disco dopo la disdetta
            }
            pthread_mutex_unlock(&lock);

            if (posti_liberati > 0) {
                printf("Prenotazione %s cancellata (%d posti liberati)\n", codice, posti_liberati);
                send(client_s, "OK", 10, 0);
            } else {
                printf("Prenotazione non esistente!\n");
                send(client_s, "NO", 10, 0);
            }

        } else if(packet[0] == 115) { // 's' = Manda mappa dei posti aggiornata al client
            char mappa[FILE_CINEMA*POLTRONE];
            pthread_mutex_lock(&lock);
            for(int i = 0; i < FILE_CINEMA; i++){
                for(int j = 0; j < POLTRONE; j++){
                    mappa[i*POLTRONE + j] = posti[i][j].occupato ? '1' : '0';
                }
            }
            send(client_s, mappa, FILE_CINEMA*POLTRONE, 0);
            pthread_mutex_unlock(&lock);
        }
        memset(packet, 0, 0xFF);
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    // --- CARICAMENTO STATO INIZIALE ---
    FILE *f_stato = fopen("stato_cinema.dat", "rb");
    if (f_stato != NULL) {
        // Se il file esiste, ricarica la matrice dei posti
        fread(posti, sizeof(Posto), FILE_CINEMA * POLTRONE, f_stato);
        fclose(f_stato);
        printf("✅ Stato del cinema ripristinato dal disco.\n");
    } else {
        // Altrimenti assicura che sia tutto a zero
        memset(posti, 0, sizeof(posti));
        printf("⚠️ Nessun salvataggio trovato. Creazione di una nuova mappa vuota.\n");
    }

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

    printf("🎥 Server Cinema in ascolto sulla porta %d...\n", PORT);
	
	// Accetta le richieste di connessione da parte dei client
    while(true) {
        pthread_t thread;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_s = accept(s, (struct sockaddr*)&client_addr, &client_len);
        int* client_s_cpy = (int*)malloc(sizeof(int));
        *client_s_cpy = client_s;

        if(pthread_create(&thread, NULL, client_handler, (void*)client_s_cpy) < 0){
            perror("Thread create\n");
            exit(EXIT_FAILURE);
        }
        pthread_detach(thread);
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
    }

    return 0;
}

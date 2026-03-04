#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Indirizzo IP del server
#define SERVER_IP "5.95.16.53"
// Porta su cui il server è in ascolto
#define PORT 5587
// Numero di file (righe) nella sala
#define FILE 10
// Numero di poltrone (colonne) nella sala
#define POLTRONE 20
#define fflush(stdin) while (getchar() != '\n')


void mostramappa(int sockfd){
/*
    - Funzione che stampa la mappa aggiornata dei posti del cinema.
    - Ogni posto viene richiesto singolarmente al server con un pacchetto 's'.
    - Se il posto è disponibile, mostra ✅, altrimenti ❌.
*/
    char packet[1] = {'s'};
    char stati[FILE*POLTRONE + 1];

    send(sockfd, packet, 1, 0);
    recv(sockfd, stati, FILE*POLTRONE, 0);
    // TODO: tutti i recv dovrebbero controllare che non diano errore -1

    

    for(int i = 0; i < FILE; i++){
        printf("fila %d:\n", i);
        for (int j = 0; j < POLTRONE; j++)
        {
            printf("[");
            char stato = stati[i * POLTRONE + j];
            if (stato == '1') {
                printf("❌");       // Posto occupato
            } else if(stato == '0'){
                printf("✅");       // Posto libero
            }else{
                printf("?");
            }
            printf("] ");
        }
        printf("\n");

    }
}

void prenota(int sockfd){
/*
    - Funzione per effettuare una prenotazione.
    - Mostra prima la mappa, poi chiede all'utente fila e poltrona da prenotare.
    - Se il posto è disponibile, riceve e stampa il codice di prenotazione.
*/

    mostramappa(sockfd);        // Stampa lo stato attuale dei posti

    char packet[3];
    packet[0] = 'a';            // Codice comando per prenotazione

    int fila, poltrona;
    printf("Inserisci fila (0-%d): ", FILE - 1);
    scanf("%d", &fila);
    fflush(stdin);
    printf("Inserisci poltrona (0-%d): ", POLTRONE - 1);
    scanf("%d", &poltrona);
    fflush(stdin);

    packet[1] = fila;
    packet[2] = poltrona;

    // Controllo validità input
    if(fila >= FILE || poltrona >= POLTRONE){
        printf("Posto non valido");
    }else{
        send(sockfd, packet, sizeof(packet), 0);        // Invio richiesta al server

        char codice[11] = {0};
        recv(sockfd, codice, 10, 0);        // Ricevo il codice di prenotazione

        if (strcmp(codice, "XXXXXXXXXX") == 0) {
            printf("❌ Posto già occupato.\n");
        } else {
            printf("✅ Prenotazione riuscita. Codice: %s\n", codice);
        }
    }
}

void disdici(int sockfd){
/*
    - Funzione per disdire una prenotazione.
    - Chiede all'utente il codice e lo invia al server con comando 'b'.
    - Riceve conferma o errore.
*/
    char packet[11];
    packet[0] = 'b';        // Codice comando per disdetta

    char codice[11];
    printf("Inserisci il codice di prenotazione: ");
    scanf("%s", codice);

    memcpy(&packet[1], codice, 10);         // Inserisci codice per la richiesta

    send(sockfd, packet, sizeof(packet), 0);

    char risposta[11] = {0};
    recv(sockfd, risposta, 10, 0);      // Ricevo la risposta dal server

    if (strcmp(risposta, "OK") == 0) {
        printf("✅ Prenotazione annullata correttamente.\n");
    } else {
        printf("❌ Codice non trovato.\n");
    }
}

int main(int argc, char *argv[]){
/**
     - Funzione principale.
    - Stabilisce la connessione TCP al server e gestisce il menu interattivo.
*/
    // FIX: metto la possibilità di impostare l'ip tramite argomento passato da riga di comando
    // TODO: aggiornare il README con questa informazione
    char* target_ip = SERVER_IP; // Usa l'IP di default

    // Se l'utente passa un IP da terminale, usa quello
    if (argc > 1) {
        target_ip = argv[1];
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // Crea il socket

    // Configura indirizzo del server
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = inet_addr(target_ip) // FIX: modificata pure qua
    };

    // Connessione al server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Errore di connessione");
        exit(EXIT_FAILURE);
    }

    // Menù principale
    char scelta;
    while (1) {
        printf("\n=== CINEMA DAVID & TARLEV 🎥🍿 ===\n");
        printf("[a] Prenota un posto 💺\n");
        printf("[b] Disdici una prenotazione ❌\n");
        printf("[q] Esci 👋\n");
        printf("Scegli un'opzione: ");
        scanf("%c", &scelta);

        switch (scelta) {
            case 'a':
                prenota(sockfd);
                break;
            case 'b':
                disdici(sockfd);
                break;
            case 'q':
                close(sockfd);
                printf("Arrivederci!\n");
                return 0;
            default:
                printf("Scelta non valida.\n");
        }

        fflush(stdin);

    }
}

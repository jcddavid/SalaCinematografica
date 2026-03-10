#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Indirizzo IP del server
#define SERVER_IP "37.183.92.179"
#define PORT 5587
// Numero di file (righe) nella sala
#define FILE_CINEMA 10
// Numero di poltrone nella sala
#define POLTRONE 20


void fflush_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void mostramappa(int sockfd){
/*
    - Funzione che stampa la mappa aggiornata dei posti del cinema.
    - Ogni posto viene richiesto singolarmente al server con un pacchetto 's'.
    - Se il posto è disponibile, mostra ✅, altrimenti ❌.
*/
    char packet[1] = {'s'};
    char stati[FILE_CINEMA*POLTRONE + 1];

    if (send(sockfd, packet, 1, 0) < 0) {
        perror("Errore nell'invio della richiesta mappa");
        return;
    }
    
    if (recv(sockfd, stati, FILE_CINEMA*POLTRONE, 0) <= 0) {
        perror("Errore nella ricezione della mappa");
        return;
    }

    for(int i = 0; i < FILE_CINEMA; i++){
        printf("fila %d:\t", i);
        for (int j = 0; j < POLTRONE; j++) {
            printf("[");
            char stato = stati[i * POLTRONE + j];
            if (stato == '1') {
                printf("❌");       
            } else if(stato == '0'){
                printf("✅");       
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
    mostramappa(sockfd);        

    char packet[4]; // Aumentato per contenere anche il numero di posti
    packet[0] = 'a';   // Codice comando per prenotazione         

    int fila, poltrona, n_posti;
    printf("\nInserisci fila (0-%d): ", FILE_CINEMA - 1);
    scanf("%d", &fila);
    fflush_stdin();
    
    printf("Inserisci poltrona di partenza (0-%d): ", POLTRONE - 1);
    scanf("%d", &poltrona);
    fflush_stdin();

    printf("Quanti posti adiacenti vuoi prenotare?: ");
    scanf("%d", &n_posti);
    fflush_stdin();

    packet[1] = fila;
    packet[2] = poltrona;
    packet[3] = n_posti; // Nuovo dato inviato al server

    if(fila >= FILE_CINEMA || poltrona + n_posti > POLTRONE || n_posti <= 0){
        printf("❌ Posto o quantità non validi. Riprova.\n");
    } else {
        if (send(sockfd, packet, sizeof(packet), 0) < 0) {
            perror("Errore nell'invio della richiesta di prenotazione");
            return;
        }        

        char codice[11] = {0};
        if (recv(sockfd, codice, 10, 0) <= 0) {
            perror("Errore nella ricezione del codice di prenotazione");
            return;
        }        

        if (strcmp(codice, "XXXXXXXXXX") == 0) {
            printf("❌ Impossibile prenotare. Uno o più posti richiesti sono già occupati.\n");
        } else {
            printf("✅ Prenotazione riuscita per %d posti! Codice: %s\n", n_posti, codice);
            
            // --- LAYER DI PERSISTENZA ---
            FILE *f = fopen("prenotazioni.txt", "a");
            if (f != NULL) {
                if (fprintf(f, "Codice: %s | Fila: %d | Poltrona partenza: %d | Quantita': %d\n", codice, fila, poltrona, n_posti) < 0) {
                    perror("Errore durante la scrittura della prenotazione in locale");
                }
                if (fclose(f) != 0) {
                    perror("Errore durante la chiusura del file delle prenotazioni");
                }
                printf("💾 Prenotazione salvata nel file locale 'prenotazioni.txt'.\n");
            } else {
                perror("⚠️ Errore nell'apertura del file locale. Salvataggio fallito");
            }
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
    packet[0] = 'b';     // Codice comando per disdetta    

    char codice[11];
    printf("Inserisci il codice di prenotazione: ");
    scanf("%10s", codice);
    fflush_stdin();

    memcpy(&packet[1], codice, 10);         
    if (send(sockfd, packet, sizeof(packet), 0) < 0) {
        perror("Errore nell'invio della richiesta di disdetta");
        return;
    }

    char risposta[11] = {0};
    if (recv(sockfd, risposta, 10, 0) <= 0) {
        perror("Errore nella ricezione della risposta alla disdetta");
        return;
    }      

    if (strcmp(risposta, "OK") == 0) {
        printf("✅ Prenotazione annullata correttamente. I posti sono stati liberati.\n");
    } else {
        printf("❌ Codice non trovato.\n");
    }
}

void visualizza_salvate() {
    printf("\n=== 📂 LE TUE PRENOTAZIONI SALVATE ===\n");
    FILE *f = fopen("prenotazioni.txt", "r");
    if (f == NULL) {
        printf("Nessuna prenotazione trovata nel database locale.\n");
        return;
    }
    
    char linea[256];
    while (fgets(linea, sizeof(linea), f) != NULL) {
        printf("- %s", linea);
    }
    
    if (fclose(f) != 0) {
        perror("Errore durante la chiusura del database locale");
    }
}

int main(int argc, char *argv[]){
/**
     - Funzione principale.
    - Stabilisce la connessione TCP al server e gestisce il menu interattivo.
*/
    char* target_ip = SERVER_IP; 

    if (argc > 1) {
        target_ip = argv[1];
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd < 0) {
        perror("Errore nella creazione del socket client");
        exit(EXIT_FAILURE);
    }
    
    // Configura indirizzo del server
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = inet_addr(target_ip) 
    };
    
    // Connessione al server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Errore di connessione al server");
        exit(EXIT_FAILURE);
    }
    
    // Menu' principale
    char scelta;
    while (1) {
        printf("\n=== CINEMA DAVID & TARLEV 🎥🍿 ===\n");
        printf("[a] Prenota posti 💺\n");
        printf("[b] Disdici una prenotazione ❌\n");
        printf("[c] Visualizza prenotazioni salvate in locale 📂\n");
        printf("[q] Esci 👋\n");
        printf("Scegli un'opzione: ");
        scanf("%c", &scelta);
        fflush_stdin();

        switch (scelta) {
            case 'a': prenota(sockfd); break;
            case 'b': disdici(sockfd); break;
            case 'c': visualizza_salvate(); break;
            case 'q':
                if (close(sockfd) < 0) {
                    perror("Errore durante la chiusura del socket");
                }
                printf("Arrivederci!\n");
                return 0;
            default:
                printf("Scelta non valida.\n");
        }
    }
    return 0;
}

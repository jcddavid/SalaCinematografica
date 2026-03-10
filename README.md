# 🎬 Sistema di prenotazione posti per sala cinematografica

## 📌 Introduzione

Questo progetto rappresenta un sistema di prenotazione posti per una sala cinematografica.

Un processo su una macchina **server** gestisce una mappa di posti per una sala. Ciascun posto è caratterizzato da:

- Un numero di **fila**
- Un numero di **poltrona**
- Un **flag** indicante se il posto è già stato prenotato o meno

Il server accetta e processa richieste di prenotazione da uno o più **client** (anche su macchine diverse).

Il client offre all'utente le seguenti funzionalità:

1. 🔍 **Visualizzare la mappa** dei posti per individuare quelli disponibili.
2. 📨 **Inviare al server** l’elenco dei posti da prenotare (identificati tramite `numero di fila` e `numero di poltrona`).
3. ✅ **Ricevere conferma** dal server con l’avvenuta prenotazione e un `codice di prenotazione`.
4. ❌ **Disdire una prenotazione** esistente fornendo il relativo codice.

🧵 Il server gestisce le richieste dei client in modo **concorrente**, assegnando un **thread dedicato** a ogni client al momento della connessione.


## 📚 Discussione delle Scelte Progettuali

Il progetto adotta un'architettura client-server per gestire in modo centralizzato la prenotazione di posti in una sala cinematografica, garantendo coerenza dei dati e interazione remota da parte di più utenti.

### 🧠 Scelte Architetturali

- **Architettura Client-Server TCP**  
  Il server gestisce la mappa dei posti e accetta connessioni da più client, sfruttando il protocollo TCP per garantire l'affidabilità della comunicazione.

- **Gestione concorrente tramite thread**  
  Ogni nuova connessione da parte di un client viene gestita tramite un nuovo `thread`, consentendo richieste simultanee da più utenti in parallelo.
  L'accesso al buffer `posti`, che è una risorsa condivisa, viene gestito da un semaforo di tipo mutex.

- **Modello dati semplice e diretto**  
  La sala è rappresentata come una matrice 10x20 di strutture `Posto`, ciascuna con attributi per fila, poltrona, stato di occupazione e codice di prenotazione.

### ⚙️ Tecniche e Metodologie Utilizzate

- **Scambio dati minimale ed efficiente**
  I comandi tra client e server sono codificati come caratteri (`'a'`, `'b'`, `'s'`), con payload minimali per ridurre la complessità e migliorare le performance.

- **Visualizzazione intuitiva lato client**  
  I posti disponibili e occupati sono mostrati usando simboli grafici (`✅`, `❌`) per semplificare l'esperienza utente anche da terminale.

- **Gestione dei codici prenotazione**  
  Ogni prenotazione viene identificata da un codice alfanumerico generato casualmente e trasmesso al client, che lo usa anche per eventuali disdette.

## 📖 Manuale d'uso
Per eseguire il client, usare il comando `./client [IP DEL SERVER]`
Per eseguire il server, usare il comando `./server`
### ⚙️ Compilazione
Per la compilazione, utilizzare il comando `gcc server.c -o server` e `gcc client.c -o client`.

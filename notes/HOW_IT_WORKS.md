# Ako cely system funguje (client - server)

Tento dokument popisuje celkovy tok aplikacie
od spustenia servera az po ukoncenie simulacie.

Cielom je vysvetlit:
- ako spolu komunikuje client a server
- ako su pouzite procesy a vlakna
- kde sa neskor doplni simulacna logika

Dokument je pisany ako poznamky pre seba.

---

## Zakladna myslienka

Aplikacia je rozdelena na:
- client proces
- server proces

Client a server spolu komunikuju pomocou TCP socketov.
Server obsahuje simulacnu logiku.
Client sluzi ako ovladanie a zobrazovanie.

---

## Spustenie aplikacie

### 1. Spustenie servera

Najprv sa spusti server:

```
./server 555
```

SERVER :

- vytvori TCP socket
- zacne pocuvat na zadanom porte
- spusti simulacne vlakno
- caka na klienta
- ma 2 vlakna (hlavne a simulacne)

1. Hlavne vlakno
- robi ```listen()``` na porte a ```accept()``` klienta
- prijima a spracovava spravy od klienta (HELLO, START_SIM, STOP_SIM)
2. Simulacne vlakno
- caka na prikaz ```START_SIM```
- vykona simulaciu (vsetky replikacie)
- priebezne posiela ```PROGRESS``` klientovi
- po konci posle ```SUMMARY_DONE```
- toto vlakno riesi simulacnu logiku

Server bezi stale, aj ked ziadny klient nie je pripojeny.
Server sa nezasekne pocas simulacie

---

### 2. Spustenie klienta

Potom sa spusti client:

```
./client 127.0.0.1 5555
```

CLIENT :

- vytvori TCP socket
- pripoji sa na server
- posle spravu HELLO
- ma dve vlakna (Input a Reciever)

1. Input vlakno 
- cita vstup od pouzivatela (klavesnica)
- posiela spravy serveru (```START_SIM```, ```SET_MODE```, ```SET_VIEW```, ...)

2. Receiver vlakno
- neustale cita spravy zo servera
- vypisuje ich do konzoly (```STATUS```, ```PROGRESS```, ```SUMMARY_DONE```)

Klient sa nezasekne pri cakani na server
moze naraz pisat prikazy a aj posielat data

---

## Handshake (HELLO / WELCOME)

Po pripojeni:

1. Client posle:
    - MSG_HELLO
2. Server odpovie:
    - MSG_WELCOME

Tymto sa potvrdi, ze:
- spojenie funguje
- client a server pouzivaju rovnaky protokol

---

## Architektura vlakien

### Client vlakna

Client ma 2 vlakna:

#### 1. Input thread
- cita prikazy z klavesnice
- posiela spravy serveru

Priklady:
- START_SIM
- SET_MODE
- STOP_SIM
- BYE

#### 2. Receiver thread
- cita spravy zo servera
- vypisuje ich do konzoly

Priklady:
- STATUS
- PROGRESS
- SUMMARY_DONE

Tieto dve vlakna bezia paralelne.

---

### Server vlakna

Server ma tiez 2 vlakna:

#### 1. Hlavne vlakno
- pocuva na porte
- prijima klienta (accept)
- spracovava spravy od klienta

#### 2. Simulacne vlakno
- caka na START_SIM
- po starte simulacie vykonava replikacie
- priebezne posiela PROGRESS spravy
- po konci posle SUMMARY_DONE

Simulacne vlakno bezi nezavisle od komunikacie s klientom.

---

## Tok simulacie (aktualny stav)

### Start simulacie

1. Client posle MSG_START_SIM
2. Server:
    - ulozi parametre simulacie
    - nastavi g_sim_running = 1
    - zobudi simulacne vlakno

3. Simulacne vlakno:
    - vstupi do cyklu replikacii

---

### Replikacie

Pre kazdu replikaciu:
- simulacne vlakno spravi jednu iteraciu (zatial sleep)
- posle MSG_PROGRESS (aktualna / celkova replikacia)

Toto bude neskor nahradene realnou logikou random walk.

---

### Ukoncenie simulacie

Simulacia sa skonci:
- po spracovani vsetkych replikacii
- alebo po prijati MSG_STOP_SIM

Server potom:
- posle MSG_SUMMARY_DONE
- posle STATUS spravu
- nastavi g_sim_running = 0

---

## Ukoncenie spojenia

Client moze poslat:
- MSG_BYE

Server:
- uzavrie socket klienta
- server proces dalej bezi a caka na dalsieho klienta

---

## Co je hotove a co este nie

### Hotove:
- procesy client / server
- socketova komunikacia
- binarny protokol
- vlakna na oboch stranach
- zakladny tok simulacie

### Nehotove (zatial):
- realna implementacia random walk
- vypocet statistik (avg_steps, prob_k)
- posielanie SUMMARY_CELL dat

---

## Preco je architektura nastavena takto

- komunikacia je oddelena od simulacie
- simulacia bezi vo vlastnom vlakne
- protokol je jednoduchy a rozsiritelny
- nove spravy sa daju doplnit bez zmeny zakladu

Tato architektura splna poziadavky predmetu
a umoznuje postupne doplnat dalsiu funkcionalitu.

## Kde su pouzite MUTEXY 

Su pouzite len na strane SERVERA :

### 1. Ochrana pripojeneho klienta

- chrani premennu g_client_fd
- zabezbecuje ze vlakna servera nepouzivaju client_fd naraz.
- bez mutexu by simulacne vlakno mohlo posielat data na socket ktory uz bol zatvoreny hlavnym vlaknom.
### 2. Ochrana stavu simulacie 

- zabezpecuje synchronizaciu medzi hlavnym vlaknom a simulacnym vlaknom...
- ak napr hlavne vlakno prijme spravu ```START_SIM``` tak sa simulacne vlakno zapne 


# Random walk - poznamky (aktualny stav projektu)

Tento dokument popisuje aktualny stav implementacie projektu.
V tomto momente mame hotovu:
- komunikaciu klient <-> server pomocou TCP socketov
- jednoduchy binarny protokol (header + payload)
- viacvlaknovu architekturu (server aj klient)
- zakladnu kostru simulacie (bez realnej logiky random walk)

Zamerom tejto fazy bolo mat stabilnu komunikaciu a architekturu,
do ktorej sa bude neskor doplnat samotna simulacia.

---


## Zakladna architektura

Aplikacia je rozdelena na dva procesy:

- client
- server

### Client
- spusta sa ako samostatny proces
- zodpoveda za:
    - citanie vstupu od pouzivatela (prikazy)
    - zobrazovanie vystupu zo servera
    - komunikaciu so serverom

Client pouziva 2 vlakna:
1. input thread
    - cita prikazy z klavesnice
    - posiela spravy serveru (START_SIM, SET_MODE, STOP_SIM, BYE)
2. receiver thread
    - cita spravy zo servera
    - vypisuje STATUS, PROGRESS a dalsie spravy

Tymto je splnena poziadavka na pracu s vlaknami na strane klienta.

---

### Server
- spusta sa ako samostatny proces
- existuje nezavisle od klienta
- moze bezat aj ked klient skonci

Server pouziva 2 vlakna:
1. hlavne vlakno
    - pocuva na TCP porte
    - prijima klienta (accept)
    - spracovava spravy od klienta
2. simulacne vlakno
    - caka na START_SIM
    - po starte simulacie vykonava replikacie
    - priebezne posiela PROGRESS spravy klientovi

Simulacne vlakno zatial neobsahuje realnu logiku random walk,
iba simuluje pracu pomocou sleep.

---

## IPC - komunikacia medzi procesmi

Komunikacia prebieha pomocou:
- TCP socketov
- IPv4
- lokalne (127.0.0.1) alebo v ramci siete

Kazda sprava ma tvar:
- fixny header
- volitelny payload

Tymto je splnena poziadavka na IPC pomocou socketov.

---
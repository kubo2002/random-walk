# Technická dokumentácia - Náhodná pochôdzka

## 1. Štruktúra projektu
Projekt je rozdelený do troch hlavných častí:
- **Server**: Zodpovedá za simuláciu, prácu so svetom a ukladanie výsledkov.
- **Klient**: Zabezpečuje interakciu s používateľom, vykresľovanie a posielanie príkazov.
- **Common**: Spoločné hlavičkové súbory a komunikačný protokol.

### Moduly:
- `server.c`: Hlavná logika servera, správa vlákien a sieťovej komunikácie.
- `simulation.c`: Samotný algoritmus náhodnej pochôdzky.
- `world.c`: Správa herného sveta (prekážky, generovanie, BFS, I/O).
- `client.c`: Hlavný cyklus klienta.
- `menu.c`: Spracovanie vstupov od používateľa.
- `render.c`: Vykresľovanie sveta v termináli.
- `protocol_io.c`: Nízkoúrovňové funkcie pre posielanie/prijímanie správ.

## 2. Architektúra a vlákna
Aplikácia využíva viacvláknový model na strane servera aj klienta (knižnica `pthread`).

### Server:
1. **Hlavné vlákno (Main Thread)**:
   - Inicializuje server socket (`network_listen_on_port`).
   - V nekonečnom cykle čaká na pripojenie klienta (`accept`).
   - Po pripojení klienta číta jeho správy v cykle pomocou `protocol_receive`.
   - Reaguje na príkazy ako štart simulácie, zmena módu alebo stop.
2. **Simulačné vlákno (Simulation Thread)**:
   - Vytvorené pri štarte servera.
   - Väčšinu času čaká na podmienkovú premennú (`g_sim_cv`).
   - Po signále vykonáva simuláciu náhodnej pochôdzky.
   - Priebežne posiela dáta klientovi cez socket.

### Klient:
1. **Hlavné vlákno (UI Thread)**:
   - Zobrazuje textové menu a číta vstupy od používateľa.
   - Posiela príkazy serveru.
   - Po spustení simulácie čaká na jej ukončenie (synchronizácia cez ENTER).
2. **Receiver vlákno**:
   - Beží na pozadí počas celého spojenia.
   - Prijíma správy od servera (statusy, progres, kroky interaktívneho módu, summary dáta).
   - Volá funkcie modulu `render.c` na aktualizáciu zobrazenia.

## 3. Komunikačný protokol (IPC)
Komunikácia (Inter-Process Communication) prebieha cez **TCP sockety** (AF_INET, SOCK_STREAM).
- **Socket**: Vytvorený pomocou systémového volania `socket()`.
- **Protocol**: Vlastný binárny protokol definovaný v `protocol.h`.
- **Marshalling**: Dáta sú prenášané v sieťovom poradí bajtov (Big Endian) pomocou funkcií `htonl/ntohl` pre zabezpečenie kompatibility.

Štruktúra správy:
```c
typedef struct {
    uint32_t type;         // Typ správy (napr. MSG_START_SIM)
    uint32_t payload_len;  // Dĺžka dát nasledujúcich za hlavičkou
} msg_header_t;
```
Za hlavičkou nasleduje voliteľný payload (napr. štruktúra `start_sim_t` alebo binárne dáta mapy sveta).

## 4. Synchronizácia a kritické oblasti
Na zabezpečenie konzistencie dát v multithreadovom prostredí používame:

### Mutexy (`pthread_mutex_t`):
- `g_client_lock`: Chráni prístup k file descriptoru klienta (`g_client_fd`). Zabezpečuje, že simulácia neposiela dáta do zatvoreného socketu a že naraz posiela dáta len jedno vlákno.
- `g_sim_lock`: Chráni stavové premenné simulácie (`g_sim_running`, `g_sim_params`, `g_world`). Zabezpečuje, aby sa parametre nemenili počas prebiehajúceho výpočtu.

### Podmienkové premenné (`pthread_cond_t`):
- `g_sim_cv`: Používa sa na signalizáciu medzi hlavným vláknom (ktoré prijme príkaz na štart) a simulačným vláknom (ktoré čaká na prácu). Tým sa šetrí CPU, keď server nerobí simuláciu.

## 5. Algoritmy
- **Náhodná pochôdzka**:
  - Implementovaná v `simulation.c`.
  - Každý krok sa náhodne vyberá smer (hore, dole, vľavo, vpravo) podľa zadaných pravdepodobností.
  - **Wrap-around**: Ak je svet bez prekážok, pri prekročení hranice sa objekt objaví na opačnej strane (modulo logika).
  - **Prekážky**: Ak je svet s prekážkami, krok na políčko typu `CELL_OBSTACLE` (alebo mimo hraníc) je ignorovaný (objekt zostáva stáť).
- **BFS (Breadth-First Search)**:
  - Použitý v `world.c` pri generovaní sveta.
  - Zabezpečuje, že z každého voľného políčka existuje cesta do cieľa [0,0]. Ak nie, svet sa pregeneruje.

## 6. Generatívna umelá inteligencia
Pri vypracovaní semestrálnej práce bola použitá umelá inteligencia (Junie od JetBrains) na:
- Generovanie kostry sieťovej komunikácie.
- Implementáciu BFS algoritmu pre overenie dosiahnuteľnosti.
- Formátovanie dokumentácie a písanie komentárov v slovenskom jazyku.
Všetok kód bol následne manuálne skontrolovaný a upravený tak, aby spĺňal funkčné požiadavky zadania pre 62 bodov.

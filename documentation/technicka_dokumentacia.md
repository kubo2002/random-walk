# Technická dokumentácia - Náhodná pochôdzka

## 1. Úvod
Tento projekt predstavuje simuláciu "Náhodnej pochôdzky" (Random Walk) v 2D priestore s využitím architektúry klient-server. Hlavným cieľom je simulovať pohyb agenta (chodca) na mriežke, kde sa snaží dosiahnuť cieľ (stred mapy) na základe definovaných pravdepodobností pohybu.

## 2. Štruktúra projektu (UML)

Aplikácia je rozdelená na klientsku časť, serverovú časť a spoločné knižnice.

```text
+-------------------+       TCP Socket (5555)       +-------------------+
|      KLIENT       | <---------------------------> |      SERVER       |
+-------------------+                               +-------------------+
| - Main Thread     |                               | - Main Thread     |
| - Receiver Thread |                               | - Simulation Th.  |
+---------+---------+                               +---------+---------+
          |                                                   |
          v                                                   v
+---------+---------+                               +---------+---------+
|     COMMON        |                               |     SERVER LIB    |
+-------------------+                               +-------------------+
| - Protocol        |                               | - World logic     |
| - Config          |                               | - Simulation      |
+-------------------+                               +-------------------+
```

### Popis adresárovej štruktúry:
- `client/`: Zdrojové kódy a hlavičkové súbory klienta.
- `server/`: Zdrojové kódy a hlavičkové súbory servera.
- `common/`: Spoločný komunikačný protokol a konštanty.
- `documentation/`: Projektová dokumentácia.

## 3. Procesy aplikácie
Aplikácia pozostáva z dvoch hlavných procesov:
1.  **Rodičovský proces (Klient)**: Zodpovedá za interakciu s používateľom (vstupy, menu), vizualizáciu dát a správu perzistentných údajov (prikazuje serveru ukladanie/načítanie stavu). Pri štarte simulácie vytvára proces servera (bod P7).
2.  **Potomkovský proces (Server)**: Vykonáva simulačnú logiku, spravuje herný svet (generovanie prekážok, kolízie) a vykonáva výpočtovo náročné simulácie. Je spustený klientom pomocou systémového volania `fork()` a následného `exec()` (bod P8).

### Životný cyklus servera (P5)
Proces server existuje, pokiaľ beží ním spravovaná simulácia, bez ohľadu na to, či klient, ktorý ho vytvoril, stále beží. Ak klient počas simulácie zanikne, server dokončí výpočet a následne zanikne. Ak simulácia skončí, proces server zaniká.

## 4. Vlákna a ich účel
Obidve časti aplikácie využívajú vlákna (POSIX Threads) na zabezpečenie asynchrónneho správania:
- **Klient (receiver_thread)**: Toto vlákno beží na pozadí a neustále počúva na sockete. Prijíma správy od servera (progres, kroky simulácie, výsledky) a aktualizuje stav rozhrania, zatiaľ čo hlavné vlákno čaká na vstup používateľa alebo vykresľuje mapu.
- **Server (simulation_thread)**: Zabezpečuje samotný výpočet náhodnej pochôdzky. Oddelenie do samostatného vlákna umožňuje serveru v hlavnom vlákne stále prijímať riadiace príkazy od klienta (napr. ukončenie) aj počas bežiaceho výpočtu.

## 5. Medziprocesová komunikácia (IPC)
V projekte sú využité nasledujúce typy IPC (bod P9):
1.  **TCP Sockety**: Primárny komunikačný kanál medzi klientom a serverom. Používajú sa na prenos štruktúrovaných správ. TCP rozhranie (IP adresa a port) sa definuje pri vytvorení procesu server (bod P10).
2.  **Správa procesov (fork/exec)**: Mechanizmus na vytvorenie a spustenie serverového procesu priamo z klienta.
3.  **Signály/Ukončenie**: Korektné ukončenie procesov pri zatvorení aplikácie.

*Poznámka: V aktuálnej verzii sa nepoužíva zdieľaná pamäť ani dátovody (pipes), komunikácia je plne zabezpečená pomocou sieťových socketov.*

## 6. Synchronizačné problémy a riešenia
Pri použití viacerých vlákien v serveri vznikali nasledujúce problémy:
-   **Prístup k sieťovému socketu**: Aby nedochádzalo ku kolízii pri odosielaní správ zo simulačného vlákna a hlavného vlákna servera, bol implementovaný mutex `g_client_lock`.
-   **Synchronizácia štartu simulácie**: Simulačné vlákno musí čakať, kým klient nepošle kompletnú konfiguráciu. Toto je vyriešené pomocou podmienkovej premennej (`pthread_cond_t g_sim_cv`) a mutexu `g_sim_lock`. Vlákno spí, kým nedostane signál o pripravenosti dát.

## 7. Ďalšie kľúčové problémy
-   **Priechodnosť sveta**: Pri generovaní náhodných prekážok bolo nutné zabezpečiť, aby bol cieľ (stred) dosiahnuteľný z každej voľnej bunky. Na tento účel bol implementovaný algoritmus **BFS** (Breadth-First Search).
-   **Toroidný svet**: Implementácia sveta bez okrajov vyžadovala ošetrenie pretečenia súradníc (modulo operácie), aby sa chodec po prejdení okraja objavil na opačnej strane.
-   **Perzistencia**: Implementácia ukladania a načítania binárneho stavu simulácie umožnila pokračovať v prerušenej práci.

## 8. Implementačné detaily simulácie
- **Pohyb**: Chodec sa pohybuje v 4 smeroch na základe zadaných pravdepodobností (v promile).
- **Svet**: Podporuje toroidný svet, svet s prekážkami a načítanie zo súboru.

## 9. Perzistencia dát
- **Stav simulácie**: Binárny súbor `.state.bin` pre obnovu.
- **Výsledky**: CSV súbor so štatistikami pre sumárny mód.

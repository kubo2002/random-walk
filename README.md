# Používateľská príručka - Náhodná pochôdzka

Aplikácia "Náhodná pochôdzka" je simulačný nástroj umožňujúci skúmať správanie agenta v 2D prostredí pri náhodnom pohybe.

## 1. Požiadavky a Inštalácia
Pre spustenie aplikácie potrebujete mať nainštalovaný prekladač C (napr. GCC) a nástroj `cmake`.

### Kompilácia
V koreňovom priečinku projektu vykonajte nasledujúce príkazy:
```bash
mkdir build
cd build
cmake ..
make
```
Po úspešnej kompilácii vzniknú v priečinku `build` dva spustiteľné súbory: `client` a `server`.

## 2. Spustenie aplikácie
Aplikácia sa spúšťa prostredníctvom klienta. Server je automaticky spúšťaný klientom v prípade potreby.
```bash
./client [IP_ADRESA] [PORT]
```
- **IP_ADRESA**: Voliteľný parameter (predvolené 127.0.0.1).
- **PORT**: Voliteľný parameter (predvolené 5555).

## 3. Hlavné menu
Po spustení klienta sa zobrazí hlavné menu s nasledujúcimi možnosťami:
1. **Nova simulacia**: Umožňuje definovať parametre pre novú simuláciu.
2. **Opatovne spustenie**: Načíta predchádzajúci uložený stav a pokračuje v simulácii.
3. **Koniec**: Bezpečne ukončí klienta aj bežiaci proces servera.

## 4. Konfigurácia simulácie
Pri vytváraní novej simulácie zadávate tieto parametre:
- **Mód simulácie**:
    - `0 (Interactive)`: Vizualizácia pohybu jedného chodca v reálnom čase.
    - `1 (Summary)`: Hromadná simulácia pre každú bunku mriežky s výslednou štatistickou tabuľkou.
- **Typ sveta**:
    - `0`: Bez okrajov (agent prechádza cez steny na opačnú stranu).
    - `1`: S náhodnými prekážkami (server overuje priechodnosť k cieľu).
    - `2`: Načítanie mapy zo súboru.
- **Rozmery sveta**: Šírka a výška mriežky.
- **Pravdepodobnosti pohybu**: Štyri celé čísla určujúce smer (hore, dole, vľavo, vpravo). Ich súčet musí byť presne 1000 (reprezentuje 100%).
- **Replikácie a K**:
    - Počet replikácií pre každú štartovaciu pozíciu.
    - Limit krokov (K), po ktorom sa simulácia považuje za neúspešnú.

## 5. Legenda mapy (Interaktívny mód)
V interaktívnom móde sa zobrazuje mriežka s nasledujúcimi znakmi:
- `C`: Aktuálna pozícia agenta.
- `O`: Prekážka (nepriechodná stena).
- `x`: Cesta, ktorou agent už prešiel.

## 6. Výstupy simulácie
V sumárnom móde sa po skončení zobrazia v termináli tabuľky:
- **Average Steps**: Priemerný počet krokov potrebných na dosiahnutie cieľa z danej bunky.
- **Probability of K**: Pravdepodobnosť, že agent dosiahne cieľ do limitu K krokov.

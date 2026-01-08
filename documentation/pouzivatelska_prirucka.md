# Používateľská príručka - Náhodná pochôdzka

Tento dokument popisuje, ako spustiť a ovládať aplikáciu "Náhodná pochôdzka".

## 1. Spustenie aplikácie

Aplikácia pozostáva z dvoch častí: **server** a **klient**. Pre správne fungovanie je potrebné najprv spustiť server.

### Spustenie servera:
```bash
./server [port]
```
- `port`: (voliteľné) port, na ktorom bude server počúvať (predvolený je 5555).

### Spustenie klienta:
```bash
./client [ip_adresa] [port]
```
- `ip_adresa`: IP adresa servera (pre lokálny beh použite 127.0.0.1).
- `port`: port servera.

## 2. Ovládanie (Klientske menu)

Po spustení a pripojení klienta sa zobrazí hlavné menu:

1. **Nova simulacia**: Umožňuje nastaviť parametre pre nový beh.
2. **Pripojit sa k simulacii**: Umožňuje sledovať už bežiacu simuláciu.
3. **Opatovne spustenie simulacie**: Načíta predtým uložený svet a spustí simuláciu s novým počtom replikácií.
4. **Koniec**: Korektne ukončí aplikáciu a odpojí sa od servera.

## 3. Nastavenie simulácie

Pri vytváraní novej simulácie zadávate tieto parametre:
- **Typ sveta**: 
  - `0`: Bez prekážok (pohyb cez okraj vás vráti na opačnú stranu - wrap-around).
  - `1`: S náhodnými prekážkami (cesta k stredu [0,0] je vždy zaručená).
  - `2`: Načítať svet zo súboru.
- **Rozmery sveta**: Šírka a výška (stred je vždy [0,0]).
- **Počet replikácií**: Koľkokrát sa má simulácia zopakovať z každého políčka.
- **Maximálny počet krokov (K)**: Limit pre výpočet pravdepodobnosti dosiahnutia cieľa.
- **Pravdepodobnosti pohybu**: Zadáte 4 čísla (hore, dole, vľavo, vpravo), ktorých súčet musí byť 1000.
- **Meno súboru pre výsledky**: Názov CSV súboru, kam server uloží finálne štatistiky.
- **Mód**: 
  - `Interaktívny`: Vidíte pohyb chodca v reálnom čase priamo v termináli.
  - `Sumárny`: Server vypočíta štatistiky a pošle ich klientovi na zobrazenie.

## 4. Výsledky

Výsledky simulácie sa ukladajú na serveri do CSV súboru. Súbor obsahuje pre každé políčko:
- Priemerný počet krokov do stredu.
- Pravdepodobnosť dosiahnutia stredu v limite K krokov.

Okrem toho server automaticky ukladá binárny súbor sveta s predponou `world_`, ktorý môžete neskôr použiť vo voľbe "Opatovne spustenie simulacie".

### Čo všetko program spĺňa (Body P1 - P11)

*   **P1: Funguje na frios2** - Kód je v Cčku, používa štandardné veci, takže na friose by nemal byť problém.
*   **P2: Klient proces** - Program `client` je samostatný proces, v ktorom sa ovláda menu.
*   **P3: Server proces** - Server nevzniká hneď, ale až keď si ho klient zapne (pri novej simulácii) cez `fork` a `exec`.
*   **P4: Viac klientov** - Cez sockety sa dá teoreticky pripojiť aj z iného klienta.
*   **P5: Server končí** - Keď simulácia dobehne, server sa sám vypne a uvoľní port.
*   **P6: Viac simulácií naraz** - Keďže používame porty, môže bežať viac serverov a klientov naraz na jednom stroji.
*   **P7: Čo robí klient** - Klient rieši celé menu, pýta sa na dáta a kreslí mapu/tabuľky.
*   **P8: Čo robí server** - Server počíta kroky, generuje mapy a ukladá to do súborov.
*   **P9: Sockety** - Na komunikáciu používame TCP sockety.
*   **P10: Rozhranie** - Server pri štarte povie, na akom porte počúva.
*   **P11: Vlákna** - Server aj klient majú vlákna, aby vedeli naraz prijímať správy a niečo iné robiť.

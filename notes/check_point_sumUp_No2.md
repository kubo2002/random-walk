# Checkpoint #2 - co sa zmenilo od posledneho velkeho checkpointu

Tento dokument popisuje, co pribudlo v projekte po tom,
co sme mali hotovu len komunikaciu klient-server a "dummy" simulaciu.

Aktualny stav:
- server realne pocita random walk
- server posiela summary vysledky (AVG_STEPS alebo PROB_K)
- klient summary uklada a pekne vypise ako tabulku

---

## 1) Co bolo predtym (checkpoint #1)

Predtym :
- client-server spojenie cez TCP socket
- binarny protokol (header + payload)
- 2 vlakna na klientovi (input + receiver)
- 2 vlakna na serveri (main + simulation)
- START_SIM spustil iba "dummy" loop (sleep) a posielal PROGRESS
- SUMMARY sa neposielalo, alebo sa len testovalo

Ciel checkpointu #2 bol:
- implementovat skutocnu simulaciu random walk
- vypocitat statistiky pre summary mode
- poslat vysledky na klienta a zobrazit ich

---

## 2) Nova cast: modul simulation.c

Pribudol modul:
- server/src/simulation.c
- server/headers/simulation.h

Tento modul obsahuje samotnu logiku pohybu chodca:
- vyber smer podla pravdepodobnosti (promile 0..1000)
- wrap-around sveta (torus):
    - ked x ide mimo [0..w-1], prejde na opacnu stranu
    - rovnako pre y

Najdolezitejsia funkcia:
- simulate_walk_torus(...)
  Vrati pocet krokov potrebnych na dojdenie do stredu,
  alebo max_steps+1 ak sa do limitu nepodari.

Poznamka:
- rand() sa seeduje na serveri (srand(time(NULL))) raz v main.

---

## 3) Zmena na serveri: realna summary simulacia

Server simulation_thread uz nerobi len usleep.
Teraz robi skutocny vypocet:

Po MSG_START_SIM server ulozi parametre a zobudi simulation_thread.

Simulation_thread:
1) precita parametre (w, h, reps, K, p_up/p_down/p_left/p_right, view)
2) pripravi si polia pre statistiky:
    - total_steps[w*h] ako uint64_t
    - success_k[w*h] ako uint32_t
3) spusti cyklus replikacii:
   for r = 1..reps:
    - pre kazdu bunku (xx,yy) v svete:
        - spravi jednu random walk trajektoriu do stredu
        - prida kroky do total_steps
        - ak kroky <= K, zvysi success_k
    - po replikacii posle MSG_PROGRESS (r/reps)

Po dokonceni replikacii server posle vysledky pre kazdu bunku:
- MSG_SUMMARY_CELL pre kazde (x,y)
- a nakoniec MSG_SUMMARY_DONE

View urcuje co sa posiela do value_fixed:
- view 0 (AVG_STEPS):
  value_fixed = avg_steps * 100
- view 1 (PROB_K):
  value_fixed = prob * 10000
  kde prob = success_k / reps

x,y sa posielaju relativne k stredu (0,0 je stred):
- cx = xx - center_x
- cy = yy - center_y

---

## 4) Bug fix: spam STATUS / DONE

Pri prechode na realnu simulaciu vznikol bug:
- simulation_thread robil simulaciu vo vnutornom cykle "reps krat"
- a vnutri mal dalsi cyklus reps
- vysledok bol reps*reps a opakovane posielane STATUS a DONE

Fix bol:
- simulation_thread vykona simulaciu presne raz na jeden START_SIM
- po skonceni nastavi g_sim_running = 0
- potom sa vrati na cond_wait a caka na dalsi START_SIM

---

## 5) Zmena na klientovi: render summary ako tabulku

Predtym klient vypisoval kazdu CELL ako text.

Teraz klient:
- pri 'start' pripravi buffer render_summary_begin(w,h,view)
- pri MSG_SUMMARY_CELL ulozi hodnotu do 2D pola (indexovane cez center)
- pri MSG_SUMMARY_DONE vypise tabulku naraz render_summary_end(view)

Tym sa dosiahlo:
- cisty vypis
- lepsia citatelnost
- obhajitelna architektura (render oddeleny modul)

---

## 6) View 0 / View 1

Klient podporuje prikaz:
- view 0 (avg_steps)
- view 1 (prob_k)

Klient si drzi globalnu premennu:
- g_current_view

Pri start:
- posiela s.view = g_current_view
- render buffer je pripraveny pre g_current_view
  Pri DONE:
- tabulka sa vytlaci podla g_current_view

---

## 7) Co je teraz hotove

Hotove:
- realny random walk na serveri
- replikacie a progress
- avg_steps statistika
- prob_k statistika
- summary cells a done sprava
- klientsky render tabulky (view 0 aj view 1)

Neimplementovane (zatial):
- interactive mode (mode 0)
- trajektoria krok po kroku
- ukladanie do suboru
- zmena parametrov (K/reps/w/h) cez prikazy

---

## 8) Dalsi krok (checkpoint #3)

Nasledujuci krok je implementovat interactive mode:
- server posiela pozicie chodca krok po kroku
- klient to zobrazi (vypis alebo ascii grafika)

Tym doplnime druhy rezim simulacie.
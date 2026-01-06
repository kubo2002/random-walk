# Komunikacny protokol (client <-> server)

Tento dokument popisuje binarny protokol pouzity na komunikaciu
medzi klientom a serverom.

Protokol je navrhnuty tak, aby bol:
- jednoduchy
- citatelny
- lahko rozsiritelny

---

## Format spravy

Kazda sprava ma tvar:

[msg_header][payload]

### Header

Header ma fixnu velkost a obsahuje:
- type        (uint32)
- payload_len (uint32)

Vsetky cisla su posielane v network byte order.

---

## Typy sprav (client -> server)

MSG_HELLO
- klient sa predstavi serveru
- payload: hello_t

MSG_START_SIM
- spusti novu simulaciu
- payload: start_sim_t

MSG_SET_MODE
- zmena modu (interactive / summary)
- payload: set_mode_t

MSG_SET_VIEW
- zmena zobrazenia v summary mode
- payload: set_view_t

MSG_STOP_SIM
- zastavi simulaciu
- payload: none

MSG_BYE
- korektne odpojenie klienta
- payload: none

---

## Typy sprav (server -> client)

MSG_WELCOME
- odpoved na HELLO
- payload: none

MSG_STATUS
- textova informacna sprava
- payload: text

MSG_PROGRESS
- priebezny stav simulacie
- payload: progress_t

MSG_SUMMARY_DONE
- signalizuje koniec posielania summary dat
- payload: none

---

# Preco```ntohl```
Server posle :
```
type = 1
pyload_len = 16
```
Na little-endian CPU bez ```ntohl``` :
```
type = 1677...
pyaload_len = 2684354...
```


## Poznamky k implementacii

- payload sa posiela iba ak payload_len > 0
- client aj server pouzivaju rovnaky protocol.h
- send/recv je obalene v helper funkciach proto_send / proto_recv
- protokol je binarny, nie textovy


### Preco binarny protokol
Predpoklad ze v ```protocol.h``` su spravy medzi serverom a klientom definovane ako :

```
#define MSG_START  1
#define MSG_BYE    2
#define MSG_TICK   3
```

Klient posle :

```
protocol_send(fd, MSG_START, NULL, 0);
```

Po sieti sa posle :

```
[type = 1][payload_len = 0]
```

Tym padom netreba ziadne otravne parsovanie stringov.

Protokol je navrhnuty tak, aby sa dala simulacna logika
jednoducho doplnit bez zmeny architektury.

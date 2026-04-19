# TCP Client-Server Project

Ky projekt është ndërtuar në gjuhën C duke përdorur protokollin TCP dhe përfshin komunikim ndërmjet serverit dhe klientëve, menaxhim të file-ve dhe monitorim përmes një HTTP serveri.

---

## Funksionalitetet kryesore

- Lidhje TCP ndërmjet serverit dhe klientëve
- Menaxhim i shumë klientëve njëkohësisht
- Role të ndryshme: ADMIN dhe USER
- Komanda për menaxhimin e file-ve:
  - /list
  - /read
  - /upload
  - /download
  - /delete
  - /search
  - /info
- HTTP server për monitorim në kohë reale
- Endpoint për statistika:
  - /stats

---

## Struktura e projektit

- server/ – kodi i serverit kryesor TCP
- client/ – kodi i klientit
- http/ – HTTP server për monitorim
- server_storage/ – file-t e ruajtura në server
- shared/ – file për statistikat e serverit
- docs/ – dokumentim dhe screenshot-e

---

## Monitorimi me HTTP

HTTP serveri punon në portin 8081 dhe ofron endpoint-in /stats, i cili lexon të dhënat nga file:

shared/server_stats.txt

Ky file përditësohet nga serveri kryesor dhe përmban:

- numrin e lidhjeve aktive
- IP adresat e klientëve
- numrin e mesazheve
- mesazhet e klientëve

---

## Testimi

Projekti është testuar me sukses:

- Serveri TCP funksionon dhe pranon lidhje
- HTTP serveri funksionon në portin 8081
- Endpoint / dhe /stats kthejnë përgjigje të sakta
- Endpoint-et e panjohura kthejnë 404 Not Found

---

## Anëtarët dhe përgjegjësitë

- Pjesa 1: Serveri kryesor TCP  
- Pjesa 2: Klienti  
- Pjesa 3: Komandat dhe privilegjet për file  
- Pjesa 4: HTTP monitorimi dhe organizimi i projektit  

---

## Gjendja aktuale

- Serveri kryesor është implementuar
- HTTP serveri është implementuar
- Struktura e projektit është organizuar në GitHub
- Monitorimi realizohet përmes shared/server_stats.txt

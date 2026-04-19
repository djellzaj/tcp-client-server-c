# TCP Client-Server Project

Ky projekt është ndërtuar në gjuhën C duke përdorur protokollin TCP.  
Ai përfshin një server kryesor, klientë të shumtë, komandat për menaxhimin e file-ve dhe një HTTP server për monitorim.

## Funksionalitetet kryesore
- Lidhje TCP ndërmjet serverit dhe klientëve
- Menaxhim i shumë klientëve
- Role të ndryshme: ADMIN dhe USER
- Komanda për file:
  - /list
  - /read <filename>
  - /upload <filename>
  - /download <filename>
  - /delete <filename>
  - /search <keyword>
  - /info <filename>
- HTTP server në port të dytë për monitorim
- Endpoint:
  - /stats

## Struktura e projektit
- `server/` - kodi i serverit kryesor TCP
- `client/` - kodi i klientit
- `http/` - kodi i HTTP serverit për monitorim
- `server_storage/` - file-t e ruajtura në server
- `shared/` - file për statistikat e serverit
- `docs/` - dokumentim dhe screenshot-e

## Monitorimi me HTTP
HTTP serveri punon në portin 8080 dhe ofron endpoint-in `/stats`, i cili lexon të dhënat nga file:
`shared/server_stats.txt`

Ky file duhet të përditësohet nga serveri kryesor me:
- numrin e lidhjeve aktive
- IP adresat e klientëve
- numrin e mesazheve
- mesazhet e klientëve

## Anëtarët dhe përgjegjësitë
- Pjesa 1: Serveri kryesor TCP
- Pjesa 2: Klienti
- Pjesa 3: Komandat dhe privilegjet për file
- Pjesa 4: HTTP monitorimi dhe organizimi i projektit

## Gjendja aktuale
- Serveri kryesor është shtuar
- HTTP serveri është shtuar
- Struktura e projektit është organizuar në GitHub
- Integrimi final i statistikave bëhet përmes `shared/server_stats.txt`

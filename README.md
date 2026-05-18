# tidalrpc

Discord Rich Presence für Tidal — zeigt deinen aktuellen Tidal-Song (Titel,
Interpret, Album, Albumcover, Fortschrittsbalken) in deinem Discord-Status an.
Standalone-EXE für Windows, ohne Laufzeit-Abhängigkeiten. Läuft ohne
Konsolenfenster als Icon im System-Tray.

## Wie es funktioniert

- **Track lesen** — über die Windows *System Media Transport Controls*
  (C++/WinRT). Es wird die Sitzung gewählt, deren App-ID `tidal` enthält.
- **Discord** — lokale IPC-Named-Pipe (`discord-ipc-0`), `SET_ACTIVITY` mit
  `type: 2` ("Listening to …").
- **Cover** — Tidal-Such-API → `resources.tidal.com`-Bild-URL. Fällt der
  Tidal-Token aus, greift die iTunes Search API als Fallback.

## Bauen

Voraussetzung: Visual Studio 2022 mit C++-Workload + Windows SDK.

```bat
build.bat
```

Ergebnis: `tidalrpc.exe` im Projektordner.

Alternativ mit CMake:

```bat
cmake -B build -A x64
cmake --build build --config Release
```

## Benutzen

1. **Discord-Desktop-App** starten (nicht im Browser — Browser-Discord hat
   keine IPC-Pipe, Rich Presence funktioniert dort nicht).
2. Tidal starten und etwas abspielen.
3. `tidalrpc.exe` ausführen — es erscheint ein Icon im System-Tray.

- **Rechtsklick** aufs Tray-Icon → aktueller Track, *In Tidal-App öffnen*,
  *Beenden*.
- **Doppelklick** aufs Tray-Icon → aktuellen Track in der Tidal-Desktop-App
  öffnen (`tidal://`-Protokoll).
- **Mauszeiger** über das Icon → Tooltip mit Status.
- Der *Play on Tidal*-Button in Discord öffnet standardmäßig den Browser —
  Discord erlaubt in Buttons nur `http(s)`-Links, kein `tidal://`. Mit der
  optionalen Redirect-Seite (siehe unten) startet der Klick die Desktop-App.

## Button-Klick → Tidal-Desktop-App (optional)

Spotifys „Listen on Spotify“ ist eine fest in Discord eingebaute Integration
und für eigene Apps nicht nachbaubar. Damit der Button-Klick trotzdem die
Desktop-App öffnet, dient die mitgelieferte `redirect.html` als Brücke
(`https://…` → `tidal://track/<id>`):

1. GitHub-Repo anlegen (public), `redirect.html` hochladen.
2. *Settings → Pages* → Branch `main`, Ordner `/root` → Save.
3. Ergebnis-URL notieren, z. B.
   `https://DEINNAME.github.io/tidalrpc/redirect.html`.
4. Diese URL in `src/main.cpp` bei `REDIRECT_BASE` eintragen.
5. Neu bauen (`build.bat`).

Klick auf den Button → Redirect-Seite → Browser fragt „Tidal öffnen?“ →
Desktop-App startet. Ohne installierte App fällt die Seite auf den Webplayer
zurück.
- Discord oder Tidal dürfen jederzeit neu starten — die Verbindung wird
  automatisch wiederhergestellt.
- Es läuft nur eine Instanz gleichzeitig.

### Fehler-Anzeige

Tooltip und Sprechblase nennen den Grund, falls etwas klemmt:

- *„Discord-Desktop-App nicht gefunden“* — Discord läuft nicht als Desktop-App.
- *„Discord lehnt die App-ID ab“* — die `APP_ID` in `src/main.cpp` ist falsch.
- *„Tidal: nichts läuft“* — Tidal spielt gerade nichts ab.

## Konfiguration

Die Discord Application ID steht in `src/main.cpp` (`APP_ID`). Der angezeigte
Name "… listening to <Name>" ist der Name der Discord-App im
[Developer Portal](https://discord.com/developers/applications).

## Grenzen / Hinweise

- Nur Windows 10/11 (SMTC + C++/WinRT).
- Der Tidal-Cover-Token ist öffentlich bekannt und kann von Tidal deaktiviert
  werden; danach liefert iTunes das Cover.
- Cover-Treffer beruhen auf Titel/Interpret-Suche — selten Fehl-Matches möglich.

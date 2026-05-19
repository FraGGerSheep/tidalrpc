9# tidalrpc

Discord Rich Presence for Tidal — displays your current Tidal track (title, artist, album, album art, progress bar) in your Discord status. Standalone EXE for Windows, no runtime dependencies. Runs without a console window as an icon in the system tray.

## How it works

- **Reading the track** — via the Windows *System Media Transport Controls* (C++/WinRT). The session whose app ID contains `tidal` is selected.
- **Discord** — local IPC named pipe (`discord-ipc-0`), `SET_ACTIVITY` with `type: 2` ("Listening to …").
- **Cover art** — Tidal Search API → `resources.tidal.com` image URL. If the Tidal token fails, the iTunes Search API is used as a fallback.

## Building

Requirements: Visual Studio 2022 with C++ workload + Windows SDK.

```bat
build.bat
```

Output: `tidalrpc.exe` in the project folder.

Alternatively with CMake:

```bat
cmake -B build -A x64
cmake --build build --config Release
```

## Usage

1. Start the **Discord desktop app** (not in the browser — browser Discord has no IPC pipe, Rich Presence does not work there).
2. Start Tidal and play something.
3. Run `tidalrpc.exe` — an icon appears in the system tray.

- **Right-click** the tray icon → current track + *Exit*.
- **Hover** over the icon → tooltip with status.
- The *Play on Tidal* button in Discord opens the browser by default — Discord only allows `http(s)` links in buttons, not `tidal://`. The optional redirect page (see below) makes the click launch the desktop app instead.

## Button click → Tidal desktop app (optional)

Spotify's "Listen on Spotify" is a built-in Discord integration and cannot be replicated for third-party apps. To make the button click open the desktop app anyway, the included `redirect.html` serves as a bridge (`https://…` → `tidal://track/<id>`):

1. Create a public GitHub repo and upload `redirect.html`.
2. Go to *Settings → Pages* → branch `main`, folder `/root` → Save.
3. Note the resulting URL, e.g. `https://YOURNAME.github.io/tidalrpc/redirect.html`.
4. Enter this URL in `src/main.cpp` at `REDIRECT_BASE`.
5. Rebuild (`build.bat`).

Click the button → redirect page → browser prompts "Open Tidal?" → desktop app launches. If the app is not installed, the page falls back to the web player.

- Discord and Tidal can be restarted at any time — the connection is restored automatically.
- Only one instance runs at a time.

## Configuration

The Discord application ID is set in `src/main.cpp` (`APP_ID`). The displayed name in "… listening to <Name>" is the name of the Discord app in the [Developer Portal](https://discord.com/developers/applications).


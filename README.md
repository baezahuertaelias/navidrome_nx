# Navidrome NX

A Nintendo Switch homebrew Navidrome client using Borealis and the Subsonic API.
Browse albums alphabetically in pages of 50, open an album, and select a track.
Playback advances through the album. Controls provide previous/next, pause/resume,
and stop; **+** exits. Network requests run outside the UI thread.

The library and persistent Now Playing panel sit side by side and follow the
console's light/dark theme. Album and track rows show separate titles/artists;
tracks also show numbers, durations, and an accent on the playing track.
The player shows elapsed/total time and progress (elapsed time is estimated from
the playback clock; pausing freezes it). Returning to albums preserves the page
and selected album without a network request.

Controller shortcuts: **B** returns to albums, **X** pauses/resumes, **L/R** skip
tracks, **Y** refreshes albums, and **A** selects the focused item. Page buttons
stay above the list. Unavailable player/page controls are disabled.

## Artwork, lyrics, and Favorites

Album rows show cover thumbnails; the player shows the current track's cover.
Images load only when visible, outside the UI thread, with a 64-image RAM cache.
Missing/invalid artwork leaves a blank placeholder and does not stop playback.
Artwork responses are limited to 512 KiB and 1024×1024 decoded pixels.

Choose **Favorites** to view your starred songs, then **Fav albums** to switch to
starred albums. **Fav songs** switches back; **All albums** returns to the full
library. Both favorite lists use pages of 50. **Refresh** fetches current stars
from Navidrome. This app displays favorites; it does not change your stars.

Choose **Lyrics** in the player, or press **ZR**, for the selected track's lyrics.
Use **Up/Down** to scroll and **B** to return. Music continues playing.
The client tries `getLyricsBySongId`, then legacy artist/title lookup. It displays
the first available lyric version as text, including synced sources without timed
highlighting. Lyrics must exist on your server; unavailable lyrics show an empty
state. Responses are limited to 256 KiB and 2000 lines.

## Build with Docker

```sh
bash scripts/build.sh
```

Produces `navidrome_nx.nro` using `devkitpro/devkita64:20260219`.
The image includes libnx, SDL2_mixer, curl, Jansson, and mbedTLS.
Docker must be installed and running. No host Switch SDK installation is needed.
Borealis source and its resources are included under `vendor/borealis` and
`resources`; a normal Git clone includes them, with no submodule setup required.

## Configure

Copy `config.example.json` to **`/switch/navidrome_nx/config.json` on the SD card**
and edit the server URL, username, and password. Include any Navidrome URL prefix,
for example `https://example.com/music`; do not append `/rest`.
Settings are read at startup. If the config is missing, create it and choose
**Refresh**. Restart after changing an existing valid config.

The password is stored as plain text on the SD card. Requests use Subsonic salted
MD5 token authentication; use HTTPS to protect tokens and music in transit.
TLS verification stays enabled. Redirects are rejected; configure the final URL.
The app never writes credentials to logs, downloads to disk, or changes your library.

## Run and test on Switch with nxlink

Use Homebrew Menu with full application memory (title takeover), rather than album
applet mode. Start its network loader with **Y**, then run:

```sh
bash scripts/nxlink.sh 192.168.1.91
```

The script uploads the NRO to `/switch/navidrome_nx/navidrome_nx.nro`, alongside
`config.json`, and listens for console output. It uses Docker host
networking, so the host needs access to the Switch's LAN. Exit with Ctrl+C after
testing. Alternatively, copy the NRO into `/switch/navidrome_nx/` and launch it
from Homebrew Menu. nxlink uploads the executable; it does not install the config.

## Testing status

- Docker cross-compilation and automated API, artwork, and playback-clock checks pass.
- nxlink launch, live library loading, and music playback work on a Switch.
- Switch screenshots confirm readable album/track lists, artwork, and the player.
- Favorites and lyrics endpoints were checked against a live Navidrome server.
- Favorites/lyrics navigation, pause/resume, skip shortcuts, automatic advance,
  and exit during a request still need a complete hardware test.

## Automated API checks

```sh
docker build -t navidrome-nx-test -f tests/Dockerfile .
docker run --rm -v "$PWD:/app:ro" navidrome-nx-test
```

Runs the same C++ API client against a local mock server. Checks token auth,
escaping, album/song parsing, invalid config, API/HTTP errors, malformed replies,
redirect rejection, bounded downloads, and cancellation. Also checks favorites,
cover-art metadata/requests, structured/plain/missing lyrics and lyric size limits,
and image validation/decoding. No real credentials needed.

## Limitations

One complete MP3 is downloaded into RAM before playback (64 MiB maximum).
Navidrome must support MP3 transcoding for non-MP3 sources. There is no progressive
streaming, seeking, search, playlists, offline cache, or scrobbling yet.
Borealis UI sounds are disabled so SDL exclusively owns the audio renderer.

## Repository layout

| Path | Contents |
| --- | --- |
| `src/` | Borealis UI, Subsonic client, artwork validation, playback clock |
| `resources/` | Fonts and UI resources bundled into RomFS |
| `vendor/borealis/` | Pinned Borealis source with local compatibility changes |
| `scripts/` | Docker build and nxlink upload commands |
| `tests/` | Mock-server API tests, image checks, and test container |
| `config.example.json` | Template for the SD-card configuration |

Build artifacts and real `config.json` files are ignored by Git. Keep credentials
in your own config; never replace the example with real account details.

## Upstream

Borealis is vendored from `natinusala/borealis` commit
`20e2d33b6c4ffce139ce304c503c04f5b94da920` under its included Apache-2.0 license
and third-party notices. Local compatibility fixes add `<optional>` and replace
the obsolete keyboard length function with `swkbdConfigSetStringLenMin`.
The app supplies `src/switch_audio.cpp` in place of Borealis' UI sound player.
The vendored Image component adds safe in-memory loading for network artwork.

References: [Borealis](https://github.com/natinusala/borealis),
[Navidrome Subsonic API](https://www.navidrome.org/docs/developers/subsonic-api/),
[getCoverArt](https://opensubsonic.github.io/docs/endpoints/getcoverart/),
[getStarred2](https://opensubsonic.github.io/docs/endpoints/getstarred2/),
[getLyricsBySongId](https://opensubsonic.github.io/docs/endpoints/getlyricsbysongid/).

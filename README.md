# DZSTUDIO

A free object and loot editor that runs inside DayZ on your PC. Everything it makes works on PC **and** console
(Xbox, PlayStation) servers: object spawner files, loot and events. Nothing it offers needs a mod on the server or on
your players' machines.

> **Public beta.** Things may still break between updates, and a project may not open in a later version. Keep a
> copy of builds you care about, and tell us what breaks.

- Place, move and rotate objects on Chernarus, Livonia, Sakhal and Nasdara
- Find anything in the catalog, paint with brushes, group objects into files
- Edit loot: what spawns where, how much, and how long it stays
- Build together: several people edit one project live, each in their own game
- Apply a project to a mission folder: your server's or the map's vanilla mission, changed file by file, with a
  report of what was applied and what the server had different
- Save to local projects, your own [collaboration server](https://github.com/siemendev/dzstudio-collab-server), or
  [dzbot](https://dzbot.de)

**Get it:** [Steam Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3834582618) ·
**Guide:** [dzbot.de/creators/studio](https://dzbot.de/creators/studio) ·
**Talk to us:** [#dzstudio on Discord](https://discord.gg/fxyev4w9yZ)

Inspired by the awesome [DayZ Editor](https://github.com/InclementDab/DayZ-Editor). If you come from it, the keyboard
shortcuts will feel familiar. DZSTUDIO is its own code base, written from scratch.

DZSTUDIO and the DayZ Editor cannot run together: both take over the main menu and the same keys. With both
enabled, DZSTUDIO stays off and says so; disable one of them in the DayZ Launcher.

## Everything is a plugin

The core of DZSTUDIO only knows the scene (projects, groups, objects), operations as the one way to change it (which
gives every change undo, redo and live collaboration for free), events, settings and a UI frame to hang menus, panels,
tools and shortcuts into. Everything else is a plugin on the same public API: the camera, selection, the move and
rotate handles, snapping, the catalog, brushes, the panels, every file format and every place a project can be saved
to.

The plugins that ship with DZSTUDIO use nothing a plugin of yours could not use. Your own mod can add tools, panels,
file formats or storage the same way: depend on `dzstudio` in your `config.cpp`, put
`[DZS_RegisterPlugin(MyPlugin)]` in front of a class derived from `DZS_Plugin`, and it is loaded with the rest.
The shipped plugins under `dzstudio/Scripts/*/dzstudio/plugins/` are the best examples to read. A guide for plugin authors
is on its way.

## Repository

| Path | What |
|---|---|
| `dzstudio/` | The mod as it is packed into `@dzstudio`: `config.cpp`, `Scripts/`, `gui/`, `data/`, `licenses/` |
| `tools/brush/` | The script that builds the shipped brush sets (`dzstudio/data/brush-sets.txt`) from the game's own model list |
| `tools/dzpbo.py` | Packs, signs and verifies the PBO like AddonBuilder and DSSignFile, without DayZ Tools |
| `tools/steam_guard_code.py` | The Steam Guard code for the Workshop upload in CI |
| `workshop/` | The Workshop page: title, description, preview, logo, `mod.cpp`, change notes |
| `keys/` | The public key the Workshop build is signed with |

## Build it yourself

You need DayZ on Windows (Steam) to run it; packing works anywhere with Python 3.

1. Pack the folder `dzstudio/` into `dzstudio.pbo`:
   `python3 tools/dzpbo.py pack dzstudio <out>/@dzstudio/Addons/dzstudio.pbo`
   (AddonBuilder from DayZ Tools works too: `AddonBuilder.exe <repo>\dzstudio <out>\@dzstudio\Addons -packonly -prefix=dzstudio`)
2. Copy `dzstudio/licenses/` next to `Addons/` in `@dzstudio`.
3. Copy `@dzstudio` into your DayZ folder and start `DayZ_x64.exe "-mod=@dzstudio" -nosplash -skipIntro -noPause`
   from there, with Steam running. DZSTUDIO opens instead of the main menu.

The Workshop build is signed with our own key; a build of your own is not, which does not matter for the editor since
it never joins a server.

## Releases

The `build` workflow packs every push and pull request. On `main`, a version in `dzstudio/config.cpp` that has no
`v<version>` tag yet is signed, uploaded to the Steam Workshop and tagged. The Workshop page comes from `workshop/`;
the change note from `workshop/notes/<version>.txt`, or the commit subjects since the last tag when there is none.

## Contributing

Issues and pull requests are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md). Contributions are accepted under the
Developer Certificate of Origin (sign off your commits with `git commit -s`).

## License

DZSTUDIO is licensed under the [Mozilla Public License 2.0](LICENSE): changes to the files of this repository stay
open, while plugins and mods that build on DZSTUDIO may use any license.

Bundled third-party files keep their own licenses, which ship in `dzstudio/licenses/`:
the fonts Barlow, Barlow Condensed, Bebas Neue and JetBrains Mono (SIL Open Font License 1.1) and the
[Lucide](https://lucide.dev) icons (ISC). DayZ and its data are © Bohemia Interactive; DZSTUDIO reads them from your
own installation and ships none of them.

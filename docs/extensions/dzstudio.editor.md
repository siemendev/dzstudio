# dzStudio editor state, version 1

Extension URL: `https://developers.dzbot.de/specs/dzstudio/editor/1.json` (spec file served there).

What only dzStudio needs to show a project the way it was left: its name, the base it is applied to, its groups as
the project bar shows them, the loot editor's switches, plugin state and the camera. dzStudio writes it on every save;
other programs keep it as it is. Not `required`: without it, dzStudio opens the project with defaults (each spawner
block becomes a group named after the block, loot off, the camera at the map's default).

```json
"extensions": {
  "https://developers.dzbot.de/specs/dzstudio/editor/1.json": {
    "data": {
      "provider": { "name": "KotH Arena", "map": "chernarusplus" },
      "base": "vanilla",
      "project": {
        "groups": [
          { "block": "g-arena", "name": "Arena", "color": "#60A5FA", "hidden": false, "locked": false, "objectCount": 12,
            "objects": [ { "index": 3, "name": "Land_Mil_Tower_Small", "pos": [4520.5, 312.25, 10210.0], "hidden": true, "locked": false } ],
            "plugins": {} }
        ],
        "loot": { "enabled": false, "hidden": false, "locked": false, "projectId": null, "writtenTypes": [] },
        "plugins": {}
      },
      "view": { "camera": { "pos": [4500.0, 340.0, 10180.0], "heading": 45.0, "pitch": -20.0 } }
    }
  }
}
```

## `data`

- `provider` — `name`: the project's name as dzStudio shows it (the `slug` at the top level follows it); `map`: the map
  key.
- `base` — the base dzStudio applies the project to: `"vanilla"` (Bohemia's mission of the map) or
  `"mission:<folder>"`, a mission folder in `Documents\DayZ\dzstudio\Missions` by its name
  (`"mission:dayzOffline.chernarusplus"`). Missing or empty means `"vanilla"`.
- `project` — the project bar's state:
  - `groups` — one entry per group, in the bar's order: `block` (the id of its `spawnerGroup` block), `name`, `color`
    (`#RRGGBB`), `hidden`, `locked`, `objectCount`; `objects` lists only objects that are hidden or locked (`index` in
    the block's `objects`, `name`, `pos`); `plugins` holds per-group data of plugins by plugin name.
  - `loot` — the loot editor: `enabled`, `hidden`, `locked`; `projectId` and `writtenTypes` are kept for files that
    came from older versions.
  - `plugins` — per-project data of plugins by plugin name.
- `view` — `camera`: `pos` `[x, y, z]`, `heading` and `pitch` in degrees.

dzStudio writes the whole entry anew on every save; members not listed here do not survive it. The extension lives
only in the project's `extensions`; dzStudio puts nothing of it on blocks, changes or elements.

## Reading

A version 2 file written before this extension existed holds the same object at the top level as `"editor"`;
dzStudio reads it from there and writes it into the extension on the next save. A later version of this extension
would have another URL; this dzStudio keeps such an entry untouched and writes its own next to it.

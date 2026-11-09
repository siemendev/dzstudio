# dzbot rules, version 1

Extension URL: `https://developers.dzbot.de/specs/dzbot/rules/1.json` (spec file served there).

The rules [dzbot](https://dzbot.de) applies to a project mod on each server it is installed on: whether a block,
a change or a single element of a block takes part, values computed from the mod's settings, repetition. They live in
the project file itself — a dzbot project mod is that one file, `dzstudio/project.json` — and each rule sits next to
what it belongs to: in the `extensions` of that block, that change or that list element, under this extension's URL.
The project's own entry holds the mod's settings. dzStudio edits rules and settings when the open project is a dzbot
project mod (Project › dzbot rules …), dzbot's web workbench and AI tools edit them there too; other programs keep the
entries as they are. Not `required`: without the rules the
project means "everything, with the values as saved".

```json
"blocks": [
  {
    "id": "g-teleporter", "kind": "spawnerGroup",
    "data": {
      "objects": [
        { "name": "Land_Teleporter", "pos": [0, 0, 0] },
        { "name": "Sign_Back", "pos": [2, 0, 0],
          "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": {
            "if": { "field": "reverse", "op": "eq", "value": true } } } }
      ]
    },
    "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": {
      "fields": { "objects[0].pos": { "setting": "source" } } } }
  }
],
"changes": [
  { "op": "set", "block": "event", "key": { "name": "VehicleCivilianSedan" }, "field": "nominal", "from": 8, "to": 12,
    "extensions": { "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "formula": "round(vanilla * boost)", "required": true } } }
]
```

The entry under the URL is the rule itself (an object). The project's own `extensions` entry of this URL holds the
mod's settings (below): `{"data": {"settings": [ … ]}}`.

## A rule

- On a **block**:
  - `if` — a condition (below); the block and its child blocks take part only while it holds.
  - `required` — `true`: a block that goes into an existing entry (its `parent` is a key) must find exactly one
    target, or the whole project is not applied on that server.
  - `repeat` — `{"count": "<formula>"}` or `{"over": "<list setting>"}`: the block (with its child blocks) is repeated.
    Copies are named `<id>-1`, `<id>-2` …; formulas in them read `index` (0, 1, …) and, over a list, `item` and
    `item.<property>`.
  - `fields` — per path in the block's `data`, `{"setting": "<name>"}` (the setting's value as it is; a coordinate is a
    whole position) or `{"formula": "<formula>"}` (a number; `base` is the value in the project). Settings are applied
    before formulas.
- On a **list element** of a block's data (an object of a group, an item of a preset — any object inside a list):
  `if`, `repeat` and `fields`, with paths in `fields` starting at the element (`pos`, `pos[0]`).
- On a **change**: `if`, `formula` (only for `op: "set"`; names: the settings, `base` = the value found on the server,
  `vanilla` = Bohemia's value) and `required`.

Members not listed here are kept by every program that rewrites the entry. Patchers ignore `extensions` everywhere —
of the project, of blocks and changes and of elements inside block data — and never write them into a mission file.

**Paths** name a value inside a block's data: keys joined with dots, list elements by their index from 0 —
`nominal`, `objects[2].pos`, `children[0].max`, `objects[0].pos[1]`. They count as in the project, also when an
element before them is left out.

**Conditions** are JSON trees: `{"field": "<setting>", "op": "<op>", "value": …}` with `op` one of `eq`, `neq`, `in`,
`not_in` (value a list), `empty`, `not_empty` (no value), `gt`, `gte`, `lt`, `lte`, `contains`; groups
`{"all": [ … ]}`, `{"any": [ … ]}` and `{"not": { … }}`. Values compare loosely (`"1"`, `true` and `1` are the same
for a yes/no setting).

**Formulas**: numbers, names (also with parts: `source.x`), `+ - * /`, parentheses and `min`, `max`, `round`,
`floor`, `ceil`, `abs`, `clamp`; at most 500 characters.

## Settings

The settings are what a server admin fills in when installing the mod — the names conditions, formulas, `fields` and
`repeat` read. They live in the project file, in the project's own entry, in the order of dzbot's install form:

```json
"extensions": {
  "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "data": { "settings": [
    { "identifier": "weapons", "title": "Weapons", "type": "section" },
    { "identifier": "boost", "title": "Car boost", "type": "number", "required": true, "default": 1.5,
      "help": "1 = vanilla", "typeConfig": { "min": 0, "max": 5 } },
    { "identifier": "mode", "title": "Mode", "type": "generic_select", "default": "easy",
      "typeConfig": { "options": [ { "value": "easy", "label": "Easy" }, { "value": "hard", "label": "Hard" } ] },
      "visibleIf": { "field": "boost", "op": "gt", "value": 1 } }
  ] } }
}
```

| Member | Meaning |
| --- | --- |
| `identifier` | The name rules read: letters, digits and `_`, not starting with a digit, unique among the settings. A `section` may repeat one. |
| `title` | What server admins see (up to 255 characters). Absent: the identifier is shown. |
| `type` | A dzbot config field type — `number`, `boolean`, `string`, `textarea`, `coordinate`, `datetime`, `email`, `generic_select`, `object_list`, `item_class` … — or `section`, a heading over the settings below it. |
| `help` | Optional explanation under the field (up to 255 characters). |
| `required` | Optional, default `false`: an installation needs a value. Never on a `section`. |
| `default` | Optional: the value a new installation starts with — a number, `true`/`false` or a position `[x, y, z]` for those types, text for every other. |
| `visibleIf` | Optional: when the field (or a `section` with everything in it) is shown — a condition as above, on settings above it. |
| `typeConfig` | Optional: the type's configuration — `generic_select`: `options` (`[{"value", "label"}]`) and `multiple`; `object_list`: `properties` (`[{"name", "type", "label"}]`); `number`: `min`, `max` … |

Readers also take a simpler shape: `id` for a missing `identifier`, the type `select` for `generic_select`, and
`options` (objects or plain values), `multiple` and `props` (`[{"name"}]`) beside the entry for the same members of
`typeConfig`. dzbot writes the shape of the table; members it does not know stay in the entry. A project without
`settings` leaves the mod's settings as they are in dzbot.

## Names in reports

dzbot's reports and problems name a change `<block> <key> <field>`, trimmed: its `block` kind, its `key` as compact JSON
and its `field` (empty for a `remove`) — `event {"name":"VehicleCivilianSedan"} nominal`. The rules themselves need no
names: they sit on what they belong to.

## In dzStudio

Project › dzbot rules … lists the project's own blocks and changes (the selected object's group first) with a
summary of their rules; a group's context menu in the project bar opens the same list with that group first, and
groups with rules are marked "dzbot rules". A rule opens in its own dialog:

- **Takes part when** — comparisons of a setting (picked from the mod's settings), an operator and a value, joined by
  "all apply" or "at least one applies". Nested groups and `not` are shown read-only.
- **Formula for the value** (changes that set a value), **Target required** (changes, and blocks inside an existing
  entry).
- **Repeat**, **Values from settings and formulas** (paths picked from the block's data; the selected objects'
  positions first) and **Single elements** (each with its own condition, repetition and values).

Every edit — a rule, a setting added, changed or removed — is a step of the history and saved with the project.
Renaming a setting renames it in every rule that names it in the same step, so undo puts back the settings and the rules
together. The dialogs hold on to their block, change, list element (by its element, not its place) or setting (by its
identifier); one whose target is deleted meanwhile — by undo or by a partner in a live session — closes with "This
element was deleted" and writes nothing.

**Copying.** The spec sets `"copy": "same-project"`: an object duplicated or pasted in the same project keeps its rules;
pasted into another project it starts without them, because rules name the settings of their own project. Block-level
`fields` paths into a group's data (`objects[2].pos`) count places in the list; deleting an object before them shifts
what they mean — put a value on the object's own rule (`pos`) to tie it to that object.

## In dzbot

The project file is where a dzbot project mod keeps its settings. Every save of the file — from dzStudio, the
workbench, an import or dzbot's AI tools — makes the mod's config fields match the list: new identifiers become new
fields, changed ones are updated, missing ones removed, and the order follows the list. A setting replaced in its place
by one new identifier of the same type counts as renamed, so installations keep their value. A list dzbot cannot take
(two settings with one identifier, an identifier rules cannot read, a condition on a setting further down) refuses the
whole save with the reason. The workbench's config panel writes into the same list (and only into it); renaming a
setting there can rename it in every rule too. New project mods start with an empty list; dzStudio is sent the
settings with the project as well.

dzbot reads the rules when it applies the project at a transfer and in the preview; a later version would be another
URL, which this dzbot does not read. dzbot also knows dzStudio's editor extension (`https://developers.dzbot.de/specs/dzstudio/editor/1.json`) and ignores it; any other
extension marked `required` keeps the project off every server ("Project not applied: it needs an extension dzbot does not support"), in
the server's mod log and in the preview (only the project's own entries can be required; an entity's entries cannot).
The workbench rewrites only the blocks and changes it edits and keeps every other byte of the file; renaming a mod
setting rewrites its name in the rules the same way, each rule in its own layout. Export downloads the
project file as it is, rules included; import replaces it with another project file.

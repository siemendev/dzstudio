# dzbot rules, version 1

Extension URL: `https://developers.dzbot.de/specs/dzbot/rules/1.json` (spec file served there).

The rules [dzbot](https://dzbot.de) applies to a project mod on each server it is installed on: whether a block,
a change or a single element of a block takes part, values computed from the mod's settings, repetition. They live in
the project file itself — a dzbot project mod is that one file, `dzstudio/project.json` — and each rule sits next to
what it belongs to: in the `extensions` of that block, that change or that list element, under this extension's URL.
dzStudio edits them when the open project is a dzbot project mod (Project › dzbot rules …), dzbot's web workbench and
AI tools edit them there too; other programs keep the entries as they are. Not `required`: without the rules the
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

The entry under the URL is the rule itself (an object). The project's own `extensions` entry of this URL holds nothing
for now (`{"data": {}}` or absent).

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

Edits count as an unsaved change of the project and are saved with it.

## In dzbot

dzbot reads the rules when it applies the project at a transfer and in the preview; a later version would be another
URL, which this dzbot does not read. dzbot also knows dzStudio's editor extension (`https://developers.dzbot.de/specs/dzstudio/editor/1.json`) and ignores it; any other
extension marked `required` keeps the project off every server ("Project not applied: it needs an extension dzbot does not support"), in
the server's mod log and in the preview (only the project's own entries can be required; an entity's entries cannot).
The workbench rewrites only the blocks and changes it edits and keeps every other byte of the file; renaming a mod
setting rewrites its name in the rules the same way, each rule in its own layout. Export downloads the
project file as it is, rules included; import replaces it with another project file.

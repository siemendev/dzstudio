# dzbot rules, version 1

Extension URL: `https://developers.dzbot.de/specs/dzbot/rules/1.json` (spec file served there).

The rules [dzbot](https://dzbot.de) applies to a project mod on each server it is installed on: whether a block or
change takes part, values computed from the mod's settings, repetition. They live in the project file itself — a
dzbot project mod is that one file, `dzstudio/project.json`. dzStudio edits them when the open project is a dzbot
project mod (Project › dzbot rules …), dzbot's web workbench and AI tools edit them there too; other programs keep the
entry as it is. Not `required`: without the rules the project means "everything, with the values as saved".

```json
"extensions": {
  "https://developers.dzbot.de/specs/dzbot/rules/1.json": {
    "data": {
      "blocks": {
        "g-teleporter": {
          "fields": { "objects[0].pos": { "setting": "source" } },
          "items": { "objects[1]": { "if": { "field": "reverse", "op": "eq", "value": true } } }
        }
      },
      "changes": {
        "event {\"event\":\"VehicleCivilianSedan\"} nominal": { "formula": "round(vanilla * boost)", "required": true }
      }
    }
  }
}
```

## `data`

- `blocks` — per own block, by its `id`, a rule:
  - `if` — a condition (below); the block and its child blocks take part only while it holds.
  - `required` — `true`: a block that goes into an existing entry (its `parent` is a key) must find exactly one
    target, or the whole project is not applied on that server.
  - `repeat` — `{"count": "<formula>"}` or `{"over": "<list setting>"}`: the block (with its child blocks) is repeated.
    Copies are named `<id>-1`, `<id>-2` …; formulas in them read `index` (0, 1, …) and, over a list, `item` and
    `item.<property>`.
  - `fields` — per path in the block's `data`, `{"setting": "<name>"}` (the setting's value as it is; a coordinate is a
    whole position) or `{"formula": "<formula>"}` (a number; `base` is the value in the project). Settings are applied
    before formulas.
  - `items` — per list element of the block's `data` (`objects[1]`), a rule with `if`, `repeat` and `fields`; paths in
    its `fields` start at the element (`pos`, `pos[0]`).
- `changes` — per change, by its ref (below), a rule with `if`, `formula` (only for `op: "set"`; names: the settings,
  `base` = the value found on the server, `vanilla` = Bohemia's value) and `required`.

Members not listed here are kept by every program that rewrites the entry.

**Paths** name a value inside a block's data: keys joined with dots, list elements by their index from 0 —
`nominal`, `objects[2].pos`, `children[0].max`, `objects[0].pos[1]`. They count as in the project, also when an
element before them is left out.

**Conditions** are JSON trees: `{"field": "<setting>", "op": "<op>", "value": …}` with `op` one of `eq`, `neq`, `in`,
`not_in` (value a list), `empty`, `not_empty` (no value), `gt`, `gte`, `lt`, `lte`, `contains`; groups
`{"all": [ … ]}`, `{"any": [ … ]}` and `{"not": { … }}`. Values compare loosely (`"1"`, `true` and `1` are the same
for a yes/no setting).

**Formulas**: numbers, names (also with parts: `source.x`), `+ - * /`, parentheses and `min`, `max`, `round`,
`floor`, `ceil`, `abs`, `clamp`; at most 500 characters.

## Change refs

A change is addressed by `<block> <key> <field>`, trimmed: its `block` kind, its `key` as compact JSON and its
`field` (empty for a `remove`). The key is written as PHP's `json_encode` writes it after `json_decode`, with slashes
and non-ASCII characters unescaped and whole numbers without a decimal point: `5.0` → `5`, `1.50` → `1.5`,
`1e2` → `100`, `0.00001` → `1.0e-5`. Keys keep their order. Example:
`spawnPosition {"event":"VehicleCivilianSedan","pos":[1234.5,0,5678]} pos`.

A rule whose block or change the project no longer has is kept and reported as a problem; dzStudio lists it under
"Rules without a target".

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

dzbot reads `data` when it applies the project at a transfer and in the preview; a later version would be another URL,
which this dzbot does not read. dzbot also knows dzStudio's editor extension (`https://developers.dzbot.de/specs/dzstudio/editor/1.json`) and ignores it; any other
extension marked `required` keeps the project off every server ("Project not applied: it needs an extension dzbot does not support"), in
the server's mod log and in the preview. The workbench writes only the entry's `data` and keeps every other byte of the
file; renaming a mod setting rewrites its name in the conditions and formulas the same way. Export downloads the
project file as it is, rules included; import replaces it with another project file.

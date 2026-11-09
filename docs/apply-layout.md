# Projects applied to a mission folder

dzStudio applies projects ([`project-format.md`](project-format.md)) straight into a mission folder below
`Documents\DayZ\dzstudio\Missions` ("Apply to mission folder …"). The folder keeps what it needs to take a project out
again, the same way dzbot keeps a server's files when it transfers mods. This is the layout.

```
<mission folder>\
  cfggameplay.json
  cfggameplay.json.original          the file as it was before the first project changed it
  dzstudio\my-arena\g-arena.json     a file a project created
  dzstudio\my-arena\g-arena.json.original   zero bytes: the file did not exist
  .dzstudio\
    applied.json                     the projects applied here, in order
    projects\my-arena.dzstudio.json  the project file as it was applied
    projects\my-arena.report.txt     the report of its last run
    vanilla-copy.txt                 only in a folder made from the vanilla mission
```

## `.original` beside the files

- Before a project changes a file for the first time, the file is copied to `<file>.original` beside it. A file a
  project creates gets a `<file>.original` of zero bytes.
- An existing `.original` is never overwritten. A folder that already has them (copied from a server dzbot manages)
  keeps them: they count as the originals.
- Applying, undoing and removing rebuild the folder: every touched file goes back to its `.original` (a zero-byte one
  deletes the file and the folders that became empty), then every listed project is applied again in its order, each on
  the result of the ones before. Applying the same project twice changes nothing twice.
- When the folder is the base of a project, the patcher and the editor read `<file>.original` instead of `<file>` (a
  zero-byte one hides the file): the folder's own state, without the projects applied to it.
- A `.original` dzStudio wrote goes away when no project touches its file any more; one that was there before stays.

## `.dzstudio\applied.json`

```json
{
  "format": "dzstudio-applied",
  "version": 1,
  "projects": [
    { "slug": "my-arena", "name": "My arena", "applied": "2026-11-09 18:42:07", "file": "projects/my-arena.dzstudio.json" }
  ],
  "touched": ["cfggameplay.json", "dzstudio/my-arena/g-arena.json"],
  "originals": ["cfggameplay.json", "dzstudio/my-arena/g-arena.json"]
}
```

| Member | Meaning |
| --- | --- |
| `projects` | The applied projects in the order they are applied: `slug` (the project's `slug`), `name`, `applied` (when it was last applied, local time) and `file` (its copy, relative to `.dzstudio`). |
| `touched` | The files (relative to the mission folder, `/` as separator) the last rebuild wrote. |
| `originals` | The files whose `.original` dzStudio wrote — and may delete again. |

Members dzStudio does not know are not kept: the file is rewritten whole on every rebuild.

## `.dzstudio\projects\`

- `<slug>.dzstudio.json` — the project file as it was applied, so the folder can be rebuilt (and stays complete when
  it is moved or uploaded) without the project list it came from.
- `<slug>.report.txt` — the last run's report in words: counts, what needs a look, the files (the statuses of
  [`project-format.md`](project-format.md#report)).

Undo or Remove takes the project out of `applied.json` and deletes both files. When the last project goes, `.dzstudio`
goes with it.

## Vanilla as the base

With Bohemia's vanilla mission as the base, the result goes into a new folder `<project> (Vanilla <map>)` that holds
only the projects' files (every one with a zero-byte `.original`) and `.dzstudio\vanilla-copy.txt`. Such a folder is
not offered as a base of its own; when its last project is taken out, the folder is deleted once nothing else is in it.

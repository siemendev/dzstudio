# Project file extensions

A dzStudio project file (`format: "dzstudio-project"`, `version: 2`, spec: [`../project-format.md`](../project-format.md))
holds building blocks and changes. Data that belongs to another application, to a plugin or to dzStudio's own view of
the project lives under `extensions`. An extension is the URL of its spec file; the project file references it:

```json
{
  "format": "dzstudio-project", "version": 2, "map": "chernarusplus", "slug": "my-project",
  "blocks": [], "changes": [],
  "extensions": {
    "https://developers.dzbot.de/specs/dzstudio/editor/1.json": { "data": { "provider": { "name": "My project", "map": "chernarusplus" } } },
    "https://developers.dzbot.de/specs/dzbot/rules/1.json": { "data": { "blocks": {}, "changes": {} } }
  }
}
```

## Rules for every extension

- The key is the URL of the extension's spec file. The version is part of the URL: an incompatible new version gets a
  new URL. Programs match extensions by URL.
- The entry is `{"data": …}` with `data` in the shape its spec defines, optionally `"required": true`.
- `"required": true` means: a program that does not know this URL must not apply the project. It sits in the project
  file, so a program that cannot fetch the spec still knows. Leave it out when the project still means the same
  without the extension.
- Whoever rewrites the file keeps entries it does not know as they are, and keeps the members of a known entry it does
  not use.
- The patcher (applying a project to a mission folder) ignores `extensions`.

## Spec files

Each extension publishes a JSON spec file at its URL with at least `$id` (that URL), `title` (`en`, `de`),
`description` and `spec` — a link to its human-readable spec, a markdown file like the ones in this folder. dzbot's and
dzStudio's own are served from `https://developers.dzbot.de/specs/…`:

| URL | Spec |
| --- | --- |
| `https://developers.dzbot.de/specs/dzstudio/editor/1.json` | [`dzstudio.editor.md`](dzstudio.editor.md) |
| `https://developers.dzbot.de/specs/dzbot/rules/1.json` | [`dzbot.rules.md`](dzbot.rules.md) |

Planned: a `fields` list in the spec file that describes the data, so dzStudio can show and edit an extension it has no
plugin for — each field marked as editable in the studio or only by tools and plugins.

## How dzStudio handles them

Opening a project hands every entry to the extension registered for its URL. Saving writes back what each extension
holds, in the order the file had, followed by new entries. Entries no registered extension reads are written back
unchanged. dzStudio never fetches a spec file.

The editor extension is built in: the project file itself reads and writes it (`DZS_ProjectFile.EDITOR_EXT`,
`EditorOf`, `SetEditor`), no plugin registers it.

A `required` entry dzStudio cannot read: the project still opens, with a warning in the project's open notes, and the
entry stays in the file as it is. Applying it to a mission folder is refused (Project › Apply to mission folder is
disabled with the reason, and so is `-dzsPatch` from the command line); `DZS_ProjectExtensions.Blocked` names it for
the open project, `UnreadableIn` for any project file.

## Adding an extension from a plugin

Derive from `DZS_ProjectExtension` and register an instance once, for example in your plugin's `OnRegister`:

```c
class MyNotes : DZS_ProjectExtension
{
	protected ref DZS_Json m_Data;

	override string Url() { return "https://example.com/specs/notes/1.json"; }
	override bool IsActive() { return true; }
	override void Load(DZS_Json data) { m_Data = data; }
	override void Reset() { m_Data = null; }
	override DZS_Json Data() { return m_Data; }
}

// in OnRegister:
DZS_ProjectExtensions.Add(new MyNotes());
```

| Member | What it does |
| --- | --- |
| `Url()` | The URL of the extension's spec file — its identity, version included. To read two versions, register two extensions. |
| `Required()` | Writes `"required": true`. |
| `IsActive()` | Whether its user interface applies to the open project. Inactive extensions still load and save. |
| `Load(data)` | The entry's `data` of the opened project (a copy). Not called when the project has no entry. |
| `Reset()` | The project was closed; forget its data. |
| `Data()` | What to save; `null` leaves the entry out. |
| `GroupMark(inst, groupId)` | A short text shown with a group in the project bar (`""` = none). |
| `GroupMenu(inst, groupId, items)` / `GroupMenuChosen(inst, groupId, tag)` | Entries for a group's context menu. |

After changing its data outside the undo history, an extension calls `DZS_ProjectExtensions.Changed(instance)`: the
project counts one unsaved change until it is saved. Menu entries, dialogs and panels are registered by the plugin
itself, as any plugin does (`DZS_UiRegistry.RegisterAction`, `AddMenuEntry`).

Specs: [`dzstudio.editor`](dzstudio.editor.md), [`dzbot.rules`](dzbot.rules.md).

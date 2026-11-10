// The hidden working folder of a project saved in version 2: the project file holds only building blocks and the
// editor part, so opening builds below $profile:dzstudio\Work what the open path reads — one spawner file per
// spawner group, the loot files of the project's base with the project's loot applied by the patcher, and a project
// file of version 1 composed from the editor part. Saving never writes here, only the project file.
class DZS_WorkResult : Managed
{
	string m_Dir;
	ref array<string> m_Manifest;
	string m_Base;
	// the project's loot entries and rows, with their text in the base
	ref DZS_LootOrigin m_Origin = new DZS_LootOrigin();
	string m_Error;
}

class DZS_WorkFolder
{
	static const string ROOT = "$profile:dzstudio\\Work";

	// One folder per project id (a project's backup folder gets its own).
	static string DirOf(string folder)
	{
		string safe = "";
		string bad = "\\/:*?\"<>|. $";
		for (int i = 0; i < folder.Length(); i++)
		{
			string c = folder.Get(i);
			if (bad.Contains(c))
				safe += "_";
			else
				safe += c;
		}
		return ROOT + "\\" + safe;
	}

	static string SpawnerPath(string slug, string blockId)
	{
		return "dzstudio/" + slug + "/" + blockId + ".json";
	}

	// The mission folder a base names, "" when it is not on this computer (the vanilla mission not fetched yet, a
	// mission folder that is gone).
	static string BaseDir(string base, string mapKey)
	{
		if (base.IndexOf("mission:") == 0)
		{
			string dir = DZS_PatchApply.MISSIONS + "\\" + base.Substring(8, base.Length() - 8);
			if (DZS_Files.IsFolder(dir))
				return dir;
			return "";
		}
		if (mapKey != "" && DZS_LootVanilla.HaveMission(mapKey))
			return DZS_LootVanilla.MissionDir(mapKey);
		return "";
	}

	// Whether opening needs the base's files: loot is on in the editor part (a file without one: it has loot blocks or
	// changes).
	static bool NeedsBase(DZS_Json root)
	{
		DZS_Json proj = ProjectOf(root);
		if (!proj)
			return HasLoot(root);
		DZS_Json loot = proj.Get("loot");
		return loot && loot.IsObj() && loot.GetBool("enabled");
	}

	static DZS_Json ProjectOf(DZS_Json root)
	{
		DZS_Json ed = DZS_ProjectFile.EditorOf(root);
		if (!ed)
			return null;
		DZS_Json proj = ed.Get("project");
		if (!proj || !proj.IsObj())
			return null;
		return proj;
	}

	static bool HasLoot(DZS_Json root)
	{
		array<string> lists = {"blocks", "changes"};
		foreach (string ln : lists)
		{
			DZS_Json list = root.Get(ln);
			if (!list || !list.IsArr())
				continue;
			for (int i = 0; i < list.Count(); i++)
			{
				DZS_Json one = list.At(i);
				if (!one.IsObj())
					continue;
				string kind = one.GetStr("kind");
				if (ln == "changes")
					kind = one.GetStr("block");
				if (kind.IndexOf("loot") == 0)
					return true;
			}
		}
		return false;
	}

	static DZS_Json RootOf(array<string> chunks)
	{
		if (!chunks)
			return null;
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc.m_Root || !doc.m_Root.IsObj())
			return null;
		return doc.m_Root.ToJson();
	}

	static string MapOf(DZS_Json root)
	{
		string mk = root.GetStr("map");
		DZS_Json ed = DZS_ProjectFile.EditorOf(root);
		if (mk == "" && ed && ed.Get("provider"))
			mk = ed.Get("provider").GetStr("map");
		return DZS_Maps.FromFormat(mk);
	}

	static string BaseOf(DZS_Json root)
	{
		DZS_Json ed = DZS_ProjectFile.EditorOf(root);
		string b = "";
		if (ed)
			b = ed.GetStr("base");
		if (b == "")
			b = "vanilla";
		return b;
	}

	// The base the open path uses: the named one, the vanilla mission when a named mission folder is gone (noted in
	// 'issues'); "" when neither is here.
	static string UsableBase(string base, string mapKey, DZS_Issues issues)
	{
		string dir = BaseDir(base, mapKey);
		if (dir != "" || base == "vanilla")
			return dir;
		if (issues)
		{
			string shown = base.Substring(8, base.Length() - 8);
			issues.Add(DZS_Issue.WARNING, "Basis " + shown + " fehlt in dzstudio\\Missions — Vanilla gilt", "Base " + shown + " is missing in dzstudio\\Missions — vanilla applies");
		}
		return BaseDir("vanilla", mapKey);
	}

	// Copies the loot files of a base into the working folder (cfglimitsdefinition.xml from the fetched vanilla state
	// when the base has none; the vanilla mission also as vanilla\). A file projects were applied to counts as its
	// .original. False when mapgroupproto.xml or mapgrouppos.xml is missing there.
	static bool CopyLoot(string baseDir, string mapKey, string work)
	{
		DZS_PatchFiles look = new DZS_PatchFiles(baseDir);
		look.m_Over.Copy(DZS_ApplyStore.Originals(baseDir));
		array<string> names = {DZS_LootFiles.PROTO, DZS_LootFiles.POS, DZS_LootFiles.LIMITS};
		bool ok = true;
		foreach (string rel : names)
		{
			string real = look.Resolve(rel);
			string dst = work + "\\" + rel;
			string alt;
			if (look.m_Over.Find(rel, alt))
			{
				real = "";
				if (alt != "" && DZS_Files.Copy(alt, dst))
					continue;
			}
			if (real != "")
			{
				string src = baseDir + "\\" + real;
				src.Replace("/", "\\");
				if (DZS_Files.Copy(src, dst))
					continue;
			}
			if (rel == DZS_LootFiles.LIMITS)
			{
				string fetched = DZS_LootVanilla.PathOf(mapKey, rel);
				if (FileExist(fetched))
					DZS_Files.Copy(fetched, dst);
				continue;
			}
			ok = false;
		}
		// on the vanilla mission the base is the vanilla state the loot editor compares with
		if (ok && baseDir == BaseDir("vanilla", mapKey))
		{
			DZS_Files.MakeDirs(work + "\\vanilla");
			DZS_Files.Copy(work + "\\" + DZS_LootFiles.PROTO, work + "\\vanilla\\" + DZS_LootFiles.PROTO);
			DZS_Files.Copy(work + "\\" + DZS_LootFiles.LIMITS, work + "\\vanilla\\" + DZS_LootFiles.LIMITS);
		}
		return ok;
	}

	// Builds the working folder of the project file in 'folder' at once (DZS_WorkBuild over several frames). m_Error set
	// when the file is no readable version 2.
	static DZS_WorkResult Build(string folder, array<string> chunks, string loadedMap, DZS_Issues issues)
	{
		DZS_WorkBuild j = new DZS_WorkBuild(folder, chunks, loadedMap, issues);
		j.m_Wait = false;
		DZS_Jobs.RunNow(j);
		return j.m_Result;
	}

	// The spawner file of a group as the editor reads and writes it: the block's objects as they stand (the spawner
	// format's own entries, enableCEPersistency 0 or 1), not the server's form the patcher writes.
	static DZS_Json GroupRoot(DZS_PatchBlock b)
	{
		DZS_Json root = DZS_Json.Obj();
		DZS_Json objs = b.m_Data.Get("objects");
		if (!objs || !objs.IsArr())
			objs = DZS_Json.Arr();
		root.Set("Objects", objs);
		return root;
	}

	// The project file of version 1 for the open path: the editor part with every group's block as its file
	// ('paths': block id -> relative file; a block without a path keeps "<id>.json").
	static array<string> Manifest(DZS_Json root, string mapKey, map<string, string> paths)
	{
		string name = "";
		string viewText = "";
		DZS_Json ed = DZS_ProjectFile.EditorOf(root);
		if (ed)
		{
			DZS_Json prov = ed.Get("provider");
			if (prov && prov.IsObj())
				name = prov.GetStr("name");
			DZS_Json view = ed.Get("view");
			if (view && view.IsObj())
				viewText = TextOf(view, 1);
		}
		DZS_Json proj = ProjectOf(root);
		if (!proj)
		{
			proj = DZS_Json.Obj();
			DZS_Json loot = DZS_Json.Obj();
			DZS_Json on = DZS_Json.Boolean(HasLoot(root));
			loot.Set("enabled", on);
			proj.Set("loot", loot);
		}
		DZS_Json part = PartV1(proj, paths);
		Unlisted(root, part, paths);
		DZS_KeptBlocks.Put(root, part);
		string projText = TextOf(part, 1);
		return DZS_ProjectFile.Compose(name, mapKey, projText, viewText);
	}

	// Spawner blocks the editor part names no group for (a project file written elsewhere): a group entry each, named
	// after the block.
	protected static void Unlisted(DZS_Json root, DZS_Json part, map<string, string> paths)
	{
		DZS_Json groups = part.Get("groups");
		if (!groups || !groups.IsArr())
		{
			groups = DZS_Json.Arr();
			part.Set("groups", groups);
		}
		array<string> files = new array<string>();
		array<string> colors = new array<string>();
		for (int g = 0; g < groups.Count(); g++)
		{
			DZS_Json ge = groups.At(g);
			if (!ge.IsObj())
				continue;
			files.Insert(ge.GetStr("file"));
			colors.Insert(ge.GetStr("color"));
		}
		DZS_Json blocks = root.Get("blocks");
		if (!blocks || !blocks.IsArr())
			return;
		for (int i = 0; i < blocks.Count(); i++)
		{
			DZS_Json b = blocks.At(i);
			if (!b.IsObj() || b.GetStr("kind") != "spawnerGroup")
				continue;
			string rel;
			if (!paths.Find(b.GetStr("id"), rel) || files.Find(rel) >= 0)
				continue;
			DZS_Json data = b.Get("data");
			string gname = b.GetStr("id");
			int count = 0;
			if (data && data.IsObj())
			{
				if (data.GetStr("name") != "")
					gname = data.GetStr("name");
				DZS_Json objs = data.Get("objects");
				if (objs && objs.IsArr())
					count = objs.Count();
			}
			string color = DZS_Palette.Next(colors);
			colors.Insert(color);
			files.Insert(rel);
			array<ref DZS_PFObject> none = new array<ref DZS_PFObject>();
			string text = DZS_ProjectFile.GroupEntry(rel, gname, color, false, false, count, none, "[]");
			string err;
			DZS_Json entry = DZS_Json.ParseText(text, err);
			if (entry)
				groups.Add(entry);
		}
	}

	// The project part with "block" replaced by "file" and the loot part in the form of version 1.
	static DZS_Json PartV1(DZS_Json proj, map<string, string> paths)
	{
		DZS_Json part = proj.Clone();
		DZS_Json groups = part.Get("groups");
		if (groups && groups.IsArr())
		{
			for (int i = 0; i < groups.Count(); i++)
			{
				DZS_Json g = groups.At(i);
				if (!g.IsObj() || !g.Has("block"))
					continue;
				string id = g.GetStr("block");
				string rel;
				if (!paths.Find(id, rel))
					rel = id + ".json";
				DZS_Json fresh = DZS_Json.Obj();
				for (int k = 0; k < g.m_Keys.Count(); k++)
				{
					string key = g.m_Keys[k];
					if (key == "block")
					{
						DZS_Json fj = DZS_Json.Str(rel);
						fresh.Set("file", fj);
						continue;
					}
					fresh.Set(key, g.Get(key));
				}
				groups.m_Items[i] = fresh;
			}
		}
		DZS_Json loot = part.Get("loot");
		if (!loot || !loot.IsObj())
		{
			loot = DZS_Json.Obj();
			DZS_Json off = DZS_Json.Boolean(false);
			loot.Set("enabled", off);
			part.Set("loot", loot);
		}
		if (!loot.Has("hidden"))
		{
			DZS_Json h = DZS_Json.Boolean(false);
			loot.Set("hidden", h);
		}
		if (!loot.Has("locked"))
		{
			DZS_Json l = DZS_Json.Boolean(false);
			loot.Set("locked", l);
		}
		DZS_Json none = DZS_Json.Null();
		loot.Set("projectId", none);
		DZS_Json empty = DZS_Json.Arr();
		loot.Set("writtenTypes", empty);
		return part;
	}

	static string TextOf(DZS_Json v, int depth)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		DZS_TextForm.Value(w, v, depth);
		array<string> chunks = w.Finish();
		return DZS_Files.Join(chunks);
	}

	// The loot blocks and changes of a project (the types they name into 'types', own rows into the origin).
	static DZS_PatchProject LootPart(DZS_PatchProject pp, array<string> types, DZS_LootOrigin origin)
	{
		DZS_PatchProject lp = new DZS_PatchProject();
		lp.m_Map = pp.m_Map;
		lp.m_Slug = pp.m_Slug;
		foreach (DZS_PatchBlock b : pp.m_Blocks)
		{
			if (b.m_Kind.IndexOf("loot") != 0)
				continue;
			lp.m_Blocks.Insert(b);
			if (b.m_Kind == "lootType")
				AddType(types, b.m_Data.GetStr("name"));
			else if (b.m_Kind == "lootRow")
				AddRow(origin, b.m_Data);
			else if (b.m_Parent)
				AddType(types, b.m_Parent.GetStr("group"));
		}
		foreach (DZS_PatchChange c : pp.m_Changes)
		{
			if (c.m_Block.IndexOf("loot") != 0)
				continue;
			lp.m_Changes.Insert(c);
			if (c.m_Block == "lootType")
				AddType(types, c.m_Key.GetStr("name"));
			else if (c.m_Block != "lootRow")
				AddType(types, c.m_Key.GetStr("group"));
		}
		return lp;
	}

	// The patched loot files into the working folder; changes that do not fit the base are noted in 'issues'.
	static void WriteLoot(DZS_Patcher p, string dir, DZS_Issues issues)
	{
		foreach (string rel : p.m_OutPaths)
		{
			string low = DZS_PatchText.Lower(rel);
			if (low != DZS_LootFiles.PROTO && low != DZS_LootFiles.POS)
				continue;
			array<string> data = p.m_Out.Get(rel);
			DZS_Files.Write(dir + "\\" + low, data);
		}
		int bad = 0;
		foreach (DZS_PatchEntry e : p.m_Report.m_Entries)
		{
			if (e.m_Status == "invalid" || e.m_Status == "ambiguous" || e.m_Status == "collision" || e.m_Status == "orphaned")
				bad++;
		}
		if (bad > 0)
		{
			string de = bad.ToString() + DZS_Issues.Plural(bad, " Loot-Änderung passt", " Loot-Änderungen passen") + " nicht zur Basis und gilt nicht";
			string en = bad.ToString() + DZS_Issues.Plural(bad, " loot change does", " loot changes do") + " not fit the base and do not apply";
			issues.Add(DZS_Issue.WARNING, de, en);
		}
	}

	// The text of the project's own loot types in the base's mapgroupproto.xml, read from the base.
	static void OriginsFromBase(string baseDir, map<string, string> originals, string dir, array<string> types, DZS_LootOrigin origin)
	{
		DZS_PatchFiles look = new DZS_PatchFiles(baseDir);
		string protoPath = baseDir + "\\" + look.Resolve(DZS_LootFiles.PROTO);
		string protoAlt;
		if (originals && originals.Find(DZS_LootFiles.PROTO, protoAlt) && protoAlt != "")
			protoPath = protoAlt;
		DZS_LootFiles basis = DZS_LootFiles.ReadBase(protoPath, dir + "\\" + DZS_LootFiles.LIMITS);
		foreach (string t : types)
		{
			string orig = "";
			if (basis.m_Error == "")
				orig = basis.SourceOf(t);
			origin.m_OwnTypes.Insert(t);
			origin.m_Origs.Insert(orig);
		}
	}

	protected static void AddType(array<string> types, string name)
	{
		if (name == "")
			return;
		string low = DZS_PatchText.Lower(name);
		foreach (string t : types)
		{
			if (DZS_PatchText.Lower(t) == low)
				return;
		}
		types.Insert(name);
	}

	// A row as the patcher writes it: the name, the position with six decimals.
	protected static void AddRow(DZS_LootOrigin o, DZS_Json data)
	{
		DZS_PField f = new DZS_PField("@pos", DZS_PField.VEC3, true);
		string err;
		string pos = DZS_PatcherBase.Render(f, data.Get("pos"), err);
		if (err != "")
			return;
		o.m_OwnRows.Set(data.GetStr("name") + "\n" + pos, true);
	}
}

// DZS_WorkFolder.Build over several frames, for opening: the project file read in steps, one spawner group file at a
// time, the patcher run on the base's loot files in steps (DZS_Patcher.Step). Before anything is written it finds out
// whether the project needs its base and the base is not here (m_NeedBase: the caller fetches it and starts again).
class DZS_WorkBuild : DZS_Job
{
	static const int PARSE = 0;
	static const int ROOT = 1;
	static const int GROUPS = 2;
	static const int LOOT = 3;
	static const int PATCH = 4;
	static const int ORIGIN = 5;
	static const int DONE = 6;

	string m_Folder;
	string m_LoadedMap;
	ref DZS_Issues m_Issues;
	ref DZS_WorkResult m_Result = new DZS_WorkResult();
	// the project file as read (also for the caller: extensions, the base it waits for)
	ref DZS_Json m_Root;
	bool m_NeedBase;
	// stop for a missing base (m_NeedBase) instead of building without its loot
	bool m_Wait = true;
	protected ref array<string> m_Chunks;
	protected ref DZS_SrcJsonSteps m_Parse;
	protected ref DZS_PatchProject m_Project;
	protected int m_Phase;
	protected int m_At;
	protected string m_Slug;
	protected string m_MapKey;
	protected string m_Dir;
	protected int m_GroupFiles;
	protected ref map<string, string> m_Paths = new map<string, string>();
	protected string m_BaseDir;
	protected ref map<string, string> m_Originals;
	protected ref array<string> m_Types = new array<string>();
	protected ref DZS_Patcher m_Run;
	protected ref DZS_JsonWriteSteps m_GroupText;
	protected int m_T0;

	void DZS_WorkBuild(string folder, array<string> chunks, string loadedMap, DZS_Issues issues)
	{
		m_De = "Projekt wird geöffnet";
		m_En = "Opening the project";
		m_Folder = folder;
		m_Chunks = chunks;
		m_LoadedMap = loadedMap;
		m_Issues = issues;
		m_T0 = TickCount(0);
		if (!m_Chunks)
			m_Chunks = new array<string>();
		m_Parse = new DZS_SrcJsonSteps(m_Chunks, 5);
	}

	// Opening may stop here: only the hidden working folder is half made, the next opening makes it again.
	override bool CanCancelNow()
	{
		return true;
	}

	override bool Step(float ms)
	{
		int t0 = TickCount(0);
		while (m_Phase != DONE)
		{
			if (DZS_Job.Over(t0, ms))
				break;
			if (m_Phase == PARSE)
			{
				SetStep("Projektdatei", "project file");
				float left = ms - (TickCount(0) - t0) / 10000.0;
				if (!m_Parse.Step(Math.Max(left, 0.5)))
				{
					m_Share = 0.3 * m_Parse.Progress();
					break;
				}
				m_Phase = ROOT;
			}
			else if (m_Phase == ROOT)
				TakeRoot();
			else if (m_Phase == GROUPS)
			{
				float gleft = ms - (TickCount(0) - t0) / 10000.0;
				if (!OneGroup(Math.Max(gleft, 0.5)))
					break;
			}
			else if (m_Phase == LOOT)
				PrepareLoot();
			else if (m_Phase == PATCH)
			{
				SetStep("Loot der Basis", "the base's loot");
				float rest = ms - (TickCount(0) - t0) / 10000.0;
				if (!m_Run.Step(Math.Max(rest, 0.5)))
				{
					m_Share = 0.5 + 0.45 * m_Run.Share();
					break;
				}
				WriteLoot();
				m_Phase = ORIGIN;
			}
			else if (m_Phase == ORIGIN)
			{
				Origins();
				m_Phase = DONE;
			}
		}
		if (m_Phase != DONE)
			return false;
		m_Share = 1;
		if (m_Result.m_Error == "" && !m_NeedBase)
		{
			DZS_WorkResult res = m_Result;
			DZS_Log.Info("work folder " + res.m_Dir + ": " + m_GroupFiles + " group file(s), base " + res.m_Base + ", " + res.m_Origin.m_OwnTypes.Count() + " own loot type(s), " + res.m_Origin.m_OwnRows.Count() + " own row(s) in " + ((TickCount(0) - m_T0) / 10000) + " ms");
		}
		return true;
	}

	protected void TakeRoot()
	{
		m_Phase = DONE;
		DZS_SrcDoc doc = m_Parse.Result();
		m_Parse = null;
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
		{
			m_Result.m_Error = "unreadable";
			return;
		}
		m_Root = doc.m_Root.ToJson();
		string err;
		m_Project = DZS_PatchProject.FromJson(m_Root, err);
		if (!m_Project)
		{
			m_Result.m_Error = err;
			return;
		}
		m_MapKey = DZS_WorkFolder.MapOf(m_Root);
		if (DZS_Maps.WorldOf(m_MapKey) == "")
			m_MapKey = m_LoadedMap;
		m_Result.m_Base = DZS_WorkFolder.BaseOf(m_Root);
		if (m_Wait && DZS_WorkFolder.NeedsBase(m_Root) && DZS_WorkFolder.UsableBase(m_Result.m_Base, m_MapKey, null) == "")
		{
			m_NeedBase = true;
			return;
		}
		m_Slug = m_Project.m_Slug;
		if (m_Slug == "")
			m_Slug = "project";
		m_Dir = DZS_WorkFolder.DirOf(m_Folder);
		if (DZS_Files.IsFolder(m_Dir))
			DZS_Files.DeleteTree(m_Dir);
		DZS_Files.MakeDirs(m_Dir);
		m_Result.m_Dir = m_Dir;
		m_At = 0;
		m_Share = 0.3;
		m_Phase = GROUPS;
	}

	// One group's file, its text over several slices (a group of thousands of objects takes hundreds of milliseconds);
	// false while the text is not complete.
	protected bool OneGroup(float ms)
	{
		SetStep("Gruppen", "groups");
		if (!m_GroupText)
		{
			if (m_At >= m_Project.m_Blocks.Count())
			{
				m_Result.m_Manifest = DZS_WorkFolder.Manifest(m_Root, m_MapKey, m_Paths);
				m_Phase = LOOT;
				return true;
			}
			DZS_PatchBlock next = m_Project.m_Blocks[m_At];
			if (next.m_Kind != "spawnerGroup")
			{
				m_At++;
				return true;
			}
			m_GroupText = new DZS_JsonWriteSteps(DZS_WorkFolder.GroupRoot(next), true, 2);
		}
		if (!m_GroupText.Step(ms))
			return false;
		array<string> text = m_GroupText.Result();
		m_GroupText = null;
		DZS_PatchBlock b = m_Project.m_Blocks[m_At];
		m_At++;
		m_Share = 0.3 + 0.2 * m_At / Math.Max(1, m_Project.m_Blocks.Count());
		string rel = DZS_WorkFolder.SpawnerPath(m_Slug, b.m_Id);
		string full = m_Dir + "\\" + rel;
		full.Replace("/", "\\");
		DZS_Files.MakeDirs(full.Substring(0, full.LastIndexOf("\\")));
		DZS_Files.Write(full, text);
		m_Paths.Set(b.m_Id, rel);
		m_GroupFiles++;
		return true;
	}

	// The base's loot files into the working folder and the patcher run on them for the project's loot.
	protected void PrepareLoot()
	{
		m_Phase = DONE;
		m_Share = 0.5;
		m_BaseDir = DZS_WorkFolder.UsableBase(m_Result.m_Base, m_MapKey, m_Issues);
		if (m_BaseDir == "")
		{
			if (DZS_WorkFolder.NeedsBase(m_Root))
				m_Issues.Add(DZS_Issue.WARNING, "Basis des Projekts fehlt — Loot lässt sich erst nach dem Laden der Vanilla-Mission bearbeiten", "The project's base is missing — loot can be edited only after the vanilla mission is fetched");
			return;
		}
		if (!DZS_WorkFolder.CopyLoot(m_BaseDir, m_MapKey, m_Dir))
		{
			if (DZS_WorkFolder.NeedsBase(m_Root))
				m_Issues.Add(DZS_Issue.WARNING, "Die Basis hat keine Loot-Dateien (mapgroupproto.xml, mapgrouppos.xml)", "The base has no loot files (mapgroupproto.xml, mapgrouppos.xml)");
			return;
		}
		DZS_PatchProject lp = DZS_WorkFolder.LootPart(m_Project, m_Types, m_Result.m_Origin);
		if (lp.m_Blocks.Count() + lp.m_Changes.Count() == 0)
			return;
		m_Originals = DZS_ApplyStore.Originals(m_BaseDir);
		m_Run = DZS_Patcher.Begin(lp, m_BaseDir, m_Originals);
		m_Phase = PATCH;
	}

	protected void WriteLoot()
	{
		SetStep("Loot-Dateien", "loot files");
		DZS_WorkFolder.WriteLoot(m_Run, m_Dir, m_Issues);
	}

	// The project's own loot types with their text in the base: from the base's mapgroupproto.xml as the patcher read
	// it (read again only when the run did not read it).
	protected void Origins()
	{
		if (!m_Run || m_Types.Count() == 0)
			return;
		DZS_PatchFile pf = m_Run.m_Files.m_Files.Get(DZS_LootFiles.PROTO);
		if (pf && pf.Ok() && pf.m_Xml && pf.m_Xml.m_Root && pf.m_Xml.m_Root.m_Name == "prototype")
		{
			map<string, int> first = new map<string, int>();
			DZS_XmlNode root = pf.m_Xml.m_Root;
			for (int i = 0; i < root.m_Children.Count(); i++)
			{
				DZS_XmlNode g = root.m_Children[i];
				if (g.m_Name != "group")
					continue;
				string key = g.Get("name");
				if (key == "")
					continue;
				key.ToLower();
				if (!first.Contains(key))
					first.Set(key, i);
			}
			foreach (string t : m_Types)
			{
				string low = t;
				low.ToLower();
				string orig = "";
				int at;
				if (first.Find(low, at))
					orig = pf.m_Xml.Raw(root.m_Children[at]);
				m_Result.m_Origin.m_OwnTypes.Insert(t);
				m_Result.m_Origin.m_Origs.Insert(orig);
			}
			return;
		}
		DZS_WorkFolder.OriginsFromBase(m_BaseDir, m_Originals, m_Dir, m_Types, m_Result.m_Origin);
	}
}

// The open project as a building-block project (format version 2): every spawner group an own spawner block, every
// loot edit changes against the loot type as it was before the project, every object with loot an own loot row.
// The result applies to any mission folder of the map.
class DZS_ProjectV2
{
	// The project's slug: lower-case letters, digits and dashes from its name.
	static string Slug(string name)
	{
		string low = DZS_PatchText.Lower(name);
		string ok = "abcdefghijklmnopqrstuvwxyz0123456789";
		string r = "";
		bool dash = false;
		for (int i = 0; i < low.Length(); i++)
		{
			string c = low.Get(i);
			if (ok.Contains(c))
			{
				if (dash && r != "")
					r += "-";
				r += c;
				dash = false;
			}
			else
				dash = true;
		}
		if (r == "")
			r = "project";
		return r;
	}

	// A block id from a text: letters, digits, '-' and '_', the rest as '-'.
	static string IdPart(string t)
	{
		string ok = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
		string r = "";
		for (int i = 0; i < t.Length(); i++)
		{
			string c = t.Get(i);
			if (ok.Contains(c))
				r += c;
			else
				r += "-";
		}
		return r;
	}

	// The block id of a spawner group from its file's stem: a stem that already is a block id stays as it is, so a group
	// read from the working folder keeps its id at the next save.
	static string GroupId(string stem)
	{
		string low = DZS_PatchText.Lower(stem);
		if (low.IndexOf("g-") == 0)
			return IdPart(low);
		return "g-" + IdPart(low);
	}

	static string StemOf(string file)
	{
		string stem = file;
		int slash = stem.LastIndexOf("/");
		if (slash >= 0)
			stem = stem.Substring(slash + 1, stem.Length() - slash - 1);
		int dot = stem.LastIndexOf(".");
		if (dot > 0)
			stem = stem.Substring(0, dot);
		return stem;
	}

	// Block id per spawner group (group element id -> block id), unique; groups whose stem is a block id already
	// (read from a working folder) take theirs first.
	static map<string, string> GroupIds(DZS_Instance inst)
	{
		map<string, string> r = new map<string, string>();
		array<string> used = new array<string>();
		array<string> groups = inst.GetGroupIds();
		for (int pass = 0; pass < 2; pass++)
		{
			foreach (string gid : groups)
			{
				if (inst.GetKind(gid) != DZS_KIND_GROUP || r.Contains(gid))
					continue;
				string format, file, gname;
				inst.GetField(gid, DZS_Obj.GROUP_FORMAT, format);
				inst.GetField(gid, DZS_Obj.GROUP_FILE, file);
				inst.GetField(gid, DZS_Obj.GROUP_NAME, gname);
				if (format != "" && format != "spawner")
					continue;
				string stem = StemOf(file);
				if (stem == "")
					stem = gname;
				bool own = DZS_PatchText.Lower(stem).IndexOf("g-") == 0;
				if ((pass == 0) != own)
					continue;
				string id = GroupId(stem);
				string unique = id;
				int n = 2;
				while (used.Find(unique) >= 0)
				{
					unique = id + "-" + n;
					n++;
				}
				used.Insert(unique);
				r.Set(gid, unique);
			}
		}
		return r;
	}

	// format, version, map, slug and empty block and change lists.
	static DZS_Json Empty(string mapKey, string name)
	{
		DZS_Json root = DZS_Json.Obj();
		DZS_Json fmt = DZS_Json.Str(DZS_PatchProject.FORMAT);
		root.Set("format", fmt);
		DZS_Json ver = DZS_Json.Int(DZS_PatchProject.VERSION);
		root.Set("version", ver);
		DZS_Json mp = DZS_Json.Str(DZS_Maps.FormatKey(mapKey));
		root.Set("map", mp);
		DZS_Json slug = DZS_Json.Str(Slug(name));
		root.Set("slug", slug);
		DZS_Json blocks = DZS_Json.Arr();
		root.Set("blocks", blocks);
		DZS_Json changes = DZS_Json.Arr();
		root.Set("changes", changes);
		return root;
	}

	// noLoot: the loot of the project is left out (a copy saved with loot off). Every entity carries the extensions its
	// element in the model holds; where they live is recorded for the editors (DZS_EntityExts.Index). At once; a big
	// project over several frames with Start or a DZS_ProjectBuild the caller steps.
	static DZS_Json Build(DZS_Kernel kernel, DZS_Instance inst, string name, string mapKey, bool noLoot = false)
	{
		DZS_ProjectBuild j = DZS_ProjectBuild.Make(kernel, inst, name, mapKey, noLoot, false);
		DZS_Jobs.RunNow(j);
		return j.m_Root;
	}

	// Build over the next frames with the shared progress plate; the state is read as it is now (later changes do not
	// count). The job's m_OnDone says when m_Root is there (null when the project was closed meanwhile).
	static DZS_ProjectBuild Start(DZS_Kernel kernel, DZS_Instance inst, string name, string mapKey, bool noLoot = false)
	{
		DZS_ProjectBuild j = DZS_ProjectBuild.Make(kernel, inst, name, mapKey, noLoot, true);
		DZS_Jobs.Start(j);
		return j;
	}

	// One spawner object as its block element: the spawner format's own entry, its extensions recorded.
	static DZS_Json ObjectEntry(DZS_SpawnerFormat sf, DZS_Instance inst, string oid, DZS_Issues issues, float worldSize, string blockId, int index)
	{
		DZS_WriteResult res = new DZS_WriteResult();
		string text = sf.Entry(inst, oid, issues, worldSize, res);
		string err;
		DZS_Json entry = DZS_Json.ParseText(text, err);
		if (!entry || err != "")
			return null;
		DZS_Json obj = DZS_Json.Obj();
		CopyMember(entry, obj, "name");
		CopyMember(entry, obj, "pos");
		CopyMember(entry, obj, "ypr");
		CopyMember(entry, obj, "scale");
		CopyMember(entry, obj, "enableCEPersistency");
		CopyMember(entry, obj, "customString");
		DZS_EntityExts.AttachElement(obj, inst, blockId, "objects", index, oid, DZS_EntityExts.SLOT);
		return obj;
	}

	// The block of a spawner group from its objects.
	static DZS_Json GroupBlock(DZS_Instance inst, string gid, string blockId, DZS_Json objects)
	{
		string gname;
		inst.GetField(gid, DZS_Obj.GROUP_NAME, gname);
		DZS_Json data = DZS_Json.Obj();
		DZS_Json gn = DZS_Json.Str(gname);
		data.Set("name", gn);
		data.Set("objects", objects);
		DZS_Json b = Block(blockId, "spawnerGroup", data);
		DZS_EntityExts.Attach(b, inst, gid, DZS_EntityExts.SLOT);
		return b;
	}

	// A layer's part ({blocks, changes} in project.plugins: Spawns …) into the project's lists.
	static void AddFragment(DZS_Json frag, DZS_Json blocks, DZS_Json changes)
	{
		if (!frag || !frag.IsObj())
			return;
		DZS_Json fb = frag.Get("blocks");
		if (fb && fb.IsArr())
		{
			for (int i = 0; i < fb.Count(); i++)
			{
				DZS_Json one = fb.At(i);
				blocks.Add(one);
			}
		}
		DZS_Json fc = frag.Get("changes");
		if (fc && fc.IsArr())
		{
			for (int k = 0; k < fc.Count(); k++)
			{
				DZS_Json two = fc.At(k);
				changes.Add(two);
			}
		}
	}

	protected static void CopyMember(DZS_Json from, DZS_Json to, string key)
	{
		DZS_Json v = from.Get(key);
		if (v)
			to.Set(key, v);
	}

	static DZS_Json Block(string id, string kind, DZS_Json data)
	{
		DZS_Json b = DZS_Json.Obj();
		DZS_Json ij = DZS_Json.Str(id);
		b.Set("id", ij);
		DZS_Json kj = DZS_Json.Str(kind);
		b.Set("kind", kj);
		DZS_Json none = DZS_Json.Null();
		b.Set("parent", none);
		b.Set("data", data);
		return b;
	}

	// ---- loot ----

	// The changes and own blocks of one edited loot type, their extensions bound to the type's loot element.
	static void LootType(DZS_LootPlugin lp, DZS_Instance inst, string type, DZS_Json blocks, DZS_Json changes)
	{
		string start;
		DZS_LootEEntry from = null;
		if (lp.StartText(type, start))
			from = DZS_LootPlugin.FromSource(start);
		DZS_LootEEntry to = lp.Effective(inst, type);
		if (DZS_LootBlocks.Canon(from) == DZS_LootBlocks.Canon(to))
			return;
		DZS_LootEEntry src = lp.OpenEntry(type);
		int c0 = changes.Count();
		int b0 = blocks.Count();
		DZS_LootBlocks.s_Paths = new map<string, string>();
		DZS_LootBlocks.Diff(from, to, src, "l-" + IdPart(DZS_PatchText.Lower(type)), changes, blocks);
		// the type's changes keep their extensions on its loot element by ref, own blocks on their part of the entry
		string elem = lp.ElementOf(inst, type);
		for (int ci = c0; ci < changes.Count(); ci++)
		{
			DZS_Json ch = changes.At(ci);
			DZS_ExtOwner co = DZS_ExtOwner.Make(elem, DZS_EntityExts.SLOT, DZS_ProjectRefs.ChangeRef(ch));
			DZS_EntityExts.Bind(ch, inst, co, true);
		}
		for (int bi = b0; bi < blocks.Count(); bi++)
		{
			DZS_Json ob = blocks.At(bi);
			string path;
			if (!DZS_LootBlocks.s_Paths.Find(ob.GetStr("id"), path))
				continue;
			DZS_ExtOwner bo = DZS_ExtOwner.Make(elem, "data", "", path);
			DZS_EntityExts.Bind(ob, inst, bo, false);
		}
		DZS_LootBlocks.s_Paths = null;
	}

	// The loot row of an object with loot as an own block, null for an object without.
	static DZS_Json LootRow(DZS_LootPlugin lp, DZS_Instance inst, string oid, int number)
	{
		if (lp.SettingOf(inst, oid) == DZS_LootPlugin.NONE)
			return null;
		string oname, pos, ypr;
		inst.GetField(oid, DZS_Obj.NAME, oname);
		inst.GetField(oid, DZS_Obj.POS, pos);
		inst.GetField(oid, DZS_Obj.YPR, ypr);
		vector p = DZS_Obj.QuickVec(pos);
		vector a = DZS_Obj.QuickVec(ypr);
		string rowText = DZS_LootFiles.RowText(oname, p, a, "");
		array<string> chunks = DZS_Files.Chunk(rowText);
		DZS_XmlDoc doc = DZS_Xml.Parse(chunks);
		if (!doc.m_Root)
			return null;
		DZS_Json data = DZS_Json.Obj();
		DZS_Json nj = DZS_Json.Str(doc.m_Root.Get("name"));
		data.Set("name", nj);
		DZS_Json pj = DZS_Json.Str(doc.m_Root.Get("pos"));
		data.Set("pos", pj);
		DZS_Json rj = DZS_Json.Str(doc.m_Root.Get("rpy"));
		data.Set("rpy", rj);
		DZS_Json aj = DZS_Json.Num(doc.m_Root.Get("a"));
		data.Set("a", aj);
		DZS_Json b = Block("row-" + number, "lootRow", data);
		DZS_EntityExts.Attach(b, inst, oid, DZS_EntityExts.SLOT + ":lootRow");
		return b;
	}

	// ---- applying ----

	// Patches 'baseDir' with the project and writes the changed files and report.json into 'outDir' (never in place).
	static DZS_Patcher Apply(DZS_Json project, string baseDir, string outDir, out string err)
	{
		DZS_PatchProject pp = DZS_PatchProject.FromJson(project, err);
		if (!pp)
			return null;
		DZS_Patcher p = DZS_Patcher.Run(pp, baseDir);
		DZS_Files.MakeDirs(outDir);
		foreach (string rel : p.m_OutPaths)
		{
			string full = outDir + "\\" + rel;
			full.Replace("/", "\\");
			int slash = full.LastIndexOf("\\");
			DZS_Files.MakeDirs(full.Substring(0, slash));
			array<string> data = p.m_Out.Get(rel);
			DZS_Files.Write(full, data);
		}
		array<string> report = p.m_Report.Write();
		DZS_Files.Write(outDir + "\\report.json", report);
		return p;
	}
}

// DZS_ProjectV2.Build over several frames: one spawner object, edited loot type, loot row or layer part at a time, in
// the order of the whole build at once (same result). With its own freeze it reads the state as it was at its start;
// without (the save plan) the caller holds the view of its freeze while it steps.
class DZS_ProjectBuild : DZS_Job
{
	static const int GROUPS = 0;
	static const int TYPES = 1;
	static const int ROWS = 2;
	static const int SECTIONS = 3;
	static const int DONE = 4;

	DZS_Kernel m_Kernel;
	DZS_Instance m_Inst;
	string m_Name;
	string m_MapKey;
	bool m_NoLoot;
	// the file's slug stays (set by the save of a provider's project before the build steps)
	bool m_KeepSlug;
	// the state's generation at the start
	int m_Gen;
	// the result; null when the project went away meanwhile
	ref DZS_Json m_Root;
	protected ref DZS_Json m_Blocks;
	protected ref DZS_Json m_Changes;
	protected ref DZS_ExtIndex m_Rec;
	protected DZS_StateFreeze m_Freeze;
	protected int m_Phase;
	protected int m_At;
	protected int m_Total;
	protected int m_Count;
	protected DZS_SpawnerFormat m_Sf;
	protected float m_WorldSize;
	protected ref map<string, string> m_BlockIds;
	protected ref array<string> m_Groups = new array<string>();
	protected ref array<string> m_Ids;
	protected int m_Oi;
	protected ref DZS_Json m_Objects;
	protected string m_Unique;
	protected ref DZS_Issues m_Issues;
	protected DZS_LootPlugin m_Lp;
	protected ref array<string> m_Types = new array<string>();
	protected ref array<string> m_LootObjs = new array<string>();
	protected int m_Rows;
	protected ref array<ref DZS_ProjectSection> m_Sections;
	// what the open carried over unchanged (DZS_KeptBlocks), appended last
	protected ref DZS_Json m_Kept;

	static DZS_ProjectBuild Make(DZS_Kernel kernel, DZS_Instance inst, string name, string mapKey, bool noLoot, bool ownFreeze)
	{
		DZS_ProjectBuild j = new DZS_ProjectBuild();
		j.m_De = "Projekt wird zusammengestellt";
		j.m_En = "Putting the project together";
		j.m_Kernel = kernel;
		j.m_Inst = inst;
		j.m_Name = name;
		j.m_MapKey = mapKey;
		j.m_NoLoot = noLoot;
		j.m_Root = DZS_ProjectV2.Empty(mapKey, name);
		j.m_Blocks = j.m_Root.Get("blocks");
		j.m_Changes = j.m_Root.Get("changes");
		j.m_Rec = DZS_EntityExts.NewRecord(inst);
		if (inst)
			j.m_Gen = inst.GetGen();
		if (ownFreeze && inst)
		{
			j.m_Freeze = inst.BeginFreeze();
			inst.SetView(j.m_Freeze);
		}
		j.Prepare();
		if (j.m_Freeze)
			inst.SetView(null);
		return j;
	}

	// The generation of the state the build reads, when that is not the live one (the save plan's freeze began earlier):
	// the index it leaves is valid for that state only.
	void StampGen(int gen)
	{
		m_Gen = gen;
		if (m_Rec)
			m_Rec.m_Gen = gen;
	}

	// What the build goes over, taken at its start.
	protected void Prepare()
	{
		if (!m_Inst)
			return;
		m_Sf = DZS_SpawnerFormat.Cast(m_Kernel.GetFormat("spawner"));
		if (GetGame().GetWorld())
			m_WorldSize = GetGame().GetWorld().GetWorldSize();
		m_BlockIds = DZS_ProjectV2.GroupIds(m_Inst);
		if (m_Sf)
			m_Groups.Copy(m_Inst.GetGroupIds());
		foreach (string gid : m_Groups)
			m_Total += m_Inst.GetChildren(gid).Count();
		if (!m_NoLoot)
		{
			m_Lp = DZS_LootPlugin.Cast(m_Kernel.GetPlugin("loot"));
			if (m_Lp && m_Lp.IsOn())
			{
				m_Types.Copy(m_Lp.EditedTypes(m_Inst));
				m_LootObjs.Copy(m_Lp.ClassObjects(m_Inst));
			}
			else
				m_Lp = null;
		}
		m_Total += m_Types.Count() + m_LootObjs.Count();
		m_Sections = DZS_ProjectSections.All();
		m_Total += m_Sections.Count();
		m_Kept = DZS_KeptBlocks.From(m_Inst);
	}

	override bool Step(float ms)
	{
		if (m_Phase == DONE)
			return true;
		if (!m_Inst)
		{
			DZS_Log.Warn("project build: the project went away");
			m_Root = null;
			m_Phase = DONE;
			return true;
		}
		int t0 = TickCount(0);
		DZS_ExtIndex prev = DZS_EntityExts.SwapRecord(m_Rec);
		if (m_Freeze)
			m_Inst.SetView(m_Freeze);
		bool done = Work(t0, ms);
		if (m_Freeze)
			m_Inst.SetView(null);
		DZS_EntityExts.SwapRecord(prev);
		if (m_Total > 0)
			m_Share = m_Count / (m_Total * 1.0);
		if (!done)
			return false;
		m_Phase = DONE;
		m_Share = 1;
		DZS_EntityExts.KeepRecord(m_Rec, m_Root, !m_NoLoot);
		EndFreeze();
		return true;
	}

	override void OnCancel()
	{
		EndFreeze();
		m_Root = null;
	}

	protected void EndFreeze()
	{
		if (m_Freeze && m_Inst)
			m_Inst.EndFreeze(m_Freeze);
		m_Freeze = null;
	}

	protected bool Work(int t0, float ms)
	{
		while (m_Phase != DONE)
		{
			if (DZS_Job.Over(t0, ms))
				return false;
			if (m_Phase == GROUPS)
			{
				if (!StepGroups(t0, ms))
					return false;
				m_Phase = TYPES;
				m_At = 0;
			}
			else if (m_Phase == TYPES)
			{
				if (m_At >= m_Types.Count())
				{
					m_Phase = ROWS;
					m_At = 0;
					continue;
				}
				DZS_ProjectV2.LootType(m_Lp, m_Inst, m_Types[m_At], m_Blocks, m_Changes);
				m_At++;
				m_Count++;
			}
			else if (m_Phase == ROWS)
			{
				if (m_At >= m_LootObjs.Count())
				{
					m_Phase = SECTIONS;
					m_At = 0;
					continue;
				}
				DZS_Json row = DZS_ProjectV2.LootRow(m_Lp, m_Inst, m_LootObjs[m_At], m_Rows + 1);
				if (row)
				{
					m_Rows++;
					m_Blocks.Add(row);
				}
				m_At++;
				m_Count++;
			}
			else if (m_Phase == SECTIONS)
			{
				if (m_At >= m_Sections.Count())
				{
					DZS_KeptBlocks.AddTo(m_Kept, m_Root, m_KeepSlug);
					m_Kept = null;
					return true;
				}
				DZS_Json frag = m_Sections[m_At].Fragment(m_Inst);
				DZS_ProjectV2.AddFragment(frag, m_Blocks, m_Changes);
				m_At++;
				m_Count++;
			}
		}
		return true;
	}

	protected bool StepGroups(int t0, float ms)
	{
		while (m_At < m_Groups.Count())
		{
			string gid = m_Groups[m_At];
			if (!m_Ids)
			{
				if (!m_BlockIds.Find(gid, m_Unique))
				{
					m_Count += m_Inst.GetChildren(gid).Count();
					m_At++;
					continue;
				}
				m_Ids = new array<string>();
				m_Ids.Copy(m_Inst.GetChildren(gid));
				m_Objects = DZS_Json.Arr();
				m_Issues = new DZS_Issues();
				m_Oi = 0;
			}
			while (m_Oi < m_Ids.Count())
			{
				if (DZS_Job.Over(t0, ms))
					return false;
				string oid = m_Ids[m_Oi];
				m_Oi++;
				m_Count++;
				if (m_Inst.GetKind(oid) != DZS_KIND_OBJECT)
					continue;
				DZS_Json obj = DZS_ProjectV2.ObjectEntry(m_Sf, m_Inst, oid, m_Issues, m_WorldSize, m_Unique, m_Objects.Count());
				if (obj)
					m_Objects.Add(obj);
			}
			DZS_Json b = DZS_ProjectV2.GroupBlock(m_Inst, gid, m_Unique, m_Objects);
			m_Blocks.Add(b);
			m_Ids = null;
			m_Objects = null;
			m_At++;
		}
		return true;
	}
}

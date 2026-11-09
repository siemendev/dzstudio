// "Apply to mission folder …": the open project as building blocks, applied straight into a mission folder below
// dzstudio\Missions (the game reads and writes only below its own folders, so a server's mission folder is copied
// there first). Every project applied to a folder is listed there and the folder is rebuilt from the original files
// with all of them in order, so applying again changes nothing twice and "Undo" takes one project out. With the map's
// vanilla mission as the base (fetched on demand) the result goes into a new folder that holds only the projects'
// files. The chosen base is remembered per project.
class DZS_PatchBase : Managed
{
	string m_Key;
	string m_Name;
	string m_Dir;
	string m_Meta;
	bool m_Vanilla;
	// where the changes go, and when this project was last applied there ("" = not applied)
	string m_Target;
	string m_TargetShown;
	string m_Applied;
}

class DZS_AppliedProject : Managed
{
	string m_Slug;
	string m_Name;
	string m_Stamp;
}

// The projects applied to one mission folder, the way dzbot transfers mods to a server: beside every file the projects
// changed lies <file>.original with the file as it was before (an empty one: the file did not exist), and a rebuild
// puts every original back and applies all listed projects in their order, each on the result of the ones before.
// A folder that already has .original files (copied from a server dzbot manages) counts them as the originals.
// <folder>\.dzstudio\applied.json lists the projects, the files the last rebuild touched and the .original files
// dzStudio wrote; projects\<slug>.dzstudio.json is the copy of each project that was applied, so the folder stays
// complete when it is moved or uploaded. Relative paths use "/" like the patcher's output.
class DZS_ApplyStore : Managed
{
	static const string ROOT = ".dzstudio";
	static const string STATE = "applied.json";
	static const string ORIGINAL = ".original";
	// marks a folder made from the vanilla mission: it holds only projects' files and is no base of its own
	static const string VANILLA_MARK = "vanilla-copy.txt";

	string m_Target;
	// where the patcher reads: the folder itself, or the vanilla mission for a vanilla copy
	string m_Base;
	ref array<ref DZS_AppliedProject> m_Projects = new array<ref DZS_AppliedProject>();
	ref array<string> m_Touched = new array<string>();
	ref array<string> m_Own = new array<string>();

	void DZS_ApplyStore(string target, string baseDir)
	{
		m_Target = target;
		m_Base = baseDir;
		DZS_Json root = DZS_WorkFolder.RootOf(DZS_Files.Read(StatePath()));
		if (!root || root.GetStr("format") != "dzstudio-applied")
			return;
		DZS_Json list = root.Get("projects");
		if (list && list.IsArr())
		{
			for (int i = 0; i < list.Count(); i++)
			{
				DZS_Json one = list.At(i);
				if (!one.IsObj() || one.GetStr("slug") == "")
					continue;
				DZS_AppliedProject ap = new DZS_AppliedProject();
				ap.m_Slug = one.GetStr("slug");
				ap.m_Name = one.GetStr("name");
				ap.m_Stamp = one.GetStr("applied");
				m_Projects.Insert(ap);
			}
		}
		Strings(root.Get("touched"), m_Touched);
		Strings(root.Get("originals"), m_Own);
	}

	protected static void Strings(DZS_Json list, array<string> into)
	{
		if (!list || !list.IsArr())
			return;
		for (int i = 0; i < list.Count(); i++)
		{
			DZS_Json one = list.At(i);
			if (one.IsStr() && one.m_Str != "")
				into.Insert(one.m_Str);
		}
	}

	string StatePath()
	{
		return m_Target + "\\" + ROOT + "\\" + STATE;
	}

	string CopyPath(string slug)
	{
		return m_Target + "\\" + ROOT + "\\projects\\" + slug + ".dzstudio.json";
	}

	string ReportPath(string slug)
	{
		return m_Target + "\\" + ROOT + "\\projects\\" + slug + ".report.txt";
	}

	static bool IsVanillaCopy(string dir)
	{
		return FileExist(dir + "\\" + ROOT + "\\" + VANILLA_MARK);
	}

	static void MarkVanillaCopy(string dir)
	{
		DZS_Files.MakeDirs(dir + "\\" + ROOT);
		string mark = dir + "\\" + ROOT + "\\" + VANILLA_MARK;
		if (FileExist(mark))
			return;
		array<string> text = DZS_Files.Chunk("dzStudio: only the files of the projects applied to the vanilla mission\n");
		DZS_Files.Write(mark, text);
	}

	static string Win(string rel)
	{
		string r = rel;
		r.Replace("/", "\\");
		return r;
	}

	static int IndexLow(array<string> list, string rel)
	{
		string want = DZS_PatchText.Lower(rel);
		for (int i = 0; i < list.Count(); i++)
		{
			if (DZS_PatchText.Lower(list[i]) == want)
				return i;
		}
		return -1;
	}

	static void MakeParent(string full)
	{
		int slash = full.LastIndexOf("\\");
		if (slash > 0)
			DZS_Files.MakeDirs(full.Substring(0, slash));
	}

	// Removes the folders of rel below root that became empty (deleting a folder that still has files fails).
	static void PruneUp(string root, string rel)
	{
		array<string> parts = new array<string>();
		rel.Split("/", parts);
		for (int k = parts.Count() - 1; k >= 1; k--)
		{
			string dir = root;
			for (int j = 0; j < k; j++)
				dir += "\\" + parts[j];
			if (!DeleteFile(dir))
				return;
		}
	}

	static bool IsEmptyFile(string path)
	{
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (!fh)
			return false;
		string part;
		int n = ReadFile(fh, part, 1);
		CloseFile(fh);
		return n <= 0;
	}

	// What the patcher reads instead of a folder's files: every <file>.original (an empty one hides the file). Also
	// what the editor takes as a mission folder's own state when it is a project's base.
	static map<string, string> Originals(string dir)
	{
		map<string, string> r = new map<string, string>();
		int ol = ORIGINAL.Length();
		array<string> all = DZS_Files.ListTree(dir);
		foreach (string rel : all)
		{
			string low = DZS_PatchText.Lower(rel);
			int n = low.Length();
			if (n <= ol || low.Substring(n - ol, ol) != ORIGINAL || low.IndexOf(ROOT + "/") == 0)
				continue;
			string path = dir + "\\" + Win(rel);
			string alt = "";
			if (!IsEmptyFile(path))
				alt = path;
			r.Set(low.Substring(0, n - ol), alt);
		}
		return r;
	}

	DZS_AppliedProject Find(string slug)
	{
		foreach (DZS_AppliedProject ap : m_Projects)
		{
			if (ap.m_Slug == slug)
				return ap;
		}
		return null;
	}

	// Adds a project or replaces it in its place, with the project file it is applied with.
	bool Put(string slug, string name, array<string> project)
	{
		string copy = CopyPath(slug);
		MakeParent(copy);
		if (!DZS_Files.Write(copy, project))
			return false;
		DZS_AppliedProject ap = Find(slug);
		if (!ap)
		{
			ap = new DZS_AppliedProject();
			ap.m_Slug = slug;
			m_Projects.Insert(ap);
		}
		ap.m_Name = name;
		ap.m_Stamp = DZS_ProjectsPlugin.Stamp();
		return true;
	}

	void Take(string slug)
	{
		for (int i = m_Projects.Count() - 1; i >= 0; i--)
		{
			if (m_Projects[i].m_Slug == slug)
				m_Projects.RemoveOrdered(i);
		}
		DZS_Files.Delete(CopyPath(slug));
		DZS_Files.Delete(ReportPath(slug));
	}

	// A file back to its original: the copy of it, or gone when the original is empty.
	protected void Restore(string rel)
	{
		string full = m_Target + "\\" + Win(rel);
		string orig = full + ORIGINAL;
		if (!FileExist(orig))
			return;
		if (IsEmptyFile(orig))
		{
			if (FileExist(full))
				DeleteFile(full);
			PruneUp(m_Target, rel);
			return;
		}
		DZS_Files.Copy(orig, full);
	}

	// Before a file is changed or created for the first time: its .original, unless one is there already.
	protected bool KeepOriginal(string rel, array<string> own)
	{
		string full = m_Target + "\\" + Win(rel);
		string orig = full + ORIGINAL;
		if (FileExist(orig))
		{
			if (IndexLow(m_Own, rel) >= 0)
				own.Insert(rel);
			return true;
		}
		MakeParent(orig);
		bool ok;
		if (FileExist(full))
			ok = DZS_Files.Copy(full, orig);
		else
		{
			array<string> none = new array<string>();
			ok = DZS_Files.Write(orig, none);
		}
		if (!ok || !FileExist(orig))
			return false;
		own.Insert(rel);
		return true;
	}

	protected void SaveState(array<string> touched, array<string> own)
	{
		DZS_Json root = DZS_Json.Obj();
		DZS_Json fj = DZS_Json.Str("dzstudio-applied");
		root.Set("format", fj);
		DZS_Json vj = DZS_Json.Int(1);
		root.Set("version", vj);
		DZS_Json list = DZS_Json.Arr();
		foreach (DZS_AppliedProject ap : m_Projects)
		{
			DZS_Json one = DZS_Json.Obj();
			DZS_Json sj = DZS_Json.Str(ap.m_Slug);
			one.Set("slug", sj);
			DZS_Json nj = DZS_Json.Str(ap.m_Name);
			one.Set("name", nj);
			DZS_Json aj = DZS_Json.Str(ap.m_Stamp);
			one.Set("applied", aj);
			string cj = "projects/" + ap.m_Slug + ".dzstudio.json";
			DZS_Json pj = DZS_Json.Str(cj);
			one.Set("file", pj);
			list.Add(one);
		}
		root.Set("projects", list);
		DZS_Json tj = DZS_Json.Arr();
		foreach (string t : touched)
		{
			DZS_Json ts = DZS_Json.Str(t);
			tj.Add(ts);
		}
		root.Set("touched", tj);
		DZS_Json oj = DZS_Json.Arr();
		foreach (string o : own)
		{
			DZS_Json os = DZS_Json.Str(o);
			oj.Add(os);
		}
		root.Set("originals", oj);
		string path = StatePath();
		MakeParent(path);
		array<string> chunks = root.ToChunks(true);
		DZS_Files.Write(path, chunks);
	}

	// Every touched file back to its original, then every listed project in order. focusRun: the run of the project
	// 'focus'; others: the names of the other projects applied again (unreadable ones are left out and named in
	// 'skipped'). An empty list leaves the folder as it was before the first project.
	bool Rebuild(string focus, string baseName, out DZS_Patcher focusRun, array<string> others, array<string> skipped, out string err)
	{
		if (m_Projects.Count() > 0 && !DZS_Files.IsFolder(m_Base))
		{
			err = "base missing: " + m_Base;
			return false;
		}
		foreach (string back : m_Touched)
			Restore(back);
		array<string> touched = new array<string>();
		array<string> own = new array<string>();
		map<string, string> over = new map<string, string>();
		if (m_Base == m_Target)
			over = Originals(m_Target);
		foreach (DZS_AppliedProject ap : m_Projects)
		{
			DZS_Json root = DZS_WorkFolder.RootOf(DZS_Files.Read(CopyPath(ap.m_Slug)));
			string perr = "unreadable";
			DZS_PatchProject pj = null;
			if (root)
				pj = DZS_PatchProject.FromJson(root, perr);
			if (!pj)
			{
				DZS_Log.Warn("patch: applied project " + ap.m_Slug + " in " + m_Target + " left out: " + perr);
				skipped.Insert(ap.m_Name);
				continue;
			}
			DZS_Patcher run = DZS_Patcher.Run(pj, m_Base, over);
			if (ap.m_Slug == focus)
				focusRun = run;
			else
				others.Insert(ap.m_Name);
			foreach (string rel : run.m_OutPaths)
			{
				if (IndexLow(touched, rel) >= 0)
					continue;
				if (!KeepOriginal(rel, own))
				{
					err = "cannot keep " + rel;
					return false;
				}
				touched.Insert(rel);
			}
			array<string> pending = new array<string>();
			pending.Copy(m_Touched);
			foreach (string tr : touched)
			{
				if (IndexLow(pending, tr) < 0)
					pending.Insert(tr);
			}
			array<string> pendingOwn = new array<string>();
			pendingOwn.Copy(m_Own);
			foreach (string orr : own)
			{
				if (IndexLow(pendingOwn, orr) < 0)
					pendingOwn.Insert(orr);
			}
			SaveState(pending, pendingOwn);
			foreach (string w : run.m_OutPaths)
			{
				string full = m_Target + "\\" + Win(w);
				MakeParent(full);
				array<string> data = run.m_Out.Get(w);
				if (!DZS_Files.Write(full, data))
				{
					err = "cannot write " + w;
					return false;
				}
				over.Set(DZS_PatchText.Lower(w), full);
			}
			array<string> lines = DZS_ApplyResult.Lines(run.m_Report, baseName);
			string text = "";
			foreach (string l : lines)
				text += l + "\n";
			array<string> reportChunks = DZS_Files.Chunk(text);
			DZS_Files.Write(ReportPath(ap.m_Slug), reportChunks);
		}
		// files no project changes any more are back to their original already; dzStudio's .original goes
		foreach (string gone : m_Touched)
		{
			if (IndexLow(touched, gone) >= 0 || IndexLow(m_Own, gone) < 0)
				continue;
			DeleteFile(m_Target + "\\" + Win(gone) + ORIGINAL);
			PruneUp(m_Target, gone);
		}
		m_Touched = touched;
		m_Own = own;
		if (m_Projects.Count() == 0)
		{
			Drop();
			return true;
		}
		SaveState(m_Touched, m_Own);
		return true;
	}

	// No project left: .dzstudio goes, and a vanilla copy that holds nothing else by then.
	protected void Drop()
	{
		DZS_Files.DeleteTree(m_Target + "\\" + ROOT);
		if (m_Base != m_Target)
			DeleteFile(m_Target);
	}
}

class DZS_PatchApply : Managed
{
	static const string MISSIONS = "$saves:dzstudio\\Missions";
	static const string SETTING = "patcher.bases";

	DZS_Kernel m_Kernel;
	ref DZS_LootVanilla m_Fetch;
	ref DZS_PatchBase m_Pending;
	ref DZS_PatchBase m_UndoBase;
	DZS_ApplyDialog m_UndoFrom;

	void DZS_PatchApply(DZS_Kernel k)
	{
		m_Kernel = k;
	}

	DZS_ProjectsPlugin Projects()
	{
		return DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
	}

	// The map of the open project: the loaded world, else the map the project file names.
	string MapKey()
	{
		DZS_ProjectsPlugin pp = Projects();
		if (!pp || !pp.m_Open)
			return "";
		if (pp.m_Open.m_MapKey != "")
			return pp.m_Open.m_MapKey;
		DZS_ProjectInfo info = DZS_LocalStore.Info(pp.m_Open.m_Folder);
		if (info)
			return info.m_MapKey;
		return "";
	}

	string ProjectName()
	{
		DZS_ProjectsPlugin pp = Projects();
		if (!pp || !pp.m_Open)
			return "";
		return pp.m_Open.m_Name;
	}

	bool CanApply(out string reasonDe, out string reasonEn)
	{
		DZS_ProjectsPlugin pp = Projects();
		if (!pp || !pp.m_Open || !m_Kernel.GetShown())
		{
			reasonDe = "Kein Projekt offen";
			reasonEn = "No project open";
			return false;
		}
		if (m_Fetch)
		{
			reasonDe = "Vanilla-Mission wird geladen";
			reasonEn = "Loading the vanilla mission";
			return false;
		}
		// a required extension this studio does not know changes what the project means
		if (DZS_ProjectExtensions.Blocked(reasonDe, reasonEn))
			return false;
		return true;
	}

	// Shown form of a folder below $saves:dzstudio ("Documents\DayZ\dzstudio\Missions\Mein Server").
	static string Shown(string sub)
	{
		string root = DZS_LocalStore.ShownRoot();
		int cut = root.LastIndexOf("\\");
		if (cut > 0)
			root = root.Substring(0, cut);
		return root + "\\" + sub;
	}

	static string FolderName(string projectName)
	{
		string bad = "\\/:*?\"<>|";
		string r = "";
		for (int i = 0; i < projectName.Length(); i++)
		{
			string c = projectName.Get(i);
			if (bad.Contains(c))
				r += "-";
			else
				r += c;
		}
		r.TrimInPlace();
		if (r == "")
			r = "project";
		return r;
	}

	// Where a base's changes go: the mission folder itself, or for vanilla "<project> (Vanilla <map>)".
	void Describe(DZS_PatchBase b, string projectName, string mapKey)
	{
		string leaf = b.m_Name;
		if (b.m_Vanilla)
			leaf = FolderName(projectName) + " (Vanilla " + DZS_LootPlugin.MapName(mapKey) + ")";
		b.m_Target = MISSIONS + "\\" + leaf;
		b.m_TargetShown = Shown("Missions\\" + leaf);
		string slug = DZS_ProjectV2.Slug(projectName);
		DZS_ApplyStore st = new DZS_ApplyStore(b.m_Target, b.m_Dir);
		DZS_AppliedProject mine = st.Find(slug);
		b.m_Applied = "";
		if (mine)
			b.m_Applied = mine.m_Stamp;
		int others = st.m_Projects.Count();
		if (mine)
			others--;
		string meta = "Missions\\" + b.m_Name;
		if (b.m_Vanilla && DZS_LootVanilla.HaveMission(mapKey))
			meta = DZS_Texts.Pick("Bohemia · geladen", "Bohemia · fetched");
		else if (b.m_Vanilla)
			meta = DZS_Texts.Pick("Bohemia · wird beim Anwenden geladen (Internet)", "Bohemia · fetched when applying (internet)");
		else if (!FileExist(b.m_Dir + "\\db\\types.xml") && !FileExist(b.m_Dir + "\\cfgeventspawns.xml"))
			meta += DZS_Texts.Pick(" · sieht nicht wie ein Missionsordner aus", " · does not look like a mission folder");
		if (b.m_Applied != "")
			meta += DZS_Texts.Pick(" · Projekt angewendet am ", " · project applied on ") + DZS_ProjectsPlugin.ShowStamp(b.m_Applied);
		if (others > 0)
			meta += " · " + others + DZS_Texts.Pick(" weitere(s) Projekt(e) angewendet", " other project(s) applied");
		b.m_Meta = meta;
	}

	// The vanilla mission of the map first, then every folder in dzstudio\Missions (not the vanilla copies).
	array<ref DZS_PatchBase> Bases(string mapKey, string projectName)
	{
		array<ref DZS_PatchBase> r = new array<ref DZS_PatchBase>();
		DZS_PatchBase v = new DZS_PatchBase();
		v.m_Key = "vanilla";
		v.m_Vanilla = true;
		v.m_Name = "Vanilla " + DZS_LootPlugin.MapName(mapKey);
		v.m_Dir = DZS_LootVanilla.MissionDir(mapKey);
		Describe(v, projectName, mapKey);
		r.Insert(v);
		DZS_Files.MakeDirs(MISSIONS);
		array<string> names;
		array<bool> folders;
		DZS_Files.List(MISSIONS, names, folders);
		for (int i = 0; i < names.Count(); i++)
		{
			if (!folders[i])
				continue;
			string dir = MISSIONS + "\\" + names[i];
			if (DZS_ApplyStore.IsVanillaCopy(dir))
				continue;
			DZS_PatchBase b = new DZS_PatchBase();
			b.m_Key = "mission:" + names[i];
			b.m_Name = names[i];
			b.m_Dir = dir;
			Describe(b, projectName, mapKey);
			r.Insert(b);
		}
		return r;
	}

	// The base of a project: the open project's own (from its project file), else the one last chosen for the folder.
	string Remembered(string folder)
	{
		DZS_ProjectsPlugin pp = Projects();
		if (pp && pp.m_Open && pp.m_Open.m_Folder == folder && pp.m_Open.m_Base != "")
			return pp.m_Open.m_Base;
		DZS_Json all = m_Kernel.GetSettings().Get(SETTING);
		if (!all || !all.IsObj())
			return "vanilla";
		string k = all.GetStr(folder);
		if (k == "")
			return "vanilla";
		return k;
	}

	void Remember(string folder, string key)
	{
		DZS_Settings st = m_Kernel.GetSettings();
		DZS_Json all = st.Get(SETTING);
		DZS_Json copy = DZS_Json.Obj();
		if (all && all.IsObj())
			copy = all.Clone();
		DZS_Json kj = DZS_Json.Str(key);
		copy.Set(folder, kj);
		st.Set(SETTING, copy);
		DZS_ProjectsPlugin pp = Projects();
		if (pp && pp.m_Open && pp.m_Open.m_Folder == folder)
			pp.BaseChosen(key);
	}

	void Ask()
	{
		DZS_ProjectsPlugin pp = Projects();
		string mapKey = MapKey();
		array<ref DZS_PatchBase> bases = Bases(mapKey, pp.m_Open.m_Name);
		string want = Remembered(pp.m_Open.m_Folder);
		int pick = 0;
		for (int i = 0; i < bases.Count(); i++)
		{
			if (bases[i].m_Key == want)
				pick = i;
		}
		DZS_ApplyDialog d = new DZS_ApplyDialog(this, bases, pick);
		d.Open();
	}

	void Go(DZS_PatchBase b)
	{
		DZS_ProjectsPlugin pp = Projects();
		Remember(pp.m_Open.m_Folder, b.m_Key);
		if (b.m_Vanilla && !DZS_LootVanilla.HaveMission(MapKey()))
		{
			m_Pending = b;
			m_Fetch = new DZS_LootVanilla();
			m_Fetch.m_OnDone.Insert(OnFetched);
			if (!m_Fetch.StartMission(MapKey()))
			{
				m_Fetch = null;
				Failed(DZS_Texts.Pick("kein Netz", "no network"));
				return;
			}
			m_Kernel.GetScreen().ShowBanner("patcher.fetch", 20, "info", DZS_Texts.Pick("Vanilla-Mission wird geladen …", "Loading the vanilla mission …"));
			return;
		}
		Run(b);
	}

	void OnFetched(DZS_LootVanilla v)
	{
		m_Kernel.GetScreen().HideBanner("patcher.fetch");
		bool ok = v.m_Ok;
		string why = v.m_Why;
		m_Fetch = null;
		if (!ok)
		{
			Failed(why);
			return;
		}
		DZS_PatchBase b = m_Pending;
		m_Pending = null;
		Run(b);
	}

	protected void Failed(string why)
	{
		m_Kernel.GetScreen().ShowMessage("patcher.apply", "warning", DZS_Texts.Pick("Anwenden nicht möglich: ", "Could not apply: ") + why, 10.0);
		DZS_Log.Info("patch: apply failed: " + why);
	}

	// The project goes into the target folder's list (in its place when it is there already) and the folder is rebuilt
	// from its originals with every listed project, so applying again changes nothing twice.
	void Run(DZS_PatchBase b)
	{
		DZS_ProjectsPlugin pp = Projects();
		DZS_Instance inst = m_Kernel.GetShown();
		if (!pp || !pp.m_Open || !inst)
			return;
		string blockedDe;
		string blockedEn;
		if (DZS_ProjectExtensions.Blocked(blockedDe, blockedEn))
		{
			Failed(DZS_Texts.Pick(blockedDe, blockedEn));
			return;
		}
		string name = pp.m_Open.m_Name;
		string mapKey = MapKey();
		Describe(b, name, mapKey);
		int t0 = TickCount(0);
		DZS_Json project = DZS_ProjectV2.Build(m_Kernel, inst, name, mapKey);
		string err;
		DZS_PatchProject pj = DZS_PatchProject.FromJson(project, err);
		if (!pj)
		{
			Failed(err);
			return;
		}
		if (b.m_Vanilla)
			DZS_ApplyStore.MarkVanillaCopy(b.m_Target);
		string slug = DZS_ProjectV2.Slug(name);
		DZS_ApplyStore st = new DZS_ApplyStore(b.m_Target, b.m_Dir);
		array<string> text = project.ToChunks(true);
		if (!st.Put(slug, name, text))
		{
			Failed("cannot write " + st.CopyPath(slug));
			return;
		}
		DZS_Patcher p = null;
		array<string> others = new array<string>();
		array<string> skipped = new array<string>();
		if (!st.Rebuild(slug, b.m_Name, p, others, skipped, err) || !p)
		{
			if (err == "")
				err = "no result";
			Failed(err);
			return;
		}
		Describe(b, name, mapKey);
		int ms = (TickCount(0) - t0) / 10000;
		string done = "patch: applied '" + name + "' to " + b.m_Dir + " in " + ms + " ms -> " + b.m_Target;
		done += ", " + p.m_OutPaths.Count() + " file(s), " + st.m_Touched.Count() + " touched in all, " + others.Count() + " other project(s) again";
		DZS_Log.Info(done);
		DZS_ApplyResultDialog d = new DZS_ApplyResultDialog(this, p.m_Report, b, p.m_OutPaths.Count(), others, skipped, slug);
		d.Open();
	}

	// "Undo" / "Remove": asks first; from the apply dialog its row is updated afterwards.
	void AskUndo(DZS_PatchBase b, DZS_ApplyDialog source = null)
	{
		m_UndoBase = b;
		m_UndoFrom = source;
		string kicker = DZS_Texts.Pick("Projekt", "Project");
		string title = DZS_Texts.Pick("Rückgängig machen", "Undo");
		string text = DZS_Texts.Pick("Alle Änderungen von „", "Take back every change of '") + ProjectName();
		text += DZS_Texts.Pick("“ in ", "' in ") + b.m_TargetShown;
		text += DZS_Texts.Pick(" zurücknehmen? Der Ordner wird aus den Originalen neu aufgebaut, andere dort angewendete Projekte bleiben.", "? The folder is rebuilt from the originals; other projects applied there stay.");
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(kicker, title, text);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button yes = c.AddFootButton("undo", DZS_Texts.Pick("Rückgängig", "Undo"), "danger");
		yes.m_Hit.m_Primary = true;
		c.m_OnResult.Insert(OnUndoAnswer);
		c.Open();
	}

	void OnUndoAnswer(DZS_Dialog d, string result)
	{
		DZS_PatchBase b = m_UndoBase;
		DZS_ApplyDialog source = m_UndoFrom;
		m_UndoBase = null;
		m_UndoFrom = null;
		if (result != "undo" || !b)
			return;
		string slug = DZS_ProjectV2.Slug(ProjectName());
		DZS_ApplyStore st = new DZS_ApplyStore(b.m_Target, b.m_Dir);
		int before = st.m_Touched.Count();
		if (st.m_Projects.Count() > 1 && !DZS_Files.IsFolder(b.m_Dir))
		{
			Failed(DZS_Texts.Pick("Basis fehlt: ", "base missing: ") + b.m_Name);
			return;
		}
		st.Take(slug);
		DZS_Patcher none = null;
		array<string> others = new array<string>();
		array<string> skipped = new array<string>();
		string err;
		if (!st.Rebuild("", b.m_Name, none, others, skipped, err))
		{
			Failed(err);
			return;
		}
		DZS_Log.Info("patch: undone '" + ProjectName() + "' in " + b.m_Target + ": " + before + " file(s) back to the original, " + others.Count() + " other project(s) again, " + st.m_Touched.Count() + " touched now");
		string msg = DZS_Texts.Pick("Rückgängig gemacht: Originale zurück", "Undone: originals back");
		if (others.Count() > 0)
			msg += DZS_Texts.Pick(", erneut angewendet: ", ", applied again: ") + Joined(others);
		m_Kernel.GetScreen().ShowMessage("patcher.apply", "success", msg, 8.0);
		Describe(b, ProjectName(), MapKey());
		if (source)
			source.Refresh();
	}

	static string Joined(array<string> names)
	{
		string r = "";
		foreach (string n : names)
		{
			if (r != "")
				r += ", ";
			r += n;
		}
		return r;
	}
}

// The report in words: counts, what needs a look, the files.
class DZS_ApplyResult
{
	static bool Fine(string status)
	{
		return status == "applied" || status == "already" || status == "gone" || status == "deduplicated";
	}

	static int CountOf(DZS_PatchReport r, string which)
	{
		int n = 0;
		foreach (DZS_PatchEntry e : r.m_Entries)
		{
			string s = e.m_Status;
			if (which == "ok" && Fine(s))
				n++;
			else if (which == "overrode" && s == "overrode")
				n++;
			else if (which == "missing" && s == "orphaned")
				n++;
			else if (which == "refused" && (s == "invalid" || s == "ambiguous" || s == "collision"))
				n++;
		}
		return n;
	}

	static string StatusText(string s)
	{
		if (s == "overrode")
			return DZS_Texts.Pick("überschrieben (auf dem Server stand etwas anderes)", "replaced (the server had another value)");
		if (s == "orphaned")
			return DZS_Texts.Pick("gibt es in dieser Basis nicht", "not in this base");
		if (s == "ambiguous")
			return DZS_Texts.Pick("mehrdeutig", "ambiguous");
		if (s == "collision")
			return DZS_Texts.Pick("gibt es schon", "already exists");
		if (s == "invalid")
			return DZS_Texts.Pick("abgelehnt", "refused");
		return s;
	}

	static string FileText(string s)
	{
		if (s == "written")
			return DZS_Texts.Pick("geändert", "changed");
		if (s == "created")
			return DZS_Texts.Pick("neu", "new");
		if (s == "unchanged")
			return DZS_Texts.Pick("unverändert", "unchanged");
		if (s == "missing")
			return DZS_Texts.Pick("fehlt in der Basis", "missing in the base");
		if (s == "broken")
			return DZS_Texts.Pick("in der Basis defekt", "broken in the base");
		if (s == "invalid")
			return DZS_Texts.Pick("nicht geschrieben", "not written");
		return s;
	}

	// "lootType Land_Shed_M1 · lootmax" for an entry.
	static string What(DZS_PatchEntry e)
	{
		string w = e.m_Kind;
		if (e.m_What == "registration")
			w = e.m_Field;
		if (e.m_Key && e.m_Key.IsObj())
		{
			foreach (string k : e.m_Key.m_Keys)
			{
				DZS_Json v = e.m_Key.Get(k);
				if (v && (v.IsStr() || v.IsNum()))
					w += " " + v.m_Str;
			}
		}
		else if (e.m_Id != "")
			w += " " + e.m_Id;
		if (e.m_Value != "")
			w += " " + e.m_Value;
		if (e.m_Field != "" && e.m_What != "registration")
			w += " · " + e.m_Field;
		return w;
	}

	static array<string> Notable(DZS_PatchReport r, int max)
	{
		array<string> found = new array<string>();
		foreach (DZS_PatchEntry e : r.m_Entries)
		{
			if (Fine(e.m_Status))
				continue;
			if (found.Count() >= max)
				break;
			string d = "";
			if (e.m_Detail != "")
				d = " [" + e.m_Detail + "]";
			found.Insert(What(e) + " — " + StatusText(e.m_Status) + d);
		}
		return found;
	}

	static array<string> Lines(DZS_PatchReport r, string baseName)
	{
		array<string> l = new array<string>();
		l.Insert(DZS_Texts.Pick("Angewendet auf ", "Applied to ") + baseName);
		string counts = CountOf(r, "ok").ToString() + DZS_Texts.Pick(" übernommen, ", " applied, ");
		counts += CountOf(r, "overrode").ToString() + DZS_Texts.Pick(" überschrieben, ", " replaced, ");
		counts += CountOf(r, "missing").ToString() + DZS_Texts.Pick(" fehlen, ", " missing, ");
		counts += CountOf(r, "refused").ToString() + DZS_Texts.Pick(" abgelehnt", " refused");
		l.Insert(counts);
		l.Insert("");
		array<string> all = Notable(r, 1000);
		foreach (string n : all)
			l.Insert(n);
		if (all.Count() > 0)
			l.Insert("");
		array<DZS_PatchFileNote> files = r.SortedFiles();
		foreach (DZS_PatchFileNote f : files)
			l.Insert(f.m_Path + " — " + FileText(f.m_Status));
		if (r.m_Notes.Find("enableCfgGameplayFile") >= 0)
		{
			l.Insert("");
			l.Insert(DZS_Texts.Pick("Hinweis: Der Server braucht enableCfgGameplayFile = 1 in serverDZ.cfg.", "Note: the server needs enableCfgGameplayFile = 1 in serverDZ.cfg."));
		}
		return l;
	}
}

// Choosing the base: one row per base, the chosen one marked, "Remove" where this project is applied; below, the
// folder the changes go to.
class DZS_ApplyDialog : DZS_Dialog
{
	protected DZS_PatchApply m_A;
	protected ref array<ref DZS_PatchBase> m_Bases;
	protected int m_Pick;
	protected ref array<TextWidget> m_Names = new array<TextWidget>();
	protected ref array<TextWidget> m_Metas = new array<TextWidget>();
	protected ref array<ref DZS_Button> m_Picks = new array<ref DZS_Button>();
	protected ref array<ref DZS_Button> m_Removes = new array<ref DZS_Button>();
	protected TextWidget m_BaseLbl;
	protected TextWidget m_Hint;
	protected TextWidget m_ChgLbl;
	protected TextWidget m_ChgPath;
	protected TextWidget m_ChgNote;

	void DZS_ApplyDialog(DZS_PatchApply a, array<ref DZS_PatchBase> bases, int pick)
	{
		m_A = a;
		m_Bases = bases;
		m_Pick = pick;
		Setup(MEDIUM, DZS_Texts.Pick("Projekt", "Project"), DZS_Texts.Pick("Auf Missionsordner anwenden", "Apply to mission folder"), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button go = AddFootButton("apply", DZS_Texts.Pick("Anwenden", "Apply"), "main");
		go.m_Hit.m_Primary = true;
		m_OnResult.Insert(OnAnswer);
	}

	override void BuildContent(Widget inner)
	{
		m_BaseLbl = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		m_BaseLbl.SetText(DZS_Texts.Pick("BASIS", "BASE"));
		for (int i = 0; i < m_Bases.Count(); i++)
		{
			TextWidget n = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
			n.SetText(m_Bases[i].m_Name);
			TextWidget m = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
			m.SetText(m_Bases[i].m_Meta);
			DZS_Button b = new DZS_Button(inner, "side", DZS_Texts.Pick("Wählen", "Choose"));
			b.m_Tag = i.ToString();
			b.m_OnClick.Insert(OnPick);
			DZS_Button r = new DZS_Button(inner, "danger", DZS_Texts.Pick("Entfernen", "Remove"));
			r.m_Tag = i.ToString();
			r.m_OnClick.Insert(OnRemove);
			r.SetTip(DZS_Texts.Pick("Änderungen dieses Projekts zurücknehmen, Originale wiederherstellen", "Take this project's changes back, restore the originals"));
			m_Names.Insert(n);
			m_Metas.Insert(m);
			m_Picks.Insert(b);
			m_Removes.Insert(r);
			AddFocusHit(b.m_Hit);
			AddFocusHit(r.m_Hit);
		}
		m_Hint = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Hint.SetText(DZS_Texts.Pick("Eigene Server-Dateien: Missionsordner nach ", "Your server's files: copy the mission folder to ") + DZS_PatchApply.Shown("Missions") + DZS_Texts.Pick(" kopieren.", "."));
		m_ChgLbl = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		m_ChgLbl.SetText(DZS_Texts.Pick("WIRD GEÄNDERT", "CHANGES GO TO"));
		m_ChgPath = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_ChgNote = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		Mark();
		AddFootFocus();
	}

	protected void Mark()
	{
		for (int i = 0; i < m_Picks.Count(); i++)
		{
			bool on = i == m_Pick;
			if (on)
			{
				m_Picks[i].SetLabel(DZS_Texts.Pick("Gewählt", "Chosen"));
				m_Names[i].SetColor(DZS_Color.Get("accentText"));
			}
			else
			{
				m_Picks[i].SetLabel(DZS_Texts.Pick("Wählen", "Choose"));
				m_Names[i].SetColor(DZS_Color.Get("text"));
			}
			m_Picks[i].SetEnabled(!on);
			m_Metas[i].SetText(m_Bases[i].m_Meta);
			m_Removes[i].Show(m_Bases[i].m_Applied != "");
		}
		DZS_PatchBase b = m_Bases[m_Pick];
		m_ChgPath.SetText(b.m_TargetShown);
		if (b.m_Vanilla)
			m_ChgNote.SetText(DZS_Texts.Pick("Neuer Ordner, nur die Dateien des Projekts – auf einen Server mit Vanilla-Mission kopieren", "A new folder with only the project's files – copy it onto a server that runs the vanilla mission"));
		else
			m_ChgNote.SetText(DZS_Texts.Pick("Originale bleiben als .original daneben – Rückgängig jederzeit", "The originals stay beside them as .original – undo any time"));
	}

	// After a remove: the rows show the new state.
	void Refresh()
	{
		Mark();
		Layout();
	}

	void OnPick(DZS_Button b)
	{
		m_Pick = b.m_Tag.ToInt();
		Mark();
		Layout();
	}

	void OnRemove(DZS_Button b)
	{
		DZS_PatchBase chosen = m_Bases[b.m_Tag.ToInt()];
		m_A.AskUndo(chosen, this);
	}

	protected float RowKnobs(int i)
	{
		float bw = m_Picks[i].GetWidth();
		if (m_Bases[i].m_Applied != "")
			bw += m_Removes[i].GetWidth() + DZS_Scale.Px(8);
		return bw;
	}

	override float BoxWidth()
	{
		float w = super.BoxWidth();
		for (int i = 0; i < m_Metas.Count(); i++)
		{
			float need = Math.Max(DZS_W.TextWidth(m_Metas[i]), DZS_W.TextWidth(m_Names[i])) + RowKnobs(i) + DZS_Scale.Px(64);
			w = Math.Max(w, need);
		}
		w = Math.Max(w, DZS_W.TextWidth(m_ChgPath) + DZS_Scale.Px(48));
		w = Math.Max(w, DZS_W.TextWidth(m_ChgNote) + DZS_Scale.Px(48));
		w = Math.Max(w, DZS_W.TextWidth(m_Hint) + DZS_Scale.Px(48));
		return Math.Min(w, DZS_Scale.Px(960));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		DZS_W.Place(m_BaseLbl, 0, y, width, DZS_Scale.Px(16));
		y += DZS_Scale.Px(22);
		for (int i = 0; i < m_Names.Count(); i++)
		{
			float bw = RowKnobs(i);
			float pw = m_Picks[i].GetWidth();
			DZS_W.Place(m_Names[i], 0, y, width - bw - DZS_Scale.Px(8), DZS_Scale.Px(20));
			DZS_W.Place(m_Metas[i], 0, y + DZS_Scale.Px(22), width - bw - DZS_Scale.Px(8), DZS_Scale.Px(18));
			m_Picks[i].Place(width - pw, y + DZS_Scale.Px(8));
			m_Removes[i].Place(width - bw, y + DZS_Scale.Px(8));
			y += DZS_Scale.Px(50);
		}
		DZS_W.Place(m_Hint, 0, y, width, DZS_Scale.Px(18));
		y += DZS_Scale.Px(34);
		DZS_W.Place(m_ChgLbl, 0, y, width, DZS_Scale.Px(16));
		y += DZS_Scale.Px(22);
		DZS_W.Place(m_ChgPath, 0, y, width, DZS_Scale.Px(20));
		y += DZS_Scale.Px(22);
		DZS_W.Place(m_ChgNote, 0, y, width, DZS_Scale.Px(18));
		y += DZS_Scale.Px(22);
		return y;
	}

	void OnAnswer(DZS_Dialog d, string result)
	{
		if (result != "apply")
			return;
		// after this dialog has closed, so the result opens on top of the editor
		DZS_PatchBase chosen = m_Bases[m_Pick];
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(m_A.Go, 1, false, chosen);
	}
}

// After applying: the counts, what needs a look (up to six lines), the files, the folder that was changed.
class DZS_ApplyResultDialog : DZS_Dialog
{
	protected DZS_PatchApply m_A;
	protected ref DZS_PatchBase m_Base;
	protected ref array<string> m_Lines = new array<string>();
	protected ref array<string> m_Tones = new array<string>();
	protected ref array<TextWidget> m_Widgets = new array<TextWidget>();

	void DZS_ApplyResultDialog(DZS_PatchApply a, DZS_PatchReport r, DZS_PatchBase b, int fileCount, array<string> others, array<string> skipped, string slug)
	{
		m_A = a;
		m_Base = b;
		Add(CountLine(r), "text");
		if (others.Count() > 0)
			Add(DZS_Texts.Pick("Erneut angewendet, weil sie im selben Ordner liegen: ", "Applied again, as they are in the same folder: ") + DZS_PatchApply.Joined(others), "textQuiet");
		if (skipped.Count() > 0)
			Add(DZS_Texts.Pick("Nicht lesbar und ausgelassen: ", "Unreadable and left out: ") + DZS_PatchApply.Joined(skipped), "warning");
		array<string> notable = DZS_ApplyResult.Notable(r, 6);
		int more = DZS_ApplyResult.CountOf(r, "overrode") + DZS_ApplyResult.CountOf(r, "missing") + DZS_ApplyResult.CountOf(r, "refused") - notable.Count();
		if (notable.Count() > 0)
			Add("", "text");
		foreach (string n : notable)
			Add(n, "warning");
		if (more > 0)
		{
			string report = ".dzstudio\\projects\\" + slug + ".report.txt";
			string moreText = DZS_Texts.Pick("… und ", "… and ") + more;
			moreText += DZS_Texts.Pick(" weitere (siehe ", " more (see ") + report + ")";
			Add(moreText, "textQuiet");
		}
		Add("", "text");
		array<DZS_PatchFileNote> files = r.SortedFiles();
		foreach (DZS_PatchFileNote f : files)
			Add(f.m_Path + " — " + DZS_ApplyResult.FileText(f.m_Status), "textQuiet");
		Add("", "text");
		if (fileCount == 0)
			Add(DZS_Texts.Pick("Nichts zu ändern – die Basis entspricht schon dem Projekt.", "Nothing to change – the base already matches the project."), "text");
		else if (b.m_Vanilla)
			Add(DZS_Texts.Pick("Neuer Ordner mit den Dateien des Projekts – auf einen Server mit Vanilla-Mission kopieren:", "A new folder with the project's files – copy it onto a server that runs the vanilla mission:"), "text");
		else
			Add(DZS_Texts.Pick("Direkt im Missionsordner geändert (Originale gesichert, Rückgängig jederzeit):", "Changed in the mission folder itself (originals kept, undo any time):"), "text");
		Add(b.m_TargetShown, "accentText");
		if (r.m_Notes.Find("enableCfgGameplayFile") >= 0)
			Add(DZS_Texts.Pick("Der Server braucht enableCfgGameplayFile = 1 in serverDZ.cfg.", "The server needs enableCfgGameplayFile = 1 in serverDZ.cfg."), "textQuiet");
		Setup(MEDIUM, DZS_Texts.Pick("Projekt", "Project"), DZS_Texts.Pick("Angewendet auf ", "Applied to ") + b.m_Name, false);
		if (b.m_Applied != "")
			AddFootButtonLeft("undo", DZS_Texts.Pick("Rückgängig", "Undo"), "side");
		AddFootButton("copy", DZS_Texts.Pick("Pfad kopieren", "Copy path"), "side");
		DZS_Button ok = AddFootButton("done", DZS_Texts.Pick("Fertig", "Done"), "main");
		ok.m_Hit.m_Primary = true;
		m_OnResult.Insert(OnAnswer);
	}

	protected void Add(string text, string tone)
	{
		m_Lines.Insert(text);
		m_Tones.Insert(tone);
	}

	protected static string CountLine(DZS_PatchReport r)
	{
		string s = DZS_ApplyResult.CountOf(r, "ok").ToString() + DZS_Texts.Pick(" übernommen", " applied");
		s += " · " + DZS_ApplyResult.CountOf(r, "overrode") + DZS_Texts.Pick(" überschrieben", " replaced");
		s += " · " + DZS_ApplyResult.CountOf(r, "missing") + DZS_Texts.Pick(" fehlen", " missing");
		s += " · " + DZS_ApplyResult.CountOf(r, "refused") + DZS_Texts.Pick(" abgelehnt", " refused");
		return s;
	}

	override void BuildContent(Widget inner)
	{
		for (int i = 0; i < m_Lines.Count(); i++)
		{
			int size = 13;
			if (i == 0)
				size = 15;
			TextWidget t = DZS_W.Text(inner, DZS_Font.READ, size, DZS_Color.Get(m_Tones[i]));
			t.SetText(m_Lines[i]);
			m_Widgets.Insert(t);
		}
		AddFootFocus();
	}

	override float BoxWidth()
	{
		float w = super.BoxWidth();
		foreach (TextWidget t : m_Widgets)
			w = Math.Max(w, DZS_W.TextWidth(t) + DZS_Scale.Px(48));
		return Math.Min(w, DZS_Scale.Px(1100));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		for (int i = 0; i < m_Widgets.Count(); i++)
		{
			float h = DZS_Scale.Px(19);
			if (m_Lines[i] == "")
				h = DZS_Scale.Px(8);
			if (i == 0)
				h = DZS_Scale.Px(24);
			DZS_W.Place(m_Widgets[i], 0, y, width, h);
			y += h;
		}
		return y;
	}

	void OnAnswer(DZS_Dialog d, string result)
	{
		if (result == "copy")
			GetGame().CopyToClipboard(m_Base.m_TargetShown);
		if (result == "undo")
		{
			DZS_PatchBase b = m_Base;
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(m_A.AskUndo, 1, false, b, null);
		}
	}
}

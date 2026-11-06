// Storage provider "Local folder": one folder per project
// under $saves:dzstudio\Projects\, one file per group, the project file beside them.
class DZS_ProjectInfo : Managed
{
	string m_Folder;
	string m_Name;
	string m_MapKey;
	int m_Groups;
	string m_Status;
}

// What the open project needs beyond the project state: where it lies and what was on disk.
class DZS_OpenProject : Managed
{
	string m_Provider = "local";
	string m_Folder;
	string m_Name;
	string m_MapKey;
	ref map<string, string> m_Sums;
	ref DZS_Issues m_OpenIssues;

	void DZS_OpenProject()
	{
		m_Sums = new map<string, string>();
		m_OpenIssues = new DZS_Issues();
	}

	static string Key(string rel)
	{
		string k = rel;
		k.ToLower();
		k.Replace("\\", "/");
		return k;
	}

	void Remember(string rel, string sum)
	{
		m_Sums.Set(Key(rel), sum);
	}

	string Remembered(string rel)
	{
		return m_Sums.Get(Key(rel));
	}
}

class DZS_SaveFile : Managed
{
	string m_Rel;
	ref array<string> m_Chunks;
	// DZS_Files.Checksum of m_Chunks when a provider already formed it in slices ("" = not yet)
	string m_Sum;
	// the file of a deleted group: removed by the save (with a fallback like a replaced one)
	bool m_Delete;
	bool m_HadOld;
	bool m_Replaced;
}

class DZS_LocalStore
{
	static const string ROOT = "$saves:dzstudio\\Projects";
	static const string GROUP_PLUGINS = "projects:plugins";
	static const string GROUP_ENTRY = "projects:entry";

	protected static bool EndsInDotOrSpace(string s, int keep)
	{
		string c = s.Get(keep - 1);
		return c == "." || c == " ";
	}

	// A folder under $profile: (the backup of a project) is taken as it is.
	static string Dir(string folder)
	{
		if (folder.IndexOf("$profile:") == 0)
			return folder;
		return ROOT + "\\" + folder;
	}

	protected static string s_Shown;

	// The folder behind $saves: as the user finds it: with -profiles the game puts $saves: into
	// <profiles>\Users\<profile name>\, otherwise into Documents\DayZ.
	static string ShownRoot()
	{
		if (s_Shown != "")
			return s_Shown;
		s_Shown = "Documents\\DayZ\\dzstudio\\Projects";
		string prof;
		if (!GetCLIParam("profiles", prof) || prof == "")
			return s_Shown;
		prof.Replace("/", "\\");
		while (prof.Length() > 0 && prof.Substring(prof.Length() - 1, 1) == "\\")
			prof = prof.Substring(0, prof.Length() - 1);
		array<string> users;
		array<bool> folders;
		DZS_Files.List("$profile:Users", users, folders);
		for (int i = 0; i < users.Count(); i++)
		{
			if (folders[i] && DZS_Files.IsFolder("$profile:Users\\" + users[i] + "\\dzstudio"))
			{
				s_Shown = prof + "\\Users\\" + users[i] + "\\dzstudio\\Projects";
				DZS_Log.Info("projects shown under " + s_Shown);
				break;
			}
		}
		return s_Shown;
	}

	static string Path(string folder, string rel)
	{
		string r = rel;
		r.Replace("/", "\\");
		return Dir(folder) + "\\" + r;
	}

	static void EnsureRoot()
	{
		DZS_Files.MakeDirs(ROOT);
	}

	// ---- listing ----

	// Folders whose name starts with "@" belong to other providers (DZS_StoreProvider).
	static array<ref DZS_ProjectInfo> List(bool withProviders = false)
	{
		EnsureRoot();
		array<ref DZS_ProjectInfo> r = new array<ref DZS_ProjectInfo>();
		array<string> names;
		array<bool> folders;
		DZS_Files.List(ROOT, names, folders);
		for (int i = 0; i < names.Count(); i++)
		{
			if (!folders[i])
				continue;
			if (!withProviders && names[i].IndexOf("@") == 0)
				continue;
			DZS_ProjectInfo t121 = Info(names[i]);
			r.Insert(t121);
		}
		return r;
	}

	static DZS_ProjectInfo Info(string folder)
	{
		DZS_ProjectInfo info = new DZS_ProjectInfo();
		info.m_Folder = folder;
		info.m_Name = folder;
		DZS_ProjectFile.Adopt(Dir(folder));
		DZS_PFData pf = DZS_ProjectFile.Read(DZS_Files.Read(Path(folder, DZS_ProjectFile.NAME)));
		info.m_Status = pf.m_Status;
		if (pf.m_Status == "ok")
		{
			if (pf.m_Name != "")
				info.m_Name = pf.m_Name;
			info.m_MapKey = pf.m_MapKey;
		}
		info.m_Groups = GroupFiles(folder).Count();
		return info;
	}

	static bool Exists(string folder)
	{
		return DZS_Files.IsFolder(Dir(folder));
	}

	// Files that become object groups.
	static array<string> GroupFiles(string folder)
	{
		array<string> r = new array<string>();
		array<string> all = DZS_Files.ListTree(Dir(folder));
		DZS_Kernel k = DZS_Kernel.Get();
		foreach (string rel : all)
		{
			if (DZS_ProjectFile.IsNeverGroup(rel))
				continue;
			if (k && k.FormatForFile(rel))
				r.Insert(rel);
		}
		return r;
	}

	// ---- folder names ----

	static string FolderFor(string name)
	{
		string bad = "\\/:*?\"<>|";
		string res = "";
		for (int i = 0; i < name.Length(); i++)
		{
			string c = name.Get(i);
			int b = DZS_Utf8.Byte(name, i);
			if (bad.Contains(c) || b < 32)
				res += "_";
			else
				res += c;
		}
		int n = res.Length();
		int keep = n;
		while (keep > 0 && EndsInDotOrSpace(res, keep))
			keep--;
		if (keep < n)
		{
			string tail = "";
			for (int j = keep; j < n; j++)
				tail += "_";
			res = res.Substring(0, keep) + tail;
		}
		string base = res;
		string cand = base;
		int k = 2;
		while (Exists(cand))
		{
			cand = base + " (" + k + ")";
			k++;
		}
		return cand;
	}

	// ---- create ----

	static string NewProject(string name, string mapKey, string viewPart, string groupFile, string groupName)
	{
		EnsureRoot();
		string folder = FolderFor(name);
		DZS_Files.MakeDirs(Dir(folder));
		string empty = "{\n    \"Objects\": []\n}\n";
		DZS_Files.Write(Path(folder, groupFile), DZS_Files.Chunk(empty));
		array<ref DZS_PFObject> none = new array<ref DZS_PFObject>();
		array<string> entries = new array<string>();
		array<string> colors = DZS_Palette.Colors();
		string first = colors[0];
		entries.Insert(DZS_ProjectFile.GroupEntry(groupFile, groupName, first, false, false, 0, none, "[]"));
		array<string> noTypes = new array<string>();
		string proj = DZS_ProjectFile.ProjectPart(entries, false, false, false, "", noTypes, noTypes, false, "[]");
		DZS_Files.Write(Path(folder, DZS_ProjectFile.NAME), DZS_ProjectFile.Compose(name, mapKey, proj, viewPart));
		DZS_Log.Info("project created: folder '" + folder + "', name '" + name + "', map " + mapKey);
		return folder;
	}

	// ---- rename: only the provider part changes; project and view stay byte for byte ----

	static bool Rename(string folder, string newName, out string newSum)
	{
		string path = Path(folder, DZS_ProjectFile.NAME);
		array<string> cur = DZS_Files.Read(path);
		DZS_PFData pf = DZS_ProjectFile.Read(cur);
		if (pf.m_Status == "newer")
			return false;
		string mapKey = pf.m_MapKey;
		if (mapKey == "")
			mapKey = pf.m_MapRaw;
		string proj = "";
		string view = "";
		if (pf.m_Status == "ok")
		{
			proj = pf.m_RawProject;
			view = pf.m_RawView;
		}
		array<string> chunks = DZS_ProjectFile.Compose(newName, mapKey, proj, view);
		if (!DZS_Files.Write(path + ".new", chunks))
			return false;
		bool ok = DZS_Files.Copy(path + ".new", path);
		DZS_Files.Delete(path + ".new");
		if (ok)
			newSum = DZS_Files.Checksum(chunks);
		return ok;
	}

	static bool Delete(string folder)
	{
		return DZS_Files.DeleteTree(Dir(folder));
	}

	// ---- opening ----

	static bool InvalidRel(string rel)
	{
		string first = "";
		if (rel != "")
			first = rel.Get(0);
		if (rel == "" || rel.Contains(":") || first == "/" || first == "\\" || rel.Contains(".."))
			return true;
		return DZS_ProjectFile.IsNeverGroup(rel);
	}

	static int ComparePaths(string a, string b)
	{
		string la = a;
		string lb = b;
		la.ToLower();
		lb.ToLower();
		if (la != lb)
		{
			if (la < lb)
				return -1;
			return 1;
		}
		if (a == b)
			return 0;
		if (a < b)
			return -1;
		return 1;
	}

	static bool Near(float a, float b)
	{
		return Math.AbsFloat(a - b) <= 0.001;
	}

	// Assigns the manifest's object entries; returns per object the entry or null.
	static array<DZS_PFObject> Match(DZS_PFGroup g, DZS_ReadResult rr, out int lostLocks, out int lostHides)
	{
		int n = rr.m_Objects.Count();
		array<DZS_PFObject> got = new array<DZS_PFObject>();
		for (int i = 0; i < n; i++)
			got.Insert(null);
		array<DZS_PFObject> entries = new array<DZS_PFObject>();
		foreach (DZS_PFObject e0 : g.m_Objects)
		{
			if (e0.m_Hidden || e0.m_Locked)
				entries.Insert(e0);
		}
		array<DZS_PFObject> left = new array<DZS_PFObject>();
		foreach (DZS_PFObject e : entries)
		{
			bool done = false;
			if (g.m_ObjectCount == n && e.m_Index >= 0 && e.m_Index < n && !got[e.m_Index])
			{
				DZS_ReadObject o = rr.m_Objects[e.m_Index];
				if (o.m_FileName == e.m_Name && Near(o.m_FileX, e.m_X) && Near(o.m_FileY, e.m_Y) && Near(o.m_FileZ, e.m_Z))
				{
					got[e.m_Index] = e;
					done = true;
				}
			}
			if (!done)
				left.Insert(e);
		}
		foreach (DZS_PFObject l : left)
		{
			int found = -1;
			int count = 0;
			for (int k = 0; k < n; k++)
			{
				if (got[k])
					continue;
				DZS_ReadObject c = rr.m_Objects[k];
				if (c.m_FileName == l.m_Name && Near(c.m_FileX, l.m_X) && Near(c.m_FileY, l.m_Y) && Near(c.m_FileZ, l.m_Z))
				{
					found = k;
					count++;
				}
			}
			if (count == 1)
				got[found] = l;
			else
			{
				if (l.m_Locked)
					lostLocks++;
				if (l.m_Hidden)
					lostHides++;
			}
		}
		return got;
	}

	static string LostText(string group, int locks, int hides, bool english)
	{
		string a = "";
		if (english)
		{
			if (locks > 0)
				a = locks.ToString() + DZS_Issues.Plural(locks, " lock", " locks");
			if (hides > 0)
			{
				if (a != "")
					a += " and ";
				a += hides.ToString() + DZS_Issues.Plural(hides, " hide", " hides");
			}
			return "Group " + group + ": " + a + " no longer matched the file and were lifted";
		}
		if (locks > 0)
			a = locks.ToString() + DZS_Issues.Plural(locks, " Sperre", " Sperren");
		if (hides > 0)
		{
			if (a != "")
				a += " und ";
			a += hides.ToString() + DZS_Issues.Plural(hides, " Ausblendung", " Ausblendungen");
		}
		return "Gruppe " + group + ": " + a + " passten nicht mehr zur Datei und wurden aufgehoben";
	}

	// ---- saving: write next to, keep the old as fallback, replace ----

	// Returns "" on success, else the reason; "incomplete:<list>" when restoring failed too.
	static string WriteAll(string folder, array<ref DZS_SaveFile> files)
	{
		int failAt = -1;
		string fa;
		if (GetCLIParam("dzsFailReplace", fa))
			failAt = fa.ToInt();
		string failWrite;
		GetCLIParam("dzsFailWrite", failWrite);
		foreach (DZS_SaveFile f0 : files)
		{
			int slash = Math.Max(f0.m_Rel.LastIndexOf("/"), f0.m_Rel.LastIndexOf("\\"));
			if (slash > 0)
				DZS_Files.MakeDirs(Path(folder, f0.m_Rel.Substring(0, slash)));
		}
		foreach (DZS_SaveFile f : files)
		{
			if (f.m_Delete)
				continue;
			string p = Path(folder, f.m_Rel);
			bool ok = failWrite == "" || f.m_Rel != failWrite;
			if (ok)
				ok = DZS_Files.Write(p + ".new", f.m_Chunks);
			if (ok)
			{
				array<string> back = DZS_Files.Read(p + ".new");
				ok = DZS_Files.SameContent(back, f.m_Chunks);
			}
			if (!ok)
			{
				Cleanup(folder, files);
				return DZS_Texts.Pick("Datei " + f.m_Rel + " lässt sich nicht schreiben", "File " + f.m_Rel + " cannot be written");
			}
		}
		return ReplaceAll(folder, files);
	}

	// Every file written as ‹file›.new and read back: keep the old ones as .old, replace, clean up.
	static string ReplaceAll(string folder, array<ref DZS_SaveFile> files)
	{
		int failAt = -1;
		string fa;
		if (GetCLIParam("dzsFailReplace", fa))
			failAt = fa.ToInt();
		foreach (DZS_SaveFile g : files)
		{
			string gp = Path(folder, g.m_Rel);
			g.m_HadOld = DZS_Files.Exists(gp);
			if (g.m_HadOld && !DZS_Files.Copy(gp, gp + ".old"))
			{
				Cleanup(folder, files);
				return DZS_Texts.Pick("Rückfall für " + g.m_Rel + " lässt sich nicht anlegen", "Fallback for " + g.m_Rel + " cannot be made");
			}
		}
		for (int i = 0; i < files.Count(); i++)
		{
			DZS_SaveFile r = files[i];
			string rp = Path(folder, r.m_Rel);
			bool replaced = i + 1 != failAt;
			if (replaced && r.m_Delete)
				replaced = !DZS_Files.Exists(rp) || DZS_Files.Delete(rp);
			else if (replaced)
				replaced = DZS_Files.Copy(rp + ".new", rp);
			if (!replaced)
			{
				DZS_Log.Warn("save: replacing " + r.m_Rel + " failed, restoring");
				string bad = "";
				foreach (DZS_SaveFile u : files)
				{
					if (!u.m_Replaced)
						continue;
					string up = Path(folder, u.m_Rel);
					bool restored;
					if (u.m_HadOld)
						restored = DZS_Files.Copy(up + ".old", up);
					else
						restored = DZS_Files.Delete(up);
					if (!restored)
						bad += " " + u.m_Rel + " (new)";
				}
				if (bad != "")
				{
					foreach (DZS_SaveFile s : files)
					{
						if (!s.m_Replaced && bad.IndexOf(" " + s.m_Rel + " ") < 0)
							bad += " " + s.m_Rel + " (old)";
					}
					return "incomplete:" + bad;
				}
				Cleanup(folder, files);
				return DZS_Texts.Pick("Datei " + r.m_Rel + " lässt sich nicht ersetzen", "File " + r.m_Rel + " cannot be replaced");
			}
			r.m_Replaced = true;
		}
		Cleanup(folder, files);
		return "";
	}

	static void Cleanup(string folder, array<ref DZS_SaveFile> files)
	{
		foreach (DZS_SaveFile f : files)
		{
			string p = Path(folder, f.m_Rel);
			if (DZS_Files.Exists(p + ".new"))
				DZS_Files.Delete(p + ".new");
			if (DZS_Files.Exists(p + ".old"))
				DZS_Files.Delete(p + ".old");
		}
	}
}

// WriteAll in slices: each ‹file›.new written and read back over several frames, then replaced in one go.
class DZS_LocalWrite : Managed
{
	protected string m_Folder;
	protected ref array<ref DZS_SaveFile> m_Files;
	protected int m_File = -1;
	protected int m_Chunk;
	protected FileHandle m_Fh;
	protected bool m_Reading;
	protected ref array<string> m_Back;
	protected string m_FailWrite;
	protected ref DZS_FileSum m_Sum;
	protected int m_SumFile;
	string m_Fail;
	int m_Frames;
	float m_Slowest;

	void DZS_LocalWrite(string folder, array<ref DZS_SaveFile> files)
	{
		m_Folder = folder;
		m_Files = files;
		GetCLIParam("dzsFailWrite", m_FailWrite);
	}

	protected string Failed(DZS_SaveFile f)
	{
		if (m_Fh)
			CloseFile(m_Fh);
		m_Fh = null;
		DZS_LocalStore.Cleanup(m_Folder, m_Files);
		return DZS_Texts.Pick("Datei " + f.m_Rel + " lässt sich nicht schreiben", "File " + f.m_Rel + " cannot be written");
	}

	// true when done; m_Fail then holds "" or the reason
	bool Step(float ms)
	{
		int t0 = TickCount(0);
		float limit = ms * 10000;
		m_Frames++;
		bool done = Work(t0, limit);
		float took = (TickCount(0) - t0) / 10000.0;
		if (took > m_Slowest)
			m_Slowest = took;
		return done;
	}

	protected bool Work(int t0, float limit)
	{
		if (m_File < 0)
		{
			foreach (DZS_SaveFile f0 : m_Files)
			{
				int slash = Math.Max(f0.m_Rel.LastIndexOf("/"), f0.m_Rel.LastIndexOf("\\"));
				if (slash > 0)
					DZS_Files.MakeDirs(DZS_LocalStore.Path(m_Folder, f0.m_Rel.Substring(0, slash)));
			}
			m_File = 0;
			m_Chunk = 0;
		}
		while (m_File < m_Files.Count())
		{
			DZS_SaveFile f = m_Files[m_File];
			if (f.m_Delete)
			{
				m_File++;
				continue;
			}
			string p = DZS_LocalStore.Path(m_Folder, f.m_Rel) + ".new";
			if (!m_Fh)
			{
				if (m_FailWrite != "" && f.m_Rel == m_FailWrite)
				{
					m_Fail = Failed(f);
					return true;
				}
				if (m_Reading)
					m_Fh = OpenFile(p, FileMode.READ);
				else
					m_Fh = OpenFile(p, FileMode.WRITE);
				if (!m_Fh)
				{
					m_Fail = Failed(f);
					return true;
				}
				m_Chunk = 0;
				m_Back = new array<string>();
			}
			if (!m_Reading)
			{
				while (m_Chunk < f.m_Chunks.Count())
				{
					FPrint(m_Fh, f.m_Chunks[m_Chunk]);
					m_Chunk++;
					if ((m_Chunk & 31) == 0 && TickCount(0) - t0 >= limit)
						return false;
				}
				CloseFile(m_Fh);
				m_Fh = null;
				m_Reading = true;
				continue;
			}
			while (true)
			{
				string part;
				int n = ReadFile(m_Fh, part, DZS_Files.BLOCK);
				if (n <= 0)
					break;
				m_Back.Insert(part);
				if ((m_Back.Count() & 31) == 0 && TickCount(0) - t0 >= limit)
					return false;
			}
			CloseFile(m_Fh);
			m_Fh = null;
			m_Reading = false;
			if (!DZS_Files.SameContent(m_Back, f.m_Chunks))
			{
				m_Fail = Failed(f);
				return true;
			}
			m_Back = null;
			m_File++;
			if (TickCount(0) - t0 >= limit)
				return false;
		}
		while (m_SumFile < m_Files.Count())
		{
			DZS_SaveFile sf = m_Files[m_SumFile];
			if (sf.m_Delete || sf.m_Sum != "")
			{
				m_SumFile++;
				continue;
			}
			if (!m_Sum)
				m_Sum = new DZS_FileSum();
			int leftMs = (limit - (TickCount(0) - t0)) / 10000;
			if (leftMs < 1)
				return false;
			if (!m_Sum.Feed(sf.m_Chunks, leftMs))
				return false;
			sf.m_Sum = m_Sum.Result();
			m_Sum = null;
			m_SumFile++;
		}
		m_Fail = DZS_LocalStore.ReplaceAll(m_Folder, m_Files);
		return true;
	}

	void Abort()
	{
		if (m_Fh)
			CloseFile(m_Fh);
		m_Fh = null;
		DZS_LocalStore.Cleanup(m_Folder, m_Files);
	}
}

// Files changed outside the editor since they were read or written, checked in slices.
class DZS_OutsideCheck : Managed
{
	protected string m_Folder;
	protected ref array<ref DZS_SaveFile> m_Files;
	protected DZS_OpenProject m_Open;
	protected int m_File;
	protected FileHandle m_Fh;
	protected ref array<string> m_Cur;
	protected ref DZS_FileSum m_Sum;
	ref array<string> m_Changed = new array<string>();
	int m_Frames;

	void DZS_OutsideCheck(string folder, array<ref DZS_SaveFile> files, DZS_OpenProject open)
	{
		m_Folder = folder;
		m_Files = files;
		m_Open = open;
	}

	bool Step(float ms)
	{
		int t0 = TickCount(0);
		float limit = ms * 10000;
		m_Frames++;
		while (m_File < m_Files.Count())
		{
			DZS_SaveFile f = m_Files[m_File];
			if (f.m_Delete)
			{
				m_File++;
				continue;
			}
			if (!m_Cur)
			{
				m_Fh = OpenFile(DZS_LocalStore.Path(m_Folder, f.m_Rel), FileMode.READ);
				if (!m_Fh)
				{
					m_File++;
					continue;
				}
				m_Cur = new array<string>();
			}
			if (m_Fh)
			{
				while (true)
				{
					string part;
					int n = ReadFile(m_Fh, part, DZS_Files.BLOCK);
					if (n <= 0)
						break;
					m_Cur.Insert(part);
					if ((m_Cur.Count() & 31) == 0 && TickCount(0) - t0 >= limit)
						return false;
				}
				CloseFile(m_Fh);
				m_Fh = null;
				m_Sum = new DZS_FileSum();
			}
			int leftMs = (limit - (TickCount(0) - t0)) / 10000;
			if (leftMs < 1)
				return false;
			if (!m_Sum.Feed(m_Cur, leftMs))
				return false;
			if (m_Sum.Result() != m_Open.Remembered(f.m_Rel))
				m_Changed.Insert(f.m_Rel);
			m_Cur = null;
			m_Sum = null;
			m_File++;
		}
		return true;
	}

	void Abort()
	{
		if (m_Fh)
			CloseFile(m_Fh);
		m_Fh = null;
	}
}

// Extensions of the project file: data of other applications or plugins under "extensions" — per id "vendor.name" an
// entry {"version": n, "data": …} and optionally "required": true (a consumer that does not know the extension, or that
// version, must not apply the project). A plugin registers a DZS_ProjectExtension for its id; opening a project hands
// it its entry, saving writes back what it holds. Entries no registered extension reads are carried over untouched.
// Contract: docs/extensions/README.md.
class DZS_ProjectExtension : Managed
{
	// "vendor.name"
	string Id() { return ""; }
	// the version this studio writes
	int Version() { return 1; }
	// versions it reads; an entry of another version is carried over untouched
	bool Reads(int version) { return version == Version(); }
	// written as "required": true
	bool Required() { return false; }
	// whether its user interface applies to the open project (the dzbot rules: only in a dzbot project mod)
	bool IsActive() { return false; }
	// the entry's data of the opened project; null when the project has none
	void Load(DZS_Json data, int version) {}
	// no project open any more
	void Reset() {}
	// the data to save; null leaves the entry out
	DZS_Json Data() { return null; }
	// a short mark for a group of the open project in the project bar ("" = none)
	string GroupMark(DZS_Instance inst, string groupId) { return ""; }
	// entries for the context menu of a group; their m_Tag comes back in GroupMenuChosen
	void GroupMenu(DZS_Instance inst, string groupId, array<ref DZS_MenuItem> items) {}
	void GroupMenuChosen(DZS_Instance inst, string groupId, string tag) {}
}

class DZS_ProjectExtensions
{
	protected static ref array<ref DZS_ProjectExtension> s_All;
	// the "extensions" object of the open project as it was read; entries nobody reads are written back from here
	protected static ref DZS_Json s_Loaded;
	// ids whose entry a registered extension took
	protected static ref array<string> s_Taken;
	// required entries this studio cannot read ("id" or "id v2")
	protected static ref array<string> s_Blocking;

	static void Add(DZS_ProjectExtension e)
	{
		if (!s_All)
			s_All = new array<ref DZS_ProjectExtension>();
		s_All.Insert(e);
	}

	static array<ref DZS_ProjectExtension> All()
	{
		if (!s_All)
			s_All = new array<ref DZS_ProjectExtension>();
		return s_All;
	}

	static DZS_ProjectExtension Find(string id)
	{
		foreach (DZS_ProjectExtension e : All())
		{
			if (e.Id() == id)
				return e;
		}
		return null;
	}

	static void Reset()
	{
		s_Loaded = null;
		s_Taken = new array<string>();
		s_Blocking = new array<string>();
		foreach (DZS_ProjectExtension e : All())
			e.Reset();
	}

	// Opening a project file (its root object); 'issues' gets a warning per required entry this studio cannot read.
	static void Load(DZS_Json root, DZS_Issues issues)
	{
		Reset();
		if (!root || !root.IsObj())
			return;
		DZS_Json list = root.Get("extensions");
		if (!list || !list.IsObj())
			return;
		s_Loaded = list.Clone();
		for (int i = 0; i < list.m_Keys.Count(); i++)
		{
			string id = list.m_Keys[i];
			DZS_Json entry = list.Get(id);
			int version = 0;
			bool required = false;
			DZS_Json data = null;
			if (entry && entry.IsObj())
			{
				version = entry.GetInt("version");
				required = entry.GetBool("required");
				data = entry.Get("data");
			}
			DZS_ProjectExtension e = Find(id);
			if (e && e.Reads(version))
			{
				DZS_Json own = null;
				if (data)
					own = data.Clone();
				e.Load(own, version);
				s_Taken.Insert(id);
				continue;
			}
			if (!required)
				continue;
			string named = id + " v" + version;
			s_Blocking.Insert(named);
			if (issues)
				issues.Add(DZS_Issue.WARNING, "Die Projektdatei braucht die Erweiterung " + named + ", die dieser Editor nicht kennt — sie bleibt unverändert, Anwenden ist gesperrt", "The project file needs the extension " + named + ", which this editor does not know — it stays as it is, applying is blocked");
		}
	}

	// Why the open project must not be applied here: a required extension this studio cannot read (false = none).
	static bool Blocked(out string de, out string en)
	{
		de = "";
		en = "";
		if (!s_Blocking || s_Blocking.Count() == 0)
			return false;
		string list = "";
		foreach (string b : s_Blocking)
		{
			if (list != "")
				list += ", ";
			list += b;
		}
		de = "Braucht die Erweiterung " + list;
		en = "Needs the extension " + list;
		return true;
	}

	// The "extensions" object to save, in the order it was read, new entries after it; null when there is none.
	static DZS_Json Save()
	{
		DZS_Json result = DZS_Json.Obj();
		if (s_Loaded)
		{
			for (int i = 0; i < s_Loaded.m_Keys.Count(); i++)
			{
				string id = s_Loaded.m_Keys[i];
				DZS_Json was = s_Loaded.Get(id);
				if (!s_Taken || s_Taken.Find(id) < 0)
				{
					DZS_Json kept = was.Clone();
					result.Set(id, kept);
					continue;
				}
				DZS_Json entry = Entry(Find(id), was);
				if (entry)
					result.Set(id, entry);
			}
		}
		foreach (DZS_ProjectExtension e : All())
		{
			if (result.Has(e.Id()) || (s_Taken && s_Taken.Find(e.Id()) >= 0))
				continue;
			DZS_Json fresh = Entry(e, null);
			if (fresh)
				result.Set(e.Id(), fresh);
		}
		if (result.Count() == 0)
			return null;
		return result;
	}

	// An extension's entry: the members of the entry as read (other than version, required and data) stay.
	protected static DZS_Json Entry(DZS_ProjectExtension e, DZS_Json was)
	{
		if (!e)
			return null;
		DZS_Json data = e.Data();
		if (!data)
			return null;
		DZS_Json entry = DZS_Json.Obj();
		DZS_Json vj = DZS_Json.Int(e.Version());
		entry.Set("version", vj);
		if (e.Required())
		{
			DZS_Json rq = DZS_Json.Boolean(true);
			entry.Set("required", rq);
		}
		entry.Set("data", data);
		if (was && was.IsObj())
		{
			for (int i = 0; i < was.m_Keys.Count(); i++)
			{
				string k = was.m_Keys[i];
				if (k == "version" || k == "required" || k == "data")
					continue;
				DZS_Json other = was.Get(k);
				DZS_Json copy = other.Clone();
				entry.Set(k, copy);
			}
		}
		return entry;
	}

	// The project's data changed outside the history (an extension's editor): one unsaved change until the next save.
	static void Changed(DZS_Instance inst)
	{
		if (!inst)
			return;
		string mode = inst.GetSaveMode();
		if (mode == "" || mode == "opened")
			inst.SetSaveMode("extension");
	}

	static string GroupMarks(DZS_Instance inst, string groupId)
	{
		string marks = "";
		foreach (DZS_ProjectExtension e : All())
		{
			if (!e.IsActive())
				continue;
			string m = e.GroupMark(inst, groupId);
			if (m == "")
				continue;
			if (marks != "")
				marks += " ";
			marks += m;
		}
		return marks;
	}

	static void GroupMenu(DZS_Instance inst, string groupId, array<ref DZS_MenuItem> items)
	{
		foreach (DZS_ProjectExtension e : All())
		{
			if (!e.IsActive())
				continue;
			array<ref DZS_MenuItem> own = new array<ref DZS_MenuItem>();
			e.GroupMenu(inst, groupId, own);
			foreach (DZS_MenuItem it : own)
			{
				it.m_Tag = "ext:" + e.Id() + ":" + it.m_Tag;
				items.Insert(it);
			}
		}
	}

	// true when the tag was an extension's entry
	static bool GroupMenuChosen(DZS_Instance inst, string groupId, string tag)
	{
		if (tag.IndexOf("ext:") != 0)
			return false;
		string rest = tag.Substring(4, tag.Length() - 4);
		int colon = rest.IndexOf(":");
		if (colon < 0)
			return true;
		DZS_ProjectExtension e = Find(rest.Substring(0, colon));
		if (e)
			e.GroupMenuChosen(inst, groupId, rest.Substring(colon + 1, rest.Length() - colon - 1));
		return true;
	}
}

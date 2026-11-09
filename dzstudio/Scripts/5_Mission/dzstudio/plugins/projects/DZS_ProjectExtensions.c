// Extensions of the project file: data of other applications or plugins under "extensions". An extension is the URL
// of its spec file (the version is part of the URL); its entry is {"data": …} and optionally "required": true (a
// consumer that does not know the URL must not apply the project). A plugin registers a DZS_ProjectExtension for its
// URL; opening a project hands it its entry, saving writes back what it holds. Every other URL gets a generic handler
// (DZS_ExtGeneric) that edits the fields its spec file offers and otherwise writes the entry back untouched. Blocks,
// changes and list elements carry their own "extensions" (DZS_EntityExts). Contract: docs/extensions/README.md.
class DZS_ProjectExtension : Managed
{
	// the URL of its spec, e.g. "https://developers.dzbot.de/specs/dzbot/rules/1.json"
	string Url() { return ""; }
	// written as "required": true
	bool Required() { return false; }
	// whether its user interface applies to the open project (the dzbot rules: only in a dzbot project mod)
	bool IsActive() { return false; }
	// the entry's data of the opened project; not called when the project has no entry
	void Load(DZS_Json data) {}
	// no project open any more
	void Reset() {}
	// the opened project's scene is shown (entities' extensions are bound to it)
	void Opened(DZS_Instance inst) {}
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
	// URLs whose entry a registered extension took
	protected static ref array<string> s_Taken;
	// URLs of required entries this studio cannot read
	protected static ref array<string> s_Blocking;
	// a generic handler per URL of the open project that no plugin reads (DZS_ExtGeneric)
	protected static ref array<ref DZS_ExtGeneric> s_Generic;
	// URLs removed from the open project since it was read
	protected static ref array<string> s_Removed;

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

	static DZS_ProjectExtension Find(string url)
	{
		foreach (DZS_ProjectExtension e : All())
		{
			if (e.Url() == url)
				return e;
		}
		return null;
	}

	// A generic handler of the open project for a URL no plugin reads, null when there is none.
	static DZS_ExtGeneric Generic(string url)
	{
		if (!s_Generic)
			return null;
		foreach (DZS_ExtGeneric g : s_Generic)
		{
			if (g.Url() == url)
				return g;
		}
		return null;
	}

	// The handler of a URL: a plugin's, else the generic one of the open project.
	static DZS_ProjectExtension Handler(string url)
	{
		DZS_ProjectExtension e = Find(url);
		if (e)
			return e;
		return Generic(url);
	}

	// Plugins, then the generic handlers of the open project.
	static array<DZS_ProjectExtension> Handlers()
	{
		array<DZS_ProjectExtension> r = new array<DZS_ProjectExtension>();
		foreach (DZS_ProjectExtension e : All())
			r.Insert(e);
		if (s_Generic)
		{
			foreach (DZS_ExtGeneric g : s_Generic)
				r.Insert(g);
		}
		return r;
	}

	static void Reset()
	{
		s_Loaded = null;
		s_Taken = new array<string>();
		s_Blocking = new array<string>();
		s_Generic = new array<ref DZS_ExtGeneric>();
		s_Removed = new array<string>();
		foreach (DZS_ProjectExtension e : All())
			e.Reset();
		DZS_EntityExts.Reset();
	}

	// The URLs the open project uses, in file order: its own entries, then those only entities hold, then added ones.
	static array<string> Urls()
	{
		array<string> r = new array<string>();
		if (s_Loaded)
		{
			foreach (string lu : s_Loaded.m_Keys)
			{
				if (lu != DZS_ProjectFile.EDITOR_EXT && !IsRemoved(lu))
					r.Insert(lu);
			}
		}
		array<string> ents = DZS_EntityExts.Urls();
		foreach (string eu : ents)
		{
			if (r.Find(eu) < 0 && eu != DZS_ProjectFile.EDITOR_EXT)
				r.Insert(eu);
		}
		if (s_Generic)
		{
			foreach (DZS_ExtGeneric g : s_Generic)
			{
				if (r.Find(g.Url()) < 0)
					r.Insert(g.Url());
			}
		}
		foreach (DZS_ProjectExtension e : All())
		{
			if (r.Find(e.Url()) < 0 && !IsRemoved(e.Url()) && e.Data() != null)
				r.Insert(e.Url());
		}
		return r;
	}

	protected static bool IsRemoved(string url)
	{
		return s_Removed && s_Removed.Find(url) >= 0;
	}

	// Whether the open project's own entry for the URL is required.
	static bool IsRequired(string url)
	{
		DZS_ProjectExtension h = Handler(url);
		if (h)
			return h.Required();
		if (!s_Loaded)
			return false;
		DZS_Json entry = s_Loaded.Get(url);
		return entry && entry.IsObj() && entry.GetBool("required");
	}

	// Adds an extension to the open project: {"data": {}}, "required" as its spec says. A URL a plugin reads only
	// comes back from the removed ones; its entry is written once it holds something.
	static void AddUrl(DZS_Instance inst, string url, bool required)
	{
		Ensure();
		s_Removed.RemoveItem(url);
		if (url == DZS_ProjectFile.EDITOR_EXT || Find(url) || Generic(url))
		{
			Changed(inst);
			return;
		}
		DZS_Json empty = DZS_Json.Obj();
		DZS_ExtGeneric g = new DZS_ExtGeneric(url, required, empty);
		g.MarkChanged();
		s_Generic.Insert(g);
		if (required && s_Blocking.Find(url) < 0)
			s_Blocking.Insert(url);
		DZS_ExtSpecs.Want(url);
		Changed(inst);
	}

	// Removes an extension from the open project: its own entry and its values on every entity.
	static void RemoveUrl(DZS_Instance inst, string url)
	{
		Ensure();
		if (url == DZS_ProjectFile.EDITOR_EXT)
			return;
		if (s_Removed.Find(url) < 0)
			s_Removed.Insert(url);
		s_Taken.RemoveItem(url);
		s_Blocking.RemoveItem(url);
		DZS_ProjectExtension e = Find(url);
		if (e)
			e.Reset();
		DZS_ExtGeneric g = Generic(url);
		if (g)
			s_Generic.RemoveItem(g);
		DZS_EntityExts.RemoveUrl(url);
		Changed(inst);
	}

	protected static void Ensure()
	{
		if (!s_Taken)
			Reset();
	}

	// Opening a project file (its root object); 'issues' gets a warning per required entry this studio cannot read.
	static void Load(DZS_Json root, DZS_Issues issues)
	{
		Reset();
		if (!root || !root.IsObj())
			return;
		DZS_EntityExts.Load(root);
		array<string> ents = DZS_EntityExts.Urls();
		foreach (string eu : ents)
			DZS_ExtSpecs.Want(eu);
		DZS_Json list = root.Get("extensions");
		if (!list || !list.IsObj())
			return;
		s_Loaded = list.Clone();
		for (int i = 0; i < list.m_Keys.Count(); i++)
		{
			string url = list.m_Keys[i];
			DZS_Json entry = list.Get(url);
			bool required = false;
			DZS_Json data = null;
			if (entry && entry.IsObj())
			{
				required = entry.GetBool("required");
				data = entry.Get("data");
			}
			// dzStudio's own part, read and written by the project file itself (DZS_ProjectFile.EditorOf/SetEditor)
			if (url == DZS_ProjectFile.EDITOR_EXT)
				continue;
			DZS_Json own = null;
			if (data)
				own = data.Clone();
			DZS_ProjectExtension e = Find(url);
			DZS_ExtSpecs.Want(url);
			if (e)
			{
				e.Load(own);
				s_Taken.Insert(url);
				continue;
			}
			DZS_ExtGeneric g = new DZS_ExtGeneric(url, required, own);
			s_Generic.Insert(g);
			if (!required)
				continue;
			s_Blocking.Insert(url);
			if (issues)
				issues.Add(DZS_Issue.WARNING, "Die Projektdatei braucht die Erweiterung " + url + ", die dieser Editor nicht kennt — sie bleibt unverändert, Anwenden ist gesperrt", "The project file needs the extension " + url + ", which this editor does not know — it stays as it is, applying is blocked");
		}
	}

	// The URLs of the required entries of a project file's "extensions" this studio cannot read — for a file that is
	// not the open project (applying from the command line).
	static array<string> UnreadableIn(DZS_Json root)
	{
		array<string> names = new array<string>();
		if (!root || !root.IsObj())
			return names;
		DZS_Json list = root.Get("extensions");
		if (!list || !list.IsObj())
			return names;
		for (int i = 0; i < list.m_Keys.Count(); i++)
		{
			string url = list.m_Keys[i];
			DZS_Json entry = list.Get(url);
			if (!entry || !entry.IsObj() || !entry.GetBool("required"))
				continue;
			if (url == DZS_ProjectFile.EDITOR_EXT || Find(url) != null)
				continue;
			names.Insert(url);
		}
		return names;
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
				string url = s_Loaded.m_Keys[i];
				DZS_Json was = s_Loaded.Get(url);
				if (IsRemoved(url))
					continue;
				DZS_ExtGeneric lg = Generic(url);
				bool taken = s_Taken && s_Taken.Find(url) >= 0;
				if (!taken && (!lg || !lg.IsChanged()))
				{
					DZS_Json kept = was.Clone();
					result.Set(url, kept);
					continue;
				}
				DZS_ProjectExtension h = lg;
				if (taken)
					h = Find(url);
				DZS_Json entry = Entry(h, was);
				if (entry)
					result.Set(url, entry);
			}
		}
		foreach (DZS_ProjectExtension e : All())
		{
			if (result.Has(e.Url()) || (s_Taken && s_Taken.Find(e.Url()) >= 0))
				continue;
			DZS_Json fresh = Entry(e, null);
			if (fresh)
				result.Set(e.Url(), fresh);
		}
		if (s_Generic)
		{
			foreach (DZS_ExtGeneric ng : s_Generic)
			{
				if (result.Has(ng.Url()) || (s_Loaded && s_Loaded.Has(ng.Url())))
					continue;
				DZS_Json added = Entry(ng, null);
				if (added)
					result.Set(ng.Url(), added);
			}
		}
		if (result.Count() == 0)
			return null;
		return result;
	}

	// An extension's entry: the members of the entry as read (other than required and data) stay.
	protected static DZS_Json Entry(DZS_ProjectExtension e, DZS_Json was)
	{
		if (!e)
			return null;
		DZS_Json data = e.Data();
		if (!data)
			return null;
		DZS_Json entry = DZS_Json.Obj();
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
				if (k == "required" || k == "data")
					continue;
				DZS_Json other = was.Get(k);
				DZS_Json copy = other.Clone();
				entry.Set(k, copy);
			}
		}
		return entry;
	}

	// The opened project's scene is shown: entities take their extensions, extensions may look at the scene.
	static void Opened(DZS_Instance inst)
	{
		DZS_EntityExts.Resolve(inst);
		array<DZS_ProjectExtension> handlers = Handlers();
		foreach (DZS_ProjectExtension e : handlers)
			e.Opened(inst);
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
		array<DZS_ProjectExtension> handlers = Handlers();
		foreach (DZS_ProjectExtension e : handlers)
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
		// tagged with the extension's place among the handlers: a URL holds ":" itself
		array<DZS_ProjectExtension> all = Handlers();
		for (int n = 0; n < all.Count(); n++)
		{
			DZS_ProjectExtension e = all[n];
			if (!e.IsActive())
				continue;
			array<ref DZS_MenuItem> own = new array<ref DZS_MenuItem>();
			e.GroupMenu(inst, groupId, own);
			foreach (DZS_MenuItem it : own)
			{
				it.m_Tag = "ext:" + n + ":" + it.m_Tag;
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
		int at = rest.Substring(0, colon).ToInt();
		array<DZS_ProjectExtension> all = Handlers();
		if (at < 0 || at >= all.Count())
			return true;
		DZS_ProjectExtension e = all[at];
		if (e)
			e.GroupMenuChosen(inst, groupId, rest.Substring(colon + 1, rest.Length() - colon - 1));
		return true;
	}
}

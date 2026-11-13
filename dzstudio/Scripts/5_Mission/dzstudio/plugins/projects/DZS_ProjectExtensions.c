// Extensions of the project file: data of other applications or plugins under "extensions". An extension is the URL
// of its spec file (the version is part of the URL); its entry is {"data": …} and optionally "required": true (a
// consumer that does not know the URL must not apply the project). A plugin registers a DZS_ProjectExtension for its
// URL; opening a project hands it its entry, saving writes back what it holds. Every other URL gets a generic handler
// (DZS_ExtGeneric) that edits the fields its spec file offers and otherwise writes the entry back untouched. Blocks,
// changes and list elements carry their own "extensions", kept on their elements in the model (DZS_EntityExts). Which
// extensions the project carries is in the model too (List), so adding and removing one is a step of the history.
// Contract: docs/extensions/README.md.
class DZS_ProjectExtension : Managed
{
	// the URL of its spec, e.g. "https://example.com/specs/rules/1.json"
	string Url() { return ""; }
	// written as "required": true
	bool Required() { return false; }
	// whether its user interface applies to the open project (e.g. only in a provider's own projects)
	bool IsActive() { return false; }
	// the entry's data of the opened project; not called when the project has no entry
	void Load(DZS_Json data) {}
	// no project open any more
	void Reset() {}
	// the opened project's model is complete (entities carry their extensions)
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
	// the "extensions" object of the open project as it was read
	protected static ref DZS_Json s_Loaded;
	// a generic handler per URL of the project's list that no plugin reads (DZS_ExtGeneric), kept in step with the
	// list in the model (undo and redo add and remove extensions)
	protected static ref array<ref DZS_ExtGeneric> s_Generic;
	protected static DZS_Instance s_SyncInst;
	protected static int s_SyncGen = -1;
	// between reading a file and its model being complete: the generic handlers hold the file's data
	protected static bool s_Pending;

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

	protected static DZS_Instance Shown()
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return null;
		return k.GetShown();
	}

	// A generic handler of the open project for a URL no plugin reads, null when there is none.
	static DZS_ExtGeneric Generic(string url)
	{
		SyncGeneric();
		return OldGeneric(url);
	}

	protected static DZS_ExtGeneric OldGeneric(string url)
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

	// One generic handler per URL of the list without plugin, those of URLs still listed kept.
	protected static void SyncGeneric()
	{
		if (s_Pending)
			return;
		DZS_Instance inst = Shown();
		int gen = -1;
		if (inst)
			gen = inst.GetGen();
		if (s_Generic && s_SyncInst == inst && s_SyncGen == gen)
			return;
		s_SyncInst = inst;
		s_SyncGen = gen;
		DZS_Json list = List(inst);
		array<ref DZS_ExtGeneric> fresh = new array<ref DZS_ExtGeneric>();
		foreach (string url : list.m_Keys)
		{
			if (url == DZS_ProjectFile.EDITOR_EXT || Find(url))
				continue;
			DZS_ExtGeneric g = OldGeneric(url);
			if (!g)
				g = new DZS_ExtGeneric(url, null);
			fresh.Insert(g);
		}
		s_Generic = fresh;
	}

	// Plugins, then the generic handlers of the open project.
	static array<DZS_ProjectExtension> Handlers()
	{
		SyncGeneric();
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
		s_Generic = new array<ref DZS_ExtGeneric>();
		s_SyncInst = null;
		s_SyncGen = -1;
		s_Pending = false;
		foreach (DZS_ProjectExtension e : All())
			e.Reset();
		DZS_EntityExts.Reset();
	}

	// ---- the list of extensions the project carries ----

	// The project's extensions, {"<url>": <its entry without "data">} in file order ("data": null marks where the
	// data stood): the model's (DZS_EntityExts.ProjectList, a step of the history like everything else), else the
	// opened file's. A copy.
	static DZS_Json List(DZS_Instance inst)
	{
		DZS_Json own = DZS_EntityExts.ProjectList(inst);
		if (own)
			return own;
		return LoadedList();
	}

	protected static DZS_Json LoadedList()
	{
		DZS_Json r = DZS_Json.Obj();
		if (!s_Loaded)
			return r;
		foreach (string url : s_Loaded.m_Keys)
		{
			if (url == DZS_ProjectFile.EDITOR_EXT)
				continue;
			DZS_Json meta = MetaOf(s_Loaded.Get(url));
			r.Set(url, meta);
		}
		return r;
	}

	// An entry without its data (an entry that is no object stays as it is).
	protected static DZS_Json MetaOf(DZS_Json entry)
	{
		if (!entry)
			return DZS_Json.Obj();
		if (!entry.IsObj())
			return entry.Clone();
		DZS_Json meta = DZS_Json.Obj();
		foreach (string k : entry.m_Keys)
		{
			if (k == "data")
			{
				DZS_Json mark = DZS_Json.Null();
				meta.Set(k, mark);
				continue;
			}
			DZS_Json one = entry.Get(k);
			DZS_Json copy = one.Clone();
			meta.Set(k, copy);
		}
		return meta;
	}

	// The open project carries the URL (staged as part of the edit being made, see DZS_EntityExts.SetProjectData).
	static void Carry(DZS_Instance inst, string url)
	{
		if (!inst || url == DZS_ProjectFile.EDITOR_EXT)
			return;
		DZS_Json list = List(inst);
		if (list.Has(url))
			return;
		DZS_Json meta = DZS_Json.Obj();
		list.Set(url, meta);
		DZS_EntityExts.SetProjectList(inst, list);
	}

	// The URLs the open project uses, in file order: its own entries, then those only entities hold, then plugins
	// holding data outside the list. entities false: of the entities only the opened file's not taken yet (no index,
	// so nothing is built: whether an extension may still be added).
	static array<string> Urls(bool entities = true)
	{
		array<string> r = new array<string>();
		DZS_Instance inst = Shown();
		DZS_Json list = List(inst);
		foreach (string lu : list.m_Keys)
		{
			if (lu != DZS_ProjectFile.EDITOR_EXT)
				r.Insert(lu);
		}
		array<string> ents;
		if (entities)
			ents = DZS_EntityExts.Urls();
		else
			ents = DZS_EntityExts.FileUrls();
		foreach (string eu : ents)
		{
			if (r.Find(eu) < 0 && eu != DZS_ProjectFile.EDITOR_EXT)
				r.Insert(eu);
		}
		foreach (DZS_ProjectExtension e : All())
		{
			if (r.Find(e.Url()) < 0 && e.Data() != null)
				r.Insert(e.Url());
		}
		return r;
	}

	// Whether the open project's own entry for the URL is required.
	static bool IsRequired(string url)
	{
		DZS_ProjectExtension e = Find(url);
		if (e)
			return e.Required();
		DZS_Json list = List(Shown());
		DZS_Json meta = list.Get(url);
		return meta && meta.IsObj() && meta.GetBool("required");
	}

	// Adds an extension to the open project as one step of the history: listed, "required" as its spec says, a URL no
	// plugin reads with {"data": {}}; a plugin's entry is written once it holds something.
	static void AddUrl(DZS_Instance inst, string url, bool required)
	{
		if (!inst || url == DZS_ProjectFile.EDITOR_EXT)
			return;
		bool plugin = Find(url) != null;
		DZS_Json list = List(inst);
		if (!list.Has(url))
		{
			DZS_Json meta = DZS_Json.Obj();
			if (required && !plugin)
			{
				DZS_Json rq = DZS_Json.Boolean(true);
				meta.Set("required", rq);
			}
			list.Set(url, meta);
			DZS_EntityExts.SetProjectList(inst, list);
		}
		if (!plugin && !DZS_EntityExts.HasProjectData(inst, url))
		{
			DZS_Json empty = DZS_Json.Obj();
			DZS_EntityExts.SetProjectData(inst, url, empty);
		}
		DZS_EntityExts.Commit(inst, "Erweiterung hinzufügen", "Add extension");
		DZS_ExtSpecs.Want(url);
	}

	// Removes an extension from the open project: its entry in the list, its data and its values on every entity, as
	// one step of the history (undo brings all of it back).
	static void RemoveUrl(DZS_Instance inst, string url)
	{
		if (!inst || url == DZS_ProjectFile.EDITOR_EXT)
			return;
		DZS_EntityExts.RemoveUrl(inst, url);
		// a plugin that keeps project data outside the model forgets it (not undoable)
		DZS_ProjectExtension e = Find(url);
		if (e && !DZS_EntityExts.HasProjectData(inst, url))
			e.Reset();
	}

	// Opening a project file (its root object); 'issues' gets a warning per required entry this studio cannot read.
	static void Load(DZS_Json root, DZS_Issues issues)
	{
		Reset();
		if (!root || !root.IsObj())
			return;
		s_Pending = true;
		DZS_EntityExts.Load(root);
		array<string> ents = DZS_EntityExts.FileUrls();
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
				continue;
			}
			DZS_ExtGeneric g = new DZS_ExtGeneric(url, own);
			s_Generic.Insert(g);
			if (required && issues)
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
		DZS_Json list = List(Shown());
		string names = "";
		foreach (string url : list.m_Keys)
		{
			DZS_Json meta = list.Get(url);
			if (url == DZS_ProjectFile.EDITOR_EXT || Find(url) || !meta.IsObj() || !meta.GetBool("required"))
				continue;
			if (names != "")
				names += ", ";
			names += url;
		}
		if (names == "")
			return false;
		de = "Braucht die Erweiterung " + names;
		en = "Needs the extension " + names;
		return true;
	}

	// The "extensions" object to save, in the order of the project's list, then entries of plugins that hold data
	// outside it; null when there is none.
	static DZS_Json Save()
	{
		DZS_Instance inst = Shown();
		DZS_Json result = DZS_Json.Obj();
		DZS_Json list = List(inst);
		foreach (string url : list.m_Keys)
		{
			if (url == DZS_ProjectFile.EDITOR_EXT)
				continue;
			DZS_Json entry = Entry(inst, url, list.Get(url));
			if (entry)
				result.Set(url, entry);
		}
		foreach (DZS_ProjectExtension e : All())
		{
			if (result.Has(e.Url()) || list.Has(e.Url()))
				continue;
			DZS_Json data = e.Data();
			if (!data)
				continue;
			DZS_Json fresh = DZS_Json.Obj();
			if (e.Required())
			{
				DZS_Json rq = DZS_Json.Boolean(true);
				fresh.Set("required", rq);
			}
			fresh.Set("data", data);
			result.Set(e.Url(), fresh);
		}
		if (result.Count() == 0)
			return null;
		return result;
	}

	// An extension's entry: its members from the list, its data from its plugin, else from the model, else as read;
	// null when a plugin has nothing to write.
	protected static DZS_Json Entry(DZS_Instance inst, string url, DZS_Json meta)
	{
		if (!meta.IsObj())
			return meta.Clone();
		DZS_ProjectExtension plugin = Find(url);
		DZS_Json data = null;
		if (plugin)
		{
			data = plugin.Data();
			if (!data)
				return null;
		}
		else if (DZS_EntityExts.HasProjectData(inst, url))
			data = DZS_EntityExts.ProjectData(inst, url);
		else if (s_Loaded)
		{
			DZS_Json was = s_Loaded.Get(url);
			if (was && was.IsObj() && was.Has("data"))
			{
				DZS_Json wasData = was.Get("data");
				data = wasData.Clone();
			}
		}
		DZS_Json entry = DZS_Json.Obj();
		if (plugin && plugin.Required())
		{
			DZS_Json rq = DZS_Json.Boolean(true);
			entry.Set("required", rq);
		}
		foreach (string k : meta.m_Keys)
		{
			if (k == "data")
			{
				if (data)
					entry.Set(k, data);
				continue;
			}
			if (plugin && k == "required")
				continue;
			DZS_Json one = meta.Get(k);
			DZS_Json copy = one.Clone();
			entry.Set(k, copy);
		}
		if (data && !entry.Has("data"))
			entry.Set("data", data);
		return entry;
	}

	// The opened project's model is complete: the project's list of extensions goes into it (once: a restored backup
	// or a live session brings its own), entities take their extensions, extensions may look at the scene.
	static void Opened(DZS_Instance inst)
	{
		s_Pending = false;
		if (inst && !DZS_EntityExts.ProjectList(inst))
		{
			DZS_Json loaded = LoadedList();
			if (loaded.Count() > 0)
			{
				DZS_EntityExts.SetProjectList(inst, loaded);
				DZS_EntityExts.Commit(inst, "", "", true);
			}
		}
		DZS_EntityExts.Distribute(inst);
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

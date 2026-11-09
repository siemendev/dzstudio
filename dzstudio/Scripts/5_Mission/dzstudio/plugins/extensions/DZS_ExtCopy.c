// Whether an entity's extension values go along when the entity is copied (docs/extensions/README.md "Copying"): the
// spec's "copy" ("always", "same-project", "never"; default and fallback without a spec "same-project"), overridden per
// field for the value at the field's path. Duplicating, the pipette and pasting in the project the copy came from count
// as the same project; pasting into another project as another.
class DZS_ExtCopy
{
	static const string ALWAYS = "always";
	static const string SAME = "same-project";
	static const string NEVER = "never";

	static bool Valid(string policy)
	{
		return policy == ALWAYS || policy == SAME || policy == NEVER;
	}

	static bool Allows(string policy, bool sameProject)
	{
		if (policy == ALWAYS)
			return true;
		if (policy == NEVER)
			return false;
		return sameProject;
	}

	// The spec's policy from the copy dzStudio has of it; same-project without one.
	static string Policy(string url)
	{
		DZS_ExtSpecs.Want(url);
		DZS_ExtSpec s = DZS_ExtSpecs.Get(url);
		if (!s)
			return SAME;
		return s.m_Copy;
	}

	// One extension's value on a copied entity: what may go along (null: nothing). attach/kind/list name the entity
	// as the spec's fields do ("element", "spawnerGroup", "objects"); "" leaves the fields out of it.
	static DZS_Json Value(string url, DZS_Json value, string attach, string kind, string list, bool sameProject)
	{
		if (!value)
			return null;
		DZS_ExtSpec spec = DZS_ExtSpecs.Get(url);
		bool allowed = Allows(Policy(url), sameProject);
		array<DZS_ExtField> fields = new array<DZS_ExtField>();
		if (spec && attach != "" && value.IsObj())
			fields = spec.FieldsFor(attach, kind, list);
		DZS_Json result;
		if (allowed)
			result = value.Clone();
		else
			result = DZS_Json.Obj();
		foreach (DZS_ExtField f : fields)
		{
			if (f.m_Copy == "")
				continue;
			bool keep = Allows(f.m_Copy, sameProject);
			if (keep == allowed)
				continue;
			if (!keep)
			{
				DZS_ExtGeneric.SetAt(result, f.m_Path, null);
				continue;
			}
			DZS_Json at = DZS_ExtGeneric.At(value, f.m_Path);
			if (!at)
				continue;
			DZS_Json atCopy = at.Clone();
			DZS_ExtGeneric.SetAt(result, f.m_Path, atCopy);
		}
		if (!allowed && result.Count() == 0)
			return null;
		if (result.IsObj() && result.Count() == 0 && value.IsObj() && value.Count() > 0)
			return null;
		return result;
	}

	// An entity's whole extensions object filtered (null: nothing goes along).
	static DZS_Json Exts(DZS_Json exts, string attach, string kind, string list, bool sameProject)
	{
		if (!exts || !exts.IsObj())
			return null;
		DZS_Json r = DZS_Json.Obj();
		for (int i = 0; i < exts.m_Keys.Count(); i++)
		{
			string url = exts.m_Keys[i];
			DZS_Json kept = Value(url, exts.Get(url), attach, kind, list, sameProject);
			if (kept)
				r.Set(url, kept);
		}
		if (r.Count() == 0)
			return null;
		return r;
	}

	// The same for the JSON text of an element's field ("" = nothing).
	static string Text(string text, string attach, string kind, string list, bool sameProject)
	{
		if (text == "")
			return "";
		string err;
		DZS_Json all = DZS_Json.ParseText(text, err);
		if (!all || !all.IsObj())
			return "";
		DZS_Json kept = Exts(all, attach, kind, list, sameProject);
		if (!kept)
			return "";
		return DZS_EntityExts.TextOf(kept);
	}

	// The fields of a copied scene object: its own extensions ("ext", an object of a spawner group) and its loot row's
	// ("ext:lootRow") filtered in place; other "ext:" fields by their spec's policy alone.
	static void ObjectFields(map<string, string> fields, bool sameProject)
	{
		if (!fields)
			return;
		array<string> keys = new array<string>();
		for (int i = 0; i < fields.Count(); i++)
		{
			string k = fields.GetKey(i);
			if (k == DZS_EntityExts.SLOT || k.IndexOf(DZS_EntityExts.SLOT + ":") == 0)
				keys.Insert(k);
		}
		foreach (string key : keys)
		{
			string attach = "";
			string kind = "";
			string list = "";
			if (key == DZS_EntityExts.SLOT)
			{
				attach = "element";
				kind = "spawnerGroup";
				list = "objects";
			}
			else if (key == DZS_EntityExts.SLOT + ":lootRow")
			{
				attach = "block";
				kind = "lootRow";
			}
			string kept = Text(fields.Get(key), attach, kind, list, sameProject);
			if (kept == "")
				fields.Remove(key);
			else
				fields.Set(key, kept);
		}
	}

	// Which project an entity is copied from: the open project (provider and id), else the instance of this session.
	static string ProjectKey(DZS_Instance inst)
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (k && inst && k.GetShown() == inst)
		{
			DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(k.GetPlugin("projects"));
			if (pp && pp.m_Open && pp.m_Open.m_Folder != "")
				return pp.m_Open.m_Provider + ":" + pp.m_Open.m_Folder;
		}
		if (!inst)
			return "";
		return "instance:" + inst.GetLabel();
	}
}

// What dzStudio copied to the clipboard beside the spawner JSON: per object its extension fields, and the project they
// came from. The clipboard keeps only the spawner text (other tools read it); this sidecar lives in dzStudio's memory,
// found again by the SHA-256 of the trimmed text, so pasting that text brings the extensions along while any other text
// (or the same text after a restart) pastes without them.
class DZS_ExtClipEntry : Managed
{
	string m_Hash;
	string m_Project;
	// per object of the text, in its order: field -> JSON text
	ref array<ref map<string, string>> m_Objects = new array<ref map<string, string>>();
}

class DZS_ExtClip
{
	static const int KEEP = 16;
	protected static ref array<ref DZS_ExtClipEntry> s_Entries;

	static string HashOf(string clipText)
	{
		string t = clipText.Trim();
		return DZS_Sha256.OfText(t);
	}

	// A copy was put on the clipboard: the extension fields of its objects (empty maps for objects without any).
	static void Remember(string clipText, string project, array<ref map<string, string>> objects)
	{
		if (!s_Entries)
			s_Entries = new array<ref DZS_ExtClipEntry>();
		string h = HashOf(clipText);
		for (int i = s_Entries.Count() - 1; i >= 0; i--)
		{
			if (s_Entries[i].m_Hash == h)
				s_Entries.RemoveOrdered(i);
		}
		bool any = false;
		foreach (map<string, string> m : objects)
		{
			if (m && m.Count() > 0)
				any = true;
		}
		if (!any)
			return;
		DZS_ExtClipEntry e = new DZS_ExtClipEntry();
		e.m_Hash = h;
		e.m_Project = project;
		foreach (map<string, string> src : objects)
		{
			map<string, string> one = new map<string, string>();
			if (src)
			{
				for (int k = 0; k < src.Count(); k++)
					one.Set(src.GetKey(k), src.GetElement(k));
			}
			e.m_Objects.Insert(one);
		}
		s_Entries.Insert(e);
		while (s_Entries.Count() > KEEP)
			s_Entries.RemoveOrdered(0);
	}

	// The entry of exactly this clipboard text with as many objects, null when dzStudio did not copy it.
	static DZS_ExtClipEntry Find(string clipText, int objectCount)
	{
		if (!s_Entries)
			return null;
		string h = HashOf(clipText);
		for (int i = s_Entries.Count() - 1; i >= 0; i--)
		{
			DZS_ExtClipEntry e = s_Entries[i];
			if (e.m_Hash == h && e.m_Objects.Count() == objectCount)
				return e;
		}
		return null;
	}
}

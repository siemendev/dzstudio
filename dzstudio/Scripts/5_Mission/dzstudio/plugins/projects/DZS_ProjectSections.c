// A plugin's own part of the project file: project.plugins.<name>, written from the state at every save. The plugin
// reads it back from the projects:plugins companion when the project opens. Its blocks and changes are also the
// project's own (DZS_ProjectV2.Build); their extensions are written only there, the part keeps none.
class DZS_ProjectSection : Managed
{
	string Name() { return ""; }

	// The fragment {blocks, changes} with the entities' extensions, null when the project has nothing of this plugin.
	DZS_Json Fragment(DZS_Instance inst) { return null; }

	// The section as JSON text, "" when the project has nothing of this plugin.
	string Text(DZS_Instance inst)
	{
		DZS_Json frag = Fragment(inst);
		if (!frag)
			return "";
		DZS_Json plain = DZS_PatchProject.WithoutExtensions(frag);
		array<string> chunks = plain.ToChunks();
		return DZS_SpawnerFormat.Join(chunks);
	}
}

class DZS_ProjectSections
{
	protected static ref array<ref DZS_ProjectSection> s_All;

	static void Add(DZS_ProjectSection s)
	{
		if (!s_All)
			s_All = new array<ref DZS_ProjectSection>();
		s_All.Insert(s);
	}

	static array<ref DZS_ProjectSection> All()
	{
		if (!s_All)
			s_All = new array<ref DZS_ProjectSection>();
		return s_All;
	}

	// The companion's entries ([name, key text, value text] each) with every registered section's current text.
	static string Merge(DZS_Instance inst, string entries)
	{
		string err;
		DZS_Json list = DZS_Json.ParseText(entries, err);
		if (!list || !list.IsArr())
			list = DZS_Json.Arr();
		// what the open carried over is taken from the file's blocks anew at every open, never from this part
		DZS_Json rest = DZS_Json.Arr();
		for (int r = 0; r < list.Count(); r++)
		{
			DZS_Json re = list.At(r);
			if (re.At(0).m_Str != DZS_KeptBlocks.NAME)
				rest.Add(re);
		}
		list = rest;
		array<ref DZS_ProjectSection> sections = All();
		foreach (DZS_ProjectSection s : sections)
		{
			string name = s.Name();
			DZS_Json kept = DZS_Json.Arr();
			for (int i = 0; i < list.Count(); i++)
			{
				DZS_Json e = list.At(i);
				if (e.At(0).m_Str != name)
					kept.Add(e);
			}
			list = kept;
			string text = s.Text(inst);
			if (text == "")
				continue;
			DZS_Json one = DZS_Json.Arr();
			DZS_Json nj = DZS_Json.Str(name);
			one.Add(nj);
			DZS_Json kj = DZS_Json.Str(DZS_TextForm.Str(name));
			one.Add(kj);
			DZS_Json tj = DZS_Json.Str(text);
			one.Add(tj);
			list.Add(one);
		}
		array<string> chunks = list.ToChunks();
		return DZS_SpawnerFormat.Join(chunks);
	}

	// the entries last read by Stored and their texts by section name: the layers and the kept blocks each ask for
	// their part of the same entries when a project opens
	protected static string s_Entries;
	protected static ref map<string, string> s_Texts;

	// The stored text of one section from the companion entries, or "".
	static string Stored(string entries, string name)
	{
		if (!s_Texts || entries != s_Entries)
		{
			s_Entries = entries;
			s_Texts = new map<string, string>();
			string err;
			DZS_Json list = DZS_Json.ParseText(entries, err);
			if (list && list.IsArr())
			{
				for (int i = 0; i < list.Count(); i++)
				{
					DZS_Json e = list.At(i);
					if (!e || !e.IsArr() || e.Count() < 3)
						continue;
					string key = e.At(0).m_Str;
					if (!s_Texts.Contains(key))
						s_Texts.Set(key, e.At(2).m_Str);
				}
			}
		}
		string text;
		s_Texts.Find(name, text);
		return text;
	}
}

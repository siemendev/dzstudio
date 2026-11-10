// A setting (config field) of a dzbot project mod, as the load reply lists it: for the rules' pickers.
class DZS_DzbotSetting : Managed
{
	string m_Id;
	string m_Title;
	string m_Type;
	bool m_Multiple;
	ref array<string> m_Options = new array<string>();
	ref array<string> m_Props = new array<string>();

	// a list a block can be repeated over (dzbot: every type that holds more than one value)
	bool IsList()
	{
		array<string> single = {"number", "boolean", "string", "textarea", "coordinate", "datetime", "email"};
		return single.Find(m_Type) < 0;
	}

	string Shown()
	{
		if (m_Title == "" || m_Title == m_Id)
			return m_Id;
		return m_Title + " (" + m_Id + ")";
	}
}

// dzbot's rules for a project mod (extension URL, docs/extensions/dzbot.rules.md). The rules sit on the entities they
// concern, in their own "extensions", kept on the entities' elements in the model (DZS_EntityExts): a block's
// {if, required, repeat, fields}, a list element's {if, repeat, fields}, a change's {if, formula, required}. Editing
// them is a step of the history. The project's own entry holds the mod's settings ("settings": dzbot's config
// fields), kept in the model too (DZS_EntityExts.ProjectData), so editing, renaming and removing a setting are steps
// of the history — a rename together with every rule that names it. The dialogs see a block's rule with its elements'
// rules under "items" by "<list>@<element id>" (DZS_EntityExts.ItemKeys), never by a place in the list. Whether rules
// go along when entities are copied is the spec's "copy" policy (same-project). Members it does not know stay as they
// are.
class DZS_DzbotRules : DZS_ProjectExtension
{
	static const string URL = "https://developers.dzbot.de/specs/dzbot/rules/1.json";
	// the member of the project's entry with the mod's settings
	static const string SETTINGS = "settings";
	// dzbot's type of a heading over the settings below it, and of a choice ("select" is dzStudio's older name)
	static const string SECTION = "section";
	static const string SELECT = "generic_select";

	DZS_DzbotPlugin m_P;
	// the opened file's entry data until Opened moved it into the model
	protected ref DZS_Json m_File;
	// bumped on every change, for cached marks
	int m_Gen;
	protected int m_MarkGen = -1;
	protected int m_MarkStateGen = -1;
	protected int m_MarkTick;
	protected ref map<string, string> m_Marks = new map<string, string>();
	protected ref array<ref DZS_DzbotSetting> m_Own;
	protected int m_OwnGen = -1;
	protected int m_OwnStateGen = -1;

	override string Url() { return URL; }

	override bool IsActive()
	{
		return m_P && m_P.OpenProjectMod() != null;
	}

	override void Load(DZS_Json data)
	{
		m_File = null;
		if (data && data.IsObj())
			m_File = data;
		m_Gen++;
	}

	override void Reset()
	{
		m_File = null;
		m_Gen++;
	}

	// The entry's data moves into the model (not a step of the history).
	override void Opened(DZS_Instance inst)
	{
		if (inst && m_File && m_File.Count() > 0 && !DZS_EntityExts.HasProjectData(inst, URL))
			DZS_EntityExts.SetProjectData(inst, URL, m_File);
		DZS_EntityExts.Commit(inst, "", "", true);
		if (inst)
			m_File = null;
		m_Gen++;
	}

	// The entry's data as it is now (a copy): the model's, else the opened file's; null when there is none.
	DZS_Json Current()
	{
		DZS_Instance inst = Inst();
		if (inst && DZS_EntityExts.HasProjectData(inst, URL))
			return DZS_EntityExts.ProjectData(inst, URL);
		if (m_File)
			return m_File.Clone();
		return null;
	}

	override DZS_Json Data()
	{
		DZS_Json d = Current();
		if (!d || !d.IsObj() || d.Count() == 0)
			return null;
		return d;
	}

	protected DZS_Instance Inst()
	{
		if (!m_P)
			return null;
		return m_P.Kernel().GetShown();
	}

	// ---- names of repeated blocks ----

	// what a copy of a named block is called without a template in its repeat
	static const string DEFAULT_COPY_NAME = "{name}_{n}";

	// The template with {name}, {n} (from 1) and {index} (from 0) put in, in one pass like dzbot.
	static string CopyName(string pattern, string name, int index)
	{
		string res = "";
		int number = index + 1;
		string nText = number.ToString();
		string iText = index.ToString();
		int len = pattern.Length();
		int at = 0;
		while (at < len)
		{
			string rest = pattern.Substring(at, len - at);
			if (rest.IndexOf("{name}") == 0)
			{
				res += name;
				at += 6;
			}
			else if (rest.IndexOf("{n}") == 0)
			{
				res += nText;
				at += 3;
			}
			else if (rest.IndexOf("{index}") == 0)
			{
				res += iText;
				at += 7;
			}
			else
			{
				res += pattern.Get(at);
				at++;
			}
		}
		return res;
	}

	// Why dzbot would not take a repeat's name template ("" = fine; empty means the default).
	static string CopyNameProblem(string pattern)
	{
		if (pattern == "")
			return "";
		string rest = pattern;
		rest.Replace("{name}", "");
		rest.Replace("{n}", "");
		rest.Replace("{index}", "");
		if (rest.IndexOf("{") >= 0)
			return DZS_Texts.Pick("nur {name}, {n} und {index} in Klammern", "only {name}, {n} and {index} in braces");
		string allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
		for (int i = 0; i < rest.Length(); i++)
		{
			if (allowed.IndexOf(rest.Get(i)) < 0)
				return DZS_Texts.Pick("nur Buchstaben, Ziffern und _ (Namen im Spiel erlauben nichts anderes)", "only letters, digits and _ (game names allow nothing else)");
		}
		if (pattern.IndexOf("{n}") < 0 && pattern.IndexOf("{index}") < 0)
			return DZS_Texts.Pick("braucht {n} oder {index}, sonst heißen alle Kopien gleich", "needs {n} or {index}, or every copy gets the same name");
		if (pattern.Length() > 200)
			return DZS_Texts.Pick("länger als 200 Zeichen", "longer than 200 characters");
		return "";
	}

	// ---- settings (config fields of the mod) ----

	// The project's own list of settings (a copy), null when it has none.
	DZS_Json SettingsList()
	{
		DZS_Json d = Current();
		if (!d || !d.IsObj())
			return null;
		DZS_Json list = d.Get(SETTINGS);
		if (!list || !list.IsArr())
			return null;
		return list;
	}

	// The settings for the pickers: the project's own, else the ones dzbot sent with the mod.
	array<ref DZS_DzbotSetting> Settings()
	{
		DZS_Json list = SettingsList();
		if (!list)
		{
			if (!m_P)
				return new array<ref DZS_DzbotSetting>();
			return m_P.OpenSettings();
		}
		int stateGen = -1;
		if (Inst())
			stateGen = Inst().GetGen();
		if (!m_Own || m_OwnGen != m_Gen || m_OwnStateGen != stateGen)
		{
			m_Own = new array<ref DZS_DzbotSetting>();
			m_OwnGen = m_Gen;
			m_OwnStateGen = stateGen;
			for (int i = 0; i < list.Count(); i++)
			{
				DZS_Json entry = list.At(i);
				DZS_DzbotSetting st = SettingOf(entry);
				// a section is a heading of dzbot's form, no value a rule can read
				if (st && st.m_Type != SECTION)
					m_Own.Insert(st);
			}
		}
		return m_Own;
	}

	DZS_DzbotSetting Setting(string id)
	{
		foreach (DZS_DzbotSetting s : Settings())
		{
			if (s.m_Id == id)
				return s;
		}
		return null;
	}

	// The identifier of an entry ("identifier", or "id" of the provider's shape).
	static string SettingId(DZS_Json entry)
	{
		if (!entry || !entry.IsObj())
			return "";
		string id = entry.GetStr("identifier");
		if (id == "")
			id = entry.GetStr("id");
		return id;
	}

	static bool IsSelect(string type)
	{
		return type == SELECT || type == "select";
	}

	// An entry's options: "typeConfig.options" (dzbot's place), else "options" beside it; null when there are none.
	static DZS_Json OptionsOf(DZS_Json entry)
	{
		DZS_Json tc = entry.Get("typeConfig");
		if (tc && tc.IsObj())
		{
			DZS_Json to = tc.Get("options");
			if (to && to.IsArr())
				return to;
		}
		DZS_Json o = entry.Get("options");
		if (o && o.IsArr())
			return o;
		return null;
	}

	// An entry's "typeConfig", created when it has none (or one that is no object).
	static DZS_Json TypeConfigForEdit(DZS_Json entry)
	{
		DZS_Json tc = entry.Get("typeConfig");
		if (tc && tc.IsObj())
			return tc;
		DZS_Json made = DZS_Json.Obj();
		entry.Set("typeConfig", made);
		return made;
	}

	static DZS_DzbotSetting SettingOf(DZS_Json entry)
	{
		string id = SettingId(entry);
		if (id == "")
			return null;
		DZS_DzbotSetting st = new DZS_DzbotSetting();
		st.m_Id = id;
		st.m_Title = entry.GetStr("title");
		st.m_Type = entry.GetStr("type");
		if (st.m_Type == "select")
			st.m_Type = SELECT;
		st.m_Multiple = entry.GetBool("multiple");
		DZS_Json tc = entry.Get("typeConfig");
		if (tc && tc.IsObj() && tc.GetBool("multiple"))
			st.m_Multiple = true;
		DZS_Json opts = OptionsOf(entry);
		if (opts)
		{
			for (int i = 0; i < opts.Count(); i++)
			{
				DZS_Json o = opts.At(i);
				if (o.IsObj())
					st.m_Options.Insert(o.GetStr("value"));
				else if (o.IsStr() || o.IsNum())
					st.m_Options.Insert(o.m_Str);
			}
		}
		// an object list's properties: dzbot's "typeConfig.properties", else "props" beside it or in typeConfig
		DZS_Json props = null;
		if (tc && tc.IsObj())
			props = tc.Get("properties");
		if (!props || !props.IsArr())
			props = entry.Get("props");
		if ((!props || !props.IsArr()) && tc && tc.IsObj())
			props = tc.Get("props");
		if (props && props.IsArr())
		{
			for (int k = 0; k < props.Count(); k++)
			{
				DZS_Json pr = props.At(k);
				if (pr.IsObj())
					st.m_Props.Insert(pr.GetStr("name"));
			}
		}
		return st;
	}

	// A copy of the project's list of settings to edit and hand to CommitSettings; without one, dzbot's list as it sent
	// it (the project's shape, every member kept), which the first commit copies into the project.
	DZS_Json SettingsForEdit()
	{
		DZS_Json list = SettingsList();
		if (list)
			return list;
		if (m_P)
			list = m_P.OpenSettingsList();
		if (!list)
			list = DZS_Json.Arr();
		return list;
	}

	// The list of settings as edited, and a setting renamed from 'old' to 'nw' in every rule that names it, as ONE
	// step of the history: undoing it puts back the settings and the rules together. False when nothing changed.
	bool CommitSettings(DZS_Json list, string old, string nw, string nameDe, string nameEn, out int renamed)
	{
		renamed = 0;
		DZS_Instance inst = Inst();
		if (!inst || !list)
			return false;
		DZS_Json d = Current();
		if (!d || !d.IsObj())
			d = DZS_Json.Obj();
		DZS_Json listCopy = list.Clone();
		d.Set(SETTINGS, listCopy);
		DZS_EntityExts.SetProjectData(inst, URL, d);
		if (old != "" && old != nw)
			renamed = StageRename(inst, old, nw);
		bool done = DZS_EntityExts.Commit(inst, nameDe, nameEn);
		m_Gen++;
		return done;
	}

	// A setting's new name in every condition, formula, value and repetition of the project's rules (staged).
	protected int StageRename(DZS_Instance inst, string old, string nw)
	{
		int n = 0;
		array<ref DZS_ExtOwner> owners = DZS_EntityExts.OwnersWith(inst, URL);
		foreach (DZS_ExtOwner o : owners)
		{
			DZS_Json v = DZS_EntityExts.Value(inst, o, URL);
			int one = RenameIn(v, old, nw);
			if (one == 0)
				continue;
			DZS_EntityExts.SetValue(inst, o, URL, v);
			n += one;
		}
		return n;
	}

	// Names in a rule tree: "field" of a comparison, "setting" of a value, "over" of a repetition, names in
	// "formula" and "count".
	static int RenameIn(DZS_Json node, string old, string nw)
	{
		if (!node)
			return 0;
		int n = 0;
		if (node.IsArr())
		{
			for (int i = 0; i < node.Count(); i++)
			{
				DZS_Json item = node.At(i);
				n += RenameIn(item, old, nw);
			}
			return n;
		}
		if (!node.IsObj())
			return 0;
		for (int k = 0; k < node.m_Keys.Count(); k++)
		{
			string key = node.m_Keys[k];
			DZS_Json v = node.Get(key);
			if (!v)
				continue;
			if (v.IsStr())
			{
				bool nameKey = key == "setting" || key == "over" || (key == "field" && node.Has("op"));
				if (nameKey && v.m_Str == old)
				{
					v.m_Str = nw;
					n++;
				}
				else if (key == "formula" || key == "count")
				{
					string renamed = RenameInFormula(v.m_Str, old, nw);
					if (renamed != v.m_Str)
					{
						v.m_Str = renamed;
						n++;
					}
				}
				continue;
			}
			n += RenameIn(v, old, nw);
		}
		return n;
	}

	protected static bool NameChar(string c, bool first)
	{
		string letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
		if (letters.IndexOf(c) >= 0)
			return true;
		string digits = "0123456789";
		return !first && digits.IndexOf(c) >= 0;
	}

	// Whole names (not the part after a dot: "source.x" renames "source", never "x").
	static string RenameInFormula(string f, string old, string nw)
	{
		string r = "";
		int i = 0;
		int len = f.Length();
		while (i < len)
		{
			string c = f.Get(i);
			if (!NameChar(c, true))
			{
				r += c;
				i++;
				continue;
			}
			int j = i;
			while (j < len && NameChar(f.Get(j), false))
				j++;
			string word = f.Substring(i, j - i);
			bool afterDot = i > 0 && f.Get(i - 1) == ".";
			if (word == old && !afterDot)
				r += nw;
			else
				r += word;
			i = j;
		}
		return r;
	}

	// ---- rules on the entities ----

	// The rule of an entity (a block's with its elements' rules under "items" by "<list>@<element id>"), null when
	// there is none.
	DZS_Json RuleAt(DZS_ExtOwner o)
	{
		DZS_Instance inst = Inst();
		if (!o || !inst)
			return null;
		DZS_Json bv = DZS_EntityExts.Value(inst, o, URL);
		map<string, ref DZS_Json> els = DZS_EntityExts.ItemValues(inst, o, URL);
		bool own = bv && bv.IsObj();
		if (!own && els.Count() == 0)
			return null;
		DZS_Json r;
		if (own)
			r = bv.Clone();
		else
			r = DZS_Json.Obj();
		if (els.Count() > 0)
		{
			DZS_Json items = DZS_Json.Obj();
			for (int i = 0; i < els.Count(); i++)
			{
				DZS_Json el = els.GetElement(i);
				DZS_Json elCopy = el.Clone();
				items.Set(els.GetKey(i), elCopy);
			}
			r.Set("items", items);
		}
		return r;
	}

	// An entity's rule replaced (its elements' under "items" by "<list>@<element id>"); an empty one (or null) removes
	// it. One step of the history. False when the entity is not in the project any more (nothing is written then);
	// an element that left the block is skipped.
	bool PutAt(DZS_ExtOwner o, DZS_Json rule)
	{
		DZS_Instance inst = Inst();
		if (!DZS_EntityExts.Alive(inst, o))
			return false;
		DZS_Json own = null;
		DZS_Json items = null;
		if (rule && rule.IsObj())
		{
			own = rule.Clone();
			items = own.Get("items");
			own.Remove("items");
			if (IsEmpty(own))
				own = null;
		}
		DZS_EntityExts.SetValue(inst, o, URL, own);
		map<string, ref DZS_Json> was = DZS_EntityExts.ItemValues(inst, o, URL);
		for (int i = 0; i < was.Count(); i++)
		{
			string old = was.GetKey(i);
			if (items && items.IsObj() && items.Has(old))
				continue;
			DZS_ExtOwner gone = DZS_EntityExts.ItemOwner(inst, o, old);
			if (gone)
				DZS_EntityExts.SetValue(inst, gone, URL, null);
		}
		if (items && items.IsObj())
		{
			for (int k = 0; k < items.m_Keys.Count(); k++)
			{
				string key = items.m_Keys[k];
				DZS_ExtOwner io = DZS_EntityExts.ItemOwner(inst, o, key);
				if (!io)
				{
					DZS_Log.Warn("dzbot: element " + key + " is not in the block any more, its rules are not stored");
					continue;
				}
				DZS_Json one = items.Get(key);
				DZS_Json oneCopy = null;
				if (!IsEmpty(one))
					oneCopy = one.Clone();
				DZS_EntityExts.SetValue(inst, io, URL, oneCopy);
			}
		}
		DZS_EntityExts.Commit(inst, "dzbot-Regeln ändern", "Edit dzbot rules");
		m_Gen++;
		return true;
	}

	// no member, or only empty "fields" / "items"
	static bool IsEmpty(DZS_Json rule)
	{
		if (!rule || !rule.IsObj())
			return true;
		for (int i = 0; i < rule.m_Keys.Count(); i++)
		{
			string k = rule.m_Keys[i];
			DZS_Json v = rule.Get(k);
			if ((k == "fields" || k == "items") && v && v.IsObj() && v.Count() == 0)
				continue;
			return false;
		}
		return true;
	}

	// "Condition · Formula · …" of a rule, "" for none.
	static string Summary(DZS_Json rule)
	{
		if (IsEmpty(rule))
			return "";
		string s = "";
		if (rule.Has("if"))
			s = Join(s, DZS_Texts.Pick("Bedingung", "Condition"));
		if (rule.Has("formula"))
			s = Join(s, DZS_Texts.Pick("Formel", "Formula"));
		if (rule.GetBool("required"))
			s = Join(s, DZS_Texts.Pick("Ziel nötig", "Target required"));
		if (rule.Has("repeat"))
			s = Join(s, DZS_Texts.Pick("Wiederholt", "Repeated"));
		DZS_Json f = rule.Get("fields");
		if (f && f.IsObj() && f.Count() > 0)
			s = Join(s, f.Count().ToString() + DZS_Texts.Pick(DZS_Issues.Plural(f.Count(), " Wert", " Werte"), DZS_Issues.Plural(f.Count(), " value", " values")));
		DZS_Json it = rule.Get("items");
		if (it && it.IsObj() && it.Count() > 0)
			s = Join(s, it.Count().ToString() + DZS_Texts.Pick(DZS_Issues.Plural(it.Count(), " Element", " Elemente"), DZS_Issues.Plural(it.Count(), " element", " elements")));
		return s;
	}

	protected static string Join(string a, string b)
	{
		if (a == "")
			return b;
		return a + " · " + b;
	}

	// ---- addressing and labels (DZS_ProjectRefs) ----

	static string ChangeRef(DZS_Json change) { return DZS_ProjectRefs.ChangeRef(change); }
	static string PhpNumber(string raw) { return DZS_ProjectRefs.PhpNumber(raw); }
	static string KindLabel(string kind) { return DZS_ProjectRefs.KindLabel(kind); }
	static string BlockLabel(DZS_Json block) { return DZS_ProjectRefs.BlockLabel(block); }
	static string ChangeLabel(DZS_Json change) { return DZS_ProjectRefs.ChangeLabel(change); }
	static string Short(DZS_Json v, int max) { return DZS_ProjectRefs.Short(v, max); }

	// The list element a field path steps into by its place ("objects[2]" of "objects[2].pos"), "" when it addresses
	// none: only keys, or an index into a list of plain values ("pos[1]"). dzbot refuses such a path: deleting an
	// element before it would retarget it, so the value belongs into the element's own rule. data: where the path
	// starts (a block's data or the element), null when unknown — then only an index with more after it counts.
	static string ByPlace(string path, DZS_Json data)
	{
		DZS_Json at = data;
		string shown = "";
		string key = "";
		int len = path.Length();
		int c = 0;
		while (c < len)
		{
			string ch = path.Get(c);
			if (ch != "." && ch != "[")
			{
				key += ch;
				c++;
				continue;
			}
			if (key != "")
			{
				if (shown != "")
					shown += ".";
				shown += key;
				if (at && at.IsObj())
					at = at.Get(key);
				else
					at = null;
				key = "";
			}
			if (ch == ".")
			{
				c++;
				continue;
			}
			int close = path.IndexOfFrom(c, "]");
			if (close < 0)
				return "";
			string num = path.Substring(c + 1, close - c - 1);
			shown += "[" + num + "]";
			if (close < len - 1)
				return shown;
			if (HoldsElements(at))
				return shown;
			at = null;
			c = close + 1;
		}
		return "";
	}

	// whether a list holds elements (objects, lists) rather than plain values like a position's numbers
	static bool HoldsElements(DZS_Json list)
	{
		if (!list)
			return false;
		if (!list.IsArr())
			return false;
		for (int i = 0; i < list.Count(); i++)
		{
			DZS_Json el = list.At(i);
			if (el && (el.IsObj() || el.IsArr()))
				return true;
		}
		return false;
	}

	// A change can carry a formula when it sets a value.
	static bool CanFormula(DZS_Json change)
	{
		return change.GetStr("op") == "set";
	}

	// A block can require its target when it goes into an entry that is not its own (a base key as parent).
	static bool CanRequire(DZS_Json block)
	{
		DZS_Json par = block.Get("parent");
		return par && par.IsObj();
	}

	// ---- marks in the group list ----

	override string GroupMark(DZS_Instance inst, string groupId)
	{
		int now = TickCount(0);
		if (m_MarkGen != m_Gen || m_MarkStateGen != inst.GetGen() || now - m_MarkTick > 20000000 || now < m_MarkTick)
		{
			m_Marks.Clear();
			m_MarkGen = m_Gen;
			m_MarkStateGen = inst.GetGen();
			m_MarkTick = now;
			map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
			for (int i = 0; i < ids.Count(); i++)
			{
				string gid = ids.GetKey(i);
				if (DZS_EntityExts.GroupHas(inst, gid, URL))
					m_Marks.Set(gid, DZS_Texts.Pick("dzbot-Regeln", "dzbot rules"));
			}
		}
		string mark;
		if (m_Marks.Find(groupId, mark))
			return mark;
		return "";
	}

	override void GroupMenu(DZS_Instance inst, string groupId, array<ref DZS_MenuItem> items)
	{
		DZS_MenuItem it = new DZS_MenuItem();
		it.m_Tag = "rules";
		it.m_Text = DZS_Texts.Pick("dzbot-Regeln der Gruppe …", "dzbot rules of the group …");
		it.m_SepBefore = true;
		items.Insert(it);
	}

	override void GroupMenuChosen(DZS_Instance inst, string groupId, string tag)
	{
		if (tag == "rules" && m_P)
			m_P.OpenRules(groupId);
	}
}

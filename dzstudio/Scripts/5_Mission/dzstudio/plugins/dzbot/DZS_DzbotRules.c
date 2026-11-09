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

// dzbot's rules for a project mod (extension URL, docs/extensions/dzbot.rules.md): per own block (by its id)
// and per change (by its ref) a condition on the mod's settings, a formula, "target required", and for blocks
// repetition, values from settings and rules for single list elements. dzStudio edits them; dzbot applies them.
// Members it does not know stay as they are.
class DZS_DzbotRules : DZS_ProjectExtension
{
	static const string URL = "https://developers.dzbot.de/specs/dzbot/rules/1.json";

	DZS_DzbotPlugin m_P;
	protected ref DZS_Json m_Data;
	// bumped on every change, for cached marks
	int m_Gen;
	protected int m_MarkGen = -1;
	protected int m_MarkTick;
	protected ref map<string, string> m_Marks = new map<string, string>();

	override string Url() { return URL; }

	override bool IsActive()
	{
		return m_P && m_P.OpenProjectMod() != null;
	}

	override void Load(DZS_Json data)
	{
		m_Data = null;
		if (data && data.IsObj())
			m_Data = data;
		m_Gen++;
	}

	override void Reset()
	{
		m_Data = null;
		m_Gen++;
	}

	override DZS_Json Data()
	{
		if (!m_Data)
			return null;
		Clean(m_Data, "blocks");
		Clean(m_Data, "changes");
		if (m_Data.Count() == 0)
			return null;
		return m_Data.Clone();
	}

	protected static void Clean(DZS_Json data, string section)
	{
		DZS_Json s = data.Get(section);
		if (s && s.IsObj() && s.Count() == 0)
			data.Remove(section);
	}

	array<ref DZS_DzbotSetting> Settings()
	{
		if (!m_P)
			return new array<ref DZS_DzbotSetting>();
		return m_P.OpenSettings();
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

	// ---- rules ----

	protected DZS_Json Section(string section, bool create)
	{
		if (!m_Data)
		{
			if (!create)
				return null;
			m_Data = DZS_Json.Obj();
		}
		DZS_Json s = m_Data.Get(section);
		if ((!s || !s.IsObj()) && create)
		{
			s = DZS_Json.Obj();
			m_Data.Set(section, s);
		}
		if (s && !s.IsObj())
			return null;
		return s;
	}

	// section "blocks" or "changes"; null when there is no rule
	DZS_Json Rule(string section, string rid)
	{
		DZS_Json s = Section(section, false);
		if (!s)
			return null;
		DZS_Json r = s.Get(rid);
		if (!r || !r.IsObj())
			return null;
		return r;
	}

	// A rule replaced; an empty one (or null) removes it.
	void Put(string section, string rid, DZS_Json rule)
	{
		if (IsEmpty(rule))
		{
			DZS_Json s = Section(section, false);
			if (s)
				s.Remove(rid);
		}
		else
		{
			DZS_Json into = Section(section, true);
			into.Set(rid, rule);
		}
		m_Gen++;
		DZS_ProjectExtensions.Changed(m_P.Kernel().GetShown());
	}

	array<string> Refs(string section)
	{
		array<string> r = new array<string>();
		DZS_Json s = Section(section, false);
		if (s)
			r.Copy(s.m_Keys);
		return r;
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
			s = Join(s, f.Count().ToString() + DZS_Texts.Pick(" Werte", " values"));
		DZS_Json it = rule.Get("items");
		if (it && it.IsObj() && it.Count() > 0)
			s = Join(s, it.Count().ToString() + DZS_Texts.Pick(" Elemente", " elements"));
		return s;
	}

	protected static string Join(string a, string b)
	{
		if (a == "")
			return b;
		return a + " · " + b;
	}

	// ---- addressing, exactly as dzbot does (ProjectRules::ref) ----

	// block, the key as compact JSON (numbers as PHP writes them after reading), field; trimmed
	static string ChangeRef(DZS_Json change)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		w.Append(change.GetStr("block"));
		w.Append(" ");
		DZS_Json key = change.Get("key");
		if (!key || key.m_Type == DZS_Json.NUL)
			w.Append("{}");
		else
			Canon(w, key);
		w.Append(" ");
		w.Append(change.GetStr("field"));
		array<string> parts = w.Finish();
		string r = "";
		foreach (string p : parts)
			r += p;
		r.TrimInPlace();
		return r;
	}

	static void Canon(DZS_TextWriter w, DZS_Json v)
	{
		if (v.m_Type == DZS_Json.NUM)
		{
			w.Append(PhpNumber(v.m_Str));
			return;
		}
		if (v.m_Type == DZS_Json.ARR)
		{
			w.Append("[");
			for (int i = 0; i < v.m_Items.Count(); i++)
			{
				if (i > 0)
					w.Append(",");
				DZS_Json item = v.m_Items[i];
				Canon(w, item);
			}
			w.Append("]");
			return;
		}
		if (v.m_Type == DZS_Json.OBJ)
		{
			w.Append("{");
			for (int k = 0; k < v.m_Keys.Count(); k++)
			{
				if (k > 0)
					w.Append(",");
				string name = v.m_Keys[k];
				DZS_Json.WriteString(w, name);
				w.Append(":");
				DZS_Json member = v.m_Map.Get(name);
				Canon(w, member);
			}
			w.Append("}");
			return;
		}
		v.Write(w, false, 0);
	}

	// A JSON number as PHP's json_encode writes it after json_decode: 5.0 → 5, 1.50 → 1.5, 1e2 → 100, 0.00001 → 1.0e-5,
	// 1e17 → 1.0e+17. More than 17 significant digits are not rounded.
	static string PhpNumber(string raw)
	{
		string s = raw;
		bool neg = false;
		if (s.Length() > 0 && s.Get(0) == "-")
		{
			neg = true;
			s = s.Substring(1, s.Length() - 1);
		}
		int exp = 0;
		string low = s;
		low.ToLower();
		int e = low.IndexOf("e");
		string mant = s;
		if (e >= 0)
		{
			string es = s.Substring(e + 1, s.Length() - e - 1);
			bool eneg = false;
			if (es.Length() > 0 && (es.Get(0) == "+" || es.Get(0) == "-"))
			{
				eneg = es.Get(0) == "-";
				es = es.Substring(1, es.Length() - 1);
			}
			exp = es.ToInt();
			if (eneg)
				exp = -exp;
			mant = s.Substring(0, e);
		}
		int dot = mant.IndexOf(".");
		string ip = mant;
		string fp = "";
		if (dot >= 0)
		{
			ip = mant.Substring(0, dot);
			fp = mant.Substring(dot + 1, mant.Length() - dot - 1);
		}
		string digits = ip + fp;
		int decpt = ip.Length() + exp;
		while (digits.Length() > 0 && digits.Get(0) == "0")
		{
			digits = digits.Substring(1, digits.Length() - 1);
			decpt--;
		}
		if (e < 0 && dot < 0)
		{
			if (digits == "")
				return "0";
			if (neg)
				return "-" + digits;
			return digits;
		}
		while (digits.Length() > 0 && digits.Get(digits.Length() - 1) == "0")
			digits = digits.Substring(0, digits.Length() - 1);
		if (digits == "")
			return "0";
		string sign = "";
		if (neg)
			sign = "-";
		int n = digits.Length();
		if (decpt >= n && decpt <= 17)
		{
			string whole = digits;
			for (int z = n; z < decpt; z++)
				whole += "0";
			return sign + whole;
		}
		if (decpt < -3 || decpt > 17)
		{
			string rest = "0";
			if (n > 1)
				rest = digits.Substring(1, n - 1);
			int pexp = decpt - 1;
			string esign = "+";
			if (pexp < 0)
			{
				esign = "-";
				pexp = -pexp;
			}
			return sign + digits.Get(0) + "." + rest + "e" + esign + pexp.ToString();
		}
		if (decpt <= 0)
		{
			string lead = "0.";
			for (int q = 0; q < -decpt; q++)
				lead += "0";
			return sign + lead + digits;
		}
		return sign + digits.Substring(0, decpt) + "." + digits.Substring(decpt, n - decpt);
	}

	// ---- what the project holds (DZS_ProjectV2.Build) ----

	static string KindLabel(string kind)
	{
		if (kind == "spawnerGroup")
			return DZS_Texts.Pick("Objektgruppe", "Object group");
		if (kind == "lootRow")
			return DZS_Texts.Pick("Loot-Objekt", "Loot object");
		if (kind == "lootType" || kind == "itemType")
			return DZS_Texts.Pick("Item", "Item");
		if (kind == "lootContainer")
			return DZS_Texts.Pick("Loot-Behälter", "Loot container");
		if (kind == "lootPoint")
			return DZS_Texts.Pick("Loot-Punkt", "Loot point");
		if (kind == "event")
			return "Event";
		if (kind == "eventChild")
			return DZS_Texts.Pick("Event-Fahrzeug/-Tier", "Event child");
		if (kind == "spawnPosition")
			return DZS_Texts.Pick("Spawnpunkt", "Spawn position");
		if (kind == "effectArea")
			return DZS_Texts.Pick("Effektbereich", "Effect area");
		if (kind == "territory" || kind == "territoryZone")
			return DZS_Texts.Pick("Territorium", "Territory");
		if (kind == "weather")
			return DZS_Texts.Pick("Wetter", "Weather");
		if (kind == "gameplay")
			return "cfggameplay.json";
		return kind;
	}

	static string BlockLabel(DZS_Json block)
	{
		string label = KindLabel(block.GetStr("kind"));
		DZS_Json data = block.Get("data");
		string name = "";
		if (data && data.IsObj())
		{
			name = data.GetStr("name");
			if (name == "")
				name = data.GetStr("type");
		}
		if (name != "")
			label += " · " + name;
		return label + " · " + block.GetStr("id");
	}

	static string ChangeLabel(DZS_Json change)
	{
		string label = KindLabel(change.GetStr("block"));
		DZS_Json key = change.Get("key");
		if (key && key.IsObj())
		{
			for (int i = 0; i < key.m_Keys.Count(); i++)
			{
				string kn = key.m_Keys[i];
				DZS_Json kv = key.Get(kn);
				label += " · " + Short(kv, 40);
			}
		}
		string field = change.GetStr("field");
		if (field != "")
			label += " · " + field;
		if (change.GetStr("op") == "remove")
			label += DZS_Texts.Pick(" · entfernen", " · remove");
		else
		{
			DZS_Json to = change.Get("to");
			if (to)
				label += " → " + Short(to, 30);
		}
		return label;
	}

	static string Short(DZS_Json v, int max)
	{
		string t;
		if (v.IsStr())
			t = v.m_Str;
		else if (v.IsNum())
			t = v.m_Str;
		else
		{
			array<string> chunks = v.ToChunks(false);
			t = "";
			foreach (string c : chunks)
			{
				t += c;
				if (t.Length() > max)
					break;
			}
		}
		if (t.Length() > max)
			t = DZS_Utf8.Part(t, 0, max - 1) + "…";
		return t;
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
		if (m_MarkGen != m_Gen || now - m_MarkTick > 20000000 || now < m_MarkTick)
		{
			m_Marks.Clear();
			m_MarkGen = m_Gen;
			m_MarkTick = now;
			map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
			for (int i = 0; i < ids.Count(); i++)
			{
				string gid = ids.GetKey(i);
				string bid = ids.GetElement(i);
				if (Rule("blocks", bid))
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

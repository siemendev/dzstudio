// One setting with its row in the settings dialog.
class DZS_SettingDef : Managed
{
	string m_Key;
	string m_Tab;
	string m_NameDe;
	string m_NameEn;
	string m_ExplDe;
	string m_ExplEn;
	string m_Kind;
	float m_Min;
	float m_Max;
	float m_Step;
	int m_Decimals;
	string m_Unit;
	int m_Seq;
	// Several values in one row: the keys of the other switches and their short names.
	ref array<string> m_RowKeys;
	ref array<string> m_RowNamesDe;
	ref array<string> m_RowNamesEn;

	string Name() { return DZS_Texts.Pick(m_NameDe, m_NameEn); }
	string Expl() { return DZS_Texts.Pick(m_ExplDe, m_ExplEn); }
}

// A plugin's own page in a settings tab instead of the generic rows (e.g. a server list).
class DZS_SettingsPage : Managed
{
	void Build(Widget page, DZS_Dialog dialog) {}
	float Layout(float width) { return 0; }
	void Update(float dt) {}
	void Destroy() {}
}

class DZS_SettingTab : Managed
{
	string m_Key;
	string m_NameDe;
	string m_NameEn;
	int m_Order;

	string Name() { return DZS_Texts.Pick(m_NameDe, m_NameEn); }
}

// $profile:dzstudio\einstellungen.json: only values that differ from their default, plus
// remembered values; unknown entries (plugin not loaded) stay untouched.
class DZS_Settings : Managed
{
	static const string DIR = "$profile:dzstudio";
	static const string FILE = "$profile:dzstudio\\einstellungen.json";
	static const string FILE_NEW = "$profile:dzstudio\\einstellungen.neu.json";
	static const string FILE_BROKEN = "$profile:dzstudio\\einstellungen.defekt.json";

	protected ref DZS_Json m_Root;
	protected ref map<string, ref DZS_Json> m_Defaults;
	protected bool m_Dirty;
	protected float m_DirtyAge;
	protected bool m_WasBroken;
	protected DZS_EventBus m_Events;
	protected ref map<string, ref DZS_SettingDef> m_Defs;
	protected ref array<string> m_DefOrder;
	protected ref array<ref DZS_SettingTab> m_Tabs;
	protected bool m_HoldFlush;
	protected int m_DefSeq;

	void DZS_Settings(DZS_EventBus events)
	{
		m_Events = events;
		m_Defaults = new map<string, ref DZS_Json>();
		m_Defs = new map<string, ref DZS_SettingDef>();
		m_DefOrder = new array<string>();
		m_Tabs = new array<ref DZS_SettingTab>();
		AddTab("camera", "Kamera", "Camera", 10);
		AddTab("placement", "Platzieren", "Placing", 20);
		AddTab("project", "Projekt", "Project", 30);
		AddTab("ui", "Oberfläche", "Interface", 40);
		AddTab("keys", "Tastenbelegung", "Key bindings", 100000);
		NewRoot();
	}

	protected void NewRoot()
	{
		m_Root = DZS_Json.Obj();
		m_Root.Set("format", DZS_Json.Str("dzstudio-settings"));
		m_Root.Set("version", DZS_Json.Int(1));
		m_Root.Set("values", DZS_Json.Obj());
		m_Root.Set("keys", DZS_Json.Obj());
	}

	bool WasBroken() { return m_WasBroken; }

	static array<string> ReadLines(string path)
	{
		array<string> chunks = new array<string>();
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (!fh)
			return null;
		string line;
		while (FGets(fh, line) >= 0)
			chunks.Insert(line + "\n");
		CloseFile(fh);
		return chunks;
	}

	void Load()
	{
		MakeDirectory(DIR);
		string path = FILE;
		if (!FileExist(path))
		{
			if (FileExist(FILE_NEW))
				path = FILE_NEW;
			else
				return;
		}
		array<string> chunks = ReadLines(path);
		string error;
		DZS_Json parsed = null;
		if (chunks)
			parsed = DZS_Json.Parse(chunks, error);
		if (!parsed || !parsed.IsObj() || parsed.GetStr("format") != "dzstudio-settings")
		{
			if (path == FILE && FileExist(FILE_NEW))
			{
				array<string> alt = ReadLines(FILE_NEW);
				string error2;
				DZS_Json parsed2 = DZS_Json.Parse(alt, error2);
				if (parsed2 && parsed2.IsObj() && parsed2.GetStr("format") == "dzstudio-settings")
				{
					Adopt(parsed2);
					return;
				}
			}
			DeleteFile(FILE_BROKEN);
			CopyFile(FILE, FILE_BROKEN);
			m_WasBroken = true;
			DZS_Log.Warn("settings unreadable (" + error + "), defaults in use");
			if (m_Events)
				m_Events.FireNotice(new DZS_Notice("settings", "Einstellungen nicht lesbar — Standard wird verwendet", "Settings unreadable — using defaults", "warning", 4000));
			return;
		}
		Adopt(parsed);
	}

	protected void Adopt(DZS_Json parsed)
	{
		m_Root = parsed;
		if (!m_Root.Get("values") || !m_Root.Get("values").IsObj())
			m_Root.Set("values", DZS_Json.Obj());
		if (!m_Root.Get("keys") || !m_Root.Get("keys").IsObj())
			m_Root.Set("keys", DZS_Json.Obj());
	}

	bool IsDeveloper()
	{
		return m_Root.GetBool("developer", false);
	}

	// A plugin declares its settings with their defaults; a stored value of another type counts as absent.
	void Define(string key, DZS_Json def)
	{
		DZS_Json t153 = def.Clone();
		m_Defaults.Set(key, t153);
	}

	DZS_Json Get(string key)
	{
		DZS_Json v = m_Root.Get("values").Get(key);
		DZS_Json def = m_Defaults.Get(key);
		if (v && (!def || def.m_Type == v.m_Type))
			return v;
		return def;
	}

	string GetString(string key, string fallback = "")
	{
		DZS_Json v = Get(key);
		if (!v || !v.IsStr())
			return fallback;
		return v.m_Str;
	}

	bool GetBool(string key, bool fallback = false)
	{
		DZS_Json v = Get(key);
		if (!v || !v.IsBool())
			return fallback;
		return v.m_Bool;
	}

	float GetNumber(string key, float fallback = 0)
	{
		DZS_Json v = Get(key);
		if (!v || !v.IsNum())
			return fallback;
		return v.m_Str.ToFloat();
	}

	void Set(string key, DZS_Json v)
	{
		DZS_Json copy = v.Clone();
		DZS_Json values = m_Root.Get("values");
		DZS_Json def = m_Defaults.Get(key);
		if (def && SameValue(def, copy))
			values.Remove(key);
		else
			values.Set(key, copy);
		Touch();
		if (m_Events)
			m_Events.FireSettingChanged(key);
	}

	// ---- setting rows ----

	protected void AddTab(string key, string de, string en, int order)
	{
		DZS_SettingTab t = new DZS_SettingTab();
		t.m_Key = key;
		t.m_NameDe = de;
		t.m_NameEn = en;
		t.m_Order = order;
		m_Tabs.Insert(t);
	}

	protected ref map<string, ref DZS_SettingsPage> m_Pages;

	void RegisterPage(string key, DZS_SettingsPage page)
	{
		if (!m_Pages)
			m_Pages = new map<string, ref DZS_SettingsPage>();
		m_Pages.Set(key, page);
	}

	DZS_SettingsPage GetPage(string key)
	{
		if (!m_Pages)
			return null;
		return m_Pages.Get(key);
	}

	// A plugin's own tab, placed after the bundled ones in loading order and before the key bindings.
	void RegisterTab(string key, string de, string en)
	{
		if (FindTab(key))
			return;
		AddTab(key, de, en, 1000 + m_Tabs.Count());
	}

	DZS_SettingTab FindTab(string key)
	{
		foreach (DZS_SettingTab t : m_Tabs)
		{
			if (t.m_Key == key)
				return t;
		}
		return null;
	}

	// Tabs in order; a tab without settings is left out (the key bindings always stay).
	array<DZS_SettingTab> GetTabs()
	{
		array<DZS_SettingTab> r = new array<DZS_SettingTab>();
		foreach (DZS_SettingTab t : m_Tabs)
		{
			if (t.m_Key != "keys" && GetDefs(t.m_Key).Count() == 0 && !GetPage(t.m_Key))
				continue;
			int at = 0;
			while (at < r.Count() && r[at].m_Order <= t.m_Order)
				at++;
			r.InsertAt(t, at);
		}
		return r;
	}

	DZS_SettingDef DefineToggle(string key, string tab, string nameDe, string nameEn, string explDe, string explEn, bool def)
	{
		DZS_SettingDef d = NewDef(key, tab, nameDe, nameEn, explDe, explEn, "toggle");
		Define(key, DZS_Json.Boolean(def));
		return d;
	}

	DZS_SettingDef DefineSlider(string key, string tab, string nameDe, string nameEn, string explDe, string explEn, float min, float max, float step, int decimals, string unit, float def)
	{
		DZS_SettingDef d = NewDef(key, tab, nameDe, nameEn, explDe, explEn, "slider");
		d.m_Min = min;
		d.m_Max = max;
		d.m_Step = step;
		d.m_Decimals = decimals;
		d.m_Unit = unit;
		Define(key, NumJson(def, decimals));
		return d;
	}

	// More switches in the same row (Gerade stellen with three switches).
	void AddRowToggle(string rowKey, string key, string nameDe, string nameEn, bool def)
	{
		DZS_SettingDef d = m_Defs.Get(rowKey);
		if (!d)
			return;
		if (!d.m_RowKeys)
		{
			d.m_RowKeys = new array<string>();
			d.m_RowNamesDe = new array<string>();
			d.m_RowNamesEn = new array<string>();
		}
		d.m_RowKeys.Insert(key);
		d.m_RowNamesDe.Insert(nameDe);
		d.m_RowNamesEn.Insert(nameEn);
		Define(key, DZS_Json.Boolean(def));
	}

	protected DZS_SettingDef NewDef(string key, string tab, string nameDe, string nameEn, string explDe, string explEn, string kind)
	{
		DZS_SettingDef d = new DZS_SettingDef();
		d.m_Key = key;
		d.m_Tab = tab;
		d.m_NameDe = nameDe;
		d.m_NameEn = nameEn;
		d.m_ExplDe = explDe;
		d.m_ExplEn = explEn;
		d.m_Kind = kind;
		m_DefSeq++;
		d.m_Seq = m_DefSeq;
		m_Defs.Set(key, d);
		m_DefOrder.Insert(key);
		return d;
	}

	DZS_SettingDef GetDef(string key) { return m_Defs.Get(key); }

	array<DZS_SettingDef> GetDefs(string tab)
	{
		array<DZS_SettingDef> r = new array<DZS_SettingDef>();
		foreach (string k : m_DefOrder)
		{
			DZS_SettingDef d = m_Defs.Get(k);
			if (d.m_Tab == tab)
				r.Insert(d);
		}
		return r;
	}

	static DZS_Json NumJson(float v, int decimals)
	{
		if (decimals == 0)
			return DZS_Json.Int(Math.Round(v));
		return DZS_Json.Num(DZS_Num.Fixed(v, decimals, true));
	}

	// Clamped to the range, then set to the nearest step counted from the lower end; a tie goes up.
	static float Normalize(DZS_SettingDef d, float v)
	{
		float c = Math.Clamp(v, d.m_Min, d.m_Max);
		if (d.m_Step <= 0)
			return c;
		float n = (c - d.m_Min) / d.m_Step;
		float steps = Math.Floor(n + 0.5 + 0.0001);
		float r = d.m_Min + steps * d.m_Step;
		if (r > d.m_Max + 0.00001)
			r -= d.m_Step;
		float mul = Math.Pow(10, d.m_Decimals);
		return Math.Round(r * mul) / mul;
	}

	float GetSettingNumber(string key)
	{
		DZS_SettingDef d = m_Defs.Get(key);
		DZS_Json def = m_Defaults.Get(key);
		float fallback = 0;
		if (def)
			fallback = def.m_Str.ToFloat();
		float v = GetNumber(key, fallback);
		if (d)
			v = Normalize(d, v);
		return v;
	}

	void SetSettingNumber(string key, float v)
	{
		DZS_SettingDef d = m_Defs.Get(key);
		int dec = 0;
		if (d)
		{
			v = Normalize(d, v);
			dec = d.m_Decimals;
		}
		Set(key, NumJson(v, dec));
	}

	bool IsDefault(string key)
	{
		return !m_Root.Get("values").Get(key);
	}

	DZS_Json GetDefault(string key) { return m_Defaults.Get(key); }

	void Reset(string key)
	{
		if (IsDefault(key))
			return;
		m_Root.Get("values").Remove(key);
		Touch();
		if (m_Events)
			m_Events.FireSettingChanged(key);
	}

	// Keys of all rows of a tab (with the extra switches of a row).
	array<string> TabKeys(string tab)
	{
		array<string> r = new array<string>();
		array<DZS_SettingDef> defs = GetDefs(tab);
		foreach (DZS_SettingDef d : defs)
		{
			r.Insert(d.m_Key);
			if (d.m_RowKeys)
			{
				foreach (string rk : d.m_RowKeys)
					r.Insert(rk);
			}
		}
		return r;
	}

	// While a slider is dragged, the file is written only after release.
	void SetHoldFlush(bool hold)
	{
		m_HoldFlush = hold;
		if (!hold && m_Dirty)
			m_DirtyAge = 0;
	}

	bool HasCustomKeys()
	{
		return m_Root.Get("keys").Count() > 0;
	}

	// keys == null removes the entry (default binding); an empty list means "no key on purpose".
	void SetKeys(string action, array<string> keys)
	{
		DZS_Json k = m_Root.Get("keys");
		if (!keys)
			k.Remove(action);
		else
		{
			DZS_Json list = DZS_Json.Arr();
			foreach (string s : keys)
				list.Add(DZS_Json.Str(s));
			k.Set(action, list);
		}
		Touch();
	}

	array<string> GetKeys(string action)
	{
		DZS_Json k = m_Root.Get("keys").Get(action);
		if (!k || !k.IsArr())
			return null;
		array<string> r = new array<string>();
		for (int i = 0; i < k.Count(); i++)
		{
			if (k.At(i).IsStr())
				r.Insert(k.At(i).m_Str);
		}
		return r;
	}

	protected bool SameValue(DZS_Json a, DZS_Json b)
	{
		if (a.m_Type != b.m_Type)
			return false;
		array<string> ca = a.ToChunks();
		array<string> cb = b.ToChunks();
		if (ca.Count() != cb.Count())
			return false;
		for (int i = 0; i < ca.Count(); i++)
		{
			if (ca[i] != cb[i])
				return false;
		}
		return true;
	}

	protected void Touch()
	{
		m_Dirty = true;
		m_DirtyAge = 0;
	}

	void Update(float dt)
	{
		if (!m_Dirty || m_HoldFlush)
			return;
		m_DirtyAge += dt;
		if (m_DirtyAge >= 1.0)
			Flush();
	}

	// Written completely as .neu.json, copied over the old file, then the copy is removed.
	void Flush()
	{
		if (!m_Dirty)
			return;
		m_Dirty = false;
		MakeDirectory(DIR);
		FileHandle fh = OpenFile(FILE_NEW, FileMode.WRITE);
		if (!fh)
		{
			DZS_Log.Error("settings: cannot write " + FILE_NEW);
			return;
		}
		array<string> chunks = m_Root.ToChunks(true);
		foreach (string c : chunks)
			FPrint(fh, c);
		CloseFile(fh);
		DeleteFile(FILE);
		if (CopyFile(FILE_NEW, FILE))
			DeleteFile(FILE_NEW);
		else
			DZS_Log.Error("settings: cannot replace " + FILE);
	}
}

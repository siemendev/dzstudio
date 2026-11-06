// Values of one object as the properties show them and the customString variants.
class DZS_PropObj : Managed
{
	string m_Id;
	string m_Name;
	vector m_Pos;
	vector m_Ypr;
	float m_Scale = 1;
	bool m_Persist;
	string m_Custom;
	string m_Group;
	bool m_Locked;
	bool m_Hidden;

	static DZS_PropObj Read(DZS_Instance inst, string id)
	{
		if (!inst || id == "" || !inst.Has(id) || inst.GetKind(id) != DZS_KIND_OBJECT)
			return null;
		DZS_PropObj o = new DZS_PropObj();
		o.m_Id = id;
		string name, pos, ypr, scale, persist, custom;
		inst.GetField(id, DZS_Obj.NAME, name);
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		inst.GetField(id, DZS_Obj.PERSIST, persist);
		inst.GetField(id, DZS_Obj.CUSTOM, custom);
		o.m_Name = name;
		float a, b, c;
		if (DZS_Obj.ParseVec(pos, a, b, c))
			o.m_Pos = Vector(a, b, c);
		if (DZS_Obj.ParseVec(ypr, a, b, c))
			o.m_Ypr = Vector(a, b, c);
		float s;
		if (scale != "" && DZS_F32.FromText(scale, s))
			o.m_Scale = s;
		o.m_Persist = persist == "1";
		o.m_Custom = custom;
		o.m_Group = inst.GetParent(id);
		o.m_Locked = inst.IsLocked(id) || inst.IsLocked(o.m_Group);
		o.m_Hidden = inst.IsHidden(id) || inst.IsHidden(o.m_Group);
		return o;
	}

	DZS_PropObj Copy()
	{
		DZS_PropObj o = new DZS_PropObj();
		o.m_Id = m_Id;
		o.m_Name = m_Name;
		o.m_Pos = m_Pos;
		o.m_Ypr = m_Ypr;
		o.m_Scale = m_Scale;
		o.m_Persist = m_Persist;
		o.m_Custom = m_Custom;
		o.m_Group = m_Group;
		o.m_Locked = m_Locked;
		o.m_Hidden = m_Hidden;
		return o;
	}

	// One of the seven number values by field index (0–2 position, 3–5 yaw/pitch/roll, 6 size).
	float Value(int f)
	{
		if (f < 3)
			return m_Pos[f];
		if (f < 6)
			return m_Ypr[f - 3];
		return m_Scale;
	}

	void SetValue(int f, float v)
	{
		if (f < 3)
			m_Pos = With(m_Pos, f, v);
		else if (f < 6)
			m_Ypr = With(m_Ypr, f - 3, v);
		else
			m_Scale = v;
	}

	static vector With(vector a, int i, float v)
	{
		float x = a[0];
		float y = a[1];
		float z = a[2];
		if (i == 0)
			x = v;
		else if (i == 1)
			y = v;
		else
			z = v;
		return Vector(x, y, z);
	}
}

class DZS_PropText
{
	static string GameName(string name)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!cat || name == "")
			return name;
		string shown = cat.ShownName(name);
		return shown;
	}

	static string Icon(string name)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!cat)
			return "package";
		string icon = cat.IconOf(name);
		return icon;
	}

	// Warning of a placeholder or "" for an object the editor shows.
	static string Placeholder(string name)
	{
		if (name == "")
			return DZS_Texts.Pick("Objekt ohne Namen — entsteht auf dem Server nicht", "Object without a name — does not appear on the server");
		if (DZS_Obj.IsModelPath(name))
		{
			if (!FileExist(name))
				return DZS_Texts.Pick("Modell nicht gefunden", "Model not found");
			return "";
		}
		if (DZS_ClassDir.Find(name) == "")
			return DZS_Texts.Pick("Klasse unbekannt — entsteht auf dem Server nicht", "Unknown class — does not appear on the server");
		return "";
	}

	static bool HasControlChars(string s)
	{
		int n = s.Length();
		for (int i = 0; i < n; i++)
		{
			int b = s.Get(i).ToAscii();
			if (b >= 0 && b < 32)
				return true;
		}
		return false;
	}

	// Number with the decimals of a field; "6512,400" in German.
	static string Num(float v, int decimals)
	{
		return DZS_Num.Fixed(v, decimals);
	}

	static float Round(float v, int decimals)
	{
		float mul = 1;
		for (int i = 0; i < decimals; i++)
			mul *= 10;
		return Math.Round(v * mul) / mul;
	}
}

// Classes that read customString, found in the class data of the running game.
class DZS_Variants
{
	static const int NONE = 0;
	static const int FLAG = 1;
	static const int TRIGGER = 2;
	static const string TAG_KEY = "undergroundTriggerTag";
	protected static ref map<string, int> s_Kinds;
	protected static ref array<string> s_Flags;

	static int KindOf(string name)
	{
		if (name == "" || DZS_Obj.IsModelPath(name))
			return NONE;
		if (!s_Kinds)
			s_Kinds = new map<string, int>();
		string key = name;
		key.ToLower();
		if (s_Kinds.Contains(key))
			return s_Kinds.Get(key);
		int kind = NONE;
		string cls = DZS_ClassDir.Find(name);
		if (cls != "")
		{
			if (GetGame().IsKindOf(cls, "StaticFlagPole"))
				kind = FLAG;
			else if (GetGame().IsKindOf(cls, "Land_WarheadStorage_Main") || GetGame().IsKindOf(cls, "Land_WarheadStorage_Bunker_Facility"))
				kind = TRIGGER;
		}
		s_Kinds.Set(key, kind);
		if (kind != NONE)
			DZS_Log.Info("properties: variant class " + cls + " kind " + kind);
		return kind;
	}

	// Every flag class of the running game (subclasses of Flag_Base that can be made), in the game's spelling.
	static array<string> Flags()
	{
		if (s_Flags)
			return s_Flags;
		s_Flags = new array<string>();
		int n = GetGame().ConfigGetChildrenCount("CfgVehicles");
		for (int i = 0; i < n; i++)
		{
			string cls;
			GetGame().ConfigGetChildName("CfgVehicles", i, cls);
			if (cls == "Flag_Base")
				continue;
			if (GetGame().ConfigGetInt("CfgVehicles " + cls + " scope") < 2)
				continue;
			if (GetGame().IsKindOf(cls, "Flag_Base"))
				s_Flags.Insert(cls);
		}
		DZS_Log.Info("properties: " + s_Flags.Count() + " flag classes");
		return s_Flags;
	}

	// The flag a customString names (compared without case), in the game's spelling, or "".
	static string FlagOf(string custom)
	{
		if (custom == "")
			return "";
		string low = custom;
		low.ToLower();
		array<string> flags = Flags();
		foreach (string f : flags)
		{
			string fl = f;
			fl.ToLower();
			if (fl == low)
				return f;
		}
		return "";
	}

	// Splits at ";" and keeps empty pieces (the game's Split drops them).
	static array<string> Entries(string s)
	{
		array<string> r = new array<string>();
		int start = 0;
		int n = s.Length();
		for (int i = 0; i < n; i++)
		{
			if (s.Get(i) == ";")
			{
				r.Insert(s.Substring(start, i - start));
				start = i + 1;
			}
		}
		r.Insert(s.Substring(start, n - start));
		return r;
	}

	static string KeyOfEntry(string e)
	{
		int eq = e.IndexOf("=");
		if (eq < 0)
			return e;
		return e.Substring(0, eq);
	}

	static string ValueOfEntry(string e)
	{
		int eq = e.IndexOf("=");
		if (eq < 0)
			return "";
		return e.Substring(eq + 1, e.Length() - eq - 1);
	}

	// Value of the first undergroundTriggerTag entry, or "".
	static string TagOf(string custom)
	{
		if (custom == "")
			return "";
		array<string> es = Entries(custom);
		foreach (string e : es)
		{
			if (KeyOfEntry(e) == TAG_KEY)
				return ValueOfEntry(e);
		}
		return "";
	}

	// Only this value changes; other entries, their order and separators stay character for character.
	static string WithTag(string custom, string value)
	{
		array<string> es = Entries(custom);
		int at = -1;
		for (int i = 0; i < es.Count(); i++)
		{
			if (KeyOfEntry(es[i]) == TAG_KEY)
			{
				at = i;
				break;
			}
		}
		if (value == "")
		{
			if (at < 0)
				return custom;
			es.RemoveOrdered(at);
			return Join(es);
		}
		string entry = TAG_KEY + "=" + value;
		if (at >= 0)
		{
			es[at] = entry;
			return Join(es);
		}
		if (custom == "")
			return entry;
		int last = custom.Length() - 1;
		if (custom.Get(last) == ";")
			return custom + entry;
		return custom + ";" + entry;
	}

	static string Join(array<string> es)
	{
		string r = "";
		for (int i = 0; i < es.Count(); i++)
		{
			if (i > 0)
				r += ";";
			r += es[i];
		}
		return r;
	}

	static bool ValidTag(string v)
	{
		return v.IndexOf(";") < 0 && v.IndexOf("=") < 0;
	}

	// Tags other objects of the project already use that contain the typed text (no case), at most eight.
	static array<string> Suggestions(DZS_Instance inst, string typed, string exceptId)
	{
		array<string> r = new array<string>();
		if (!inst)
			return r;
		string low = typed;
		low.ToLower();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			array<string> ids = inst.GetChildren(g);
			foreach (string id : ids)
			{
				if (id == exceptId)
					continue;
				string custom;
				inst.GetField(id, DZS_Obj.CUSTOM, custom);
				if (custom == "")
					continue;
				string tag = TagOf(custom);
				if (tag == "" || r.Find(tag) >= 0)
					continue;
				string tl = tag;
				tl.ToLower();
				if (low != "" && tl.IndexOf(low) < 0)
					continue;
				r.Insert(tag);
				if (r.Count() >= 8)
					return r;
			}
		}
		return r;
	}
}

// Text broken into lines at spaces and "\n" (the game's text widgets draw one line).
class DZS_WrapText : Managed
{
	protected Widget m_Parent;
	protected string m_Role;
	protected float m_Size;
	protected int m_Color;
	protected bool m_Centered;
	protected string m_Text;
	protected ref array<TextWidget> m_Lines;
	protected bool m_Shown = true;

	void DZS_WrapText(Widget parent, string role, float size, int color, bool centered = false)
	{
		m_Parent = parent;
		m_Role = role;
		m_Size = size;
		m_Color = color;
		m_Centered = centered;
		m_Lines = new array<TextWidget>();
	}

	void SetText(string t) { m_Text = t; }
	string GetText() { return m_Text; }

	void SetColor(int c)
	{
		m_Color = c;
		foreach (TextWidget l : m_Lines)
			l.SetColor(c);
	}

	void Show(bool s)
	{
		m_Shown = s;
		foreach (TextWidget l : m_Lines)
			l.Show(s);
	}

	bool IsShown() { return m_Shown; }

	protected TextWidget Line(int i)
	{
		while (m_Lines.Count() <= i)
			m_Lines.Insert(DZS_W.Text(m_Parent, m_Role, m_Size, m_Color, m_Centered));
		return m_Lines[i];
	}

	// Lays the lines out from y; returns the height used.
	float Place(float x, float y, float width)
	{
		float lh = DZS_Font.WidgetHeight(m_Role, m_Size) + DZS_Scale.Px(2);
		int used = 0;
		array<string> paras = new array<string>();
		string rest = m_Text;
		int nl = rest.IndexOf("\n");
		while (nl >= 0)
		{
			paras.Insert(rest.Substring(0, nl));
			rest = rest.Substring(nl + 1, rest.Length() - nl - 1);
			nl = rest.IndexOf("\n");
		}
		paras.Insert(rest);
		if (m_Text == "")
			paras.Clear();
		foreach (string p : paras)
		{
			array<string> words = new array<string>();
			p.Split(" ", words);
			string cur = "";
			foreach (string w : words)
			{
				string tryText = w;
				if (cur != "")
					tryText = cur + " " + w;
				TextWidget probe = Line(used);
				probe.SetText(DZS_W.Literal(tryText));
				if (cur != "" && DZS_W.TextWidth(probe) > width)
				{
					probe.SetText(DZS_W.Literal(cur));
					DZS_W.Place(probe, x, y + used * lh, width, lh);
					used++;
					cur = w;
				}
				else
					cur = tryText;
			}
			TextWidget last = Line(used);
			last.SetText(DZS_W.Literal(cur));
			DZS_W.Place(last, x, y + used * lh, width, lh);
			used++;
		}
		for (int i = 0; i < m_Lines.Count(); i++)
			m_Lines[i].Show(m_Shown && i < used);
		return used * lh;
	}
}

class DZS_HitCall : DZS_HitListener
{
	ref ScriptInvoker m_OnClick;

	void DZS_HitCall()
	{
		m_OnClick = new ScriptInvoker();
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_OnClick.Invoke(h);
	}
}

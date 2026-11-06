// Test items at loot points: the bundled test list (five classes per
// category by nominal in the vanilla types.xml of Chernarus, made by editor/mod-tools/loot/build_testlist.py), the
// dialog "Testgegenstand" with category chips, tiles and a class search.
class DZS_LootTestList
{
	static const string IN_MOD = "dzstudio/data/testliste.txt";
	protected static ref array<string> s_Cats;
	protected static ref map<string, ref array<string>> s_Classes;

	protected static void Load()
	{
		s_Cats = new array<string>();
		s_Classes = new map<string, ref array<string>>();
		FileHandle fh = OpenFile(IN_MOD, FileMode.READ);
		if (!fh)
		{
			DZS_Log.Warn("loot: test list missing");
			return;
		}
		string line;
		while (FGets(fh, line) >= 0)
		{
			line.Trim();
			int tab = line.IndexOf("\t");
			string cat = line;
			string rest = "";
			if (tab >= 0)
			{
				cat = line.Substring(0, tab);
				rest = line.Substring(tab + 1, line.Length() - tab - 1);
			}
			if (cat == "")
				continue;
			array<string> parts = new array<string>();
			rest.Split(",", parts);
			array<string> known = new array<string>();
			foreach (string c : parts)
			{
				string spelled = DZS_ClassDir.Find(c);
				if (spelled != "")
					known.Insert(spelled);
			}
			s_Cats.Insert(cat);
			s_Classes.Set(cat, known);
		}
		CloseFile(fh);
		DZS_Log.Info("loot: test list " + s_Cats.Count() + " categories");
	}

	static array<string> Categories()
	{
		if (!s_Cats)
			Load();
		array<string> r = new array<string>();
		r.Copy(s_Cats);
		return r;
	}

	static array<string> Classes(string cat)
	{
		if (!s_Cats)
			Load();
		array<string> r = new array<string>();
		array<string> list = s_Classes.Get(cat);
		if (list)
			r.Copy(list);
		return r;
	}
}

class DZS_LootTestTile : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	string m_Class;
	protected DZS_LootTestDialog m_D;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected ImageWidget m_Icon;
	protected TextWidget m_Text;
	protected TextWidget m_Sub;

	void DZS_LootTestTile(Widget parent, DZS_LootTestDialog d)
	{
		m_D = d;
		m_Hit = new DZS_Hit(parent, this, "loot.testTile");
		m_Hit.m_FocusKind = "";
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("well"));
		m_Border = new DZS_Border(m_Hit.m_W, DZS_Color.Get("line"));
		m_Icon = DZS_W.Icon(m_Hit.m_W, "package", DZS_Color.Get("textMuted"));
		m_Text = DZS_W.Text(m_Hit.m_W, DZS_Font.READ, 12, DZS_Color.Get("text"), true);
		m_Sub = DZS_W.Text(m_Hit.m_W, DZS_Font.MONO, 9, DZS_Color.Get("textQuiet"), true);
	}

	void SetClass(string cls)
	{
		m_Class = cls;
		string shown = cls;
		string icon = "package";
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat)
		{
			shown = cat.ShownName(cls);
			icon = cat.IconOf(cls);
		}
		DZS_W.SetIcon(m_Icon, icon);
		DZS_W.FitText(m_Text, shown, DZS_Scale.Px(90));
		DZS_W.FitText(m_Sub, cls, DZS_Scale.Px(90));
	}

	void Show(bool s) { m_Hit.m_W.Show(s); }

	void Place(float x, float y, float size)
	{
		m_Hit.Place(x, y, size, size);
		DZS_W.Place(m_Bg, 0, 0, size, size);
		m_Border.Place(0, 0, size, size, 1);
		float ic = DZS_Scale.Px(28);
		DZS_W.Place(m_Icon, (size - ic) / 2, DZS_Scale.Px(16), ic, ic);
		DZS_W.Place(m_Text, DZS_Scale.Px(3), DZS_Scale.Px(52), size - DZS_Scale.Px(6), DZS_Scale.Px(18));
		DZS_W.Place(m_Sub, DZS_Scale.Px(3), DZS_Scale.Px(70), size - DZS_Scale.Px(6), DZS_Scale.Px(14));
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		m_D.Choose(m_Class);
	}
}

class DZS_LootSuggestRow : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	string m_Class;
	protected DZS_LootTestDialog m_D;
	protected Widget m_Tint;
	protected TextWidget m_Text;

	void DZS_LootSuggestRow(Widget parent, DZS_LootTestDialog d)
	{
		m_D = d;
		m_Hit = new DZS_Hit(parent, this, "loot.suggest");
		m_Hit.m_FocusKind = "";
		m_Tint = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accentTint"));
		m_Text = DZS_W.Text(m_Hit.m_W, DZS_Font.MONO, 12, DZS_Color.Get("text"));
	}

	void Place(string cls, float y, float width, bool marked)
	{
		m_Class = cls;
		m_Hit.m_W.Show(true);
		m_Hit.Place(0, y, width, DZS_Scale.Px(22));
		DZS_W.Place(m_Tint, 0, 0, width, DZS_Scale.Px(22));
		m_Tint.Show(marked || m_Hit.m_Hover);
		m_Text.SetText(cls);
		DZS_W.Place(m_Text, DZS_Scale.Px(6), 0, width - DZS_Scale.Px(12), DZS_Scale.Px(22));
	}

	void Hide() { m_Hit.m_W.Show(false); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		m_D.OnSuggest(m_Class);
	}
}

class DZS_LootTestDialog : DZS_Dialog
{
	protected DZS_LootEditor m_E;
	protected ref array<string> m_Cats = new array<string>();
	protected ref array<ref DZS_Chip> m_Chips = new array<ref DZS_Chip>();
	protected ref array<ref DZS_LootTestTile> m_Tiles = new array<ref DZS_LootTestTile>();
	protected ref array<ref DZS_LootSuggestRow> m_Rows = new array<ref DZS_LootSuggestRow>();
	protected ref array<string> m_Found = new array<string>();
	protected ref DZS_SearchField m_Search;
	protected string m_Cat;
	protected string m_Typed = "~";
	protected string m_Marked;
	protected string m_LastClick;
	protected float m_LastClickAt;
	protected float m_Now;
	protected bool m_EnterWas = true;
	protected bool m_UpWas = true;
	protected bool m_DownWas = true;
	protected static ref array<string> s_All;
	protected static ref array<string> s_Low;

	void DZS_LootTestDialog(DZS_LootEditor e, array<string> first)
	{
		m_E = e;
		foreach (string f : first)
		{
			if (m_Cats.Find(f) < 0)
				m_Cats.Insert(f);
		}
		array<string> rest = DZS_LootTestList.Categories();
		foreach (string r : rest)
		{
			if (m_Cats.Find(r) < 0)
				m_Cats.Insert(r);
		}
		if (m_Cats.Count() > 0)
			m_Cat = m_Cats[0];
		Setup(MEDIUM, DZS_Texts.Get("loot.row"), DZS_Texts.Pick("Testgegenstand", "Test item"), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("show", DZS_Texts.Pick("Anzeigen", "Show"), "main");
	}

	override void BuildContent(Widget inner)
	{
		foreach (string c : m_Cats)
		{
			DZS_Chip chip = new DZS_Chip(inner, c, c);
			chip.m_OnClick.Insert(OnChip);
			m_Chips.Insert(chip);
		}
		for (int i = 0; i < 5; i++)
		{
			DZS_LootTestTile t = new DZS_LootTestTile(inner, this);
			m_Tiles.Insert(t);
		}
		m_Search = new DZS_SearchField("loot.testSearch", DZS_Texts.Pick("Klassenname", "Class name"));
		m_Search.Build(inner);
		AddFocusField(m_Search);
		for (int k = 0; k < 20; k++)
		{
			DZS_LootSuggestRow row = new DZS_LootSuggestRow(inner, this);
			m_Rows.Insert(row);
		}
		AddFootFocus();
		DZS_Log.Info("loot: test dialog, categories " + m_Cats.Count() + ", first " + m_Cat);
	}

	void OnChip(DZS_Chip c)
	{
		m_Cat = c.m_Key;
		DZS_Log.Info("loot: test category " + m_Cat);
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		float x = 0;
		float chipH = DZS_Scale.Px(26);
		foreach (DZS_Chip chip : m_Chips)
		{
			float cw = chip.Width();
			if (x > 0 && x + cw > width)
			{
				x = 0;
				y += chipH + DZS_Scale.Px(6);
			}
			chip.Place(x, y);
			chip.SetOn(chip.m_Key == m_Cat);
			x += cw + DZS_Scale.Px(6);
		}
		y += chipH + DZS_Scale.Px(12);
		array<string> classes = DZS_LootTestList.Classes(m_Cat);
		float tile = DZS_Scale.Px(96);
		float gap = DZS_Scale.Px(8);
		for (int i = 0; i < m_Tiles.Count(); i++)
		{
			DZS_LootTestTile t = m_Tiles[i];
			if (i >= classes.Count())
			{
				t.Show(false);
				continue;
			}
			t.Show(true);
			if (t.m_Class != classes[i])
				t.SetClass(classes[i]);
			t.Place(i * (tile + gap), y, tile);
		}
		if (classes.Count() > 0)
			y += tile + DZS_Scale.Px(12);
		m_Search.SetWidth(width / DZS_Scale.Factor());
		m_Search.Place(0, y, DZS_Scale.Px(32));
		y += DZS_Scale.Px(38);
		for (int r = 0; r < m_Rows.Count(); r++)
		{
			if (r >= m_Found.Count())
			{
				m_Rows[r].Hide();
				continue;
			}
			string cls = m_Found[r];
			m_Rows[r].Place(cls, y, width, cls == m_Marked);
			y += DZS_Scale.Px(22);
		}
		return y;
	}

	// Classes containing the typed text (case ignored), those starting with it first, then alphabetically.
	protected void Find(string typed)
	{
		m_Found.Clear();
		if (DZS_Names.Chars(typed) < 2)
			return;
		if (!s_All)
		{
			s_All = new array<string>();
			s_Low = new array<string>();
			DZS_ClassDir.All(s_All);
			s_All.Sort();
			foreach (string a : s_All)
			{
				string low = a;
				low.ToLower();
				s_Low.Insert(low);
			}
		}
		array<string> later = new array<string>();
		for (int i = 0; i < s_All.Count(); i++)
		{
			int at = s_Low[i].IndexOf(typed);
			if (at == 0)
			{
				if (m_Found.Count() < 20)
					m_Found.Insert(s_All[i]);
			}
			else if (at > 0 && later.Count() < 20)
				later.Insert(s_All[i]);
		}
		foreach (string l : later)
		{
			if (m_Found.Count() >= 20)
				break;
			m_Found.Insert(l);
		}
	}

	override void UpdateContent(float dt)
	{
		m_Now += dt;
		Keys();
		string typed = m_Search.GetTyped();
		typed.ToLower();
		if (typed == m_Typed)
			return;
		m_Typed = typed;
		Find(typed);
		m_Marked = "";
		if (m_Found.Count() > 0)
			m_Marked = m_Found[0];
		DZS_Log.Info("loot: test search '" + typed + "' " + m_Found.Count() + " suggestion(s)");
	}

	// Enter on a suggestion and ↑/↓ through the list while the search field holds the keys.
	protected void Keys()
	{
		bool enter = KeyState(KeyCode.KC_RETURN) != 0 || KeyState(KeyCode.KC_NUMPADENTER) != 0;
		bool up = KeyState(KeyCode.KC_UP) != 0;
		bool down = KeyState(KeyCode.KC_DOWN) != 0;
		bool enterDown = enter && !m_EnterWas;
		bool upDown = up && !m_UpWas;
		bool downDown = down && !m_DownWas;
		m_EnterWas = enter;
		m_UpWas = up;
		m_DownWas = down;
		if (m_Found.Count() == 0)
			return;
		int at = m_Found.Find(m_Marked);
		if (downDown)
			m_Marked = m_Found[Math.Min(at + 1, m_Found.Count() - 1)];
		else if (upDown)
			m_Marked = m_Found[Math.Max(at - 1, 0)];
		else if (enterDown && m_Marked != "")
			Choose(m_Marked);
	}

	void OnSuggest(string cls)
	{
		if (m_LastClick == cls && m_Now - m_LastClickAt < 0.4)
		{
			Choose(cls);
			return;
		}
		m_LastClick = cls;
		m_LastClickAt = m_Now;
		m_Marked = cls;
	}

	void Choose(string cls)
	{
		Close();
		m_E.PlaceTests(cls);
	}

	override void OnFoot(string tag)
	{
		if (tag == "show" && m_Marked != "")
		{
			Choose(m_Marked);
			return;
		}
		Close();
	}
}

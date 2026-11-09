// Section "Loot entry" of the project bar during loot editing:
// head with state, scope, maximum, usages and values, containers (the active one opened
// with name, maximum, categories and tags), preserved parts, hints, the selected points and the buttons. Rebuilt
// whenever the entry or the point selection changes.
class DZS_LootRowHit : DZS_HitListener
{
	DZS_LootEntryPanel m_Panel;
	ref DZS_Hit m_Hit;
	// the container's identity (m_Uid)
	string m_Uid;
	protected float m_Last = -10;

	void DZS_LootRowHit(Widget parent, DZS_LootEntryPanel p, string uid)
	{
		m_Panel = p;
		m_Uid = uid;
		m_Hit = new DZS_Hit(parent, this, "lootrow");
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		float now = GetGame().GetTickTime();
		bool dbl = now - m_Last < 0.4;
		m_Last = now;
		m_Panel.OnContainerRow(m_Uid, dbl);
	}
}

class DZS_LootEntryPanel : DZS_PanelContent
{
	protected DZS_LootEditor m_E;
	protected Widget m_Parent;
	protected Widget m_Root;
	protected int m_BuiltGen = -1;
	protected float m_Width;
	protected float m_H;
	protected ref array<ref DZS_Button> m_Buttons = new array<ref DZS_Button>();
	protected ref array<ref DZS_NumberField> m_Fields = new array<ref DZS_NumberField>();
	protected ref array<ref DZS_LootRowHit> m_RowHits = new array<ref DZS_LootRowHit>();
	protected ref array<ref DZS_Chip> m_Chips = new array<ref DZS_Chip>();
	protected float m_Y;
	protected float m_Scroll;
	protected float m_ViewH;
	// what a "+ add" menu adds to: "<kind>|<container uid>", kind "usage", "value", "category", "tag"
	protected string m_AddKind;

	void DZS_LootEntryPanel(DZS_LootEditor e)
	{
		m_E = e;
	}

	override void Build(Widget parent)
	{
		m_Parent = parent;
	}

	override bool FillsView() { return true; }

	override void SetView(float width, float height)
	{
		bool changed = Math.AbsFloat(width - m_Width) > 0.5 || Math.AbsFloat(height - m_ViewH) > 0.5;
		m_Width = width;
		m_ViewH = height;
		Rebuild(changed);
	}

	override bool Wheel(int delta)
	{
		m_Scroll = Math.Clamp(m_Scroll - delta * DZS_Scale.Px(40), 0, Math.Max(0, m_H - m_ViewH));
		Rebuild(true);
		return true;
	}

	override float Layout(float width)
	{
		bool changed = Math.AbsFloat(width - m_Width) > 0.5;
		m_Width = width;
		Rebuild(changed);
		return m_H;
	}

	override void Update(float dt)
	{
		if (m_E.IsOn() && m_BuiltGen != m_E.m_Gen)
			Rebuild(false);
		foreach (DZS_NumberField f : m_Fields)
			f.Update(dt);
	}

	protected bool AnyFocused()
	{
		foreach (DZS_NumberField f : m_Fields)
		{
			if (f.IsFocused() || f.IsDragging())
				return true;
		}
		return false;
	}

	protected void Clear()
	{
		foreach (DZS_Button b : m_Buttons)
			b.Destroy();
		foreach (DZS_NumberField f : m_Fields)
			f.Destroy();
		foreach (DZS_LootRowHit h : m_RowHits)
			h.m_Hit.Destroy();
		m_Buttons.Clear();
		m_Fields.Clear();
		m_RowHits.Clear();
		m_Chips.Clear();
		if (m_Root)
			m_Root.Unlink();
		m_Root = null;
	}

	protected void Rebuild(bool force)
	{
		if (!m_Parent || m_Width <= 0)
			return;
		if (!force && m_BuiltGen == m_E.m_Gen)
			return;
		if (AnyFocused() && !force)
			return;
		m_BuiltGen = m_E.m_Gen;
		Clear();
		m_Root = DZS_W.Frame(m_Parent);
		if (!m_E.IsOn() || !m_E.m_Entry)
			return;
		DZS_Instance inst = GetDZS().GetShown();
		if (!inst)
			return;
		m_Y = DZS_Scale.Px(8) - m_Scroll;
		BuildHead(inst);
		BuildLootmax("", m_E.m_Entry.m_Lootmax, m_E.m_P.m_Files.m_DefGroup, DZS_Texts.Pick("Höchstzahl im Gebäude", "Maximum in the building"));
		NameList("value", DZS_Texts.Pick("Stufen", "Tiers"), m_E.m_Entry.m_Values, "");
		NameList("usage", DZS_Texts.Pick("Arten des Orts", "Kinds of place"), m_E.m_Entry.m_Usages, "");
		BuildContainers();
		BuildKept();
		BuildHints(inst);
		BuildPoints();
		BuildButtons(inst);
		m_H = m_Y + m_Scroll + DZS_Scale.Px(8);
		DZS_W.Place(m_Root, 0, 0, m_Width, Math.Max(m_H, m_ViewH));
	}

	protected DZS_Kernel GetDZS()
	{
		return DZS_Kernel.Get();
	}

	protected float Pad() { return DZS_Scale.Px(12); }

	// A line too wide for the section continues on further lines, broken between words.
	protected TextWidget Line(string text, string role, float size, string color, float h = 20)
	{
		float room = m_Width - 2 * Pad();
		TextWidget t = DZS_W.Text(m_Root, role, size, DZS_Color.Get(color));
		t.SetText(DZS_W.Literal(text));
		if (DZS_W.TextWidth(t) > room && text.IndexOf(" ") > 0)
		{
			array<string> words = new array<string>();
			text.Split(" ", words);
			string line = "";
			TextWidget first = null;
			foreach (string w : words)
			{
				string tryLine = w;
				if (line != "")
					tryLine = line + " " + w;
				t.SetText(DZS_W.Literal(tryLine));
				if (line != "" && DZS_W.TextWidth(t) > room)
				{
					t.SetText(DZS_W.Literal(line));
					DZS_W.Place(t, Pad(), m_Y, room, DZS_Scale.Px(h));
					m_Y += DZS_Scale.Px(h);
					if (!first)
						first = t;
					t = DZS_W.Text(m_Root, role, size, DZS_Color.Get(color));
					line = w;
				}
				else
					line = tryLine;
			}
			t.SetText(DZS_W.Literal(line));
			DZS_W.Place(t, Pad(), m_Y, room, DZS_Scale.Px(h));
			m_Y += DZS_Scale.Px(h);
			if (first)
				return first;
			return t;
		}
		DZS_W.Place(t, Pad(), m_Y, room, DZS_Scale.Px(h));
		m_Y += DZS_Scale.Px(h);
		return t;
	}

	protected DZS_Button Btn(string kind, string label, string icon, float x, float y, float h = 24)
	{
		DZS_Button b = new DZS_Button(m_Root, kind, label, icon, h);
		b.Place(x, y);
		m_Buttons.Insert(b);
		return b;
	}

	protected void BuildHead(DZS_Instance inst)
	{
		string type = m_E.m_Type;
		string mark = m_E.m_P.OriginText(inst, type);
		TextWidget name = Line(m_E.m_P.ShownName(type), DZS_Font.READBOLD, 15, "text", 22);
		TextWidget badge = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("accent"), false, true);
		badge.SetText(mark);
		DZS_W.Place(badge, Pad(), m_Y - DZS_Scale.Px(22), m_Width - 2 * Pad(), DZS_Scale.Px(22));
		Line(type, DZS_Font.MONO, 12, "textQuiet", 18);
		int pts, mapRows, inProj;
		m_E.m_P.Facts(inst, type, pts, mapRows, inProj);
		string mapText = DZS_LootPlugin.Plural(mapRows, "Gebäude", "Gebäude", "building", "buildings");
		string projText = DZS_LootPlugin.Plural(inProj, "Objekt", "Objekte", "object", "objects");
		string g1 = DZS_Texts.Pick("betrifft ", "affects ");
		string g2 = DZS_Texts.Pick(" auf der Karte und ", " on the map and ");
		string g3 = DZS_Texts.Pick(" im Projekt", " in the project");
		Line(g1 + mapText + g2 + projText + g3, DZS_Font.READ, 13, "text", 22);
		bool foreign;
		m_E.m_P.Origin(inst, type, foreign);
		string other;
		if (foreign && m_E.m_P.StartMark(type, other))
		{
			Line(DZS_Texts.Pick("Markierung eines anderen Projekts", "Mark of another project"), DZS_Font.READ, 13, "warning", 20);
			string shownMark = other;
			if (other == "")
				shownMark = DZS_Texts.Pick("(leer)", "(empty)");
			Line("dzstudio=\"" + shownMark + "\"", DZS_Font.MONO, 12, "warning", 18);
		}
		m_Y += DZS_Scale.Px(4);
	}

	// Maximum: integer 0–9999, empty = default; tag = the container's identity, "" = the entry.
	protected void BuildLootmax(string tag, string value, int def, string label)
	{
		TextWidget l = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		l.SetText(label);
		float rowH = DZS_Scale.Px(26);
		DZS_W.Place(l, Pad(), m_Y, m_Width * 0.55, rowH);
		DZS_NumberField f = new DZS_NumberField("loot.lootmax" + tag, "", "textMuted", 110);
		f.Build(m_Root);
		f.m_Tag = tag;
		f.m_OnCommit.Insert(OnLootmax);
		string shown = value;
		if (value == "")
			shown = DZS_Texts.Pick("Standard (", "Default (") + def + ")";
		f.ShowValue(shown);
		f.Place(m_Width - Pad() - f.GetWidth(), m_Y, rowH);
		m_Fields.Insert(f);
		if (value != "")
		{
			DZS_Button r = Btn("ghost", "", "undo-2", m_Width - Pad() - f.GetWidth() - DZS_Scale.Px(26), m_Y + DZS_Scale.Px(1));
			r.SetTip(DZS_Texts.Pick("Auf Standard", "To default"));
			r.m_Tag = tag;
			r.m_OnClick.Insert(OnLootmaxReset);
		}
		m_Y += rowH + DZS_Scale.Px(4);
	}

	void OnLootmax(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		string t = typed;
		t.Trim();
		DZS_LootEEntry e = m_E.Copy();
		string v = "";
		if (t != "" && t.IndexOf("Standard") < 0 && t.IndexOf("Default") < 0)
		{
			float num;
			r.m_Ok = DZS_Num.Parse(t, num);
			if (!r.m_Ok)
				return;
			int n = Math.Round(num);
			n = Math.Clamp(n, 0, 9999);
			v = n.ToString();
		}
		r.m_Ok = true;
		if (!SetLootmax(e, f.m_Tag, v))
			return;
		m_E.Change(e);
	}

	void OnLootmaxReset(DZS_Button b)
	{
		DZS_LootEEntry e = m_E.Copy();
		if (SetLootmax(e, b.m_Tag, ""))
			m_E.Change(e);
	}

	protected bool SetLootmax(DZS_LootEEntry e, string tag, string v)
	{
		if (tag == "")
		{
			if (e.m_Lootmax == v)
				return false;
			e.m_Lootmax = v;
			return true;
		}
		DZS_LootEContainer lc = e.ContainerByUid(tag);
		if (!lc || lc.m_Lootmax == v)
			return false;
		lc.m_Lootmax = v;
		return true;
	}

	// A list of names with "×" and "+ add"; container = its identity, "" = the entry's list. A name is held by itself.
	protected void NameList(string kind, string title, array<string> names, string container)
	{
		float x0 = Pad();
		if (container != "")
			x0 += DZS_Scale.Px(14);
		TextWidget t = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		t.SetText(title);
		DZS_W.Place(t, x0, m_Y, m_Width, DZS_Scale.Px(18));
		m_Y += DZS_Scale.Px(18);
		array<string> list = ListOf(kind);
		for (int i = 0; i < names.Count(); i++)
		{
			string nm = names[i];
			bool known = list.Count() == 0 || list.Find(nm) >= 0;
			string color = "text";
			if (!known)
				color = "warning";
			TextWidget n = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get(color));
			string shown = nm;
			if (!known)
				shown = "⚠ " + nm;
			n.SetText(DZS_W.Literal(shown));
			DZS_W.Place(n, x0 + DZS_Scale.Px(6), m_Y, m_Width - x0 - DZS_Scale.Px(40), DZS_Scale.Px(22));
			DZS_Button x = Btn("ghost", "×", "", m_Width - Pad() - DZS_Scale.Px(24), m_Y);
			x.m_Tag = kind + "|" + container + "|" + nm;
			x.m_OnClick.Insert(OnRemoveName);
			m_Y += DZS_Scale.Px(22);
		}
		DZS_Button add = Btn("ghost", DZS_Texts.Pick("+ hinzufügen", "+ add"), "", x0, m_Y);
		add.m_Tag = kind + "|" + container;
		add.m_OnClick.Insert(OnAddName);
		bool any = false;
		foreach (string cand : list)
		{
			if (names.Find(cand) < 0)
				any = true;
		}
		if (!any)
			add.SetEnabled(false, DZS_Texts.Pick("Alle Namen der Liste sind gesetzt", "All names of the list are set"));
		m_Y += DZS_Scale.Px(26);
	}

	protected array<string> ListOf(string kind)
	{
		DZS_LootFiles f = m_E.m_P.m_Files;
		DZS_LootFiles b = m_E.m_P.m_Base;
		array<string> r = null;
		if (kind == "usage")
			r = f.m_Usages;
		else if (kind == "value")
			r = f.m_Values;
		else if (kind == "category")
			r = f.m_Categories;
		else
			r = f.m_Tags;
		if (r.Count() == 0 && b)
		{
			if (kind == "usage")
				r = b.m_Usages;
			else if (kind == "value")
				r = b.m_Values;
			else if (kind == "category")
				r = b.m_Categories;
			else
				r = b.m_Tags;
		}
		return r;
	}

	protected array<string> NamesIn(DZS_LootEEntry e, string kind, string cuid)
	{
		if (kind == "usage")
			return e.m_Usages;
		if (kind == "value")
			return e.m_Values;
		DZS_LootEContainer lc = e.ContainerByUid(cuid);
		if (!lc)
			return null;
		if (kind == "category")
			return lc.m_Cats;
		return lc.m_Tags;
	}

	// "<kind>|<container uid>|<name>" (the name may contain "|"); missing parts stay "".
	protected static void TagParts(string tag, out string kind, out string cuid, out string nm)
	{
		kind = tag;
		cuid = "";
		nm = "";
		int a = tag.IndexOf("|");
		if (a < 0)
			return;
		kind = tag.Substring(0, a);
		string rest = tag.Substring(a + 1, tag.Length() - a - 1);
		int b = rest.IndexOf("|");
		if (b < 0)
		{
			cuid = rest;
			return;
		}
		cuid = rest.Substring(0, b);
		nm = rest.Substring(b + 1, rest.Length() - b - 1);
	}

	void OnRemoveName(DZS_Button b)
	{
		string kind, cuid, nm;
		TagParts(b.m_Tag, kind, cuid, nm);
		DZS_LootEEntry e = m_E.Copy();
		array<string> names = NamesIn(e, kind, cuid);
		if (!names)
			return;
		int i = names.Find(nm);
		if (i < 0)
			return;
		names.RemoveOrdered(i);
		m_E.Change(e);
	}

	void OnAddName(DZS_Button b)
	{
		m_AddKind = b.m_Tag;
		string kind, cuid, nm;
		TagParts(b.m_Tag, kind, cuid, nm);
		array<string> names = NamesIn(m_E.m_Entry, kind, cuid);
		array<string> list = ListOf(kind);
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		foreach (string n : list)
		{
			if (names && names.Find(n) >= 0)
				continue;
			DZS_MenuItem it = new DZS_MenuItem();
			it.m_Text = n;
			it.m_Tag = n;
			items.Insert(it);
		}
		DZS_Popup pop = GetDZS().GetScreen().OpenContextMenu(items);
		if (pop)
			pop.m_OnChoose.Insert(OnNameChosen);
	}

	void OnNameChosen(DZS_Popup pop, DZS_MenuItem it)
	{
		string kind, cuid, nm;
		TagParts(m_AddKind, kind, cuid, nm);
		DZS_LootEEntry e = m_E.Copy();
		array<string> names = NamesIn(e, kind, cuid);
		if (!names || names.Find(it.m_Tag) >= 0)
			return;
		names.Insert(it.m_Tag);
		DZS_Log.Info("loot: add " + kind + " " + it.m_Tag);
		m_E.Change(e);
	}

	protected void BuildContainers()
	{
		TextWidget t = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		t.SetText(DZS_Texts.Pick("Container", "Containers"));
		DZS_W.Place(t, Pad(), m_Y, m_Width, DZS_Scale.Px(18));
		m_Y += DZS_Scale.Px(18);
		DZS_LootEEntry e = m_E.m_Entry;
		int defC = m_E.m_P.m_Files.m_DefContainer;
		for (int c = 0; c < e.m_Containers.Count(); c++)
		{
			DZS_LootEContainer lc = e.m_Containers[c];
			float rowH = DZS_Scale.Px(26);
			DZS_LootRowHit rh = new DZS_LootRowHit(m_Root, this, lc.m_Uid);
			rh.m_Hit.Place(0, m_Y, m_Width - Pad() - DZS_Scale.Px(28), rowH);
			m_RowHits.Insert(rh);
			bool active = lc.m_Uid == m_E.m_ActiveUid;
			if (active)
			{
				Widget bar = DZS_W.Panel(m_Root, DZS_Color.Get("accent"));
				DZS_W.Place(bar, 0, m_Y, DZS_Scale.Px(3), rowH);
			}
			int col = DZS_LootPlugin.ContainerColor(c);
			Widget sq = DZS_W.Panel(m_Root, col);
			float sqs = DZS_Scale.Px(8);
			DZS_W.Place(sq, Pad(), m_Y + (rowH - sqs) / 2, sqs, sqs);
			TextWidget nm = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("text"));
			nm.SetText(DZS_W.Literal(lc.m_Name));
			DZS_W.Place(nm, Pad() + DZS_Scale.Px(14), m_Y, m_Width * 0.4, rowH);
			string most = lc.m_Lootmax;
			if (most == "")
				most = DZS_Texts.Pick("Standard (", "default (") + defC + ")";
			string info = DZS_LootPlugin.Plural(lc.m_Points.Count(), "Punkt", "Punkte", "point", "points") + DZS_Texts.Pick(" · höchstens ", " · at most ") + most;
			if (active)
				info = DZS_Texts.Pick("aktiv · ", "active · ") + info;
			TextWidget qi = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"), false, true);
			qi.SetText(DZS_W.Literal(info));
			DZS_W.Place(qi, m_Width * 0.35, m_Y, m_Width * 0.65 - Pad() - DZS_Scale.Px(30), rowH);
			DZS_Button x = Btn("ghost", "×", "", m_Width - Pad() - DZS_Scale.Px(24), m_Y + DZS_Scale.Px(1));
			x.m_Tag = lc.m_Uid;
			x.m_OnClick.Insert(OnDeleteContainer);
			m_Y += rowH;
			if (active)
			{
				BuildContainerName(lc);
				BuildLootmax(lc.m_Uid, lc.m_Lootmax, defC, DZS_Texts.Pick("Höchstzahl", "Maximum"));
				NameList("category", DZS_Texts.Pick("Kategorien", "Categories"), lc.m_Cats, lc.m_Uid);
				NameList("tag", DZS_Texts.Pick("Tags", "Tags"), lc.m_Tags, lc.m_Uid);
			}
		}
		DZS_Button add = Btn("ghost", DZS_Texts.Pick("+ Container", "+ Container"), "", Pad(), m_Y);
		add.m_OnClick.Insert(OnAddContainer);
		m_Y += DZS_Scale.Px(30);
	}

	protected void BuildContainerName(DZS_LootEContainer lc)
	{
		float rowH = DZS_Scale.Px(26);
		TextWidget l = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		l.SetText(DZS_Texts.Pick("Name", "Name"));
		DZS_W.Place(l, Pad() + DZS_Scale.Px(14), m_Y, m_Width * 0.3, rowH);
		DZS_TextField f = new DZS_TextField("loot.cname", "", "textMuted", 160);
		f.Build(m_Root);
		f.m_Tag = lc.m_Uid;
		f.ShowValue(lc.m_Name);
		f.m_OnCommit.Insert(OnContainerName);
		f.Place(m_Width - Pad() - f.GetWidth(), m_Y, rowH);
		m_Fields.Insert(f);
		m_Y += rowH + DZS_Scale.Px(4);
	}

	void OnContainerName(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		string t = typed;
		t.Trim();
		DZS_LootEEntry e = m_E.Copy();
		DZS_LootEContainer lc = e.ContainerByUid(f.m_Tag);
		if (!lc)
			return;
		if (t == "" || t == lc.m_Name)
		{
			r.m_Ok = true;
			return;
		}
		bool bad = DZS_Names.Chars(t) > 64 || t.IndexOf("\"") >= 0 || t.IndexOf("<") >= 0 || t.IndexOf(">") >= 0 || t.IndexOf("&") >= 0;
		if (bad)
		{
			r.m_Ok = false;
			DZS_Log.Info("loot: container name '" + t + "' not taken");
			return;
		}
		r.m_Ok = true;
		lc.m_Name = t;
		m_E.Change(e);
	}

	void OnContainerRow(string uid, bool dbl)
	{
		if (dbl)
			m_E.SelectContainer(uid);
		else
		{
			if (!m_E.m_Entry || !m_E.m_Entry.ContainerByUid(uid))
				return;
			m_E.m_ActiveUid = uid;
			m_E.m_Gen++;
			DZS_Log.Info("loot: active container " + uid);
		}
	}

	void OnDeleteContainer(DZS_Button b)
	{
		DZS_LootEEntry e = m_E.Copy();
		DZS_LootEContainer lc = e.ContainerByUid(b.m_Tag);
		if (!lc)
			return;
		DZS_Log.Info("loot: delete container " + lc.m_Name + " with " + lc.m_Points.Count() + " point(s)");
		int c = e.m_Containers.Find(lc);
		e.m_Containers.RemoveOrdered(c);
		m_E.m_Sel.Clear();
		m_E.Change(e);
	}

	void OnAddContainer(DZS_Button b)
	{
		DZS_LootEEntry e = m_E.Copy();
		array<string> wanted = {"lootFloor", "lootshelves", "lootweapons", "loot"};
		string name = "";
		foreach (string w : wanted)
		{
			if (!HasContainer(e, w))
			{
				name = w;
				break;
			}
		}
		int k = 2;
		while (name == "")
		{
			string cand = "loot" + k;
			if (!HasContainer(e, cand))
				name = cand;
			k++;
		}
		DZS_LootEContainer lc = new DZS_LootEContainer();
		lc.m_Name = name;
		lc.m_Uid = e.FreshUid();
		e.m_Containers.Insert(lc);
		m_E.m_ActiveUid = lc.m_Uid;
		DZS_Log.Info("loot: new container " + name);
		m_E.Change(e);
	}

	protected static bool HasContainer(DZS_LootEEntry e, string name)
	{
		string want = name;
		want.ToLower();
		foreach (DZS_LootEContainer c : e.m_Containers)
		{
			string n = c.m_Name;
			n.ToLower();
			if (n == want)
				return true;
		}
		return false;
	}

	protected void BuildKept()
	{
		DZS_LootEEntry e = m_E.m_Entry;
		if (e.m_Dispatch > 0)
			Line(e.m_Dispatch.ToString() + DZS_Texts.Pick(" feste Teile, nicht bearbeitbar", " fixed parts, not editable"), DZS_Font.READ, 12, "textQuiet", 18);
		int flagged = e.CountFlagged();
		if (flagged > 0)
			Line(flagged.ToString() + DZS_Texts.Pick(" Punkte mit Sonderwert (flags) — bleibt erhalten", " points with a special value (flags) — kept"), DZS_Font.READ, 12, "textQuiet", 18);
		int extra = e.m_Extra;
		foreach (DZS_LootEContainer c : e.m_Containers)
			extra += c.m_Extra;
		if (extra > 0)
			Line(DZS_Texts.Pick("Weitere Angaben aus der Datei bleiben erhalten", "Further parts of the file are kept"), DZS_Font.READ, 12, "textQuiet", 18);
	}

	protected void BuildHints(DZS_Instance inst)
	{
		TextWidget t = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		t.SetText(DZS_Texts.Pick("Hinweise", "Notes"));
		DZS_W.Place(t, Pad(), m_Y, m_Width, DZS_Scale.Px(18));
		m_Y += DZS_Scale.Px(18);
		array<string> hints = new array<string>();
		array<string> tones = new array<string>();
		DZS_LootEEntry e = m_E.m_Entry;
		string type = m_E.m_Type;
		if (!m_E.m_P.HasBase(type))
		{
			hints.Insert(DZS_Texts.Get("loot.noVanilla"));
			tones.Insert("textQuiet");
			int mapRows = m_E.m_P.m_Files.ForeignRows(type);
			if (mapRows > 0)
			{
				hints.Insert(mapRows.ToString() + DZS_Texts.Pick(" Kartengebäude dieses Typs bekommen keinen Loot — sie stehen nicht in mapgrouppos.xml", " map buildings of this type get no loot — they are not in mapgrouppos.xml"));
				tones.Insert("textQuiet");
			}
		}
		string st = m_E.m_P.Status(inst, type);
		bool own = st == "own" || st == "new";
		if (own && e.m_Usages.Count() == 0)
		{
			hints.Insert(DZS_Texts.Pick("Eintrag ohne Usage", "Entry without usage"));
			tones.Insert("textQuiet");
		}
		map<string, bool> seen = new map<string, bool>();
		foreach (DZS_LootEContainer c : e.m_Containers)
		{
			if (own && c.m_Cats.Count() == 0)
			{
				string tv6 = c.m_Name + DZS_Texts.Pick(": Container ohne Kategorie", ": container without category");
				hints.Insert(tv6);
				tones.Insert("textQuiet");
			}
			if (own && c.m_Points.Count() == 0)
			{
				string tv7 = c.m_Name + DZS_Texts.Pick(": Container ohne Punkte", ": container without points");
				hints.Insert(tv7);
				tones.Insert("textQuiet");
			}
			string low = c.m_Name;
			low.ToLower();
			if (seen.Contains(low))
			{
				hints.Insert(DZS_Texts.Pick("Zwei Container heißen ", "Two containers are named ") + c.m_Name);
				tones.Insert("textQuiet");
			}
			seen.Set(low, true);
		}
		array<string> outside = new array<string>();
		m_E.OutsidePoints(outside);
		int outN = outside.Count();
		int outLine = -1;
		if (outN > 0)
		{
			outLine = hints.Count();
			if (outN == 1)
				hints.Insert(DZS_Texts.Pick("1 Punkt liegt außerhalb des Gebäudes", "1 point lies outside the building"));
			else
				hints.Insert(outN.ToString() + DZS_Texts.Pick(" Punkte liegen außerhalb des Gebäudes", " points lie outside the building"));
			tones.Insert("textQuiet");
		}
		DZS_Instance i2 = inst;
		string sc;
		i2.GetField(m_E.m_Obj, DZS_Obj.SCALE, sc);
		float scale = sc.ToFloat();
		if (sc != "" && Math.AbsFloat(scale - 1) > 0.001)
		{
			hints.Insert(DZS_Texts.Pick("Loot-Punkte wachsen nicht mit der Größe des Objekts mit", "Loot points do not grow with the object’s size"));
			tones.Insert("textQuiet");
		}
		vector o, ypr;
		if (m_E.Frame(o, ypr) && (Math.AbsFloat(ypr[1]) > 3 || Math.AbsFloat(ypr[2]) > 3))
		{
			hints.Insert(DZS_Texts.Pick("Ob gekippte Gebäude ihre Punkte mitkippen, ist nicht geprüft", "Whether tilted buildings tilt their points is not checked"));
			tones.Insert("textQuiet");
		}
		if (hints.Count() == 0)
		{
			hints.Insert(DZS_Texts.Pick("Keine Hinweise", "No notes"));
			tones.Insert("textQuiet");
		}
		for (int h = 0; h < hints.Count(); h++)
		{
			if (h == outLine)
			{
				DZS_Button show = Btn("ghost", DZS_Texts.Pick("Zeigen", "Show"), "", m_Width - Pad() - DZS_Scale.Px(64), m_Y - DZS_Scale.Px(3), 22);
				show.m_OnClick.Insert(OnShowOutside);
			}
			Line(hints[h], DZS_Font.READ, 12, tones[h], 18);
		}
		m_Y += DZS_Scale.Px(6);
	}

	void OnShowOutside(DZS_Button b)
	{
		array<string> outside = new array<string>();
		m_E.OutsidePoints(outside);
		m_E.SelectKeys(outside);
	}

	protected void BuildPoints()
	{
		int n = m_E.m_Sel.Count();
		if (n == 0)
		{
			Line(DZS_Texts.Pick("Kein Punkt ausgewählt — Doppelklick auf eine Fläche setzt einen Punkt", "No point selected — double click on a surface sets a point"), DZS_Font.READ, 12, "textQuiet", 18);
			m_Y += DZS_Scale.Px(6);
			return;
		}
		DZS_LootEEntry e = m_E.m_Entry;
		string key = m_E.m_Sel.GetKey(0);
		DZS_LootEContainer lc = e.ContainerOfPoint(key);
		DZS_LootEPoint p = m_E.PointOf(key);
		if (!lc || !p)
			return;
		if (n == 1)
		{
			int total = e.CountPoints();
			// its place is only shown
			int idx = 0;
			bool found = false;
			foreach (DZS_LootEContainer ic : e.m_Containers)
			{
				foreach (DZS_LootEPoint ip : ic.m_Points)
				{
					if (found)
						break;
					idx++;
					if (ip == p)
						found = true;
				}
			}
			string w1 = DZS_Texts.Pick("Punkt ", "Point ");
			string w2 = DZS_Texts.Pick(" von ", " of ");
			Line(w1 + idx + w2 + total + " · " + lc.m_Name, DZS_Font.READBOLD, 13, "text", 22);
			vector lp = p.P();
			PointField("x", "X", -lp[2], 3);
			PointField("y", "Y", lp[1], 3);
			PointField("z", "Z", lp[0], 3);
		}
		else
			Line(n.ToString() + DZS_Texts.Pick(" Punkte", " points"), DZS_Font.READBOLD, 13, "text", 22);
		float range = p.Range();
		float height = p.Height();
		bool sameR = true;
		bool sameH = true;
		for (int s = 0; s < n; s++)
		{
			DZS_LootEPoint q = m_E.PointOf(m_E.m_Sel.GetKey(s));
			if (!q)
				continue;
			if (Math.AbsFloat(q.Range() - range) > 0.0005)
				sameR = false;
			if (Math.AbsFloat(q.Height() - height) > 0.0005)
				sameH = false;
		}
		PointFieldText("r", DZS_Texts.Pick("Radius", "Radius"), sameR, range);
		PointFieldText("h", DZS_Texts.Pick("Höhe", "Height"), sameH, height);
		if (n == 1)
		{
			vector w = m_E.WorldOfPoint(p);
			string wl = DZS_Texts.Pick("Welt ", "World ");
			string wx = DZS_Num.Fixed(w[0], 2);
			string wy = DZS_Num.Fixed(w[1], 2);
			string wz = DZS_Num.Fixed(w[2], 2);
			Line(wl + wx + " / " + wy + " / " + wz, DZS_Font.READ, 12, "textQuiet", 18);
			if (p.m_Flags != "")
			{
				string f1 = DZS_Texts.Pick("Sonderwert (flags) ", "Special value (flags) ");
				string f2 = DZS_Texts.Pick(" — bleibt erhalten", " — kept");
				Line(f1 + p.m_Flags + f2, DZS_Font.READ, 12, "textQuiet", 18);
			}
		}
		// container buttons: move the selected points to the end of that container
		float x = Pad();
		for (int cc = 0; cc < e.m_Containers.Count(); cc++)
		{
			DZS_Button cb = Btn("side", e.m_Containers[cc].m_Name, "", x, m_Y);
			cb.m_Tag = e.m_Containers[cc].m_Uid;
			cb.m_OnClick.Insert(OnMoveTo);
			x += cb.GetWidth() + DZS_Scale.Px(4);
			if (x > m_Width - DZS_Scale.Px(90))
			{
				x = Pad();
				m_Y += DZS_Scale.Px(28);
			}
		}
		m_Y += DZS_Scale.Px(32);
		DZS_Button tb = Btn("side", DZS_Texts.Pick("Testgegenstand …", "Test item …"), "package", Pad(), m_Y, 28);
		tb.m_OnClick.Insert(OnTestItem);
		m_Y += DZS_Scale.Px(34);
	}

	void OnTestItem(DZS_Button b)
	{
		m_E.OpenTestDialog();
	}

	void OnRemoveTests(DZS_Button b)
	{
		m_E.RemoveTests();
		DZS_WorldLines.Moved();
	}

	protected void PointField(string tag, string label, float v, int dec)
	{
		float rowH = DZS_Scale.Px(26);
		DZS_NumberField f = new DZS_NumberField("loot.p" + tag, label, "textMuted", 120);
		f.Build(m_Root);
		f.m_Tag = tag;
		f.ShowValue(DZS_Num.Fixed(v, dec));
		f.m_OnCommit.Insert(OnPointField);
		f.m_OnWheel.Insert(OnPointWheel);
		f.Place(Pad(), m_Y, rowH);
		m_Fields.Insert(f);
		m_Y += rowH + DZS_Scale.Px(2);
	}

	protected void PointFieldText(string tag, string label, bool same, float v)
	{
		float rowH = DZS_Scale.Px(26);
		TextWidget l = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		l.SetText(label);
		DZS_W.Place(l, Pad(), m_Y, m_Width * 0.4, rowH);
		DZS_NumberField f = new DZS_NumberField("loot.p" + tag, "", "textMuted", 120);
		f.Build(m_Root);
		f.m_Tag = tag;
		if (same)
			f.ShowValue(DZS_Num.Fixed(v, 3));
		else
			f.ShowValue("—");
		f.m_OnCommit.Insert(OnPointField);
		f.m_OnWheel.Insert(OnPointWheel);
		f.Place(m_Width - Pad() - f.GetWidth(), m_Y, rowH);
		m_Fields.Insert(f);
		m_Y += rowH + DZS_Scale.Px(2);
	}

	void OnPointField(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = DZS_Num.Parse(typed, v);
		if (!r.m_Ok)
			return;
		ApplyPointValue(f.m_Tag, v, false);
	}

	void OnPointWheel(DZS_NumberField f, int steps, bool fine)
	{
		float step = 0.1;
		if (f.m_Tag == "r" || f.m_Tag == "h")
			step = 0.05;
		if (fine)
			step = step / 10;
		ApplyPointValue(f.m_Tag, steps * step, true);
	}

	protected void ApplyPointValue(string tag, float v, bool relative)
	{
		DZS_LootEEntry e = m_E.Copy();
		for (int s = 0; s < m_E.m_Sel.Count(); s++)
		{
			DZS_LootEPoint p = e.PointByUid(m_E.m_Sel.GetKey(s));
			if (!p)
				continue;
			vector lp = p.P();
			if (tag == "r")
			{
				float nr = v;
				if (relative)
					nr = p.Range() + v;
				p.SetRange(Math.Clamp(nr, 0.01, 10));
				m_E.m_LastRange = p.Range();
			}
			else if (tag == "h")
			{
				float nh = v;
				if (relative)
					nh = p.Height() + v;
				p.SetHeight(Math.Clamp(nh, 0.01, 10));
				m_E.m_LastHeight = p.Height();
			}
			else
			{
				// fields show the model axes: X = -pz, Y = py, Z = px
				float mx = -lp[2];
				float my = lp[1];
				float mz = lp[0];
				if (tag == "x")
				{
					if (relative)
						mx += v;
					else
						mx = v;
				}
				else if (tag == "y")
				{
					if (relative)
						my += v;
					else
						my = v;
				}
				else
				{
					if (relative)
						mz += v;
					else
						mz = v;
				}
				mx = Math.Clamp(mx, -1000, 1000);
				my = Math.Clamp(my, -1000, 1000);
				mz = Math.Clamp(mz, -1000, 1000);
				p.SetP(Vector(mz, my, -mx));
			}
		}
		DZS_Log.Info("loot: point value " + tag + " " + v + " relative " + relative);
		m_E.Change(e);
	}

	// The selected points (in list order) to the end of the container with the button's identity.
	void OnMoveTo(DZS_Button b)
	{
		DZS_LootEEntry e = m_E.Copy();
		DZS_LootEContainer target = e.ContainerByUid(b.m_Tag);
		if (!target)
			return;
		array<ref DZS_LootEPoint> moved = new array<ref DZS_LootEPoint>();
		foreach (DZS_LootEContainer sc : e.m_Containers)
		{
			if (sc == target)
				continue;
			int i = 0;
			while (i < sc.m_Points.Count())
			{
				DZS_LootEPoint p = sc.m_Points[i];
				if (m_E.m_Sel.Contains(p.m_Uid))
				{
					moved.Insert(p);
					sc.m_Points.RemoveOrdered(i);
				}
				else
					i++;
			}
		}
		if (moved.Count() == 0)
			return;
		string sel = "";
		foreach (DZS_LootEPoint mp : moved)
		{
			mp.m_Src = -1;
			target.m_Points.Insert(mp);
			if (sel != "")
				sel += ",";
			sel += mp.m_Uid;
		}
		m_E.m_ActiveUid = target.m_Uid;
		DZS_Log.Info("loot: " + moved.Count() + " point(s) to " + target.m_Name);
		m_E.Change(e, sel);
	}

	protected void BuildButtons(DZS_Instance inst)
	{
		string type = m_E.m_Type;
		string st = m_E.m_P.Status(inst, type);
		float x = Pad();
		if (m_E.HasTests())
		{
			DZS_Button tr = Btn("side", DZS_Texts.Pick("Testgegenstände entfernen", "Remove test items"), "", x, m_Y, 28);
			tr.m_OnClick.Insert(OnRemoveTests);
			m_Y += DZS_Scale.Px(32);
		}
		if (st == "own" && m_E.m_P.HasBase(type))
		{
			DZS_Button r = Btn("side", DZS_Texts.Pick("Zurücksetzen …", "Reset …"), "", x, m_Y, 28);
			r.m_OnClick.Insert(OnReset);
			m_Y += DZS_Scale.Px(32);
		}
		bool foreign;
		string origin = m_E.m_P.Origin(inst, type, foreign);
		if (origin == "before" && m_E.m_P.HasVanilla(type))
		{
			DZS_Button v = Btn("side", DZS_Texts.Pick("Auf Vanilla setzen …", "Set to vanilla …"), "", x, m_Y, 28);
			v.m_OnClick.Insert(OnVanillaSet);
			m_Y += DZS_Scale.Px(32);
		}
		if (st == "new" && !UsedByObjects(inst, type))
		{
			DZS_Button d = Btn("danger", DZS_Texts.Pick("Eintrag entfernen", "Remove entry"), "", x, m_Y, 28);
			d.m_OnClick.Insert(OnRemoveEntry);
			m_Y += DZS_Scale.Px(32);
		}
		DZS_Button done = Btn("main", DZS_Texts.Pick("Fertig", "Done"), "", x, m_Y, 28);
		done.m_OnClick.Insert(OnDone);
		m_Y += DZS_Scale.Px(34);
	}

	protected bool UsedByObjects(DZS_Instance inst, string type)
	{
		array<string> objs = m_E.m_P.ClassObjects(inst);
		string low = type;
		low.ToLower();
		foreach (string id : objs)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			name.ToLower();
			if (name == low && m_E.m_P.SettingOf(inst, id) != DZS_LootPlugin.NONE)
				return true;
		}
		return false;
	}

	void OnDone(DZS_Button b)
	{
		m_E.Stop("");
	}

	void OnReset(DZS_Button b)
	{
		DZS_Instance inst = GetDZS().GetShown();
		int pts, mapRows, inProj;
		m_E.m_P.Facts(inst, m_E.m_Type, pts, mapRows, inProj);
		string mapText = DZS_LootPlugin.Plural(mapRows, "Gebäude", "Gebäude", "building", "buildings");
		string projText = DZS_LootPlugin.Plural(inProj, "Objekt", "Objekte", "object", "objects");
		string q = DZS_Texts.Fmt("loot.resetQ", m_E.m_P.ShownName(m_E.m_Type), mapText, projText);
		DZS_LootTextDialog d = new DZS_LootTextDialog(DZS_Texts.Get("loot.row"), DZS_Texts.Pick("Zurücksetzen", "Reset"), q);
		d.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		d.AddFootButton("reset", DZS_Texts.Get("loot.resetGo"), "danger");
		d.m_OnResult.Insert(OnResetAnswer);
		d.Open();
	}

	void OnResetAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("loot: reset answer " + result);
		if (result != "reset")
			return;
		DZS_Instance inst = GetDZS().GetShown();
		string type = m_E.m_Type;
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		DZS_LootEEntry be = m_E.m_P.BaseEntry(type);
		m_E.m_P.SetEntry(b, inst, type, be, true);
		array<string> objs = m_E.m_P.ClassObjects(inst);
		string low = type;
		low.ToLower();
		foreach (string id : objs)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			name.ToLower();
			if (name == low && m_E.m_P.SettingOf(inst, id) == DZS_LootPlugin.OWN)
				b.SetCompanion(id, DZS_LootPlugin.SETTING, 1, DZS_LootPlugin.VANILLA);
		}
		string de = m_E.StepName("loot.stepReset", true);
		string en = m_E.StepName("loot.stepReset", false);
		DZS_SubmitResult r = b.Commit(de, en);
		m_E.m_Sel.Clear();
		DZS_Log.Info("loot: step '" + de + "' ok " + r.Ok());
	}

	void OnVanillaSet(DZS_Button b)
	{
		DZS_Instance inst = GetDZS().GetShown();
		int pts, mapRows, inProj;
		m_E.m_P.Facts(inst, m_E.m_Type, pts, mapRows, inProj);
		string mapText = DZS_LootPlugin.Plural(mapRows, "Gebäude", "Gebäude", "building", "buildings");
		string projText = DZS_LootPlugin.Plural(inProj, "Objekt", "Objekte", "object", "objects");
		string q = DZS_Texts.Fmt("loot.vanSetQ", m_E.m_P.ShownName(m_E.m_Type), mapText, projText);
		DZS_LootTextDialog d = new DZS_LootTextDialog(DZS_Texts.Get("loot.row"), DZS_Texts.Get("loot.vanSetGo"), q);
		d.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		d.AddFootButton("vanilla", DZS_Texts.Get("loot.vanSetGo"), "main");
		d.m_OnResult.Insert(OnVanillaSetAnswer);
		d.Open();
	}

	void OnVanillaSetAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("loot: set to vanilla answer " + result);
		if (result != "vanilla")
			return;
		DZS_Instance inst = GetDZS().GetShown();
		string type = m_E.m_Type;
		DZS_LootEEntry ve = m_E.m_P.VanillaEntry(type);
		if (!ve)
			return;
		foreach (DZS_LootEContainer c : ve.m_Containers)
		{
			c.m_Src = -1;
			foreach (DZS_LootEPoint p : c.m_Points)
				p.m_Src = -1;
		}
		DZS_OpBuilder ob = inst.BeginOperation("loot:edit", 1);
		m_E.m_P.SetEntry(ob, inst, type, ve, false, "vanilla");
		array<string> objs = m_E.m_P.ClassObjects(inst);
		string low = type;
		low.ToLower();
		foreach (string id : objs)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			name.ToLower();
			if (name == low && m_E.m_P.SettingOf(inst, id) == DZS_LootPlugin.VANILLA)
				ob.SetCompanion(id, DZS_LootPlugin.SETTING, 1, DZS_LootPlugin.OWN);
		}
		string de = m_E.StepName("loot.stepVanilla", true);
		string en = m_E.StepName("loot.stepVanilla", false);
		DZS_SubmitResult r = ob.Commit(de, en);
		m_E.m_Sel.Clear();
		DZS_Log.Info("loot: step '" + de + "' ok " + r.Ok());
	}

	void OnRemoveEntry(DZS_Button b)
	{
		DZS_Instance inst = GetDZS().GetShown();
		string type = m_E.m_Type;
		DZS_OpBuilder ob = inst.BeginOperation("loot:edit", 1);
		m_E.m_P.SetEntry(ob, inst, type, null);
		string de = m_E.StepName("loot.stepDelete", true);
		string en = m_E.StepName("loot.stepDelete", false);
		DZS_SubmitResult r = ob.Commit(de, en);
		DZS_Log.Info("loot: step '" + de + "' ok " + r.Ok());
		m_E.Stop("");
	}
}

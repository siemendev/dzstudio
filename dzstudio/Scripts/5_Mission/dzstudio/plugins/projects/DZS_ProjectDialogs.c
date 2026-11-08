// Project dialogs: New project, project browser, Rename, Save as, backups.

// A small dialog with one name field: empty with the placeholder; empty confirmed counts as the placeholder.
class DZS_NameDialog : DZS_Dialog
{
	protected ref DZS_TextField m_Field;
	protected TextWidget m_Label;
	protected TextWidget m_Info;
	protected string m_Placeholder;
	protected string m_InfoText;
	protected string m_MainTag;

	void SetupName(string kicker, string title, string placeholder, string info, string mainTag, string mainLabel)
	{
		m_Placeholder = placeholder;
		m_InfoText = info;
		m_MainTag = mainTag;
		Setup(SMALL, kicker, title, true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton(mainTag, mainLabel, "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Label = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Name", "Name")));
		m_Field = new DZS_TextField("projects.name", m_Placeholder, "textMuted", 372);
		m_Field.Build(inner);
		m_Field.m_OnCommit.Insert(OnFieldCommit);
		m_Info = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Info.SetText(m_InfoText);
		AddFocusField(m_Field);
		AddFootFocus();
	}

	void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		r.m_Ok = DZS_Names.ValidProjectName(typed);
	}

	string Value()
	{
		string t = m_Field.GetText();
		if (t == "")
			return m_Placeholder;
		return t;
	}

	override float LayoutContent(float width)
	{
		DZS_W.Place(m_Label, 0, 0, width, DZS_Scale.Px(14));
		m_Field.Place(0, DZS_Scale.Px(18), DZS_Scale.Px(28));
		DZS_W.Place(m_Info, 0, DZS_Scale.Px(56), width, DZS_Scale.Px(20));
		return DZS_Scale.Px(80);
	}

	override void UpdateContent(float dt)
	{
		m_Field.Refresh();
		DZS_Button main = FootButton(m_MainTag);
		bool ok = DZS_Names.ValidProjectName(Value());
		if (main && main.m_Hit.m_Enabled != ok)
			main.SetEnabled(ok, DZS_Texts.Pick("Name: 1–60 Zeichen, nicht nur Leerzeichen", "Name: 1–60 characters, not only spaces"));
	}
}

class DZS_NewProjectDialog : DZS_NameDialog
{
	protected DZS_ProjectsPlugin m_P;
	protected string m_Map;

	void DZS_NewProjectDialog(DZS_ProjectsPlugin p, string mapKey)
	{
		m_P = p;
		m_Map = mapKey;
		string tx124 = DZS_Texts.Pick("Projekt", "Project");
		string tx125 = DZS_Texts.Pick("Neues Projekt", "New project");
		string tx126 = DZS_Texts.Pick("Neues Projekt", "New project");
		string tx127 = DZS_Texts.Pick("Karte: ", "Map: ") + DZS_Maps.Display(mapKey);
		string tx128 = DZS_Texts.Pick("Anlegen", "Create");
		SetupName(tx124, tx125, tx126, tx127, "create", tx128);
	}

	override void OnFoot(string tag)
	{
		if (tag == "create")
		{
			string name = Value();
			if (!DZS_Names.ValidProjectName(name))
				return;
			Close();
			DZS_Log.Info("new project dialog: create '" + name + "' on " + m_Map);
			m_P.RequestNew(name, m_Map);
			return;
		}
		DZS_Log.Info("new project dialog: " + tag);
		Close();
	}
}

class DZS_SaveAsDialog : DZS_NameDialog
{
	protected DZS_ProjectsPlugin m_P;
	protected bool m_Copy;
	// Target: "" = local, else the id of a target of DZS_SaveTargets
	protected ref array<string> m_TargetIds = new array<string>();
	protected ref array<ref DZS_Chip> m_TargetChips = new array<ref DZS_Chip>();
	protected TextWidget m_TargetLabel;
	protected string m_Target;
	protected ref array<string> m_TargetNames = new array<string>();
	protected ref array<string> m_TargetStates = new array<string>();

	void DZS_SaveAsDialog(DZS_ProjectsPlugin p, string suggestion, bool copy = false)
	{
		m_P = p;
		m_Copy = copy;
		string tx120 = DZS_Texts.Pick("Projekt", "Project");
		string tx121 = DZS_Texts.Pick("Speichern unter", "Save as");
		if (copy)
			tx121 = DZS_Texts.Pick("Kopie speichern", "Save a copy");
		string tx122 = DZS_Texts.Pick("Neues lokales Projekt auf derselben Karte", "New local project on the same map");
		string tx123 = DZS_Texts.Pick("Speichern", "Save");
		SetupName(tx120, tx121, suggestion, tx122, "saveas", tx123);
	}

	override void BuildContent(Widget inner)
	{
		DZS_SaveTarget t = DZS_SaveTargets.Get();
		array<string> names = new array<string>();
		if (t && !m_Copy)
			t.Targets(m_TargetIds, names);
		if (m_TargetIds.Count() > 0)
		{
			m_TargetLabel = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
			m_TargetLabel.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Ziel", "Target")));
			m_TargetIds.InsertAt("", 0);
			names.InsertAt(DZS_Texts.Pick("Lokal", "Local"), 0);
			for (int i = 0; i < m_TargetIds.Count(); i++)
			{
				DZS_Chip c = new DZS_Chip(inner, m_TargetIds[i], names[i]);
				c.m_OnClick.Insert(OnTarget);
				c.SetOn(i == 0);
				m_TargetChips.Insert(c);
				m_TargetNames.Insert(names[i]);
				m_TargetStates.Insert("?");
			}
			UpdateTargets();
		}
		super.BuildContent(inner);
	}

	// Rows of servers wait for their project/list ("Loading …"); a failed list stays grey, a server that lets this
	// user create nothing goes.
	protected void UpdateTargets()
	{
		DZS_SaveTarget t = DZS_SaveTargets.Get();
		for (int i = 1; i < m_TargetChips.Count(); i++)
		{
			string st = "none";
			if (t)
				st = t.TargetState(m_TargetIds[i]);
			if (st == m_TargetStates[i])
				continue;
			m_TargetStates[i] = st;
			DZS_Chip c = m_TargetChips[i];
			if (st == "loading")
			{
				c.SetLabel(m_TargetNames[i] + " · " + DZS_Texts.Pick("Wird geladen …", "Loading …"));
				c.SetEnabled(false, DZS_Texts.Pick("Wird geladen …", "Loading …"));
			}
			else if (st == "unreachable")
			{
				c.SetLabel(m_TargetNames[i] + " · " + DZS_Texts.Pick("Server nicht erreichbar", "Server not reachable"));
				c.SetEnabled(false, DZS_Texts.Pick("Server nicht erreichbar", "Server not reachable"));
			}
			else
			{
				c.SetLabel(m_TargetNames[i]);
				c.SetEnabled(st == "");
			}
			c.Show(st != "none");
			if ((st == "none" || st == "unreachable") && m_Target == m_TargetIds[i])
				OnTarget(m_TargetChips[0]);
			DZS_Log.Info("projects: save-as target " + m_TargetIds[i] + " " + st);
		}
	}

	override void UpdateContent(float dt)
	{
		super.UpdateContent(dt);
		if (m_TargetChips.Count() > 0)
			UpdateTargets();
	}

	void OnTarget(DZS_Chip c)
	{
		m_Target = c.m_Key;
		foreach (DZS_Chip o : m_TargetChips)
			o.SetOn(o == c);
		if (m_Target == "")
			m_Info.SetText(DZS_Texts.Pick("Neues lokales Projekt auf derselben Karte", "New local project on the same map"));
		else
			m_Info.SetText(DZS_Texts.Pick("Neues Projekt beim Server, offen in einer Sitzung", "New project at the server, open in a session"));
	}

	// As wide as the longest target row, so "‹Server› · Server not reachable" is never cut.
	override float BoxWidth()
	{
		float w = super.BoxWidth();
		foreach (DZS_Chip c : m_TargetChips)
			w = Math.Max(w, c.Width() + DZS_Scale.Px(48));
		return Math.Min(w, DZS_Scale.Px(960));
	}

	override float LayoutContent(float width)
	{
		if (m_TargetChips.Count() == 0)
			return super.LayoutContent(width);
		float y = 0;
		DZS_W.Place(m_TargetLabel, 0, y, width, DZS_Scale.Px(14));
		y += DZS_Scale.Px(18);
		for (int i = 0; i < m_TargetChips.Count(); i++)
		{
			if (i > 0 && m_TargetStates[i] == "none")
				continue;
			m_TargetChips[i].Place(0, y, width);
			y += DZS_Chip.Height() + DZS_Scale.Px(4);
		}
		y += DZS_Scale.Px(10);
		DZS_W.Place(m_Label, 0, y, width, DZS_Scale.Px(14));
		m_Field.Place(0, y + DZS_Scale.Px(18), DZS_Scale.Px(28));
		DZS_W.Place(m_Info, 0, y + DZS_Scale.Px(56), width, DZS_Scale.Px(20));
		return y + DZS_Scale.Px(80);
	}

	override void OnFoot(string tag)
	{
		if (tag == "saveas")
		{
			string name = Value();
			if (!DZS_Names.ValidProjectName(name))
				return;
			Close();
			if (m_Copy)
				m_P.SaveCopy(name);
			else if (m_Target != "" && DZS_SaveTargets.Get())
				DZS_SaveTargets.Get().SaveAs(m_Target, name);
			else
				m_P.SaveAs(name);
			return;
		}
		Close();
	}

	override void OnClosed()
	{
		foreach (DZS_Chip c : m_TargetChips)
		{
			if (c.m_Hit)
				c.m_Hit.Destroy();
		}
		m_TargetChips.Clear();
		super.OnClosed();
	}
}

class DZS_RenameDialog : DZS_NameDialog
{
	protected DZS_ProjectsPlugin m_P;
	protected DZS_BrowserDialog m_Browser;
	protected string m_Folder;

	void DZS_RenameDialog(DZS_ProjectsPlugin p, DZS_BrowserDialog browser, string folder, string oldName)
	{
		m_P = p;
		m_Browser = browser;
		m_Folder = folder;
		string tx116 = DZS_Texts.Pick("Projekt", "Project");
		string tx117 = DZS_Texts.Pick("Umbenennen", "Rename");
		string tx118 = DZS_Texts.Pick("Die Projektdatei wird mit umbenannt", "The project file is renamed too");
		string tx119 = DZS_Texts.Pick("Umbenennen", "Rename");
		SetupName(tx116, tx117, oldName, tx118, "rename", tx119);
	}

	override void OnFoot(string tag)
	{
		if (tag == "rename")
		{
			string name = Value();
			if (!DZS_Names.ValidProjectName(name))
				return;
			Close();
			m_P.RenameProject(m_Folder, name);
			if (m_Browser)
				m_Browser.Reload();
			return;
		}
		Close();
	}
}

// One line of a list: name, details, optional mark; focus kind "list".
class DZS_ListRow : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	Widget m_Bg;
	Widget m_Sel;
	Widget m_Bar;
	ref DZS_Border m_Ring;
	TextWidget m_Name;
	TextWidget m_Meta;
	TextWidget m_Mark;
	bool m_Selected;
	bool m_Off;
	string m_Key;
	ref ScriptInvoker m_OnClick;
	ref ScriptInvoker m_OnDouble;
	protected float m_LastClick = -10;

	void DZS_ListRow(Widget parent, string key, string name, string meta, string mark)
	{
		m_Key = key;
		m_OnClick = new ScriptInvoker();
		m_OnDouble = new ScriptInvoker();
		m_Hit = new DZS_Hit(parent, this, "row");
		m_Hit.m_FocusKind = "list";
		Widget f = m_Hit.m_W;
		m_Bg = DZS_W.Panel(f, DZS_Color.Get("surfaceRaised"));
		m_Sel = DZS_W.Panel(f, DZS_Color.Get("accentTint"));
		m_Bar = DZS_W.Panel(f, DZS_Color.Get("accent"));
		m_Ring = new DZS_Border(f, DZS_Color.Get("accent"));
		m_Name = DZS_W.Text(f, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_Name.SetText(name);
		m_Meta = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Meta.SetText(meta);
		m_Mark = DZS_W.Text(f, DZS_Font.LABELBOLD, 11, DZS_Color.Get("accentText"), false, true);
		m_Mark.SetText(DZS_Texts.Upper(mark));
		Refresh();
	}

	void SetOff(bool off, string tip)
	{
		m_Off = off;
		if (off)
			m_Hit.SetTip(tip);
		Refresh();
	}

	void SetSelected(bool s)
	{
		m_Selected = s;
		Refresh();
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		float now = GetGame().GetTickTime();
		bool dbl = now - m_LastClick < 0.4;
		m_LastClick = now;
		m_OnClick.Invoke(this);
		if (dbl)
			m_OnDouble.Invoke(this);
	}

	override void OnHitActivate(DZS_Hit h)
	{
		m_OnDouble.Invoke(this);
	}

	void Refresh()
	{
		m_Bg.Show(m_Hit.m_Hover && !m_Selected);
		m_Sel.Show(m_Selected);
		m_Bar.Show(m_Selected);
		m_Ring.Show(m_Hit.m_Focus);
		string nameCol = "text";
		if (m_Off)
			nameCol = "textOff";
		else if (m_Selected)
			nameCol = "accentText";
		m_Name.SetColor(DZS_Color.Get(nameCol));
	}

	float Place(float x, float y, float w, float h)
	{
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		DZS_W.Place(m_Sel, 0, 0, w, h);
		DZS_W.Place(m_Bar, 0, 0, DZS_Scale.Px(2), h);
		m_Ring.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		float pad = DZS_Scale.Px(12);
		float markW = DZS_Scale.Px(70);
		DZS_W.Place(m_Name, pad, DZS_Scale.Px(4), w - 2 * pad - markW, DZS_Scale.Px(20));
		DZS_W.Place(m_Meta, pad, h / 2, w - 2 * pad - markW, DZS_Scale.Px(18));
		DZS_W.Place(m_Mark, w - pad - markW, 0, markW, h);
		return h;
	}

	void Destroy()
	{
		m_Hit.Destroy();
	}
}

class DZS_BrowserDialog : DZS_Dialog
{
	protected DZS_ProjectsPlugin m_P;
	protected string m_Map;
	protected ref DZS_SearchField m_Search;
	protected ref array<ref DZS_Button> m_MapButtons;
	protected ref DZS_Button m_TabLocal;
	protected ref array<ref DZS_Button> m_Tabs = new array<ref DZS_Button>();
	protected string m_Tab = "local";
	protected bool m_WasLoading;
	protected ref array<ref DZS_ListRow> m_Rows;
	protected ref array<ref DZS_ProjectInfo> m_All;
	protected TextWidget m_Path;
	protected TextWidget m_Empty;
	protected Widget m_ListFrame;
	protected string m_SelFolder;
	protected string m_LastSearch;
	protected bool m_Dirty;

	void DZS_BrowserDialog(DZS_ProjectsPlugin p, string mapKey)
	{
		m_P = p;
		m_Map = mapKey;
		m_Rows = new array<ref DZS_ListRow>();
		m_MapButtons = new array<ref DZS_Button>();
		string tx114 = DZS_Texts.Pick("Projekt", "Project");
		string tx115 = DZS_Texts.Pick("Projekte", "Projects");
		Setup(LARGE, tx114, tx115, false);
		AddFootButton("new", DZS_Texts.Pick("Neues Projekt …", "New project …"), "side");
		AddFootButton("import", DZS_Texts.Pick(".dze importieren …", "Import .dze …"), "side");
		AddFootButton("rename", DZS_Texts.Pick("Umbenennen …", "Rename …"), "side");
		AddFootButton("delete", DZS_Texts.Pick("Löschen …", "Delete …"), "danger");
		AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "side");
		DZS_Button open = AddFootButton("open", DZS_Texts.Pick("Öffnen", "Open"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Search = new DZS_SearchField("projects.search", DZS_Texts.Pick("Projekte suchen", "Search projects"));
		m_Search.SetWidth(300);
		m_Search.Build(inner);
		m_TabLocal = new DZS_Button(inner, "side", DZS_Texts.Pick("Lokal", "Local"));
		m_TabLocal.SetOn(true);
		m_TabLocal.m_Tag = "local";
		m_TabLocal.m_OnClick.Insert(OnTabClick);
		m_Tabs.Insert(m_TabLocal);
		array<string> tabIds = new array<string>();
		array<ref DZS_StoreProvider> provs = DZS_StoreProvider.All();
		foreach (DZS_StoreProvider sp : provs)
		{
			string tl = sp.TabLabel();
			if (tl == "" || tabIds.Find(sp.Id()) >= 0)
				continue;
			tabIds.Insert(sp.Id());
			DZS_Button tb = new DZS_Button(inner, "side", tl);
			tb.m_Tag = sp.Id();
			tb.m_OnClick.Insert(OnTabClick);
			m_Tabs.Insert(tb);
		}
		array<string> keys = DZS_Maps.Keys();
		keys.Insert("");
		foreach (string k : keys)
		{
			string label = DZS_Texts.Pick("Alle Karten", "All maps");
			if (k != "")
				label = DZS_Maps.Display(k);
			DZS_Button b = new DZS_Button(inner, "side", label);
			b.m_Tag = k;
			b.m_OnClick.Insert(OnMapClick);
			m_MapButtons.Insert(b);
		}
		m_ListFrame = DZS_W.Frame(inner);
		m_Path = DZS_W.Text(inner, DZS_Font.MONO, 12, DZS_Color.Get("textMuted"));
		m_Empty = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("textMuted"));
		m_Empty.SetText(DZS_Texts.Pick("Keine Projekte für diesen Filter", "No projects for this filter"));
		Reload();
	}

	protected DZS_StoreProvider TabProvider()
	{
		array<ref DZS_StoreProvider> provs = DZS_StoreProvider.All();
		foreach (DZS_StoreProvider sp : provs)
		{
			if (sp.Id() == m_Tab)
				return sp;
		}
		return null;
	}

	void OnTabClick(DZS_Button b)
	{
		if (b.m_Tag == m_Tab)
			return;
		m_Tab = b.m_Tag;
		m_SelFolder = "";
		DZS_Log.Info("browser: tab " + m_Tab);
		DZS_StoreProvider sp = TabProvider();
		if (sp)
		{
			sp.RefreshRows();
			m_WasLoading = sp.RowsLoading();
		}
		Reload();
	}

	void OnMapClick(DZS_Button b)
	{
		m_Map = b.m_Tag;
		Rebuild();
	}

	void Reload()
	{
		m_All = new array<ref DZS_ProjectInfo>();
		DZS_StoreProvider tp = TabProvider();
		if (tp)
		{
			tp.Rows(m_All);
			Rebuild();
			return;
		}
		array<ref DZS_ProjectInfo> recent = m_P.Recent("", 0, false);
		array<ref DZS_ProjectInfo> all = DZS_LocalStore.List();
		foreach (DZS_ProjectInfo r : recent)
			m_All.Insert(r);
		array<ref DZS_ProjectInfo> never = new array<ref DZS_ProjectInfo>();
		foreach (DZS_ProjectInfo a : all)
		{
			bool known = false;
			foreach (DZS_ProjectInfo r2 : recent)
			{
				if (r2.m_Folder == a.m_Folder)
					known = true;
			}
			if (!known)
				never.Insert(a);
		}
		for (int i = 1; i < never.Count(); i++)
		{
			DZS_ProjectInfo v = never[i];
			int j = i - 1;
			while (j >= 0 && never[j].m_Name > v.m_Name)
			{
				never[j + 1] = never[j];
				j--;
			}
			never[j + 1] = v;
		}
		foreach (DZS_ProjectInfo n : never)
			m_All.Insert(n);
		Rebuild();
	}

	protected void Rebuild()
	{
		foreach (DZS_ListRow old : m_Rows)
			old.Destroy();
		m_Rows.Clear();
		string q = m_Search.GetText();
		q.ToLower();
		m_LastSearch = m_Search.GetText();
		string loaded = m_P.LoadedMapKey();
		bool inEditor = !m_P.IsHome();
		foreach (DZS_ProjectInfo p : m_All)
		{
			string mapKey = p.m_MapKey;
			if (m_Map != "" && mapKey != m_Map)
				continue;
			string low = p.m_Name;
			low.ToLower();
			if (q != "" && !low.Contains(q))
				continue;
			string mapName = DZS_Texts.Pick("unbekannt", "unknown");
			if (mapKey != "")
				mapName = DZS_Maps.Display(mapKey);
			string saved = DZS_ProjectsPlugin.ShowStamp(m_P.RecentStamp(p.m_Folder, "saved"));
			string meta = mapName + " · " + p.m_Groups + DZS_Texts.Pick(DZS_Issues.Plural(p.m_Groups, " Gruppe", " Gruppen"), DZS_Issues.Plural(p.m_Groups, " group", " groups"));
			if (saved != "")
				meta += " · " + DZS_Texts.Pick("gespeichert ", "saved ") + saved;
			DZS_StoreProvider rp = TabProvider();
			if (rp)
				meta = rp.RowMeta(p);
			string mark = "";
			if (rp)
				mark = rp.RowMark(p);
			if (m_P.m_Open && m_P.m_Open.m_Folder == p.m_Folder)
				mark = DZS_Texts.Pick("offen", "open");
			DZS_ListRow row = new DZS_ListRow(m_ListFrame, p.m_Folder, p.m_Name, meta, mark);
			string offDe, offEn;
			if (inEditor && mapKey != "" && mapKey != loaded)
				row.SetOff(true, DZS_Texts.Pick("Andere Karte — über Zurück ins Hauptmenü öffnen", "Other map — open it via Back to the main menu"));
			else if (rp && rp.RowOff(p, offDe, offEn))
				row.SetOff(true, DZS_Texts.Pick(offDe, offEn));
			row.m_OnClick.Insert(OnRowClick);
			row.m_OnDouble.Insert(OnRowDouble);
			m_Rows.Insert(row);
		}
		if (!SelectedRow())
			m_SelFolder = "";
		if (m_SelFolder == "" && m_Rows.Count() > 0)
			m_SelFolder = m_Rows[0].m_Key;
		ClearFocusables();
		foreach (DZS_ListRow r : m_Rows)
			AddFocusHit(r.m_Hit);
		AddFootFocus();
		UpdateSelection();
	}

	DZS_ProjectInfo Selected()
	{
		foreach (DZS_ProjectInfo p : m_All)
		{
			if (p.m_Folder == m_SelFolder)
				return p;
		}
		return null;
	}

	DZS_ListRow SelectedRow()
	{
		foreach (DZS_ListRow r : m_Rows)
		{
			if (r.m_Key == m_SelFolder)
				return r;
		}
		return null;
	}

	void OnRowClick(DZS_ListRow r)
	{
		m_SelFolder = r.m_Key;
		UpdateSelection();
	}

	void OnRowDouble(DZS_ListRow r)
	{
		m_SelFolder = r.m_Key;
		UpdateSelection();
		OpenSelected();
	}

	protected void UpdateSelection()
	{
		foreach (DZS_ListRow r : m_Rows)
			r.SetSelected(r.m_Key == m_SelFolder);
		DZS_ProjectInfo p = Selected();
		DZS_ListRow row = SelectedRow();
		DZS_StoreProvider sp = TabProvider();
		if (p && sp)
			m_Path.SetText(DZS_W.Literal(sp.Address()));
		else if (p)
			m_Path.SetText(DZS_W.Literal(DZS_LocalStore.ShownRoot() + "\\" + p.m_Folder + DZS_LocalStore.EXT));
		else
			m_Path.SetText("");
		bool any = row != null;
		bool off = row && row.m_Off;
		bool isOpen = p && m_P.m_Open && m_P.m_Open.m_Folder == p.m_Folder;
		string openTip = DZS_Texts.Pick("Andere Karte — über Zurück ins Hauptmenü öffnen", "Other map — open it via Back to the main menu");
		string rowDe, rowEn;
		if (p && sp && sp.RowOff(p, rowDe, rowEn))
			openTip = DZS_Texts.Pick(rowDe, rowEn);
		FootButton("open").SetEnabled(any && !off, openTip);
		FootButton("rename").SetEnabled(any, "");
		FootButton("delete").SetEnabled(any && !isOpen, DZS_Texts.Pick("Das offene Projekt lässt sich nicht löschen", "The open project cannot be deleted"));
		if (sp)
		{
			string webDe, webEn;
			sp.NoRename(webDe, webEn);
			FootButton("rename").SetEnabled(false, DZS_Texts.Pick(webDe, webEn));
			FootButton("delete").SetEnabled(false, DZS_Texts.Pick(webDe, webEn));
		}
	}

	protected void OpenSelected()
	{
		DZS_ListRow row = SelectedRow();
		if (!row || row.m_Off)
			return;
		string folder = m_SelFolder;
		Close();
		DZS_Log.Info("browser: open " + folder);
		m_P.RequestOpen(folder);
	}

	override void OnFoot(string tag)
	{
		DZS_ProjectInfo p = Selected();
		if (tag == "open")
			OpenSelected();
		else if (tag == "close")
			Close();
		else if (tag == "new")
		{
			string mapKey = m_Map;
			if (mapKey == "")
				mapKey = m_P.CurrentMapForDialogs();
			if (!m_P.IsHome())
				mapKey = m_P.LoadedMapKey();
			Close();
			m_P.OpenNewDialog(mapKey);
		}
		else if (tag == "import")
		{
			Close();
			m_P.OpenDzeDialog(false);
		}
		else if (tag == "rename" && p)
		{
			DZS_RenameDialog rd = new DZS_RenameDialog(m_P, this, p.m_Folder, p.m_Name);
			rd.Open();
		}
		else if (tag == "delete" && p)
		{
			string tx111 = DZS_Texts.Pick("Projekt", "Project");
			string tx112 = DZS_Texts.Pick("Löschen", "Delete");
			string tx113 = DZS_Texts.Pick(p.m_Name + " endgültig löschen? Die Projektdatei wird gelöscht.", "Delete " + p.m_Name + " for good? The project file is deleted.");
			DZS_ConfirmDialog c = new DZS_ConfirmDialog(tx111, tx112, tx113);
			c.m_Tag = p.m_Folder;
			c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
			DZS_Button del = c.AddFootButton("delete", DZS_Texts.Pick("Löschen", "Delete"), "danger");
			del.m_Hit.m_Primary = true;
			c.m_OnResult.Insert(OnDeleteAnswer);
			c.Open();
		}
	}

	void OnDeleteAnswer(DZS_Dialog d, string result)
	{
		if (result != "delete")
			return;
		m_P.DeleteProject(d.m_Tag);
		m_SelFolder = "";
		Reload();
	}

	override void UpdateContent(float dt)
	{
		DZS_StoreProvider sp = TabProvider();
		if (sp)
		{
			bool loading = sp.RowsLoading();
			if (m_WasLoading && !loading)
				Reload();
			m_WasLoading = loading;
			if (loading)
				m_Empty.SetText(DZS_Texts.Pick("Lädt …", "Loading …"));
			else if (m_All.Count() == 0 && sp.RowsEmptyText() != "")
				m_Empty.SetText(DZS_W.Literal(sp.RowsEmptyText()));
			else
				m_Empty.SetText(DZS_Texts.Pick("Keine Projekte für diesen Filter", "No projects for this filter"));
		}
		else
			m_Empty.SetText(DZS_Texts.Pick("Keine Projekte für diesen Filter", "No projects for this filter"));
		foreach (DZS_Button tb : m_Tabs)
			tb.SetOn(tb.m_Tag == m_Tab);
		if (m_Search.GetText() != m_LastSearch)
			Rebuild();
		m_Search.RefreshExtras();
		foreach (DZS_Button b : m_MapButtons)
			b.SetOn(b.m_Tag == m_Map);
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		m_Search.Place(0, y, DZS_Scale.Px(32));
		float x = m_Search.GetWidth() + DZS_Scale.Px(16);
		foreach (DZS_Button b : m_MapButtons)
		{
			b.Place(x, y + DZS_Scale.Px(2));
			x += b.GetWidth() + DZS_Scale.Px(4);
		}
		float tabsW = 0;
		foreach (DZS_Button tw : m_Tabs)
			tabsW += tw.GetWidth() + DZS_Scale.Px(4);
		float tabY = y;
		if (x + DZS_Scale.Px(12) + tabsW > width)
			tabY = y + DZS_Scale.Px(40);
		float tx = width;
		for (int ti = m_Tabs.Count() - 1; ti >= 0; ti--)
		{
			tx -= m_Tabs[ti].GetWidth();
			m_Tabs[ti].Place(tx, tabY + DZS_Scale.Px(2));
			tx -= DZS_Scale.Px(4);
		}
		y = tabY + DZS_Scale.Px(44);
		float rowH = DZS_Scale.Px(48);
		float ly = 0;
		foreach (DZS_ListRow r : m_Rows)
			ly += r.Place(0, ly, width, rowH) + DZS_Scale.Px(2);
		DZS_W.Place(m_ListFrame, 0, y, width, Math.Max(ly, rowH));
		m_Empty.Show(m_Rows.Count() == 0);
		DZS_W.Place(m_Empty, 0, y + DZS_Scale.Px(12), width, DZS_Scale.Px(20));
		y += Math.Max(ly, rowH) + DZS_Scale.Px(12);
		DZS_W.Place(m_Path, 0, y, width, DZS_Scale.Px(18));
		return y + DZS_Scale.Px(24);
	}

	override void FocusArrow(int dir)
	{
		super.FocusArrow(dir);
		DZS_Hit h = FocusedHit();
		foreach (DZS_ListRow r : m_Rows)
		{
			if (r.m_Hit == h)
			{
				m_SelFolder = r.m_Key;
				UpdateSelection();
			}
		}
	}

	// Enter on a row opens it.
	override void EnterKey()
	{
		DZS_Hit h = FocusedHit();
		foreach (DZS_ListRow r : m_Rows)
		{
			if (r.m_Hit == h)
			{
				m_SelFolder = r.m_Key;
				OpenSelected();
				return;
			}
		}
		super.EnterKey();
	}
}

// Start: "Unsaved work found" — one line per backup, newest first.
class DZS_RestoreDialog : DZS_Dialog
{
	protected DZS_ProjectsPlugin m_P;
	protected ref array<string> m_Files;
	protected ref array<TextWidget> m_Texts;
	protected ref array<TextWidget> m_Metas;
	protected ref array<ref DZS_Button> m_OpenB;
	protected ref array<ref DZS_Button> m_DropB;

	void DZS_RestoreDialog(DZS_ProjectsPlugin p, array<string> files)
	{
		m_P = p;
		m_Files = new array<string>();
		m_Files.Copy(files);
		m_Texts = new array<TextWidget>();
		m_Metas = new array<TextWidget>();
		m_OpenB = new array<ref DZS_Button>();
		m_DropB = new array<ref DZS_Button>();
		string tx109 = DZS_Texts.Pick("Projekt", "Project");
		string tx110 = DZS_Texts.Pick("Nicht gespeicherte Arbeit gefunden", "Unsaved work found");
		Setup(MEDIUM, tx109, tx110, true);
		AddFootButton("later", DZS_Texts.Pick("Später", "Later"), "side");
	}

	override void BuildContent(Widget inner)
	{
		foreach (string f : m_Files)
		{
			DZS_Json b = DZS_ProjectsPlugin.ReadBackup(f);
			TextWidget t = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
			TextWidget m = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
			int n = b.GetInt("unsaved");
			t.SetText(b.GetStr("name"));
			string bm = DZS_ProjectsPlugin.BackupMap(b);
			string bt = b.GetStr("time");
			string md = DZS_Texts.Pick("Karte unbekannt", "Map unknown");
			if (bm != "")
				md = DZS_Maps.Display(bm);
			string mt = DZS_ProjectsPlugin.ShowStamp(bt);
			string mp = DZS_Issues.Plural(n, DZS_Texts.Pick(" ungespeicherte Änderung", " unsaved change"), DZS_Texts.Pick(" ungespeicherte Änderungen", " unsaved changes"));
			string loc = DZS_Texts.Get("core.provider.local");
			string bprov = b.GetStr("provider");
			if (bprov != "" && bprov != "local")
			{
				loc = bprov;
				DZS_StoreProvider bsp = m_P.ProviderOf(b.GetStr("location"));
				if (bsp && bsp.TabLabel() != "")
					loc = bsp.TabLabel();
			}
			string meta = md + " · " + loc + " · " + mt + " · " + n + mp;
			if (m_P.BackupFilesChanged(b))
				meta += " · " + DZS_Texts.Pick("Dateien seitdem geändert", "Files changed since");
			m.SetText(meta);
			DZS_Button o = new DZS_Button(inner, "side", DZS_Texts.Pick("Öffnen", "Open"));
			o.m_Tag = f;
			o.m_OnClick.Insert(OnOpen);
			DZS_Button d = new DZS_Button(inner, "danger", DZS_Texts.Pick("Verwerfen", "Discard"));
			d.m_Tag = f;
			d.m_OnClick.Insert(OnDrop);
			string blockedDe = DZS_ProjectsPlugin.BackupBlocked(f, b, true);
			string blockedEn = DZS_ProjectsPlugin.BackupBlocked(f, b, false);
			if (blockedDe != "")
				o.SetEnabled(false, DZS_Texts.Pick(blockedDe, blockedEn));
			else if (bm == "")
				o.SetEnabled(false, DZS_Texts.Pick("Karte der Sicherung unbekannt", "Map of the backup unknown"));
			else if (!DZS_Maps.Available(bm))
				o.SetEnabled(false, DZS_Maps.Display(bm) + DZS_Texts.Pick(" ist nicht verfügbar", " is not available"));
			m_Texts.Insert(t);
			m_Metas.Insert(m);
			m_OpenB.Insert(o);
			m_DropB.Insert(d);
			AddFocusHit(o.m_Hit);
			AddFocusHit(d.m_Hit);
		}
		AddFootFocus();
	}

	// As wide as the longest line next to its knobs, so "Files changed since" is never cut.
	override float BoxWidth()
	{
		float w = super.BoxWidth();
		for (int i = 0; i < m_Metas.Count(); i++)
		{
			float bw = m_OpenB[i].GetWidth() + m_DropB[i].GetWidth() + DZS_Scale.Px(8);
			float need = Math.Max(DZS_W.TextWidth(m_Metas[i]), DZS_W.TextWidth(m_Texts[i])) + DZS_Scale.Px(16) + bw + DZS_Scale.Px(48);
			w = Math.Max(w, need);
		}
		return Math.Min(w, DZS_Scale.Px(960));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		for (int i = 0; i < m_Texts.Count(); i++)
		{
			float bw = m_OpenB[i].GetWidth() + m_DropB[i].GetWidth() + DZS_Scale.Px(8);
			DZS_W.Place(m_Texts[i], 0, y, width - bw, DZS_Scale.Px(20));
			DZS_W.Place(m_Metas[i], 0, y + DZS_Scale.Px(22), width - bw, DZS_Scale.Px(18));
			m_DropB[i].Place(width - m_DropB[i].GetWidth(), y + DZS_Scale.Px(8));
			m_OpenB[i].Place(width - bw, y + DZS_Scale.Px(8));
			y += DZS_Scale.Px(52);
		}
		return y;
	}

	void OnOpen(DZS_Button b)
	{
		string f = b.m_Tag;
		Close();
		DZS_Log.Info("restore dialog: open " + f);
		m_P.RequestRestore(f);
	}

	void OnDrop(DZS_Button b)
	{
		string tx106 = DZS_Texts.Pick("Projekt", "Project");
		string tx107 = DZS_Texts.Pick("Verwerfen", "Discard");
		string tx108 = DZS_Texts.Pick("Die nicht gespeicherte Arbeit endgültig verwerfen?", "Discard the unsaved work for good?");
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(tx106, tx107, tx108);
		c.m_Tag = b.m_Tag;
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button del = c.AddFootButton("discard", DZS_Texts.Pick("Verwerfen", "Discard"), "danger");
		del.m_Hit.m_Primary = true;
		c.m_OnResult.Insert(OnDropAnswer);
		c.Open();
	}

	void OnDropAnswer(DZS_Dialog d, string result)
	{
		if (result != "discard")
			return;
		m_P.DiscardBackup(d.m_Tag);
		int i = m_Files.Find(d.m_Tag);
		if (i >= 0)
		{
			m_Texts[i].Unlink();
			m_Metas[i].Unlink();
			m_OpenB[i].Destroy();
			m_DropB[i].Destroy();
			m_Files.RemoveOrdered(i);
			m_Texts.RemoveOrdered(i);
			m_Metas.RemoveOrdered(i);
			m_OpenB.RemoveOrdered(i);
			m_DropB.RemoveOrdered(i);
		}
		ClearFocusables();
		for (int k = 0; k < m_OpenB.Count(); k++)
		{
			AddFocusHit(m_OpenB[k].m_Hit);
			AddFocusHit(m_DropB[k].m_Hit);
		}
		AddFootFocus();
		if (m_Files.Count() == 0)
			Close();
	}

	override void Cancel()
	{
		DZS_Log.Info("restore dialog: later");
		Close();
	}
}

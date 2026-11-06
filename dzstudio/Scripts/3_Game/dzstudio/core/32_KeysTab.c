// Tab "Key bindings".
class DZS_KeyField : DZS_HitListener
{
	DZS_KeysTab m_Tab;
	string m_Action;
	int m_Slot;
	ref DZS_Hit m_Hit;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected ref DZS_RichLine m_Caps;
	protected TextWidget m_Empty;
	protected TextWidget m_Rec;
	protected TextWidget m_RecSub;
	string m_Binding;
	bool m_Recording;
	string m_Warn;
	float m_WarnLeft;

	void DZS_KeyField(Widget parent, DZS_KeysTab tab, string action, int slot, string binding)
	{
		m_Tab = tab;
		m_Action = action;
		m_Slot = slot;
		m_Binding = binding;
		m_Hit = new DZS_Hit(parent, this, "keyfield");
		m_Hit.m_FocusKind = "list";
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("well"));
		m_Border = new DZS_Border(m_Hit.m_W, DZS_Color.Get("line"));
		m_Caps = new DZS_RichLine(m_Hit.m_W, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"));
		if (binding != "")
			m_Caps.Set("[" + binding + "]");
		m_Empty = DZS_W.Text(m_Hit.m_W, DZS_Font.MONO, 12, DZS_Color.Get("textQuiet"), true);
		m_Empty.SetText("—");
		m_Rec = DZS_W.Text(m_Hit.m_W, DZS_Font.READ, 12, DZS_Color.Get("text"), true);
		m_RecSub = DZS_W.Text(m_Hit.m_W, DZS_Font.READ, 10, DZS_Color.Get("textQuiet"), true);
	}

	static float W() { return DZS_Scale.Px(120); }
	static float H() { return DZS_Scale.Px(32); }

	void Place(float x, float y)
	{
		float w = W();
		float h = H();
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		Refresh();
	}

	void Refresh()
	{
		float w = W();
		float h = H();
		bool warn = m_WarnLeft > 0;
		if (m_Recording)
		{
			m_Border.SetColor(DZS_Color.Get("accent"));
			m_Border.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		}
		else if (m_Hit.m_Focus)
		{
			m_Border.SetColor(DZS_Color.Get("accent"));
			m_Border.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		}
		else
		{
			m_Border.SetColor(DZS_Color.Get("line"));
			m_Border.Place(0, 0, w, h, 1);
		}
		m_Caps.Show(!m_Recording && m_Binding != "");
		m_Empty.Show(!m_Recording && m_Binding == "");
		m_Rec.Show(m_Recording);
		m_RecSub.Show(m_Recording);
		if (m_Recording)
		{
			if (warn)
			{
				DZS_W.FitText(m_Rec, m_Warn, w - DZS_Scale.Px(6));
				m_Rec.SetColor(DZS_Color.Get("warning"));
			}
			else
			{
				m_Rec.SetText(DZS_Texts.Get("core.keys.press"));
				m_Rec.SetColor(DZS_Color.Get("text"));
			}
			m_RecSub.SetText(DZS_Texts.Get("core.keys.escCancels"));
			DZS_W.Place(m_Rec, 0, DZS_Scale.Px(1), w, DZS_Scale.Px(18));
			DZS_W.Place(m_RecSub, 0, DZS_Scale.Px(17), w, DZS_Scale.Px(14));
		}
		else
		{
			m_Caps.Place((w - m_Caps.GetWidth()) / 2, (h - m_Caps.GetHeight()) / 2);
			DZS_W.Place(m_Empty, 0, 0, w, h);
		}
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_Tab.StartRecording(this);
	}

	override void OnHitActivate(DZS_Hit h)
	{
		m_Tab.StartRecording(this);
	}
}

class DZS_KeyRow : DZS_HitListener
{
	DZS_KeysTab m_Tab;
	string m_Action;
	protected Widget m_Frame;
	protected TextWidget m_Name;
	protected TextWidget m_When;
	ref array<ref DZS_KeyField> m_Fields;
	ref array<ref DZS_Button> m_Remove;
	ref DZS_Button m_Reset;
	protected ref array<TextWidget> m_Warnings;

	void DZS_KeyRow(Widget parent, DZS_KeysTab tab, DZS_Action a)
	{
		m_Tab = tab;
		m_Action = a.m_Id;
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		m_Frame = DZS_W.Frame(parent);
		m_Name = DZS_W.Text(m_Frame, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_Name.SetText(a.Name());
		m_When = DZS_W.Text(m_Frame, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
		m_When.SetText(DZS_KeysTab.WhenText(a));
		array<string> binds = ui.GetBindings(a.m_Id);
		m_Fields = new array<ref DZS_KeyField>();
		m_Remove = new array<ref DZS_Button>();
		for (int i = 0; i < 2; i++)
		{
			string b = "";
			if (i < binds.Count())
				b = binds[i];
			m_Fields.Insert(new DZS_KeyField(m_Frame, tab, a.m_Id, i, b));
			DZS_Button x = new DZS_Button(m_Frame, "ghost", "", "x", 28);
			x.m_Tag = i.ToString();
			x.m_OnClick.Insert(OnRemove);
			x.SetTip(DZS_Texts.Get("core.keys.remove"));
			if (a.m_MustKeep && binds.Count() == 1)
			{
				x.SetEnabled(false, DZS_Texts.Get("core.keys.mustKeep"));
			}
			x.Show(b != "");
			m_Remove.Insert(x);
		}
		m_Reset = new DZS_Button(m_Frame, "ghost", "", "undo-2", 28);
		m_Reset.m_OnClick.Insert(OnReset);
		string defs = "";
		array<string> d = ui.GetEffectiveDefaults(a.m_Id);
		if (a.m_BlockedDefault != "")
			d = a.m_DefaultKeys;
		foreach (string k : d)
		{
			if (defs != "")
				defs += " · ";
			defs += DZS_KeysTab.CapsText(k);
		}
		if (defs == "")
			defs = "—";
		m_Reset.SetTip(DZS_Texts.Fmt("core.settings.default", defs));
		m_Reset.Show(ui.IsCustom(a.m_Id));
		m_Warnings = new array<TextWidget>();
		array<string> warns = tab.WarningsFor(a);
		foreach (string w : warns)
		{
			TextWidget t = DZS_W.Text(m_Frame, DZS_Font.READ, 12, DZS_Color.Get("warning"));
			t.SetText(w);
			m_Warnings.Insert(t);
		}
		if (a.m_MustKeep)
		{
			TextWidget n = DZS_W.Text(m_Frame, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
			n.SetText(DZS_Texts.Get("core.keys.characterNote"));
			m_Warnings.Insert(n);
		}
	}

	float Layout(float y, float width)
	{
		float rowH = DZS_Scale.Px(48);
		float lineH = DZS_Scale.Px(18);
		float h = rowH + m_Warnings.Count() * lineH;
		DZS_W.Place(m_Frame, 0, y, width, h);
		DZS_W.Place(m_Name, 0, DZS_Scale.Px(5), DZS_Scale.Px(420), DZS_Scale.Px(20));
		DZS_W.Place(m_When, 0, DZS_Scale.Px(25), DZS_Scale.Px(420), DZS_Scale.Px(18));
		float x = DZS_Scale.Px(430);
		float fy = (rowH - DZS_KeyField.H()) / 2;
		for (int i = 0; i < 2; i++)
		{
			m_Fields[i].Place(x, fy);
			x += DZS_KeyField.W() + DZS_Scale.Px(2);
			m_Remove[i].Place(x, (rowH - DZS_Scale.Px(28)) / 2);
			x += DZS_Scale.Px(28) + DZS_Scale.Px(10);
		}
		m_Reset.Place(x, (rowH - DZS_Scale.Px(28)) / 2);
		float wy = rowH - DZS_Scale.Px(4);
		foreach (TextWidget t : m_Warnings)
		{
			DZS_W.Place(t, 0, wy, width, lineH);
			wy += lineH;
		}
		return h;
	}

	void AddFocus(DZS_Dialog dlg)
	{
		for (int i = 0; i < 2; i++)
		{
			dlg.AddFocusHit(m_Fields[i].m_Hit);
			dlg.AddFocusHit(m_Remove[i].m_Hit);
		}
		dlg.AddFocusHit(m_Reset.m_Hit);
	}

	void OnRemove(DZS_Button b)
	{
		m_Tab.RemoveKey(m_Action, b.m_Tag.ToInt());
	}

	void OnReset(DZS_Button b)
	{
		DZS_Kernel.Get().GetUi().ResetBindings(m_Action);
	}

	void Update(float dt)
	{
		foreach (DZS_KeyField f : m_Fields)
		{
			if (f.m_WarnLeft > 0)
			{
				f.m_WarnLeft -= dt;
				f.Refresh();
			}
		}
	}

	void Destroy()
	{
		foreach (DZS_KeyField f : m_Fields)
			f.m_Hit.Destroy();
		foreach (DZS_Button x : m_Remove)
			x.Destroy();
		m_Reset.Destroy();
		m_Frame.Unlink();
	}
}

class DZS_KeysTab : DZS_HitListener
{
	protected DZS_SettingsDialog m_Dialog;
	protected Widget m_Parent;
	protected ref DZS_SearchField m_Search;
	protected ref DZS_Button m_ConflictLink;
	protected Widget m_List;
	protected ref array<ref DZS_KeyRow> m_Rows;
	protected ref array<Widget> m_Heads;
	protected ref array<float> m_HeadY;
	protected ref array<Widget> m_FixedRows;
	protected TextWidget m_EmptyTitle;
	protected string m_Filter;
	protected bool m_OnlyConflicts;
	protected bool m_Dirty = true;
	protected ref DZS_KeyField m_RecField;
	protected ref map<string, bool> m_RecDown;
	protected string m_RecModOnly;
	protected bool m_RecModOther;
	protected float m_ListH;

	void DZS_KeysTab(Widget parent, DZS_SettingsDialog dialog)
	{
		m_Dialog = dialog;
		m_Parent = parent;
		m_Rows = new array<ref DZS_KeyRow>();
		m_Heads = new array<Widget>();
		m_HeadY = new array<float>();
		m_FixedRows = new array<Widget>();
		m_RecDown = new map<string, bool>();
		m_Search = new DZS_SearchField("keys.search", DZS_Texts.Get("core.keys.search"));
		m_Search.SetWidth(360);
		m_Search.Build(parent);
		m_ConflictLink = new DZS_Button(parent, "ghost", "");
		m_ConflictLink.m_OnClick.Insert(OnConflictLink);
		m_List = DZS_W.Frame(parent);
		DZS_Kernel.Get().GetUi().m_OnBindingsChanged.Insert(OnBindingsChanged);
		Rebuild();
	}

	void OnBindingsChanged()
	{
		m_Dirty = true;
	}

	void OnConflictLink(DZS_Button b)
	{
		m_OnlyConflicts = true;
		m_Dirty = true;
	}

	static string CapsText(string binding)
	{
		array<string> caps = DZS_KeyNames.Caps(binding);
		string s = "";
		foreach (string c : caps)
		{
			if (s != "")
				s += " ";
			s += c;
		}
		return s;
	}

	static string WhenText(DZS_Action a)
	{
		string s = "";
		if (a.m_When.Find("scene") < 0 && a.m_When.Find("always") < 0)
		{
			if (a.m_When.Find("selection") >= 0)
				s = DZS_Texts.Get("core.when.selection");
			else if (a.m_When.Find("hand") >= 0)
				s = DZS_Texts.Get("core.when.hand");
			else if (a.m_When.Find("character") >= 0)
				s = DZS_Texts.Get("core.when.character");
			else if (a.m_When.Find("drag") >= 0)
				s = DZS_Texts.Get("core.when.drag");
		}
		if (a.m_Hold)
		{
			if (s != "")
				s += " · ";
			s += DZS_Texts.Get("core.when.hold");
		}
		return s;
	}

	protected static string Norm(string s)
	{
		string t = s;
		t.ToLower();
		t.Replace("+", " ");
		while (t.Contains("  "))
			t.Replace("  ", " ");
		t.TrimInPlace();
		return t;
	}

	protected bool Matches(DZS_Action a, string q)
	{
		if (q == "")
			return true;
		string n = a.Name();
		n.ToLower();
		if (n.Contains(q))
			return true;
		array<string> binds = DZS_Kernel.Get().GetUi().GetBindings(a.m_Id);
		foreach (string b : binds)
		{
			if (Norm(CapsText(b)) == q)
				return true;
		}
		return false;
	}

	// Warning lines of a row: conflicts, speed keys as modifiers, a default taken by another action.
	array<string> WarningsFor(DZS_Action a)
	{
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		array<string> r = new array<string>();
		array<string> cs = ui.ConflictsOf(a.m_Id);
		foreach (string oid : cs)
		{
			DZS_Action o = ui.GetAction(oid);
			DZS_KeyGroup g = ui.FindGroup(o.m_Group);
			string gname = o.m_Group;
			if (g)
				gname = DZS_Texts.Pick(g.m_NameDe, g.m_NameEn);
			r.Insert(DZS_Texts.Fmt("core.keys.alsoBound", o.Name(), gname));
		}
		if (a.m_Hold && !a.m_Tempo && !a.m_Gesture)
		{
			array<string> binds = ui.GetBindings(a.m_Id);
			foreach (string b : binds)
			{
				array<string> mods = DZS_UiRegistry.ModsOf(b);
				if (mods.Find("Shift") >= 0 && TempoOnKey("LSHIFT"))
					r.Insert(DZS_Texts.Get("core.keys.shiftTempo"));
				if (mods.Find("Alt") >= 0 && TempoOnKey("LMENU"))
					r.Insert(DZS_Texts.Get("core.keys.altTempo"));
			}
		}
		if (a.m_BlockedDefault != "" && a.m_BlockedBy != "")
		{
			DZS_Action by = ui.GetAction(a.m_BlockedBy);
			string byName = a.m_BlockedBy;
			if (by)
				byName = by.Name();
			r.Insert(DZS_Texts.Fmt("core.keys.defaultTaken", CapsText(a.m_BlockedDefault), byName));
		}
		return r;
	}

	protected bool TempoOnKey(string key)
	{
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		array<string> ids = ui.GetActionIds();
		foreach (string id : ids)
		{
			DZS_Action a = ui.GetAction(id);
			if (a.m_Tempo && ui.GetBindings(id).Find(key) >= 0)
				return true;
		}
		return false;
	}

	protected void Rebuild()
	{
		m_Dirty = false;
		foreach (DZS_KeyRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		foreach (Widget h : m_Heads)
		{
			if (h)
				h.Unlink();
		}
		m_Heads.Clear();
		foreach (Widget fr : m_FixedRows)
			fr.Unlink();
		m_FixedRows.Clear();
		if (m_EmptyTitle)
			m_EmptyTitle.Unlink();
		m_EmptyTitle = null;
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		string q = Norm(m_Search.GetText());
		m_Filter = m_Search.GetText();
		int conflicts = ui.CountConflictCombos();
		if (conflicts == 0)
			m_OnlyConflicts = false;
		m_ConflictLink.Show(conflicts > 0);
		if (conflicts == 1)
			m_ConflictLink.SetLabel(DZS_Texts.Get("core.keys.conflict1"));
		else
			m_ConflictLink.SetLabel(DZS_Texts.Fmt("core.keys.conflictN", conflicts.ToString()));
		array<ref DZS_KeyGroup> groups = ui.GetGroups();
		array<DZS_KeyGroup> ordered = new array<DZS_KeyGroup>();
		foreach (DZS_KeyGroup g : groups)
		{
			int at = 0;
			while (at < ordered.Count() && ordered[at].m_Order <= g.m_Order)
				at++;
			ordered.InsertAt(g, at);
		}
		array<string> ids = ui.GetActionIds();
		int shown = 0;
		foreach (DZS_KeyGroup og : ordered)
		{
			bool head = false;
			foreach (string id : ids)
			{
				DZS_Action a = ui.GetAction(id);
				if (!a.m_Listed || a.m_Group != og.m_Key)
					continue;
				if (!Matches(a, q))
					continue;
				if (m_OnlyConflicts && ui.ConflictsOf(id).Count() == 0)
					continue;
				if (!head)
				{
					head = true;
					TextWidget ht = DZS_W.Text(m_List, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
					ht.SetText(DZS_Texts.Upper("// " + DZS_Texts.Pick(og.m_NameDe, og.m_NameEn)));
					m_Heads.Insert(ht);
				}
				m_Rows.Insert(new DZS_KeyRow(m_List, this, a));
				m_Heads.Insert(null);
				shown++;
			}
		}
		// fixed keys, read only
		array<string> fixedKeys = {"esc", "click", "dclick", "rclick", "rdrag", "altrdrag", "wheel", "modclick", "ctrl", "fieldkeys", "fieldctrl"};
		bool fixedHead = false;
		foreach (string fk : fixedKeys)
		{
			string keyText = DZS_Texts.Get("core.fixed." + fk + ".key");
			string what = DZS_Texts.Get("core.fixed." + fk);
			string k1 = Norm(keyText);
			string w1 = what;
			w1.ToLower();
			if (q != "" && !k1.Contains(q) && !w1.Contains(q))
				continue;
			if (m_OnlyConflicts)
				continue;
			if (!fixedHead)
			{
				fixedHead = true;
				TextWidget fh = DZS_W.Text(m_List, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
				fh.SetText(DZS_Texts.Upper("// " + DZS_Texts.Get("core.keys.fixed")));
				m_FixedRows.Insert(fh);
			}
			TextWidget kt = DZS_W.Text(m_List, DZS_Font.MONO, 12, DZS_Color.Get("textMuted"));
			kt.SetText(keyText);
			TextWidget wt = DZS_W.Text(m_List, DZS_Font.READ, 13, DZS_Color.Get("text"));
			wt.SetText(what);
			m_FixedRows.Insert(kt);
			m_FixedRows.Insert(wt);
			shown++;
		}
		if (shown == 0)
		{
			m_EmptyTitle = DZS_W.Text(m_List, DZS_Font.DISPLAY, 24, DZS_Color.Get("textMuted"));
			m_EmptyTitle.SetText(DZS_Texts.Get("core.keys.none"));
		}
		DZS_Log.Info("keys list: " + m_Rows.Count() + " action(s), filter '" + m_Filter + "', conflicts " + conflicts);
	}

	void AddFocus()
	{
		m_Dialog.AddFocusField(m_Search);
		if (m_ConflictLink.m_Hit.m_W.IsVisible())
			m_Dialog.AddFocusHit(m_ConflictLink.m_Hit);
		foreach (DZS_KeyRow r : m_Rows)
			r.AddFocus(m_Dialog);
	}

	float Layout(float width)
	{
		float y = 0;
		m_Search.Place(0, 0, 0);
		m_ConflictLink.Place(DZS_Scale.Px(376), DZS_Scale.Px(2));
		y += DZS_Scale.Px(48);
		DZS_W.Place(m_List, 0, y, width, DZS_Scale.Px(10));
		float listTop = y;
		float ly = 0;
		int ri = 0;
		foreach (Widget h : m_Heads)
		{
			if (h)
			{
				ly += DZS_Scale.Px(8);
				DZS_W.Place(h, 0, ly, width, DZS_Scale.Px(18));
				ly += DZS_Scale.Px(22);
				continue;
			}
			ly += m_Rows[ri].Layout(ly, width);
			ri++;
		}
		int fi = 0;
		while (fi < m_FixedRows.Count())
		{
			Widget w = m_FixedRows[fi];
			TextWidget tw = TextWidget.Cast(w);
			if (fi == 0)
			{
				ly += DZS_Scale.Px(16);
				DZS_W.Place(w, 0, ly, width, DZS_Scale.Px(18));
				ly += DZS_Scale.Px(24);
				fi++;
				continue;
			}
			DZS_W.Place(m_FixedRows[fi], 0, ly, DZS_Scale.Px(300), DZS_Scale.Px(26));
			DZS_W.Place(m_FixedRows[fi + 1], DZS_Scale.Px(310), ly, width - DZS_Scale.Px(310), DZS_Scale.Px(26));
			ly += DZS_Scale.Px(28);
			fi += 2;
		}
		if (m_EmptyTitle)
		{
			DZS_W.Place(m_EmptyTitle, 0, ly + DZS_Scale.Px(16), width, DZS_Scale.Px(36));
			ly += DZS_Scale.Px(60);
		}
		DZS_W.Place(m_List, 0, listTop, width, Math.Max(ly, 1));
		return y + ly;
	}

	void Update(float dt)
	{
		if (m_Search.GetText() != m_Filter)
		{
			m_OnlyConflicts = false;
			m_Dirty = true;
		}
		m_Search.RefreshExtras();
		if (m_Dirty && !m_RecField)
		{
			Rebuild();
			m_Dialog.RefocusKeys();
		}
		foreach (DZS_KeyRow r : m_Rows)
			r.Update(dt);
		if (m_RecField)
			Record();
	}

	void RemoveKey(string action, int slot)
	{
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		array<string> b = new array<string>();
		b.Copy(ui.GetBindings(action));
		if (slot < b.Count())
			b.RemoveOrdered(slot);
		ui.SetBindings(action, b);
	}

	// ---- recording ----

	bool IsRecording() { return m_RecField != null; }

	void StartRecording(DZS_KeyField f)
	{
		if (m_RecField)
			CancelRecording();
		m_RecField = f;
		f.m_Recording = true;
		f.m_WarnLeft = 0;
		f.Refresh();
		m_RecDown.Clear();
		map<string, int> codes = DZS_UiRegistry.KeyCodes();
		foreach (string name, int code : codes)
		{
			if (KeyState(code) != 0)
				m_RecDown.Set(name, true);
		}
		array<string> mouse = {"MouseMiddle", "Mouse4", "Mouse5"};
		foreach (string m : mouse)
		{
			if (DZS_UiRegistry.KeyDown(m))
				m_RecDown.Set(m, true);
		}
		m_RecModOnly = "";
		m_RecModOther = false;
		DZS_Kernel.Get().GetScreen().SetRecorder(this);
		DZS_Log.Info("recording key for " + f.m_Action + " slot " + f.m_Slot);
	}

	void CancelRecording()
	{
		if (!m_RecField)
			return;
		DZS_Log.Info("recording cancelled");
		EndRecording();
	}

	protected void EndRecording()
	{
		DZS_KeyField f = m_RecField;
		m_RecField = null;
		f.m_Recording = false;
		f.m_WarnLeft = 0;
		f.Refresh();
		DZS_Kernel.Get().GetScreen().SetRecorder(null);
	}

	protected static string ModsPrefix()
	{
		string p = "";
		if (DZS_UiRegistry.ModDown("Ctrl"))
			p += "Ctrl+";
		if (DZS_UiRegistry.ModDown("Shift"))
			p += "Shift+";
		if (DZS_UiRegistry.ModDown("Alt"))
			p += "Alt+";
		return p;
	}

	// The first combination of modifiers and another key, at the moment that key goes down.
	protected void Record()
	{
		map<string, int> codes = DZS_UiRegistry.KeyCodes();
		string pressed = "";
		foreach (string name, int code : codes)
		{
			bool down = KeyState(code) != 0;
			bool was = m_RecDown.Contains(name);
			if (down && !was)
			{
				m_RecDown.Set(name, true);
				if (name == "ESCAPE")
					continue;
				if (DZS_UiRegistry.IsModifierKey(name))
				{
					if (m_RecModOnly == "")
						m_RecModOnly = name;
					else
						m_RecModOther = true;
					continue;
				}
				if (pressed == "")
					pressed = name;
				m_RecModOther = true;
			}
			else if (!down && was)
			{
				m_RecDown.Remove(name);
				if (name == m_RecModOnly)
				{
					if (!m_RecModOther)
					{
						if (TryModifierAlone(name))
							return;
					}
					m_RecModOnly = "";
					m_RecModOther = false;
				}
			}
		}
		array<string> mouse = {"MouseMiddle", "Mouse4", "Mouse5"};
		foreach (string m : mouse)
		{
			bool md = DZS_UiRegistry.KeyDown(m);
			bool mw = m_RecDown.Contains(m);
			if (md && !mw)
			{
				m_RecDown.Set(m, true);
				if (pressed == "")
					pressed = m;
				m_RecModOther = true;
			}
			else if (!md && mw)
				m_RecDown.Remove(m);
		}
		if (pressed != "")
			Offer(ModsPrefix() + pressed);
	}

	// A modifier alone counts only for hold actions, as exactly this key ("Right Shift").
	protected bool TryModifierAlone(string key)
	{
		DZS_Action a = DZS_Kernel.Get().GetUi().GetAction(m_RecField.m_Action);
		if (!a || !a.m_Hold)
			return false;
		if (a.m_Tempo && (key == "LCONTROL" || key == "RCONTROL"))
		{
			Reject(DZS_Texts.Get("core.keys.onlySingle"));
			return false;
		}
		if (a.m_NoCtrl && (key == "LCONTROL" || key == "RCONTROL"))
		{
			Reject(DZS_Texts.Get("core.keys.ctrlStops"));
			return false;
		}
		Accept(key);
		return true;
	}

	protected void Offer(string binding)
	{
		DZS_Action a = DZS_Kernel.Get().GetUi().GetAction(m_RecField.m_Action);
		array<string> mods = DZS_UiRegistry.ModsOf(binding);
		if (a.m_NoCtrl && mods.Find("Ctrl") >= 0)
		{
			Reject(DZS_Texts.Get("core.keys.ctrlStops"));
			return;
		}
		if (a.m_Tempo && mods.Count() > 0)
		{
			Reject(DZS_Texts.Get("core.keys.onlySingle"));
			return;
		}
		Accept(binding);
	}

	protected void Reject(string why)
	{
		m_RecField.m_Warn = why;
		m_RecField.m_WarnLeft = 2.0;
		m_RecField.Refresh();
		DZS_Log.Info("recording: not taken (" + why + ")");
	}

	protected void Accept(string binding)
	{
		DZS_UiRegistry ui = DZS_Kernel.Get().GetUi();
		string id = m_RecField.m_Action;
		int slot = m_RecField.m_Slot;
		array<string> b = new array<string>();
		b.Copy(ui.GetBindings(id));
		DZS_Log.Info("recorded " + binding + " for " + id + " slot " + slot);
		EndRecording();
		int other = 1 - slot;
		if (other < b.Count() && b[other] == binding)
			return;
		if (slot < b.Count())
			b[slot] = binding;
		else
			b.Insert(binding);
		ui.SetBindings(id, b);
	}

	void Destroy()
	{
		if (m_RecField)
			EndRecording();
		DZS_Kernel.Get().GetUi().m_OnBindingsChanged.Remove(OnBindingsChanged);
		foreach (DZS_KeyRow r : m_Rows)
			r.Destroy();
		m_Search.Destroy();
		m_ConflictLink.Destroy();
	}
}

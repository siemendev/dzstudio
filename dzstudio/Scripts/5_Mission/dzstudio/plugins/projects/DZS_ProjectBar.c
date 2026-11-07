// One line of the group list: group or object row with the knobs at its end.
class DZS_GroupRow : DZS_HitListener
{
	DZS_ProjectBar m_Bar;
	ref DZS_Hit m_Hit;
	ref DZS_Hit m_Fold;
	Widget m_Tint;
	Widget m_Lead;
	Widget m_Swatch;
	ImageWidget m_FoldSign;
	string m_FoldIcon = "chevron-down";
	TextWidget m_Name;
	TextWidget m_Sub;
	TextWidget m_Mark;
	TextWidget m_Holder;
	ref DZS_Border m_HolderBox;
	ref DZS_Button m_SelectAll;
	ref DZS_Button m_HideB;
	ref DZS_Button m_LockB;
	string m_Type;
	string m_Group;
	string m_Class;
	string m_Obj;
	// what an object row was last placed with; the same again places nothing
	string m_Sig;

	void DZS_GroupRow(Widget parent, DZS_ProjectBar bar)
	{
		m_Bar = bar;
		m_Hit = new DZS_Hit(parent, this, "projects.row");
		m_Hit.m_FocusKind = "";
		Widget f = m_Hit.m_W;
		m_Tint = DZS_W.Panel(f, DZS_Color.Get("accentTint"));
		m_Lead = DZS_W.Panel(f, DZS_Color.Get("accent"));
		m_Swatch = DZS_W.Panel(f, 0);
		m_Fold = new DZS_Hit(f, this, "projects.fold");
		m_Fold.m_FocusKind = "";
		m_FoldSign = DZS_W.Icon(m_Fold.m_W, "chevron-down", DZS_Color.Get("text"));
		m_Name = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_Sub = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Mark = DZS_W.Text(f, DZS_Font.LABELBOLD, 10, DZS_Color.Get("accentText"), false, true);
		m_Holder = DZS_W.Text(f, DZS_Font.READ, 11, DZS_Color.Get("text"), true);
		m_HolderBox = new DZS_Border(f, 0);
		ShowHolder(false);
		m_SelectAll = Knob(f, "mouse-pointer-2", "select");
		m_HideB = Knob(f, "eye", "hide");
		m_LockB = Knob(f, "lock-open", "lock");
	}

	protected DZS_Button Knob(Widget f, string icon, string tag)
	{
		DZS_Button b = new DZS_Button(f, "ghost", "", icon, 24);
		b.m_Tag = tag;
		b.m_Hit.m_FocusKind = "";
		b.m_OnClick.Insert(OnKnob);
		if (tag != "select")
		{
			b.m_OnPress = new ScriptInvoker();
			b.m_OnPress.Insert(OnKnobPress);
		}
		return b;
	}

	void OnKnob(DZS_Button b)
	{
		m_Bar.OnRowKnob(this, b.m_Tag);
	}

	void OnKnobPress(DZS_Button b)
	{
		m_Bar.StartSweep(this, b.m_Tag);
	}

	DZS_Button KnobOf(string what)
	{
		if (what == "hide")
			return m_HideB;
		if (what == "lock")
			return m_LockB;
		return null;
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (h == m_Fold)
		{
			m_Bar.OnFold(this);
			return;
		}
		m_Bar.OnRowClick(this, button);
	}

	override void OnHitState(DZS_Hit h)
	{
		if (h == m_Hit && m_Obj != "")
		{
			DZS_SelectionApi sel = DZS_SelectionApi.Get();
			if (sel)
			{
				if (m_Hit.m_Hover)
					sel.SetListHover(m_Obj);
				else
					sel.SetListHover("");
			}
		}
	}

	void Show(bool s)
	{
		m_Hit.m_W.Show(s);
	}

	void ShowHolder(bool s)
	{
		m_Holder.Show(s);
		m_HolderBox.Show(s);
	}
}

// Upper part of the project bar: project head, meta line, the groups with their
// objects, one row each.
class DZS_ProjectBar : DZS_PanelContent
{
	protected DZS_ProjectsPlugin m_P;
	protected Widget m_Parent;
	protected TextWidget m_Name;
	protected TextWidget m_Meta;
	protected ref array<ref DZS_GroupRow> m_Rows;
	protected ref map<string, bool> m_Closed;
	protected DZS_Instance m_FoldInst;
	protected string m_LastClickGroup;
	protected string m_LastClickObj;
	protected ref map<string, int> m_Dups;
	protected ref map<string, string> m_GroupCountKey = new map<string, string>();
	protected ref map<string, int> m_GroupCount = new map<string, int>();
	protected ref map<string, int> m_GroupSelected = new map<string, int>();
	protected ref map<string, int> m_GroupDups = new map<string, int>();
	protected string m_DupSel;

	void DZS_ProjectBar(DZS_ProjectsPlugin p)
	{
		m_P = p;
		m_Rows = new array<ref DZS_GroupRow>();
		m_Closed = new map<string, bool>();
	}

	override void Build(Widget parent)
	{
		m_Parent = parent;
		m_Name = DZS_W.Text(parent, DZS_Font.DISPLAY, 24, DZS_Color.Get("text"));
		m_Meta = DZS_W.Text(parent, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
	}

	protected DZS_GroupRow Row(int i)
	{
		while (m_Rows.Count() <= i)
			m_Rows.Insert(new DZS_GroupRow(m_Parent, this));
		DZS_GroupRow r = m_Rows[i];
		r.Show(true);
		return r;
	}

	// Sweeping: pressing a hide or lock knob and moving over more knobs of that kind gives them
	// all the state the first one got, as one undo step.
	protected string m_SweepWhat;
	protected bool m_SweepTarget;
	protected string m_SweepKey;
	protected int m_SweepCount;
	protected ref map<string, bool> m_SweepDone = new map<string, bool>();

	void StartSweep(DZS_GroupRow r, string what)
	{
		DZS_Instance inst = m_P.Kernel().GetShown();
		if (!inst || r.m_Type == "ext" || r.m_Type == "")
			return;
		array<string> targets = RowTargets(inst, r);
		bool target = false;
		foreach (string t : targets)
		{
			if (what == "hide" && !inst.IsHidden(t))
				target = true;
			if (what == "lock" && !inst.IsLocked(t))
				target = true;
		}
		m_SweepCount++;
		m_SweepWhat = what;
		m_SweepTarget = target;
		m_SweepKey = "gesture:sweep:" + m_SweepCount;
		m_SweepDone.Clear();
		SweepRow(inst, r);
	}

	protected string RowKey(DZS_GroupRow r)
	{
		return r.m_Type + "|" + r.m_Group + "|" + r.m_Class + "|" + r.m_Obj;
	}

	protected array<string> RowTargets(DZS_Instance inst, DZS_GroupRow r)
	{
		array<string> targets = new array<string>();
		if (r.m_Type == "group")
			targets.Insert(r.m_Group);
		else if (r.m_Obj != "")
			targets.Insert(r.m_Obj);
		return targets;
	}

	protected void SweepRow(DZS_Instance inst, DZS_GroupRow r)
	{
		string key = RowKey(r);
		if (m_SweepDone.Contains(key))
			return;
		m_SweepDone.Set(key, true);
		array<string> targets = RowTargets(inst, r);
		bool hide = m_SweepWhat == "hide";
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		foreach (string t : targets)
		{
			if (hide && inst.IsHidden(t) != m_SweepTarget)
				b.SetHidden(t, m_SweepTarget);
			if (!hide && inst.IsLocked(t) != m_SweepTarget)
				b.SetLocked(t, m_SweepTarget);
		}
		string de = "Entsperren";
		string en = "Unlock";
		if (hide && m_SweepTarget)
		{
			de = "Ausblenden";
			en = "Hide";
		}
		else if (hide)
		{
			de = "Einblenden";
			en = "Show";
		}
		else if (m_SweepTarget)
		{
			de = "Sperren";
			en = "Lock";
		}
		DZS_SubmitResult res = b.Commit(de, en, m_SweepKey);
		DZS_Log.Info("projects: sweep " + en + " " + r.m_Type + " (" + targets.Count() + ") ok " + res.Ok() + ", rows " + m_SweepDone.Count());
	}

	protected void UpdateSweep(DZS_Instance inst)
	{
		if (m_SweepWhat == "")
			return;
		if ((GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) == 0)
		{
			m_SweepWhat = "";
			return;
		}
		int mx, my;
		GetMousePos(mx, my);
		foreach (DZS_GroupRow r : m_Rows)
		{
			if (r.m_Type == "ext" || r.m_Type == "" || !r.m_Hit.m_W.IsVisible())
				continue;
			DZS_Button k = r.KnobOf(m_SweepWhat);
			if (!k || !k.GetWidget().IsVisible())
				continue;
			float x, y, w, h;
			k.GetWidget().GetScreenPos(x, y);
			k.GetWidget().GetScreenSize(w, h);
			if (mx >= x && mx < x + w && my >= y && my < y + h)
				SweepRow(inst, r);
		}
	}

	override float Layout(float width)
	{
		if (!m_Name)
			return 0;
		DZS_Instance inst = m_P.Kernel().GetShown();
		if (inst)
			UpdateSweep(inst);
		float pad = DZS_Scale.Px(12);
		if (!inst || !m_P.m_Open)
		{
			m_Name.SetText(DZS_Texts.Pick("Kein Projekt", "No project"));
			m_Meta.SetText("");
			foreach (DZS_GroupRow hr : m_Rows)
				hr.Show(false);
			DZS_W.Place(m_Name, pad, DZS_Scale.Px(8), width - 2 * pad, DZS_Scale.Px(28));
			return DZS_Scale.Px(44);
		}
		if (inst != m_FoldInst)
		{
			m_FoldInst = inst;
			m_Closed.Clear();
		}
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		string active = DZS_ActiveGroup.Get(inst);
		UpdateDuplicates(inst, sel);
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		int n = 0;
		foreach (string g0 : groups)
		{
			if (inst.GetKind(g0) == DZS_KIND_GROUP)
				n++;
		}
		array<DZS_BarGroup> exts = DZS_BarGroups.All();
		foreach (DZS_BarGroup eg0 : exts)
		{
			if (eg0.IsShown(inst))
				n++;
		}
		m_Name.SetText(DZS_W.Literal(m_P.m_Open.m_Name));
		string one = DZS_Texts.Pick(" Gruppe", " group");
		string many = DZS_Texts.Pick(" Gruppen", " groups");
		string groupWord = DZS_Issues.Plural(n, one, many);
		string localText = m_P.ProviderLabel(m_P.m_Open.m_Folder);
		m_Meta.SetText(localText + " · " + DZS_Maps.Display(m_P.m_Open.m_MapKey) + " · " + n + groupWord);
		DZS_W.Place(m_Name, pad, DZS_Scale.Px(8), width - 2 * pad, DZS_Scale.Px(28));
		DZS_W.Place(m_Meta, pad, DZS_Scale.Px(38), width - 2 * pad, DZS_Scale.Px(18));
		float y = DZS_Scale.Px(64);
		int ri = 0;
		foreach (string gid : groups)
		{
			if (inst.GetKind(gid) != DZS_KIND_GROUP)
				continue;
			DZS_GroupRow gr = Row(ri);
			ri++;
			PlaceGroup(inst, gr, gid, gid == active, width, y, sel);
			y += DZS_Scale.Px(40);
			if (m_Closed.Contains(gid))
				continue;
			// one row per object in group order, kinds counted on ("Fass", "Fass 2" …); rows only where the section shows
			DZS_BarLayout lay = LayoutOf(inst, gid);
			float viewTop = m_SectionTop - DZS_Scale.Px(80);
			float viewEnd = m_SectionTop + m_SectionH + DZS_Scale.Px(80);
			float y0 = y;
			int count = lay.m_Ids.Count();
			for (int oi = lay.FirstBelow(viewTop - y0); oi < count; oi++)
			{
				float oy = y0 + lay.m_Top[oi];
				if (oy > viewEnd)
					break;
				DZS_GroupRow orow = Row(ri);
				ri++;
				PlaceObject(inst, orow, gid, lay.m_Cls[oi], lay.m_Ids[oi], lay.m_Nth[oi], lay.m_H[oi], width, oy, sel);
			}
			y = y0 + lay.m_Total;
		}
		foreach (DZS_BarGroup eg : exts)
		{
			if (!eg.IsShown(inst))
				continue;
			DZS_GroupRow er = Row(ri);
			ri++;
			PlaceExt(inst, er, eg, width, y);
			y += DZS_Scale.Px(40);
			if (m_Closed.Contains("ext:" + eg.GetKey()))
				continue;
			array<ref DZS_BarGroupRow> erows = new array<ref DZS_BarGroupRow>();
			eg.GetRows(inst, erows);
			foreach (DZS_BarGroupRow item : erows)
			{
				DZS_GroupRow xr = Row(ri);
				ri++;
				PlaceExtRow(xr, eg, item, width, y);
				y += DZS_Scale.Px(26);
			}
		}
		for (int h = ri; h < m_Rows.Count(); h++)
			m_Rows[h].Show(false);
		return y + DZS_Scale.Px(8);
	}

	protected void Knobs(DZS_GroupRow r, float width, float rowH, bool withSelect, bool hidden, bool locked, bool selectOn)
	{
		float kw = DZS_Scale.Px(24);
		float pad = DZS_Scale.Px(8);
		float y = (rowH - kw) / 2;
		r.m_LockB.Place(width - pad - kw, y);
		r.m_HideB.Place(width - pad - 2 * kw, y);
		r.m_LockB.GetWidget().Show(true);
		r.m_HideB.GetWidget().Show(true);
		r.m_LockB.SetEnabled(true);
		r.m_HideB.SetEnabled(true);
		r.m_SelectAll.Place(width - pad - 3 * kw, y);
		r.m_SelectAll.GetWidget().Show(withSelect);
		r.m_SelectAll.SetEnabled(selectOn);
		if (hidden)
			r.m_HideB.SetIcon("eye-off");
		else
			r.m_HideB.SetIcon("eye");
		if (locked)
			r.m_LockB.SetIcon("lock");
		else
			r.m_LockB.SetIcon("lock-open");
		r.m_HideB.SetOn(hidden);
		r.m_LockB.SetOn(locked);
	}

	protected void PlaceGroup(DZS_Instance inst, DZS_GroupRow r, string gid, bool isActive, float width, float y, DZS_SelectionApi sel)
	{
		r.m_Sig = "";
		r.m_Type = "group";
		r.m_Group = gid;
		r.m_Class = "";
		r.m_Obj = "";
		float rowH = DZS_Scale.Px(40);
		r.m_Hit.Place(0, y, width, rowH);
		float pad = DZS_Scale.Px(12);
		string name, color, file, unreadable;
		inst.GetField(gid, DZS_Obj.GROUP_NAME, name);
		inst.GetField(gid, DZS_Obj.GROUP_COLOR, color);
		inst.GetField(gid, DZS_Obj.GROUP_FILE, file);
		inst.GetField(gid, "unreadable", unreadable);
		string countKey = m_P.m_StateVer.ToString() + "|" + m_DupGen + "|" + gid;
		bool closed = m_Closed.Contains(gid);
		if (closed)
			countKey += "|closed";
		if (sel && closed)
			countKey += "|" + sel.Signature();
		int count = 0;
		int selected = 0;
		int dupCount = 0;
		if (m_GroupCountKey.Get(gid) == countKey)
		{
			count = m_GroupCount.Get(gid);
			selected = m_GroupSelected.Get(gid);
			dupCount = m_GroupDups.Get(gid);
		}
		else
		{
			array<string> ids = inst.GetChildren(gid);
			count = ids.Count();
			if (sel && closed)
			{
				foreach (string id : ids)
				{
					if (sel.IsSelected(id))
						selected++;
				}
			}
			if (closed)
				dupCount = CountDups(ids);
			m_GroupCountKey.Set(gid, countKey);
			m_GroupCount.Set(gid, count);
			m_GroupSelected.Set(gid, selected);
			m_GroupDups.Set(gid, dupCount);
		}
		bool hidden = inst.IsHidden(gid);
		bool locked = inst.IsLocked(gid);
		r.m_Tint.Show(isActive);
		r.m_Lead.Show(isActive);
		DZS_W.Place(r.m_Tint, 0, 0, width, rowH);
		DZS_W.Place(r.m_Lead, 0, 0, DZS_Scale.Px(3), rowH);
		r.m_Swatch.Show(true);
		if (DZS_Palette.Valid(color))
			r.m_Swatch.SetColor(DZS_Palette.ToARGB(color));
		float sq = DZS_Scale.Px(8);
		float fold = PlaceFold(r, pad, rowH, !m_Closed.Contains(gid));
		float tx = pad + fold + DZS_Scale.Px(2);
		DZS_W.Place(r.m_Swatch, tx, DZS_Scale.Px(8), sq, sq);
		r.m_Name.SetText(DZS_W.Literal(name));
		if (hidden)
			r.m_Name.SetColor(DZS_Color.Get("textQuiet"));
		else
			r.m_Name.SetColor(DZS_Color.Get("text"));
		float textW = width - tx - DZS_Scale.Px(14) - DZS_Scale.Px(8) - DZS_Scale.Px(24) * 3 - DZS_Scale.Px(50);
		DZS_W.Place(r.m_Name, tx + DZS_Scale.Px(14), DZS_Scale.Px(2), textW, DZS_Scale.Px(20));
		string sub = file + " · " + count;
		if (unreadable == "1")
			sub = file + " · " + DZS_Texts.Pick("nicht lesbar", "not readable");
		if (selected > 0 && m_Closed.Contains(gid))
			sub += " · " + selected + DZS_Texts.Pick(" ausgewählt", " selected");
		if (dupCount > 0 && m_Closed.Contains(gid))
			sub += " · " + dupCount + DZS_Texts.Pick(" doppelt", " duplicate");
		if (m_Closed.Contains(gid))
			sub += HeldBy(inst, gid);
		r.m_Sub.SetText(DZS_W.Literal(sub));
		DZS_W.Place(r.m_Sub, tx + DZS_Scale.Px(14), DZS_Scale.Px(20), textW + DZS_Scale.Px(50), DZS_Scale.Px(18));
		string mark = "";
		if (isActive)
			mark = DZS_Texts.Pick("aktiv", "active");
		r.m_Mark.SetText(DZS_Texts.Upper(mark));
		DZS_W.Place(r.m_Mark, width - DZS_Scale.Px(8) - DZS_Scale.Px(24) * 3 - DZS_Scale.Px(54), 0, DZS_Scale.Px(50), DZS_Scale.Px(20));
		r.m_Mark.Show(true);
		r.m_Sub.Show(true);
		r.ShowHolder(false);
		Knobs(r, width, rowH, true, hidden, locked, !hidden && !locked && unreadable != "1");
		string holder = GroupHolder(inst, gid);
		if (holder != "")
		{
			string why = DZS_Texts.Pick(holder + " bearbeitet Objekte dieser Gruppe", holder + " is editing objects of this group");
			r.m_HideB.SetEnabled(false, why);
			r.m_LockB.SetEnabled(false, why);
		}
	}

	// A plugin's group (the loot group): no colour, no object count, no "select all", always last.
	protected void PlaceExt(DZS_Instance inst, DZS_GroupRow r, DZS_BarGroup g, float width, float y)
	{
		r.m_Sig = "";
		r.m_Type = "ext";
		r.ShowHolder(false);
		r.m_Group = g.GetKey();
		r.m_Class = "";
		r.m_Obj = "";
		float rowH = DZS_Scale.Px(40);
		r.m_Hit.Place(0, y, width, rowH);
		float pad = DZS_Scale.Px(12);
		bool hidden = g.IsHidden(inst);
		bool locked = g.IsLocked(inst);
		r.m_Tint.Show(false);
		r.m_Lead.Show(false);
		r.m_Swatch.Show(false);
		float fold = PlaceFold(r, pad, rowH, !m_Closed.Contains("ext:" + g.GetKey()));
		float tx = pad + fold + DZS_Scale.Px(2);
		r.m_Name.SetText(DZS_W.Literal(g.GetName()));
		if (hidden)
			r.m_Name.SetColor(DZS_Color.Get("textQuiet"));
		else
			r.m_Name.SetColor(DZS_Color.Get("text"));
		float textW = width - tx - DZS_Scale.Px(14) - DZS_Scale.Px(8) - DZS_Scale.Px(24) * 3 - DZS_Scale.Px(50);
		DZS_W.Place(r.m_Name, tx + DZS_Scale.Px(14), DZS_Scale.Px(2), textW, DZS_Scale.Px(20));
		r.m_Sub.SetText(DZS_W.Literal(g.GetSub(inst)));
		DZS_W.Place(r.m_Sub, tx + DZS_Scale.Px(14), DZS_Scale.Px(20), textW + DZS_Scale.Px(50), DZS_Scale.Px(18));
		r.m_Sub.Show(true);
		r.m_Mark.Show(false);
		Knobs(r, width, rowH, false, hidden, locked, false);
		string rd, re;
		bool on = g.KnobsEnabled(inst, rd, re);
		string reason = DZS_Texts.Pick(rd, re);
		r.m_HideB.SetEnabled(on, reason);
		r.m_LockB.SetEnabled(on, reason);
	}

	protected void PlaceExtRow(DZS_GroupRow r, DZS_BarGroup g, DZS_BarGroupRow item, float width, float y)
	{
		r.m_Sig = "";
		r.m_Type = "extrow";
		r.ShowHolder(false);
		r.m_Group = g.GetKey();
		r.m_Class = item.m_Key;
		r.m_Obj = "";
		float rowH = DZS_Scale.Px(26);
		r.m_Hit.Place(0, y, width, rowH);
		r.m_Tint.Show(item.m_Lead);
		r.m_Lead.Show(item.m_Lead);
		DZS_W.Place(r.m_Tint, 0, 0, width, rowH);
		DZS_W.Place(r.m_Lead, 0, 0, DZS_Scale.Px(3), rowH);
		r.m_Swatch.Show(false);
		r.m_Fold.m_W.Show(false);
		float tx = DZS_Scale.Px(24 + 20);
		r.m_Mark.SetText(DZS_Texts.Upper(item.m_Mark));
		r.m_Mark.Show(item.m_Mark != "");
		float mw = 0;
		if (item.m_Mark != "")
			mw = DZS_W.TextWidth(r.m_Mark) + DZS_Scale.Px(4);
		DZS_W.Place(r.m_Mark, width - DZS_Scale.Px(8) - mw, 0, mw, rowH);
		r.m_Name.SetText(DZS_W.Literal(item.m_Name));
		r.m_Name.SetColor(DZS_Color.Get("text"));
		float nw = DZS_W.TextWidth(r.m_Name);
		float maxName = width - tx - mw - DZS_Scale.Px(84);
		DZS_W.Place(r.m_Name, tx, 0, Math.Min(nw + 2, maxName), rowH);
		r.m_Sub.SetText(DZS_W.Literal(item.m_Sub));
		r.m_Sub.Show(true);
		float sx = tx + Math.Min(nw, maxName) + DZS_Scale.Px(8);
		DZS_W.Place(r.m_Sub, sx, 0, width - DZS_Scale.Px(12) - mw - sx, rowH);
		r.m_SelectAll.GetWidget().Show(false);
		r.m_HideB.GetWidget().Show(false);
		r.m_LockB.GetWidget().Show(false);
	}

	protected DZS_BarGroup ExtGroup(string key)
	{
		array<DZS_BarGroup> exts = DZS_BarGroups.All();
		foreach (DZS_BarGroup g : exts)
		{
			if (g.GetKey() == key)
				return g;
		}
		return null;
	}

	// Fold arrow at the left of a group row, clickable over the whole row height; returns its width.
	protected float PlaceFold(DZS_GroupRow r, float pad, float rowH, bool isOpen)
	{
		float fw = DZS_Scale.Px(22);
		float isz = DZS_Scale.Px(14);
		r.m_Fold.m_W.Show(true);
		r.m_Fold.Place(pad - DZS_Scale.Px(6), 0, fw, rowH);
		DZS_W.Place(r.m_FoldSign, (fw - isz) / 2, (rowH - isz) / 2, isz, isz);
		string icon = "chevron-right";
		if (isOpen)
			icon = "chevron-down";
		if (icon != r.m_FoldIcon)
		{
			r.m_FoldIcon = icon;
			DZS_W.SetIcon(r.m_FoldSign, icon);
		}
		return fw - DZS_Scale.Px(6);
	}

	// Rows of kinds whose game name other catalog objects share carry the class name below.
	protected ref map<string, ref DZS_BarLayout> m_Layouts = new map<string, ref DZS_BarLayout>();

	// rows of a group's objects, rebuilt when the state changes; tops again only when a kind's row height changes
	protected DZS_BarLayout LayoutOf(DZS_Instance inst, string gid)
	{
		DZS_BarLayout lay = m_Layouts.Get(gid);
		if (!lay)
		{
			lay = new DZS_BarLayout();
			m_Layouts.Set(gid, lay);
		}
		int ver = m_P.m_StateVer;
		if (lay.m_Ver != ver || lay.m_Inst != inst)
		{
			int lt0 = TickCount(0);
			lay.m_Ver = ver;
			lay.m_Inst = inst;
			lay.m_HeightSig = "-";
			lay.m_Ids = inst.GetChildren(gid);
			lay.m_Cls.Clear();
			lay.m_Nth.Clear();
			lay.m_Kinds.Clear();
			map<string, int> nth = new map<string, int>();
			foreach (string oid : lay.m_Ids)
			{
				string cls;
				inst.GetField(oid, DZS_Obj.NAME, cls);
				int k = nth.Get(cls) + 1;
				nth.Set(cls, k);
				if (k == 1)
					lay.m_Kinds.Insert(cls);
				lay.m_Cls.Insert(cls);
				lay.m_Nth.Insert(k);
			}
			int lms = (TickCount(0) - lt0) / 10000;
			if (lms >= 10)
				DZS_Log.Info("project bar: rows of " + gid + " in " + lms + " ms");
		}
		string sig = "";
		map<string, float> heights = new map<string, float>();
		foreach (string kind : lay.m_Kinds)
		{
			float kh = ObjectRowH(kind);
			heights.Set(kind, kh);
			sig += kh.ToString() + ",";
		}
		if (sig != lay.m_HeightSig)
		{
			lay.m_HeightSig = sig;
			lay.m_H.Clear();
			lay.m_Top.Clear();
			float top = 0;
			foreach (string c : lay.m_Cls)
			{
				float h = heights.Get(c);
				lay.m_Top.Insert(top);
				lay.m_H.Insert(h);
				top += h;
			}
			lay.m_Total = top;
		}
		return lay;
	}

	protected float ObjectRowH(string cls)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat && cat.NameShared(cls))
			return DZS_Scale.Px(40);
		return DZS_Scale.Px(26);
	}

	protected void PlaceObject(DZS_Instance inst, DZS_GroupRow r, string gid, string cls, string id, int nth, float rowH, float width, float y, DZS_SelectionApi sel)
	{
		bool selected = sel && sel.IsSelected(id);
		bool hover = sel && sel.GetHover() == id;
		bool hidden = inst.IsHidden(id);
		bool locked = inst.IsLocked(id);
		bool isDup = m_Dups && m_Dups.Contains(id);
		string holder = "";
		int holderColor = 0;
		DZS_SelectGuard guard = DZS_SelectGuard.Get();
		if (guard && guard.Blocks(id))
		{
			holder = guard.HolderName(id);
			holderColor = guard.HolderColor(id);
		}
		string label = Shown(cls);
		if (nth > 1)
			label = label + " " + nth.ToString();
		string sig = id + "|" + gid + "|" + label + "|" + y + "|" + width + "|" + rowH + "|" + selected + hover + hidden + locked + isDup;
		sig += "|" + holder + "|" + holderColor + "|" + DZS_Scale.Px(26) + "|" + DZS_Color.Get("text") + "|" + DZS_Texts.Pick("d", "e");
		if (sig == r.m_Sig)
			return;
		r.m_Sig = sig;
		r.m_Type = "object";
		r.m_Group = gid;
		r.m_Class = cls;
		r.m_Obj = id;
		r.m_Hit.Place(0, y, width, rowH);
		r.m_Tint.Show(selected || hover);
		r.m_Lead.Show(selected);
		DZS_W.Place(r.m_Tint, 0, 0, width, rowH);
		DZS_W.Place(r.m_Lead, 0, 0, DZS_Scale.Px(3), rowH);
		r.m_Swatch.Show(false);
		r.m_Fold.m_W.Show(false);
		float tx = DZS_Scale.Px(24 + 20);
		r.m_Name.SetText(DZS_W.Literal(label));
		if (hidden)
			r.m_Name.SetColor(DZS_Color.Get("textQuiet"));
		else
			r.m_Name.SetColor(DZS_Color.Get("text"));
		float lineH = DZS_Scale.Px(26);
		float maxName = width - tx - DZS_Scale.Px(130);
		DZS_W.Place(r.m_Name, tx, 0, maxName, lineH);
		bool twoLines = rowH > lineH + 1;
		r.m_Sub.Show(twoLines);
		if (twoLines)
		{
			DZS_W.FitText(r.m_Sub, cls, maxName);
			DZS_W.Place(r.m_Sub, tx, DZS_Scale.Px(18), maxName, DZS_Scale.Px(18));
		}
		r.m_Mark.Show(isDup);
		if (isDup)
		{
			r.m_Mark.SetText(DZS_Texts.Upper(DZS_Texts.Pick("doppelt", "duplicate")));
			DZS_W.Place(r.m_Mark, width - DZS_Scale.Px(8) - DZS_Scale.Px(24) * 2 - DZS_Scale.Px(70), 0, DZS_Scale.Px(66), lineH);
		}
		r.ShowHolder(holder != "");
		if (holder != "")
		{
			r.m_Holder.SetText(DZS_W.Literal(holder));
			float hw = DZS_W.TextWidth(r.m_Holder) + DZS_Scale.Px(10);
			float hx = width - DZS_Scale.Px(8) - DZS_Scale.Px(24) * 2 - DZS_Scale.Px(6) - hw;
			if (isDup)
				hx -= DZS_Scale.Px(72);
			float hh = DZS_Scale.Px(16);
			float hy = (lineH - hh) / 2;
			DZS_W.Place(r.m_Holder, hx, hy, hw, hh);
			r.m_HolderBox.SetColor(holderColor);
			r.m_HolderBox.Place(hx, hy, hw, hh, Math.Max(1, DZS_Scale.Px(1)));
		}
		Knobs(r, width, lineH, false, hidden, locked, false);
	}

	// Exact duplicates: same name, position within 1 mm on each
	// axis, turn within 0.01°, size within 0.001; chains form one set; a set with a selected member is not marked.
	protected void UpdateDuplicates(DZS_Instance inst, DZS_SelectionApi sel)
	{
		string selSig = "";
		if (sel)
			selSig = sel.Signature();
		string cli;
		if (GetCLIParam("dzsNoDups", cli))
		{
			m_Dups = new map<string, int>();
			return;
		}
		int ver = m_P.m_StateVer;
		if (ver != m_DupJobVer)
		{
			m_DupJobVer = ver;
			m_DupJob = new DZS_DupJob();
			m_DupJob.m_Inst = inst;
			m_DupJob.m_Groups = inst.GetChildren(DZS_PROJECT_ID);
			m_DupJob.m_T0 = TickCount(0);
		}
		if (m_DupJob)
		{
			// the project's duplicates in slices of 30 ms per frame (100 ms, 15 frames/s)
			if (StepDuplicates(m_DupJob, 30))
			{
				m_DupSets = m_DupJob.m_Found;
				int dms = (TickCount(0) - m_DupJob.m_T0) / 10000;
				if (dms >= 30)
					DZS_Log.Info("project bar: duplicates of the project after " + dms + " ms in " + m_DupJob.m_Frames + " frame(s), " + m_DupSets.Count() + " set(s)");
				m_DupJob = null;
				m_DupSel = "-";
			}
			else if (!m_Dups)
				m_Dups = new map<string, int>();
		}
		if (selSig == m_DupSel && m_Dups)
			return;
		m_DupSel = selSig;
		m_Dups = new map<string, int>();
		m_DupGen++;
		if (!m_DupSets)
			return;
		// a set with a selected member is not marked (the selection is being worked on)
		foreach (array<string> members : m_DupSets)
		{
			bool anySel = false;
			foreach (string mid : members)
			{
				if (sel && sel.IsSelected(mid))
					anySel = true;
			}
			if (anySel)
				continue;
			int count = members.Count();
			foreach (string mid2 : members)
				m_Dups.Set(mid2, count);
		}
	}

	protected ref array<ref array<string>> m_DupSets;
	protected ref DZS_DupJob m_DupJob;
	protected int m_DupJobVer = -1;
	protected int m_DupGen;

	// objects of the same name closer than 1 mm lie in the same or a neighbouring cell of a 10 cm grid (shifted by
	// half a cell, so positions on round decimetres lie mid-cell); only objects that share cells are compared exactly
	protected bool StepDuplicates(DZS_DupJob j, int ms)
	{
		int t0 = TickCount(0);
		j.m_Frames++;
		DZS_Instance inst = j.m_Inst;
		while (j.m_Gi < j.m_Groups.Count())
		{
			if (!j.m_Ids)
			{
				string g = j.m_Groups[j.m_Gi];
				if (inst.GetKind(g) != DZS_KIND_GROUP)
				{
					j.m_Gi++;
					continue;
				}
				j.m_Ids = inst.GetChildren(g);
				j.m_Ii = 0;
			}
			while (j.m_Ii < j.m_Ids.Count())
			{
				CollectOne(j, j.m_Ids[j.m_Ii]);
				j.m_Ii++;
				if ((j.m_Ii & 255) == 0 && (TickCount(0) - t0) / 10000 >= ms)
					return false;
			}
			j.m_Ids = null;
			j.m_Gi++;
		}
		array<string> all = j.m_All;
		array<int> parent = j.m_Parent;
		for (int x = 0; x < all.Count(); x++)
		{
			if (parent[x] != x)
				continue;
			if (j.m_Next[x] < 0 && j.m_Cells.Get(j.m_Keys[x]) == x && !NearFace(j.m_Spots[x]))
				continue;
			array<string> near = NearKeys(j.m_Names[x], j.m_Spots[x]);
			foreach (string nk : near)
			{
				int head;
				if (j.m_Cells.Find(nk, head))
					JoinNear(inst, all, j.m_Spots, parent, x, head, j.m_Next);
			}
		}
		int total = all.Count();
		array<int> roots = new array<int>();
		array<int> sizes = new array<int>();
		for (int z = 0; z < total; z++)
			sizes.Insert(0);
		for (int q = 0; q < total; q++)
		{
			int rq = Root(parent, q);
			roots.Insert(rq);
			sizes[rq] = sizes[rq] + 1;
		}
		map<int, ref array<string>> sets = new map<int, ref array<string>>();
		for (int k = 0; k < total; k++)
		{
			int r = roots[k];
			if (sizes[r] < 2)
				continue;
			array<string> inSet = sets.Get(r);
			if (!inSet)
			{
				inSet = new array<string>();
				sets.Set(r, inSet);
			}
			inSet.Insert(all[k]);
		}
		for (int si = 0; si < sets.Count(); si++)
		{
			array<string> members = sets.GetElement(si);
			j.m_Found.Insert(members);
		}
		return true;
	}

	// Objects with the same text of name, pos, ypr and scale are one spot without an exact comparison; only the first
	// of them takes part in the pairs (10 000 copies at one place stay linear).
	protected void CollectOne(DZS_DupJob j, string id)
	{
		DZS_Instance inst = j.m_Inst;
		string name, pos, ypr, scale;
		inst.GetField(id, DZS_Obj.NAME, name);
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		vector p = DZS_Obj.QuickVec(pos);
		int index = j.m_All.Count();
		j.m_All.Insert(id);
		j.m_Names.Insert(name);
		j.m_Spots.Insert(p);
		string ek = name + "|" + pos + "|" + ypr + "|" + scale;
		int first;
		if (j.m_Exact.Find(ek, first))
		{
			j.m_Parent.Insert(first);
			j.m_Next.Insert(-1);
			j.m_Keys.Insert("");
			return;
		}
		j.m_Exact.Set(ek, index);
		j.m_Parent.Insert(index);
		string key = CellKey(name, p);
		j.m_Keys.Insert(key);
		int head;
		if (j.m_Cells.Find(key, head))
			j.m_Next.Insert(head);
		else
			j.m_Next.Insert(-1);
		j.m_Cells.Set(key, index);
	}

	// Exact comparison only for objects in neighbouring cells (rare): position 1 mm, size, turn 0.01°.
	protected void JoinNear(DZS_Instance inst, array<string> all, array<vector> spots, array<int> parent, int x, int head, array<int> next)
	{
		for (int y = head; y >= 0; y = next[y])
		{
			if (y > x)
			{
				vector d = spots[x] - spots[y];
				bool close = Math.AbsFloat(d[0]) <= 0.002 && Math.AbsFloat(d[1]) <= 0.002 && Math.AbsFloat(d[2]) <= 0.002;
				if (close)
				{
					DZS_PlaceObj p = DZS_PlaceObj.FromState(inst, all[x]);
					DZS_PlaceObj q = DZS_PlaceObj.FromState(inst, all[y]);
					bool same = SameSpot(p, q);
					if (same)
					{
						int rx = Root(parent, x);
						int ry = Root(parent, y);
						if (rx != ry)
							parent[ry] = rx;
					}
				}
			}
		}
	}

	protected static string CellKey(string name, vector p)
	{
		int cx = Math.Floor(p[0] * 10 + 0.5);
		int cy = Math.Floor(p[1] * 10 + 0.5);
		int cz = Math.Floor(p[2] * 10 + 0.5);
		string sx = cx.ToString();
		string sy = cy.ToString();
		string sz = cz.ToString();
		return name + "|" + sx + "," + sy + "," + sz;
	}

	// own cell plus the neighbours the object lies within 2 mm of (JoinNear's reach); mostly just the own cell
	protected static array<string> NearKeys(string name, vector p)
	{
		array<string> r = new array<string>();
		array<ref array<int>> offs = new array<ref array<int>>();
		int base[3];
		for (int ax = 0; ax < 3; ax++)
		{
			float f = p[ax] * 10 + 0.5;
			int cell = Math.Floor(f);
			base[ax] = cell;
			array<int> o = {0};
			float within = f - cell;
			if (within < 0.02)
				o.Insert(-1);
			if (within > 0.98)
				o.Insert(1);
			offs.Insert(o);
		}
		foreach (int a : offs[0])
		{
			foreach (int b : offs[1])
			{
				foreach (int c : offs[2])
				{
					int kx = base[0] + a;
					int ky = base[1] + b;
					int kz = base[2] + c;
					string tx = kx.ToString();
					string ty = ky.ToString();
					string tz = kz.ToString();
					string key = name + "|" + tx + "," + ty + "," + tz;
					r.Insert(key);
				}
			}
		}
		return r;
	}

	protected static bool NearFace(vector p)
	{
		for (int ax = 0; ax < 3; ax++)
		{
			float f = p[ax] * 10 + 0.5;
			float within = f - Math.Floor(f);
			if (within < 0.02 || within > 0.98)
				return true;
		}
		return false;
	}

	protected static bool SameSpot(DZS_PlaceObj p, DZS_PlaceObj q)
	{
		vector d = p.m_Pos - q.m_Pos;
		if (Math.AbsFloat(d[0]) > 0.001 || Math.AbsFloat(d[1]) > 0.001 || Math.AbsFloat(d[2]) > 0.001)
			return false;
		if (Math.AbsFloat(p.m_Scale - q.m_Scale) > 0.001)
			return false;
		return DZS_PlacementPlugin.TurnBetween(p, q) <= 0.01;
	}

	protected int Root(array<int> parent, int i)
	{
		while (parent[i] != i)
			i = parent[i];
		return i;
	}

	protected int CountDups(array<string> ids)
	{
		int n = 0;
		foreach (string id : ids)
		{
			if (m_Dups && m_Dups.Contains(id))
				n++;
		}
		return n;
	}

	protected string Shown(string cls)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat)
			return cat.ShownName(cls);
		return cls;
	}

	void OnFold(DZS_GroupRow r)
	{
		string k = r.m_Group;
		if (r.m_Type == "ext")
			k = "ext:" + r.m_Group;
		else if (r.m_Type != "group")
			return;
		if (m_Closed.Contains(k))
			m_Closed.Remove(k);
		else
			m_Closed.Set(k, true);
		DZS_Log.Info("project bar: fold " + k + " open " + !m_Closed.Contains(k));
	}

	// Clicks on rows.
	void OnRowClick(DZS_GroupRow r, int button)
	{
		DZS_Instance inst = m_P.Kernel().GetShown();
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!inst || !sel)
			return;
		if (r.m_Type == "ext")
			return;
		if (r.m_Type == "extrow")
		{
			DZS_BarGroup xg = ExtGroup(r.m_Group);
			if (xg)
				xg.OnRowClick(inst, r.m_Class, button);
			return;
		}
		if (r.m_Type == "group")
		{
			if (button == MouseState.RIGHT)
			{
				OpenGroupMenu(inst, r.m_Group);
				return;
			}
			DZS_ActiveGroup.Set(inst, r.m_Group);
			int gnow = GetGame().GetTime();
			string gkey = "group|" + r.m_Group;
			if (button == MouseState.LEFT && gkey == m_DblKey && gnow - m_DblAt <= 450)
			{
				m_DblKey = "";
				OnFold(r);
				return;
			}
			m_DblKey = gkey;
			m_DblAt = gnow;
			return;
		}
		if (inst.IsHidden(r.m_Group) || inst.IsLocked(r.m_Group))
			return;
		bool shift = DZS_UiRegistry.ModDown("Shift");
		bool ctrl = DZS_UiRegistry.ModDown("Ctrl");
		array<string> ids = new array<string>();
		ids.Insert(r.m_Obj);
		if (r.m_Type == "object" && shift && m_LastClickGroup == r.m_Group && m_LastClickObj != "")
		{
			array<string> order = inst.GetChildren(r.m_Group);
			int a = order.Find(m_LastClickObj);
			int b = order.Find(r.m_Obj);
			if (a >= 0 && b >= 0)
			{
				ids.Clear();
				for (int i = Math.Min(a, b); i <= Math.Max(a, b); i++)
					ids.Insert(order[i]);
				sel.Add(ids);
				return;
			}
		}
		if (ctrl)
		{
			bool allIn = true;
			foreach (string id : ids)
			{
				if (!sel.IsSelected(id))
					allIn = false;
			}
			if (allIn)
				sel.Remove(ids);
			else
				sel.Add(ids);
		}
		else if (shift)
			sel.Add(ids);
		else
			sel.SetSelection(ids, ids[ids.Count() - 1]);
		if (r.m_Type == "object")
		{
			m_LastClickGroup = r.m_Group;
			m_LastClickObj = r.m_Obj;
		}
		if (button == MouseState.RIGHT)
			m_P.Kernel().GetEvents().FirePluginEvent("selection.contextRequest", null);
		// A double click on an object row selects and focuses it.
		int now = GetGame().GetTime();
		string key = r.m_Type + "|" + r.m_Group + "|" + r.m_Obj + "|" + r.m_Class;
		if (button == MouseState.LEFT && !shift && !ctrl && key == m_DblKey && now - m_DblAt <= 450)
		{
			m_DblKey = "";
			DZS_Log.Info("project bar: double click on " + key + ", focus");
			m_P.Kernel().GetEvents().FirePluginEvent("camera.focusRequest", null);
			return;
		}
		m_DblKey = key;
		m_DblAt = now;
	}

	protected string m_DblKey;
	protected int m_DblAt;

	// Name of another participant holding objects of this group ("" = none).
	// Folded group: " · 2 with Ben" per holder, in the order first met.
	protected string HeldBy(DZS_Instance inst, string gid)
	{
		DZS_SelectGuard g = DZS_SelectGuard.Get();
		if (!g || !inst)
			return "";
		array<string> ids = new array<string>();
		array<int> colors = new array<int>();
		array<bool> lapsed = new array<bool>();
		g.Held(ids, colors, lapsed);
		array<string> names = new array<string>();
		map<string, int> counts = new map<string, int>();
		for (int i = 0; i < ids.Count(); i++)
		{
			string oid = ids[i];
			if (!inst.Has(oid) || inst.GetParent(oid) != gid)
				continue;
			string who = g.HolderName(oid);
			if (who == "")
				continue;
			if (!counts.Contains(who))
			{
				counts.Set(who, 0);
				names.Insert(who);
			}
			counts.Set(who, counts.Get(who) + 1);
		}
		string r = "";
		foreach (string nm : names)
			r += " · " + DZS_Num.Count(counts.Get(nm)) + DZS_Texts.Pick(" bei ", " at ") + nm;
		return r;
	}

	string GroupHolder(DZS_Instance inst, string gid)
	{
		DZS_SelectGuard g = DZS_SelectGuard.Get();
		if (!g || !inst)
			return "";
		array<string> ids = new array<string>();
		array<int> colors = new array<int>();
		array<bool> lapsed = new array<bool>();
		g.Held(ids, colors, lapsed);
		for (int i = 0; i < ids.Count(); i++)
		{
			if (lapsed[i])
				continue;
			string oid = ids[i];
			if (inst.Has(oid) && inst.GetParent(oid) == gid)
				return g.HolderName(oid);
		}
		return "";
	}

	// Context menu of an object group.
	protected string m_MenuGroup;

	protected void OpenGroupMenu(DZS_Instance inst, string gid)
	{
		m_MenuGroup = gid;
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		DZS_MenuItem del = new DZS_MenuItem();
		del.m_Tag = "deleteGroup";
		del.m_Text = DZS_Texts.Pick("Gruppe löschen …", "Delete group …");
		string why = m_P.GroupsFixedReason();
		string holder = GroupHolder(inst, gid);
		if (why == "" && holder != "")
			why = DZS_Texts.Pick(holder + " bearbeitet Objekte dieser Gruppe", holder + " is editing objects of this group");
		if (why != "")
		{
			del.m_Enabled = false;
			del.m_Reason = why;
		}
		items.Insert(del);
		DZS_Popup pop = m_P.Kernel().GetScreen().OpenContextMenu(items);
		pop.m_OnChoose.Insert(OnGroupMenu);
		DZS_Log.Info("projects: group menu " + gid + ", delete " + (why == ""));
	}

	void OnGroupMenu(DZS_Popup p, DZS_MenuItem it)
	{
		m_P.Kernel().GetScreen().CloseMenu();
		if (!it.m_Enabled || it.m_Tag != "deleteGroup")
			return;
		m_P.AskDeleteGroup(m_MenuGroup);
	}

	void OnRowKnob(DZS_GroupRow r, string what)
	{
		DZS_Instance inst = m_P.Kernel().GetShown();
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!inst)
			return;
		if (r.m_Type == "ext")
		{
			DZS_BarGroup kg = ExtGroup(r.m_Group);
			if (!kg)
				return;
			if (what == "hide")
				kg.SetHidden(inst, !kg.IsHidden(inst));
			else if (what == "lock")
				kg.SetLocked(inst, !kg.IsLocked(inst));
			return;
		}
		if (what == "hide" || what == "lock")
			return;
		if (what == "select" && sel)
		{
			array<string> all = new array<string>();
			array<string> ids = inst.GetChildren(r.m_Group);
			foreach (string id : ids)
			{
				if (!inst.IsHidden(id) && !inst.IsLocked(id))
					all.Insert(id);
			}
			if (DZS_UiRegistry.ModDown("Shift"))
				sel.Add(all);
			else if (all.Count() > 0)
				sel.SetSelection(all, all[0]);
			else
				sel.SetSelection(all, "");
			return;
		}
		array<string> targets = new array<string>();
		if (r.m_Type == "group")
			targets.Insert(r.m_Group);
		else
			targets.Insert(r.m_Obj);
		bool hide = what == "hide";
		bool target = false;
		foreach (string t : targets)
		{
			if (hide && !inst.IsHidden(t))
				target = true;
			if (!hide && !inst.IsLocked(t))
				target = true;
		}
		DZS_OpBuilder b = inst.BeginOperation("projects:edit", 1);
		foreach (string t2 : targets)
		{
			if (hide)
				b.SetHidden(t2, target);
			else
				b.SetLocked(t2, target);
		}
		string de;
		string en;
		if (hide && target)
		{
			de = "Ausblenden";
			en = "Hide";
		}
		else if (hide)
		{
			de = "Einblenden";
			en = "Show";
		}
		else if (target)
		{
			de = "Sperren";
			en = "Lock";
		}
		else
		{
			de = "Entsperren";
			en = "Unlock";
		}
		DZS_SubmitResult res = b.Commit(de, en);
		DZS_Log.Info("projects: " + en + " " + r.m_Type + " (" + targets.Count() + ") ok " + res.Ok());
	}
}

// The object rows of one group in the project bar: order, kind, count within the kind, height and top.
class DZS_BarLayout : Managed
{
	int m_Ver = -1;
	DZS_Instance m_Inst;
	string m_HeightSig;
	ref array<string> m_Ids = new array<string>();
	ref array<string> m_Cls = new array<string>();
	ref array<int> m_Nth = new array<int>();
	ref array<string> m_Kinds = new array<string>();
	ref array<float> m_H = new array<float>();
	ref array<float> m_Top = new array<float>();
	float m_Total;

	// first row whose bottom reaches 'rel' (binary search over the tops)
	int FirstBelow(float rel)
	{
		int lo = 0;
		int hi = m_Top.Count();
		while (lo < hi)
		{
			int mid = (lo + hi) / 2;
			if (m_Top[mid] + m_H[mid] < rel)
				lo = mid + 1;
			else
				hi = mid;
		}
		return lo;
	}
}

// Work of one search for duplicates, spread over frames (DZS_ProjectBar.StepDuplicates).
class DZS_DupJob : Managed
{
	DZS_Instance m_Inst;
	ref array<string> m_Groups;
	int m_Gi;
	ref array<string> m_Ids;
	int m_Ii;
	int m_T0;
	int m_Frames;
	ref array<string> m_All = new array<string>();
	ref array<string> m_Names = new array<string>();
	ref array<vector> m_Spots = new array<vector>();
	ref map<string, int> m_Cells = new map<string, int>();
	ref array<int> m_Next = new array<int>();
	ref array<string> m_Keys = new array<string>();
	ref map<string, int> m_Exact = new map<string, int>();
	ref array<int> m_Parent = new array<int>();
	ref array<ref array<string>> m_Found = new array<ref array<string>>();
}

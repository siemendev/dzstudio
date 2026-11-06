// Own brush sets: name dialog, edit dialog, entries in the catalog's context menu.
class DZS_BrushNameDialog : DZS_Dialog
{
	protected DZS_BrushPlugin m_P;
	protected ref DZS_TextField m_Field;
	protected TextWidget m_Label;
	protected TextWidget m_Note;
	protected TextWidget m_Error;
	protected string m_Initial;
	protected string m_Mode;
	protected string m_NoteText;
	protected DZS_BrushSet m_Own;
	ref array<ref DZS_BrushEntry> m_Entries;

	// mode: "new" (m_Entries), "copy" (copy of the current set), "rename" (own = the set)
	void DZS_BrushNameDialog(DZS_BrushPlugin p, string mode, string initial, string note, DZS_BrushSet own)
	{
		m_P = p;
		m_Mode = mode;
		m_Initial = initial;
		m_NoteText = note;
		m_Own = own;
		string title = DZS_Texts.Get("brush.own.newTitle");
		string main = DZS_Texts.Get("brush.own.create");
		if (mode == "rename")
		{
			title = DZS_Texts.Get("brush.own.renameTitle");
			main = DZS_Texts.Get("brush.own.renameBtn");
		}
		Setup(SMALL, DZS_Texts.Get("brush.own.kicker"), title, true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("main", main, "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Note = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("warning"));
		m_Note.SetText(m_NoteText);
		m_Label = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Get("brush.own.name")));
		m_Field = new DZS_TextField("brush.ownName", m_Initial, "textMuted", 372);
		m_Field.Build(inner);
		m_Field.m_OnCommit.Insert(OnFieldCommit);
		m_Error = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("danger"));
		AddFocusField(m_Field);
		AddFootFocus();
	}

	void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		r.m_Ok = true;
	}

	string Value()
	{
		string t = m_Field.GetText();
		if (t == "")
			t = m_Initial;
		t.TrimInPlace();
		return t;
	}

	protected string Blocked()
	{
		if (m_Mode == "new" && (!m_Entries || m_Entries.Count() == 0))
			return DZS_Texts.Get("brush.own.nothingLeft");
		return "";
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		m_Note.Show(m_NoteText != "");
		if (m_NoteText != "")
		{
			DZS_W.Place(m_Note, 0, 0, width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(26);
		}
		DZS_W.Place(m_Label, 0, y, width, DZS_Scale.Px(14));
		m_Field.Place(0, y + DZS_Scale.Px(18), DZS_Scale.Px(28));
		DZS_W.Place(m_Error, 0, y + DZS_Scale.Px(52), width, DZS_Scale.Px(18));
		return y + DZS_Scale.Px(72);
	}

	override void UpdateContent(float dt)
	{
		m_Field.Refresh();
		string why = m_P.NameProblem(Value(), m_Own);
		m_Error.SetText(why);
		m_Field.SetError(why != "");
		DZS_Button main = FootButton("main");
		string blocked = Blocked();
		bool ok = why == "" && blocked == "";
		string reason = why;
		if (blocked != "")
			reason = blocked;
		if (main && main.m_Hit.m_Enabled != ok)
			main.SetEnabled(ok, reason);
	}

	override void OnFoot(string tag)
	{
		if (tag != "main")
		{
			Close();
			return;
		}
		string name = Value();
		if (m_P.NameProblem(name, m_Own) != "" || Blocked() != "")
			return;
		Close();
		if (m_Mode == "rename")
			m_P.RenameOwn(m_Own, name);
		else if (m_Mode == "copy")
			m_P.CopyCurrentAs(name);
		else
			m_P.CreateOwn(name, m_Entries);
	}
}

// One entry of a set in the edit dialog.
class DZS_BrushEntryRow : Managed
{
	DZS_BrushEditDialog m_D;
	DZS_BrushEntry m_E;
	ImageWidget m_Icon;
	TextWidget m_Name;
	ImageWidget m_Warn;
	ref DZS_Hit m_WarnHit;
	ref DZS_Button m_Remove;
	TextWidget m_ShareLabel;
	ref DZS_NumberField m_Share;
	TextWidget m_Percent;
	TextWidget m_YawLabel;
	ref DZS_Toggle m_Yaw;
	TextWidget m_TiltLabel;
	ref DZS_NumberField m_Tilt;
	TextWidget m_SlopeLabel;
	ref DZS_Toggle m_Slope;
	ref DZS_NumberField m_SlopeMax;
	TextWidget m_OffsetLabel;
	ref DZS_NumberField m_Offset;
	TextWidget m_ScaleLabel;
	ref DZS_NumberField m_ScaleMin;
	TextWidget m_ScaleDash;
	ref DZS_NumberField m_ScaleMax;
	Widget m_Line;

	void DZS_BrushEntryRow(DZS_BrushEditDialog d, Widget parent, DZS_BrushEntry e)
	{
		m_D = d;
		m_E = e;
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		string icon = "package";
		string shown = e.m_Name;
		if (cat)
		{
			icon = cat.IconOf(e.m_Name);
			shown = cat.ShownName(e.m_Name);
		}
		m_Icon = DZS_W.Icon(parent, icon, DZS_Color.Get("textMuted"));
		m_Name = DZS_W.Text(parent, DZS_Font.READBOLD, 13, DZS_Color.Get("text"));
		m_Name.SetText(DZS_W.Literal(shown));
		m_WarnHit = new DZS_Hit(parent, null, "brush.entryWarn");
		m_WarnHit.m_FocusKind = "";
		m_WarnHit.SetTip(DZS_Texts.Get("brush.own.notInCatalog"));
		m_Warn = DZS_W.Icon(m_WarnHit.m_W, "triangle-alert", DZS_Color.Get("warning"));
		m_Remove = new DZS_Button(parent, "ghost", DZS_Texts.Get("brush.own.remove"), "", 24);
		m_Remove.m_OnClick.Insert(OnRemove);
		m_ShareLabel = Label(parent, "brush.own.share");
		m_Share = Field(parent, "brush.share", 52);
		m_Share.m_OnCommit.Insert(OnShare);
		m_Percent = DZS_W.Text(parent, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
		m_YawLabel = Label(parent, "brush.own.randomYaw");
		m_Yaw = new DZS_Toggle(parent);
		m_Yaw.m_OnChange.Insert(OnYaw);
		m_TiltLabel = Label(parent, "brush.own.tilt");
		m_Tilt = Field(parent, "brush.tilt", 60);
		m_Tilt.m_OnCommit.Insert(OnTilt);
		m_SlopeLabel = Label(parent, "brush.own.slope");
		m_Slope = new DZS_Toggle(parent);
		m_Slope.m_OnChange.Insert(OnSlope);
		m_SlopeMax = Field(parent, "brush.slopeMax", 56);
		m_SlopeMax.m_OnCommit.Insert(OnSlopeMax);
		m_OffsetLabel = Label(parent, "brush.own.offset");
		m_Offset = Field(parent, "brush.offset", 68);
		m_Offset.m_OnCommit.Insert(OnOffset);
		m_ScaleLabel = Label(parent, "brush.own.scale");
		m_ScaleMin = Field(parent, "brush.scaleMin", 60);
		m_ScaleMin.m_OnCommit.Insert(OnScaleMin);
		m_ScaleDash = DZS_W.Text(parent, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_ScaleDash.SetText("–");
		m_ScaleMax = Field(parent, "brush.scaleMax", 60);
		m_ScaleMax.m_OnCommit.Insert(OnScaleMax);
		m_Line = DZS_W.Panel(parent, DZS_Color.Get("line"));
	}

	protected TextWidget Label(Widget parent, string key)
	{
		TextWidget t = DZS_W.Text(parent, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		t.SetText(DZS_Texts.Get(key));
		return t;
	}

	protected DZS_NumberField Field(Widget parent, string id, int w)
	{
		DZS_NumberField f = new DZS_NumberField(id, "", "textMuted", w);
		f.Build(parent);
		return f;
	}

	void AddFocus(DZS_Dialog d)
	{
		d.AddFocusField(m_Share);
		d.AddFocusHit(m_Yaw.m_Hit);
		d.AddFocusField(m_Tilt);
		d.AddFocusHit(m_Slope.m_Hit);
		d.AddFocusField(m_SlopeMax);
		d.AddFocusField(m_Offset);
		d.AddFocusField(m_ScaleMin);
		d.AddFocusField(m_ScaleMax);
		d.AddFocusHit(m_Remove.m_Hit);
	}

	static float Height() { return DZS_Scale.Px(108); }

	void Place(float y, float width, bool locked, bool lastOne, int shareSum)
	{
		float rowH = DZS_Scale.Px(28);
		float icon = DZS_Scale.Px(20);
		DZS_W.Place(m_Icon, 0, y + (rowH - icon) / 2, icon, icon);
		float rw = m_Remove.GetWidth();
		m_Remove.Place(width - rw, y + DZS_Scale.Px(2));
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		bool missing = cat && !cat.InCatalog(m_E.m_Name);
		float nx = icon + DZS_Scale.Px(8);
		float wi = DZS_Scale.Px(14);
		m_WarnHit.m_W.Show(missing);
		if (missing)
		{
			m_WarnHit.Place(nx, y + (rowH - wi) / 2, wi, wi);
			DZS_W.Place(m_Warn, 0, 0, wi, wi);
			nx += wi + DZS_Scale.Px(6);
		}
		DZS_W.Place(m_Name, nx, y, width - nx - rw - DZS_Scale.Px(8), rowH);
		float y2 = y + DZS_Scale.Px(34);
		float x = 0;
		x = LabelAt(m_ShareLabel, x, y2, DZS_Scale.Px(46));
		m_Share.ShowValue(m_E.m_Share.ToString());
		m_Share.Place(x, y2, rowH);
		x += m_Share.GetWidth() + DZS_Scale.Px(4);
		int pct = 0;
		if (shareSum > 0)
			pct = Math.Round(100.0 * m_E.m_Share / shareSum);
		m_Percent.SetText(pct.ToString() + " %");
		DZS_W.Place(m_Percent, x, y2, DZS_Scale.Px(44), rowH);
		x += DZS_Scale.Px(50);
		x = LabelAt(m_YawLabel, x, y2, DZS_Scale.Px(104));
		if (m_Yaw.IsOn() != m_E.m_RandomYaw)
			m_Yaw.SetOn(m_E.m_RandomYaw);
		m_Yaw.Place(x, y2 + (rowH - DZS_Toggle.Height()) / 2);
		x += DZS_Toggle.Width() + DZS_Scale.Px(16);
		x = LabelAt(m_TiltLabel, x, y2, DZS_Scale.Px(80));
		m_Tilt.ShowValue(DZS_Num.Fixed(m_E.m_Tilt, 1) + "°");
		m_Tilt.Place(x, y2, rowH);
		float y3 = y2 + DZS_Scale.Px(34);
		x = 0;
		x = LabelAt(m_SlopeLabel, x, y3, DZS_Scale.Px(128));
		if (m_Slope.IsOn() != m_E.m_Slope)
			m_Slope.SetOn(m_E.m_Slope);
		m_Slope.Place(x, y3 + (rowH - DZS_Toggle.Height()) / 2);
		x += DZS_Toggle.Width() + DZS_Scale.Px(6);
		m_SlopeMax.ShowValue(DZS_Num.Fixed(m_E.m_SlopeMax, 0) + "°");
		m_SlopeMax.Place(x, y3, rowH);
		x += m_SlopeMax.GetWidth() + DZS_Scale.Px(14);
		x = LabelAt(m_OffsetLabel, x, y3, DZS_Scale.Px(84));
		m_Offset.ShowValue(DZS_Num.Fixed(m_E.m_Offset, 2) + " m");
		m_Offset.Place(x, y3, rowH);
		x += m_Offset.GetWidth() + DZS_Scale.Px(14);
		x = LabelAt(m_ScaleLabel, x, y3, DZS_Scale.Px(42));
		m_ScaleMin.ShowValue(DZS_Num.Fixed(m_E.m_ScaleMin, 2));
		m_ScaleMin.Place(x, y3, rowH);
		x += m_ScaleMin.GetWidth() + DZS_Scale.Px(2);
		DZS_W.Place(m_ScaleDash, x, y3, DZS_Scale.Px(10), rowH);
		x += DZS_Scale.Px(12);
		m_ScaleMax.ShowValue(DZS_Num.Fixed(m_E.m_ScaleMax, 2));
		m_ScaleMax.Place(x, y3, rowH);
		DZS_W.Place(m_Line, 0, y + Height() - DZS_Scale.Px(6), width, 1);
		bool on = !locked;
		m_Share.SetEnabled(on);
		m_Tilt.SetEnabled(on);
		m_SlopeMax.SetEnabled(on);
		m_Offset.SetEnabled(on);
		m_ScaleMin.SetEnabled(on);
		m_ScaleMax.SetEnabled(on);
		m_Yaw.m_Hit.SetEnabled(on);
		m_Slope.m_Hit.SetEnabled(on);
		string why = "";
		if (locked)
			why = DZS_Texts.Get("brush.own.bundledLocked");
		else if (lastOne)
			why = DZS_Texts.Get("brush.own.lastOne");
		m_Remove.SetEnabled(why == "", why);
	}

	protected float LabelAt(TextWidget t, float x, float y, float w)
	{
		DZS_W.Place(t, x, y, w, DZS_Scale.Px(28));
		return x + w;
	}

	void Update(float dt)
	{
		m_Share.Update(dt);
		m_Tilt.Update(dt);
		m_SlopeMax.Update(dt);
		m_Offset.Update(dt);
		m_ScaleMin.Update(dt);
		m_ScaleMax.Update(dt);
	}

	protected bool Num(string typed, float lo, float hi, out float v)
	{
		string t = typed;
		t.Replace("°", "");
		t.Replace("m", "");
		t.Replace("%", "");
		t.TrimInPlace();
		if (!DZS_Num.Parse(t, v))
			return false;
		v = Math.Clamp(v, lo, hi);
		return true;
	}

	void OnShare(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = Num(typed, 0, 100, v);
		if (r.m_Ok)
		{
			m_E.m_Share = Math.Round(v);
			m_D.Changed("share");
		}
	}

	void OnTilt(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = Num(typed, 0, 15, v);
		if (r.m_Ok)
		{
			m_E.m_Tilt = Math.Round(v * 2) / 2;
			m_D.Changed("tilt");
		}
	}

	void OnSlopeMax(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = Num(typed, 0, 45, v);
		if (r.m_Ok)
		{
			m_E.m_SlopeMax = Math.Round(v);
			m_D.Changed("slopeMax");
		}
	}

	void OnOffset(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = Num(typed, -10, 10, v);
		if (r.m_Ok)
		{
			m_E.m_Offset = Math.Round(v * 100) / 100;
			m_D.Changed("heightOffset");
		}
	}

	void OnScaleMin(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = Num(typed, DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX, v);
		if (r.m_Ok)
		{
			m_E.m_ScaleMin = Math.Round(v * 100) / 100;
			SwapScale();
			m_D.Changed("scaleMin");
		}
	}

	void OnScaleMax(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = Num(typed, DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX, v);
		if (r.m_Ok)
		{
			m_E.m_ScaleMax = Math.Round(v * 100) / 100;
			SwapScale();
			m_D.Changed("scaleMax");
		}
	}

	protected void SwapScale()
	{
		if (m_E.m_ScaleMin <= m_E.m_ScaleMax)
			return;
		float t = m_E.m_ScaleMin;
		m_E.m_ScaleMin = m_E.m_ScaleMax;
		m_E.m_ScaleMax = t;
	}

	void OnYaw(DZS_Toggle t, bool on)
	{
		if (m_D.IsLocked())
		{
			t.SetOn(m_E.m_RandomYaw);
			return;
		}
		m_E.m_RandomYaw = on;
		m_D.Changed("randomYaw");
	}

	void OnSlope(DZS_Toggle t, bool on)
	{
		if (m_D.IsLocked())
		{
			t.SetOn(m_E.m_Slope);
			return;
		}
		m_E.m_Slope = on;
		m_D.Changed("slope");
	}

	void OnRemove(DZS_Button b)
	{
		m_D.RemoveEntry(m_E);
	}
}

// "Edit set …": tool dialog, medium, only "Close"; every change is applied and stored at once.
class DZS_BrushEditDialog : DZS_Dialog
{
	protected DZS_BrushPlugin m_P;
	protected DZS_BrushSet m_S;
	protected TextWidget m_Locked;
	protected ref DZS_Button m_CopyBtn;
	protected TextWidget m_Label;
	protected ref DZS_TextField m_NameField;
	protected TextWidget m_NameError;
	protected TextWidget m_Footer;
	protected ref array<ref DZS_BrushEntryRow> m_Rows = new array<ref DZS_BrushEntryRow>();
	protected Widget m_RowsRoot;
	protected bool m_Rebuild;

	void DZS_BrushEditDialog(DZS_BrushPlugin p, DZS_BrushSet s)
	{
		m_P = p;
		m_S = s;
		Setup(MEDIUM, DZS_Texts.Get("brush.own.kicker"), DZS_Texts.Get("brush.own.editTitle"), false);
		AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "main");
	}

	bool IsLocked() { return !m_S || m_S.m_Bundled; }

	override void BuildContent(Widget inner)
	{
		m_Locked = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("warning"));
		m_Locked.SetText(DZS_Texts.Get("brush.own.bundledLocked"));
		m_CopyBtn = new DZS_Button(inner, "side", DZS_Texts.Get("brush.own.copy"), "copy", 28);
		m_CopyBtn.m_OnClick.Insert(OnCopy);
		m_Label = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Get("brush.own.name")));
		m_NameField = new DZS_TextField("brush.editName", m_S.Name(), "textMuted", 372);
		m_NameField.Build(inner);
		m_NameField.m_OnCommit.Insert(OnNameCommit);
		m_NameError = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("danger"));
		m_Footer = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Footer.SetText(DZS_Texts.Get("brush.own.footer"));
		m_RowsRoot = DZS_W.Frame(inner);
		BuildRows();
	}

	protected void BuildRows()
	{
		m_Rows.Clear();
		if (m_RowsRoot)
		{
			m_RowsRoot.Unlink();
			m_RowsRoot = DZS_W.Frame(m_Inner);
		}
		ClearFocusables();
		if (!IsLocked())
			AddFocusField(m_NameField);
		else
			AddFocusHit(m_CopyBtn.m_Hit);
		foreach (DZS_BrushEntry e : m_S.m_Entries)
		{
			DZS_BrushEntryRow row = new DZS_BrushEntryRow(this, m_RowsRoot, e);
			m_Rows.Insert(row);
			if (!IsLocked())
				row.AddFocus(this);
		}
		AddFootFocus();
	}

	void OnNameCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		if (IsLocked())
		{
			r.m_Ok = false;
			return;
		}
		string t = typed;
		t.TrimInPlace();
		string why = m_P.NameProblem(t, m_S);
		r.m_Ok = why == "";
		if (r.m_Ok)
			m_P.RenameOwn(m_S, t);
	}

	void OnCopy(DZS_Button b)
	{
		Close();
		m_P.AskCopy();
	}

	void Changed(string what)
	{
		m_P.OwnChanged(m_S, what);
	}

	void RemoveEntry(DZS_BrushEntry e)
	{
		if (IsLocked() || m_S.m_Entries.Count() <= 1)
			return;
		int i = m_S.m_Entries.Find(e);
		if (i < 0)
			return;
		string name = e.m_Name;
		m_S.m_Entries.RemoveOrdered(i);
		m_S.ForgetNames();
		m_Rebuild = true;
		m_P.OwnChanged(m_S, "remove " + name);
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		bool locked = IsLocked();
		m_Locked.Show(locked);
		m_CopyBtn.GetWidget().Show(locked);
		if (locked)
		{
			DZS_W.Place(m_Locked, 0, y, width - m_CopyBtn.GetWidth() - DZS_Scale.Px(8), DZS_Scale.Px(28));
			m_CopyBtn.Place(width - m_CopyBtn.GetWidth(), y);
			y += DZS_Scale.Px(40);
		}
		DZS_W.Place(m_Label, 0, y, width, DZS_Scale.Px(14));
		m_NameField.Place(0, y + DZS_Scale.Px(18), DZS_Scale.Px(28));
		m_NameField.SetEnabled(!locked);
		DZS_W.Place(m_NameError, 0, y + DZS_Scale.Px(50), width, DZS_Scale.Px(18));
		y += DZS_Scale.Px(76);
		DZS_W.Place(m_RowsRoot, 0, y, width, m_Rows.Count() * DZS_BrushEntryRow.Height());
		int sum = m_S.ShareSum();
		bool lastOne = m_S.m_Entries.Count() <= 1;
		for (int i = 0; i < m_Rows.Count(); i++)
			m_Rows[i].Place(i * DZS_BrushEntryRow.Height(), width, locked, lastOne, sum);
		y += m_Rows.Count() * DZS_BrushEntryRow.Height();
		DZS_W.Place(m_Footer, 0, y, width, DZS_Scale.Px(20));
		y += DZS_Scale.Px(28);
		return y;
	}

	override void UpdateContent(float dt)
	{
		if (m_Rebuild)
		{
			m_Rebuild = false;
			BuildRows();
		}
		m_NameField.Refresh();
		if (!IsLocked())
		{
			string typed = m_NameField.GetText();
			typed.TrimInPlace();
			string why = "";
			if (m_NameField.IsFocused() && typed != "")
				why = m_P.NameProblem(typed, m_S);
			m_NameError.SetText(why);
			m_NameField.SetError(why != "");
		}
		foreach (DZS_BrushEntryRow r : m_Rows)
			r.Update(dt);
	}

	override void OnFoot(string tag)
	{
		Close();
	}

	override void Cancel()
	{
		Close();
	}
}

// Entries of the brush in the catalog's context menu.
class DZS_BrushCatalogMenu : DZS_CatalogMenuSource
{
	DZS_BrushPlugin m_P;

	override void AddItems(string entry, array<ref DZS_MenuItem> items)
	{
		DZS_MenuItem a = new DZS_MenuItem();
		a.m_ActionId = "brush.own.newWith";
		a.m_Text = DZS_Texts.Get("brush.own.newWith");
		a.m_Icon = "paintbrush";
		a.m_SepBefore = true;
		items.Insert(a);
		DZS_BrushSet s = m_P.CurrentSet();
		if (!s || s.m_Bundled)
			return;
		DZS_MenuItem b = new DZS_MenuItem();
		b.m_ActionId = "brush.own.addTo";
		b.m_Text = DZS_Texts.Fmt("brush.own.addTo", s.Name());
		if (s.HasName(entry))
		{
			b.m_Enabled = false;
			b.m_Reason = DZS_Texts.Get("brush.own.already");
		}
		else if (s.m_Entries.Count() >= DZS_BrushOwnStore.MAX_ENTRIES)
		{
			b.m_Enabled = false;
			b.m_Reason = DZS_Texts.Get("brush.own.full");
		}
		items.Insert(b);
	}

	override bool OnChoose(string actionId, string entry)
	{
		if (actionId == "brush.own.newWith")
		{
			m_P.AskNewWith(entry);
			return true;
		}
		if (actionId == "brush.own.addTo")
		{
			m_P.AddToCurrent(entry);
			return true;
		}
		return false;
	}
}

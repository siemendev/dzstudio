// "Details …": everything until "Apply" is the dialog's own value; "Apply" submits all
// changed values as one operation with the values seen at opening as precondition.
class DZS_DetailsDialog : DZS_Dialog
{
	protected DZS_PropertiesPlugin m_P;
	protected string m_Id;
	protected ref DZS_PropObj m_Seen;
	protected ref DZS_PropObj m_Work;
	protected bool m_Locked;
	protected bool m_Gone;
	protected bool m_Changed;
	protected bool m_ModelPath;
	protected bool m_Control;
	protected int m_Unknown;

	protected ImageWidget m_NoteIcon;
	protected ref DZS_WrapText m_Note;
	protected string m_NoteTone;
	protected ref array<TextWidget> m_FactLabels;
	protected ref array<TextWidget> m_FactValues;
	protected Widget m_ClassBg;
	protected TextWidget m_ClassText;
	protected ref DZS_Button m_Swap;
	protected ref array<TextWidget> m_RowLabels;
	protected ref array<ref DZS_NumberField> m_Fields;
	protected int m_DragField = -1;
	protected float m_DragStart;
	protected ref DZS_Toggle m_Persist;
	protected TextWidget m_PersistName;
	protected ref DZS_Hit m_PersistHit;
	protected ref DZS_WrapText m_PersistText;
	protected ref DZS_PropVariantRow m_Variant;
	protected TextWidget m_CustomLabel;
	protected ref DZS_WrapText m_CustomText;
	protected ref DZS_TextField m_Custom;
	protected TextWidget m_Counter;
	protected ref DZS_Hit m_CustomTip;

	void DZS_DetailsDialog(DZS_PropertiesPlugin p, string id)
	{
		m_P = p;
		m_Id = id;
		m_Seen = DZS_PropObj.Read(p.Inst(), id);
		m_Work = m_Seen.Copy();
		m_Locked = m_Seen.m_Locked;
		m_ModelPath = DZS_Obj.IsModelPath(m_Seen.m_Name);
		m_Control = DZS_PropText.HasControlChars(m_Seen.m_Custom);
		m_Unknown = CountUnknown(p.Inst(), id);
		string title = DZS_PropText.GameName(m_Seen.m_Name);
		if (DZS_PropText.Placeholder(m_Seen.m_Name) != "")
			title = m_Seen.m_Name;
		Setup(MEDIUM, DZS_Texts.Pick("Objekt", "Object"), title, true);
		AddFootButtonLeft("copyText", DZS_Texts.Pick("Als Text kopieren", "Copy as text"), "ghost");
		if (m_Locked)
			AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "main");
		else
		{
			AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
			AddFootButton("apply", DZS_Texts.Pick("Übernehmen", "Apply"), "main");
		}
	}

	// Unknown fields read from the file stay; their number shows here.
	static int CountUnknown(DZS_Instance inst, string id)
	{
		int version;
		string data;
		if (!inst.GetCompanion(id, "spawner:read", version, data) || data == "")
			return 0;
		array<string> chunks = {data};
		DZS_SrcDoc doc = DZS_SrcJson.Parse(chunks);
		if (!doc.m_Root || !doc.m_Root.IsObj())
			return 0;
		DZS_Json j = doc.m_Root.ToJson();
		DZS_Json u = j.Get("u");
		if (!u || !u.IsArr())
			return 0;
		return u.Count();
	}

	override void BuildContent(Widget inner)
	{
		m_NoteIcon = DZS_W.Icon(inner, "triangle-alert", DZS_Color.Get("warning"));
		m_Note = new DZS_WrapText(inner, DZS_Font.READ, 13, DZS_Color.Get("warning"));
		m_FactLabels = new array<TextWidget>();
		m_FactValues = new array<TextWidget>();
		array<string> facts = {DZS_Texts.Pick("Klassenname", "Class name"), DZS_Texts.Pick("Gruppe", "Group"), DZS_Texts.Pick("Maße", "Size"), DZS_Texts.Pick("Höhenversatz", "Height offset"), DZS_Texts.Pick("Weitere Felder aus der Datei", "More fields from the file")};
		foreach (string f : facts)
		{
			m_FactLabels.Insert(DZS_PropertiesApi.RowLabel(inner, f));
			m_FactValues.Insert(DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text")));
		}
		m_ClassBg = DZS_W.Panel(inner, DZS_Color.Get("well"));
		m_ClassText = DZS_W.Text(inner, DZS_Font.MONO, 12, DZS_Color.Get("text"));
		m_Swap = new DZS_Button(inner, "side", DZS_Texts.Pick("Austauschen", "Replace"), "replace", 28);
		m_Swap.m_OnClick.Insert(OnSwap);
		m_Swap.SetEnabled(!m_Locked, m_P.LockedTip());

		m_RowLabels = new array<TextWidget>();
		array<string> rows = {DZS_Texts.Pick("Position", "Position"), DZS_Texts.Pick("Drehung", "Rotation"), DZS_Texts.Pick("Größe", "Size")};
		foreach (string r : rows)
			m_RowLabels.Insert(DZS_PropertiesApi.RowLabel(inner, r));
		m_Fields = new array<ref DZS_NumberField>();
		array<string> letters = {"X", "Y", "Z", "G", "N", "R", "×"};
		array<string> colors = {"axisX", "axisY", "axisZ", "textMuted", "textMuted", "textMuted", "textMuted"};
		for (int i = 0; i < 7; i++)
		{
			DZS_NumberField nf = new DZS_NumberField("details.f" + i, letters[i], colors[i], 180);
			nf.Build(inner);
			nf.EnableDrag(DZS_PropertiesPlugin.FieldName(i));
			if (i >= 3 && i <= 5)
				nf.EnableReset(DZS_Texts.Pick("Auf 0°", "To 0°"));
			if (i == 6)
				nf.EnableReset(DZS_Texts.Pick("Auf 1", "To 1"));
			nf.m_OnCommit.Insert(OnFieldCommit);
			nf.m_OnWheel.Insert(OnFieldWheel);
			nf.m_OnDrag.Insert(OnFieldDrag);
			nf.m_OnReset.Insert(OnFieldReset);
			nf.SetEnabled(!m_Locked);
			if (m_Locked)
				nf.SetFieldTip(m_P.LockedTip());
			m_Fields.Insert(nf);
			if (!m_Locked)
				AddFocusField(nf);
		}

		m_Persist = new DZS_Toggle(inner);
		m_Persist.SetOn(m_Work.m_Persist);
		m_Persist.m_OnChange.Insert(OnPersistToggle);
		m_PersistName = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_PersistName.SetText(DZS_Texts.Pick("Vom Server speichern lassen", "Let the server save it"));
		m_PersistHit = new DZS_Hit(inner, this, "details.persistName");
		m_PersistHit.m_FocusKind = "";
		m_PersistText = new DZS_WrapText(inner, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		m_PersistText.SetText(DZS_Texts.Pick("An: Das Objekt bekommt eine Lebensdauer und wird mit dem Spielstand des Servers gespeichert. Aus: Es entsteht bei jedem Serverstart neu.", "On: the object gets a lifetime and is saved with the server’s game state. Off: it appears anew at every server start."));
		bool persistOff = m_Locked || m_ModelPath;
		m_Persist.m_Hit.SetEnabled(!persistOff);
		if (m_ModelPath)
		{
			m_Persist.m_Hit.SetTip("", "");
			m_Persist.m_Hit.m_TipReason = DZS_Texts.Get("properties.plants");
			m_PersistHit.SetTip(DZS_Texts.Get("properties.plants"));
		}
		if (!persistOff)
			AddFocusHit(m_Persist.m_Hit);

		m_Variant = new DZS_PropVariantRow(m_P, inner, true);
		m_Variant.SetObject(m_P.Inst(), m_Work);
		m_Variant.m_OnChange.Insert(OnVariantChange);
		if (!m_Locked && !m_Control && m_Variant.Kind() == DZS_Variants.TRIGGER)
			AddFocusField(m_Variant.TagField());
		else if (!m_Locked && !m_Control && m_Variant.Kind() == DZS_Variants.FLAG)
			AddFocusHit(m_Variant.FlagButton().m_Hit);

		m_CustomLabel = DZS_PropertiesApi.RowLabel(inner, DZS_Texts.Pick("Freier Text (customString)", "Free text (customString)"));
		m_CustomText = new DZS_WrapText(inner, DZS_Font.READ, 12, DZS_Color.Get("textMuted"));
		m_CustomText.SetText(DZS_Texts.Pick("Nur wenige Objekte werten diesen Text aus (Fahnenmast, Raketensilo); für alle anderen ohne Wirkung.", "Only a few objects read this text (flag pole, missile silo); it has no effect on all others."));
		m_Custom = new DZS_TextField("details.custom", "", "textMuted", 400);
		m_Custom.Build(inner);
		m_Custom.EnableClear();
		m_Custom.SetClearOnFocus(true);
		m_Custom.m_OnCommit.Insert(OnCustomCommit);
		m_Custom.m_OnClear.Insert(OnCustomClear);
		m_Custom.SetText(m_Work.m_Custom);
		m_CustomTip = new DZS_Hit(inner, null, "details.customTip");
		m_CustomTip.m_FocusKind = "";
		bool customOff = m_Locked || m_ModelPath || m_Control;
		m_Custom.SetEnabled(!customOff);
		if (m_ModelPath)
			m_CustomTip.SetTip(DZS_Texts.Get("properties.plants"));
		if (!customOff)
			AddFocusField(m_Custom);
		m_Counter = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
		AddFootFocus();
		Refresh();
	}

	protected void Refresh()
	{
		DZS_Instance inst = m_P.Inst();
		string cls = m_Work.m_Name;
		m_ClassText.SetText(DZS_W.Literal(cls));
		string gname = "";
		if (inst && inst.Has(m_Work.m_Group))
			inst.GetField(m_Work.m_Group, DZS_Obj.GROUP_NAME, gname);
		m_FactValues[1].SetText(DZS_W.Literal(gname));
		vector bmin, bmax;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		string size = "—";
		if (scene && scene.GetBox(cls, bmin, bmax))
		{
			float s = m_Work.m_Scale;
			size = DZS_PropText.Num((bmax[0] - bmin[0]) * s, 1) + " × " + DZS_PropText.Num((bmax[2] - bmin[2]) * s, 1) + " × " + DZS_PropText.Num((bmax[1] - bmin[1]) * s, 1) + " m";
		}
		m_FactValues[2].SetText(size);
		float h;
		string hs = "—";
		if (!m_Gone && m_P.Measure(m_Id, h))
			hs = DZS_PropText.Num(h, 3) + " m";
		m_FactValues[3].SetText(hs);
		m_FactValues[4].SetText(DZS_Texts.Pick(m_Unknown.ToString() + " · bleiben erhalten", m_Unknown.ToString() + " · are kept"));
		ShowFieldValues(true);
		UpdateNote();
	}

	protected void ShowFieldValues(bool force)
	{
		for (int i = 0; i < 7; i++)
		{
			string t = DZS_PropText.Num(m_Work.Value(i), DZS_PropertiesPlugin.Decimals(i));
			if (force && !m_Fields[i].IsFocused())
				m_Fields[i].ForceValue(t);
			else
				m_Fields[i].ShowValue(t);
		}
		for (int a = 3; a <= 5; a++)
			m_Fields[a].ShowReset(Math.AbsFloat(m_Work.Value(a)) >= 0.005);
		m_Fields[6].ShowReset(Math.AbsFloat(m_Work.m_Scale - 1) >= 0.0005);
	}

	protected void UpdateNote()
	{
		string note = "";
		string tone = "warning";
		string warn = DZS_PropText.Placeholder(m_Work.m_Name);
		if (m_Gone)
			note = DZS_Texts.Pick("Das Objekt gibt es nicht mehr", "The object no longer exists");
		else if (m_Changed)
			note = DZS_Texts.Pick("Inzwischen geändert — die Felder zeigen jetzt den aktuellen Stand", "Changed meanwhile — the fields now show the current state");
		else if (m_Locked)
		{
			note = m_P.LockedTip();
			tone = "info";
		}
		else if (warn != "")
			note = warn;
		m_Note.SetText(note);
		m_Note.SetColor(DZS_Color.Get(tone));
		m_NoteIcon.SetColor(DZS_Color.Get(tone));
		m_NoteTone = tone;
		int bytes = m_Work.m_Custom.Length();
		string counter = bytes.ToString() + " / 1023 Byte";
		string ccol = "textQuiet";
		if (bytes > 1023)
		{
			counter = DZS_Texts.Pick("Der Server liest nur 1023 Byte", "The server reads only 1023 bytes") + " (" + bytes + ")";
			ccol = "warning";
		}
		if (m_Control)
		{
			counter = DZS_Texts.Get("properties.control");
			ccol = "warning";
		}
		m_Counter.SetText(counter);
		m_Counter.SetColor(DZS_Color.Get(ccol));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		float lh = DZS_Scale.Px(15);
		if (m_Note.GetText() != "")
		{
			float wi = DZS_Scale.Px(16);
			DZS_W.Place(m_NoteIcon, 0, y + DZS_Scale.Px(2), wi, wi);
			m_NoteIcon.Show(true);
			m_Note.Show(true);
			y += m_Note.Place(wi + DZS_Scale.Px(8), y, width - wi - DZS_Scale.Px(8)) + DZS_Scale.Px(10);
		}
		else
		{
			m_NoteIcon.Show(false);
			m_Note.Show(false);
		}
		// facts: class name with "Swap", then group, size, height offset (and unknown fields) in two columns
		DZS_W.Place(m_FactLabels[0], 0, y, width, DZS_Scale.Px(14));
		y += lh;
		float cw = DZS_W.TextWidth(m_ClassText) + DZS_Scale.Px(12);
		DZS_W.Place(m_ClassBg, 0, y, cw, DZS_Scale.Px(28));
		DZS_W.Place(m_ClassText, DZS_Scale.Px(6), y, cw, DZS_Scale.Px(28));
		m_Swap.Place(cw + DZS_Scale.Px(10), y);
		y += DZS_Scale.Px(36);
		float col = (width - DZS_Scale.Px(16)) / 2;
		int shown = 0;
		for (int i = 1; i < 5; i++)
		{
			bool on = i < 4 || m_Unknown > 0;
			m_FactLabels[i].Show(on);
			m_FactValues[i].Show(on);
			if (!on)
				continue;
			int colIdx = shown - (shown / 2) * 2;
			int rowIdx = shown / 2;
			float x = colIdx * (col + DZS_Scale.Px(16));
			float fy = y + rowIdx * DZS_Scale.Px(38);
			DZS_W.Place(m_FactLabels[i], x, fy, col, DZS_Scale.Px(14));
			DZS_W.Place(m_FactValues[i], x, fy + lh, col, DZS_Scale.Px(20));
			shown++;
		}
		int factRows = (shown + 1) / 2;
		y += factRows * DZS_Scale.Px(38) + DZS_Scale.Px(6);
		// number fields
		float gap = DZS_Scale.Px(8);
		float fw = (width - 2 * gap) / 3;
		int wref = fw / DZS_Scale.Factor();
		for (int r = 0; r < 3; r++)
		{
			DZS_W.Place(m_RowLabels[r], 0, y, width, DZS_Scale.Px(14));
			y += lh;
			int count = 3;
			if (r == 2)
				count = 1;
			for (int c = 0; c < count; c++)
			{
				DZS_NumberField f = m_Fields[r * 3 + c];
				f.SetWidthRef(wref);
				f.Place(c * (fw + gap), y, DZS_Scale.Px(28));
			}
			y += DZS_Scale.Px(34);
		}
		// persistence
		y += DZS_Scale.Px(4);
		float nw = DZS_W.TextWidth(m_PersistName);
		DZS_W.Place(m_PersistName, 0, y, nw + 2, DZS_Scale.Px(20));
		m_PersistHit.Place(0, y, nw + 2, DZS_Scale.Px(20));
		m_Persist.Place(nw + DZS_Scale.Px(16), y + DZS_Scale.Px(3));
		y += DZS_Scale.Px(22);
		y += m_PersistText.Place(0, y, width) + DZS_Scale.Px(10);
		// variant
		y += m_Variant.Layout(0, y, width);
		// customString
		DZS_W.Place(m_CustomLabel, 0, y, width, DZS_Scale.Px(14));
		y += lh;
		y += m_CustomText.Place(0, y, width) + DZS_Scale.Px(4);
		int cref = width / DZS_Scale.Factor();
		m_Custom.SetWidthRef(cref);
		m_Custom.Place(0, y, DZS_Scale.Px(28));
		m_CustomTip.Place(0, y, width, DZS_Scale.Px(28));
		m_CustomTip.m_W.Show(m_ModelPath);
		y += DZS_Scale.Px(32);
		DZS_W.Place(m_Counter, 0, y, width, DZS_Scale.Px(18));
		y += DZS_Scale.Px(22);
		return y;
	}

	override void UpdateContent(float dt)
	{
		foreach (DZS_NumberField f : m_Fields)
			f.Update(dt);
		m_Variant.Update(dt);
		m_Custom.Refresh();
		ShowFieldValues(false);
		bool hasText = m_Work.m_Custom != "";
		m_Custom.ShowClear(hasText && !m_Locked && !m_ModelPath && !m_Control);
		DZS_Instance inst = m_P.Inst();
		if (!m_Gone && (!inst || !inst.Has(m_Id)))
		{
			m_Gone = true;
			GoneFoot();
			UpdateNote();
		}
		UpdateViewOnly(inst);
	}

	// Session switched to "View only": the dialog stays, fields grey, "Apply" off with the
	// reason; the intermediate values stay until "Cancel" or until the connection is back.
	protected bool m_ViewOnly;

	protected void UpdateViewOnly(DZS_Instance inst)
	{
		if (m_Gone || m_Locked || !inst)
			return;
		string de, en;
		bool off = DZS_EditGate.Get() && DZS_EditGate.Get().ViewOnly(inst, de, en);
		if (off == m_ViewOnly)
			return;
		m_ViewOnly = off;
		string why = DZS_Texts.Pick(de, en);
		foreach (DZS_NumberField f : m_Fields)
			f.SetEnabled(!off);
		m_Custom.SetEnabled(!off && !m_ModelPath && !m_Control);
		m_Persist.m_Hit.SetEnabled(!off && !m_ModelPath);
		m_Swap.SetEnabled(!off, why);
		DZS_Button apply = FootButton("apply");
		if (apply)
			apply.SetEnabled(!off, why);
		DZS_Log.Info("details: view only " + off + " (" + de + ")");
	}

	protected void GoneFoot()
	{
		DZS_Button apply = FootButton("apply");
		if (apply)
			apply.Show(false);
		DZS_Button cancel = FootButton("cancel");
		if (cancel)
		{
			cancel.SetLabel(DZS_Texts.Get("core.dialog.close"));
			cancel.m_Hit.m_Primary = true;
		}
		foreach (DZS_NumberField f : m_Fields)
			f.SetEnabled(false);
		m_Custom.SetEnabled(false);
		m_Persist.m_Hit.SetEnabled(false);
	}

	// ---- fields (intermediate values only) ----

	void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		int i = m_Fields.Find(f);
		float v;
		if (i < 0 || !DZS_Num.Parse(typed, v))
		{
			r.m_Ok = false;
			return;
		}
		r.m_Ok = true;
		v = DZS_PropertiesPlugin.Clamp(i, DZS_PropText.Round(v, DZS_PropertiesPlugin.Decimals(i)));
		m_Work.SetValue(i, v);
		f.ForceValue(DZS_PropText.Num(v, DZS_PropertiesPlugin.Decimals(i)));
		Refresh();
	}

	void OnFieldWheel(DZS_NumberField f, int wheel, bool fine)
	{
		int i = m_Fields.Find(f);
		if (i < 0 || m_Locked || m_Gone)
			return;
		float step = DZS_PropertiesPlugin.StepOf(i);
		if (fine)
			step = step * 0.1;
		float v = DZS_PropertiesPlugin.Clamp(i, DZS_PropText.Round(m_Work.Value(i) + wheel * step, 4));
		m_Work.SetValue(i, v);
		f.ForceValue(DZS_PropText.Num(v, DZS_PropertiesPlugin.Decimals(i)));
	}

	void OnFieldReset(DZS_NumberField f)
	{
		int i = m_Fields.Find(f);
		if (i < 3)
			return;
		float v = 0;
		if (i == 6)
			v = 1;
		m_Work.SetValue(i, v);
		Refresh();
	}

	void OnFieldDrag(DZS_NumberField f, int phase, float steps)
	{
		int i = m_Fields.Find(f);
		if (i < 0)
			return;
		if (phase == DZS_NumberField.DRAG_START)
		{
			m_DragField = i;
			m_DragStart = m_Work.Value(i);
			return;
		}
		if (i != m_DragField)
			return;
		if (phase == DZS_NumberField.DRAG_CANCEL)
		{
			m_Work.SetValue(i, m_DragStart);
			m_DragField = -1;
			Refresh();
			return;
		}
		float v = DZS_PropertiesPlugin.Clamp(i, m_DragStart + steps * DZS_PropertiesPlugin.StepOf(i));
		v = DZS_PropText.Round(v, DZS_PropertiesPlugin.Decimals(i));
		m_Work.SetValue(i, v);
		f.ForceValue(DZS_PropText.Num(v, DZS_PropertiesPlugin.Decimals(i)));
		if (phase == DZS_NumberField.DRAG_END)
		{
			m_DragField = -1;
			Refresh();
		}
	}

	void OnPersistToggle(DZS_Toggle t, bool on)
	{
		m_Work.m_Persist = on;
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (h == m_PersistHit && m_Persist.m_Hit.m_Enabled)
		{
			m_Persist.Flip();
			return;
		}
		super.OnHitClick(h, button);
	}

	void OnVariantChange(string custom)
	{
		m_Work.m_Custom = custom;
		m_Custom.SetText(custom);
		UpdateNote();
	}

	void OnCustomCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		r.m_Ok = true;
		SetCustom(typed);
	}

	void OnCustomClear(DZS_TextField f)
	{
		SetCustom("");
		m_Custom.SetText("");
	}

	protected void SetCustom(string custom)
	{
		m_Work.m_Custom = custom;
		m_Variant.SetCustom(custom);
		UpdateNote();
	}

	// ---- foot ----

	protected bool HasChanges()
	{
		return DZS_PropertiesPlugin.VecChanged(m_Seen.m_Pos, m_Work.m_Pos, 0.000001) || DZS_PropertiesPlugin.VecChanged(m_Seen.m_Ypr, m_Work.m_Ypr, 0.000001) || Math.AbsFloat(m_Seen.m_Scale - m_Work.m_Scale) > 0.0000001 || m_Seen.m_Persist != m_Work.m_Persist || m_Seen.m_Custom != m_Work.m_Custom;
	}

	override void OnFoot(string tag)
	{
		if (tag == "copyText")
		{
			m_P.Kernel().GetUi().Trigger("placement.copyText");
			return;
		}
		if (tag != "apply")
		{
			Close();
			return;
		}
		if (m_ViewOnly)
			return;
		DZS_Screen scr = m_P.Kernel().GetScreen();
		if (scr.IsFieldFocused())
			scr.Unfocus(true);
		DZS_PropObj now = DZS_PropObj.Read(m_P.Inst(), m_Id);
		if (!now)
		{
			m_Gone = true;
			GoneFoot();
			UpdateNote();
			DZS_Log.Info("properties: details apply, object gone");
			return;
		}
		if (DZS_PropertiesPlugin.VecChanged(now.m_Pos, m_Seen.m_Pos, 0.000001) || DZS_PropertiesPlugin.VecChanged(now.m_Ypr, m_Seen.m_Ypr, 0.000001) || now.m_Scale != m_Seen.m_Scale || now.m_Persist != m_Seen.m_Persist || now.m_Custom != m_Seen.m_Custom || now.m_Name != m_Seen.m_Name)
		{
			m_Changed = true;
			m_Seen = now;
			m_Work = now.Copy();
			m_Variant.SetObject(m_P.Inst(), m_Work);
			m_Custom.SetText(m_Work.m_Custom);
			m_Persist.SetOn(m_Work.m_Persist);
			Refresh();
			DZS_Log.Info("properties: details apply refused, changed meanwhile");
			return;
		}
		if (!HasChanges())
		{
			DZS_Log.Info("properties: details apply without change");
			Close();
			return;
		}
		string game = DZS_PropText.GameName(m_Seen.m_Name);
		m_P.Submit(m_Seen, m_Work, game + " ändern", "Change " + game);
		Close();
	}

	void OnSwap(DZS_Button b)
	{
		if (m_Locked || m_Gone)
			return;
		if (!HasChanges())
		{
			Close();
			m_P.StartSwapFor(m_Id);
			return;
		}
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(DZS_Texts.Pick("Objekt", "Object"), DZS_Texts.Pick("Änderungen verwerfen?", "Discard changes?"), DZS_Texts.Pick("Die geänderten Werte gehen verloren.", "The changed values are lost."));
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("discard", DZS_Texts.Pick("Verwerfen", "Discard"), "danger");
		c.m_OnResult.Insert(OnDiscardAnswer);
		c.Open();
	}

	void OnDiscardAnswer(DZS_Dialog d, string result)
	{
		if (result != "discard")
			return;
		Close();
		m_P.StartSwapFor(m_Id);
	}
}

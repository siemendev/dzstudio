// The variant row: "Flag" at flag poles, "Trigger id" at the silo parts.
// In the properties part a change is an operation at once; in "Details …" it only changes the dialog's value.
class DZS_PropVariantRow : Managed
{
	protected DZS_PropertiesPlugin m_P;
	protected bool m_InDialog;
	protected Widget m_Frame;
	protected TextWidget m_Label;
	protected ref DZS_Hit m_LabelHit;
	protected int m_Kind;
	protected string m_Id;
	protected string m_Name;
	protected string m_Custom;
	protected bool m_Off;
	protected string m_OffTip;
	protected bool m_Shown;

	// flag
	protected Widget m_FlagTile;
	protected ImageWidget m_FlagIcon;
	protected TextWidget m_FlagText;
	protected ImageWidget m_FlagWarnIcon;
	protected ref DZS_WrapText m_FlagWarn;
	protected ref DZS_Button m_FlagButton;
	protected ref DZS_Hit m_FlagTip;
	protected bool m_FlagWarnShown;

	// trigger tag
	protected ref DZS_TextField m_Tag;
	protected Widget m_SugBox;
	protected ref array<ref DZS_SugRow> m_SugRows;
	protected string m_SugFor;

	// dialog: (string newCustom)
	ref ScriptInvoker m_OnChange;

	void DZS_PropVariantRow(DZS_PropertiesPlugin p, Widget parent, bool inDialog)
	{
		m_P = p;
		m_InDialog = inDialog;
		m_OnChange = new ScriptInvoker();
		m_Frame = DZS_W.Frame(parent);
		m_Label = DZS_W.Text(m_Frame, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_LabelHit = new DZS_Hit(m_Frame, null, "properties.variantLabel");
		m_LabelHit.m_FocusKind = "";
		m_FlagTile = DZS_W.Panel(m_Frame, DZS_Color.Get("well"));
		m_FlagIcon = DZS_W.Icon(m_Frame, "flag", DZS_Color.Get("textMuted"));
		m_FlagText = DZS_W.Text(m_Frame, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_FlagTip = new DZS_Hit(m_Frame, null, "properties.flagText");
		m_FlagTip.m_FocusKind = "";
		m_FlagWarnIcon = DZS_W.Icon(m_Frame, "triangle-alert", DZS_Color.Get("warning"));
		m_FlagWarn = new DZS_WrapText(m_Frame, DZS_Font.READ, 12, DZS_Color.Get("warning"));
		m_FlagWarn.SetText(DZS_Texts.Pick("Kein Flaggenname — ob der Server eine Flagge zeigt, ist offen", "No flag name — whether the server shows a flag is open"));
		m_FlagButton = new DZS_Button(m_Frame, "side", DZS_Texts.Pick("Flagge …", "Flag …"), "", 28);
		m_FlagButton.m_OnClick.Insert(OnFlagButton);
		if (!inDialog)
			m_FlagButton.m_Hit.m_FocusKind = "";
		m_Tag = new DZS_TextField("properties.tag", "", "textMuted", 200);
		m_Tag.MarkChangesSelection();
		m_Tag.Build(m_Frame);
		m_Tag.EnableClear();
		m_Tag.SetClearOnFocus(true);
		m_Tag.m_OnCommit.Insert(OnTagCommit);
		m_Tag.m_OnClear.Insert(OnTagClear);
		m_SugRows = new array<ref DZS_SugRow>();
		Show(false);
	}

	DZS_TextField TagField() { return m_Tag; }
	DZS_Button FlagButton() { return m_FlagButton; }
	int Kind() { return m_Kind; }

	void Show(bool s)
	{
		m_Shown = s;
		m_Frame.Show(s);
		if (!s)
			HideSuggestions();
	}

	void SetObject(DZS_Instance inst, DZS_PropObj o)
	{
		m_Id = o.m_Id;
		m_Name = o.m_Name;
		m_Kind = DZS_Variants.KindOf(o.m_Name);
		m_Off = o.m_Locked || DZS_PropText.HasControlChars(o.m_Custom);
		m_OffTip = "";
		if (o.m_Locked)
			m_OffTip = m_P.LockedTip();
		else if (m_Off)
			m_OffTip = DZS_Texts.Get("properties.control");
		SetCustom(o.m_Custom);
		Show(m_Kind != DZS_Variants.NONE);
	}

	// The text the row shows (in the dialog the intermediate value of the customString field).
	void SetCustom(string custom)
	{
		m_Custom = custom;
		if (m_Kind == DZS_Variants.FLAG)
		{
			m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Flagge", "Flag")));
			string flag = DZS_Variants.FlagOf(custom);
			string text;
			string tip = "";
			m_FlagWarnShown = false;
			if (custom == "")
				text = DZS_Texts.Pick("Keine Flagge", "No flag");
			else if (flag != "")
				text = DZS_PropText.GameName(flag) + " · " + flag;
			else
			{
				text = DZS_Texts.Pick("Eigener Text: ", "Own text: ") + custom;
				tip = custom;
				m_FlagWarnShown = !DZS_PropText.HasControlChars(custom);
			}
			bool cut = DZS_W.FitText(m_FlagText, text, DZS_Scale.Px(150));
			if (!cut)
				tip = "";
			if (m_Off)
				tip = m_OffTip;
			m_FlagTip.SetTip(tip);
			m_FlagButton.SetEnabled(!m_Off, m_OffTip);
			m_LabelHit.SetTip("");
		}
		else if (m_Kind == DZS_Variants.TRIGGER)
		{
			m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Trigger-Kennung", "Trigger tag")));
			string tag = DZS_Variants.TagOf(custom);
			if (!m_Tag.IsFocused())
				m_Tag.SetText(tag);
			m_Tag.ShowClear(tag != "");
			m_Tag.SetEnabled(!m_Off);
			string ltip = DZS_Texts.Pick("Verbindet das Teil mit den Untergrund-Triggern gleicher Kennung auf dem Server", "Links the part to the underground triggers with the same tag on the server");
			if (m_Off)
				ltip = m_OffTip;
			m_LabelHit.SetTip(ltip);
		}
		bool flagRow = m_Kind == DZS_Variants.FLAG;
		m_FlagTile.Show(flagRow);
		m_FlagIcon.Show(flagRow);
		m_FlagText.Show(flagRow);
		m_FlagTip.m_W.Show(flagRow);
		m_FlagButton.Show(flagRow);
		m_FlagWarnIcon.Show(flagRow && m_FlagWarnShown);
		m_FlagWarn.Show(flagRow && m_FlagWarnShown);
		m_Tag.Show(m_Kind == DZS_Variants.TRIGGER);
	}

	float Layout(float x, float y, float w)
	{
		if (!m_Shown)
			return 0;
		float h = 0;
		DZS_W.Place(m_Frame, x, y, w, DZS_Scale.Px(200));
		DZS_W.Place(m_Label, 0, 0, w, DZS_Scale.Px(14));
		m_LabelHit.Place(0, 0, DZS_W.TextWidth(m_Label) + 4, DZS_Scale.Px(14));
		h = DZS_Scale.Px(15);
		if (m_Kind == DZS_Variants.FLAG)
		{
			float t = DZS_Scale.Px(24);
			DZS_W.Place(m_FlagTile, 0, h + DZS_Scale.Px(2), t, t);
			float ic = DZS_Scale.Px(14);
			DZS_W.Place(m_FlagIcon, (t - ic) / 2, h + DZS_Scale.Px(2) + (t - ic) / 2, ic, ic);
			float tx = t + DZS_Scale.Px(8);
			float bw = m_FlagButton.GetWidth();
			DZS_W.Place(m_FlagText, tx, h, w - tx - bw - DZS_Scale.Px(8), DZS_Scale.Px(28));
			m_FlagTip.Place(tx, h, w - tx - bw - DZS_Scale.Px(8), DZS_Scale.Px(28));
			m_FlagButton.Place(w - bw, h);
			h += DZS_Scale.Px(32);
			if (m_FlagWarnShown)
			{
				float wi = DZS_Scale.Px(14);
				DZS_W.Place(m_FlagWarnIcon, 0, h + DZS_Scale.Px(2), wi, wi);
				h += m_FlagWarn.Place(wi + DZS_Scale.Px(6), h, w - wi - DZS_Scale.Px(6)) + DZS_Scale.Px(4);
			}
		}
		else
		{
			m_Tag.SetRefHeight(28);
			int wref = w / DZS_Scale.Factor();
			m_Tag.SetWidthRef(wref);
			m_Tag.Place(0, h, DZS_Scale.Px(28));
			h += DZS_Scale.Px(32);
		}
		return h + DZS_Scale.Px(4);
	}

	void Update(float dt)
	{
		if (!m_Shown || m_Kind != DZS_Variants.TRIGGER)
			return;
		m_Tag.Refresh();
		if (!m_Tag.IsFocused())
		{
			HideSuggestions();
			return;
		}
		SuggestionKeys();
		string typed = m_Tag.GetText();
		if (typed == m_SugFor)
			return;
		ShowSuggestions(DZS_Variants.Suggestions(m_P.Inst(), typed, m_Id));
	}

	protected void ShowSuggestions(array<string> tags)
	{
		HideSuggestions();
		m_SugFor = m_Tag.GetText();
		if (tags.Count() == 0)
			return;
		Widget layer = m_P.Kernel().GetScreen().GetPopupLayer();
		m_SugBox = DZS_W.Frame(layer);
		Widget bg = DZS_W.Panel(m_SugBox, DZS_Color.Get("surfaceRaised"));
		m_SugBorder = new DZS_Border(m_SugBox, DZS_Color.Get("lineStrong"));
		float fx, fy, fw, fh;
		m_Tag.m_Frame.GetScreenPos(fx, fy);
		m_Tag.m_Frame.GetScreenSize(fw, fh);
		float rh = DZS_Scale.Px(28);
		float y = 0;
		foreach (string t : tags)
		{
			DZS_SugRow row = new DZS_SugRow(m_SugBox, t);
			row.m_OnPick.Insert(OnSuggestion);
			row.Place(0, y, fw, rh);
			m_SugRows.Insert(row);
			y += rh;
		}
		m_SugIdx = -1;
		DZS_W.Place(m_SugBox, fx, fy + fh + DZS_Scale.Px(2), fw, y);
		DZS_W.Place(bg, 0, 0, fw, y);
		m_SugBorder.Place(0, 0, fw, y);
		DZS_Log.Info("properties: " + tags.Count() + " tag suggestion(s) for '" + m_SugFor + "'");
	}

	protected ref DZS_Border m_SugBorder;
	protected int m_SugIdx = -1;
	protected bool m_DownWas;
	protected bool m_UpWas;

	protected void HideSuggestions()
	{
		foreach (DZS_SugRow r : m_SugRows)
			r.Destroy();
		m_SugRows.Clear();
		if (m_SugBox)
			m_SugBox.Unlink();
		m_SugBox = null;
		m_SugBorder = null;
		m_SugFor = "~";
		m_SugIdx = -1;
	}

	// Arrow keys move the mark in the list while the field has the focus.
	protected void SuggestionKeys()
	{
		bool down = KeyState(KeyCode.KC_DOWN) != 0;
		bool up = KeyState(KeyCode.KC_UP) != 0;
		if (m_SugRows.Count() > 0)
		{
			if (down && !m_DownWas)
				m_SugIdx = Math.Min(m_SugIdx + 1, m_SugRows.Count() - 1);
			if (up && !m_UpWas)
				m_SugIdx = Math.Max(m_SugIdx - 1, -1);
			for (int i = 0; i < m_SugRows.Count(); i++)
				m_SugRows[i].SetMarked(i == m_SugIdx);
		}
		m_DownWas = down;
		m_UpWas = up;
	}

	// The suggestion a commit should take: the marked one, or the one under the pointer when the commit is the click.
	protected string PickedSuggestion()
	{
		if (m_SugIdx >= 0 && m_SugIdx < m_SugRows.Count())
			return m_SugRows[m_SugIdx].m_Tag;
		Widget under = GetWidgetUnderCursor();
		foreach (DZS_SugRow r : m_SugRows)
		{
			if (r.m_Hit && r.m_Hit.m_W && DZS_W.IsInside(under, r.m_Hit.m_W))
				return r.m_Tag;
		}
		return "";
	}

	void OnSuggestion(string tag)
	{
		m_P.Kernel().GetScreen().Unfocus(false);
		HideSuggestions();
		ApplyTag(tag);
	}

	void OnTagCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		string picked = PickedSuggestion();
		if (picked != "")
		{
			r.m_Ok = true;
			HideSuggestions();
			ApplyTag(picked);
			return;
		}
		if (!DZS_Variants.ValidTag(typed))
		{
			r.m_Ok = false;
			DZS_Log.Info("properties: tag '" + typed + "' refused");
			return;
		}
		r.m_Ok = true;
		HideSuggestions();
		ApplyTag(typed);
	}

	void OnTagClear(DZS_TextField f)
	{
		HideSuggestions();
		ApplyTag("");
	}

	protected void ApplyTag(string tag)
	{
		if (m_Off)
			return;
		string next = DZS_Variants.WithTag(m_Custom, tag);
		DZS_Log.Info("properties: tag '" + tag + "' -> customString '" + next + "'");
		if (m_InDialog)
		{
			SetCustom(next);
			m_OnChange.Invoke(next);
			return;
		}
		m_P.ApplyCustom(m_Id, next, "Trigger-Kennung", "trigger tag");
	}

	void OnFlagButton(DZS_Button b)
	{
		if (m_Off)
			return;
		DZS_FlagDialog d = new DZS_FlagDialog(this, m_Custom);
		d.Open();
	}

	// From the flag dialog: the flag's class name (or "" for "No flag") replaces the whole text.
	void ChooseFlag(string flag)
	{
		DZS_Log.Info("properties: flag chosen '" + flag + "'");
		if (m_InDialog)
		{
			SetCustom(flag);
			m_OnChange.Invoke(flag);
			return;
		}
		m_P.ApplyCustom(m_Id, flag, "Flagge", "flag");
	}
}

// "Choose flag": search field and tiles, "No flag" first.
class DZS_FlagDialog : DZS_Dialog
{
	protected DZS_PropVariantRow m_Row;
	protected string m_Current;
	protected string m_Marked;
	protected ref DZS_SearchField m_Search;
	protected ref array<string> m_All;
	protected ref array<ref DZS_FlagTile> m_Tiles;
	protected string m_Filter = "~";
	protected string m_LastClick;
	protected float m_LastClickAt;
	protected float m_Now;

	void DZS_FlagDialog(DZS_PropVariantRow row, string custom)
	{
		m_Row = row;
		m_Current = DZS_Variants.FlagOf(custom);
		m_Marked = m_Current;
		m_All = new array<string>();
		m_All.Insert("");
		array<string> flags = DZS_Variants.Flags();
		array<string> sorted = new array<string>();
		foreach (string f : flags)
			sorted.Insert(f);
		for (int i = 0; i < sorted.Count(); i++)
		{
			for (int j = i + 1; j < sorted.Count(); j++)
			{
				string ni = DZS_PropText.GameName(sorted[i]);
				string nj = DZS_PropText.GameName(sorted[j]);
				if (nj < ni || (nj == ni && sorted[j] < sorted[i]))
				{
					string t = sorted[i];
					sorted[i] = sorted[j];
					sorted[j] = t;
				}
			}
		}
		foreach (string s : sorted)
			m_All.Insert(s);
		Setup(MEDIUM, DZS_Texts.Pick("Flagge", "Flag"), DZS_Texts.Pick("Flagge wählen", "Choose flag"), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton("apply", DZS_Texts.Pick("Übernehmen", "Apply"), "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Search = new DZS_SearchField("properties.flagSearch", DZS_Texts.Pick("Flagge suchen …", "Search flag …"));
		m_Search.Build(inner);
		AddFocusField(m_Search);
		m_Tiles = new array<ref DZS_FlagTile>();
		foreach (string f : m_All)
		{
			DZS_FlagTile t = new DZS_FlagTile(inner, this, f);
			m_Tiles.Insert(t);
		}
		AddFootFocus();
	}

	protected string Typed()
	{
		string t = m_Search.GetTyped();
		t.ToLower();
		return t;
	}

	override float LayoutContent(float width)
	{
		m_Search.SetWidth(width / DZS_Scale.Factor());
		m_Search.Place(0, 0, DZS_Scale.Px(32));
		float y = DZS_Scale.Px(44);
		float tile = DZS_Scale.Px(96);
		float gap = DZS_Scale.Px(8);
		float colsF = Math.Floor((width + gap) / (tile + gap));
		int cols = colsF;
		if (cols < 1)
			cols = 1;
		int n = 0;
		string typed = Typed();
		foreach (DZS_FlagTile t : m_Tiles)
		{
			bool show = t.Matches(typed);
			t.Show(show);
			if (!show)
				continue;
			int r = n / cols;
			int c = n - r * cols;
			t.Place(c * (tile + gap), y + r * (tile + gap), tile, t.m_Flag == m_Marked);
			n++;
		}
		int rows = (n + cols - 1) / cols;
		return y + rows * (tile + gap);
	}

	override void UpdateContent(float dt)
	{
		m_Now += dt;
		string typed = Typed();
		if (typed != m_Filter)
		{
			m_Filter = typed;
			int n = 0;
			foreach (DZS_FlagTile t : m_Tiles)
			{
				if (t.Matches(typed))
					n++;
			}
			DZS_Log.Info("properties: flag search '" + typed + "' " + n + " tile(s)");
		}
	}

	void OnTileClick(DZS_FlagTile t)
	{
		if (m_LastClick == t.m_Flag && m_Now - m_LastClickAt < 0.4)
		{
			m_Marked = t.m_Flag;
			Apply();
			return;
		}
		m_LastClick = t.m_Flag;
		m_LastClickAt = m_Now;
		m_Marked = t.m_Flag;
	}

	protected void Apply()
	{
		Close();
		if (m_Marked == m_Current && m_Current != "")
			return;
		m_Row.ChooseFlag(m_Marked);
	}

	override void OnFoot(string tag)
	{
		if (tag == "apply")
		{
			Apply();
			return;
		}
		Close();
	}
}

class DZS_FlagTile : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	string m_Flag;
	protected string m_Search;
	protected DZS_FlagDialog m_D;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected ImageWidget m_Icon;
	protected TextWidget m_Text;
	protected TextWidget m_Sub;

	void DZS_FlagTile(Widget parent, DZS_FlagDialog d, string flag)
	{
		m_D = d;
		m_Flag = flag;
		m_Hit = new DZS_Hit(parent, this, "properties.flagTile");
		m_Hit.m_FocusKind = "";
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("well"));
		m_Border = new DZS_Border(m_Hit.m_W, DZS_Color.Get("line"));
		string icon = "flag";
		if (flag == "")
			icon = "x";
		m_Icon = DZS_W.Icon(m_Hit.m_W, icon, DZS_Color.Get("textMuted"));
		m_Text = DZS_W.Text(m_Hit.m_W, DZS_Font.READ, 12, DZS_Color.Get("text"), true);
		m_Sub = DZS_W.Text(m_Hit.m_W, DZS_Font.MONO, 9, DZS_Color.Get("textQuiet"), true);
		string name = DZS_Texts.Pick("Keine Flagge", "No flag");
		if (flag != "")
			name = DZS_PropText.GameName(flag);
		DZS_W.FitText(m_Text, name, DZS_Scale.Px(90));
		DZS_W.FitText(m_Sub, flag, DZS_Scale.Px(90));
		m_Search = name + " " + flag;
		m_Search.ToLower();
	}

	bool Matches(string typed)
	{
		if (typed == "")
			return true;
		return m_Search.IndexOf(typed) >= 0;
	}

	void Show(bool s) { m_Hit.m_W.Show(s); }

	void Place(float x, float y, float size, bool marked)
	{
		m_Hit.Place(x, y, size, size);
		DZS_W.Place(m_Bg, 0, 0, size, size);
		if (marked)
		{
			m_Border.SetColor(DZS_Color.Get("accent"));
			m_Border.Place(0, 0, size, size, Math.Max(1, DZS_Scale.Px(2)));
		}
		else
		{
			m_Border.SetColor(DZS_Color.Get("line"));
			m_Border.Place(0, 0, size, size, 1);
		}
		float ic = DZS_Scale.Px(28);
		DZS_W.Place(m_Icon, (size - ic) / 2, DZS_Scale.Px(16), ic, ic);
		DZS_W.Place(m_Text, DZS_Scale.Px(3), DZS_Scale.Px(52), size - DZS_Scale.Px(6), DZS_Scale.Px(18));
		DZS_W.Place(m_Sub, DZS_Scale.Px(3), DZS_Scale.Px(72), size - DZS_Scale.Px(6), DZS_Scale.Px(14));
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_D.OnTileClick(this);
	}
}

class DZS_SugRow : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	string m_Tag;
	protected Widget m_Bg;
	protected TextWidget m_Text;
	protected bool m_Marked;
	ref ScriptInvoker m_OnPick;

	void DZS_SugRow(Widget parent, string tag)
	{
		m_Tag = tag;
		m_OnPick = new ScriptInvoker();
		m_Hit = new DZS_Hit(parent, this, "properties.suggestion");
		m_Hit.m_FocusKind = "";
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accentTint"));
		m_Bg.Show(false);
		m_Text = DZS_W.Text(m_Hit.m_W, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_Text.SetText(DZS_W.Literal(tag));
	}

	void Place(float x, float y, float w, float h)
	{
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		DZS_W.Place(m_Text, DZS_Scale.Px(8), 0, w - DZS_Scale.Px(16), h);
	}

	void SetMarked(bool m)
	{
		m_Marked = m;
		m_Bg.Show(m || m_Hit.m_Hover);
	}

	override void OnHitState(DZS_Hit h) { m_Bg.Show(m_Marked || h.m_Hover); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_OnPick.Invoke(m_Tag);
	}

	void Destroy()
	{
		if (m_Hit)
			m_Hit.Destroy();
		m_Hit = null;
	}
}

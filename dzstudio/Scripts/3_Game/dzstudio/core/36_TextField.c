// Text input: a field that opens empty with the old value as placeholder.
class DZS_TextField : DZS_NumberField
{
	protected TextWidget m_Placeholder;
	protected float m_RefH = 28;

	void DZS_TextField(string id = "", string letter = "", string axisColor = "textMuted", int widthRef = 300)
	{
		m_Id = id;
		m_LetterText = letter;
		m_WidthRef = widthRef;
	}

	override void Build(Widget parent)
	{
		m_Frame = DZS_W.HitFrame(parent);
		m_Bg = DZS_W.Panel(m_Frame, DZS_Color.Get("well"));
		m_Border = new DZS_Border(m_Frame, DZS_Color.Get("line"));
		m_Placeholder = DZS_W.Text(m_Frame, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Placeholder.SetText(m_LetterText);
		m_Edit = EditBoxWidget.Cast(DZS_W.FromTexts("edit_read", m_Frame));
		m_Edit.SetColor(DZS_Color.Get("text"));
		m_Edit.SetHandler(DZS_FieldEvents.Get());
		DZS_FieldEvents.Get().Bind(m_Edit, this);
	}

	override bool IsSearch() { return true; }

	protected ref DZS_Button m_Clear;
	protected bool m_ClearWanted;
	// "×" in the field: owner gets (field); the field gets no focus from it
	ref ScriptInvoker m_OnClear;

	void EnableClear()
	{
		if (m_Clear || !m_Frame)
			return;
		m_OnClear = new ScriptInvoker();
		m_Clear = new DZS_Button(m_Frame, "ghost", "", "x", 22);
		m_Clear.m_Hit.m_FocusKind = "";
		m_Clear.SetTip(DZS_Texts.Pick("Leeren", "Clear"));
		m_Clear.m_OnClick.Insert(OnClearClick);
		m_Clear.Show(false);
	}

	void ShowClear(bool show)
	{
		m_ClearWanted = show;
		if (m_Clear)
			m_Clear.Show(show && m_Enabled);
	}

	void OnClearClick(DZS_Button b)
	{
		if (!m_Enabled)
			return;
		if (m_Focused)
			DZS_Kernel.Get().GetScreen().Unfocus(false);
		m_OnClear.Invoke(this);
	}

	void SetRefHeight(float h) { m_RefH = h; }

	override void SetEnabled(bool on)
	{
		super.SetEnabled(on);
		if (m_Clear)
			m_Clear.Show(m_ClearWanted && on);
	}

	void SetPlaceholder(string p)
	{
		m_LetterText = p;
		m_Placeholder.SetText(p);
	}

	override void Place(float x, float y, float h)
	{
		float w = GetWidth();
		float fh = DZS_Scale.Px(m_RefH);
		DZS_W.Place(m_Frame, x, y, w, fh);
		DZS_W.Place(m_Bg, 0, 0, w, fh);
		UpdateBorder();
		float tx = DZS_Scale.Px(8);
		DZS_W.Place(m_Placeholder, tx, 0, w - 2 * tx, fh);
		int eh = DZS_Font.WidgetHeight(DZS_Font.READ, 13);
		float cw = 0;
		if (m_Clear)
		{
			cw = DZS_Scale.Px(24);
			m_Clear.SetFixedWidth(22);
			m_Clear.Place(w - cw, (fh - DZS_Scale.Px(22)) / 2);
		}
		DZS_W.Place(m_Edit, tx, (fh - eh) / 2, w - 2 * tx - cw, eh);
		Refresh();
	}

	void Refresh()
	{
		m_Placeholder.Show(m_Edit.GetText() == "");
	}

	string GetText() { return m_Edit.GetText(); }

	void SetText(string t)
	{
		m_Edit.SetText(t);
		Refresh();
	}

	float Height() { return DZS_Scale.Px(m_RefH); }

	protected bool m_ClearOnFocus;
	protected string m_BeforeFocus;

	// The field empties when it gets the focus; left empty, the old value comes back.
	void SetClearOnFocus(bool on) { m_ClearOnFocus = on; }

	override void OnFocusChanged(bool focused)
	{
		m_Focused = focused;
		if (m_ClearOnFocus)
		{
			if (focused)
			{
				m_BeforeFocus = m_Edit.GetText();
				m_Placeholder.SetText(m_BeforeFocus);
				m_Edit.SetText("");
			}
			else if (m_Edit.GetText() == "")
			{
				m_Edit.SetText(m_BeforeFocus);
				m_Placeholder.SetText(m_LetterText);
			}
		}
		UpdateBorder();
		Refresh();
	}
}

// Project names: 1–60 characters, not only spaces.
class DZS_Names
{
	static int Chars(string s)
	{
		int n = 0;
		for (int i = 0; i < s.Length(); i++)
		{
			if (!DZS_Utf8.IsContinuation(s, i))
				n++;
		}
		return n;
	}

	static bool ValidProjectName(string s)
	{
		int n = Chars(s);
		if (n < 1 || n > 60)
			return false;
		string t = s;
		t.Replace(" ", "");
		return t != "";
	}
}

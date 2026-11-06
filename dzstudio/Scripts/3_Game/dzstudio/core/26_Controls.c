// Controls: knob, switch, slider, search field. Every clickable area is a DZS_Hit;
// the screen finds the hit under the pointer each frame and sends hover, press, click and wheel to it.
class DZS_HitListener : Managed
{
	void OnHitClick(DZS_Hit h, int button) {}
	void OnHitPress(DZS_Hit h, int button) {}
	void OnHitRelease(DZS_Hit h) {}
	void OnHitState(DZS_Hit h) {}
	// true if the wheel was used here (lists scroll, fields step)
	bool OnHitWheel(DZS_Hit h, int delta) { return false; }
	// Space or Enter on a focused element.
	void OnHitActivate(DZS_Hit h) { OnHitClick(h, MouseState.LEFT); }
}

class DZS_Hit : Managed
{
	Widget m_W;
	DZS_HitListener m_Listener;
	string m_Tag;
	bool m_Enabled = true;
	bool m_Hover;
	bool m_Pressed;
	bool m_Focus;
	// focus kinds for Tab and arrows in dialogs: button, toggle, field, list, keyfield, tab; "" = not focusable
	string m_FocusKind;
	bool m_Primary;
	string m_Tip;
	string m_TipKeys;
	// action whose key the tooltip names, resolved when it shows
	string m_TipAction;
	string m_TipReason;
	bool m_Destroyed;
	// a whole surface: takes the pointer and the wheel but is no button (bars, menus, dialogs)
	bool m_Surface;

	void DZS_Hit(Widget parent, DZS_HitListener listener, string tag)
	{
		m_W = DZS_W.HitFrame(parent);
		m_Listener = listener;
		m_Tag = tag;
		DZS_HitRegistry.Add(this);
	}

	void Place(float x, float y, float w, float h)
	{
		DZS_W.Place(m_W, x, y, w, h);
	}

	void SetEnabled(bool on)
	{
		if (m_Enabled == on)
			return;
		m_Enabled = on;
		if (!on)
			m_Pressed = false;
		Changed();
	}

	void Changed()
	{
		if (m_Listener)
			m_Listener.OnHitState(this);
	}

	// Tooltip: a name (with the key caps of an action binding) or, when off, the reason.
	void SetTip(string text, string binding = "")
	{
		m_Tip = text;
		m_TipKeys = binding;
		m_TipAction = "";
	}

	void SetTipAction(string text, string actionId)
	{
		m_Tip = text;
		m_TipKeys = "";
		m_TipAction = actionId;
	}

	void Destroy()
	{
		m_Destroyed = true;
		DZS_HitRegistry.Remove(this);
		if (m_W)
			m_W.Unlink();
		m_W = null;
	}
}

class DZS_HitRegistry
{
	protected static ref map<Widget, DZS_Hit> s_Hits;

	static void Add(DZS_Hit h)
	{
		if (!s_Hits)
			s_Hits = new map<Widget, DZS_Hit>();
		s_Hits.Set(h.m_W, h);
	}

	static void Remove(DZS_Hit h)
	{
		if (s_Hits && h.m_W)
			s_Hits.Remove(h.m_W);
	}

	// The hit under a widget: walks up the parents.
	static DZS_Hit Find(Widget w)
	{
		if (!s_Hits)
			return null;
		while (w)
		{
			DZS_Hit h = s_Hits.Get(w);
			if (h && !h.m_Destroyed && w.IsVisible())
				return h;
			w = w.GetParent();
		}
		return null;
	}

	static void Clear()
	{
		if (s_Hits)
			s_Hits.Clear();
	}
}

// Knob: kinds main, side, danger, ghost, alone (only "Save" in the top bar).
class DZS_Button : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	protected Widget m_Bg;
	protected Widget m_Tint;
	protected ref DZS_Border m_Border;
	protected ref DZS_Border m_FocusRing;
	protected ImageWidget m_Icon;
	protected TextWidget m_Text;
	protected ref DZS_RichLine m_Caps;
	protected string m_Kind;
	protected string m_Label;
	protected string m_CapsBinding;
	protected bool m_On;
	protected float m_H = 28;
	protected int m_FixedWidth;
	protected ref DZS_CutBox m_CutBg;
	protected ref DZS_CutBox m_Cut;
	DZS_HitListener m_Owner;
	string m_Tag;
	bool m_FootLeft;
	ref ScriptInvoker m_OnClick;

	void DZS_Button(Widget parent, string kind, string label, string icon = "", float refHeight = 28)
	{
		m_Kind = kind;
		m_H = refHeight;
		m_OnClick = new ScriptInvoker();
		m_Hit = new DZS_Hit(parent, this, "button");
		m_Hit.m_FocusKind = "button";
		m_Hit.m_Primary = kind == "main";
		Widget f = m_Hit.m_W;
		m_Bg = DZS_W.Panel(f, DZS_Color.Get("surfaceRaised"));
		m_Tint = DZS_W.Panel(f, DZS_Color.Get("accentTint"));
		m_Border = new DZS_Border(f, DZS_Color.Get("lineStrong"));
		m_FocusRing = new DZS_Border(f, DZS_Color.Get("accent"));
		if (kind == "alone")
		{
			m_CutBg = new DZS_CutBox(f, DZS_Color.Get("surfaceRaised"), DZS_Color.Get("accent"));
			m_CutBg.ShowLines(false);
			m_Cut = new DZS_CutBox(f, DZS_Color.Get("accentTint"), DZS_Color.Get("accent"));
		}
		if (icon != "")
			m_Icon = DZS_W.Icon(f, icon, DZS_Color.Get("textMuted"));
		string role = DZS_Font.LABEL;
		float size = 11;
		if (kind == "alone")
		{
			role = DZS_Font.DISPLAY;
			size = 22;
		}
		else if (kind == "menu")
		{
			role = DZS_Font.MENU;
			size = 14;
		}
		m_Text = DZS_W.Text(f, role, size, DZS_Color.Get("textMuted"), true);
		SetLabel(label);
		Refresh();
	}

	Widget GetWidget() { return m_Hit.m_W; }

	void SetLabel(string label)
	{
		m_Label = label;
		m_Text.SetText(DZS_Texts.Upper(label));
		m_Text.Show(label != "");
	}

	// Key caps behind the text (current binding of an action).
	void SetCaps(string binding)
	{
		if (binding == m_CapsBinding)
			return;
		m_CapsBinding = binding;
		if (!m_Caps)
			m_Caps = new DZS_RichLine(m_Hit.m_W, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"));
		if (binding == "")
			m_Caps.Set("");
		else
			m_Caps.Set("[" + binding + "]");
	}

	void SetFixedWidth(int refW) { m_FixedWidth = refW; }

	void SetIcon(string name)
	{
		if (m_Icon)
			DZS_W.SetIcon(m_Icon, name);
	}

	void SetOn(bool on)
	{
		if (m_On == on)
			return;
		m_On = on;
		Refresh();
	}

	bool IsOn() { return m_On; }

	void SetEnabled(bool on, string reason = "")
	{
		m_Hit.m_TipReason = reason;
		m_Hit.SetEnabled(on);
		Refresh();
	}

	void SetTip(string text, string binding = "")
	{
		m_Hit.SetTip(text, binding);
	}

	void SetTipAction(string text, string actionId)
	{
		m_Hit.SetTipAction(text, actionId);
	}

	float GetWidth()
	{
		if (m_FixedWidth > 0)
			return DZS_Scale.Px(m_FixedWidth);
		float h = DZS_Scale.Px(m_H);
		if (m_Label == "" && !m_Caps)
			return h;
		float w = DZS_Scale.Px(12) * 2 + DZS_W.TextWidth(m_Text);
		if (m_Icon)
			w += DZS_Scale.Px(16 + 6);
		if (m_Caps && m_CapsBinding != "")
			w += DZS_Scale.Px(8) + m_Caps.GetWidth();
		return w;
	}

	float GetHeight() { return DZS_Scale.Px(m_H); }

	void Place(float x, float y)
	{
		float w = GetWidth();
		float h = DZS_Scale.Px(m_H);
		if (m_Cut)
		{
			x = Math.Round(x);
			y = Math.Round(y);
			w = Math.Round(w);
			h = Math.Round(h);
		}
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		DZS_W.Place(m_Tint, 0, 0, w, h);
		m_Border.Place(0, 0, w, h);
		m_FocusRing.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		if (m_Cut)
		{
			m_CutBg.Place(w, h);
			m_Cut.Place(w, h);
		}
		float icon = DZS_Scale.Px(16);
		if (m_Label == "" && !m_Caps)
		{
			if (m_Icon)
				DZS_W.Place(m_Icon, (w - icon) / 2, (h - icon) / 2, icon, icon);
			return;
		}
		float x0 = DZS_Scale.Px(12);
		float tw = DZS_W.TextWidth(m_Text);
		float content = tw;
		if (m_Icon)
			content += icon + DZS_Scale.Px(6);
		if (m_Caps && m_CapsBinding != "")
			content += DZS_Scale.Px(8) + m_Caps.GetWidth();
		x0 = (w - content) / 2;
		if (m_Icon)
		{
			DZS_W.Place(m_Icon, x0, (h - icon) / 2, icon, icon);
			x0 += icon + DZS_Scale.Px(6);
		}
		DZS_W.Place(m_Text, x0, 0, tw + 2, h);
		x0 += tw;
		if (m_Caps && m_CapsBinding != "")
			m_Caps.Place(x0 + DZS_Scale.Px(8), (h - m_Caps.GetHeight()) / 2);
	}

	void Show(bool s) { m_Hit.m_W.Show(s); }

	// The seven visual states.
	void Refresh()
	{
		bool enabled = m_Hit.m_Enabled;
		bool hover = enabled && m_Hit.m_Hover;
		bool pressed = enabled && m_Hit.m_Pressed && m_Hit.m_Hover;
		string textCol = "textMuted";
		string borderCol = "lineStrong";
		bool border = m_Kind != "ghost" && m_Kind != "menu";
		bool tint = false;
		if (m_Kind == "main")
		{
			textCol = "accentText";
			borderCol = "accent";
		}
		else if (m_Kind == "danger")
		{
			textCol = "danger";
			borderCol = "danger";
		}
		else if (m_Kind == "alone")
		{
			textCol = "accentText";
			borderCol = "accent";
			tint = true;
		}
		if (hover && m_Kind != "main" && m_Kind != "danger" && m_Kind != "alone")
			textCol = "text";
		if (m_On)
		{
			tint = true;
			border = true;
			borderCol = "accent";
			textCol = "accentText";
		}
		if (!enabled)
		{
			textCol = "textOff";
			if (m_Kind == "alone" || m_Kind == "main" || m_Kind == "danger")
				borderCol = "lineStrong";
			tint = false;
		}
		m_Bg.Show(hover || pressed);
		if (pressed)
			m_Bg.SetColor(DZS_Color.Get("surfacePressed"));
		else
			m_Bg.SetColor(DZS_Color.Get("surfaceRaised"));
		m_Tint.Show(tint);
		m_Border.Show(border);
		m_Border.SetColor(DZS_Color.Get(borderCol));
		m_FocusRing.Show(m_Hit.m_Focus);
		if (m_Cut)
		{
			m_Bg.Show(false);
			m_Tint.Show(false);
			m_Border.Show(false);
			m_CutBg.ShowFill(hover || pressed);
			if (pressed)
				m_CutBg.SetFill(DZS_Color.Get("surfacePressed"));
			else
				m_CutBg.SetFill(DZS_Color.Get("surfaceRaised"));
			m_Cut.ShowFill(tint);
			m_Cut.ShowLines(border);
			m_Cut.SetLine(DZS_Color.Get(borderCol));
		}
		m_Text.SetColor(DZS_Color.Get(textCol));
		if (m_Icon)
			m_Icon.SetColor(DZS_Color.Get(textCol));
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button != MouseState.LEFT || !m_Hit.m_Enabled)
			return;
		m_OnClick.Invoke(this);
		if (m_Owner)
			m_Owner.OnHitClick(m_Hit, button);
	}

	ref ScriptInvoker m_OnPress;

	override void OnHitPress(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT && m_Hit.m_Enabled && m_OnPress)
			m_OnPress.Invoke(this);
	}

	void Destroy()
	{
		m_Hit.Destroy();
	}
}

// Switch: square track 28 × 14 with a square knob; on = knob right, accent tint.
class DZS_Toggle : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	protected Widget m_Track;
	protected Widget m_Tint;
	protected ref DZS_Border m_Border;
	protected Widget m_Knob;
	protected ref DZS_Border m_FocusRing;
	protected bool m_On;
	ref ScriptInvoker m_OnChange;

	void DZS_Toggle(Widget parent)
	{
		m_OnChange = new ScriptInvoker();
		m_Hit = new DZS_Hit(parent, this, "toggle");
		m_Hit.m_FocusKind = "toggle";
		m_Track = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("well"));
		m_Tint = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accentTint"));
		m_Border = new DZS_Border(m_Hit.m_W, DZS_Color.Get("lineStrong"));
		m_Knob = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("textMuted"));
		m_FocusRing = new DZS_Border(m_Hit.m_W, DZS_Color.Get("accent"));
		Refresh();
	}

	static float Width() { return DZS_Scale.Px(28); }
	static float Height() { return DZS_Scale.Px(14); }

	protected float m_X;
	protected float m_Y;

	// The hit area reaches 2 px beyond the track so the focus ring is drawn inside it (children outside do not show).
	void Place(float x, float y)
	{
		m_X = x;
		m_Y = y;
		float w = Width();
		float h = Height();
		float fr = Math.Max(1, DZS_Scale.Px(2));
		float m = fr * 2;
		m_Hit.Place(x - m, y - m, w + 2 * m, h + 2 * m);
		DZS_W.Place(m_Track, m, m, w, h);
		DZS_W.Place(m_Tint, m, m, w, h);
		m_Border.Place(m, m, w, h);
		float pad = DZS_Scale.Px(3);
		float k = h - 2 * pad;
		if (m_On)
			DZS_W.Place(m_Knob, m + w - pad - k, m + pad, k, k);
		else
			DZS_W.Place(m_Knob, m + pad, m + pad, k, k);
		m_FocusRing.Place(0, 0, w + 2 * m, h + 2 * m, fr);
	}

	void SetOn(bool on)
	{
		m_On = on;
		Refresh();
		Place(m_X, m_Y);
	}

	bool IsOn() { return m_On; }

	void Refresh()
	{
		m_Tint.Show(m_On);
		if (m_On)
		{
			m_Border.SetColor(DZS_Color.Get("accent"));
			m_Knob.SetColor(DZS_Color.Get("accentText"));
		}
		else
		{
			m_Border.SetColor(DZS_Color.Get("lineStrong"));
			if (m_Hit.m_Hover)
				m_Knob.SetColor(DZS_Color.Get("text"));
			else
				m_Knob.SetColor(DZS_Color.Get("textMuted"));
		}
		m_FocusRing.Show(m_Hit.m_Focus);
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button != MouseState.LEFT)
			return;
		Flip();
	}

	void Flip()
	{
		SetOn(!m_On);
		m_OnChange.Invoke(this, m_On);
	}
}

// Slider: 20 square blocks up to the value in accent, a square grip; drag or click.
class DZS_Slider : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	protected ref array<Widget> m_Blocks;
	protected Widget m_Grip;
	protected ref DZS_Border m_GripBorder;
	protected float m_Min;
	protected float m_Max;
	protected float m_Value;
	protected float m_X;
	protected float m_Y;
	protected bool m_Drag;
	ref ScriptInvoker m_OnChange;
	ref ScriptInvoker m_OnRelease;

	void DZS_Slider(Widget parent, float min, float max)
	{
		m_Min = min;
		m_Max = max;
		m_OnChange = new ScriptInvoker();
		m_OnRelease = new ScriptInvoker();
		m_Hit = new DZS_Hit(parent, this, "slider");
		m_Blocks = new array<Widget>();
		for (int i = 0; i < 20; i++)
			m_Blocks.Insert(DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("blockEmpty")));
		m_Grip = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("well"));
		m_GripBorder = new DZS_Border(m_Hit.m_W, DZS_Color.Get("accent"));
	}

	static float Width() { return DZS_Scale.Px(20 * 6 + 19 * 2); }
	static float Height() { return DZS_Scale.Px(14); }

	void Place(float x, float y)
	{
		m_X = x;
		m_Y = y;
		Layout();
	}

	protected void Layout()
	{
		float w = Width();
		float h = Height();
		m_Hit.Place(m_X, m_Y, w, h);
		float b = DZS_Scale.Px(6);
		float gap = DZS_Scale.Px(2);
		float t = (m_Value - m_Min) / Math.Max(0.0001, m_Max - m_Min);
		int filled = Math.Round(t * 20);
		for (int i = 0; i < 20; i++)
		{
			DZS_W.Place(m_Blocks[i], i * (b + gap), (h - b) / 2, b, b);
			if (i < filled)
				m_Blocks[i].SetColor(DZS_Color.Get("accent"));
			else
				m_Blocks[i].SetColor(DZS_Color.Get("blockEmpty"));
		}
		float g = DZS_Scale.Px(12);
		float gx = t * (w - g);
		DZS_W.Place(m_Grip, gx, (h - g) / 2, g, g);
		m_GripBorder.Place(gx, (h - g) / 2, g, g, Math.Max(1, DZS_Scale.Px(2)));
	}

	void SetValue(float v)
	{
		m_Value = Math.Clamp(v, m_Min, m_Max);
		Layout();
	}

	float GetValue() { return m_Value; }

	bool IsDragging() { return m_Drag; }

	override void OnHitPress(DZS_Hit h, int button)
	{
		if (button != MouseState.LEFT)
			return;
		m_Drag = true;
		FollowMouse();
	}

	override void OnHitRelease(DZS_Hit h)
	{
		if (!m_Drag)
			return;
		m_Drag = false;
		m_OnRelease.Invoke(this);
	}

	void Update()
	{
		if (!m_Drag)
			return;
		if ((GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) == 0)
		{
			OnHitRelease(m_Hit);
			return;
		}
		FollowMouse();
	}

	protected void FollowMouse()
	{
		int mx, my;
		GetMousePos(mx, my);
		float sx, sy;
		m_Hit.m_W.GetScreenPos(sx, sy);
		float t = Math.Clamp((mx - sx) / Width(), 0, 1);
		m_OnChange.Invoke(this, m_Min + t * (m_Max - m_Min));
	}
}

// Search field in the well with magnifier, placeholder and "×". It is a field like
// the number field: the screen gives it the keyboard; Esc leaves it and keeps the text.
class DZS_SearchField : DZS_NumberField
{
	protected ImageWidget m_Lens;
	protected TextWidget m_Placeholder;
	protected ref DZS_Button m_Clear;
	protected float m_RefH = 32;
	protected float m_W;

	// letter: the placeholder text (same signature as the number field).
	void DZS_SearchField(string id = "", string letter = "", string axisColor = "textMuted", int widthRef = 100)
	{
		m_Id = id;
		m_LetterText = letter;
	}

	override void Build(Widget parent)
	{
		m_Frame = DZS_W.HitFrame(parent);
		m_Bg = DZS_W.Panel(m_Frame, DZS_Color.Get("well"));
		m_Border = new DZS_Border(m_Frame, DZS_Color.Get("line"));
		m_Lens = DZS_W.Icon(m_Frame, "search", DZS_Color.Get("textQuiet"));
		m_Placeholder = DZS_W.Text(m_Frame, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_Placeholder.SetText(m_LetterText);
		m_Edit = EditBoxWidget.Cast(DZS_W.FromTexts("edit_read", m_Frame));
		m_Edit.SetColor(DZS_Color.Get("text"));
		m_Edit.SetHandler(DZS_FieldEvents.Get());
		DZS_FieldEvents.Get().Bind(m_Edit, this);
		m_Clear = new DZS_Button(m_Frame, "ghost", "", "x", 24);
		m_Clear.m_OnClick.Insert(OnClear);
		m_Clear.SetTip(DZS_Texts.Get("core.search.clear"));
	}

	override bool IsSearch() { return true; }

	void SetWidth(float refW) { m_W = refW; }

	override int GetWidth() { return DZS_Scale.Px(m_W); }

	override void Place(float x, float y, float h)
	{
		float w = GetWidth();
		float fh = DZS_Scale.Px(m_RefH);
		DZS_W.Place(m_Frame, x, y, w, fh);
		DZS_W.Place(m_Bg, 0, 0, w, fh);
		UpdateBorder();
		float icon = DZS_Scale.Px(16);
		DZS_W.Place(m_Lens, DZS_Scale.Px(8), (fh - icon) / 2, icon, icon);
		float tx = DZS_Scale.Px(8 + 16 + 8);
		float cw = DZS_Scale.Px(24);
		DZS_W.Place(m_Placeholder, tx, 0, w - tx - cw, fh);
		int eh = DZS_Font.WidgetHeight(DZS_Font.READ, 13);
		DZS_W.Place(m_Edit, tx, (fh - eh) / 2, w - tx - cw - DZS_Scale.Px(4), eh);
		m_Clear.Place(w - cw - DZS_Scale.Px(4), (fh - DZS_Scale.Px(24)) / 2);
		RefreshExtras();
	}

	void RefreshExtras()
	{
		string t = m_Edit.GetText();
		m_Placeholder.Show(t == "" && !m_Focused);
		m_Clear.Show(t != "");
	}

	string GetText() { return m_Edit.GetText(); }

	void SetText(string t)
	{
		m_Edit.SetText(t);
		RefreshExtras();
	}

	void OnClear(DZS_Button b)
	{
		SetText("");
	}

	override void OnFocusChanged(bool focused)
	{
		m_Focused = focused;
		UpdateBorder();
		RefreshExtras();
	}
}

// Category button: small caps label, accent when chosen.
class DZS_Chip : DZS_HitListener
{
	string m_Key;
	ref DZS_Hit m_Hit;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected TextWidget m_T;
	bool m_On;
	ref ScriptInvoker m_OnClick;

	void DZS_Chip(Widget parent, string key, string label)
	{
		m_Key = key;
		m_OnClick = new ScriptInvoker();
		m_Hit = new DZS_Hit(parent, this, "chip");
		m_Hit.m_FocusKind = "";
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accentTint"));
		m_Border = new DZS_Border(m_Hit.m_W, DZS_Color.Get("line"));
		m_T = DZS_W.Text(m_Hit.m_W, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"), true);
		m_T.SetText(DZS_Texts.Upper(label));
	}

	float Width() { return DZS_W.TextWidth(m_T) + DZS_Scale.Px(16); }
	static float Height() { return DZS_Scale.Px(24); }

	void Place(float x, float y, float w = 0)
	{
		if (w <= 0)
			w = Width();
		float h = Height();
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		m_Border.Place(0, 0, w, h, 1);
		DZS_W.Place(m_T, 0, 0, w, h);
		Refresh();
	}

	void SetOn(bool on)
	{
		m_On = on;
		Refresh();
	}

	void SetEnabled(bool on, string reason = "")
	{
		m_Hit.m_TipReason = reason;
		m_Hit.SetEnabled(on);
		Refresh();
	}

	void Show(bool s) { m_Hit.m_W.Show(s); }

	void SetLabel(string label) { m_T.SetText(DZS_Texts.Upper(label)); }

	void Refresh()
	{
		m_Bg.Show(m_On || m_Hit.m_Hover);
		if (m_On)
		{
			m_Bg.SetColor(DZS_Color.Get("accentTint"));
			m_Border.SetColor(DZS_Color.Get("accent"));
			m_T.SetColor(DZS_Color.Get("accentText"));
		}
		else
		{
			m_Bg.SetColor(DZS_Color.Get("surfaceRaised"));
			m_Border.SetColor(DZS_Color.Get("lineStrong"));
			if (m_Hit.m_Enabled)
				m_T.SetColor(DZS_Color.Get("textMuted"));
			else
				m_T.SetColor(DZS_Color.Get("textQuiet"));
		}
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_OnClick.Invoke(this);
	}
}

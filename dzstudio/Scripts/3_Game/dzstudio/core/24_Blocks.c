// Building blocks the kernel offers every plugin. Positions are screen pixels;
// callers convert reference pixels with DZS_Scale.
class DZS_W
{
	protected static ref map<Widget, int> s_TextH;

	static Widget Make(string layout, Widget parent)
	{
		return GetGame().GetWorkspace().CreateWidgets(layout, parent);
	}

	static Widget Frame(Widget parent)
	{
		return Make("dzstudio/gui/layouts/dzs_frame.layout", parent);
	}

	// A frame that takes the pointer: clickable areas and surfaces that hide the scene from the pointer.
	static Widget HitFrame(Widget parent)
	{
		return Make("dzstudio/gui/layouts/dzs_hit.layout", parent);
	}

	static Widget Panel(Widget parent, int color)
	{
		Widget w = Make("dzstudio/gui/layouts/dzs_panel.layout", parent);
		w.SetColor(color);
		return w;
	}

	// All text and edit widgets live in one layout file: separate small layout files of the same widget class
	// were mixed up by the game (DayZ 1.30 drew one role in another's font). The child is moved to the parent.
	static Widget FromTexts(string name, Widget parent)
	{
		Widget all = Make("dzstudio/gui/layouts/dzs_texts.layout", null);
		Widget w = all.FindAnyWidget(name);
		all.RemoveChild(w);
		if (parent)
			parent.AddChild(w);
		all.Unlink();
		return w;
	}

	// Text of a font role at a reference size; its widget is exactly as high as the font's line needs.
	static TextWidget Text(Widget parent, string role, float refSize, int color, bool centered = false, bool right = false)
	{
		string name = role;
		if (centered)
			name += "_c";
		else if (right)
			name += "_r";
		TextWidget t = TextWidget.Cast(FromTexts(name, parent));
		t.SetColor(color);
		int h = DZS_Font.WidgetHeight(role, refSize);
		t.SetSize(100, h);
		if (!s_TextH)
			s_TextH = new map<Widget, int>();
		s_TextH.Set(t, h);
		return t;
	}

	static int TextHeight(Widget t)
	{
		if (!s_TextH || !s_TextH.Contains(t))
			return 0;
		return s_TextH.Get(t);
	}

	// Line icon of the bundled set, tinted with a named colour.
	static ImageWidget Icon(Widget parent, string name, int color)
	{
		ImageWidget im = ImageWidget.Cast(Make("dzstudio/gui/layouts/dzs_image.layout", parent));
		SetIcon(im, name);
		im.SetColor(color);
		return im;
	}

	static void SetIcon(ImageWidget im, string name)
	{
		im.LoadImageFile(0, "dzstudio/gui/icons/" + name + ".edds");
		im.SetImage(0);
	}

	// Text widgets keep their line height and are centred in the given box.
	static void Place(Widget w, float x, float y, float width, float height)
	{
		int th = TextHeight(w);
		if (th > 0)
		{
			w.SetPos(x, y + Math.Round((height - th) / 2));
			w.SetSize(width, th);
			return;
		}
		w.SetPos(x, y);
		w.SetSize(width, height);
	}

	static int TextWidth(TextWidget t)
	{
		int w, h;
		t.GetTextSize(w, h);
		return w;
	}

	static bool IsInside(Widget w, Widget root)
	{
		while (w)
		{
			if (w == root)
				return true;
			w = w.GetParent();
		}
		return false;
	}

	// Text widgets take "%" plus a digit as a placeholder and drop both (DayZ 1.30, no escape); a zero-width
	// space (in every bundled font) between them keeps such text as it is. Apostrophes become ’ (DZS_Texts.Shown).
	static string Literal(string raw)
	{
		string text = DZS_Texts.Shown(raw);
		if (!text.Contains("%"))
			return text;
		string r = "";
		int n = text.Length();
		for (int i = 0; i < n; i++)
		{
			string c = text.Get(i);
			r += c;
			if (c == "%" && i + 1 < n && "0123456789".Contains(text.Get(i + 1)))
				r += "​";
		}
		return r;
	}

	// Cuts a text with "…" to a width; returns true if it was cut.
	static bool FitText(TextWidget t, string text, float maxWidth)
	{
		text = Literal(text);
		t.SetText(text);
		if (TextWidth(t) <= maxWidth)
			return false;
		string s = text;
		while (s.Length() > 1)
		{
			s = s.Substring(0, DZS_Utf8.SafeCut(s, s.Length() - 1));
			t.SetText(s + "…");
			if (TextWidth(t) <= maxWidth)
				break;
		}
		return true;
	}
}

// A 1-px border made of four panels around a rectangle.
class DZS_Border : Managed
{
	protected Widget m_T;
	protected Widget m_B;
	protected Widget m_L;
	protected Widget m_R;

	void DZS_Border(Widget parent, int color)
	{
		m_T = DZS_W.Panel(parent, color);
		m_B = DZS_W.Panel(parent, color);
		m_L = DZS_W.Panel(parent, color);
		m_R = DZS_W.Panel(parent, color);
	}

	void Place(float x, float y, float w, float h, int thick = 1, int bottomThick = -1)
	{
		if (bottomThick < 0)
			bottomThick = thick;
		DZS_W.Place(m_T, x, y, w, thick);
		DZS_W.Place(m_B, x, y + h - bottomThick, w, bottomThick);
		DZS_W.Place(m_L, x, y, thick, h);
		DZS_W.Place(m_R, x + w - thick, y, thick, h);
	}

	void SetColor(int c)
	{
		m_T.SetColor(c);
		m_B.SetColor(c);
		m_L.SetColor(c);
		m_R.SetColor(c);
	}

	void Show(bool show)
	{
		m_T.Show(show);
		m_B.Show(show);
		m_L.Show(show);
		m_R.Show(show);
	}

	void Remove()
	{
		m_T.Unlink();
		m_B.Unlink();
		m_L.Unlink();
		m_R.Unlink();
	}
}

// One line of text with key caps: "Free view — [SPACE] to end". [..] holds a binding by position
// ("SPACE", "Ctrl+C") and is drawn as caps of the UI language.
class DZS_RichLine : Managed
{
	protected Widget m_Frame;
	protected string m_Role;
	protected float m_Size;
	protected int m_Color;
	protected string m_Markup;
	protected ref array<Widget> m_Parts;
	protected ref array<ref DZS_Border> m_Caps;
	protected int m_Width;
	protected int m_Height;

	void DZS_RichLine(Widget parent, string role, float refSize, int color)
	{
		m_Frame = DZS_W.Frame(parent);
		m_Role = role;
		m_Size = refSize;
		m_Color = color;
		m_Parts = new array<Widget>();
		m_Caps = new array<ref DZS_Border>();
	}

	Widget GetWidget() { return m_Frame; }
	int GetWidth() { return m_Width; }
	int GetHeight() { return m_Height; }

	void SetColor(int c)
	{
		m_Color = c;
		string keep = m_Markup;
		m_Markup = "";
		Set(keep);
	}

	void Set(string markup)
	{
		if (markup == m_Markup)
			return;
		m_Markup = markup;
		foreach (Widget w : m_Parts)
			w.Unlink();
		m_Parts.Clear();
		foreach (DZS_Border old : m_Caps)
			old.Remove();
		m_Caps.Clear();
		int x = 0;
		m_Height = Math.Max(DZS_Font.WidgetHeight(m_Role, m_Size), DZS_Font.WidgetHeight(DZS_Font.LABELBOLD, 10) + DZS_Scale.Px(4));
		int pad = DZS_Scale.Px(4);
		string rest = markup;
		while (rest.Length() > 0)
		{
			int open = rest.IndexOf("[");
			string plain = rest;
			string key = "";
			if (open >= 0)
			{
				int close = rest.IndexOfFrom(open, "]");
				if (close > open)
				{
					plain = rest.Substring(0, open);
					key = rest.Substring(open + 1, close - open - 1);
					rest = rest.Substring(close + 1, rest.Length() - close - 1);
				}
				else
					rest = "";
			}
			else
				rest = "";
			if (plain.Length() > 0)
			{
				TextWidget t = DZS_W.Text(m_Frame, m_Role, m_Size, m_Color);
				t.SetText(plain);
				int tw = DZS_W.TextWidth(t);
				DZS_W.Place(t, x, 0, tw + 2, m_Height);
				m_Parts.Insert(t);
				x += tw;
			}
			if (key != "")
			{
				array<string> caps = DZS_KeyNames.Caps(key);
				foreach (int ci, string cap : caps)
				{
					if (ci > 0)
						x += DZS_Scale.Px(3);
					TextWidget ct = DZS_W.Text(m_Frame, DZS_Font.LABELBOLD, 10, DZS_Color.Get("textMuted"));
					ct.SetText(cap);
					int cw = DZS_W.TextWidth(ct) + 2 * pad;
					int ch = DZS_W.TextHeight(ct) + DZS_Scale.Px(2);
					int cy = (m_Height - ch) / 2;
					DZS_W.Place(ct, x + pad, cy, cw - 2 * pad + 2, ch);
					DZS_Border b = new DZS_Border(m_Frame, DZS_Color.Get("capBorder"));
					b.Place(x, cy, cw, ch, 1, 2);
					m_Caps.Insert(b);
					m_Parts.Insert(ct);
					x += cw;
				}
				x += DZS_Scale.Px(2);
			}
		}
		m_Width = x;
		m_Frame.SetSize(m_Width, m_Height);
	}

	void Destroy()
	{
		if (m_Frame)
			m_Frame.Unlink();
		m_Frame = null;
	}

	void Place(float x, float y)
	{
		m_Frame.SetPos(x, y);
	}

	void Show(bool show)
	{
		m_Frame.Show(show);
	}
}

// A plate with a tone: used by messages and banners.
class DZS_Plate : Managed
{
	Widget m_Frame;
	Widget m_Bg;
	Widget m_Tint;
	ref DZS_Border m_Border;
	ImageWidget m_Icon;
	ref DZS_RichLine m_Line;
	int m_W;
	int m_H;
	bool m_HasIcon;

	// tone: info, success, warning, danger (with its symbol), or a neutral colour name for tooltips (no symbol).
	void DZS_Plate(Widget parent, string tone, string markup, string role = "read", float size = 14)
	{
		m_Frame = DZS_W.Frame(parent);
		m_Bg = DZS_W.Panel(m_Frame, DZS_Color.Get("plate"));
		string icon = ToneIcon(tone);
		m_HasIcon = icon != "";
		if (m_HasIcon)
		{
			m_Tint = DZS_W.Panel(m_Frame, DZS_Color.WithAlpha(tone, 38, "plate"));
			m_Border = new DZS_Border(m_Frame, DZS_Color.WithAlpha(tone, 102, "plate"));
			m_Icon = DZS_W.Icon(m_Frame, icon, DZS_Color.Get(tone));
		}
		else
		{
			m_Tint = DZS_W.Panel(m_Frame, 0);
			m_Border = new DZS_Border(m_Frame, DZS_Color.Get("lineStrong"));
		}
		m_Line = new DZS_RichLine(m_Frame, role, size, DZS_Color.Get("text"));
		SetText(markup);
	}

	static string ToneIcon(string tone)
	{
		if (tone == "info")
			return "info";
		if (tone == "success")
			return "circle-check";
		if (tone == "warning")
			return "triangle-alert";
		if (tone == "danger")
			return "circle-x";
		return "";
	}

	void SetText(string markup)
	{
		m_Line.Set(markup);
		int pad = DZS_Scale.Px(12);
		int icon = DZS_Scale.Px(16);
		int iconSpace = 0;
		if (m_HasIcon)
			iconSpace = icon + DZS_Scale.Px(8);
		else
			pad = DZS_Scale.Px(8);
		m_H = m_Line.GetHeight() + DZS_Scale.Px(10);
		m_W = pad + iconSpace + m_Line.GetWidth() + pad;
		Resize();
		if (m_HasIcon)
			DZS_W.Place(m_Icon, pad, (m_H - icon) / 2, icon, icon);
		m_Line.Place(pad + iconSpace, (m_H - m_Line.GetHeight()) / 2);
	}

	void Resize()
	{
		DZS_W.Place(m_Bg, 0, 0, m_W, m_H);
		DZS_W.Place(m_Tint, 0, 0, m_W, m_H);
		m_Border.Place(0, 0, m_W, m_H);
		m_Frame.SetSize(m_W, m_H);
	}

	void Place(float x, float y)
	{
		m_Frame.SetPos(x, y);
	}

	void SetAlpha(float a)
	{
		m_Frame.SetAlpha(a);
	}

	void Show(bool s)
	{
		m_Frame.Show(s);
	}

	void Destroy()
	{
		if (m_Frame)
			m_Frame.Unlink();
	}
}

// Text segment of the status bar; cut with "…" at its maximum width, full text as tooltip.
class DZS_StatusItem : Managed
{
	string m_Id;
	Widget m_Frame;
	int m_MaxWidth;
	string m_Tooltip;

	void Build(Widget parent) {}
	int GetWidth() { return 0; }
	void Place(float x, float y, float h) {}
	void Show(bool s) { if (m_Frame) m_Frame.Show(s); }
	void Update(float dt) {}
}

class DZS_StatusText : DZS_StatusItem
{
	protected TextWidget m_T;
	protected Widget m_Lamp;
	protected string m_Text;
	protected string m_Role;
	protected int m_FixedWidth;
	protected bool m_RightAlign;
	protected string m_LampTone;

	void DZS_StatusText(string id, string role = "label", int maxWidthRef = 0, int fixedWidthRef = 0, bool rightAlign = false)
	{
		m_Id = id;
		m_Role = role;
		m_MaxWidth = maxWidthRef;
		m_FixedWidth = fixedWidthRef;
		m_RightAlign = rightAlign;
	}

	override void Build(Widget parent)
	{
		m_Frame = DZS_W.Frame(parent);
		m_Frame.ClearFlags(WidgetFlags.IGNOREPOINTER);
		m_Lamp = DZS_W.Panel(m_Frame, DZS_Color.Get("textMuted"));
		m_Lamp.Show(false);
		if (m_RightAlign)
		{
			m_T = DZS_W.Text(m_Frame, m_Role, 11, DZS_Color.Get("textMuted"), false, true);
		}
		else
			m_T = DZS_W.Text(m_Frame, m_Role, 11, DZS_Color.Get("textMuted"));
		Apply();
	}

	void SetText(string text)
	{
		if (text == m_Text)
			return;
		m_Text = text;
		Apply();
	}

	string GetText() { return m_Text; }

	void SetColorName(string name)
	{
		if (m_T)
			m_T.SetColor(DZS_Color.Get(name));
	}

	void SetLamp(string tone)
	{
		m_LampTone = tone;
		Apply();
	}

	protected void Apply()
	{
		if (!m_T)
			return;
		m_T.SetText(m_Text);
		m_Tooltip = "";
		if (m_MaxWidth > 0)
		{
			int max = DZS_Scale.Px(m_MaxWidth);
			if (DZS_W.TextWidth(m_T) > max)
			{
				m_Tooltip = m_Text;
				string s = m_Text;
				while (s.Length() > 1)
				{
					s = s.Substring(0, DZS_Utf8.SafeCut(s, s.Length() - 1));
					m_T.SetText(s + "…");
					if (DZS_W.TextWidth(m_T) <= max)
						break;
				}
			}
		}
		m_Lamp.Show(m_LampTone != "");
		if (m_LampTone != "")
			m_Lamp.SetColor(DZS_Color.Get(m_LampTone));
	}

	protected int LampSpace()
	{
		if (m_LampTone == "")
			return 0;
		return DZS_Scale.Px(14);
	}

	override int GetWidth()
	{
		if (m_FixedWidth > 0)
			return DZS_Scale.Px(m_FixedWidth);
		return DZS_W.TextWidth(m_T) + LampSpace();
	}

	override void Place(float x, float y, float h)
	{
		int w = GetWidth();
		DZS_W.Place(m_Frame, x, y, w, h);
		int lamp = DZS_Scale.Px(8);
		DZS_W.Place(m_Lamp, 0, (h - lamp) / 2, lamp, lamp);
		int tw = DZS_W.TextWidth(m_T);
		if (m_RightAlign)
		{
			DZS_W.Place(m_T, 0, 0, w, h);
			return;
		}
		DZS_W.Place(m_T, LampSpace(), 0, tw + 2, h);
	}
}

// Number field with axis letter. The owner decides what a commit does.
class DZS_NumberField : DZS_StatusItem
{
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected TextWidget m_Letter;
	protected EditBoxWidget m_Edit;
	protected string m_LetterText;
	protected string m_AxisColor;
	protected int m_WidthRef;
	protected bool m_Error;
	protected bool m_Focused;
	protected bool m_Enabled = true;
	protected bool m_OwnerEnabled = true;
	protected bool m_ChangesSelection;
	protected bool m_WaitOff;
	protected ref DZS_Hit m_WaitHit;
	protected string m_Shown;
	ref ScriptInvoker m_OnCommit;
	ref ScriptInvoker m_OnFocus;
	// wheel with focus: owner gets (field, steps, fine)
	ref ScriptInvoker m_OnWheel;
	// drag on the letter: owner gets (field, phase, steps) with phase DRAG_START/MOVE/END/CANCEL and steps since the start
	ref ScriptInvoker m_OnDrag;
	// click on the reset arrow: owner gets (field)
	ref ScriptInvoker m_OnReset;
	protected ref DZS_FieldGrip m_Grip;
	protected ref DZS_Button m_Reset;
	protected bool m_ResetWanted;
	protected string m_LetterTip;
	// free for the owner (which value a shared handler edits)
	string m_Tag;

	static const int DRAG_START = 0;
	static const int DRAG_MOVE = 1;
	static const int DRAG_END = 2;
	static const int DRAG_CANCEL = 3;

	void DZS_NumberField(string id = "", string letter = "", string axisColor = "textMuted", int widthRef = 100)
	{
		m_Id = id;
		m_LetterText = letter;
		m_AxisColor = axisColor;
		m_WidthRef = widthRef;
		m_OnCommit = new ScriptInvoker();
		m_OnFocus = new ScriptInvoker();
		m_OnWheel = new ScriptInvoker();
		m_OnDrag = new ScriptInvoker();
		m_OnReset = new ScriptInvoker();
	}

	// Letter becomes a grip: press and drag sideways, one step per 4 px, a tenth with Shift.
	void EnableDrag(string tip)
	{
		m_LetterTip = tip;
		if (m_Grip || !m_Frame)
			return;
		m_Grip = new DZS_FieldGrip(m_Frame, this);
		m_Grip.m_Hit.SetTip(tip);
	}

	// Ghost knob "arrow back" at the right end; its place stays free while it is hidden.
	void EnableReset(string tip)
	{
		if (m_Reset || !m_Frame)
			return;
		m_Reset = new DZS_Button(m_Frame, "ghost", "", "undo-2", 20);
		m_Reset.m_Hit.m_FocusKind = "";
		m_Reset.SetTip(tip);
		m_Reset.m_OnClick.Insert(OnResetClick);
		m_Reset.Show(false);
	}

	void SetResetTip(string tip)
	{
		if (m_Reset)
			m_Reset.SetTip(tip);
	}

	void ShowReset(bool show)
	{
		m_ResetWanted = show;
		if (m_Reset)
			m_Reset.Show(show && m_Enabled);
	}

	void OnResetClick(DZS_Button b)
	{
		if (m_Enabled)
			m_OnReset.Invoke(this);
	}

	bool IsDragging() { return m_Grip && m_Grip.m_Dragging; }

	// Ends a running drag as with Esc: the owner puts the start value back.
	void CancelDrag()
	{
		if (m_Grip)
			m_Grip.Cancel();
	}

	// The field changes the selected objects: the kernel turns it off while their change has to wait.
	void MarkChangesSelection() { m_ChangesSelection = true; }

	override void Update(float dt)
	{
		if (m_Grip)
			m_Grip.Poll();
		if (!m_ChangesSelection)
			return;
		string why = DZS_ChangeWait.Reason();
		bool off = why != "";
		if (off != m_WaitOff)
		{
			m_WaitOff = off;
			ApplyEnabled();
		}
		if (m_WaitHit)
			m_WaitHit.m_TipReason = why;
	}

	bool IsSearch() { return false; }

	void OnRightClick() {}

	void SetWidthRef(int refW) { m_WidthRef = refW; }

	override void Build(Widget parent)
	{
		m_Frame = DZS_W.Frame(parent);
		m_Bg = DZS_W.Panel(m_Frame, DZS_Color.Get("well"));
		m_Border = new DZS_Border(m_Frame, DZS_Color.Get("line"));
		m_Letter = DZS_W.Text(m_Frame, DZS_Font.LABELBOLD, 11, DZS_Color.Get(m_AxisColor));
		m_Letter.SetText(m_LetterText);
		m_Edit = EditBoxWidget.Cast(DZS_W.FromTexts("edit_mono", m_Frame));
		m_Edit.SetColor(DZS_Color.Get("text"));
		m_Edit.SetHandler(DZS_FieldEvents.Get());
		DZS_FieldEvents.Get().Bind(m_Edit, this);
	}

	EditBoxWidget GetEdit() { return m_Edit; }
	bool IsFocused() { return m_Focused; }
	bool IsEnabled() { return m_Enabled; }

	void SetEnabled(bool on)
	{
		m_OwnerEnabled = on;
		ApplyEnabled();
	}

	protected void ApplyEnabled()
	{
		bool on = m_OwnerEnabled && !m_WaitOff;
		if (m_WaitOff && !m_WaitHit && m_Frame)
		{
			m_WaitHit = new DZS_Hit(m_Frame, null, "field.wait");
			m_WaitHit.m_FocusKind = "";
			m_WaitHit.SetEnabled(false);
			float fw, fh;
			m_Frame.GetSize(fw, fh);
			m_WaitHit.Place(0, 0, fw, fh);
		}
		if (m_WaitHit)
			m_WaitHit.m_W.Show(m_WaitOff);
		m_Enabled = on;
		if (!on && m_Focused)
			DZS_Kernel.Get().GetScreen().Unfocus(false);
		if (!on && IsDragging())
			m_Grip.Cancel();
		if (m_Grip)
			m_Grip.m_Hit.SetEnabled(on);
		if (m_Reset)
			m_Reset.Show(m_ResetWanted && on);
		if (m_Edit)
		{
			if (on)
				m_Edit.SetColor(DZS_Color.Get("text"));
			else
				m_Edit.SetColor(DZS_Color.Get("textQuiet"));
			// SetColor leaves the text of an edit box white in DayZ 1.30
			if (on)
				m_Edit.SetAlpha(1);
			else
				m_Edit.SetAlpha(0.45);
		}
		if (m_Letter)
		{
			if (on)
				m_Letter.SetColor(DZS_Color.Get(m_AxisColor));
			else
				m_Letter.SetColor(DZS_Color.Get("textQuiet"));
		}
	}

	// Tooltip over the whole field (also when it is off, e.g. the reason).
	void SetFieldTip(string tip)
	{
		if (m_Grip)
			m_Grip.m_Hit.SetTip(tip);
	}

	override int GetWidth() { return DZS_Scale.Px(m_WidthRef); }

	override void Place(float x, float y, float h)
	{
		int w = GetWidth();
		int fh = DZS_Scale.Px(22);
		float fy = (h - fh) / 2;
		DZS_W.Place(m_Frame, x, y + fy, w, fh);
		DZS_W.Place(m_Bg, 0, 0, w, fh);
		UpdateBorder();
		int lw = DZS_Scale.Px(16);
		DZS_W.Place(m_Letter, DZS_Scale.Px(6), 0, lw, fh);
		int eh = DZS_Font.WidgetHeight(DZS_Font.MONO, 12);
		int rw = 0;
		if (m_Reset)
		{
			rw = fh;
			m_Reset.SetFixedWidth(22);
			m_Reset.Place(w - rw, 0);
		}
		DZS_W.Place(m_Edit, DZS_Scale.Px(4) + lw, (fh - eh) / 2, w - lw - DZS_Scale.Px(8) - rw, eh);
		if (m_Grip)
		{
			float gw = DZS_Scale.Px(4) + lw;
			if (!m_Enabled)
				gw = w;
			m_Grip.m_Hit.Place(0, 0, gw, fh);
		}
		if (m_WaitHit)
			m_WaitHit.Place(0, 0, w, fh);
	}

	protected void UpdateBorder()
	{
		float w, h;
		m_Frame.GetSize(w, h);
		if (m_Error)
		{
			m_Border.SetColor(DZS_Color.Get("danger"));
			m_Border.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		}
		else if (m_Focused)
		{
			m_Border.SetColor(DZS_Color.Get("accent"));
			m_Border.Place(0, 0, w, h, Math.Max(1, DZS_Scale.Px(2)));
		}
		else
		{
			m_Border.SetColor(DZS_Color.Get("line"));
			m_Border.Place(0, 0, w, h, 1);
		}
	}

	// Shows a value unless the user is typing in the field.
	void ShowValue(string text)
	{
		if (m_Focused)
			return;
		if (text == m_Shown)
			return;
		m_Shown = text;
		m_Edit.SetText(text);
	}

	string GetTyped() { return m_Edit.GetText(); }

	// Sets the text even while the field has the focus (wheel steps in a focused field).
	void ForceValue(string text)
	{
		m_Shown = text;
		m_Edit.SetText(text);
	}

	void Destroy()
	{
		if (m_Frame)
			m_Frame.Unlink();
		m_Frame = null;
	}

	void SetError(bool err)
	{
		m_Error = err;
		UpdateBorder();
	}

	// The game's edit box cannot select its text; an emptied field stands in for "everything selected":
	// typing replaces the value, leaving it untouched brings the value back.
	void OnFocusChanged(bool focused)
	{
		m_Focused = focused;
		if (focused)
			m_Edit.SetText("");
		if (!focused)
		{
			m_Error = false;
			m_Shown = "";
		}
		UpdateBorder();
	}
}

// The letter of a number field as a drag grip. The game does not let a
// script set or reshape the pointer, so a double arrow is drawn beside it while it is over the letter or dragging.
class DZS_FieldGrip : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	DZS_NumberField m_Field;
	bool m_Dragging;
	protected float m_LastX;
	protected float m_Steps;
	protected static ImageWidget s_Arrow;
	protected static DZS_FieldGrip s_DragOwner;

	void DZS_FieldGrip(Widget parent, DZS_NumberField field)
	{
		m_Field = field;
		m_Hit = new DZS_Hit(parent, this, "fieldGrip");
		m_Hit.m_FocusKind = "";
	}

	override void OnHitPress(DZS_Hit h, int button)
	{
		if (button != MouseState.LEFT || !m_Field.IsEnabled())
			return;
		int mx, my;
		GetMousePos(mx, my);
		m_LastX = mx;
		m_Steps = 0;
		m_Dragging = true;
		s_DragOwner = this;
		DZS_Kernel.Get().SetGesture("fieldDrag", true);
		m_Field.m_OnDrag.Invoke(m_Field, DZS_NumberField.DRAG_START, 0.0);
	}

	void Cancel()
	{
		if (!m_Dragging)
			return;
		m_Dragging = false;
		s_DragOwner = null;
		DZS_Kernel.Get().SetGesture("fieldDrag", false);
		m_Field.m_OnDrag.Invoke(m_Field, DZS_NumberField.DRAG_CANCEL, m_Steps);
		ShowArrow(false, 0, 0);
	}

	void Poll()
	{
		int mx, my;
		GetMousePos(mx, my);
		if (!m_Dragging)
		{
			if (m_Hit.m_Hover && m_Hit.m_Enabled)
				ShowArrow(true, mx, my);
			else if (s_Arrow && s_Arrow.IsVisible() && !AnyHover() && !s_DragOwner)
				ShowArrow(false, 0, 0);
			return;
		}
		ShowArrow(true, mx, my);
		bool held = (GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) != 0;
		float dx = mx - m_LastX;
		m_LastX = mx;
		if (dx != 0)
		{
			float per = DZS_Scale.Px(4);
			float fine = 1;
			if (DZS_UiRegistry.ModDown("Shift"))
				fine = 0.1;
			m_Steps += dx / per * fine;
			m_Field.m_OnDrag.Invoke(m_Field, DZS_NumberField.DRAG_MOVE, m_Steps);
		}
		if (!held)
		{
			m_Dragging = false;
			s_DragOwner = null;
			DZS_Kernel.Get().SetGesture("fieldDrag", false);
			m_Field.m_OnDrag.Invoke(m_Field, DZS_NumberField.DRAG_END, m_Steps);
			ShowArrow(m_Hit.m_Hover, mx, my);
		}
	}

	protected bool AnyHover()
	{
		Widget under = GetWidgetUnderCursor();
		DZS_Hit h = DZS_HitRegistry.Find(under);
		return h && h.m_Tag == "fieldGrip";
	}

	protected static void ShowArrow(bool show, float x, float y)
	{
		if (!s_Arrow)
		{
			if (!show)
				return;
			s_Arrow = DZS_W.Icon(DZS_Kernel.Get().GetScreen().GetTipLayer(), "move-horizontal", DZS_Color.Get("text"));
		}
		s_Arrow.Show(show);
		if (show)
		{
			float sz = DZS_Scale.Px(20);
			DZS_W.Place(s_Arrow, x + DZS_Scale.Px(10), y + DZS_Scale.Px(8), sz, sz);
		}
	}
}

// Routes widget events of edit boxes to their number fields.
class DZS_FieldEvents : ScriptedWidgetEventHandler
{
	protected static ref DZS_FieldEvents s_Instance;
	protected ref map<Widget, DZS_NumberField> m_Fields;

	static DZS_FieldEvents Get()
	{
		if (!s_Instance)
		{
			s_Instance = new DZS_FieldEvents();
			s_Instance.m_Fields = new map<Widget, DZS_NumberField>();
		}
		return s_Instance;
	}

	void Bind(Widget w, DZS_NumberField f)
	{
		m_Fields.Set(w, f);
	}

	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		DZS_NumberField f = m_Fields.Get(w);
		if (f && button == MouseState.RIGHT && f.IsEnabled())
		{
			f.OnRightClick();
			return true;
		}
		if (f && button == MouseState.LEFT && f.IsEnabled())
		{
			DZS_Kernel.Get().GetScreen().FocusField(f);
			return false;
		}
		return f != null;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		DZS_NumberField f = m_Fields.Get(w);
		if (f && !f.IsEnabled())
			return true;
		return false;
	}

	override bool OnChange(Widget w, int x, int y, bool finished)
	{
		DZS_NumberField f = m_Fields.Get(w);
		if (f && finished && f.IsFocused())
			DZS_Kernel.Get().GetScreen().CommitField(f, "enter");
		return false;
	}
}

// The cut corner: top-left and bottom-right cut at 45°, 10 px; the diagonal is a small image.
// The fill is made of pieces so that what lies behind the box shows through the cut.
class DZS_CutBox : Managed
{
	protected Widget m_Parent;
	protected int m_FillColor;
	protected bool m_FillShown = true;
	protected Widget m_Top;
	protected Widget m_Mid;
	protected Widget m_Bot;
	// the corner fills are rows of one pixel each (images of the triangle showed their opposite edge at the cut)
	static const int STAIRS = 48;
	protected int m_N;
	protected ref array<Widget> m_StairTL = new array<Widget>();
	protected ref array<Widget> m_StairBR = new array<Widget>();
	protected Widget m_LTop;
	protected Widget m_LLeft;
	protected Widget m_LBottom;
	protected Widget m_LRight;
	protected ImageWidget m_DiagTL;
	protected ImageWidget m_DiagBR;

	void DZS_CutBox(Widget parent, int fill, int line)
	{
		m_Parent = parent;
		m_FillColor = fill;
		m_Top = DZS_W.Panel(parent, fill);
		m_Mid = DZS_W.Panel(parent, fill);
		m_Bot = DZS_W.Panel(parent, fill);
		Stairs(m_StairTL, STAIRS);
		Stairs(m_StairBR, STAIRS);
		for (int i = 0; i < STAIRS; i++)
		{
			m_StairTL[i].Show(false);
			m_StairBR[i].Show(false);
		}
		m_LTop = DZS_W.Panel(parent, line);
		m_LLeft = DZS_W.Panel(parent, line);
		m_LBottom = DZS_W.Panel(parent, line);
		m_LRight = DZS_W.Panel(parent, line);
		m_DiagTL = DZS_W.Icon(parent, "cut-line", line);
		m_DiagBR = DZS_W.Icon(parent, "cut-line", line);
	}

	protected void Stairs(array<Widget> rows, int n)
	{
		while (rows.Count() < n)
		{
			Widget w = DZS_W.Panel(m_Parent, m_FillColor);
			rows.Insert(w);
		}
	}

	void Place(float w, float h)
	{
		float c = DZS_Scale.Px(10);
		int n = Math.Round(c);
		if (n > STAIRS)
			n = STAIRS;
		m_N = n;
		DZS_W.Place(m_Top, c, 0, w - c, c);
		DZS_W.Place(m_Mid, 0, c, w, h - 2 * c);
		DZS_W.Place(m_Bot, 0, h - c, w - c, c);
		for (int k = n; k < STAIRS; k++)
		{
			m_StairTL[k].Show(false);
			m_StairBR[k].Show(false);
		}
		for (int i = 0; i < n; i++)
		{
			// row i of the top-left cell is filled right of the diagonal, from c - i - 0.5 on; bottom-right mirrored
			float from = Math.Round(c - i - 0.5);
			DZS_W.Place(m_StairTL[i], from, i, c - from, 1);
			m_StairTL[i].Show(m_FillShown);
			float to = Math.Round(i + 0.5);
			DZS_W.Place(m_StairBR[i], w - c, h - 1 - i, to, 1);
			m_StairBR[i].Show(m_FillShown);
		}
		DZS_W.Place(m_LTop, c, 0, w - c, 1);
		DZS_W.Place(m_LLeft, 0, c, 1, h - c);
		DZS_W.Place(m_LBottom, 0, h - 1, w - c, 1);
		DZS_W.Place(m_LRight, w - 1, 0, 1, h - c);
		DZS_W.Place(m_DiagTL, 0, 0, c, c);
		DZS_W.Place(m_DiagBR, w - c, h - c, c, c);
	}

	void SetLine(int line)
	{
		m_LTop.SetColor(line);
		m_LLeft.SetColor(line);
		m_LBottom.SetColor(line);
		m_LRight.SetColor(line);
		m_DiagTL.SetColor(line);
		m_DiagBR.SetColor(line);
	}

	void SetFill(int fill)
	{
		m_FillColor = fill;
		m_Top.SetColor(fill);
		m_Mid.SetColor(fill);
		m_Bot.SetColor(fill);
		foreach (Widget a : m_StairTL)
			a.SetColor(fill);
		foreach (Widget b : m_StairBR)
			b.SetColor(fill);
	}

	void ShowFill(bool show)
	{
		m_FillShown = show;
		m_Top.Show(show);
		m_Mid.Show(show);
		m_Bot.Show(show);
		for (int i = 0; i < STAIRS; i++)
		{
			m_StairTL[i].Show(show && i < m_N);
			m_StairBR[i].Show(show && i < m_N);
		}
	}

	void ShowLines(bool show)
	{
		m_LTop.Show(show);
		m_LLeft.Show(show);
		m_LBottom.Show(show);
		m_LRight.Show(show);
		m_DiagTL.Show(show);
		m_DiagBR.Show(show);
	}
}

// The cut corner over a knob that sits on a plain surface: corner images in the surface colour cover the
// two corners, a diagonal line in the border colour closes the edge.
class DZS_CutCorners : Managed
{
	protected ImageWidget m_CoverTL;
	protected ImageWidget m_CoverBR;
	protected ImageWidget m_DiagTL;
	protected ImageWidget m_DiagBR;

	void DZS_CutCorners(Widget parent, int surface, int line)
	{
		m_CoverTL = DZS_W.Icon(parent, "cut-br", surface);
		m_CoverBR = DZS_W.Icon(parent, "cut-tl", surface);
		m_DiagTL = DZS_W.Icon(parent, "cut-line", line);
		m_DiagBR = DZS_W.Icon(parent, "cut-line", line);
	}

	void Place(float w, float h)
	{
		float c = DZS_Scale.Px(10);
		DZS_W.Place(m_CoverTL, 0, 0, c, c);
		DZS_W.Place(m_CoverBR, w - c, h - c, c, c);
		DZS_W.Place(m_DiagTL, 0, 0, c, c);
		DZS_W.Place(m_DiagBR, w - c, h - c, c, c);
	}

	void SetLine(int line, bool show)
	{
		m_DiagTL.SetColor(line);
		m_DiagBR.SetColor(line);
		m_DiagTL.Show(show);
		m_DiagBR.Show(show);
	}
}

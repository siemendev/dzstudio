// Dialogs: centred over everything, the rest dimmed; head with "// kicker", title and
// close knob; content that scrolls; foot with knobs, the main one rightmost.
class DZS_Focusable : Managed
{
	DZS_Hit m_Hit;
	DZS_NumberField m_Field;
}

class DZS_Dialog : DZS_HitListener
{
	static const string SMALL = "small";
	static const string MEDIUM = "medium";
	static const string LARGE = "large";

	protected string m_Size;
	protected bool m_Question;
	Widget m_Root;
	protected ref DZS_Hit m_Dim;
	protected Widget m_DimPanel;
	ref DZS_Hit m_Box;
	protected ref DZS_CutBox m_BoxShape;
	protected TextWidget m_Kicker;
	protected TextWidget m_Title;
	protected ref DZS_Button m_CloseX;
	protected Widget m_HeadLine;
	protected Widget m_FootLine;
	protected Widget m_Clip;
	Widget m_Inner;
	protected Widget m_ScrollBar;
	protected float m_Scroll;
	protected float m_ContentH;
	protected float m_ViewH;
	protected ref array<ref DZS_Button> m_Foot;
	protected ref array<string> m_FootTags;
	protected ref array<ref DZS_Focusable> m_Focus;
	protected int m_FocusIdx = -1;
	bool m_Closed;
	string m_Tag;
	protected float m_ExtraHeadH;
	protected TextWidget m_FootNote;
	ref ScriptInvoker m_OnResult;

	// Subclasses call Setup in their constructor (base constructors run without arguments).
	void Setup(string size, string kicker, string title, bool question)
	{
		m_Size = size;
		m_Question = question;
		m_Foot = new array<ref DZS_Button>();
		m_FootTags = new array<string>();
		m_Focus = new array<ref DZS_Focusable>();
		m_OnResult = new ScriptInvoker();
		Widget layer = DZS_Kernel.Get().GetScreen().GetDialogLayer();
		m_Root = DZS_W.Frame(layer);
		m_Dim = new DZS_Hit(m_Root, this, "dim");
		m_Dim.m_Surface = true;
		m_DimPanel = DZS_W.Panel(m_Dim.m_W, DZS_Color.Get("dim"));
		m_Box = new DZS_Hit(m_Root, this, "dialog");
		m_Box.m_Surface = true;
		m_BoxShape = new DZS_CutBox(m_Box.m_W, DZS_Color.Get("surface"), DZS_Color.Get("lineStrong"));
		m_Kicker = DZS_W.Text(m_Box.m_W, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Kicker.SetText(DZS_Texts.Upper("// " + kicker));
		m_Title = DZS_W.Text(m_Box.m_W, DZS_Font.DISPLAY, 26, DZS_Color.Get("text"));
		m_Title.SetText(title);
		m_CloseX = new DZS_Button(m_Box.m_W, "ghost", "", "x", 28);
		m_CloseX.m_Hit.m_FocusKind = "";
		m_CloseX.SetTip(DZS_Texts.Get("core.dialog.close"));
		m_CloseX.m_OnClick.Insert(OnCloseX);
		m_HeadLine = DZS_W.Panel(m_Box.m_W, DZS_Color.Get("line"));
		m_FootLine = DZS_W.Panel(m_Box.m_W, DZS_Color.Get("line"));
		m_Clip = DZS_W.HitFrame(m_Box.m_W);
		m_Clip.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Inner = DZS_W.Frame(m_Clip);
		m_ScrollBar = DZS_W.Panel(m_Box.m_W, DZS_Color.Get("lineStrong"));
	}

	void SetTitle(string t) { m_Title.SetText(t); }

	// Quiet text on the left of the foot, not clickable.
	void SetFootNote(string text)
	{
		if (!m_FootNote)
			m_FootNote = DZS_W.Text(m_Box.m_W, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_FootNote.SetText(DZS_W.Literal(text));
	}

	// Footer knobs in the order given; put the main one last.
	DZS_Button AddFootButton(string tag, string label, string kind)
	{
		DZS_Button b = new DZS_Button(m_Box.m_W, kind, label);
		b.m_Tag = tag;
		b.m_OnClick.Insert(OnFootClick);
		m_Foot.Insert(b);
		m_FootTags.Insert(tag);
		return b;
	}

	// Footer knob on the left edge (e.g. a ghost knob next to the decision knobs).
	DZS_Button AddFootButtonLeft(string tag, string label, string kind)
	{
		DZS_Button b = AddFootButton(tag, label, kind);
		b.m_FootLeft = true;
		return b;
	}

	DZS_Button FootButton(string tag)
	{
		int i = m_FootTags.Find(tag);
		if (i < 0)
			return null;
		return m_Foot[i];
	}

	void OnFootClick(DZS_Button b)
	{
		OnFoot(b.m_Tag);
	}

	void OnCloseX(DZS_Button b)
	{
		Cancel();
	}

	// ---- for subclasses ----

	void BuildContent(Widget inner) {}
	float LayoutContent(float width) { return 0; }
	void UpdateContent(float dt) {}
	void OnFoot(string tag)
	{
		m_OnResult.Invoke(this, tag);
		Close();
	}
	void OnClosed() {}
	// Esc and the close knob: "Abbrechen" for questions, "Schließen" for tool dialogs.
	void Cancel()
	{
		m_OnResult.Invoke(this, "cancel");
		Close();
	}

	// Elements reachable with Tab, in order: fields, switches, knobs, list entries.
	void ClearFocusables()
	{
		ClearFocusMark();
		m_Focus.Clear();
		m_FocusIdx = -1;
	}

	void AddFocusHit(DZS_Hit h)
	{
		DZS_Focusable f = new DZS_Focusable();
		f.m_Hit = h;
		m_Focus.Insert(f);
	}

	void AddFocusField(DZS_NumberField field)
	{
		DZS_Focusable f = new DZS_Focusable();
		f.m_Field = field;
		m_Focus.Insert(f);
	}

	// The footer knobs come last in the Tab order.
	void AddFootFocus()
	{
		foreach (DZS_Button b : m_Foot)
			AddFocusHit(b.m_Hit);
	}

	// Opened by a key with Enter (Alt+Enter): the same press neither commits a field nor the dialog.
	protected bool m_WaitEnterUp;

	void Open()
	{
		BuildContent(m_Inner);
		DZS_Kernel.Get().GetScreen().PushDialog(this);
		Layout();
		m_WaitEnterUp = DZS_UiRegistry.KeyDown("RETURN") || DZS_UiRegistry.KeyDown("NUMPADENTER");
		if (!m_WaitEnterUp)
			InitialFocus();
	}

	bool WaitsForEnterUp() { return m_WaitEnterUp; }

	void Close()
	{
		if (m_Closed)
			return;
		m_Closed = true;
		DZS_Screen scr = DZS_Kernel.Get().GetScreen();
		if (FocusedField())
			scr.Unfocus(false);
		scr.PopDialog(this);
		OnClosed();
		DZS_HitRegistry.Remove(m_Dim);
		m_Root.Unlink();
	}

	// ---- focus ----

	protected DZS_NumberField FocusedField()
	{
		if (m_FocusIdx < 0 || m_FocusIdx >= m_Focus.Count())
			return null;
		return m_Focus[m_FocusIdx].m_Field;
	}

	DZS_Hit FocusedHit()
	{
		if (m_FocusIdx < 0 || m_FocusIdx >= m_Focus.Count())
			return null;
		return m_Focus[m_FocusIdx].m_Hit;
	}

	// First field; else the main knob if on; else the first list entry; else "Abbrechen"/"Schließen".
	void InitialFocus()
	{
		for (int i = 0; i < m_Focus.Count(); i++)
		{
			if (m_Focus[i].m_Field)
			{
				SetFocusIndex(i);
				return;
			}
		}
		for (int j = 0; j < m_Focus.Count(); j++)
		{
			DZS_Hit h = m_Focus[j].m_Hit;
			if (h && h.m_Primary && h.m_Enabled)
			{
				SetFocusIndex(j);
				return;
			}
		}
		for (int k = 0; k < m_Focus.Count(); k++)
		{
			DZS_Hit lh = m_Focus[k].m_Hit;
			if (lh && lh.m_FocusKind == "list")
			{
				SetFocusIndex(k);
				return;
			}
		}
		for (int c = 0; c < m_Foot.Count(); c++)
		{
			if (m_FootTags[c] == "cancel" || m_FootTags[c] == "close")
			{
				SetFocusToHit(m_Foot[c].m_Hit);
				return;
			}
		}
		if (m_Focus.Count() > 0)
			SetFocusIndex(0);
	}

	void SetFocusToHit(DZS_Hit h)
	{
		for (int i = 0; i < m_Focus.Count(); i++)
		{
			if (m_Focus[i].m_Hit == h)
			{
				SetFocusIndex(i);
				return;
			}
		}
	}

	void SetFocusToField(DZS_NumberField f)
	{
		for (int i = 0; i < m_Focus.Count(); i++)
		{
			if (m_Focus[i].m_Field == f)
			{
				m_FocusIdx = i;
				return;
			}
		}
	}

	protected void ClearFocusMark()
	{
		DZS_Hit old = FocusedHit();
		if (old && !old.m_Destroyed)
		{
			old.m_Focus = false;
			old.Changed();
		}
	}

	void SetFocusIndex(int i)
	{
		DZS_Screen scr = DZS_Kernel.Get().GetScreen();
		ClearFocusMark();
		DZS_NumberField was = FocusedField();
		m_FocusIdx = i;
		if (i < 0 || i >= m_Focus.Count())
			return;
		DZS_Focusable f = m_Focus[i];
		string kind = "field";
		if (f.m_Hit)
			kind = f.m_Hit.m_FocusKind + ":" + f.m_Hit.m_Tag;
		DZS_Log.Info("dialog focus " + i + "/" + m_Focus.Count() + " " + kind);
		if (f.m_Field)
		{
			scr.FocusField(f.m_Field);
			return;
		}
		if (scr.IsFieldFocused())
			scr.Unfocus(true);
		f.m_Hit.m_Focus = true;
		f.m_Hit.Changed();
		ScrollIntoView(f.m_Hit.m_W);
	}

	protected void ScrollIntoView(Widget w)
	{
		if (!w || !DZS_W.IsInside(w, m_Inner))
			return;
		float cx, cy, ix, iy, ww, wh;
		m_Clip.GetScreenPos(cx, cy);
		w.GetScreenPos(ix, iy);
		w.GetScreenSize(ww, wh);
		float top = iy - cy;
		if (top < 0)
			m_Scroll += top - DZS_Scale.Px(8);
		else if (top + wh > m_ViewH)
			m_Scroll += top + wh - m_ViewH + DZS_Scale.Px(8);
	}

	// Tab and Shift+Tab go round all elements of the dialog and never behind it.
	void FocusStep(int dir)
	{
		int n = m_Focus.Count();
		if (n == 0)
			return;
		int i = m_FocusIdx;
		if (i < 0)
		{
			if (dir > 0)
				i = -1;
			else
				i = 0;
		}
		for (int k = 0; k < n; k++)
		{
			i += dir;
			if (i >= n)
				i = 0;
			if (i < 0)
				i = n - 1;
			DZS_Focusable f = m_Focus[i];
			if (f.m_Field || (f.m_Hit && f.m_Hit.m_Enabled && f.m_Hit.m_W.IsVisible()))
				break;
		}
		SetFocusIndex(i);
	}

	// Down/up between list entries without wrapping; from a knob or switch down goes to the first entry.
	void FocusArrow(int dir)
	{
		DZS_Hit h = FocusedHit();
		if (!h)
			return;
		array<int> list = new array<int>();
		for (int i = 0; i < m_Focus.Count(); i++)
		{
			DZS_Hit lh = m_Focus[i].m_Hit;
			if (lh && lh.m_FocusKind == "list" && lh.m_W.IsVisible())
				list.Insert(i);
		}
		if (list.Count() == 0)
			return;
		int at = list.Find(m_FocusIdx);
		if (at < 0)
		{
			if (dir > 0)
				SetFocusIndex(list[0]);
			return;
		}
		int next = at + dir;
		if (next < 0 || next >= list.Count())
			return;
		SetFocusIndex(list[next]);
	}

	// Space: knob pressed, switch flipped, list entry carried out.
	void ActivateFocus()
	{
		DZS_Hit h = FocusedHit();
		if (h && h.m_Enabled && h.m_Listener)
			h.m_Listener.OnHitActivate(h);
	}

	// Enter: knob → that knob; field → take the value and the main knob; otherwise nothing.
	void EnterKey()
	{
		if (m_WaitEnterUp)
			return;
		DZS_Hit h = FocusedHit();
		if (h)
		{
			if (h.m_FocusKind == "button" && h.m_Enabled && h.m_Listener)
				h.m_Listener.OnHitActivate(h);
			return;
		}
		DZS_NumberField f = FocusedField();
		if (f)
		{
			DZS_Screen scr = DZS_Kernel.Get().GetScreen();
			scr.CommitField(f, "enter");
			if (scr.IsFieldFocused())
				return;
			TriggerMain();
		}
	}

	protected void TriggerMain()
	{
		foreach (DZS_Button b : m_Foot)
		{
			if (b.m_Hit.m_Primary && b.m_Hit.m_Enabled)
			{
				b.OnHitActivate(b.m_Hit);
				return;
			}
		}
	}

	// A field took the focus by a click: the dialog follows.
	void OnFieldFocused(DZS_NumberField f)
	{
		ClearFocusMark();
		SetFocusToField(f);
	}

	// ---- layout ----

	float BoxWidth()
	{
		if (m_Size == SMALL)
			return DZS_Scale.Px(420);
		if (m_Size == MEDIUM)
			return DZS_Scale.Px(640);
		return DZS_Scale.Px(960);
	}

	float HeadHeight() { return DZS_Scale.Px(72) + m_ExtraHeadH; }

	void Layout()
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Root, 0, 0, sw, sh);
		m_Dim.Place(0, 0, sw, sh);
		DZS_W.Place(m_DimPanel, 0, 0, sw, sh);
		float w = BoxWidth();
		float pad = DZS_Scale.Px(24);
		float footW = 0;
		foreach (DZS_Button fb : m_Foot)
		{
			if (fb.GetWidget().IsVisible())
				footW += fb.GetWidth() + DZS_Scale.Px(8);
		}
		w = Math.Max(w, footW - DZS_Scale.Px(8) + 2 * pad);
		float headH = HeadHeight();
		float footH = DZS_Scale.Px(60);
		if (m_Foot.Count() == 0)
			footH = 0;
		m_ContentH = LayoutContent(w - 2 * pad);
		float maxH = sh * 0.9;
		float viewH = Math.Min(m_ContentH, maxH - headH - footH - pad * 2);
		m_ViewH = viewH;
		float h = headH + pad + viewH + pad + footH;
		float x = Math.Round((sw - w) / 2);
		float y = Math.Round((sh - h) / 2);
		x = Math.Round(x);
		y = Math.Round(y);
		w = Math.Round(w);
		h = Math.Round(h);
		m_Box.Place(x, y, w, h);
		m_BoxShape.Place(w, h);
		DZS_W.Place(m_Kicker, pad, DZS_Scale.Px(14), w - 2 * pad, DZS_Scale.Px(14));
		DZS_W.Place(m_Title, pad, DZS_Scale.Px(30), w - 2 * pad - DZS_Scale.Px(40), DZS_Scale.Px(34));
		m_CloseX.Place(w - pad - DZS_Scale.Px(28) + DZS_Scale.Px(8), DZS_Scale.Px(16));
		DZS_W.Place(m_HeadLine, 0, headH - 1, w, 1);
		LayoutHeadExtra(pad, DZS_Scale.Px(72), w - 2 * pad);
		float maxScroll = Math.Max(0, m_ContentH - viewH);
		m_Scroll = Math.Clamp(m_Scroll, 0, maxScroll);
		DZS_W.Place(m_Clip, pad, headH + pad, w - 2 * pad, viewH);
		DZS_W.Place(m_Inner, 0, -m_Scroll, w - 2 * pad, Math.Max(m_ContentH, viewH));
		m_ScrollBar.Show(maxScroll > 0);
		m_FootLine.Show(footH > 0);
		if (footH > 0)
		{
			float fy = h - footH;
			DZS_W.Place(m_FootLine, 0, fy, w, 1);
			float lx = pad;
			foreach (DZS_Button lb : m_Foot)
			{
				if (!lb.m_FootLeft || !lb.GetWidget().IsVisible())
					continue;
				lb.Place(lx, fy + (footH - lb.GetHeight()) / 2);
				lx += lb.GetWidth() + DZS_Scale.Px(8);
			}
			if (m_FootNote)
				DZS_W.Place(m_FootNote, lx, fy + (footH - DZS_Scale.Px(20)) / 2, DZS_Scale.Px(200), DZS_Scale.Px(20));
			float bx = w - pad;
			for (int i = m_Foot.Count() - 1; i >= 0; i--)
			{
				DZS_Button b = m_Foot[i];
				if (b.m_FootLeft || !b.GetWidget().IsVisible())
					continue;
				bx -= b.GetWidth();
				b.Place(bx, fy + (footH - b.GetHeight()) / 2);
				bx -= DZS_Scale.Px(8);
			}
		}
	}

	// Room below the title for tabs (settings).
	void LayoutHeadExtra(float x, float y, float w) {}

	bool Wheel(int delta)
	{
		if (m_ContentH > m_ViewH)
			m_Scroll -= delta * DZS_Scale.Px(36) * 3;
		return true;
	}

	void Update(float dt)
	{
		if (m_WaitEnterUp && !DZS_UiRegistry.KeyDown("RETURN") && !DZS_UiRegistry.KeyDown("NUMPADENTER"))
		{
			m_WaitEnterUp = false;
			InitialFocus();
		}
		UpdateContent(dt);
		if (m_Closed)
			return;
		Layout();
	}

	bool IsQuestion() { return m_Question; }
}

// "Bestätigung": small question with knobs (Gefahr for what discards something for good).
class DZS_ConfirmDialog : DZS_Dialog
{
	protected string m_Text;
	protected TextWidget m_Body;
	protected ref array<TextWidget> m_Lines = new array<TextWidget>();

	void DZS_ConfirmDialog(string kicker, string title, string text)
	{
		m_Text = text;
		Setup(SMALL, kicker, title, true);
	}

	// Lines no wider than the widest box; words are measured in the dialog's own font.
	override void BuildContent(Widget inner)
	{
		m_Body = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_Body.SetText(m_Text);
		float room = DZS_Scale.Px(640) - DZS_Scale.Px(56);
		if (DZS_W.TextWidth(m_Body) > room)
		{
			array<string> words = new array<string>();
			m_Text.Split(" ", words);
			string line = "";
			foreach (string w : words)
			{
				string tryLine = w;
				if (line != "")
					tryLine = line + " " + w;
				m_Body.SetText(tryLine);
				if (line != "" && DZS_W.TextWidth(m_Body) > room)
				{
					AddLine(inner, line);
					line = w;
				}
				else
					line = tryLine;
			}
			if (line != "")
				AddLine(inner, line);
			m_Body.SetText("");
			m_Body.Show(false);
		}
		else
			m_Lines.Insert(m_Body);
		AddFootFocus();
	}

	protected void AddLine(Widget inner, string text)
	{
		TextWidget t = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
		t.SetText(text);
		m_Lines.Insert(t);
	}

	override float BoxWidth()
	{
		float w = DZS_Scale.Px(420);
		foreach (TextWidget t : m_Lines)
			w = Math.Max(w, DZS_W.TextWidth(t) + DZS_Scale.Px(56));
		w = Math.Max(w, DZS_W.TextWidth(m_Title) + DZS_Scale.Px(96));
		return Math.Min(w, DZS_Scale.Px(720));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		foreach (TextWidget t : m_Lines)
		{
			DZS_W.Place(t, 0, y, width, DZS_Scale.Px(24));
			y += DZS_Scale.Px(22);
		}
		return Math.Max(y + DZS_Scale.Px(2), DZS_Scale.Px(24));
	}
}

// Work that keeps the editor busy for a while: block indicator and „‹verb› … n %“; no knobs,
// Esc and clicks do nothing until the owner closes it.
class DZS_ProgressDialog : DZS_Dialog
{
	protected string m_Verb;
	protected float m_Share;
	protected float m_T;
	protected TextWidget m_Line;
	protected ref array<Widget> m_Blocks = new array<Widget>();

	void DZS_ProgressDialog(string kicker, string title, string verb)
	{
		m_Verb = verb;
		Setup(SMALL, kicker, DZS_W.Literal(title), false);
	}

	void SetVerb(string verb)
	{
		m_Verb = verb;
	}

	void SetShare(float share)
	{
		m_Share = Math.Clamp(share, 0, 1);
	}

	override void Cancel() {}

	override void BuildContent(Widget inner)
	{
		for (int i = 0; i < 4; i++)
			m_Blocks.Insert(DZS_W.Panel(inner, DZS_Color.Get("blockEmpty")));
		m_Line = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_CloseX.m_Hit.m_W.Show(false);
		Refresh();
	}

	override float LayoutContent(float width)
	{
		float bs = DZS_Scale.Px(10);
		for (int i = 0; i < m_Blocks.Count(); i++)
			DZS_W.Place(m_Blocks[i], i * (bs + DZS_Scale.Px(4)), DZS_Scale.Px(7), bs, bs);
		DZS_W.Place(m_Line, 4 * (bs + DZS_Scale.Px(4)) + DZS_Scale.Px(8), 0, width - DZS_Scale.Px(72), DZS_Scale.Px(24));
		return DZS_Scale.Px(28);
	}

	override void UpdateContent(float dt)
	{
		m_T += dt;
		m_CloseX.m_Hit.m_W.Show(false);
		Refresh();
	}

	protected void Refresh()
	{
		int lit = Math.Floor(m_T / 0.2);
		lit = lit % 4;
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			if (i == lit)
				m_Blocks[i].SetColor(DZS_Color.Get("accent"));
			else
				m_Blocks[i].SetColor(DZS_Color.Get("blockEmpty"));
		}
		int pct = Math.Floor(m_Share * 100);
		m_Line.SetText(m_Verb + " … " + pct + " %");
	}
}

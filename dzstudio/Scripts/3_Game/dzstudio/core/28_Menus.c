// Menus of the menu bar and the context menu: same look, entries with
// optional symbol, check mark, text and key caps right; off entries are greyed with the reason as tooltip.
class DZS_MenuItem : Managed
{
	string m_ActionId;
	string m_Text;
	string m_Icon;
	string m_Binding;
	bool m_Check;
	bool m_Checked;
	bool m_Enabled = true;
	string m_Reason;
	string m_Tip;
	bool m_SepBefore;
	string m_Tag;
	// the key does not work right now: its place stays empty so the entries do not move
	bool m_CapsHidden;
	// group heading of a filterable list: shown only while an entry below it matches
	bool m_Heading;
}

class DZS_MenuRow : DZS_HitListener
{
	DZS_Popup m_Popup;
	DZS_MenuItem m_Item;
	ref DZS_Hit m_Hit;
	Widget m_Bg;
	TextWidget m_Text;
	ImageWidget m_CheckMark;
	ImageWidget m_Icon;
	ref DZS_RichLine m_Caps;
	int m_Index;

	void DZS_MenuRow(DZS_Popup popup, Widget parent, DZS_MenuItem item, int index)
	{
		m_Popup = popup;
		m_Item = item;
		m_Index = index;
		m_Hit = new DZS_Hit(parent, this, "menuRow");
		m_Hit.m_Enabled = item.m_Enabled;
		m_Hit.m_TipReason = item.m_Reason;
		string tip = item.m_Tip;
		if (tip == "")
			tip = item.m_Text;
		m_Hit.SetTip(tip, item.m_Binding);
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("surfaceRaised"));
		m_CheckMark = DZS_W.Icon(m_Hit.m_W, "check", DZS_Color.Get("accentText"));
		if (item.m_Icon != "")
			m_Icon = DZS_W.Icon(m_Hit.m_W, item.m_Icon, DZS_Color.Get("textMuted"));
		m_Text = DZS_W.Text(m_Hit.m_W, DZS_Font.MENU, 14, DZS_Color.Get("text"));
		m_Text.SetText(DZS_Texts.Upper(item.m_Text));
		m_Caps = new DZS_RichLine(m_Hit.m_W, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"));
		if (item.m_Binding != "")
			m_Caps.Set("[" + item.m_Binding + "]");
		m_Caps.Show(!item.m_CapsHidden);
	}

	float TextW()
	{
		return DZS_W.TextWidth(m_Text);
	}

	float CapsW()
	{
		return m_Caps.GetWidth();
	}

	void Place(float x, float y, float w, float h)
	{
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		float pad = DZS_Scale.Px(12);
		float icon = DZS_Scale.Px(16);
		DZS_W.Place(m_CheckMark, pad, (h - icon) / 2, icon, icon);
		float tx = pad + icon + DZS_Scale.Px(6);
		if (m_Icon)
			DZS_W.Place(m_Icon, pad, (h - icon) / 2, icon, icon);
		DZS_W.Place(m_Text, tx, 0, TextW() + 2, h);
		m_Caps.Place(w - pad - CapsW(), (h - m_Caps.GetHeight()) / 2);
		Refresh();
	}

	void Refresh()
	{
		bool hl = m_Popup.m_Highlight == m_Index && m_Item.m_Enabled;
		m_Bg.Show(hl);
		m_CheckMark.Show(m_Item.m_Check && m_Item.m_Checked);
		if (!m_Item.m_Enabled)
			m_Text.SetColor(DZS_Color.Get("textOff"));
		else if (hl)
			m_Text.SetColor(DZS_Color.Get("text"));
		else
			m_Text.SetColor(DZS_Color.Get("textMuted"));
	}

	override void OnHitState(DZS_Hit h)
	{
		if (h.m_Hover && m_Item.m_Enabled)
			m_Popup.SetHighlight(m_Index);
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT && m_Item.m_Enabled)
			m_Popup.Choose(m_Index);
	}
}

// A dropdown or context menu; it stays completely inside the window.
class DZS_Popup : DZS_HitListener
{
	ref DZS_Hit m_Surface;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected ref array<ref DZS_MenuItem> m_Items;
	protected ref array<ref DZS_MenuRow> m_Rows;
	protected ref array<Widget> m_Seps;
	int m_Highlight = -1;
	string m_MenuId;
	ref ScriptInvoker m_OnChoose;
	protected ref array<ref DZS_MenuItem> m_All;
	protected ref DZS_SearchField m_Filter;
	protected string m_FilterText;
	protected string m_EmptyText;
	protected float m_X;
	protected float m_Y;
	protected float m_MinW;
	protected bool m_Rolls;
	protected int m_First;
	protected int m_Last = -1;

	void DZS_Popup(Widget parent, array<ref DZS_MenuItem> items, string menuId)
	{
		m_MenuId = menuId;
		m_Items = items;
		m_OnChoose = new ScriptInvoker();
		m_Surface = new DZS_Hit(parent, this, "popup");
		m_Surface.m_Surface = true;
		m_Bg = DZS_W.Panel(m_Surface.m_W, DZS_Color.Get("surface"));
		m_Border = new DZS_Border(m_Surface.m_W, DZS_Color.Get("lineStrong"));
		m_Rows = new array<ref DZS_MenuRow>();
		m_Seps = new array<Widget>();
		BuildRows();
	}

	protected void BuildRows()
	{
		foreach (DZS_MenuRow old : m_Rows)
			old.m_Hit.Destroy();
		foreach (Widget sw : m_Seps)
			sw.Unlink();
		m_Rows.Clear();
		m_Seps.Clear();
		foreach (int i, DZS_MenuItem it : m_Items)
		{
			if (it.m_SepBefore)
				m_Seps.Insert(DZS_W.Panel(m_Surface.m_W, DZS_Color.Get("line")));
			m_Rows.Insert(new DZS_MenuRow(this, m_Surface.m_W, it, i));
		}
	}

	// A filter field on top: typing keeps the entries containing the text, headings
	// only above a match; the arrow keys do not wrap around.
	void EnableFilter(string placeholder, string emptyText, float minWidthRef)
	{
		m_All = new array<ref DZS_MenuItem>();
		foreach (DZS_MenuItem it : m_Items)
			m_All.Insert(it);
		m_EmptyText = emptyText;
		m_MinW = DZS_Scale.Px(minWidthRef);
		m_Filter = new DZS_SearchField("", placeholder);
		m_Filter.Build(m_Surface.m_W);
	}

	DZS_SearchField GetFilter() { return m_Filter; }

	void Update()
	{
		if (!m_Filter)
			return;
		string t = m_Filter.GetText();
		m_Filter.RefreshExtras();
		if (t == m_FilterText)
			return;
		m_FilterText = t;
		string low = t;
		low.ToLower();
		array<ref DZS_MenuItem> shown = new array<ref DZS_MenuItem>();
		DZS_MenuItem heading = null;
		foreach (DZS_MenuItem it : m_All)
		{
			if (it.m_Heading)
			{
				heading = it;
				continue;
			}
			string text = it.m_Text;
			text.ToLower();
			if (low != "" && text.IndexOf(low) < 0)
				continue;
			if (heading)
			{
				shown.Insert(heading);
				heading = null;
			}
			shown.Insert(it);
		}
		if (shown.Count() == 0)
		{
			DZS_MenuItem none = new DZS_MenuItem();
			none.m_Text = m_EmptyText;
			none.m_Enabled = false;
			shown.Insert(none);
		}
		m_Items = shown;
		m_Highlight = -1;
		m_First = 0;
		BuildRows();
		Open(m_X, m_Y);
		DZS_Log.Info("menu filter '" + t + "': " + shown.Count() + " row(s)");
	}

	// Opens at x, y (below a menu title or at the pointer), moved back into the window if needed.
	// Opens at x, y (below a menu title or at the pointer), moved back into the window if needed. A list taller
	// than the window rolls by whole rows.
	void Open(float x, float y)
	{
		m_X = x;
		m_Y = y;
		float rowH = DZS_Scale.Px(30);
		float sepH = DZS_Scale.Px(9);
		float pad = DZS_Scale.Px(4);
		float textW = 0;
		float capsW = 0;
		foreach (DZS_MenuRow r : m_Rows)
		{
			textW = Math.Max(textW, r.TextW());
			capsW = Math.Max(capsW, r.CapsW());
		}
		float w = DZS_Scale.Px(12 + 16 + 6) + textW + DZS_Scale.Px(32) + capsW + DZS_Scale.Px(12);
		w = Math.Max(w, DZS_Scale.Px(220));
		w = Math.Max(w, m_MinW);
		float fieldH = 0;
		if (m_Filter)
			fieldH = DZS_Scale.Px(32 + 8);
		float rowsH = 0;
		foreach (DZS_MenuItem it : m_Items)
		{
			if (it.m_SepBefore)
				rowsH += sepH;
			rowsH += rowH;
		}
		int sw, sh;
		GetScreenSize(sw, sh);
		float margin = DZS_Scale.Px(8);
		float maxH = sh - 2 * margin;
		if (m_Filter)
			maxH = Math.Max(sh - y - margin, rowH * 6 + fieldH + 2 * pad);
		float h = pad * 2 + fieldH + rowsH;
		m_Rolls = h > maxH;
		if (m_Rolls)
			h = maxH;
		else
			m_First = 0;
		x = Math.Clamp(x, 0, sw - w);
		y = Math.Clamp(y, 0, sh - h);
		m_Surface.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		m_Border.Place(0, 0, w, h);
		float yy = pad;
		if (m_Filter)
		{
			m_Filter.SetWidth((w - 2 * DZS_Scale.Px(6)) / DZS_Scale.Px(1));
			m_Filter.Place(DZS_Scale.Px(6), yy + DZS_Scale.Px(2), DZS_Scale.Px(32));
			yy += fieldH;
		}
		float bottom = h - pad;
		m_Last = -1;
		int si = 0;
		foreach (int i, DZS_MenuRow row : m_Rows)
		{
			bool sep = m_Items[i].m_SepBefore;
			Widget sw2 = null;
			if (sep)
			{
				sw2 = m_Seps[si];
				si++;
			}
			bool shown = i >= m_First && yy + rowH <= bottom + 0.5;
			if (shown && sep && i > m_First)
				shown = yy + sepH + rowH <= bottom + 0.5;
			row.m_Hit.m_W.Show(shown);
			if (sw2)
				sw2.Show(shown && i > m_First);
			if (!shown)
				continue;
			if (sep && i > m_First)
			{
				DZS_W.Place(sw2, DZS_Scale.Px(8), yy + sepH / 2, w - DZS_Scale.Px(16), 1);
				yy += sepH;
			}
			row.Place(1, yy, w - 2, rowH);
			yy += rowH;
			m_Last = i;
		}
	}

	bool Wheel(int delta)
	{
		if (!m_Rolls)
			return true;
		int first = Math.Clamp(m_First - delta, 0, Math.Max(0, m_Items.Count() - 1));
		if (delta < 0 && m_Last >= m_Items.Count() - 1)
			first = m_First;
		if (first != m_First)
		{
			m_First = first;
			Open(m_X, m_Y);
		}
		return true;
	}

	protected void KeepInView(int i)
	{
		if (!m_Rolls || i < 0)
			return;
		int guard = m_Items.Count();
		while (i < m_First && guard > 0)
		{
			m_First--;
			Open(m_X, m_Y);
			guard--;
		}
		while (i > m_Last && guard > 0)
		{
			m_First++;
			Open(m_X, m_Y);
			guard--;
		}
	}

	void SetHighlight(int i)
	{
		m_Highlight = i;
		foreach (DZS_MenuRow r : m_Rows)
			r.Refresh();
	}

	// Arrow keys skip greyed entries.
	void Move(int dir)
	{
		int n = m_Items.Count();
		if (n == 0)
			return;
		int i = m_Highlight;
		for (int k = 0; k < n; k++)
		{
			i += dir;
			if (m_Filter && (i < 0 || i >= n))
				return;
			if (i < 0)
				i = n - 1;
			if (i >= n)
				i = 0;
			if (m_Items[i].m_Enabled)
			{
				SetHighlight(i);
				KeepInView(i);
				return;
			}
		}
	}

	void Choose(int i)
	{
		if (i < 0 || i >= m_Items.Count() || !m_Items[i].m_Enabled)
			return;
		m_OnChoose.Invoke(this, m_Items[i]);
	}

	void ChooseHighlight()
	{
		Choose(m_Highlight);
	}

	bool Contains(Widget w)
	{
		return w && DZS_W.IsInside(w, m_Surface.m_W);
	}

	void Destroy()
	{
		if (m_Filter)
		{
			DZS_Screen scr = DZS_Kernel.Get().GetScreen();
			if (scr.IsFocused(m_Filter))
				scr.Unfocus(false);
		}
		foreach (DZS_MenuRow r : m_Rows)
			r.m_Hit.Destroy();
		m_Surface.Destroy();
	}
}

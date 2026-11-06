// Search field of the catalog: a click empties it for typing and
// keeps the text before as placeholder (and as the search) until something is typed; Ctrl+F keeps the text;
// Esc brings the text before back; "×" and a right click empty it for good.
class DZS_CatSearchField : DZS_SearchField
{
	string m_Before;
	bool m_KeepOnFocus;
	bool m_Committed;
	protected string m_PlaceholderText;

	void SetPlaceholderText(string p)
	{
		m_PlaceholderText = p;
	}

	// What the search uses now.
	string Query()
	{
		string t = m_Edit.GetText();
		if (m_Focused && t == "")
			return m_Before;
		return t;
	}

	override void OnFocusChanged(bool focused)
	{
		if (focused)
		{
			m_Committed = false;
			m_Before = m_Edit.GetText();
			if (!m_KeepOnFocus)
				m_Edit.SetText("");
			m_KeepOnFocus = false;
		}
		else if (!m_Committed || m_Edit.GetText() == "")
			m_Edit.SetText(m_Before);
		m_Focused = focused;
		UpdateBorder();
		RefreshExtras();
	}

	override void RefreshExtras()
	{
		string t = m_Edit.GetText();
		string ph = m_PlaceholderText;
		if (m_Focused && t == "" && m_Before != "")
			ph = m_Before;
		m_Placeholder.SetText(DZS_W.Literal(ph));
		m_Placeholder.Show(t == "");
		m_Clear.Show(t != "" || (m_Focused && m_Before != ""));
	}

	override void OnClear(DZS_Button b)
	{
		m_Before = "";
		m_Committed = true;
		if (m_Focused)
			DZS_Kernel.Get().GetScreen().Unfocus(false);
		SetText("");
	}
}

// Category button: small, square, label font, 24 px; the chosen one in accent.
class DZS_CatChip : DZS_HitListener
{
	string m_Key;
	ref DZS_Hit m_Hit;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected TextWidget m_T;
	bool m_On;
	bool m_Empty;
	ref ScriptInvoker m_OnClick;

	void DZS_CatChip(Widget parent, string key, string label)
	{
		m_Key = key;
		m_OnClick = new ScriptInvoker();
		m_Hit = new DZS_Hit(parent, this, "catalog.chip");
		m_Hit.m_FocusKind = "";
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accentTint"));
		m_Border = new DZS_Border(m_Hit.m_W, DZS_Color.Get("line"));
		m_T = DZS_W.Text(m_Hit.m_W, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"), true);
		m_T.SetText(DZS_Texts.Upper(label));
	}

	float Width()
	{
		return DZS_W.TextWidth(m_T) + DZS_Scale.Px(16);
	}

	void Place(float x, float y)
	{
		float w = Width();
		float h = DZS_Scale.Px(24);
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		m_Border.Place(0, 0, w, h, 1);
		DZS_W.Place(m_T, 0, 0, w, h);
		Refresh();
	}

	void Show(bool s) { m_Hit.m_W.Show(s); }

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
			m_T.SetColor(DZS_Color.Get("textMuted"));
		}
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_OnClick.Invoke(this);
	}
}

// Empty state: symbol in the octagon frame, title in the display font, one sentence, buttons.
class DZS_CatEmpty : Managed
{
	Widget m_Root;
	protected ImageWidget m_Frame;
	protected ImageWidget m_Icon;
	protected ref array<Widget> m_Blocks;
	protected TextWidget m_Title;
	protected ref array<TextWidget> m_Lines;
	ref DZS_Button m_B1;
	ref DZS_Button m_B2;
	protected string m_Text;
	protected bool m_Loading;
	protected float m_T;

	void DZS_CatEmpty(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_Frame = DZS_W.Icon(m_Root, "octagon", DZS_Color.Get("lineStrong"));
		m_Icon = DZS_W.Icon(m_Root, "search", DZS_Color.Get("textMuted"));
		m_Blocks = new array<Widget>();
		for (int i = 0; i < 4; i++)
			m_Blocks.Insert(DZS_W.Panel(m_Root, DZS_Color.Get("blockEmpty")));
		m_Title = DZS_W.Text(m_Root, DZS_Font.DISPLAY, 24, DZS_Color.Get("text"), true);
		m_Lines = new array<TextWidget>();
		m_B1 = new DZS_Button(m_Root, "side", "");
		m_B2 = new DZS_Button(m_Root, "side", "");
	}

	void Set(string icon, string title, string text, string b1, string b2, bool loading)
	{
		DZS_W.SetIcon(m_Icon, icon);
		m_Title.SetText(DZS_W.Literal(title));
		m_Text = text;
		m_Loading = loading;
		m_B1.SetLabel(b1);
		m_B2.SetLabel(b2);
		m_B1.Show(b1 != "");
		m_B2.Show(b2 != "");
	}

	void Show(bool s) { m_Root.Show(s); }

	float Place(float x, float y, float w)
	{
		DZS_W.Place(m_Root, x, y, w, DZS_Scale.Px(400));
		float cy = DZS_Scale.Px(24);
		float box = DZS_Scale.Px(56);
		float bx = (w - box) / 2;
		DZS_W.Place(m_Frame, bx, cy, box, box);
		float ic = DZS_Scale.Px(24);
		DZS_W.Place(m_Icon, bx + (box - ic) / 2, cy + (box - ic) / 2, ic, ic);
		m_Frame.Show(!m_Loading);
		m_Icon.Show(!m_Loading);
		float bs = DZS_Scale.Px(10);
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			m_Blocks[i].Show(m_Loading);
			DZS_W.Place(m_Blocks[i], (w - 4 * bs - 3 * DZS_Scale.Px(4)) / 2 + i * (bs + DZS_Scale.Px(4)), cy + (box - bs) / 2, bs, bs);
		}
		cy += box + DZS_Scale.Px(12);
		DZS_W.Place(m_Title, 0, cy, w, DZS_Scale.Px(30));
		cy += DZS_Scale.Px(34);
		array<string> lines = DZS_CatEmpty.Wrap(m_Root, m_Text, w - DZS_Scale.Px(24));
		while (m_Lines.Count() < lines.Count())
			m_Lines.Insert(DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"), true));
		for (int k = 0; k < m_Lines.Count(); k++)
		{
			m_Lines[k].Show(k < lines.Count());
			if (k >= lines.Count())
				continue;
			m_Lines[k].SetText(DZS_W.Literal(lines[k]));
			DZS_W.Place(m_Lines[k], 0, cy, w, DZS_Scale.Px(18));
			cy += DZS_Scale.Px(18);
		}
		cy += DZS_Scale.Px(12);
		if (m_B1.GetWidget().IsVisible())
		{
			m_B1.Place((w - m_B1.GetWidth()) / 2, cy);
			cy += m_B1.GetHeight() + DZS_Scale.Px(6);
		}
		if (m_B2.GetWidget().IsVisible())
		{
			m_B2.Place((w - m_B2.GetWidth()) / 2, cy);
			cy += m_B2.GetHeight() + DZS_Scale.Px(6);
		}
		return cy;
	}

	void Update(float dt)
	{
		if (!m_Loading)
			return;
		m_T += dt;
		int lit = Math.Floor(m_T / 0.2);
		lit = lit % 4;
		for (int i = 0; i < m_Blocks.Count(); i++)
		{
			if (i == lit)
				m_Blocks[i].SetColor(DZS_Color.Get("accent"));
			else
				m_Blocks[i].SetColor(DZS_Color.Get("blockEmpty"));
		}
	}

	protected static TextWidget s_Probe;

	// Lines of a text no wider than w in the reading font (the game's text widgets do not wrap here).
	static array<string> Wrap(Widget parent, string text, float w)
	{
		array<string> r = new array<string>();
		if (!s_Probe)
		{
			s_Probe = DZS_W.Text(parent, DZS_Font.READ, 13, 0);
			s_Probe.Show(false);
		}
		array<string> words = new array<string>();
		text.Split(" ", words);
		string cur = "";
		foreach (string word : words)
		{
			string tryLine = word;
			if (cur != "")
				tryLine = cur + " " + word;
			s_Probe.SetText(DZS_W.Literal(tryLine));
			if (DZS_W.TextWidth(s_Probe) > w && cur != "")
			{
				r.Insert(cur);
				cur = word;
			}
			else
				cur = tryLine;
		}
		if (cur != "")
			r.Insert(cur);
		return r;
	}
}

class DZS_CatPanelTiles : DZS_CatTileOwner
{
	DZS_CatalogPanel m_Panel;

	override void OnTileClick(DZS_CatTile t, int button) { m_Panel.m_P.OnTileClick(t, button, "bar"); }
	override void OnTileStar(DZS_CatTile t) { m_Panel.m_P.ToggleFavourite(t.m_Entry); }
	override void OnTileDrag(DZS_CatTile t) { m_Panel.m_P.OnTileDrag(t); }
}

// Catalog bar: search, recent row, category buttons, the list (only visible rows are built),
// foot with "All objects in full screen".
class DZS_CatalogPanel : DZS_PanelContent
{
	DZS_CatalogPlugin m_P;
	protected Widget m_Root;
	ref DZS_CatSearchField m_Search;
	protected ref DZS_CatPanelTiles m_TileOwner;
	protected ref array<ref DZS_CatTile> m_Recent;
	protected TextWidget m_RecentEmpty;
	protected ref array<ref DZS_CatChip> m_Chips;
	protected Widget m_ListLine;
	protected Widget m_ListClip;
	protected ref DZS_Hit m_ListHit;
	protected ref array<ref DZS_CatTile> m_Rows;
	protected Widget m_ScrollBar;
	protected ref DZS_CatEmpty m_Empty;
	protected Widget m_FootLine;
	protected ref DZS_Button m_FullButton;
	protected float m_W;
	protected float m_H;
	protected float m_ListY;
	protected float m_ListH;
	float m_Scroll;
	protected bool m_Built;

	void DZS_CatalogPanel(DZS_CatalogPlugin p)
	{
		m_P = p;
	}

	override bool FillsView() { return true; }

	override void Build(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_TileOwner = new DZS_CatPanelTiles();
		m_TileOwner.m_Panel = this;
		m_Search = new DZS_CatSearchField("catalog.search", DZS_Texts.Get("catalog.search.placeholder"));
		m_Search.Build(m_Root);
		m_Search.SetPlaceholderText(DZS_Texts.Get("catalog.search.placeholder"));
		m_Search.m_OnCommit.Insert(OnSearchCommit);
		m_Recent = new array<ref DZS_CatTile>();
		for (int i = 0; i < 8; i++)
			m_Recent.Insert(new DZS_CatTile(m_Root, "small", m_TileOwner));
		m_RecentEmpty = DZS_W.Text(m_Root, DZS_Font.READ, 12, DZS_Color.Get("textQuiet"));
		m_RecentEmpty.SetText(DZS_Texts.Get("catalog.recent.empty"));
		m_Chips = new array<ref DZS_CatChip>();
		m_ListLine = DZS_W.Panel(m_Root, DZS_Color.Get("line"));
		m_ListHit = new DZS_Hit(m_Root, null, "catalog.list");
		m_ListHit.m_Surface = true;
		m_ListClip = m_ListHit.m_W;
		m_ListClip.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Rows = new array<ref DZS_CatTile>();
		m_ScrollBar = DZS_W.Panel(m_Root, DZS_Color.Get("lineStrong"));
		m_Empty = new DZS_CatEmpty(m_ListClip);
		m_Empty.m_B1.m_OnClick.Insert(OnEmptyButton1);
		m_Empty.m_B2.m_OnClick.Insert(OnEmptyButton2);
		m_FootLine = DZS_W.Panel(m_Root, DZS_Color.Get("line"));
		m_FullButton = new DZS_Button(m_Root, "side", DZS_Texts.Get("catalog.full.open"), "maximize-2");
		m_FullButton.m_OnClick.Insert(OnFullClick);
		m_Built = true;
	}

	void OnSearchCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		m_Search.m_Committed = true;
		r.m_Ok = true;
	}

	void OnFullClick(DZS_Button b)
	{
		m_P.OpenFull();
	}

	void OnEmptyButton1(DZS_Button b)
	{
		m_P.ClearSearch();
	}

	void OnEmptyButton2(DZS_Button b)
	{
		m_P.SetCategory("all");
	}

	void RebuildChips()
	{
		foreach (DZS_CatChip c : m_Chips)
			c.m_Hit.Destroy();
		m_Chips.Clear();
		array<string> keys = m_P.ChipKeys();
		foreach (string k : keys)
		{
			DZS_CatChip chip = new DZS_CatChip(m_Root, k, m_P.CategoryName(k));
			chip.m_OnClick.Insert(OnChip);
			m_Chips.Insert(chip);
		}
	}

	void OnChip(DZS_CatChip c)
	{
		m_P.SetCategory(c.m_Key);
	}

	override void SetView(float width, float height)
	{
		m_W = width;
		m_H = height;
	}

	override bool Wheel(int delta)
	{
		m_Scroll -= delta * DZS_Scale.Px(48) * 3;
		ClampScroll();
		return true;
	}

	protected void ClampScroll()
	{
		float rowH = DZS_Scale.Px(48);
		float total = m_P.ListCount() * rowH;
		m_Scroll = Math.Clamp(m_Scroll, 0, Math.Max(0, total - m_ListH));
	}

	// Rolls the list so that this index is in view.
	void ScrollToIndex(int i)
	{
		float rowH = DZS_Scale.Px(48);
		float top = i * rowH;
		if (top < m_Scroll)
			m_Scroll = top;
		else if (top + rowH > m_Scroll + m_ListH)
			m_Scroll = top + rowH - m_ListH;
		ClampScroll();
	}

	void ResetScroll()
	{
		m_Scroll = 0;
	}

	override void Update(float dt)
	{
		if (!m_Built || m_W <= 0)
			return;
		if (m_Chips.Count() != m_P.ChipKeys().Count())
			RebuildChips();
		float pad = DZS_Scale.Px(12);
		float y = DZS_Scale.Px(10);
		DZS_W.Place(m_Root, 0, 0, m_W, m_H);
		m_Search.SetWidth((m_W - 2 * pad) / DZS_Scale.Factor());
		m_Search.Place(pad, y, DZS_Scale.Px(32));
		m_Search.RefreshExtras();
		y += DZS_Scale.Px(32) + DZS_Scale.Px(10);
		// recent row
		array<DZS_CatEntry> recent = m_P.RecentEntries(8);
		float rs = DZS_Scale.Px(32);
		float rg = DZS_Scale.Px(4);
		for (int i = 0; i < m_Recent.Count(); i++)
		{
			DZS_CatTile t = m_Recent[i];
			bool on = i < recent.Count();
			t.Show(on);
			if (!on)
				continue;
			t.Bind(recent[i]);
			t.m_InHand = m_P.IsInHand(recent[i]);
			t.Place(pad + i * (rs + rg), y, rs);
			t.Refresh("", dt);
		}
		m_RecentEmpty.Show(recent.Count() == 0);
		DZS_W.Place(m_RecentEmpty, pad, y, m_W - 2 * pad, rs);
		y += rs + DZS_Scale.Px(10);
		// category buttons, wrapping
		float cx = pad;
		float ch = DZS_Scale.Px(24);
		float cg = DZS_Scale.Px(4);
		foreach (DZS_CatChip c : m_Chips)
		{
			float cw = c.Width();
			if (cx + cw > m_W - pad && cx > pad)
			{
				cx = pad;
				y += ch + cg;
			}
			c.m_On = c.m_Key == m_P.m_Cat;
			c.Place(cx, y);
			cx += cw + cg;
		}
		y += ch + DZS_Scale.Px(10);
		DZS_W.Place(m_ListLine, 0, y, m_W, 1);
		y += 1;
		float footH = DZS_Scale.Px(48);
		m_ListY = y;
		m_ListH = Math.Max(0, m_H - y - footH);
		m_ListHit.Place(0, m_ListY, m_W, m_ListH);
		DZS_W.Place(m_FootLine, 0, m_H - footH, m_W, 1);
		m_FullButton.Place((m_W - m_FullButton.GetWidth()) / 2, m_H - footH + (footH - m_FullButton.GetHeight()) / 2);
		m_FullButton.SetTipAction(DZS_Texts.Get("catalog.full.open"), "catalog.full");
		LayoutList(dt);
	}

	protected void LayoutList(float dt)
	{
		float rowH = DZS_Scale.Px(48);
		int n = m_P.ListCount();
		bool empty = n == 0;
		m_Empty.Show(empty);
		if (empty)
		{
			string icon, title, text, b1, b2;
			bool loading;
			m_P.EmptyState(icon, title, text, b1, b2, loading);
			m_Empty.Set(icon, title, text, b1, b2, loading);
			m_Empty.Place(0, 0, m_W);
			m_Empty.Update(dt);
		}
		ClampScroll();
		int first = Math.Floor(m_Scroll / rowH);
		int visible = Math.Ceil(m_ListH / rowH) + 1;
		while (m_Rows.Count() < visible)
			m_Rows.Insert(new DZS_CatTile(m_ListClip, "row", m_TileOwner));
		for (int k = 0; k < m_Rows.Count(); k++)
		{
			DZS_CatTile row = m_Rows[k];
			int idx = first + k;
			if (empty || k >= visible || idx >= n)
			{
				row.Show(false);
				continue;
			}
			DZS_CatEntry e = m_P.ListAt(idx);
			row.Show(true);
			row.Bind(e);
			row.m_InHand = m_P.IsInHand(e);
			row.m_Fav = m_P.IsFavourite(e);
			row.m_Off = m_P.IsOffNow(e);
			row.SetPhoto(m_P.PhotoOf(e));
			row.Place(0, idx * rowH - m_Scroll, m_W);
			row.Refresh(m_P.SubText(e), dt);
		}
		float total = n * rowH;
		bool bar = total > m_ListH && !empty;
		m_ScrollBar.Show(bar);
		if (bar)
		{
			float bh = Math.Max(DZS_Scale.Px(24), m_ListH * m_ListH / total);
			float by = (m_ListH - bh) * m_Scroll / (total - m_ListH);
			DZS_W.Place(m_ScrollBar, m_W - DZS_Scale.Px(4), m_ListY + by, DZS_Scale.Px(4), bh);
		}
	}

	DZS_CatTile RowOf(DZS_CatEntry e)
	{
		foreach (DZS_CatTile r : m_Rows)
		{
			if (r.IsShown() && r.m_Entry == e)
				return r;
		}
		return null;
	}

	float ListHeight() { return m_ListH; }
}

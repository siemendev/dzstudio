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

// Pictures on their way: "LOADING PICTURES 12 / 30" over a thin bar, a running segment while there is no
// count yet; when the last one is in, the full bar stays a moment.
class DZS_CatPhotoBar : Managed
{
	static const float HOLD = 0.8;

	protected Widget m_Root;
	protected TextWidget m_Label;
	protected TextWidget m_Count;
	protected Widget m_Track;
	protected Widget m_TrackBg;
	protected Widget m_Fill;
	protected bool m_Busy;
	protected bool m_Known;
	protected float m_Share;
	protected float m_Shown;
	protected float m_Hold;
	protected float m_T;
	protected string m_CountText;

	void DZS_CatPhotoBar(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_Label = DZS_W.Text(m_Root, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"));
		m_Count = DZS_W.Text(m_Root, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"), false, true);
		m_Track = DZS_W.Frame(m_Root);
		m_Track.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_TrackBg = DZS_W.Panel(m_Track, DZS_Color.Get("blockEmpty"));
		m_Fill = DZS_W.Panel(m_Track, DZS_Color.Get("accent"));
		m_Root.Show(false);
	}

	// Takes the state of the catalog's image sources; true while the bar is to be shown.
	bool Update(DZS_CatalogPlugin p, float dt)
	{
		int done, total;
		bool busy = p.PhotoProgress(done, total);
		m_T += dt;
		if (busy)
		{
			m_Busy = true;
			m_Known = total > 0;
			m_Hold = HOLD;
			if (m_Known)
			{
				m_Share = done;
				m_Share = m_Share / total;
				string sd = done.ToString();
				string st = total.ToString();
				m_CountText = DZS_Texts.Fmt("catalog.photos.count", sd, st);
			}
		}
		else if (m_Busy)
		{
			m_Busy = false;
			m_Share = 1;
			m_CountText = "";
		}
		if (!m_Busy)
		{
			m_Hold -= dt;
			if (m_Hold <= 0)
			{
				m_Shown = 0;
				m_Root.Show(false);
				return false;
			}
		}
		if (m_Shown > m_Share)
			m_Shown = m_Share;
		m_Shown += (m_Share - m_Shown) * Math.Min(1, dt * 10);
		string label = "catalog.photos.loading";
		if (!m_Known)
			label = "catalog.photos.list";
		if (!m_Busy)
			label = "catalog.photos.done";
		m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Get(label)));
		m_Count.SetText(m_CountText);
		m_Root.Show(true);
		return true;
	}

	float Height() { return DZS_Scale.Px(30); }

	void Place(float x, float y, float w)
	{
		float h = Height();
		DZS_W.Place(m_Root, x, y, w, h);
		float lh = DZS_Scale.Px(16);
		DZS_W.Place(m_Label, 0, DZS_Scale.Px(4), w * 0.7, lh);
		DZS_W.Place(m_Count, w * 0.4, DZS_Scale.Px(4), w * 0.6, lh);
		float th = DZS_Scale.Px(3);
		float ty = DZS_Scale.Px(4) + lh + DZS_Scale.Px(4);
		DZS_W.Place(m_Track, 0, ty, w, th);
		DZS_W.Place(m_TrackBg, 0, 0, w, th);
		if (m_Busy && !m_Known)
		{
			float seg = w * 0.25;
			float run = m_T * 0.8;
			run = run - Math.Floor(run);
			DZS_W.Place(m_Fill, -seg + run * (w + seg), 0, seg, th);
		}
		else
			DZS_W.Place(m_Fill, 0, 0, w * m_Shown, th);
	}
}

// The category tree of the bar rolls with the wheel when it is taller than the room it may take.
class DZS_CatTreeWheel : DZS_HitListener
{
	DZS_CatalogPanel m_Panel;

	override bool OnHitWheel(DZS_Hit h, int delta)
	{
		return m_Panel.TreeWheel(delta);
	}
}

class DZS_CatPanelFilter : DZS_CatFilterOwner
{
	DZS_CatalogPanel m_Panel;

	override void OnFilterRow(string key, string group) { m_Panel.OnTreeRow(key, group); }
}

class DZS_CatPanelTiles : DZS_CatTileOwner
{
	DZS_CatalogPanel m_Panel;

	override void OnTileClick(DZS_CatTile t, int button) { m_Panel.m_P.OnTileClick(t, button, "bar"); }
	override void OnTileStar(DZS_CatTile t) { m_Panel.m_P.ToggleFavourite(t.m_Entry); }
	override void OnTileDrag(DZS_CatTile t) { m_Panel.m_P.OnTileDrag(t); }
}

// Catalog bar: search, recent row, the category tree (only the chosen category open with its groups), the list
// (only visible rows are built), foot with "All objects in full screen".
class DZS_CatalogPanel : DZS_PanelContent
{
	DZS_CatalogPlugin m_P;
	protected Widget m_Root;
	ref DZS_CatSearchField m_Search;
	protected ref DZS_CatPanelTiles m_TileOwner;
	protected ref array<ref DZS_CatTile> m_Recent;
	protected TextWidget m_RecentEmpty;
	protected ref DZS_CatTreeWheel m_TreeWheel;
	protected ref DZS_CatPanelFilter m_TreeOwner;
	protected ref DZS_Hit m_TreeClip;
	protected Widget m_TreeBar;
	protected ref array<ref DZS_CatFilterRow> m_TreeRows;
	protected ref array<ref DZS_CatFilterRow> m_TreeGroups;
	protected float m_TreeScroll;
	protected float m_TreeContentH;
	protected float m_TreeViewH;
	protected string m_TreeShown = "-";
	protected Widget m_ListLine;
	protected Widget m_ListClip;
	protected ref DZS_Hit m_ListHit;
	protected ref array<ref DZS_CatTile> m_Rows;
	protected Widget m_ScrollBar;
	protected ref DZS_CatEmpty m_Empty;
	protected ref DZS_CatPhotoBar m_PhotoBar;
	protected Widget m_FootLine;
	protected ref DZS_Button m_FullButton;
	protected float m_W;
	protected float m_H;
	protected float m_ListY;
	protected float m_ListH;
	float m_Scroll;
	protected bool m_Built;
	static const float TREE_ROW_H = 26;
	// share of the bar the tree may take: at 1080p every category row fits unopened and the list keeps about
	// eight rows; a longer tree scrolls inside it
	static const float TREE_SHARE = 0.45;

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
		m_TreeWheel = new DZS_CatTreeWheel();
		m_TreeWheel.m_Panel = this;
		m_TreeOwner = new DZS_CatPanelFilter();
		m_TreeOwner.m_Panel = this;
		m_TreeClip = new DZS_Hit(m_Root, m_TreeWheel, "catalog.tree");
		m_TreeClip.m_Surface = true;
		m_TreeClip.m_W.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_TreeBar = DZS_W.Panel(m_TreeClip.m_W, DZS_Color.Get("lineStrong"));
		m_TreeRows = new array<ref DZS_CatFilterRow>();
		m_TreeGroups = new array<ref DZS_CatFilterRow>();
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
		m_PhotoBar = new DZS_CatPhotoBar(m_Root);
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

	protected DZS_CatFilterRow TreeRow(string key, string label, string group)
	{
		DZS_CatFilterRow r = new DZS_CatFilterRow(m_TreeClip.m_W, m_TreeOwner, key, label, group);
		r.m_RowH = TREE_ROW_H;
		return r;
	}

	protected void RebuildTree()
	{
		foreach (DZS_CatFilterRow old : m_TreeRows)
			old.m_Hit.Destroy();
		m_TreeRows.Clear();
		array<string> keys = m_P.RowKeys();
		foreach (string k : keys)
			m_TreeRows.Insert(TreeRow(k, m_P.CategoryName(k), ""));
	}

	protected void RebuildTreeGroups()
	{
		foreach (DZS_CatFilterRow old : m_TreeGroups)
			old.m_Hit.Destroy();
		m_TreeGroups.Clear();
		array<ref DZS_CatGroup> groups = m_P.m_Store.m_Groups;
		foreach (DZS_CatGroup g : groups)
			m_TreeGroups.Insert(TreeRow(g.m_Cat, g.m_Name, g.m_Key));
	}

	// A category opens with all its groups; a group chooses its category with it.
	void OnTreeRow(string key, string group)
	{
		m_P.SetCategory(key);
		m_P.SetGroup(group);
	}

	bool TreeWheel(int delta)
	{
		if (m_TreeContentH <= m_TreeViewH)
			return false;
		m_TreeScroll = Math.Clamp(m_TreeScroll - delta * DZS_Scale.Px(TREE_ROW_H) * 2, 0, m_TreeContentH - m_TreeViewH);
		return true;
	}

	// Lays the tree out from y; returns the height it takes.
	protected float LayoutTree(float y)
	{
		if (m_TreeRows.Count() != m_P.RowKeys().Count())
			RebuildTree();
		if (m_TreeGroups.Count() != m_P.m_Store.m_Groups.Count())
			RebuildTreeGroups();
		string cat = m_P.m_Cat;
		string group = m_P.m_Group;
		bool open = m_P.HasGroups(cat);
		float rh = DZS_Scale.Px(TREE_ROW_H);
		float gap = DZS_Scale.Px(6);
		float cy = 0;
		float chosenTop = 0;
		float chosenEnd = 0;
		for (int i = 0; i < m_TreeRows.Count(); i++)
		{
			DZS_CatFilterRow r = m_TreeRows[i];
			if (i == 2)
				cy += gap;
			bool mine = r.m_Key == cat;
			if (i < 2 || r.m_Key == "templates" || !m_P.HasGroups(r.m_Key))
				r.SetExpander(-1);
			else if (mine)
				r.SetExpander(1);
			else
				r.SetExpander(0);
			bool whole = false;
			if (mine && group == "")
				whole = true;
			bool parent = false;
			if (mine && open)
				parent = true;
			r.Place(0, cy - m_TreeScroll, m_W, m_P.Count(r.m_Key), whole, parent);
			if (mine)
			{
				chosenTop = cy;
				chosenEnd = cy + rh;
			}
			cy += rh;
			if (!mine || !open)
				continue;
			foreach (DZS_CatFilterRow gr : m_TreeGroups)
			{
				if (gr.m_Key != cat)
					continue;
				bool on = gr.m_Group == group;
				gr.Place(0, cy - m_TreeScroll, m_W, m_P.Count(gr.m_Key + "/" + gr.m_Group), on);
				if (on)
				{
					chosenTop = cy;
					chosenEnd = cy + rh;
				}
				else if (group == "")
					chosenEnd = cy + rh;
				cy += rh;
			}
		}
		foreach (DZS_CatFilterRow hide : m_TreeGroups)
		{
			bool child = false;
			if (open && hide.m_Key == cat)
				child = true;
			hide.Show(child);
		}
		m_TreeContentH = cy;
		m_TreeViewH = Math.Min(cy, Math.Max(m_H * TREE_SHARE, 6 * rh));
		string shown = cat + "/" + group;
		if (shown != m_TreeShown)
		{
			m_TreeShown = shown;
			if (chosenEnd - m_TreeScroll > m_TreeViewH)
				m_TreeScroll = chosenEnd - m_TreeViewH;
			if (chosenTop < m_TreeScroll)
				m_TreeScroll = chosenTop;
		}
		m_TreeScroll = Math.Clamp(m_TreeScroll, 0, m_TreeContentH - m_TreeViewH);
		m_TreeClip.Place(0, y, m_W, m_TreeViewH);
		bool more = m_TreeContentH > m_TreeViewH;
		m_TreeBar.Show(more);
		if (more)
		{
			float bh = Math.Max(DZS_Scale.Px(16), m_TreeViewH * m_TreeViewH / m_TreeContentH);
			float by = (m_TreeViewH - bh) * m_TreeScroll / (m_TreeContentH - m_TreeViewH);
			DZS_W.Place(m_TreeBar, m_W - DZS_Scale.Px(4), by, DZS_Scale.Px(4), bh);
		}
		return m_TreeViewH;
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
		float pad = DZS_Scale.Px(12);
		float y = DZS_Scale.Px(10);
		DZS_W.Place(m_Root, 0, 0, m_W, m_H);
		m_Search.SetWidth((m_W - 2 * pad) / DZS_Scale.Factor());
		m_Search.Place(pad, y, DZS_Scale.Px(32));
		m_Search.RefreshExtras();
		y += DZS_Scale.Px(32) + DZS_Scale.Px(10);
		// in a layer: no recent row, no categories, no full screen — only what the layer creates
		bool layer = m_P.InLayer();
		// recent row
		array<DZS_CatEntry> recent = m_P.RecentEntries(8);
		if (layer)
			recent.Clear();
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
		m_RecentEmpty.Show(recent.Count() == 0 && !layer);
		DZS_W.Place(m_RecentEmpty, pad, y, m_W - 2 * pad, rs);
		if (!layer)
			y += rs + DZS_Scale.Px(10);
		m_TreeClip.m_W.Show(!layer);
		if (!layer)
			y += LayoutTree(y) + DZS_Scale.Px(6);
		DZS_W.Place(m_ListLine, 0, y, m_W, 1);
		y += 1;
		float footH = DZS_Scale.Px(48);
		if (layer)
			footH = 0;
		m_FootLine.Show(!layer);
		m_FullButton.Show(!layer);
		float barH = 0;
		if (m_PhotoBar.Update(m_P, dt))
		{
			barH = m_PhotoBar.Height() + DZS_Scale.Px(4);
			m_PhotoBar.Place(pad, m_H - footH - barH, m_W - 2 * pad);
		}
		m_ListY = y;
		m_ListH = Math.Max(0, m_H - y - footH - barH);
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
			row.m_Loading = m_P.PhotoPending(e);
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

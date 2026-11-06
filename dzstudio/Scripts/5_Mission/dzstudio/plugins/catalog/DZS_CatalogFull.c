class DZS_CatFullTiles : DZS_CatTileOwner
{
	DZS_CatalogFull m_F;

	override void OnTileClick(DZS_CatTile t, int button) { m_F.m_P.OnTileClick(t, button, "full"); }
	override void OnTileDouble(DZS_CatTile t) { m_F.PlaceEntry(t.m_Entry); }
	override void OnTileDrag(DZS_CatTile t) { m_F.PlaceEntry(t.m_Entry); }
}

// One line of the filter list: name left, count right in quiet text, the chosen one
// with accent tint, accent text and the bar; count 0 greyed but visible.
class DZS_CatFilterRow : DZS_HitListener
{
	string m_Key;
	ref DZS_Hit m_Hit;
	protected Widget m_Tint;
	protected Widget m_Bar;
	protected TextWidget m_Name;
	protected TextWidget m_Count;
	DZS_CatalogFull m_F;

	void DZS_CatFilterRow(Widget parent, DZS_CatalogFull f, string key, string label)
	{
		m_F = f;
		m_Key = key;
		m_Hit = new DZS_Hit(parent, this, "catalog.filter");
		m_Hit.m_FocusKind = "";
		m_Tint = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accentTint"));
		m_Bar = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accent"));
		m_Name = DZS_W.Text(m_Hit.m_W, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_Name.SetText(label);
		m_Count = DZS_W.Text(m_Hit.m_W, DZS_Font.MONO, 11, DZS_Color.Get("textQuiet"), false, true);
	}

	void Place(float x, float y, float w, int count, bool chosen)
	{
		float h = DZS_Scale.Px(30);
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Tint, 0, 0, w, h);
		DZS_W.Place(m_Bar, 0, 0, DZS_Scale.Px(3), h);
		m_Tint.Show(chosen || m_Hit.m_Hover);
		m_Bar.Show(chosen);
		DZS_W.Place(m_Name, DZS_Scale.Px(12), 0, w - DZS_Scale.Px(60), h);
		m_Count.SetText(count.ToString());
		DZS_W.Place(m_Count, w - DZS_Scale.Px(60), 0, DZS_Scale.Px(50), h);
		if (chosen)
			m_Name.SetColor(DZS_Color.Get("accentText"));
		else if (count == 0)
			m_Name.SetColor(DZS_Color.Get("textOff"));
		else
			m_Name.SetColor(DZS_Color.Get("text"));
	}

	void Show(bool s) { m_Hit.m_W.Show(s); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT)
			m_F.SetFilter(m_Key);
	}
}

// Surface of the grid: the wheel rolls it, Strg + wheel changes the tile size.
class DZS_CatGridHit : DZS_HitListener
{
	DZS_CatalogFull m_F;

	override bool OnHitWheel(DZS_Hit h, int delta)
	{
		return m_F.GridWheel(delta);
	}
}

// Full-screen search: over the scene and both bars between the
// top bar and the status bar. Its search field keeps the keyboard; arrows move the mark, Enter places, Esc closes.
class DZS_CatalogFull : Managed
{
	void MarkDirty() { m_Dirty = true; }

	static const float FILTER_W = 210;
	static const float DETAIL_W = 300;
	static const float GAP = 8;

	DZS_CatalogPlugin m_P;
	protected bool m_Open;
	protected ref DZS_Hit m_Root;
	protected Widget m_Bg;
	protected Widget m_Bg2;
	protected ref array<ImageWidget> m_FactChecks;
	protected Widget m_FieldBg;
	protected ref DZS_Border m_FieldBorder;
	protected ImageWidget m_Lens;
	protected EditBoxWidget m_Edit;
	protected TextWidget m_Hits;
	protected ref DZS_RichLine m_EscText;
	protected Widget m_Lines;
	protected TextWidget m_RangeHead;
	protected TextWidget m_KindHead;
	protected ref array<ref DZS_CatFilterRow> m_Filters;
	protected ref DZS_CatGridHit m_GridListener;
	protected ref DZS_Hit m_Grid;
	protected ref array<ref DZS_CatTile> m_Cards;
	protected ref DZS_CatFullTiles m_TileOwner;
	protected ref DZS_CatEmpty m_Empty;
	protected Widget m_ScrollBar;
	// details
	protected ref DZS_Hit m_DetailImgHit;
	protected Widget m_DetailImgBg;
	protected ImageWidget m_DetailPhoto;
	protected ImageWidget m_DetailPlace;
	protected TextWidget m_DetailName;
	protected Widget m_CodeBg;
	protected TextWidget m_DetailClass;
	protected ref array<TextWidget> m_DescLines;
	protected ref array<TextWidget> m_FactLabels;
	protected ref array<TextWidget> m_FactValues;
	protected ref DZS_Button m_Place;
	protected string m_Filter = "alle";
	protected ref DZS_CatResult m_All;
	protected ref array<DZS_CatEntry> m_Shown;
	protected ref map<string, int> m_Counts;
	protected DZS_CatEntry m_Marked;
	protected string m_Text;
	protected bool m_Dirty;
	protected float m_Scroll;
	protected int m_Cols = 1;
	protected EntityAI m_DetailEntity;
	protected DZS_CatEntry m_DetailFor;
	protected ref map<string, bool> m_KeyWas;
	protected int m_ProjectStamp = -1;
	protected string m_ProjectText;
	protected DZS_CatEntry m_ProjectFor;

	void DZS_CatalogFull(DZS_CatalogPlugin p)
	{
		m_P = p;
		m_KeyWas = new map<string, bool>();
		Widget layer = p.Kernel().GetScreen().GetSheetLayer();
		m_Root = new DZS_Hit(layer, null, "catalog.full");
		m_Root.m_Surface = true;
		Widget r = m_Root.m_W;
		m_Bg = DZS_W.Panel(r, DZS_Color.WithAlpha("surface", 250));
		m_Bg2 = DZS_W.Panel(r, DZS_Color.WithAlpha("surface", 250));
		m_FieldBg = DZS_W.Panel(r, DZS_Color.Get("well"));
		m_FieldBorder = new DZS_Border(r, DZS_Color.Get("accent"));
		m_Lens = DZS_W.Icon(r, "search", DZS_Color.Get("textQuiet"));
		m_Edit = EditBoxWidget.Cast(DZS_W.FromTexts("edit_read", r));
		m_Edit.SetColor(DZS_Color.Get("text"));
		m_Hits = DZS_W.Text(r, DZS_Font.MONO, 12, DZS_Color.Get("textMuted"));
		m_EscText = new DZS_RichLine(r, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_EscText.Set(DZS_Texts.Get("catalog.full.esc"));
		m_Lines = DZS_W.Frame(r);
		m_RangeHead = DZS_W.Text(r, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_RangeHead.SetText(DZS_Texts.Upper(DZS_Texts.Get("catalog.full.range")));
		m_KindHead = DZS_W.Text(r, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_KindHead.SetText(DZS_Texts.Upper(DZS_Texts.Get("catalog.full.kind")));
		m_Filters = new array<ref DZS_CatFilterRow>();
		array<string> keys = {"alle", "favoriten", "zuletzt"};
		array<string> cats = DZS_CatalogPlugin.Categories();
		foreach (string c : cats)
			keys.Insert(c);
		foreach (string k : keys)
			m_Filters.Insert(new DZS_CatFilterRow(r, this, k, p.CategoryName(k)));
		m_GridListener = new DZS_CatGridHit();
		m_GridListener.m_F = this;
		m_Grid = new DZS_Hit(r, m_GridListener, "catalog.grid");
		m_Grid.m_Surface = true;
		m_Grid.m_W.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_TileOwner = new DZS_CatFullTiles();
		m_TileOwner.m_F = this;
		m_Cards = new array<ref DZS_CatTile>();
		m_Empty = new DZS_CatEmpty(m_Grid.m_W);
		m_Empty.m_B1.m_OnClick.Insert(OnEmptyClear);
		m_Empty.m_B2.m_OnClick.Insert(OnEmptyAll);
		m_ScrollBar = DZS_W.Panel(r, DZS_Color.Get("lineStrong"));
		m_DetailImgHit = new DZS_Hit(r, null, "catalog.detailImage");
		m_DetailImgHit.m_FocusKind = "";
		m_DetailImgBg = DZS_W.Panel(m_DetailImgHit.m_W, DZS_Color.Get("imageBg"));
		m_DetailPhoto = DZS_W.Icon(m_DetailImgHit.m_W, "image", ARGB(255, 255, 255, 255));
		m_DetailPlace = DZS_W.Icon(m_DetailImgHit.m_W, "package", DZS_Color.Get("textQuiet"));
		m_DetailName = DZS_W.Text(r, DZS_Font.DISPLAY, 24, DZS_Color.Get("text"));
		m_CodeBg = DZS_W.Panel(r, DZS_Color.Get("well"));
		m_DetailClass = DZS_W.Text(r, DZS_Font.MONO, 12, DZS_Color.Get("textMuted"));
		m_DescLines = new array<TextWidget>();
		m_FactLabels = new array<TextWidget>();
		m_FactValues = new array<TextWidget>();
		m_FactChecks = new array<ImageWidget>();
		for (int i = 0; i < 3; i++)
		{
			m_FactLabels.Insert(DZS_W.Text(r, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet")));
			m_FactValues.Insert(DZS_W.Text(r, DZS_Font.READ, 13, DZS_Color.Get("text")));
			m_FactChecks.Insert(DZS_W.Icon(r, "check", DZS_Color.Get("success")));
		}
		m_Place = new DZS_Button(r, "main", DZS_Texts.Get("catalog.full.place"), "", 32);
		m_Place.SetCaps("RETURN");
		m_Place.m_OnClick.Insert(OnPlaceClick);
		m_Shown = new array<DZS_CatEntry>();
		m_Counts = new map<string, int>();
		m_Root.m_W.Show(false);
	}

	void Destroy()
	{
		m_Root.Destroy();
		foreach (DZS_CatTile c : m_Cards)
			c.Destroy();
	}

	bool IsOpen() { return m_Open; }

	string Query() { return m_Text; }

	void Open(string text)
	{
		m_Open = true;
		m_Text = text;
		m_Edit.SetText(text);
		m_Root.m_W.Show(true);
		m_Filter = "alle";
		if (m_P.m_Cat != "alle")
			m_Filter = m_P.m_Cat;
		m_Dirty = true;
		m_Marked = null;
		m_Scroll = 0;
		DZS_Screen scr = m_P.Kernel().GetScreen();
		scr.SetKeyboardOwner("fullsearch", true);
		scr.SetHint("catalog.full", 20, DZS_Texts.Get("catalog.full.hint"));
		SetFocus(m_Edit);
		DZS_Log.Info("catalog: full-screen search open with '" + text + "'");
	}

	void Close()
	{
		m_Open = false;
		m_Root.m_W.Show(false);
		DZS_Screen scr = m_P.Kernel().GetScreen();
		scr.SetKeyboardOwner("fullsearch", false);
		scr.SetHint("catalog.full", 20, "");
		SetFocus(null);
		m_DetailEntity = null;
		m_DetailFor = null;
		foreach (DZS_CatTile c : m_Cards)
			c.Show(false);
		DZS_Log.Info("catalog: full-screen search closed");
	}

	void SetFilter(string key)
	{
		m_Filter = key;
		m_Dirty = true;
		m_Marked = null;
		m_Scroll = 0;
		DZS_Log.Info("catalog: full-screen filter " + key);
	}

	void OnEmptyClear(DZS_Button b)
	{
		m_Edit.SetText("");
	}

	void OnEmptyAll(DZS_Button b)
	{
		SetFilter("alle");
	}

	void Mark(DZS_CatEntry e)
	{
		m_Marked = e;
		ScrollToMarked();
	}

	void OnPlaceClick(DZS_Button b)
	{
		if (m_Marked)
			PlaceEntry(m_Marked);
	}

	// Platzieren: in the hand, search closed, the text goes back to the bar.
	void PlaceEntry(DZS_CatEntry e)
	{
		if (!e)
			return;
		m_P.CloseFull(true);
		m_P.TakeInHand(e);
	}

	int TileSize()
	{
		float s = m_P.Kernel().GetSettings().GetNumber("catalog.tileSize", 128);
		if (s != 96 && s != 192)
			s = 128;
		return s;
	}

	bool GridWheel(int delta)
	{
		if (DZS_UiRegistry.ModDown("Ctrl"))
		{
			array<int> sizes = {96, 128, 192};
			int i = sizes.Find(TileSize());
			if (delta > 0)
				i = Math.Min(2, i + 1);
			else
				i = Math.Max(0, i - 1);
			m_P.Kernel().GetSettings().Set("catalog.tileSize", DZS_Json.Int(sizes[i]));
			DZS_Log.Info("catalog: tile size " + sizes[i]);
			return true;
		}
		m_Scroll -= delta * DZS_Scale.Px(TileSize() * 0.75 + 42 + GAP);
		return true;
	}

	protected void Recompute()
	{
		int t0 = TickCount(0);
		m_All = m_P.m_Store.Search(m_Text, m_P.m_Store.m_All, false);
		m_Counts.Clear();
		m_Counts.Set("alle", m_All.m_Items.Count());
		int favs = 0;
		map<DZS_CatEntry, bool> hit = new map<DZS_CatEntry, bool>();
		foreach (DZS_CatEntry e : m_All.m_Items)
		{
			hit.Set(e, true);
			m_Counts.Set(e.m_Cat, m_Counts.Get(e.m_Cat) + 1);
			if (m_P.IsFavourite(e))
				favs++;
		}
		m_Counts.Set("favoriten", favs);
		array<DZS_CatEntry> recent = m_P.RecentEntries(DZS_CatalogPlugin.RECENT_MAX);
		array<DZS_CatEntry> recentHits = new array<DZS_CatEntry>();
		foreach (DZS_CatEntry r : recent)
		{
			if (hit.Contains(r))
				recentHits.Insert(r);
		}
		m_Counts.Set("zuletzt", recentHits.Count());
		m_Shown.Clear();
		if (m_Filter == "zuletzt")
		{
			foreach (DZS_CatEntry rh : recentHits)
				m_Shown.Insert(rh);
		}
		else
		{
			foreach (DZS_CatEntry a : m_All.m_Items)
			{
				if (m_Filter == "alle" || (m_Filter == "favoriten" && m_P.IsFavourite(a)) || a.m_Cat == m_Filter)
					m_Shown.Insert(a);
			}
		}
		m_Marked = null;
		if (m_Shown.Count() > 0)
			m_Marked = m_Shown[0];
		m_Scroll = 0;
		m_Dirty = false;
		DZS_Log.Info("catalog: full-screen search '" + m_Text + "' in " + m_Filter + ": " + m_Shown.Count() + " of " + m_All.m_Items.Count() + " in " + (TickCount(0) - t0) / 10000.0 + " ms");
	}

	protected bool KeyEdge(string key)
	{
		bool down = DZS_UiRegistry.KeyDown(key);
		bool was = m_KeyWas.Get(key);
		m_KeyWas.Set(key, down);
		return down && !was;
	}

	protected void Keys()
	{
		DZS_Screen scr = m_P.Kernel().GetScreen();
		bool up = KeyEdge("UP");
		bool down = KeyEdge("DOWN");
		bool left = KeyEdge("LEFT");
		bool right = KeyEdge("RIGHT");
		bool enter = KeyEdge("RETURN") || KeyEdge("NUMPADENTER");
		if (scr.IsMenuOpen() || scr.HasDialog())
			return;
		int n = m_Shown.Count();
		if (n == 0 || !m_Marked)
		{
			return;
		}
		int i = m_Shown.Find(m_Marked);
		int cols = Math.Max(1, m_Cols);
		if (right && i < n - 1)
			i++;
		if (left && i > 0)
			i--;
		if (down)
		{
			if (i + cols < n)
				i += cols;
			else if ((i / cols) < ((n - 1) / cols))
				i = n - 1;
		}
		if (up && i - cols >= 0)
			i -= cols;
		if (m_Shown[i] != m_Marked)
		{
			m_Marked = m_Shown[i];
			ScrollToMarked();
		}
		if (enter)
			PlaceEntry(m_Marked);
	}

	protected void ScrollToMarked()
	{
		int i = m_Shown.Find(m_Marked);
		if (i < 0)
			return;
		float cardH = DZS_Scale.Px(TileSize()) * 0.75 + DZS_Scale.Px(42);
		float rowH = cardH + DZS_Scale.Px(GAP);
		float top = (i / Math.Max(1, m_Cols)) * rowH;
		float gw, gh;
		m_Grid.m_W.GetSize(gw, gh);
		if (top < m_Scroll)
			m_Scroll = top;
		else if (top + cardH > m_Scroll + gh)
			m_Scroll = top + cardH - gh;
	}

	void Update(float dt)
	{
		if (!m_Open)
			return;
		DZS_Screen scr = m_P.Kernel().GetScreen();
		if (!scr.IsMenuOpen() && !scr.HasDialog() && GetFocus() != m_Edit)
			SetFocus(m_Edit);
		string t = m_Edit.GetText();
		if (t != m_Text)
		{
			m_Text = t;
			m_Dirty = true;
		}
		if (m_Dirty && m_P.m_Store.IsReady())
			Recompute();
		if (!m_All && !m_P.m_Store.IsReady())
			m_Shown.Clear();
		Keys();
		Layout(dt);
	}

	protected void Layout(float dt)
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		float top = DZS_Scale.Px(44);
		float bottom = sh - DZS_Scale.Px(28);
		float h = bottom - top;
		m_Root.Place(0, top, sw, h);
		DZS_W.Place(m_Bg, 0, 0, sw, h);
		DZS_W.Place(m_Bg2, 0, 0, sw, h);
		float pad = DZS_Scale.Px(16);
		// top line: big field, hits, "Esc schließen"
		float fh = DZS_Scale.Px(40);
		float fw = Math.Min(sw * 0.5, DZS_Scale.Px(720));
		DZS_W.Place(m_FieldBg, pad, pad, fw, fh);
		m_FieldBorder.Place(pad, pad, fw, fh, Math.Max(1, DZS_Scale.Px(2)));
		float ic = DZS_Scale.Px(18);
		DZS_W.Place(m_Lens, pad + DZS_Scale.Px(10), pad + (fh - ic) / 2, ic, ic);
		int eh = DZS_Font.WidgetHeight(DZS_Font.READ, 15);
		DZS_W.Place(m_Edit, pad + DZS_Scale.Px(36), pad + (fh - eh) / 2, fw - DZS_Scale.Px(44), eh);
		int hits = m_Shown.Count();
		m_Hits.SetText(DZS_Texts.Fmt("catalog.full.hits", hits.ToString()));
		DZS_W.Place(m_Hits, pad + fw + DZS_Scale.Px(16), pad, DZS_Scale.Px(200), fh);
		m_EscText.Place(sw - pad - m_EscText.GetWidth(), pad + (fh - m_EscText.GetHeight()) / 2);
		float y0 = pad + fh + pad;
		float colH = h - y0 - pad;
		// filter list
		float fx = pad;
		float fwid = DZS_Scale.Px(FILTER_W);
		float y = y0;
		DZS_W.Place(m_RangeHead, fx, y, fwid, DZS_Scale.Px(18));
		y += DZS_Scale.Px(22);
		bool hasTemplates = m_P.m_Store.m_Templates.Count() > 0;
		for (int i = 0; i < m_Filters.Count(); i++)
		{
			DZS_CatFilterRow fr = m_Filters[i];
			if (i == 3)
			{
				y += DZS_Scale.Px(10);
				DZS_W.Place(m_KindHead, fx, y, fwid, DZS_Scale.Px(18));
				y += DZS_Scale.Px(22);
			}
			bool show = fr.m_Key != "vorlagen" || hasTemplates;
			fr.Show(show);
			if (!show)
				continue;
			fr.Place(fx, y, fwid, m_Counts.Get(fr.m_Key), fr.m_Key == m_Filter);
			y += DZS_Scale.Px(30);
		}
		// details
		float dw = DZS_Scale.Px(DETAIL_W);
		float dx = sw - pad - dw;
		LayoutDetails(dx, y0, dw, colH, dt);
		// grid
		float gx = fx + fwid + pad;
		float gw = dx - pad - gx;
		m_Grid.Place(gx, y0, gw, colH);
		LayoutGrid(gw, colH, dt);
	}

	protected void LayoutGrid(float gw, float gh, float dt)
	{
		float ts = DZS_Scale.Px(TileSize());
		float gap = DZS_Scale.Px(GAP);
		m_Cols = Math.Max(1, Math.Floor((gw + gap) / (ts + gap)));
		float cardH = ts * 0.75 + DZS_Scale.Px(42);
		float rowH = cardH + gap;
		int n = m_Shown.Count();
		int rows = (n + m_Cols - 1) / m_Cols;
		float total = rows * rowH;
		m_Scroll = Math.Clamp(m_Scroll, 0, Math.Max(0, total - gh));
		int firstRow = Math.Floor(m_Scroll / rowH);
		int visRows = Math.Ceil(gh / rowH) + 1;
		int need = visRows * m_Cols;
		while (m_Cards.Count() < need)
			m_Cards.Insert(new DZS_CatTile(m_Grid.m_W, "card", m_TileOwner));
		bool empty = n == 0;
		m_Empty.Show(empty);
		if (empty)
		{
			string icon, title, text, b1, b2;
			bool loading;
			if (!m_P.m_Store.IsReady())
			{
				icon = "search";
				title = DZS_Texts.Get("catalog.loading.title");
				text = DZS_Texts.Get("catalog.loading.text");
				loading = true;
			}
			else if (m_Filter == "favoriten" && m_Text == "")
			{
				icon = "star";
				title = DZS_Texts.Get("catalog.nofav.title");
				text = DZS_Texts.Get("catalog.nofav.text");
			}
			else
			{
				icon = "search";
				title = DZS_Texts.Get("catalog.none.title");
				text = DZS_Texts.Fmt("catalog.none.text", m_Text);
				b1 = DZS_Texts.Get("catalog.none.clear");
				if (m_Filter != "alle")
					b2 = DZS_Texts.Get("catalog.none.all");
			}
			m_Empty.Set(icon, title, text, b1, b2, loading);
			float ew = Math.Min(gw, DZS_Scale.Px(420));
			m_Empty.Place((gw - ew) / 2, DZS_Scale.Px(40), ew);
			m_Empty.Update(dt);
		}
		for (int k = 0; k < m_Cards.Count(); k++)
		{
			DZS_CatTile c = m_Cards[k];
			int idx = firstRow * m_Cols + k;
			if (empty || k >= need || idx >= n)
			{
				c.Show(false);
				continue;
			}
			DZS_CatEntry e = m_Shown[idx];
			int row = idx / m_Cols;
			int col = idx % m_Cols;
			c.Show(true);
			c.Bind(e);
			c.m_Marked = e == m_Marked;
			c.m_InHand = m_P.IsInHand(e);
			c.SetPhoto(m_P.PhotoOf(e));
			c.Place(col * (ts + gap), row * rowH - m_Scroll, ts);
			c.Refresh(m_P.SubText(e), dt);
			int at = -1;
			if (m_All && m_All.m_HitAt.Contains(e))
				at = m_All.m_HitAt.Get(e);
			c.SetHitMark(at, m_All.m_HitLen);
		}
		bool bar = total > gh && !empty;
		m_ScrollBar.Show(bar);
		if (bar)
		{
			float gx, gy;
			m_Grid.m_W.GetPos(gx, gy);
			float bh = Math.Max(DZS_Scale.Px(24), gh * gh / total);
			float by = (gh - bh) * m_Scroll / (total - gh);
			DZS_W.Place(m_ScrollBar, gx + gw - DZS_Scale.Px(4), gy + by, DZS_Scale.Px(4), bh);
		}
	}

	// Size of the model in metres (width × depth × height) from its bounding box.
	static string SizeText(EntityAI ent)
	{
		if (!ent)
			return "–";
		vector mm[2];
		if (!ent.GetCollisionBox(mm))
			ent.ClippingInfo(mm);
		vector d = mm[1] - mm[0];
		return DZS_Num.Fixed(d[0], 2) + " × " + DZS_Num.Fixed(d[2], 2) + " × " + DZS_Num.Fixed(d[1], 2) + " m";
	}

	// Objects of the open project with exactly this name (case does not matter), by group, largest first.
	string ProjectText(DZS_CatEntry e)
	{
		DZS_Instance inst = m_P.Kernel().GetShown();
		if (!inst)
			return DZS_Texts.Get("catalog.fact.unused");
		array<string> names = new array<string>();
		array<int> counts = new array<int>();
		array<string> groups = inst.GetGroupIds();
		foreach (string g : groups)
		{
			int c = 0;
			array<string> kids = inst.GetChildren(g);
			foreach (string id : kids)
			{
				string n;
				if (inst.GetField(id, DZS_Obj.NAME, n))
				{
					n.ToLower();
					if (n == e.m_Low)
						c++;
				}
			}
			if (c == 0)
				continue;
			string gname;
			inst.GetField(g, DZS_Obj.GROUP_NAME, gname);
			int at = 0;
			while (at < counts.Count() && counts[at] >= c)
				at++;
			counts.InsertAt(c, at);
			names.InsertAt(gname, at);
		}
		if (counts.Count() == 0)
			return DZS_Texts.Get("catalog.fact.unused");
		string r = "";
		for (int i = 0; i < Math.Min(3, counts.Count()); i++)
		{
			if (i > 0)
				r += ", ";
			r += DZS_Texts.Fmt("catalog.fact.inGroup", counts[i].ToString(), names[i]);
		}
		if (counts.Count() > 3)
			r += ", " + DZS_Texts.Fmt("catalog.fact.moreGroups", (counts.Count() - 3).ToString());
		return r;
	}

	protected void LayoutDetails(float x, float y, float w, float h, float dt)
	{
		DZS_CatEntry e = m_Marked;
		bool any = e != null;
		m_DetailImgHit.m_W.Show(any);
		m_DetailName.Show(any);
		m_CodeBg.Show(any && !e.m_IsTemplate);
		m_DetailClass.Show(any && !e.m_IsTemplate);
		m_Place.SetEnabled(any, "");
		foreach (TextWidget dl : m_DescLines)
			dl.Show(false);
		for (int f = 0; f < 3; f++)
		{
			m_FactLabels[f].Show(false);
			m_FactValues[f].Show(false);
			m_FactChecks[f].Show(false);
		}
		m_Place.Place(x, y + h - m_Place.GetHeight());
		if (!any)
		{
			m_DetailFor = null;
			return;
		}
		float ih = w * 0.75;
		m_DetailImgHit.Place(x, y, w, ih);
		DZS_W.Place(m_DetailImgBg, 0, 0, w, ih);
		DZS_W.Place(m_DetailPhoto, 0, 0, w, ih);
		float ps = ih * 0.4;
		DZS_W.Place(m_DetailPlace, (w - ps) / 2, (ih - ps) / 2, ps, ps);
		string photo = m_P.PhotoOf(e);
		if (m_DetailFor != e)
		{
			m_DetailFor = e;
			m_DetailEntity = null;
			if (photo != "")
			{
				m_DetailPhoto.LoadImageFile(0, photo);
				m_DetailPhoto.SetImage(0);
			}
		}
		if (!m_DetailEntity)
			m_DetailEntity = m_P.m_Previews.Get(e);
		m_DetailPhoto.Show(photo != "");
		m_DetailPlace.Show(photo == "");
		DZS_W.SetIcon(m_DetailPlace, DZS_CatalogPlugin.CategoryIcon(e.m_Cat));
		float cy = y + ih + DZS_Scale.Px(10);
		DZS_W.FitText(m_DetailName, e.m_Name, w);
		DZS_W.Place(m_DetailName, x, cy, w, DZS_Scale.Px(30));
		cy += DZS_Scale.Px(34);
		if (!e.m_IsTemplate)
		{
			m_DetailClass.SetText(DZS_W.Literal(e.m_Key));
			float cw = Math.Min(w, DZS_W.TextWidth(m_DetailClass) + DZS_Scale.Px(12));
			DZS_W.Place(m_CodeBg, x, cy, cw, DZS_Scale.Px(22));
			DZS_W.Place(m_DetailClass, x + DZS_Scale.Px(6), cy, w - DZS_Scale.Px(12), DZS_Scale.Px(22));
			cy += DZS_Scale.Px(30);
		}
		string desc = m_P.DescriptionOf(e);
		if (desc != "")
		{
			array<string> lines = DZS_CatEmpty.Wrap(m_Root.m_W, desc, w);
			while (m_DescLines.Count() < lines.Count())
				m_DescLines.Insert(DZS_W.Text(m_Root.m_W, DZS_Font.READ, 13, DZS_Color.Get("textMuted")));
			for (int li = 0; li < lines.Count(); li++)
			{
				m_DescLines[li].Show(true);
				m_DescLines[li].SetText(DZS_W.Literal(lines[li]));
				DZS_W.Place(m_DescLines[li], x, cy, w, DZS_Scale.Px(18));
				cy += DZS_Scale.Px(18);
			}
			cy += DZS_Scale.Px(8);
		}
		array<string> labels = new array<string>();
		array<string> values = new array<string>();
		labels.Insert(DZS_Texts.Get("catalog.fact.size"));
		if (e.m_IsTemplate)
			values.Insert("–");
		else
			values.Insert(SizeText(m_DetailEntity));
		labels.Insert(DZS_Texts.Get("catalog.fact.console"));
		if (e.m_IsTemplate)
		{
			if (e.m_TplMissing > 0)
				values.Insert(DZS_Texts.Fmt("catalog.fact.notInCatalog", e.m_TplMissing.ToString(), e.m_TplCount.ToString()));
			else
				values.Insert(DZS_Texts.Fmt("catalog.fact.allSpawn", e.m_TplCount.ToString()));
			labels.Insert(DZS_Texts.Get("catalog.fact.content"));
			values.Insert(DZS_CatalogPlugin.ObjectsText(e.m_TplCount));
		}
		else
		{
			if (e.m_Cat == "licht")
				values.Insert(DZS_Texts.Get("catalog.fact.light"));
			else
				values.Insert(DZS_Texts.Get("catalog.fact.spawns"));
			labels.Insert(DZS_Texts.Get("catalog.fact.project"));
			if (m_ProjectFor != e || m_ProjectStamp != m_P.ProjectStamp())
			{
				m_ProjectFor = e;
				m_ProjectStamp = m_P.ProjectStamp();
				m_ProjectText = ProjectText(e);
			}
			values.Insert(m_ProjectText);
		}
		for (int k = 0; k < labels.Count(); k++)
		{
			m_FactLabels[k].Show(true);
			m_FactValues[k].Show(true);
			m_FactLabels[k].SetText(DZS_Texts.Upper(labels[k]));
			DZS_W.Place(m_FactLabels[k], x, cy, w, DZS_Scale.Px(14));
			string val = values[k];
			int mark = val.IndexOf(" ✓");
			string after = "";
			if (mark >= 0)
			{
				after = val.Substring(mark + 4, val.Length() - mark - 4);
				val = val.Substring(0, mark);
			}
			DZS_W.FitText(m_FactValues[k], val, w);
			DZS_W.Place(m_FactValues[k], x, cy + DZS_Scale.Px(16), w, DZS_Scale.Px(20));
			if (mark >= 0)
			{
				// the reading font has no ✓: the check symbol stands in for it
				float vw = DZS_W.TextWidth(m_FactValues[k]);
				float cs = DZS_Scale.Px(14);
				m_FactChecks[k].Show(true);
				DZS_W.Place(m_FactChecks[k], x + vw + DZS_Scale.Px(4), cy + DZS_Scale.Px(19), cs, cs);
				if (after != "")
				{
					DZS_W.FitText(m_FactValues[k], val + "      " + after, w);
					DZS_W.Place(m_FactValues[k], x, cy + DZS_Scale.Px(16), w, DZS_Scale.Px(20));
				}
			}
			cy += DZS_Scale.Px(42);
		}
	}
}

// The list of a layer in the right bar (spawn points, effect areas, zones): list rows with colour swatch, title and a
// muted detail; only the rows in view are built. A click selects, a double click flies the camera there. A source may
// group its rows under header rows (chevron, bold title, count) and hears which row the pointer is over.
class DZS_LayerListSource : Managed
{
	// changes whenever rows or the selection change
	int Gen() { return 0; }
	int RowCount() { return 0; }
	string RowKey(int i) { return ""; }
	string RowTitle(int i) { return ""; }
	string RowSub(int i) { return ""; }
	int RowColor(int i) { return 0; }
	string SelectedKey() { return ""; }
	void SelectRow(string key) {}
	void FocusRow(string key) {}
	string EmptyText() { return ""; }
	bool RowHeader(int i) { return false; }
	bool RowOpen(int i) { return false; }
	// the row under the pointer ("" for none), every frame
	void HoverRow(string key) {}
}

class DZS_LayerRow : DZS_HitListener
{
	DZS_LayerList m_List;
	ref DZS_Hit m_Hit;
	protected Widget m_Bg;
	protected Widget m_Tint;
	protected Widget m_Bar;
	protected Widget m_Swatch;
	protected TextWidget m_Title;
	protected TextWidget m_Sub;
	protected TextWidget m_Head;
	protected TextWidget m_Count;
	protected ImageWidget m_Chevron;
	protected bool m_Header;
	string m_Key;
	protected string m_Shown;
	protected float m_Last = -10;

	void DZS_LayerRow(Widget parent, DZS_LayerList list)
	{
		m_List = list;
		m_Hit = new DZS_Hit(parent, this, "layerrow");
		m_Hit.m_FocusKind = "";
		Widget f = m_Hit.m_W;
		m_Bg = DZS_W.Panel(f, DZS_Color.Get("surfaceRaised"));
		m_Tint = DZS_W.Panel(f, DZS_Color.Get("accentTint"));
		m_Bar = DZS_W.Panel(f, DZS_Color.Get("accent"));
		m_Swatch = DZS_W.Panel(f, DZS_Color.Get("accent"));
		m_Title = DZS_W.Text(f, DZS_Font.READ, 13, DZS_Color.Get("text"));
		m_Sub = DZS_W.Text(f, DZS_Font.MONO, 11, DZS_Color.Get("textQuiet"));
		m_Head = DZS_W.Text(f, DZS_Font.READBOLD, 13, DZS_Color.Get("text"));
		m_Count = DZS_W.Text(f, DZS_Font.MONO, 11, DZS_Color.Get("textQuiet"));
		m_Chevron = DZS_W.Icon(f, "chevron-right", DZS_Color.Get("textMuted"));
	}

	bool IsHover() { return m_Hit.m_Hover; }

	void Bind(string key, string title, string sub, int color, float w, bool header, bool open)
	{
		m_Key = key;
		m_Header = header;
		string sig = key + "|" + title + "|" + sub + "|" + DZS_Str.Bool(header) + "|" + DZS_Str.Bool(open);
		if (sig != m_Shown)
		{
			m_Shown = sig;
			if (header)
			{
				m_Count.SetText(DZS_W.Literal(sub));
				DZS_W.FitText(m_Head, title, w - DZS_Scale.Px(44) - DZS_W.TextWidth(m_Count) - DZS_Scale.Px(12));
				if (open)
					DZS_W.SetIcon(m_Chevron, "chevron-down");
				else
					DZS_W.SetIcon(m_Chevron, "chevron-right");
			}
			else
			{
				DZS_W.FitText(m_Title, title, w - DZS_Scale.Px(44));
				DZS_W.FitText(m_Sub, sub, w - DZS_Scale.Px(44));
			}
		}
		m_Swatch.SetColor(color);
		m_Title.Show(!header);
		m_Sub.Show(!header);
		m_Swatch.Show(!header);
		m_Head.Show(header);
		m_Count.Show(header);
		m_Chevron.Show(header);
	}

	void Place(float y, float w, float h, bool selected)
	{
		m_Hit.Place(0, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		DZS_W.Place(m_Tint, 0, 0, w, h);
		DZS_W.Place(m_Bar, 0, 0, DZS_Scale.Px(3), h);
		float sw = DZS_Scale.Px(8);
		float pad = DZS_Scale.Px(12);
		DZS_W.Place(m_Swatch, pad, DZS_Scale.Px(10), sw, sw);
		float tx = pad + sw + DZS_Scale.Px(10);
		DZS_W.Place(m_Title, tx, DZS_Scale.Px(3), w - tx - pad, DZS_Scale.Px(18));
		DZS_W.Place(m_Sub, tx, DZS_Scale.Px(20), w - tx - pad, DZS_Scale.Px(16));
		float ic = DZS_Scale.Px(16);
		DZS_W.Place(m_Chevron, pad - DZS_Scale.Px(4), (h - ic) / 2, ic, ic);
		float cw = DZS_W.TextWidth(m_Count);
		DZS_W.Place(m_Count, w - pad - cw, 0, cw, h);
		DZS_W.Place(m_Head, tx, 0, w - tx - pad - cw - DZS_Scale.Px(8), h);
		m_Bg.Show(m_Hit.m_Hover && !selected);
		m_Tint.Show(selected);
		m_Bar.Show(selected);
	}

	void Show(bool s) { m_Hit.m_W.Show(s); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button != MouseState.LEFT)
			return;
		float now = GetGame().GetTickTime();
		bool dbl = now - m_Last < 0.4;
		m_Last = now;
		m_List.OnRow(m_Key, dbl);
	}

	void Destroy() { m_Hit.Destroy(); }
}

class DZS_LayerList : DZS_PanelContent
{
	protected DZS_LayerListSource m_Src;
	protected Widget m_Root;
	protected ref array<ref DZS_LayerRow> m_Rows = new array<ref DZS_LayerRow>();
	protected TextWidget m_Empty;
	protected Widget m_ScrollBar;
	protected float m_W;
	protected float m_H;
	protected float m_Scroll;
	protected int m_SeenGen = -1;
	protected string m_SeenSel;

	void DZS_LayerList(DZS_LayerListSource src)
	{
		m_Src = src;
	}

	override bool FillsView() { return true; }

	override void Build(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_Empty = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_ScrollBar = DZS_W.Panel(m_Root, DZS_Color.Get("lineStrong"));
	}

	override void SetView(float width, float height)
	{
		m_W = width;
		m_H = height;
	}

	protected float RowH() { return DZS_Scale.Px(40); }

	override bool Wheel(int delta)
	{
		m_Scroll -= delta * RowH() * 3;
		Clamp();
		return true;
	}

	protected void Clamp()
	{
		float total = m_Src.RowCount() * RowH();
		m_Scroll = Math.Clamp(m_Scroll, 0, Math.Max(0, total - m_H));
	}

	// A selection made in the scene rolls the list to its row.
	protected void Follow()
	{
		int gen = m_Src.Gen();
		if (gen == m_SeenGen)
			return;
		m_SeenGen = gen;
		string sel = m_Src.SelectedKey();
		if (sel == m_SeenSel)
			return;
		if (sel == "")
		{
			m_SeenSel = sel;
			return;
		}
		int n = m_Src.RowCount();
		for (int i = 0; i < n; i++)
		{
			if (m_Src.RowKey(i) != sel)
				continue;
			m_SeenSel = sel;
			float top = i * RowH();
			if (top < m_Scroll)
				m_Scroll = top;
			else if (top + RowH() > m_Scroll + m_H)
				m_Scroll = top + RowH() - m_H;
			return;
		}
	}

	void OnRow(string key, bool dbl)
	{
		m_SeenSel = key;
		m_Src.SelectRow(key);
		if (dbl)
			m_Src.FocusRow(key);
	}

	override void Update(float dt)
	{
		if (!m_Root || m_W <= 0)
			return;
		Follow();
		Clamp();
		DZS_W.Place(m_Root, 0, 0, m_W, m_H);
		int n = m_Src.RowCount();
		m_Empty.Show(n == 0);
		if (n == 0)
		{
			m_Empty.SetText(DZS_W.Literal(m_Src.EmptyText()));
			DZS_W.Place(m_Empty, DZS_Scale.Px(12), DZS_Scale.Px(8), m_W - DZS_Scale.Px(24), DZS_Scale.Px(20));
		}
		float rowH = RowH();
		int first = Math.Floor(m_Scroll / rowH);
		int visible = Math.Ceil(m_H / rowH) + 1;
		while (m_Rows.Count() < visible)
			m_Rows.Insert(new DZS_LayerRow(m_Root, this));
		string sel = m_Src.SelectedKey();
		string hover = "";
		for (int k = 0; k < m_Rows.Count(); k++)
		{
			DZS_LayerRow row = m_Rows[k];
			int idx = first + k;
			if (k >= visible || idx >= n)
			{
				row.Show(false);
				continue;
			}
			row.Show(true);
			string key = m_Src.RowKey(idx);
			row.Bind(key, m_Src.RowTitle(idx), m_Src.RowSub(idx), m_Src.RowColor(idx), m_W, m_Src.RowHeader(idx), m_Src.RowOpen(idx));
			row.Place(idx * rowH - m_Scroll, m_W, rowH, key == sel);
			if (row.IsHover())
				hover = key;
		}
		m_Src.HoverRow(hover);
		float total = n * rowH;
		bool bar = total > m_H;
		m_ScrollBar.Show(bar);
		if (bar)
		{
			float bh = Math.Max(DZS_Scale.Px(24), m_H * m_H / total);
			float by = (m_H - bh) * m_Scroll / (total - m_H);
			DZS_W.Place(m_ScrollBar, m_W - DZS_Scale.Px(4), by, DZS_Scale.Px(4), bh);
		}
	}
}

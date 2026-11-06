// The editor's main menu. Widgets cannot be rotated in the game,
// so each strip is drawn from horizontal rows that follow the slant (a pixel staircase of 1 px per 5 px); each
// row shows its slice of the world's paper map (CfgWorlds mapTextureOpened) or, if that does not load,
// the neutral gradient. The name is an image rendered at build time (assets/build_assets.py mapnames).
class DZS_HomeStrip : Managed
{
	string m_Key;
	bool m_Available;
	float m_X;
	float m_W;
	float m_FromW;
	float m_ToW;
	ref array<Widget> m_Rows;
	ref array<Widget> m_EdgeL;
	ref array<Widget> m_EdgeR;
	Widget m_Top;
	Widget m_Bottom;
	ImageWidget m_Name;
	ref array<Widget> m_Clips;
	ref array<ImageWidget> m_Maps;
	string m_PaperMap;
	int m_PaperW;
	int m_PaperH;
	Widget m_Frame;
}

class DZS_HomeMenu : DZS_HitListener
{
	static const float ROW = 5;
	static const float COLLAPSED = 96;
	static const float GAP = 4;
	static const float GLIDE = 0.15;
	static const float FADE = 0.1;
	static const float HOVER_WAIT = 0.15;
	static const float NAME_IMAGE = 128;

	protected DZS_ProjectsPlugin m_P;
	protected Widget m_Root;
	protected Widget m_Bg;
	protected Widget m_TopBg;
	protected Widget m_TopLine;
	protected TextWidget m_Brand;
	protected ref DZS_Button m_Settings;
	protected ref DZS_Button m_Quit;
	protected ref array<ref DZS_Button> m_Extra = new array<ref DZS_Button>();
	protected ref DZS_Hit m_Area;
	protected Widget m_StripLayer;
	protected TextWidget m_Version;
	protected ref array<ref DZS_HomeStrip> m_Strips;
	protected int m_Open = 0;
	protected int m_Hover = -1;
	protected float m_HoverAge;
	protected float m_Glide = 1;
	protected float m_FadeAge = 1;
	protected bool m_Pressed;
	protected int m_PressStrip = -1;
	protected bool m_Shown;
	// plate of the open strip
	protected Widget m_Plate;
	protected Widget m_PlateBg;
	protected ref DZS_Border m_PlateBorder;
	protected TextWidget m_PlateName;
	protected TextWidget m_F1L;
	protected TextWidget m_F1V;
	protected TextWidget m_F2L;
	protected TextWidget m_F2V;
	protected ref array<ref DZS_ListRow> m_Rows;
	protected ref array<ref DZS_ProjectInfo> m_RowInfo;
	protected ref DZS_Button m_New;
	protected ref DZS_Button m_All;
	protected ref DZS_Button m_Import;
	protected ImageWidget m_EmptyFrame;
	protected ImageWidget m_EmptyIcon;
	protected TextWidget m_EmptyText;
	protected TextWidget m_Missing;
	protected int m_PlateFor = -1;
	// keyboard: -1 strip, 0.. rows then buttons, 100 settings, 101 quit
	protected int m_Focus = -1;
	protected bool m_StripFocus = true;
	protected ref map<string, bool> m_KeyWas;
	protected float m_Top;
	protected float m_H;
	protected float m_Slant;

	void DZS_HomeMenu(DZS_ProjectsPlugin p)
	{
		m_P = p;
		m_Strips = new array<ref DZS_HomeStrip>();
		m_Rows = new array<ref DZS_ListRow>();
		m_RowInfo = new array<ref DZS_ProjectInfo>();
		m_KeyWas = new map<string, bool>();
	}

	string Chosen()
	{
		if (m_Strips.Count() == 0)
			return m_P.LastMap();
		return m_Strips[m_Open].m_Key;
	}

	bool IsShown() { return m_Shown; }

	void Open(string mapKey)
	{
		if (!m_Root)
			Build();
		m_Shown = true;
		m_Root.Show(true);
		for (int i = 0; i < m_Strips.Count(); i++)
		{
			if (m_Strips[i].m_Key == mapKey)
				m_Open = i;
		}
		m_Glide = 1;
		m_FadeAge = 1;
		foreach (DZS_HomeStrip s : m_Strips)
			s.m_Available = DZS_Maps.Available(s.m_Key);
		m_PlateFor = -1;
		m_Focus = -1;
		m_StripFocus = true;
		Layout(true);
		DZS_Log.Info("main menu open, strip " + Chosen() + ", height " + m_H + ", slant " + m_Slant);
		foreach (DZS_HomeStrip ls : m_Strips)
			DZS_Log.Info("main menu strip " + ls.m_Key + " x " + ls.m_X + " w " + ls.m_W + " available " + ls.m_Available);
	}

	void Close()
	{
		m_Shown = false;
		if (m_Root)
			m_Root.Show(false);
	}

	void Destroy()
	{
		if (m_Root)
			m_Root.Unlink();
		m_Root = null;
	}

	protected void Build()
	{
		Widget layer = m_P.Kernel().GetScreen().GetHomeLayer();
		m_Root = DZS_W.Frame(layer);
		m_Bg = DZS_W.Panel(m_Root, ARGB(255, 0x18, 0x18, 0x1B));
		m_Area = new DZS_Hit(m_Root, this, "home.area");
		m_Area.m_FocusKind = "";
		m_StripLayer = DZS_W.Frame(m_Area.m_W);
		array<string> mapKeys = DZS_Maps.Keys();
		foreach (string key : mapKeys)
		{
			DZS_HomeStrip s = new DZS_HomeStrip();
			s.m_Key = key;
			s.m_Rows = new array<Widget>();
			s.m_EdgeL = new array<Widget>();
			s.m_EdgeR = new array<Widget>();
			s.m_Clips = new array<Widget>();
			s.m_Maps = new array<ImageWidget>();
			s.m_Frame = DZS_W.Frame(m_StripLayer);
			LoadPaper(s);
			m_Strips.Insert(s);
		}
		m_TopBg = DZS_W.Panel(m_Root, DZS_Color.Get("surface"));
		m_TopLine = DZS_W.Panel(m_Root, DZS_Color.Get("line"));
		m_Brand = DZS_W.Text(m_Root, DZS_Font.DISPLAY, 24, DZS_Color.Get("text"));
		m_Brand.SetText(DZS_Texts.Get("core.brand"));
		m_Version = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"), false, true);
		m_Version.SetText(DZS_W.Literal(DZS_Version.Label()));
		m_Settings = new DZS_Button(m_Root, "side", DZS_Texts.Pick("Einstellungen", "Settings"), "settings", 32);
		m_Settings.m_OnClick.Insert(OnSettings);
		m_Quit = new DZS_Button(m_Root, "side", DZS_Texts.Pick("Beenden", "Quit"), "x", 32);
		m_Quit.m_OnClick.Insert(OnQuit);
		array<DZS_HomeButton> extra = DZS_HomeButtons.All();
		foreach (DZS_HomeButton hb : extra)
		{
			string hl = DZS_Texts.Pick(hb.m_De, hb.m_En);
			DZS_Button xb = new DZS_Button(m_Root, "side", hl, hb.m_Icon, 32);
			xb.m_Tag = hb.m_Action;
			xb.m_OnClick.Insert(OnExtra);
			m_Extra.Insert(xb);
		}
		m_Plate = DZS_W.Frame(m_Root);
	}

	void OnSettings(DZS_Button b)
	{
		m_P.Kernel().GetScreen().OpenSettings();
	}

	void OnExtra(DZS_Button b)
	{
		m_P.Kernel().GetUi().Trigger(b.m_Tag);
	}

	void OnQuit(DZS_Button b)
	{
		m_P.Leave("quit", "", "");
	}

	// ---- geometry ----

	protected float Off(float yInStrip)
	{
		return m_Slant * (1 - yInStrip / m_H);
	}

	protected void Targets(out array<float> widths)
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		float left = DZS_Scale.Px(24);
		float total = sw - 2 * left;
		float collapsed = DZS_Scale.Px(COLLAPSED);
		float gap = DZS_Scale.Px(GAP);
		float open = total - m_Slant - 3 * gap - 3 * collapsed;
		widths = new array<float>();
		for (int i = 0; i < m_Strips.Count(); i++)
		{
			if (i == m_Open)
				widths.Insert(open);
			else
				widths.Insert(collapsed);
		}
	}

	protected void Layout(bool snap)
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Root, 0, 0, sw, sh);
		DZS_W.Place(m_Bg, 0, 0, sw, sh);
		float topH = DZS_Scale.Px(DZS_Screen.TOP_H);
		DZS_W.Place(m_TopBg, 0, 0, sw, topH);
		DZS_W.Place(m_TopLine, 0, topH - 1, sw, 1);
		DZS_W.Place(m_Brand, DZS_Scale.Px(12), (topH - DZS_Scale.Px(30)) / 2, DZS_Scale.Px(300), DZS_Scale.Px(30));
		float bx = sw - DZS_Scale.Px(12) - m_Quit.GetWidth();
		m_Quit.Place(bx, (topH - m_Quit.GetHeight()) / 2);
		bx -= DZS_Scale.Px(8) + m_Settings.GetWidth();
		m_Settings.Place(bx, (topH - m_Settings.GetHeight()) / 2);
		foreach (DZS_Button eb : m_Extra)
		{
			bx -= DZS_Scale.Px(8) + eb.GetWidth();
			eb.Place(bx, (topH - eb.GetHeight()) / 2);
		}
		m_Top = topH + DZS_Scale.Px(24);
		m_H = sh - m_Top - DZS_Scale.Px(24);
		m_Slant = Math.Round(m_H * 0.2);
		m_Area.Place(0, m_Top, sw, m_H);
		float vw = DZS_Scale.Px(300);
		DZS_W.Place(m_Version, sw - DZS_Scale.Px(12) - vw, m_Top + m_H + DZS_Scale.Px(2), vw, DZS_Scale.Px(20));
		DZS_W.Place(m_StripLayer, 0, 0, sw, m_H);
		foreach (DZS_HomeStrip fs : m_Strips)
			DZS_W.Place(fs.m_Frame, 0, 0, sw, m_H);
		array<float> widths;
		Targets(widths);
		for (int i = 0; i < m_Strips.Count(); i++)
		{
			DZS_HomeStrip s = m_Strips[i];
			if (snap)
			{
				s.m_W = widths[i];
				s.m_FromW = widths[i];
			}
			s.m_ToW = widths[i];
		}
		Place();
	}

	// Positions every strip from the current widths.
	protected void Place()
	{
		float x = DZS_Scale.Px(24);
		float gap = DZS_Scale.Px(GAP);
		for (int i = 0; i < m_Strips.Count(); i++)
		{
			DZS_HomeStrip s = m_Strips[i];
			s.m_X = Math.Round(x);
			DrawStrip(i);
			x += s.m_W + gap;
		}
	}

	protected int RowCount()
	{
		return Math.Ceil(m_H / DZS_Scale.Px(ROW));
	}

	// Colour of a row: gradient Fläche erhöht (top) → Fläche (bottom), darkened from the bottom
	// (80 % at the bottom to 0 % at half height; pressed 15 points more); unavailable 60 % darker.
	protected int RowColor(float t, bool pressed, bool available)
	{
		float r = 39.0 + (24.0 - 39.0) * t;
		float g = 39.0 + (24.0 - 39.0) * t;
		float b = 42.0 + (27.0 - 42.0) * t;
		float a = 0;
		if (t > 0.5)
			a = 0.8 * (t - 0.5) / 0.5;
		if (pressed)
			a += 0.15;
		r = r + (24.0 - r) * a;
		g = g + (24.0 - g) * a;
		b = b + (27.0 - b) * a;
		if (!available)
		{
			r *= 0.4;
			g *= 0.4;
			b *= 0.4;
		}
		int ri = r;
		int gi = g;
		int bi = b;
		return ARGB(255, ri, gi, bi);
	}

	// The paper map of the world, loaded once to learn whether it loads and its size; -dzsNoPaper=enoch,…
	// makes a map count as not loadable (proof of the fallback).
	protected void LoadPaper(DZS_HomeStrip s)
	{
		string cli;
		if (GetCLIParam("dzsNoPaper", cli) && cli.Contains(s.m_Key))
			return;
		string path;
		GetGame().ConfigGetText("CfgWorlds " + DZS_Maps.WorldOf(s.m_Key) + " mapTextureOpened", path);
		if (path == "")
			return;
		ImageWidget probe = ImageWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_image.layout", s.m_Frame));
		bool ok = probe.LoadImageFile(0, path);
		int w, h;
		if (ok)
			probe.GetImageSize(0, w, h);
		probe.Unlink();
		if (!ok || w <= 0 || h <= 0)
		{
			DZS_Log.Info("home: paper map of " + s.m_Key + " not loadable (" + path + "), gradient");
			return;
		}
		s.m_PaperMap = path;
		s.m_PaperW = w;
		s.m_PaperH = h;
		DZS_Log.Info("home: paper map " + s.m_Key + " " + path + " " + w + "x" + h);
	}

	// The paper map covers the strip's box without distortion (centre = box centre), seen through each row;
	// the row keeps only the darkening.
	protected void PlacePaper(DZS_HomeStrip s, int k, float rowX, float y, float h, int shade)
	{
		float boxW = s.m_W + m_Slant;
		float f = Math.Max(boxW / s.m_PaperW, m_H / s.m_PaperH);
		float iw = s.m_PaperW * f;
		float ih = s.m_PaperH * f;
		Widget clip = s.m_Clips[k];
		clip.Show(true);
		DZS_W.Place(clip, rowX, y, s.m_W, h);
		DZS_W.Place(s.m_Maps[k], s.m_X + (boxW - iw) / 2 - rowX, (m_H - ih) / 2 - y, iw, ih);
		s.m_Rows[k].SetColor(shade);
	}

	protected int RowShade(float t, bool pressed, bool available)
	{
		float a = 0;
		if (t > 0.5)
			a = 0.8 * (t - 0.5) / 0.5;
		if (pressed)
			a += 0.15;
		if (!available)
			a = 0.6 + 0.4 * a;
		int ai = Math.Clamp(a, 0, 1) * 255;
		return ARGB(ai, 24, 24, 27);
	}

	protected void DrawStrip(int i)
	{
		DZS_HomeStrip s = m_Strips[i];
		int n = RowCount();
		while (s.m_Rows.Count() < n)
		{
			if (s.m_PaperMap != "")
			{
				Widget clip = DZS_W.Make("dzstudio/gui/layouts/dzs_clip.layout", s.m_Frame);
				ImageWidget paper = ImageWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_image.layout", clip));
				paper.LoadImageFile(0, s.m_PaperMap);
				paper.SetImage(0);
				s.m_Clips.Insert(clip);
				s.m_Maps.Insert(paper);
			}
			s.m_Rows.Insert(DZS_W.Panel(s.m_Frame, 0));
			s.m_EdgeL.Insert(DZS_W.Panel(s.m_Frame, DZS_Color.Get("accent")));
			s.m_EdgeR.Insert(DZS_W.Panel(s.m_Frame, DZS_Color.Get("accent")));
		}
		if (!s.m_Top)
		{
			s.m_Top = DZS_W.Panel(s.m_Frame, DZS_Color.Get("accent"));
			s.m_Bottom = DZS_W.Panel(s.m_Frame, DZS_Color.Get("accent"));
		}
		bool pressed = m_Pressed && m_PressStrip == i;
		bool edge = (m_Hover == i) || (m_StripFocus && m_Focus == -1 && m_Open == i && m_P.Kernel().GetScreen().KeyboardTarget() == "mainmenu");
		float rowH = DZS_Scale.Px(ROW);
		float line = Math.Max(2, DZS_Scale.Px(2));
		for (int k = 0; k < s.m_Rows.Count(); k++)
		{
			Widget row = s.m_Rows[k];
			if (k >= n)
			{
				row.Show(false);
				if (s.m_PaperMap != "")
					s.m_Clips[k].Show(false);
				s.m_EdgeL[k].Show(false);
				s.m_EdgeR[k].Show(false);
				continue;
			}
			float y = k * rowH;
			float h = Math.Min(rowH, m_H - y);
			float off = Math.Round(Off(y + h / 2));
			row.Show(true);
			row.SetColor(RowColor((y + h / 2) / m_H, pressed, s.m_Available));
			DZS_W.Place(row, s.m_X + off, y, s.m_W, h);
			if (s.m_PaperMap != "")
				PlacePaper(s, k, s.m_X + off, y, h, RowShade((y + h / 2) / m_H, pressed, s.m_Available));
			s.m_EdgeL[k].Show(edge);
			s.m_EdgeR[k].Show(edge);
			if (edge)
			{
				DZS_W.Place(s.m_EdgeL[k], s.m_X + off, y, line, h);
				DZS_W.Place(s.m_EdgeR[k], s.m_X + off + s.m_W - line, y, line, h);
			}
		}
		s.m_Top.Show(edge);
		s.m_Bottom.Show(edge);
		DZS_W.Place(s.m_Top, s.m_X + Math.Round(Off(0)), 0, s.m_W, line);
		DZS_W.Place(s.m_Bottom, s.m_X, m_H - line, s.m_W, line);
		DrawName(i);
	}

	// The name image (128 reference px square, text in its lower left corner) starts 12 px right of the edge
	// and 24 px above the bottom.
	protected void DrawName(int i)
	{
		DZS_HomeStrip s = m_Strips[i];
		if (!s.m_Name)
			s.m_Name = DZS_W.Icon(s.m_Frame, "mapname-" + s.m_Key, DZS_Color.Get("text"));
		string col = "text";
		if (!s.m_Available)
			col = "textOff";
		else if (i == m_Open)
			col = "accentText";
		s.m_Name.SetColor(DZS_Color.Get(col));
		float size = DZS_Scale.Px(NAME_IMAGE);
		float bottom = m_H - DZS_Scale.Px(24);
		DZS_W.Place(s.m_Name, s.m_X + Off(bottom) + DZS_Scale.Px(12), bottom - size, size, size);
	}

	// ---- hit test on the slanted shapes ----

	int StripAt(int mx, int my)
	{
		if (my < m_Top || my > m_Top + m_H)
			return -1;
		float off = Off(my - m_Top);
		for (int i = 0; i < m_Strips.Count(); i++)
		{
			DZS_HomeStrip s = m_Strips[i];
			if (mx >= s.m_X + off && mx < s.m_X + off + s.m_W)
				return i;
		}
		return -1;
	}

	protected bool OverPlate()
	{
		if (!m_Plate.IsVisible())
			return false;
		Widget under = GetWidgetUnderCursor();
		return under && DZS_W.IsInside(under, m_Plate);
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button != MouseState.LEFT)
			return;
		int mx, my;
		GetMousePos(mx, my);
		int i = StripAt(mx, my);
		if (i < 0 || OverPlate())
			return;
		Expand(i, false);
		Activate(i);
	}

	override void OnHitPress(DZS_Hit h, int button)
	{
		int mx, my;
		GetMousePos(mx, my);
		m_PressStrip = StripAt(mx, my);
		m_Pressed = m_PressStrip >= 0;
	}

	override void OnHitRelease(DZS_Hit h)
	{
		m_Pressed = false;
		m_PressStrip = -1;
	}

	// Click on a strip: the map with its last opened project, else "Neues Projekt" for it.
	void Activate(int i)
	{
		DZS_HomeStrip s = m_Strips[i];
		DZS_Log.Info("main menu: strip " + s.m_Key + " activated");
		if (!s.m_Available)
			return;
		array<ref DZS_ProjectInfo> rec = m_P.Recent(s.m_Key, 1);
		if (rec.Count() > 0)
			m_P.RequestOpen(rec[0].m_Folder);
		else
			m_P.OpenNewDialog(s.m_Key);
	}

	void Expand(int i, bool fromKeys)
	{
		if (i < 0 || i >= m_Strips.Count())
			return;
		if (i == m_Open)
			return;
		foreach (DZS_HomeStrip s : m_Strips)
			s.m_FromW = s.m_W;
		m_Open = i;
		m_Glide = 0;
		m_FadeAge = -1;
		array<float> widths;
		Targets(widths);
		for (int k = 0; k < m_Strips.Count(); k++)
			m_Strips[k].m_ToW = widths[k];
		if (m_Focus < 100 || fromKeys)
		{
			m_Focus = -1;
			m_StripFocus = true;
		}
		m_PlateFor = -1;
		m_Plate.Show(false);
		m_P.Kernel().GetSettings().Set("projects.lastMap", DZS_Json.Str(m_Strips[i].m_Key));
		DZS_Log.Info("main menu: strip " + m_Strips[i].m_Key + " opens");
	}

	// ---- per frame ----

	void Update(float dt)
	{
		if (!m_Shown || !m_Root)
			return;
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_Screen scr = m_P.Kernel().GetScreen();
		bool free = scr.KeyboardTarget() == "mainmenu";
		int mx, my;
		GetMousePos(mx, my);
		int over = -1;
		if (free && !OverPlate())
			over = StripAt(mx, my);
		string tip = "";
		if (over >= 0 && !m_Strips[over].m_Available)
			tip = DZS_Texts.Pick("Karte nicht verfügbar", "Map not available");
		if (m_Area && m_Area.m_Tip != tip)
			m_Area.SetTip(tip);
		if (over != m_Hover)
		{
			m_Hover = over;
			m_HoverAge = 0;
		}
		else
			m_HoverAge += dt;
		if (m_Hover >= 0 && m_Hover != m_Open && m_HoverAge >= HOVER_WAIT)
			Expand(m_Hover, false);
		if (m_Glide < 1)
		{
			m_Glide = Math.Min(1, m_Glide + dt / GLIDE);
			float e = m_Glide;
			foreach (DZS_HomeStrip s : m_Strips)
				s.m_W = Math.Round(s.m_FromW + (s.m_ToW - s.m_FromW) * e);
			if (m_Glide >= 1)
			{
				m_FadeAge = 0;
				DZS_Log.Info("main menu: glide done");
			}
		}
		Place();
		UpdatePlate(dt);
		if (free)
			Keys();
		else
			m_KeyWas.Clear();
	}

	protected bool Edge(string key)
	{
		bool down = DZS_UiRegistry.KeyDown(key);
		bool was = m_KeyWas.Get(key);
		m_KeyWas.Set(key, down);
		return down && !was;
	}

	protected int ItemCount()
	{
		return m_Rows.Count() + 3;
	}

	protected DZS_Hit ItemHit(int idx)
	{
		if (idx == 100)
			return m_Settings.m_Hit;
		if (idx == 101)
			return m_Quit.m_Hit;
		if (idx < 0)
			return null;
		if (idx < m_Rows.Count())
			return m_Rows[idx].m_Hit;
		int b = idx - m_Rows.Count();
		if (b == 0)
			return m_New.m_Hit;
		if (b == 1)
			return m_All.m_Hit;
		if (b == 2)
			return m_Import.m_Hit;
		return null;
	}

	protected void SetFocus(int idx)
	{
		DZS_Hit old = ItemHit(m_Focus);
		if (old)
		{
			old.m_Focus = false;
			old.Changed();
		}
		m_Focus = idx;
		m_StripFocus = idx == -1;
		DZS_Hit now = ItemHit(idx);
		if (now)
		{
			now.m_Focus = true;
			now.Changed();
		}
		DZS_Log.Info("main menu focus " + idx);
	}

	protected void Keys()
	{
		bool left = Edge("LEFT");
		bool right = Edge("RIGHT");
		bool down = Edge("DOWN");
		bool up = Edge("UP");
		bool tab = Edge("TAB");
		bool enter = Edge("RETURN") || Edge("NUMPADENTER");
		bool space = Edge("SPACE");
		bool available = m_Strips[m_Open].m_Available;
		if ((left || right) && m_Focus < 100)
		{
			int to = m_Open - 1;
			if (right)
				to = m_Open + 1;
			if (to >= 0 && to < m_Strips.Count())
			{
				SetFocus(-1);
				Expand(to, true);
			}
			return;
		}
		if (tab)
		{
			bool back = DZS_UiRegistry.ModDown("Shift");
			array<int> order = new array<int>();
			order.Insert(-1);
			if (available && m_PlateFor == m_Open)
			{
				for (int i = 0; i < ItemCount(); i++)
				{
					DZS_Hit ih = ItemHit(i);
					if (ih && ih.m_Enabled)
						order.Insert(i);
				}
			}
			order.Insert(100);
			order.Insert(101);
			int at = order.Find(m_Focus);
			if (back)
				at = (at + order.Count() - 1) % order.Count();
			else
				at = (at + 1) % order.Count();
			SetFocus(order[at]);
			return;
		}
		if ((down || up) && m_Focus < 100 && available && m_PlateFor == m_Open)
		{
			int next = m_Focus;
			if (down)
				next++;
			else
				next--;
			while (next >= 0 && next < ItemCount() && !ItemHit(next).m_Enabled)
			{
				if (down)
					next++;
				else
					next--;
			}
			if (next >= -1 && next < ItemCount())
				SetFocus(next);
			return;
		}
		if (enter || space)
		{
			if (m_Focus == -1)
				Activate(m_Open);
			else
			{
				DZS_Hit h = ItemHit(m_Focus);
				if (h && h.m_Enabled && h.m_Listener)
					h.m_Listener.OnHitActivate(h);
			}
		}
	}

	// ---- plate of the open strip ----

	void RebuildPlate()
	{
		m_PlateFor = -1;
	}

	protected void BuildPlate()
	{
		m_Plate.Unlink();
		foreach (DZS_ListRow r : m_Rows)
			r.Destroy();
		m_Rows.Clear();
		m_RowInfo.Clear();
		m_Plate = DZS_W.Frame(m_Root);
		m_PlateBg = DZS_W.Panel(m_Plate, DZS_Color.Get("plate"));
		m_PlateBorder = new DZS_Border(m_Plate, DZS_Color.Get("lineStrong"));
		DZS_HomeStrip s = m_Strips[m_Open];
		m_PlateName = DZS_W.Text(m_Plate, DZS_Font.DISPLAY, 28, DZS_Color.Get("accentText"));
		m_PlateName.SetText(DZS_Texts.Upper(DZS_Maps.Display(s.m_Key)));
		m_Missing = null;
		m_EmptyText = null;
		m_EmptyFrame = null;
		m_New = null;
		m_All = null;
		m_Import = null;
		if (!s.m_Available)
		{
			m_Missing = DZS_W.Text(m_Plate, DZS_Font.READ, 14, DZS_Color.Get("textMuted"));
			m_Missing.SetText(DZS_Texts.Pick("Karte nicht verfügbar — im Spiel nicht installiert", "Map not available — not installed in the game"));
			m_PlateFor = m_Open;
			return;
		}
		int size = DZS_Maps.Size(s.m_Key);
		array<ref DZS_ProjectInfo> all = DZS_LocalStore.List();
		int count = 0;
		foreach (DZS_ProjectInfo pi : all)
		{
			if (pi.m_MapKey == s.m_Key)
				count++;
		}
		m_F1L = DZS_W.Text(m_Plate, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_F1L.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Größe", "Size")));
		m_F1V = DZS_W.Text(m_Plate, DZS_Font.READ, 14, DZS_Color.Get("text"));
		string st = size.ToString();
		m_F1V.SetText(st + " × " + st + " m");
		m_F2L = DZS_W.Text(m_Plate, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_F2L.SetText(DZS_Texts.Upper(DZS_Texts.Pick("Projekte", "Projects")));
		m_F2V = DZS_W.Text(m_Plate, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_F2V.SetText(count.ToString());
		array<ref DZS_ProjectInfo> rec = m_P.Recent(s.m_Key, 5);
		foreach (DZS_ProjectInfo p : rec)
		{
			string saved = DZS_ProjectsPlugin.ShowStamp(m_P.RecentStamp(p.m_Folder, "saved"));
			string meta = m_P.ProviderLabel(p.m_Folder);
			if (saved != "")
				meta += " · " + DZS_Texts.Pick("gespeichert ", "saved ") + saved;
			DZS_ListRow row = new DZS_ListRow(m_Plate, p.m_Folder, p.m_Name, meta, "");
			row.m_OnClick.Insert(OnRowClick);
			row.m_OnDouble.Insert(OnRowActivate);
			m_Rows.Insert(row);
			m_RowInfo.Insert(p);
		}
		if (rec.Count() == 0)
		{
			m_EmptyFrame = DZS_W.Icon(m_Plate, "octagon", DZS_Color.Get("lineStrong"));
			m_EmptyIcon = DZS_W.Icon(m_Plate, "folder-open", DZS_Color.Get("textMuted"));
			m_EmptyText = DZS_W.Text(m_Plate, DZS_Font.READ, 14, DZS_Color.Get("textMuted"));
			string mapName = DZS_Maps.Display(s.m_Key);
			m_EmptyText.SetText(DZS_Texts.Pick("Noch kein Projekt auf " + mapName + " — leg eins an oder importiere eine .dze", "No project on " + mapName + " yet — create one or import a .dze"));
		}
		m_New = new DZS_Button(m_Plate, "side", DZS_Texts.Pick("Neues Projekt …", "New project …"));
		m_New.SetCaps(m_P.Kernel().GetUi().TipBinding("projects.new"));
		m_New.SetTipAction(DZS_Texts.Pick("Neues Projekt …", "New project …"), "projects.new");
		m_New.m_OnClick.Insert(OnNew);
		m_All = new DZS_Button(m_Plate, "side", DZS_Texts.Pick("Alle Projekte …", "All projects …"));
		m_All.SetCaps(m_P.Kernel().GetUi().TipBinding("projects.browse"));
		m_All.SetTipAction(DZS_Texts.Pick("Alle Projekte …", "All projects …"), "projects.browse");
		m_All.m_OnClick.Insert(OnAll);
		m_Import = new DZS_Button(m_Plate, "side", DZS_Texts.Pick(".dze importieren …", "Import .dze …"));
		m_Import.m_OnClick.Insert(OnImport);
		m_PlateFor = m_Open;
		if (m_Focus >= 0 && m_Focus < 100)
			m_Focus = -1;
	}

	void OnRowClick(DZS_ListRow r)
	{
		DZS_Log.Info("main menu: project row " + r.m_Key);
		m_P.RequestOpen(r.m_Key);
	}

	void OnRowActivate(DZS_ListRow r)
	{
		if (r.m_Hit.m_Focus)
			OnRowClick(r);
	}

	void OnNew(DZS_Button b)
	{
		m_P.OpenNewDialog(Chosen());
	}

	void OnAll(DZS_Button b)
	{
		m_P.OpenBrowser(Chosen());
	}

	void OnImport(DZS_Button b)
	{
		m_P.OpenDzeDialog(false);
	}

	protected void UpdatePlate(float dt)
	{
		if (m_Glide < 1)
		{
			m_Plate.Show(false);
			return;
		}
		if (m_PlateFor != m_Open)
			BuildPlate();
		if (m_FadeAge < 0)
			m_FadeAge = 0;
		m_FadeAge += dt;
		m_Plate.Show(true);
		m_Plate.SetAlpha(Math.Clamp(m_FadeAge / FADE, 0, 1));
		DZS_HomeStrip s = m_Strips[m_Open];
		float pad = DZS_Scale.Px(16);
		float want = DZS_Scale.Px(520);
		if (m_New)
			want = Math.Max(want, m_New.GetWidth() + m_All.GetWidth() + m_Import.GetWidth() + DZS_Scale.Px(16) + 2 * pad);
		float w = Math.Min(want, s.m_W - DZS_Scale.Px(56) - DZS_Scale.Px(24));
		float y = 0;
		float inner = w - 2 * pad;
		y += pad;
		float nameY = y;
		y += DZS_Scale.Px(36);
		float factY = y;
		y += DZS_Scale.Px(44);
		float rowsY = y;
		if (m_Missing)
		{
			y += DZS_Scale.Px(28);
		}
		else
		{
			y += m_Rows.Count() * (DZS_Scale.Px(44) + DZS_Scale.Px(2));
			if (m_EmptyText)
				y += DZS_Scale.Px(56);
			y += DZS_Scale.Px(8);
		}
		float buttonsY = y;
		if (m_New)
			y += DZS_Scale.Px(36);
		y += pad;
		float h = y;
		float py = m_Top + m_H - DZS_Scale.Px(24) - h;
		float px = s.m_X + Math.Round(Off(py - m_Top)) + DZS_Scale.Px(56);
		DZS_W.Place(m_Plate, px, py, w, h);
		DZS_W.Place(m_PlateBg, 0, 0, w, h);
		m_PlateBorder.Place(0, 0, w, h);
		DZS_W.Place(m_PlateName, pad, nameY, inner, DZS_Scale.Px(32));
		if (m_Missing)
		{
			DZS_W.Place(m_Missing, pad, rowsY - DZS_Scale.Px(44), inner, DZS_Scale.Px(24));
			return;
		}
		DZS_W.Place(m_F1L, pad, factY, DZS_Scale.Px(160), DZS_Scale.Px(14));
		DZS_W.Place(m_F1V, pad, factY + DZS_Scale.Px(16), DZS_Scale.Px(200), DZS_Scale.Px(20));
		DZS_W.Place(m_F2L, pad + DZS_Scale.Px(220), factY, DZS_Scale.Px(160), DZS_Scale.Px(14));
		DZS_W.Place(m_F2V, pad + DZS_Scale.Px(220), factY + DZS_Scale.Px(16), DZS_Scale.Px(120), DZS_Scale.Px(20));
		float ry = rowsY;
		foreach (DZS_ListRow r : m_Rows)
		{
			r.Place(pad, ry, inner, DZS_Scale.Px(44));
			ry += DZS_Scale.Px(46);
		}
		if (m_EmptyText)
		{
			float oct = DZS_Scale.Px(40);
			DZS_W.Place(m_EmptyFrame, pad, ry, oct, oct);
			DZS_W.Place(m_EmptyIcon, pad + (oct - DZS_Scale.Px(16)) / 2, ry + (oct - DZS_Scale.Px(16)) / 2, DZS_Scale.Px(16), DZS_Scale.Px(16));
			DZS_W.Place(m_EmptyText, pad + oct + DZS_Scale.Px(12), ry + DZS_Scale.Px(10), inner - oct - DZS_Scale.Px(12), DZS_Scale.Px(20));
		}
		float bx = pad;
		m_New.Place(bx, buttonsY);
		bx += m_New.GetWidth() + DZS_Scale.Px(8);
		m_All.Place(bx, buttonsY);
		bx += m_All.GetWidth() + DZS_Scale.Px(8);
		m_Import.Place(bx, buttonsY);
	}
}

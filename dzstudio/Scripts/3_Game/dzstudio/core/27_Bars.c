// The fixed bars of the screen layout: the top bar (brand, menus, the active layer's tools, global switches,
// undo/redo, provider plate, "Save"), the layer rail at the left edge and the two side bars with their sections.
class DZS_TopBar : DZS_HitListener
{
	static const float H = 44;

	protected DZS_Screen m_Screen;
	protected DZS_UiRegistry m_Ui;
	ref DZS_Hit m_Surface;
	protected Widget m_Bg;
	protected Widget m_Line;
	protected ImageWidget m_BrandIcon;
	protected TextWidget m_Brand;
	ref array<ref DZS_Button> m_MenuButtons;
	ref array<string> m_MenuIds;
	// tool slot of the active layer
	protected ref array<ref DZS_Button> m_Tools;
	protected ref array<string> m_ToolIds;
	protected ref map<string, ref DZS_Button> m_Arrows;
	protected ref array<string> m_ToolIcons;
	protected ref array<ref DZS_Button> m_Switches;
	protected ref array<string> m_SwitchIds;
	protected Widget m_Sep;
	// global switches, right next to undo/redo
	protected ref array<ref DZS_Button> m_Globals;
	protected ref array<string> m_GlobalIds;
	protected Widget m_SepGlobal;
	protected Widget m_PlateLamp;
	protected TextWidget m_PlateText;
	protected string m_PlateTone;
	ref DZS_Button m_Save;
	ref DZS_Button m_Undo;
	ref DZS_Button m_Redo;
	protected int m_ToolCount = -1;

	void DZS_TopBar(DZS_Screen screen, Widget parent)
	{
		m_Screen = screen;
		m_Ui = DZS_Kernel.Get().GetUi();
		m_Surface = new DZS_Hit(parent, this, "topbar");
		m_Surface.m_Surface = true;
		Widget f = m_Surface.m_W;
		m_Bg = DZS_W.Panel(f, DZS_Color.Get("surface"));
		m_Line = DZS_W.Panel(f, DZS_Color.Get("line"));
		m_BrandIcon = DZS_W.Icon(f, "box", DZS_Color.Get("accent"));
		m_Brand = DZS_W.Text(f, DZS_Font.DISPLAY, 22, DZS_Color.Get("text"));
		m_Brand.SetText(DZS_Texts.Get("core.brand"));
		m_MenuButtons = new array<ref DZS_Button>();
		m_MenuIds = {"project", "edit", "view"};
		foreach (string mid : m_MenuIds)
		{
			DZS_Button mb = new DZS_Button(f, "menu", DZS_Texts.Get("core.menu." + mid), "", 32);
			mb.m_Tag = mid;
			mb.m_Hit.m_FocusKind = "";
			m_MenuButtons.Insert(mb);
		}
		m_Tools = new array<ref DZS_Button>();
		m_ToolIds = new array<string>();
		m_Arrows = new map<string, ref DZS_Button>();
		m_ToolIcons = new array<string>();
		m_Switches = new array<ref DZS_Button>();
		m_SwitchIds = new array<string>();
		m_Globals = new array<ref DZS_Button>();
		m_GlobalIds = new array<string>();
		m_Sep = DZS_W.Panel(f, DZS_Color.Get("lineStrong"));
		m_SepGlobal = DZS_W.Panel(f, DZS_Color.Get("lineStrong"));
		m_PlateLamp = DZS_W.Panel(f, DZS_Color.Get("success"));
		m_PlateText = DZS_W.Text(f, DZS_Font.MONO, 11, DZS_Color.Get("textMuted"));
		SetProviderPlate(DZS_Texts.Get("core.provider.local"), "");
		m_Undo = new DZS_Button(f, "ghost", "", "undo-2", 32);
		m_Undo.m_Tag = "core.undo";
		m_Undo.m_Hit.m_FocusKind = "";
		m_Undo.m_OnClick.Insert(OnHistoryClick);
		m_Redo = new DZS_Button(f, "ghost", "", "redo-2", 32);
		m_Redo.m_Tag = "core.redo";
		m_Redo.m_Hit.m_FocusKind = "";
		m_Redo.m_OnClick.Insert(OnHistoryClick);
		m_Save = new DZS_Button(f, "alone", DZS_Texts.Get("core.save"), "save", 32);
		m_Save.SetFixedWidth(150);
		m_Save.m_OnClick.Insert(OnSaveClick);
		m_Save.m_Hit.m_FocusKind = "";
	}

	// Provider plate: text from the storage provider; tone "" = no lamp ("Local").
	void SetProviderPlate(string text, string tone)
	{
		m_PlateText.SetText(text);
		m_PlateTone = tone;
		m_PlateLamp.Show(tone != "");
		if (tone != "")
			m_PlateLamp.SetColor(DZS_Color.Get(tone));
	}

	void OnSaveClick(DZS_Button b)
	{
		m_Ui.Trigger("core.save");
	}

	// Rebuilt when tools change or another layer becomes active: the slot gets the new layer's set as a whole.
	protected void SyncTools()
	{
		if (m_Ui.m_ToolsRev == m_ToolCount)
			return;
		m_ToolCount = m_Ui.m_ToolsRev;
		foreach (DZS_Button ob : m_Tools)
			ob.Destroy();
		foreach (string aid, DZS_Button ab : m_Arrows)
			ab.Destroy();
		foreach (DZS_Button os : m_Switches)
			os.Destroy();
		foreach (DZS_Button og : m_Globals)
			og.Destroy();
		m_Arrows.Clear();
		m_ToolIcons.Clear();
		m_Tools.Clear();
		m_ToolIds.Clear();
		m_Switches.Clear();
		m_SwitchIds.Clear();
		m_Globals.Clear();
		m_GlobalIds.Clear();
		string layer = m_Ui.GetActiveLayer();
		array<DZS_ToolInfo> tools = m_Ui.GetTools("tools");
		foreach (DZS_ToolInfo t : tools)
		{
			if (DZS_UiRegistry.ToolLayer(t) == layer)
				AddTool(t, m_Tools, m_ToolIds);
		}
		array<DZS_ToolInfo> sw = m_Ui.GetTools("switches");
		foreach (DZS_ToolInfo s : sw)
		{
			if (s.m_Layer == "")
				AddTool(s, m_Globals, m_GlobalIds);
			else if (s.m_Layer == layer)
				AddTool(s, m_Switches, m_SwitchIds);
		}
	}

	protected void AddTool(DZS_ToolInfo t, array<ref DZS_Button> list, array<string> ids)
	{
		string kind = "ghost";
		if (t.m_Labelled)
			kind = "secondary";
		DZS_Button b = new DZS_Button(m_Surface.m_W, kind, "", t.m_Icon, 32);
		b.m_Tag = t.m_Id;
		b.m_Hit.m_FocusKind = "";
		b.m_OnClick.Insert(OnToolClick);
		list.Insert(b);
		ids.Insert(t.m_Id);
		if (list == m_Tools)
			m_ToolIcons.Insert(t.m_Icon);
		if (t.m_ArrowAction == "")
			return;
		DZS_Button a = new DZS_Button(m_Surface.m_W, "ghost", "", "chevron-down", 32);
		a.SetFixedWidth(16);
		a.m_Tag = t.m_ArrowAction;
		a.m_Hit.m_FocusKind = "";
		a.m_OnClick.Insert(OnArrowClick);
		m_Arrows.Set(t.m_Id, a);
	}

	void OnArrowClick(DZS_Button b)
	{
		float ax, ay, aw, ah;
		b.GetWidget().GetScreenPos(ax, ay);
		b.GetWidget().GetScreenSize(aw, ah);
		m_Ui.m_ArrowX = ax;
		m_Ui.m_ArrowY = ay;
		m_Ui.m_ArrowW = aw;
		m_Ui.m_ArrowH = ah;
		m_Ui.m_FromArrow = true;
		m_Ui.Trigger(b.m_Tag);
		m_Ui.m_FromArrow = false;
	}

	void OnToolClick(DZS_Button b)
	{
		DZS_ToolInfo t = m_Ui.GetTool(b.m_Tag);
		if (t && t.m_Group == "tools")
		{
			string rd, re;
			if (!m_Ui.IsActionEnabled(t.m_Id, rd, re))
				return;
			m_Ui.SetActiveTool(t.m_Id);
		}
		m_Ui.Trigger(b.m_Tag);
	}

	DZS_Button MenuButton(string id)
	{
		int i = m_MenuIds.Find(id);
		if (i < 0)
			return null;
		return m_MenuButtons[i];
	}

	// States follow the registries every frame.
	protected void RefreshStates()
	{
		string active = m_Ui.GetActiveTool();
		string blockedWhy = m_Screen.SceneBlockedReason();
		for (int i = 0; i < m_Tools.Count(); i++)
		{
			DZS_Button b = m_Tools[i];
			string id = m_ToolIds[i];
			b.SetOn(id == active);
			RefreshToolButton(b, id, blockedWhy);
			DZS_ToolInfo ti = m_Ui.GetTool(id);
			if (ti && i < m_ToolIcons.Count() && m_ToolIcons[i] != ti.m_Icon)
			{
				m_ToolIcons[i] = ti.m_Icon;
				b.SetIcon(ti.m_Icon);
			}
			DZS_Button ab = m_Arrows.Get(id);
			if (ab)
				RefreshToolButton(ab, ab.m_Tag, blockedWhy);
		}
		RefreshSwitches(m_Switches, m_SwitchIds, blockedWhy);
		RefreshSwitches(m_Globals, m_GlobalIds, blockedWhy);
		string rd, re;
		bool canSave = m_Ui.IsActionEnabled("core.save", rd, re);
		m_Save.SetEnabled(canSave, DZS_Texts.Pick(rd, re));
		m_Save.SetTipAction(DZS_Texts.Get("core.save"), "core.save");
		RefreshHistoryButton(m_Undo);
		RefreshHistoryButton(m_Redo);
	}

	protected void RefreshSwitches(array<ref DZS_Button> list, array<string> ids, string blockedWhy)
	{
		for (int j = 0; j < list.Count(); j++)
		{
			DZS_Button s = list[j];
			string sid = ids[j];
			DZS_ToolInfo si = m_Ui.GetTool(sid);
			if (si && si.m_Labelled)
			{
				string ld, le;
				bool wanted = m_Ui.IsActionEnabled(sid, ld, le);
				s.Show(wanted);
				if (!wanted)
					continue;
				string label = m_Ui.GetActionLabel(sid);
				if (label != s.GetLabel())
					s.SetLabel(label);
			}
			s.SetOn(m_Ui.IsActionChecked(sid));
			RefreshToolButton(s, sid, blockedWhy);
			DZS_Button sa = m_Arrows.Get(sid);
			if (sa)
				RefreshToolButton(sa, sa.m_Tag, blockedWhy);
		}
	}

	// Undo / redo in the top bar: like the menu entries.
	protected void RefreshHistoryButton(DZS_Button b)
	{
		string id = b.m_Tag;
		string rd, re;
		bool on = m_Ui.IsActionEnabled(id, rd, re);
		b.SetEnabled(on, DZS_Texts.Pick(rd, re));
		string tip = m_Ui.GetActionTip(id);
		if (tip == "")
		{
			DZS_Action a = m_Ui.GetAction(id);
			if (a)
				tip = a.Name();
		}
		b.SetTipAction(tip, id);
	}

	void OnHistoryClick(DZS_Button b)
	{
		m_Ui.Trigger(b.m_Tag);
	}

	protected void RefreshToolButton(DZS_Button b, string id, string blockedWhy)
	{
		DZS_Action a = m_Ui.GetAction(id);
		string name = id;
		if (a)
			name = a.Name();
		string rd, re;
		bool on = m_Ui.IsActionEnabled(id, rd, re);
		string reason = DZS_Texts.Pick(rd, re);
		if (blockedWhy != "")
		{
			on = false;
			reason = blockedWhy;
		}
		b.SetEnabled(on, reason);
		DZS_ToolInfo t = m_Ui.GetTool(id);
		if (t && t.m_TipDe != "")
		{
			b.SetTipAction(DZS_Texts.Pick(t.m_TipDe, t.m_TipEn), t.m_TipKeyAction);
			return;
		}
		b.SetTipAction(name, id);
	}

	// Width of a button row (shown buttons only, their arrows, gap after each).
	protected float RowWidth(array<ref DZS_Button> list, array<string> ids, float gap)
	{
		float w = 0;
		for (int i = 0; i < list.Count(); i++)
		{
			if (!list[i].GetWidget().IsVisible())
				continue;
			w += list[i].GetWidth() + gap;
			DZS_Button a = m_Arrows.Get(ids[i]);
			if (a)
				w += a.GetWidth();
		}
		return w;
	}

	protected float PlaceRow(array<ref DZS_Button> list, array<string> ids, float x, float y, float gap)
	{
		for (int i = 0; i < list.Count(); i++)
		{
			DZS_Button b = list[i];
			if (!b.GetWidget().IsVisible())
				continue;
			b.Place(x, y);
			x += b.GetWidth();
			DZS_Button a = m_Arrows.Get(ids[i]);
			if (a)
			{
				a.Place(x, y);
				x += a.GetWidth();
			}
			x += gap;
		}
		return x;
	}

	void Layout()
	{
		SyncTools();
		RefreshStates();
		int sw, sh;
		GetScreenSize(sw, sh);
		float h = DZS_Scale.Px(H);
		m_Surface.Place(0, 0, sw, h);
		DZS_W.Place(m_Bg, 0, 0, sw, h);
		DZS_W.Place(m_Line, 0, h - 1, sw, 1);
		float y = DZS_Scale.Px(6);
		float gap = DZS_Scale.Px(4);
		float x = DZS_Scale.Px(12);
		float icon = DZS_Scale.Px(16);
		DZS_W.Place(m_BrandIcon, x, (h - icon) / 2, icon, icon);
		x += icon + DZS_Scale.Px(8);
		float bw = DZS_W.TextWidth(m_Brand);
		DZS_W.Place(m_Brand, x, 0, bw + 2, h);
		x += bw + DZS_Scale.Px(24);
		foreach (DZS_Button mb : m_MenuButtons)
		{
			mb.Place(x, y);
			x += mb.GetWidth() + gap;
		}
		x += DZS_Scale.Px(12);

		// right block, from the right edge leftwards: Save, plate, redo, undo, global switches
		float xr = sw - DZS_Scale.Px(12);
		xr -= m_Save.GetWidth();
		m_Save.Place(xr, y);
		xr -= DZS_Scale.Px(16);
		float pw = DZS_W.TextWidth(m_PlateText);
		xr -= pw;
		DZS_W.Place(m_PlateText, xr, 0, pw + 2, h);
		if (m_PlateTone != "")
		{
			float lamp = DZS_Scale.Px(8);
			xr -= lamp + DZS_Scale.Px(6);
			DZS_W.Place(m_PlateLamp, xr, (h - lamp) / 2, lamp, lamp);
		}
		xr -= DZS_Scale.Px(12);
		xr -= m_Redo.GetWidth();
		m_Redo.Place(xr, y);
		xr -= DZS_Scale.Px(2) + m_Undo.GetWidth();
		m_Undo.Place(xr, y);
		float gw = RowWidth(m_Globals, m_GlobalIds, gap);
		m_SepGlobal.Show(gw > 0);
		if (gw > 0)
		{
			xr -= DZS_Scale.Px(8);
			DZS_W.Place(m_SepGlobal, xr, (h - DZS_Scale.Px(18)) / 2, 1, DZS_Scale.Px(18));
			xr -= DZS_Scale.Px(8) + gw - gap;
			PlaceRow(m_Globals, m_GlobalIds, xr, y, gap);
		}

		// the active layer's tool slot: its tools, then its switches and commands
		x = PlaceRow(m_Tools, m_ToolIds, x, y, gap);
		int shownSwitches = 0;
		foreach (DZS_Button cs : m_Switches)
		{
			if (cs.GetWidget().IsVisible())
				shownSwitches++;
		}
		bool sep = m_Tools.Count() > 0 && shownSwitches > 0;
		m_Sep.Show(sep);
		if (sep)
		{
			x += DZS_Scale.Px(4);
			DZS_W.Place(m_Sep, x, (h - DZS_Scale.Px(18)) / 2, 1, DZS_Scale.Px(18));
			x += DZS_Scale.Px(9);
		}
		PlaceRow(m_Switches, m_SwitchIds, x, y, gap);
	}

	void Show(bool s) { m_Surface.m_W.Show(s); }
}

// The layer rail: a strip at the left screen edge, left of the catalogue bar and always shown, one symbol per layer
// (group "layers"); the active one with accent tint and a bar at its left edge, the name and key in the tooltip.
class DZS_RailItem : DZS_HitListener
{
	ref DZS_Hit m_Hit;
	protected Widget m_Bg;
	protected Widget m_Bar;
	protected ImageWidget m_Icon;
	string m_Id;
	bool m_On;

	void DZS_RailItem(Widget parent, DZS_ToolInfo t)
	{
		m_Id = t.m_Id;
		m_Hit = new DZS_Hit(parent, this, "layerrail");
		m_Hit.m_FocusKind = "";
		m_Bg = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("surfaceRaised"));
		m_Bar = DZS_W.Panel(m_Hit.m_W, DZS_Color.Get("accent"));
		m_Icon = DZS_W.Icon(m_Hit.m_W, t.m_Icon, DZS_Color.Get("textMuted"));
	}

	void Place(float x, float y, float w, float h)
	{
		m_Hit.Place(x, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		DZS_W.Place(m_Bar, 0, 0, DZS_Scale.Px(3), h);
		float icon = DZS_Scale.Px(18);
		DZS_W.Place(m_Icon, (w - icon) / 2, (h - icon) / 2, icon, icon);
		Refresh();
	}

	void Refresh()
	{
		bool hover = m_Hit.m_Enabled && m_Hit.m_Hover;
		m_Bg.Show(m_On || hover);
		if (m_On)
			m_Bg.SetColor(DZS_Color.Get("accentTint"));
		else
			m_Bg.SetColor(DZS_Color.Get("surfaceRaised"));
		m_Bar.Show(m_On);
		string col = "textMuted";
		if (m_On)
			col = "accentText";
		else if (!m_Hit.m_Enabled)
			col = "textOff";
		else if (hover)
			col = "text";
		m_Icon.SetColor(DZS_Color.Get(col));
	}

	override void OnHitState(DZS_Hit h) { Refresh(); }

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT && m_Hit.m_Enabled)
			DZS_Kernel.Get().GetUi().Trigger(m_Id);
	}

	void Destroy() { m_Hit.Destroy(); }
}

class DZS_LayerRail : DZS_HitListener
{
	static const float W = 40;

	protected DZS_UiRegistry m_Ui;
	ref DZS_Hit m_Surface;
	protected Widget m_Bg;
	protected Widget m_Edge;
	protected ref array<ref DZS_RailItem> m_Items = new array<ref DZS_RailItem>();
	protected int m_Rev = -1;

	void DZS_LayerRail(Widget parent)
	{
		m_Ui = DZS_Kernel.Get().GetUi();
		m_Surface = new DZS_Hit(parent, this, "layerrail");
		m_Surface.m_Surface = true;
		m_Bg = DZS_W.Panel(m_Surface.m_W, DZS_Color.Get("surface"));
		m_Edge = DZS_W.Panel(m_Surface.m_W, DZS_Color.Get("line"));
	}

	protected void Sync()
	{
		if (m_Ui.m_ToolsRev == m_Rev)
			return;
		m_Rev = m_Ui.m_ToolsRev;
		array<DZS_ToolInfo> layers = m_Ui.GetTools("layers");
		if (layers.Count() == m_Items.Count())
			return;
		foreach (DZS_RailItem old : m_Items)
			old.Destroy();
		m_Items.Clear();
		foreach (DZS_ToolInfo t : layers)
			m_Items.Insert(new DZS_RailItem(m_Surface.m_W, t));
	}

	// Width it takes at the left edge: 0 without layers.
	float Width()
	{
		if (m_Items.Count() == 0)
			return 0;
		return DZS_Scale.Px(W);
	}

	void Layout(float top, float bottom)
	{
		Sync();
		float w = Width();
		m_Surface.m_W.Show(w > 0);
		if (w <= 0)
			return;
		m_Surface.Place(0, top, w, bottom - top);
		DZS_W.Place(m_Bg, 0, 0, w, bottom - top);
		DZS_W.Place(m_Edge, w - 1, 0, 1, bottom - top);
		float y = DZS_Scale.Px(4);
		float ih = DZS_Scale.Px(40);
		foreach (DZS_RailItem it : m_Items)
		{
			it.m_On = m_Ui.IsActionChecked(it.m_Id);
			string rd, re;
			bool on = m_Ui.IsActionEnabled(it.m_Id, rd, re);
			it.m_Hit.m_TipReason = DZS_Texts.Pick(rd, re);
			it.m_Hit.SetEnabled(on);
			DZS_ToolInfo t = m_Ui.GetTool(it.m_Id);
			if (t)
				it.m_Hit.SetTipAction(DZS_Texts.Pick(t.m_TipDe, t.m_TipEn), it.m_Id);
			it.Place(0, y, w - 1, ih);
			y += ih;
		}
	}

	void Show(bool s) { m_Surface.m_W.Show(s && Width() > 0); }
}

// One section of a side bar: header with collapse box, title and meta, then the plugin's content.
class DZS_BarSection : DZS_HitListener
{
	DZS_PanelInfo m_Info;
	ref DZS_Hit m_Head;
	protected Widget m_HeadLine;
	protected Widget m_Box;
	protected ref DZS_Border m_BoxBorder;
	protected TextWidget m_BoxSign;
	protected TextWidget m_Title;
	protected TextWidget m_Meta;
	Widget m_Clip;
	Widget m_Inner;
	protected Widget m_ScrollBar;
	bool m_Open = true;
	// set by the bar: main section of its bar (marked, or the only section shown)
	bool m_IsMain;
	float m_Scroll;
	float m_ContentH;
	float m_ViewH;
	protected bool m_Built;

	void DZS_BarSection(Widget parent, DZS_PanelInfo info)
	{
		m_Info = info;
		m_Head = new DZS_Hit(parent, this, "sectionHead");
		m_HeadLine = DZS_W.Panel(m_Head.m_W, DZS_Color.Get("line"));
		m_Box = DZS_W.Frame(m_Head.m_W);
		m_BoxBorder = new DZS_Border(m_Box, DZS_Color.Get("capBorder"));
		m_BoxSign = DZS_W.Text(m_Box, DZS_Font.LABELBOLD, 10, DZS_Color.Get("textMuted"), true);
		m_Title = DZS_W.Text(m_Head.m_W, DZS_Font.LABEL, 11, DZS_Color.Get("textMuted"));
		m_Title.SetText(DZS_Texts.Upper(info.Title()));
		m_Meta = DZS_W.Text(m_Head.m_W, DZS_Font.MONO, 11, DZS_Color.Get("textQuiet"));
		m_Clip = DZS_W.HitFrame(parent);
		m_Clip.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Inner = DZS_W.Frame(m_Clip);
		m_ScrollBar = DZS_W.Panel(parent, DZS_Color.Get("lineStrong"));
		if (info.m_Content)
		{
			info.m_Content.Build(m_Inner);
			info.m_Content.BuildHead(m_Head.m_W);
		}
	}

	float HeadH() { return DZS_Scale.Px(36); }

	// Height of the content view for a section of fixed height, 0 for a shared one.
	float FixedView()
	{
		if (m_Info.m_FixedHeight <= 0)
			return 0;
		float total = DZS_Scale.Px(m_Info.m_FixedHeight);
		float head = HeadH();
		float view = total - head;
		if (view < 0)
			view = 0;
		return view;
	}

	// Height the content wants (0 when closed).
	float Want(float width)
	{
		if (!m_Open || !m_Info.m_Content)
			return 0;
		if (m_Info.m_Content.FillsView())
			return 1000000;
		int t0 = TickCount(0);
		m_Info.m_Content.m_SectionTop = m_Scroll;
		if (m_ViewH > 0)
			m_Info.m_Content.m_SectionH = m_ViewH;
		m_ContentH = m_Info.m_Content.Layout(width);
		if (DZS_Kernel.s_Prof)
		{
			string key = "layout." + m_Info.m_Content.ClassName();
			int had = 0;
			DZS_Kernel.s_ProfTicks.Find(key, had);
			DZS_Kernel.s_ProfTicks.Set(key, had + TickCount(0) - t0);
		}
		return m_ContentH;
	}

	// reserve: space at the right end of the head left free for the bar's collapse knob
	void Place(float x, float y, float w, float viewH, float reserve = 0)
	{
		float hh = HeadH();
		m_Head.Place(x, y, w - reserve, hh);
		DZS_W.Place(m_HeadLine, 0, hh - 1, w, 1);
		float box = DZS_Scale.Px(14);
		float pad = DZS_Scale.Px(12);
		DZS_W.Place(m_Box, pad, (hh - box) / 2, box, box);
		m_BoxBorder.Place(0, 0, box, box);
		DZS_W.Place(m_BoxSign, 0, 0, box, box);
		if (m_Open)
			m_BoxSign.SetText("−");
		else
			m_BoxSign.SetText("+");
		m_Box.Show(!m_IsMain);
		float tx = pad + box + DZS_Scale.Px(8);
		if (m_IsMain)
			tx = pad;
		float tw = DZS_W.TextWidth(m_Title);
		DZS_W.Place(m_Title, tx, 0, tw + 2, hh);
		m_Meta.SetText(m_Info.m_Meta);
		DZS_W.Place(m_Meta, tx + tw + DZS_Scale.Px(8), 0, w - tx - tw, hh);
		if (m_Info.m_Content)
			m_Info.m_Content.LayoutHead(w - reserve - DZS_Scale.Px(8), hh);
		m_ViewH = viewH;
		bool fills = m_Info.m_Content && m_Info.m_Content.FillsView();
		if (fills)
		{
			m_ContentH = viewH;
			m_Scroll = 0;
		}
		bool show = m_Open && viewH > 0;
		m_Clip.Show(show);
		float maxScroll = Math.Max(0, m_ContentH - viewH);
		m_Scroll = Math.Clamp(m_Scroll, 0, maxScroll);
		m_ScrollBar.Show(show && maxScroll > 0);
		if (!show)
			return;
		DZS_W.Place(m_Clip, x, y + hh, w, viewH);
		DZS_W.Place(m_Inner, 0, -m_Scroll, w, Math.Max(m_ContentH, viewH));
		if (fills)
			m_Info.m_Content.SetView(w, viewH);
		if (maxScroll > 0)
		{
			float bar = Math.Max(DZS_Scale.Px(24), viewH * viewH / m_ContentH);
			float by = (viewH - bar) * m_Scroll / maxScroll;
			DZS_W.Place(m_ScrollBar, x + w - DZS_Scale.Px(4), y + hh + by, DZS_Scale.Px(4), bar);
		}
	}

	void Show(bool s)
	{
		m_Head.m_W.Show(s);
		m_Clip.Show(s && m_Open);
		if (!s)
			m_ScrollBar.Show(false);
	}

	override void OnHitClick(DZS_Hit h, int button)
	{
		if (button == MouseState.LEFT && h == m_Head && !m_IsMain)
			m_Open = !m_Open;
	}

	// Mouse wheel over the content: three rows per notch.
	bool Wheel(int delta)
	{
		if (m_Info.m_Content && m_Info.m_Content.FillsView())
			return m_Info.m_Content.Wheel(delta);
		if (m_ContentH <= m_ViewH)
			return true;
		m_Scroll -= delta * DZS_Scale.Px(32) * 3;
		return true;
	}

	void Update(float dt)
	{
		if (m_Info.m_Content)
		{
			int t0 = TickCount(0);
			m_Info.m_Content.Update(dt);
			if (DZS_Kernel.s_Prof)
			{
				string key = "update." + m_Info.m_Content.ClassName();
				int had = 0;
				DZS_Kernel.s_ProfTicks.Find(key, had);
				DZS_Kernel.s_ProfTicks.Set(key, had + TickCount(0) - t0);
			}
		}
	}
}

// Side bar: 320 px, collapses to a 40 px strip with one symbol per section in 150 ms.
class DZS_SideBar : DZS_HitListener
{
	static const float W_OPEN = 320;
	static const float W_STRIP = 40;
	static const float ANIM = 0.15;

	protected string m_Side;
	ref DZS_Hit m_Surface;
	protected Widget m_Bg;
	protected Widget m_Edge;
	protected ref DZS_Button m_Collapse;
	protected ref array<ref DZS_BarSection> m_Sections;
	protected ref array<ref DZS_Button> m_StripButtons;
	protected bool m_Collapsed;
	protected float m_AnimT = 100;
	protected bool m_AnimFrom;
	protected int m_PanelCount = -1;
	protected float m_W;
	// left bar: x of its left edge (right of the layer rail)
	float m_Offset;

	void DZS_SideBar(Widget parent, string side)
	{
		m_Side = side;
		m_Surface = new DZS_Hit(parent, this, "sidebar");
		m_Surface.m_Surface = true;
		m_Surface.m_W.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Bg = DZS_W.Panel(m_Surface.m_W, DZS_Color.Get("bar"));
		m_Edge = DZS_W.Panel(m_Surface.m_W, DZS_Color.Get("line"));
		string icon = "panel-left-close";
		if (side == "right")
			icon = "panel-right-close";
		m_Collapse = new DZS_Button(m_Surface.m_W, "ghost", "", icon, 28);
		m_Collapse.m_Hit.m_FocusKind = "";
		m_Collapse.m_OnClick.Insert(OnCollapseClick);
		m_Sections = new array<ref DZS_BarSection>();
		m_StripButtons = new array<ref DZS_Button>();
		m_Collapsed = DZS_Kernel.Get().GetSettings().GetBool("core." + side + "BarCollapsed", false);
		m_W = Target();
	}

	bool IsCollapsed() { return m_Collapsed; }

	void SetCollapsed(bool c)
	{
		if (c == m_Collapsed)
			return;
		m_AnimFrom = m_Collapsed;
		m_Collapsed = c;
		m_AnimT = 0;
		DZS_Kernel.Get().GetSettings().Set("core." + m_Side + "BarCollapsed", DZS_Json.Boolean(c));
		DZS_Log.Info("bar " + m_Side + " collapsed " + DZS_Str.Bool(c));
	}

	void OnCollapseClick(DZS_Button b)
	{
		SetCollapsed(true);
	}

	// Opened by a plugin: opens the section and, if collapsed, the bar; "hidden" if not shown.
	string Unfold(string panelId)
	{
		foreach (DZS_BarSection s : m_Sections)
		{
			if (s.m_Info.m_Id != panelId)
				continue;
			if (s.m_Info.m_Hidden)
				return "hidden";
			SetCollapsed(false);
			s.m_Open = true;
			return "ok";
		}
		return "";
	}

	void OnStripClick(DZS_Button b)
	{
		SetCollapsed(false);
		foreach (DZS_BarSection s : m_Sections)
		{
			if (s.m_Info.m_Id == b.m_Tag)
				s.m_Open = true;
		}
	}

	protected float Target()
	{
		if (m_Collapsed)
			return DZS_Scale.Px(W_STRIP);
		return DZS_Scale.Px(W_OPEN);
	}

	// Current width; time-based: the end width shows in the first frame 150 ms or later after the click.
	float Width() { return m_W; }

	protected void Sync()
	{
		array<DZS_PanelInfo> panels = DZS_Kernel.Get().GetUi().GetPanels(m_Side);
		if (panels.Count() == m_PanelCount)
			return;
		m_PanelCount = panels.Count();
		foreach (DZS_PanelInfo p : panels)
		{
			bool have = false;
			foreach (DZS_BarSection s : m_Sections)
			{
				if (s.m_Info == p)
					have = true;
			}
			if (have)
				continue;
			int at = 0;
			while (at < m_Sections.Count() && m_Sections[at].m_Info.m_Order <= p.m_Order)
				at++;
			m_Sections.InsertAt(new DZS_BarSection(m_Surface.m_W, p), at);
			DZS_Button sb = new DZS_Button(m_Surface.m_W, "ghost", "", p.m_Icon, 32);
			sb.m_Tag = p.m_Id;
			sb.m_Hit.m_FocusKind = "";
			sb.SetTip(p.Title());
			sb.m_OnClick.Insert(OnStripClick);
			m_StripButtons.InsertAt(sb, at);
		}
	}

	// Sections whose panel is not hidden, in bar order.
	protected array<DZS_BarSection> Visible()
	{
		array<DZS_BarSection> r = new array<DZS_BarSection>();
		foreach (DZS_BarSection s : m_Sections)
		{
			if (!s.m_Info.m_Hidden)
				r.Insert(s);
		}
		return r;
	}

	DZS_BarSection SectionAt(Widget w)
	{
		foreach (DZS_BarSection s : m_Sections)
		{
			if (DZS_W.IsInside(w, s.m_Clip))
				return s;
		}
		return null;
	}

	void Layout(float dt, float top, float bottom)
	{
		Sync();
		float open = DZS_Scale.Px(W_OPEN);
		float strip = DZS_Scale.Px(W_STRIP);
		bool running = m_AnimT < ANIM;
		m_AnimT += dt;
		if (m_AnimT >= ANIM)
		{
			m_W = Target();
			if (running)
				DZS_Log.Info("bar " + m_Side + " end width " + m_W + " after " + Math.Round(m_AnimT * 1000) + " ms");
		}
		else
		{
			float t = m_AnimT / ANIM;
			float from = open;
			if (m_AnimFrom)
				from = strip;
			m_W = from + (Target() - from) * t;
		}
		int sw, sh;
		GetScreenSize(sw, sh);
		float x = m_Offset;
		if (m_Side == "right")
			x = sw - m_W;
		float h = bottom - top;
		m_Surface.Place(x, top, m_W, h);
		DZS_W.Place(m_Bg, 0, 0, m_W, h);
		if (m_Side == "left")
			DZS_W.Place(m_Edge, m_W - 1, 0, 1, h);
		else
			DZS_W.Place(m_Edge, 0, 0, 1, h);
		bool showStrip = m_Collapsed && m_AnimT >= ANIM;
		bool showOpen = !showStrip;
		float bw = open;
		float bx = 0;
		if (m_Side == "left")
			bx = m_W - open;
		m_Collapse.Show(showOpen);
		float cb = DZS_Scale.Px(28);
		m_Collapse.Place(bx + bw - cb - DZS_Scale.Px(4), (DZS_Scale.Px(36) - cb) / 2);
		for (int bi = 0; bi < m_StripButtons.Count(); bi++)
			m_StripButtons[bi].Show(showStrip && !m_Sections[bi].m_Info.m_Hidden);
		foreach (DZS_BarSection sec : m_Sections)
			sec.Show(showOpen && !sec.m_Info.m_Hidden);
		if (showStrip)
		{
			float y = DZS_Scale.Px(8);
			for (int bj = 0; bj < m_StripButtons.Count(); bj++)
			{
				if (m_Sections[bj].m_Info.m_Hidden)
					continue;
				DZS_Button sb = m_StripButtons[bj];
				sb.Place((m_W - sb.GetWidth()) / 2, y);
				y += sb.GetHeight() + DZS_Scale.Px(4);
			}
			return;
		}
		LayoutSections(bx, bw, h);
	}

	// Heads stay; open sections share the rest equally, a short one gives its rest to the others.
	protected void LayoutSections(float bx, float bw, float h)
	{
		array<DZS_BarSection> secs = Visible();
		DZS_BarSection main = null;
		bool markedShown = false;
		foreach (DZS_BarSection mk : secs)
		{
			if (mk.m_Info.m_Main)
				markedShown = true;
		}
		// with the marked main section hidden (a layer's list replaces the project), the first filling section leads
		foreach (DZS_BarSection ms : secs)
		{
			bool lead = ms.m_Info.m_Main;
			if (secs.Count() == 1)
				lead = true;
			if (!markedShown && ms.m_Info.m_Content && ms.m_Info.m_Content.FillsView())
				lead = true;
			ms.m_IsMain = lead;
			if (ms.m_IsMain && !main)
				main = ms;
			else
				ms.m_IsMain = false;
		}
		if (main)
			main.m_Open = true;
		float heads = 0;
		int open = 0;
		foreach (DZS_BarSection s : secs)
		{
			heads += s.HeadH();
			if (s.m_Open)
				open++;
		}
		float rest = Math.Max(0, h - heads);
		map<DZS_BarSection, float> give = new map<DZS_BarSection, float>();
		array<DZS_BarSection> pending = new array<DZS_BarSection>();
		foreach (DZS_BarSection s2 : secs)
		{
			float fixedH = s2.FixedView();
			if (s2.m_Open && fixedH > 0)
			{
				fixedH = Math.Min(fixedH, rest);
				s2.Want(bw);
				give.Set(s2, fixedH);
				rest -= fixedH;
			}
			else if (s2.m_Open && s2 != main)
				pending.Insert(s2);
			else if (s2 != main)
				give.Set(s2, 0);
		}
		int mainShare = 0;
		if (main && main.FixedView() <= 0)
			mainShare = 1;
		while (pending.Count() > 0)
		{
			float share = rest / (pending.Count() + mainShare);
			bool moved = false;
			for (int i = pending.Count() - 1; i >= 0; i--)
			{
				float want = pending[i].Want(bw);
				if (want <= share)
				{
					give.Set(pending[i], want);
					rest -= want;
					pending.Remove(i);
					moved = true;
				}
			}
			if (!moved)
			{
				foreach (DZS_BarSection p : pending)
				{
					give.Set(p, share);
					rest -= share;
				}
				pending.Clear();
			}
		}
		if (mainShare > 0)
		{
			main.Want(bw);
			give.Set(main, Math.Max(0, rest));
		}
		float y = 0;
		float reserve = DZS_Scale.Px(36);
		int last = secs.Count() - 1;
		for (int si = 0; si <= last; si++)
		{
			DZS_BarSection s3 = secs[si];
			float vh = give.Get(s3);
			if (si == last && s3.FixedView() > 0)
			{
				float atBottom = h - s3.HeadH() - vh;
				if (atBottom > y)
					y = atBottom;
			}
			s3.Place(bx, y, bw, vh, reserve);
			reserve = 0;
			y += s3.HeadH() + vh;
		}
	}

	void Update(float dt)
	{
		foreach (DZS_BarSection s : m_Sections)
			s.Update(dt);
	}

	void Show(bool s) { m_Surface.m_W.Show(s); }

	bool Wheel(Widget under, int delta)
	{
		DZS_BarSection s = SectionAt(under);
		if (s)
			return s.Wheel(delta);
		return true;
	}
}

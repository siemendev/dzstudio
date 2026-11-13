// The frame of the surface: fixed places on the screen that plugins fill — status bar, banners,
// messages, the frame and crosshair over the scene — plus pointer, cursor, keyboard focus and Esc order.
class DZS_EscHandler : Managed
{
	bool IsEscActive() { return false; }
	void OnEsc() {}
}

class DZS_Banner : Managed
{
	string m_Id;
	int m_Order;
	int m_Seq;
	ref DZS_Plate m_Plate;
	ref array<ref DZS_Button> m_Buttons;
	DZS_MessageListener m_Listener;
	// a progress plate (ShowProgress): its bar and the text it was laid out for
	ref DZS_ProgressBar m_Bar;
	string m_Markup;

	void OnButton(DZS_Button b)
	{
		if (m_Listener)
			m_Listener.OnMessageButton(m_Id + ":" + b.m_Tag);
	}

	void Destroy()
	{
		if (m_Buttons)
		{
			foreach (DZS_Button b : m_Buttons)
				b.Destroy();
		}
		if (m_Bar)
			m_Bar.Destroy();
		m_Plate.Destroy();
	}
}

class DZS_MessageListener : Managed
{
	void OnMessageButton(string kind) {}
}

class DZS_Message : Managed
{
	string m_Kind;
	string m_Tone;
	string m_Markup;
	float m_Age;
	float m_Life;
	ref DZS_Plate m_Plate;
	ref DZS_Button m_Button;
	DZS_MessageListener m_Listener;

	void OnButton(DZS_Button b)
	{
		if (m_Listener)
			m_Listener.OnMessageButton(m_Kind);
	}

	void Destroy()
	{
		if (m_Button)
			m_Button.Destroy();
		m_Plate.Destroy();
	}
}

class DZS_StatusIcon : Managed
{
	string m_Id;
	string m_Text;
	string m_Tone;
	string m_IconName;
	Widget m_Frame;
	ImageWidget m_Icon;
}

class DZS_Screen : DZS_EventListener
{
	static const int BAR_H = 28;
	static const int TOP_H = 44;

	protected DZS_Kernel m_Kernel;
	protected Widget m_Root;
	protected Widget m_SceneLayer;
	protected Widget m_MapLayer;
	protected Widget m_Bar;
	protected Widget m_BarBg;
	protected Widget m_BarLine;
	protected ref DZS_StatusText m_Save;
	protected ref DZS_StatusText m_Map;
	protected ref DZS_StatusText m_Group;
	protected ref DZS_StatusText m_Layer;
	protected ref array<ref DZS_StatusText> m_Seps;
	protected ref array<ref DZS_StatusItem> m_Left;
	protected ref array<ref DZS_StatusItem> m_Right;
	protected ref array<ref DZS_StatusIcon> m_Icons;
	protected Widget m_MoreIcon;
	protected TextWidget m_MoreText;
	protected string m_MoreTooltip;
	protected ref DZS_RichLine m_Hint;
	protected ref map<string, string> m_HintText;
	protected ref map<string, int> m_HintLevel;
	protected Widget m_HintClip;
	protected Widget m_HintCutBg;
	protected TextWidget m_HintCut;
	protected ref array<ref DZS_Banner> m_Banners;
	protected int m_BannerSeq;
	protected ref array<ref DZS_Message> m_Messages;
	protected ref DZS_Border m_SceneFrame;
	protected int m_SceneFrameColor;
	protected bool m_SceneFrameOn;
	protected Widget m_CrossH;
	protected Widget m_CrossV;
	protected bool m_UiHidden;
	protected DZS_NumberField m_Focused;
	protected ref array<DZS_NumberField> m_FocusGroup;
	protected ref array<ref DZS_EscHandler> m_EscHandlers;
	protected ref array<int> m_EscLevels;
	protected ref map<string, bool> m_CursorHide;
	protected ref map<string, bool> m_KeyboardOwners;
	protected bool m_CursorShown;
	protected bool m_EscWas;
	protected bool m_EnterWas;
	protected bool m_TabWas;
	protected bool m_LmbWas;
	protected Widget m_TipPlateOwner;
	protected ref DZS_Plate m_Tip;
	protected float m_TipWait;
	protected float m_TipAge;
	protected string m_TipText;
	protected int m_LastW;
	protected int m_LastH;
	protected Widget m_BarsLayer;
	protected Widget m_HomeLayer;
	protected bool m_Home;
	protected bool m_GameControl;
	protected bool m_GameUiWas;
	protected Widget m_OverlayLayer;
	protected Widget m_PopupLayer;
	protected Widget m_DialogLayer;
	protected Widget m_TipLayer;
	protected ref DZS_TopBar m_Top;
	protected ref DZS_SideBar m_LeftBar;
	protected ref DZS_SideBar m_RightBar;
	protected ref DZS_LayerRail m_Rail;
	protected ref array<ref DZS_Dialog> m_Dialogs;
	protected ref DZS_Popup m_Popup;
	protected string m_OpenMenu;
	protected DZS_KeysTab m_Recorder;
	protected DZS_Hit m_HoverHit;
	protected DZS_Hit m_PressHit;
	protected int m_PressButton = -1;
	protected bool m_SwallowMouse;
	protected bool m_RmbWas;
	protected ref map<string, bool> m_KeyEdge;
	protected DZS_Hit m_TipHit;

	void DZS_Screen(DZS_Kernel kernel)
	{
		m_Kernel = kernel;
		m_Seps = new array<ref DZS_StatusText>();
		m_Left = new array<ref DZS_StatusItem>();
		m_Right = new array<ref DZS_StatusItem>();
		m_Icons = new array<ref DZS_StatusIcon>();
		m_HintText = new map<string, string>();
		m_HintLevel = new map<string, int>();
		m_Banners = new array<ref DZS_Banner>();
		m_Messages = new array<ref DZS_Message>();
		m_EscHandlers = new array<ref DZS_EscHandler>();
		m_EscLevels = new array<int>();
		m_CursorHide = new map<string, bool>();
		m_KeyboardOwners = new map<string, bool>();
		m_Dialogs = new array<ref DZS_Dialog>();
		m_KeyEdge = new map<string, bool>();
		RegisterTexts();
	}

	protected void RegisterTexts()
	{
		DZS_Texts.Add("core.status.saved", "Gespeichert %1", "Saved %1");
		DZS_Texts.Add("core.status.neverSaved", "Noch nicht gespeichert", "Not saved yet");
		DZS_Texts.Add("core.status.savedPlain", "Gespeichert", "Saved");
		DZS_Texts.Add("core.status.saving", "Speichert …", "Saving …");
		DZS_Texts.Add("core.status.failed", "Speichern fehlgeschlagen", "Saving failed");
		DZS_Texts.Add("core.status.restored", "Wiederhergestellt — nicht gespeichert", "Restored — not saved");
		DZS_Texts.Add("core.status.restored1", "Wiederhergestellt — nicht gespeichert · 1 Änderung", "Restored — not saved · 1 change");
		DZS_Texts.Add("core.status.restoredN", "Wiederhergestellt — nicht gespeichert · %1 Änderungen", "Restored — not saved · %1 changes");
		DZS_Texts.Add("core.status.unsaved1", "1 ungespeicherte Änderung", "1 unsaved change");
		DZS_Texts.Add("core.status.unsavedN", "%1 ungespeicherte Änderungen", "%1 unsaved changes");
		DZS_Texts.Add("core.status.group", "Gruppe: %1", "Group: %1");
		DZS_Texts.Add("core.status.layer", "Ebene %1", "%1 layer");
		DZS_Texts.Add("core.group.objects", "Objekte", "Objects");
		DZS_Texts.Add("core.hint.browse", "Objekt im Katalog anklicken · Klick in die Szene wählt aus", "Click an object in the catalogue · Click in the scene to select");
		DZS_Texts.Add("core.map.ChernarusPlus", "Chernarus", "Chernarus");
		DZS_Texts.Add("core.map.Enoch", "Livonia", "Livonia");
		DZS_Texts.Add("core.map.Sakhal", "Sakhal", "Sakhal");
		DZS_Texts.Add("core.map.Nasdara", "Nasdara", "Nasdara");
	}

	void Build()
	{
		m_Root = DZS_W.Make("dzstudio/gui/layouts/dzs_root.layout", null);
		m_SceneLayer = DZS_W.Frame(m_Root);
		m_MapLayer = DZS_W.Frame(m_SceneLayer);
		m_BarsLayer = DZS_W.Frame(m_Root);
		m_HomeLayer = DZS_W.Frame(m_Root);
		m_HomeLayer.Show(false);
		m_PopupLayer = DZS_W.Frame(m_Root);
		m_DialogLayer = DZS_W.Frame(m_Root);
		m_OverlayLayer = DZS_W.Frame(m_Root);
		m_TipLayer = DZS_W.Frame(m_Root);
		m_SceneFrame = new DZS_Border(m_SceneLayer, DZS_Color.Get("warning"));
		m_SceneFrame.Show(false);
		m_CrossH = DZS_W.Panel(m_SceneLayer, DZS_Color.Get("text"));
		m_CrossV = DZS_W.Panel(m_SceneLayer, DZS_Color.Get("text"));
		ShowCrosshair(false);

		m_Top = new DZS_TopBar(this, m_BarsLayer);
		m_LeftBar = new DZS_SideBar(m_BarsLayer, "left");
		m_RightBar = new DZS_SideBar(m_BarsLayer, "right");
		m_Rail = new DZS_LayerRail(m_BarsLayer);
		foreach (DZS_Button mb : m_Top.m_MenuButtons)
			mb.m_OnClick.Insert(OnMenuTitle);
		m_Bar = DZS_W.Frame(m_BarsLayer);
		m_Bar.ClearFlags(WidgetFlags.IGNOREPOINTER);
		m_BarBg = DZS_W.Panel(m_Bar, DZS_Color.Get("well"));
		m_BarLine = DZS_W.Panel(m_Bar, DZS_Color.Get("line"));
		m_Save = new DZS_StatusText("core.save", DZS_Font.MONO);
		m_Save.Build(m_Bar);
		m_Map = new DZS_StatusText("core.map", DZS_Font.MONO);
		m_Map.Build(m_Bar);
		m_Map.SetText(MapName(m_Kernel.GetWorld()));
		m_Group = new DZS_StatusText("core.group", DZS_Font.MONO, 160);
		m_Group.Build(m_Bar);
		m_Layer = new DZS_StatusText("core.layer", DZS_Font.MONO, 160);
		m_Layer.Build(m_Bar);
		m_HintClip = DZS_W.Frame(m_Bar);
		m_HintClip.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Hint = new DZS_RichLine(m_HintClip, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_HintCutBg = DZS_W.Panel(m_HintClip, DZS_Color.Get("well"));
		m_HintCut = DZS_W.Text(m_HintClip, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_HintCut.SetText("…");
		m_MoreIcon = DZS_W.HitFrame(m_Bar);
		m_MoreText = DZS_W.Text(m_MoreIcon, DZS_Font.MONO, 11, DZS_Color.Get("textMuted"));
		m_MoreIcon.Show(false);
		SetHint("core", 0, DZS_Texts.Get("core.hint.browse"));
		ShowCursor();
	}

	static string MapName(string world)
	{
		string key = "core.map." + world;
		string t = DZS_Texts.Get(key);
		if (t == key)
			return world;
		return t;
	}

	void Destroy()
	{
		if (m_Focused)
			Unfocus(false);
		if (m_Root)
			m_Root.Unlink();
		m_Root = null;
		if (m_CursorShown)
		{
			GetGame().GetUIManager().ShowUICursor(false);
			GetGame().GetInput().ChangeGameFocus(-1);
			m_CursorShown = false;
		}
	}

	Widget GetRoot() { return m_OverlayLayer; }
	Widget GetHomeLayer() { return m_HomeLayer; }
	bool IsHome() { return m_Home; }

	// The game draws 3D previews only as top-level widgets, above every surface of the editor: a plugin hides
	// a preview where the editor shows something over it (hidden surface, main menu, dialog, menu, tooltip).
	bool CoversRect(float x, float y, float w, float h)
	{
		if (m_UiHidden || m_Home || m_Dialogs.Count() > 0 || m_Popup)
			return true;
		if (m_Tip && m_Tip.m_Frame)
		{
			float tx, ty, tw, th;
			m_Tip.m_Frame.GetScreenPos(tx, ty);
			m_Tip.m_Frame.GetScreenSize(tw, th);
			if (tx < x + w && x < tx + tw && ty < y + h && y < ty + th)
				return true;
		}
		return false;
	}

	// Surfaces of plugins that lie over the bars but under menus, dialogs and messages (the full-screen search):
	// the popup layer, where menus are made later and so lie above. A layer of its own between bars and popups
	// stayed blank in DayZ 1.30.
	Widget GetSheetLayer() { return m_PopupLayer; }

	// The editor's main menu covers the whole window; bars and scene are hidden.
	void SetHome(bool on)
	{
		m_Home = on;
		CloseMenu();
		if (m_Focused)
			Unfocus(false);
		m_BarsLayer.Show(!on);
		m_SceneLayer.Show(!on);
		m_HomeLayer.Show(on);
		HoldKeysUntilRelease();
		string probe;
		if (on && GetCLIParam("dzsHomeMsg", probe))
			ShowMessage("probe", "", "Test message in the main menu", 30.0);
	}
	Widget GetDialogLayer() { return m_DialogLayer; }
	Widget GetPopupLayer() { return m_PopupLayer; }
	Widget GetSceneLayer() { return m_SceneLayer; }
	// Under the scene frame and every plugin overlay of the scene: the map.
	Widget GetMapLayer() { return m_MapLayer; }

	Widget GetTipLayer() { return m_TipLayer; }

	// ---- scene area ----

	void GetSceneRect(out float x, out float y, out float w, out float h)
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		x = 0;
		y = 0;
		w = sw;
		h = sh;
		if (m_UiHidden)
			return;
		float top = DZS_Scale.Px(TOP_H);
		float bot = DZS_Scale.Px(BAR_H);
		y = top;
		h = sh - top - bot;
		x = m_Rail.Width() + m_LeftBar.Width();
		w = sw - x - m_RightBar.Width();
	}

	bool IsUiHidden() { return m_UiHidden; }

	// Height of the selection toolbar under the top bar; banners start 12 px below it.
	protected float m_TopReserve;
	void SetTopReserve(float h) { m_TopReserve = h; }

	// Keyboard and mouse belong to the game: the surface is hidden, the pointer is off,
	// and only actions that work in "player" fire.
	bool IsGameControl() { return m_GameControl; }

	void SetGameControl(bool on)
	{
		if (on == m_GameControl)
			return;
		m_GameControl = on;
		if (on)
		{
			m_GameUiWas = m_UiHidden;
			CloseMenu();
			if (m_Focused)
				Unfocus(false);
			SetUiHidden(true);
			if (m_CursorShown)
			{
				GetGame().GetUIManager().ShowUICursor(false);
				GetGame().GetInput().ChangeGameFocus(-1);
				m_CursorShown = false;
			}
		}
		else
		{
			SetUiHidden(m_GameUiWas);
			ShowCursor();
			ApplyCursorHidden(m_CursorHide.Count() > 0);
		}
		RefreshInputBlock();
		DZS_Log.Info("game control " + on);
	}

	// A press that only closed a menu or ended a key recording belongs to nobody else.
	bool IsMouseSwallowed() { return m_SwallowMouse; }

	void SetUiHidden(bool hidden)
	{
		if (hidden == m_UiHidden)
			return;
		m_UiHidden = hidden;
		if (hidden && m_Focused)
			Unfocus(false);
		if (hidden)
			CloseMenu();
		m_Bar.Show(!hidden);
		m_Top.Show(!hidden);
		m_LeftBar.Show(!hidden);
		m_RightBar.Show(!hidden);
		m_Rail.Show(!hidden);
		m_Kernel.GetEvents().FirePluginEvent("core.uiHidden", null);
	}

	// Pointer over the scene: inside the scene area and not over a surface that takes clicks.
	bool IsPointerOverScene()
	{
		if (!IsCursorVisible())
			return false;
		int mx, my;
		GetMousePos(mx, my);
		float x, y, w, h;
		GetSceneRect(x, y, w, h);
		if (mx < x || my < y || mx >= x + w || my >= y + h)
			return false;
		Widget under = GetWidgetUnderCursor();
		if (under && m_Bar && DZS_W.IsInside(under, m_Bar))
			return false;
		if (under && DZS_HitRegistry.Find(under))
			return false;
		return true;
	}

	// ---- cursor ----

	protected void ShowCursor()
	{
		if (m_CursorShown)
			return;
		GetGame().GetInput().ChangeGameFocus(1);
		GetGame().GetUIManager().ShowUICursor(true);
		m_CursorShown = true;
	}

	void SetCursorHidden(string owner, bool hidden)
	{
		bool was = m_CursorHide.Count() > 0;
		if (hidden)
			m_CursorHide.Set(owner, true);
		else
			m_CursorHide.Remove(owner);
		bool now = m_CursorHide.Count() > 0;
		if (was != now)
			ApplyCursorHidden(now);
	}

	protected void ApplyCursorHidden(bool hidden)
	{
		GetGame().GetUIManager().ShowUICursor(!hidden);
	}

	bool IsCursorVisible() { return m_CursorHide.Count() == 0; }

	// The game takes the focus back now and then (window switch, its own input handling); keep ours.
	protected void EnsureCursor()
	{
		if (!m_CursorShown || GetGame().GetUIManager().GetMenu() != null)
			return;
		if (GetGame().GetInput().HasGameFocus())
		{
			GetGame().GetInput().ChangeGameFocus(1);
			DZS_Log.Info("ui focus taken back from the game");
		}
		bool want = IsCursorVisible();
		if (GetGame().GetUIManager().IsCursorVisible() != want)
			GetGame().GetUIManager().ShowUICursor(want);
	}

	// ---- keyboard ----

	// Owners such as an open map or dialog take the keyboard away from the scene.
	void SetKeyboardOwner(string owner, bool active)
	{
		if (active)
			m_KeyboardOwners.Set(owner, true);
		else
			m_KeyboardOwners.Remove(owner);
		RefreshInputBlock();
	}

	bool HasKeyboardOwner(string owner) { return m_KeyboardOwners.Contains(owner); }

	bool IsFieldFocused() { return m_Focused != null; }

	// Scene shortcuts and the camera stand while a field, dialog, menu or the map has the keyboard.
	bool IsSceneKeyboardBlocked()
	{
		if (m_GameControl)
			return true;
		if (m_Focused || m_KeyboardOwners.Count() > 0)
			return true;
		return GetGame().GetUIManager().GetMenu() != null;
	}

	protected void RefreshInputBlock()
	{
		m_Kernel.GetUi().SetKeyboardTarget(KeyboardTarget());
	}

	void SetFocusGroup(array<DZS_NumberField> group)
	{
		m_FocusGroup = group;
	}

	bool IsFocused(DZS_NumberField f) { return f && m_Focused == f; }

	void FocusField(DZS_NumberField f)
	{
		if (m_Focused == f)
			return;
		if (m_Focused)
			CommitField(m_Focused, "switch");
		m_Focused = f;
		DZS_Log.Info("field " + f.m_Id + " focus");
		DZS_Dialog top = TopDialog();
		if (top)
			top.OnFieldFocused(f);
		f.OnFocusChanged(true);
		SetFocus(f.GetEdit());
		RefreshInputBlock();
		f.m_OnFocus.Invoke(f, true);
		m_Kernel.GetEvents().FirePluginEvent("core.fieldFocus", null);
	}

	void Unfocus(bool commit)
	{
		DZS_NumberField f = m_Focused;
		if (!f)
			return;
		if (commit)
		{
			CommitField(f, "leave");
			return;
		}
		m_Focused = null;
		f.OnFocusChanged(false);
		SetFocus(null);
		RefreshInputBlock();
		f.m_OnFocus.Invoke(f, false);
	}

	// how: "enter", "tab", "leave", "switch". The owner answers through the invoker with ok/error.
	void CommitField(DZS_NumberField f, string how)
	{
		if (!f || f != m_Focused)
			return;
		DZS_Log.Info("field " + f.m_Id + " commit (" + how + ") '" + f.GetTyped() + "'");
		if (f.GetTyped() == "")
		{
			Unfocus(false);
			if (how == "tab" && m_FocusGroup)
			{
				int ti = m_FocusGroup.Find(f);
				if (ti >= 0 && ti + 1 < m_FocusGroup.Count())
					FocusField(m_FocusGroup[ti + 1]);
			}
			return;
		}
		DZS_FieldResult r = new DZS_FieldResult();
		f.m_OnCommit.Invoke(f, f.GetTyped(), r);
		if (!r.m_Ok)
		{
			f.SetError(true);
			if (how == "switch" || how == "leave")
			{
				m_Focused = null;
				f.OnFocusChanged(false);
				SetFocus(null);
				RefreshInputBlock();
			}
			else
				SetFocus(f.GetEdit());
			return;
		}
		m_Focused = null;
		f.OnFocusChanged(false);
		SetFocus(null);
		RefreshInputBlock();
		if (how == "tab" && m_FocusGroup)
		{
			int i = m_FocusGroup.Find(f);
			if (i >= 0 && i + 1 < m_FocusGroup.Count())
				FocusField(m_FocusGroup[i + 1]);
		}
	}

	// ---- Esc order; lower level goes first ----

	void AddEscHandler(DZS_EscHandler h, int level)
	{
		int i = 0;
		while (i < m_EscLevels.Count() && m_EscLevels[i] <= level)
			i++;
		m_EscHandlers.InsertAt(h, i);
		m_EscLevels.InsertAt(level, i);
	}

	// Returns true if the editor used this Esc. Exactly one thing ends per press.
	protected bool DispatchEsc()
	{
		if (m_Recorder)
		{
			m_Recorder.CancelRecording();
			return true;
		}
		if (m_UiHidden)
		{
			SetUiHidden(false);
			return true;
		}
		if (m_Popup)
		{
			CloseMenu();
			return true;
		}
		DZS_Dialog top = TopDialog();
		if (top)
		{
			top.Cancel();
			return true;
		}
		if (m_Focused)
		{
			Unfocus(false);
			return true;
		}
		foreach (DZS_EscHandler h : m_EscHandlers)
		{
			if (h.IsEscActive())
			{
				h.OnEsc();
				return true;
			}
		}
		return false;
	}

	// ---- dialogs ----

	DZS_Dialog TopDialog()
	{
		if (m_Dialogs.Count() == 0)
			return null;
		return m_Dialogs[m_Dialogs.Count() - 1];
	}

	bool HasDialog() { return m_Dialogs.Count() > 0; }

	void PushDialog(DZS_Dialog d)
	{
		CloseMenu();
		if (m_UiHidden)
			SetUiHidden(false);
		if (m_Focused)
			Unfocus(false);
		m_Dialogs.Insert(d);
		SetKeyboardOwner("dialog", true);
		DZS_Log.Info("dialog open (" + m_Dialogs.Count() + ")");
	}

	void PopDialog(DZS_Dialog d)
	{
		int i = m_Dialogs.Find(d);
		if (i >= 0)
			m_Dialogs.Remove(i);
		SetKeyboardOwner("dialog", m_Dialogs.Count() > 0);
		HoldKeysUntilRelease();
		DZS_Log.Info("dialog closed (" + m_Dialogs.Count() + ")");
	}

	void OpenSettings()
	{
		DZS_SettingsDialog d = new DZS_SettingsDialog();
		d.Open();
	}

	// ---- menus ----

	void OnMenuTitle(DZS_Button b)
	{
		if (m_OpenMenu == b.m_Tag)
		{
			CloseMenu();
			return;
		}
		OpenMenu(b.m_Tag);
	}

	void OpenMenu(string menuId)
	{
		CloseMenu();
		DZS_UiRegistry ui = m_Kernel.GetUi();
		array<DZS_MenuEntry> entries = ui.GetMenuEntries(menuId);
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		int lastGroup = -1;
		// Map and full-screen search: what does not work by key there is off by click too
		string state = "";
		if (HasKeyboardOwner("fullsearch"))
			state = "fullsearch";
		else if (HasKeyboardOwner("map"))
			state = "map";
		string blockedWhy = SceneBlockedReason();
		foreach (DZS_MenuEntry e : entries)
		{
			DZS_Action a = ui.GetAction(e.m_ActionId);
			if (!a || !ui.IsActionListed(a.m_Id))
				continue;
			DZS_MenuItem it = new DZS_MenuItem();
			it.m_ActionId = e.m_ActionId;
			it.m_Text = ui.GetActionLabel(a.m_Id);
			it.m_Binding = ui.FirstBinding(a.m_Id);
			if (a.m_Gesture)
				it.m_Binding = "";
			it.m_Check = e.m_Check;
			it.m_Checked = ui.IsActionChecked(a.m_Id);
			string rd, re;
			it.m_Enabled = ui.IsActionEnabled(a.m_Id, rd, re);
			it.m_Reason = DZS_Texts.Pick(rd, re);
			if (state != "" && !ui.WorksIn(a.m_Id, state))
			{
				if (menuId == "project" || ui.WorksIn(a.m_Id, state + "Menu"))
					it.m_CapsHidden = true;
				else
				{
					it.m_Enabled = false;
					it.m_Reason = blockedWhy;
				}
			}
			it.m_Tip = ui.GetActionTip(a.m_Id);
			int group = e.m_Order / 100;
			it.m_SepBefore = lastGroup >= 0 && group != lastGroup;
			lastGroup = group;
			items.Insert(it);
		}
		m_Popup = new DZS_Popup(m_PopupLayer, items, menuId);
		m_Popup.m_OnChoose.Insert(OnMenuChoose);
		DZS_Button title = m_Top.MenuButton(menuId);
		float x, y;
		title.GetWidget().GetScreenPos(x, y);
		m_Popup.Open(x, DZS_Scale.Px(TOP_H) - DZS_Scale.Px(2));
		m_OpenMenu = menuId;
		title.SetOn(true);
		SetKeyboardOwner("menu", true);
		DZS_Log.Info("menu " + menuId + " open, " + items.Count() + " entries");
	}

	// Context menu at the pointer (or at atX, atY); the caller adds its handler to m_OnChoose of the returned popup.
	DZS_Popup OpenContextMenu(array<ref DZS_MenuItem> items, float atX = -1, float atY = -1)
	{
		CloseMenu();
		Widget layer = m_PopupLayer;
		if (HasDialog())
			layer = m_DialogLayer;
		m_Popup = new DZS_Popup(layer, items, "context");
		int mx, my;
		GetMousePos(mx, my);
		if (atX >= 0)
			m_Popup.Open(atX, atY);
		else
			m_Popup.Open(mx, my);
		m_OpenMenu = "context";
		SetKeyboardOwner("menu", true);
		return m_Popup;
	}

	void OnMenuChoose(DZS_Popup p, DZS_MenuItem it)
	{
		string id = it.m_ActionId;
		CloseMenu();
		if (id != "")
			m_Kernel.GetUi().Trigger(id);
	}

	void CloseMenu()
	{
		if (!m_Popup)
			return;
		if (m_OpenMenu != "context")
		{
			DZS_Button title = m_Top.MenuButton(m_OpenMenu);
			if (title)
				title.SetOn(false);
		}
		m_Popup.Destroy();
		m_Popup = null;
		m_OpenMenu = "";
		SetKeyboardOwner("menu", false);
		HoldKeysUntilRelease();
	}

	bool IsMenuOpen() { return m_Popup != null; }
	string GetOpenMenu() { return m_OpenMenu; }

	// ---- bars ----

	bool IsBarCollapsed(string side)
	{
		if (side == "left")
			return m_LeftBar.IsCollapsed();
		return m_RightBar.IsCollapsed();
	}

	void SetBarCollapsed(string side, bool c)
	{
		if (side == "left")
			m_LeftBar.SetCollapsed(c);
		else
			m_RightBar.SetCollapsed(c);
	}

	void SetProviderPlate(string text, string tone)
	{
		m_Top.SetProviderPlate(text, tone);
	}

	// Why scene tools are off right now (map or full search open); "" if they work.
	string SceneBlockedReason()
	{
		if (HasKeyboardOwner("map"))
			return DZS_Texts.Get("core.blocked.map");
		if (HasKeyboardOwner("fullsearch"))
			return DZS_Texts.Get("core.blocked.fullsearch");
		return "";
	}

	// ---- key recording in the settings ----

	void SetRecorder(DZS_KeysTab rec)
	{
		m_Recorder = rec;
		if (!rec)
			HoldKeysUntilRelease();
	}

	bool IsRecording() { return m_Recorder != null; }

	// Keys still down when the keyboard changes owner act for the new owner only after a new press.
	protected void HoldKeysUntilRelease()
	{
		array<string> keys = {"RETURN", "NUMPADENTER", "TAB", "SPACE", "ESCAPE", "UP", "DOWN", "LEFT", "RIGHT"};
		foreach (string k : keys)
			m_KeyEdge.Set(k, DZS_UiRegistry.KeyDown(k));
		m_EscWas = KeyState(KeyCode.KC_ESCAPE) != 0;
		m_EnterWas = KeyState(KeyCode.KC_RETURN) != 0 || KeyState(KeyCode.KC_NUMPADENTER) != 0;
		m_TabWas = KeyState(KeyCode.KC_TAB) != 0;
	}

	protected bool Pressed(string key)
	{
		bool down = DZS_UiRegistry.KeyDown(key);
		bool was = m_KeyEdge.Get(key);
		m_KeyEdge.Set(key, down);
		return down && !was;
	}

	// Who has the keyboard, told to the action registry every frame.
	string KeyboardTarget()
	{
		if (m_GameControl)
			return "character";
		if (m_Recorder)
			return "recording";
		if (m_Popup)
			return "menu";
		if (m_Dialogs.Count() > 0)
			return "dialog";
		if (m_Focused)
			return "field";
		if (m_Home)
			return "mainmenu";
		if (HasKeyboardOwner("fullsearch"))
			return "fullsearch";
		if (HasKeyboardOwner("map"))
			return "map";
		return "scene";
	}

	// ---- status bar ----

	void AddStatusLeft(DZS_StatusItem item)
	{
		item.Build(m_Bar);
		m_Left.Insert(item);
	}

	void AddStatusRight(DZS_StatusItem item)
	{
		item.Build(m_Bar);
		m_Right.Insert(item);
	}

	// A status symbol of a plugin: a line icon in the status colour, its text as tooltip.
	void SetStatusIcon(string id, bool visible, string icon, string text, string tone)
	{
		DZS_StatusIcon si = null;
		foreach (DZS_StatusIcon i : m_Icons)
		{
			if (i.m_Id == id)
				si = i;
		}
		if (!visible)
		{
			if (si)
			{
				si.m_Frame.Unlink();
				m_Icons.RemoveItem(si);
			}
			return;
		}
		if (!si)
		{
			si = new DZS_StatusIcon();
			si.m_Id = id;
			si.m_Frame = DZS_W.HitFrame(m_Bar);
			si.m_Icon = DZS_W.Icon(si.m_Frame, icon, DZS_Color.Get(tone));
			m_Icons.Insert(si);
		}
		si.m_Text = text;
		si.m_Tone = tone;
		if (si.m_IconName != icon)
			DZS_W.SetIcon(si.m_Icon, icon);
		si.m_IconName = icon;
		si.m_Icon.SetColor(DZS_Color.Get(tone));
	}

	bool HasStatusIcon(string id)
	{
		foreach (DZS_StatusIcon i : m_Icons)
		{
			if (i.m_Id == id)
				return true;
		}
		return false;
	}

	void SetStatusGroup(string name)
	{
		if (name == "")
			m_Group.SetText("");
		else
			m_Group.SetText(DZS_Texts.Fmt("core.status.group", name));
	}

	// The hint of the highest level wins; empty text removes the owner's hint.
	void SetHint(string owner, int level, string markup)
	{
		if (markup == "")
		{
			m_HintText.Remove(owner);
			m_HintLevel.Remove(owner);
		}
		else
		{
			m_HintText.Set(owner, markup);
			m_HintLevel.Set(owner, level);
		}
		string best = "";
		int bestLevel = -1000;
		foreach (string o, string t : m_HintText)
		{
			int l = m_HintLevel.Get(o);
			if (l > bestLevel)
			{
				bestLevel = l;
				best = t;
			}
		}
		m_Hint.Set(best);
	}

	protected void UpdateSaveState()
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
		{
			m_Save.SetText("");
			return;
		}
		m_Save.SetLamp("");
		DZS_SaveRules rules = DZS_SaveRules.Get();
		string rText, rLamp;
		if (rules && rules.StatusText(inst, rText, rLamp))
		{
			m_Save.SetText(rText);
			m_Save.SetLamp(rLamp);
			return;
		}
		int n = inst.GetUnsavedCount();
		string mode = inst.GetSaveMode();
		if (inst.IsSaving())
			m_Save.SetText(DZS_Texts.Get("core.status.saving"));
		else if (inst.IsSaveFailed())
		{
			m_Save.SetText(DZS_Texts.Get("core.status.failed"));
			m_Save.SetLamp("danger");
		}
		else if (mode == "restored")
		{
			if (n == 1)
				m_Save.SetText(DZS_Texts.Get("core.status.restored1"));
			else if (n > 1)
				m_Save.SetText(DZS_Texts.Fmt("core.status.restoredN", n.ToString()));
			else
				m_Save.SetText(DZS_Texts.Get("core.status.restored"));
		}
		else if (n == 1)
			m_Save.SetText(DZS_Texts.Get("core.status.unsaved1"));
		else if (n > 1)
			m_Save.SetText(DZS_Texts.Fmt("core.status.unsavedN", n.ToString()));
		else
		{
			string when = inst.GetSavedClock();
			if (mode == "new")
				m_Save.SetText(DZS_Texts.Get("core.status.neverSaved"));
			else if (when == "")
				m_Save.SetText(DZS_Texts.Get("core.status.savedPlain"));
			else
				m_Save.SetText(DZS_Texts.Fmt("core.status.saved", when));
		}
	}

	protected DZS_StatusText Sep(int i)
	{
		while (m_Seps.Count() <= i)
		{
			DZS_StatusText s = new DZS_StatusText("sep", DZS_Font.MONO);
			s.Build(m_Bar);
			s.SetText(" · ");
			s.SetColorName("textQuiet");
			m_Seps.Insert(s);
		}
		return m_Seps[i];
	}

	protected void LayoutBar()
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		float h = DZS_Scale.Px(BAR_H);
		float y0 = sh - h;
		DZS_W.Place(m_Bar, 0, y0, sw, h);
		DZS_W.Place(m_BarBg, 0, 0, sw, h);
		DZS_W.Place(m_BarLine, 0, 0, sw, 1);
		float pad = DZS_Scale.Px(12);
		float gap = DZS_Scale.Px(12);

		// right part, from the right edge leftwards: fields and speed (plugins), then status icons
		float xr = sw - pad;
		for (int ri = m_Right.Count() - 1; ri >= 0; ri--)
		{
			DZS_StatusItem it = m_Right[ri];
			float iw = it.GetWidth();
			xr -= iw;
			it.Place(xr, 0, h);
			xr -= DZS_Scale.Px(6);
		}
		xr -= gap;
		int shownIcons = Math.Min(m_Icons.Count(), 6);
		int extra = m_Icons.Count() - shownIcons;
		m_MoreIcon.Show(extra > 0);
		if (extra > 0)
		{
			m_MoreText.SetText("+" + extra);
			float mw = DZS_W.TextWidth(m_MoreText) + DZS_Scale.Px(4);
			xr -= mw;
			DZS_W.Place(m_MoreIcon, xr, 0, mw, h);
			DZS_W.Place(m_MoreText, 0, 0, mw, h);
			m_MoreTooltip = "";
			for (int mi = shownIcons; mi < m_Icons.Count(); mi++)
			{
				if (m_MoreTooltip != "")
					m_MoreTooltip += " · ";
				m_MoreTooltip += m_Icons[mi].m_Text;
			}
			xr -= DZS_Scale.Px(8);
		}
		for (int ii = m_Icons.Count() - 1; ii >= 0; ii--)
		{
			DZS_StatusIcon icon = m_Icons[ii];
			icon.m_Frame.Show(ii < shownIcons);
			if (ii >= shownIcons)
				continue;
			float isz = DZS_Scale.Px(16);
			float w = isz + DZS_Scale.Px(4);
			xr -= w;
			DZS_W.Place(icon.m_Frame, xr, 0, w, h);
			DZS_W.Place(icon.m_Icon, DZS_Scale.Px(2), (h - isz) / 2, isz, isz);
			xr -= DZS_Scale.Px(6);
		}

		// left part
		float xl = pad;
		array<DZS_StatusItem> left = new array<DZS_StatusItem>();
		left.Insert(m_Save);
		left.Insert(m_Map);
		string layerName = m_Kernel.GetUi().LayerName(m_Kernel.GetUi().GetActiveLayer());
		if (layerName != "")
		{
			m_Layer.SetText(DZS_Texts.Fmt("core.status.layer", layerName));
			left.Insert(m_Layer);
		}
		else
			m_Layer.Show(false);
		if (m_Group.GetText() != "")
			left.Insert(m_Group);
		else
			m_Group.Show(false);
		foreach (DZS_StatusItem li : m_Left)
			left.Insert(li);
		// plugin segments drop from the back when nothing else fits
		float sepW = Sep(0).GetWidth();
		float need = 0;
		int keep = left.Count();
		while (true)
		{
			need = 0;
			for (int k = 0; k < keep; k++)
				need += left[k].GetWidth() + sepW;
			if (xl + need + DZS_Scale.Px(120) <= xr || keep <= 2)
				break;
			keep--;
		}
		int sepIdx = 0;
		for (int li2 = 0; li2 < left.Count(); li2++)
		{
			DZS_StatusItem item = left[li2];
			item.Show(li2 < keep);
			if (li2 >= keep)
				continue;
			if (li2 > 0)
			{
				DZS_StatusText sep = Sep(sepIdx);
				sep.Show(true);
				sep.Place(xl, 0, h);
				xl += sep.GetWidth();
				sepIdx++;
			}
			item.Place(xl, 0, h);
			xl += item.GetWidth();
		}
		for (int si = sepIdx; si < m_Seps.Count(); si++)
			m_Seps[si].Show(false);

		// hint in the middle, centred in what is left, cut at the end; gone below 120 px
		float hx0 = xl + gap;
		float hx1 = xr;
		float avail = hx1 - hx0;
		bool showHint = avail >= DZS_Scale.Px(120);
		m_HintClip.Show(showHint);
		if (showHint)
		{
			float hw = Math.Min(avail, m_Hint.GetWidth());
			float hx = hx0 + (avail - hw) / 2;
			DZS_W.Place(m_HintClip, hx, 0, hw, h);
			m_Hint.Place(0, (h - m_Hint.GetHeight()) / 2);
			bool cut = m_Hint.GetWidth() > avail;
			m_HintCutBg.Show(cut);
			m_HintCut.Show(cut);
			if (cut)
			{
				float cw = DZS_W.TextWidth(m_HintCut) + DZS_Scale.Px(2);
				DZS_W.Place(m_HintCutBg, hw - cw, 1, cw, h - 1);
				DZS_W.Place(m_HintCut, hw - cw, 0, cw, h);
			}
		}
	}

	// ---- overlays over the scene ----

	void ShowSceneFrame(bool on, string colorName)
	{
		m_SceneFrameOn = on;
		m_SceneFrame.SetColor(DZS_Color.Get(colorName));
		m_SceneFrame.Show(on);
	}

	void ShowCrosshair(bool on)
	{
		m_CrossH.Show(on);
		m_CrossV.Show(on);
	}

	void ShowBanner(string id, int order, string tone, string markup)
	{
		foreach (DZS_Banner b : m_Banners)
		{
			if (b.m_Id == id)
			{
				b.m_Plate.SetText(markup);
				return;
			}
		}
		DZS_Banner nb = new DZS_Banner();
		nb.m_Id = id;
		nb.m_Order = order;
		m_BannerSeq++;
		nb.m_Seq = m_BannerSeq;
		nb.m_Plate = new DZS_Plate(m_OverlayLayer, tone, markup, DZS_Font.READ, 13);
		m_Banners.Insert(nb);
	}

	// Banner with knobs: a click calls listener.OnMessageButton("‹id›:‹tag›"); the plate beside the knobs
	// lets clicks through to the scene.
	void ShowBannerButtons(string id, int order, string tone, string markup, array<string> tags, array<string> labels, DZS_MessageListener listener)
	{
		HideBanner(id);
		DZS_Banner nb = new DZS_Banner();
		nb.m_Id = id;
		nb.m_Order = order;
		m_BannerSeq++;
		nb.m_Seq = m_BannerSeq;
		nb.m_Plate = new DZS_Plate(m_OverlayLayer, tone, markup, DZS_Font.READ, 13);
		nb.m_Listener = listener;
		nb.m_Buttons = new array<ref DZS_Button>();
		float pad = DZS_Scale.Px(8);
		float x = nb.m_Plate.m_W;
		for (int i = 0; i < tags.Count(); i++)
		{
			string kind = "secondary";
			if (i == 0)
				kind = "main";
			DZS_Button b = new DZS_Button(nb.m_Plate.m_Frame, kind, labels[i], "", 24);
			b.m_Tag = tags[i];
			b.m_OnClick.Insert(nb.OnButton);
			nb.m_Buttons.Insert(b);
			x += b.GetWidth() + pad;
			nb.m_Plate.m_H = Math.Max(nb.m_Plate.m_H, Math.Round(b.GetHeight() + DZS_Scale.Px(8)));
		}
		nb.m_Plate.m_W = Math.Round(x);
		nb.m_Plate.Resize();
		float bx = nb.m_Plate.m_W;
		for (int j = nb.m_Buttons.Count() - 1; j >= 0; j--)
		{
			DZS_Button bb = nb.m_Buttons[j];
			bx -= bb.GetWidth() + pad;
			bb.Place(bx, (nb.m_Plate.m_H - bb.GetHeight()) / 2);
		}
		m_Banners.Insert(nb);
		DZS_Log.Info("banner [" + id + "] " + markup);
	}

	// A progress plate (DZS_Jobs): the text, the shared progress bar with its number and, with a listener, a cancel
	// knob (a click calls listener.OnMessageButton("‹id›:cancel")). Called every frame while the work runs.
	void ShowProgress(string id, int order, string markup, float share, DZS_MessageListener listener)
	{
		bool cancel = listener != null;
		foreach (DZS_Banner b : m_Banners)
		{
			if (b.m_Id != id)
				continue;
			if (b.m_Bar && (b.m_Buttons != null) == cancel)
			{
				if (b.m_Markup != markup)
					LayoutProgress(b, markup);
				b.m_Bar.SetShare(share);
				return;
			}
			break;
		}
		HideBanner(id);
		DZS_Banner nb = new DZS_Banner();
		nb.m_Id = id;
		nb.m_Order = order;
		m_BannerSeq++;
		nb.m_Seq = m_BannerSeq;
		nb.m_Plate = new DZS_Plate(m_OverlayLayer, "info", markup, DZS_Font.READ, 13);
		nb.m_Bar = new DZS_ProgressBar(nb.m_Plate.m_Frame);
		if (cancel)
		{
			nb.m_Listener = listener;
			nb.m_Buttons = new array<ref DZS_Button>();
			DZS_Button cb = new DZS_Button(nb.m_Plate.m_Frame, "secondary", DZS_Texts.Get("core.dialog.cancel"), "", 24);
			cb.m_Tag = "cancel";
			cb.m_OnClick.Insert(nb.OnButton);
			nb.m_Buttons.Insert(cb);
		}
		LayoutProgress(nb, markup);
		nb.m_Bar.SetShare(share);
		m_Banners.Insert(nb);
	}

	protected void LayoutProgress(DZS_Banner b, string markup)
	{
		b.m_Markup = markup;
		DZS_Plate p = b.m_Plate;
		p.SetText(markup);
		float pad = DZS_Scale.Px(12);
		float barX = p.m_W;
		float w = barX + DZS_ProgressBar.Width() + pad;
		float h = p.m_H;
		h = Math.Max(h, DZS_ProgressBar.Height() + DZS_Scale.Px(10));
		DZS_Button knob = null;
		if (b.m_Buttons && b.m_Buttons.Count() > 0)
		{
			knob = b.m_Buttons[0];
			w += knob.GetWidth() + DZS_Scale.Px(8);
			h = Math.Max(h, knob.GetHeight() + DZS_Scale.Px(8));
		}
		p.m_W = Math.Round(w);
		p.m_H = Math.Round(h);
		p.Resize();
		float lineX = pad;
		if (p.m_HasIcon)
		{
			float icon = DZS_Scale.Px(16);
			DZS_W.Place(p.m_Icon, pad, (p.m_H - icon) / 2, icon, icon);
			lineX += icon + DZS_Scale.Px(8);
		}
		p.m_Line.Place(lineX, (p.m_H - p.m_Line.GetHeight()) / 2);
		b.m_Bar.Place(barX, (p.m_H - DZS_ProgressBar.Height()) / 2);
		if (knob)
			knob.Place(p.m_W - knob.GetWidth() - DZS_Scale.Px(8), (p.m_H - knob.GetHeight()) / 2);
	}

	void HideBanner(string id)
	{
		for (int i = 0; i < m_Banners.Count(); i++)
		{
			if (m_Banners[i].m_Id == id)
			{
				m_Banners[i].Destroy();
				m_Banners.RemoveOrdered(i);
				return;
			}
		}
	}

	// Unfolding by a plugin: "ok", "hidden" (section not shown) or "" (no such section).
	string UnfoldPanel(string panelId)
	{
		string r = m_LeftBar.Unfold(panelId);
		if (r == "")
			r = m_RightBar.Unfold(panelId);
		DZS_Log.Info("panel " + panelId + " unfold: " + r);
		return r;
	}

	bool HasBanner(string id)
	{
		foreach (DZS_Banner b : m_Banners)
		{
			if (b.m_Id == id)
				return true;
		}
		return false;
	}

	// Messages: a newer one of the same kind replaces the text in place and restarts the time.
	void ShowMessage(string kind, string tone, string markup, float seconds)
	{
		ShowMessageWithButton(kind, tone, markup, seconds, "", null);
	}

	// A message with a knob ("Show", "Save …"); it takes clicks.
	void ShowMessageWithButton(string kind, string tone, string markup, float seconds, string buttonLabel, DZS_MessageListener listener, string buttonIcon = "")
	{
		HideMessage(kind);
		DZS_Message nm = new DZS_Message();
		nm.m_Kind = kind;
		nm.m_Tone = tone;
		nm.m_Markup = markup;
		nm.m_Life = seconds;
		string hold;
		if (GetCLIParam("dzsMsgMin", hold) && hold.ToFloat() > seconds)
			nm.m_Life = hold.ToFloat();
		nm.m_Plate = new DZS_Plate(MessageLayer(), tone, markup);
		if (buttonLabel != "")
		{
			nm.m_Listener = listener;
			nm.m_Button = new DZS_Button(nm.m_Plate.m_Frame, "secondary", buttonLabel, buttonIcon, 24);
			nm.m_Button.m_OnClick.Insert(nm.OnButton);
			float bw = nm.m_Button.GetWidth();
			float pad = DZS_Scale.Px(8);
			nm.m_Plate.m_W += Math.Round(bw + pad);
			nm.m_Plate.m_H = Math.Max(nm.m_Plate.m_H, Math.Round(nm.m_Button.GetHeight() + DZS_Scale.Px(8)));
			nm.m_Plate.Resize();
			nm.m_Button.Place(nm.m_Plate.m_W - bw - pad, (nm.m_Plate.m_H - nm.m_Button.GetHeight()) / 2);
		}
		m_Messages.Insert(nm);
		while (m_Messages.Count() > 3)
		{
			m_Messages[0].Destroy();
			m_Messages.RemoveOrdered(0);
		}
		DZS_Log.Info("message [" + kind + "] " + markup);
	}

	// In the main menu the overlay layer stays hidden under the menu (DayZ 1.30) while the dialog layer shows:
	// messages go there, and the ones standing when the menu opens are made again in it.
	protected Widget MessageLayer()
	{
		if (m_Home || HasDialog())
			return m_DialogLayer;
		return m_OverlayLayer;
	}

	void RaiseMessages()
	{
		foreach (DZS_Message m : m_Messages)
		{
			if (m.m_Button)
				continue;
			m.m_Plate.Destroy();
			m.m_Plate = new DZS_Plate(MessageLayer(), m.m_Tone, m.m_Markup);
		}
	}

	void HideMessage(string kind)
	{
		for (int i = 0; i < m_Messages.Count(); i++)
		{
			if (m_Messages[i].m_Kind == kind)
			{
				m_Messages[i].Destroy();
				m_Messages.RemoveOrdered(i);
				return;
			}
		}
	}

	bool HasMessage(string kind)
	{
		foreach (DZS_Message m : m_Messages)
		{
			if (m.m_Kind == kind)
				return true;
		}
		return false;
	}

	override void OnNotice(DZS_Notice notice)
	{
		ShowMessage(notice.m_Kind, notice.m_Tone, DZS_Texts.Pick(notice.m_De, notice.m_En), notice.m_DurationMs / 1000.0);
	}

	protected void LayoutOverlays(float dt)
	{
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_SceneLayer, 0, 0, sw, sh);
		DZS_W.Place(m_MapLayer, 0, 0, sw, sh);
		float sx, sy, sW, sH;
		GetSceneRect(sx, sy, sW, sH);
		if (m_SceneFrameOn)
			m_SceneFrame.Place(sx, sy, sW, sH, DZS_Scale.Px(6));
		float cl = DZS_Scale.Px(10);
		float ct = Math.Max(1, DZS_Scale.Px(2));
		DZS_W.Place(m_CrossH, sw / 2 - cl, sh / 2 - ct / 2, 2 * cl, ct);
		DZS_W.Place(m_CrossV, sw / 2 - ct / 2, sh / 2 - cl, ct, 2 * cl);

		float by = DZS_Scale.Px(12);
		if (!m_UiHidden)
			by += DZS_Scale.Px(TOP_H);
		if (m_TopReserve > 0)
			by += m_TopReserve + DZS_Scale.Px(12);
		array<DZS_Banner> sorted = new array<DZS_Banner>();
		foreach (DZS_Banner b : m_Banners)
		{
			int at = 0;
			while (at < sorted.Count() && (sorted[at].m_Order < b.m_Order || (sorted[at].m_Order == b.m_Order && sorted[at].m_Seq < b.m_Seq)))
				at++;
			sorted.InsertAt(b, at);
		}
		foreach (DZS_Banner sb : sorted)
		{
			sb.m_Plate.Place((sw - sb.m_Plate.m_W) / 2, by);
			by += sb.m_Plate.m_H + DZS_Scale.Px(6);
		}

		float my = sh - DZS_Scale.Px(16);
		if (!m_UiHidden)
			my -= DZS_Scale.Px(BAR_H);
		for (int i = m_Messages.Count() - 1; i >= 0; i--)
		{
			DZS_Message m = m_Messages[i];
			// time counts only while it can be seen: a long frame (loading the main menu) adds one normal frame at most
			if (m.m_Plate && m.m_Plate.m_Frame && m.m_Plate.m_Frame.IsVisibleHierarchy())
				m.m_Age += Math.Min(dt, 0.05);
			if (m.m_Age >= m.m_Life + 0.15)
			{
				m.Destroy();
				m_Messages.RemoveOrdered(i);
				continue;
			}
		}
		for (int j = m_Messages.Count() - 1; j >= 0; j--)
		{
			DZS_Message mm = m_Messages[j];
			float a = 1;
			if (mm.m_Age > mm.m_Life)
				a = 1 - (mm.m_Age - mm.m_Life) / 0.15;
			mm.m_Plate.SetAlpha(Math.Clamp(a, 0, 1));
			my -= mm.m_Plate.m_H;
			mm.m_Plate.Place((sw - mm.m_Plate.m_W) / 2, my);
			my -= DZS_Scale.Px(6);
		}
	}

	// ---- pointer: hover, press, click and wheel for the clickable areas ----

	protected void UpdatePointer()
	{
		Widget under = null;
		if (IsCursorVisible())
			under = GetWidgetUnderCursor();
		DZS_Hit hit = null;
		if (under)
			hit = DZS_HitRegistry.Find(under);
		if (hit && hit.m_Surface)
			hit = null;
		DZS_Dialog top = TopDialog();
		if (hit && top && !DZS_W.IsInside(hit.m_W, top.m_Root) && !(m_Popup && m_Popup.Contains(hit.m_W)))
			hit = null;
		if (hit != m_HoverHit)
		{
			if (m_HoverHit && !m_HoverHit.m_Destroyed)
			{
				m_HoverHit.m_Hover = false;
				m_HoverHit.Changed();
			}
			m_HoverHit = hit;
			if (hit)
			{
				hit.m_Hover = true;
				hit.Changed();
			}
		}
		// while a menu is open, pointing at another menu title opens that menu
		if (m_Popup && m_OpenMenu != "context" && hit && hit.m_Listener)
		{
			DZS_Button tb = DZS_Button.Cast(hit.m_Listener);
			if (tb && m_Top.m_MenuButtons.Find(tb) >= 0 && tb.m_Tag != m_OpenMenu)
				OpenMenu(tb.m_Tag);
		}
		bool lmb = (GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) != 0;
		bool rmb = (GetMouseState(MouseState.RIGHT) & MB_PRESSED_MASK) != 0;
		bool lDown = lmb && !m_LmbWas;
		bool rDown = rmb && !m_RmbWas;
		bool lUp = !lmb && m_LmbWas;
		bool rUp = !rmb && m_RmbWas;
		m_LmbWas = lmb;
		m_RmbWas = rmb;
		if (lDown || rDown)
		{
			int button = MouseState.LEFT;
			if (!lDown)
				button = MouseState.RIGHT;
			HideTip();
			if (m_Recorder)
			{
				m_Recorder.CancelRecording();
				m_SwallowMouse = true;
			}
			else if (m_Popup && !(hit && m_Popup.Contains(hit.m_W)) && !(hit && DZS_Button.Cast(hit.m_Listener) && m_Top.m_MenuButtons.Find(DZS_Button.Cast(hit.m_Listener)) >= 0))
			{
				CloseMenu();
				m_SwallowMouse = true;
			}
			else
			{
				if (m_Focused && button == MouseState.LEFT && (!under || !DZS_W.IsInside(under, m_Focused.m_Frame)))
					Unfocus(true);
				if (hit && hit.m_Enabled)
				{
					m_PressHit = hit;
					m_PressButton = button;
					hit.m_Pressed = true;
					hit.Changed();
					if (hit.m_Listener)
						hit.m_Listener.OnHitPress(hit, button);
				}
			}
		}
		if ((lUp && m_PressButton == MouseState.LEFT) || (rUp && m_PressButton == MouseState.RIGHT))
		{
			DZS_Hit ph = m_PressHit;
			int pb = m_PressButton;
			m_PressHit = null;
			m_PressButton = -1;
			if (ph && !ph.m_Destroyed)
			{
				ph.m_Pressed = false;
				ph.Changed();
				if (ph.m_Listener)
					ph.m_Listener.OnHitRelease(ph);
				if (ph == hit && ph.m_Enabled && ph.m_Listener)
					ph.m_Listener.OnHitClick(ph, pb);
			}
		}
		if (!lmb && !rmb)
			m_SwallowMouse = false;
	}

	// Wheel over a surface belongs to the surface (rolling, field steps), never also to the scene.
	protected void UpdateWheel()
	{
		int wheel = GetMouseState(MouseState.WHEEL);
		if (wheel == 0 || !IsCursorVisible())
			return;
		Widget under = GetWidgetUnderCursor();
		if (!under)
			return;
		if (m_Focused && DZS_W.IsInside(under, m_Focused.m_Frame))
		{
			m_Focused.m_OnWheel.Invoke(m_Focused, wheel, DZS_UiRegistry.ModDown("Shift"));
			return;
		}
		if (m_Popup && m_Popup.Contains(under))
		{
			m_Popup.Wheel(wheel);
			return;
		}
		DZS_Dialog top = TopDialog();
		if (top)
		{
			if (DZS_W.IsInside(under, top.m_Box.m_W))
				top.Wheel(wheel);
			return;
		}
		if (m_Popup)
			return;
		Widget w = under;
		while (w)
		{
			DZS_Hit wh = DZS_HitRegistry.Find(w);
			if (!wh)
				break;
			if (wh.m_Listener && wh.m_Listener.OnHitWheel(wh, wheel))
				return;
			w = wh.m_W.GetParent();
		}
		if (DZS_W.IsInside(under, m_LeftBar.m_Surface.m_W))
			m_LeftBar.Wheel(under, wheel);
		else if (DZS_W.IsInside(under, m_RightBar.m_Surface.m_W))
			m_RightBar.Wheel(under, wheel);
	}

	// ---- keyboard for menus and dialogs ----

	protected void UpdateKeys()
	{
		bool esc = KeyState(KeyCode.KC_ESCAPE) != 0;
		bool escDown = esc && !m_EscWas;
		m_EscWas = esc;
		bool enter = KeyState(KeyCode.KC_RETURN) != 0 || KeyState(KeyCode.KC_NUMPADENTER) != 0;
		bool enterDown = enter && !m_EnterWas;
		m_EnterWas = enter;
		bool tab = KeyState(KeyCode.KC_TAB) != 0;
		bool tabDown = tab && !m_TabWas;
		m_TabWas = tab;
		bool space = Pressed("SPACE");
		bool up = Pressed("UP");
		bool down = Pressed("DOWN");
		bool left = Pressed("LEFT");
		bool right = Pressed("RIGHT");
		if (escDown || enterDown || tabDown || space || up || down)
			HideTip();
		if (m_Recorder)
		{
			if (escDown)
				DispatchEsc();
			return;
		}
		if (escDown)
		{
			if (DispatchEsc())
				DZS_Log.Info("esc handled");
			return;
		}
		if (m_Popup)
		{
			if (down)
				m_Popup.Move(1);
			if (up)
				m_Popup.Move(-1);
			if (enterDown)
				m_Popup.ChooseHighlight();
			if ((left || right) && m_OpenMenu != "context")
			{
				int i = m_Top.m_MenuIds.Find(m_OpenMenu);
				int n = m_Top.m_MenuIds.Count();
				if (left)
					i = (i + n - 1) % n;
				else
					i = (i + 1) % n;
				OpenMenu(m_Top.m_MenuIds[i]);
			}
			return;
		}
		DZS_Dialog top = TopDialog();
		if (top)
		{
			if (tabDown)
			{
				int dir = 1;
				if (DZS_UiRegistry.ModDown("Shift"))
					dir = -1;
				top.FocusStep(dir);
			}
			else if (enterDown)
				top.EnterKey();
			else if (space && !m_Focused)
				top.ActivateFocus();
			else if (down && !m_Focused)
				top.FocusArrow(1);
			else if (up && !m_Focused)
				top.FocusArrow(-1);
			return;
		}
		if (enterDown && m_Focused)
			CommitField(m_Focused, "enter");
		if (tabDown && m_Focused)
			CommitField(m_Focused, "tab");
	}

	// ---- tooltips: after 600 ms, below the element or above it, inside the window ----

	protected void HideTip()
	{
		if (m_Tip)
			m_Tip.Destroy();
		m_Tip = null;
		m_TipText = "";
		m_TipWait = 0.6;
		m_TipHit = m_HoverHit;
	}

	protected void UpdateTooltip(float dt)
	{
		string text = "";
		Widget anchor = null;
		bool pressed = (GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) != 0;
		if (m_HoverHit && !m_HoverHit.m_Destroyed && m_HoverHit != m_TipHit && !pressed)
		{
			if (!m_HoverHit.m_Enabled && m_HoverHit.m_TipReason != "")
				text = m_HoverHit.m_TipReason;
			else if (m_HoverHit.m_Tip != "")
			{
				text = m_HoverHit.m_Tip;
				string tipKeys = m_HoverHit.m_TipKeys;
				if (m_HoverHit.m_TipAction != "")
					tipKeys = m_Kernel.GetUi().TipBinding(m_HoverHit.m_TipAction);
				if (tipKeys != "")
					text += " · [" + tipKeys + "]";
			}
			anchor = m_HoverHit.m_W;
		}
		Widget under = GetWidgetUnderCursor();
		if (text == "" && under && IsCursorVisible() && !m_UiHidden)
		{
			foreach (DZS_StatusIcon icon : m_Icons)
			{
				if (DZS_W.IsInside(under, icon.m_Frame))
				{
					text = icon.m_Text;
					anchor = icon.m_Frame;
				}
			}
			if (m_MoreIcon.IsVisible() && DZS_W.IsInside(under, m_MoreIcon))
			{
				text = m_MoreTooltip;
				anchor = m_MoreIcon;
			}
			array<DZS_StatusItem> all = new array<DZS_StatusItem>();
			all.Insert(m_Group);
			foreach (DZS_StatusItem li : m_Left)
				all.Insert(li);
			foreach (DZS_StatusItem it : all)
			{
				if (it.m_Frame && it.m_Tooltip != "" && DZS_W.IsInside(under, it.m_Frame))
				{
					text = it.m_Tooltip;
					anchor = it.m_Frame;
				}
			}
		}
		if (m_HoverHit != m_TipHit)
			m_TipHit = null;
		if (text != m_TipText)
		{
			bool direct = m_Tip != null && text != "";
			m_TipText = text;
			if (m_Tip)
				m_Tip.Destroy();
			m_Tip = null;
			m_TipWait = 0.6;
			if (direct)
				m_TipWait = 0;
		}
		if (m_TipText == "")
			return;
		m_TipWait -= dt;
		if (m_TipWait > 0)
			return;
		if (!m_Tip)
		{
			m_Tip = new DZS_Plate(m_TipLayer, "tip", m_TipText, DZS_Font.READ, 13);
			m_TipAge = 0;
			DZS_Log.Info("tooltip '" + m_TipText + "'");
		}
		m_TipAge += dt;
		m_Tip.SetAlpha(Math.Clamp(m_TipAge / 0.1, 0, 1));
		int sw, sh;
		GetScreenSize(sw, sh);
		float ax, ay, aw, ah;
		if (anchor)
		{
			anchor.GetScreenPos(ax, ay);
			anchor.GetScreenSize(aw, ah);
		}
		else
		{
			int mx, my;
			GetMousePos(mx, my);
			ax = mx;
			ay = my;
		}
		float gap = DZS_Scale.Px(6);
		float tx = Math.Clamp(ax + aw / 2 - m_Tip.m_W / 2, 0, sw - m_Tip.m_W);
		float ty = ay + ah + gap;
		if (ty + m_Tip.m_H > sh)
			ty = ay - m_Tip.m_H - gap;
		m_Tip.Place(tx, Math.Max(0, ty));
	}

	// ---- per frame ----

	void Update(float dt)
	{
		if (!m_Root)
			return;
		RefreshInputBlock();
		if (!m_GameControl)
		{
			UpdateKeys();
			UpdatePointer();
			UpdateWheel();
			EnsureCursor();
		}
		UpdateSaveState();
		if (m_Popup)
			m_Popup.Update();
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_BarsLayer, 0, 0, sw, sh);
		DZS_W.Place(m_HomeLayer, 0, 0, sw, sh);
		DZS_W.Place(m_OverlayLayer, 0, 0, sw, sh);
		DZS_W.Place(m_PopupLayer, 0, 0, sw, sh);
		DZS_W.Place(m_DialogLayer, 0, 0, sw, sh);
		DZS_W.Place(m_TipLayer, 0, 0, sw, sh);
		float top = DZS_Scale.Px(TOP_H);
		float bottom = sh - DZS_Scale.Px(BAR_H);
		if (!m_UiHidden)
			m_Rail.Layout(top, bottom);
		m_LeftBar.m_Offset = m_Rail.Width();
		if (m_UiHidden)
			m_LeftBar.m_Offset = 0;
		m_LeftBar.Layout(dt, top, bottom);
		m_RightBar.Layout(dt, top, bottom);
		m_LeftBar.Update(dt);
		m_RightBar.Update(dt);
		float clipX, clipY, clipW, clipH;
		GetSceneRect(clipX, clipY, clipW, clipH);
		DZS_Clip.Set(clipX, clipY, clipW, clipH);
		if (!m_UiHidden)
		{
			m_Top.Layout();
			LayoutBar();
		}
		LayoutOverlays(dt);
		for (int i = 0; i < m_Dialogs.Count(); i++)
			m_Dialogs[i].Update(dt);
		UpdateTooltip(dt);
		RefreshInputBlock();
	}
}

class DZS_FieldResult : Managed
{
	bool m_Ok;
}

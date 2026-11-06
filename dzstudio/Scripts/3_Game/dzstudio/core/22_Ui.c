// UI frame of the kernel: actions with their keys, menu entries, panels
// and tools. Plugins register here; the screen builds the visible parts from these registries.
class DZS_Action : Managed
{
	string m_Id;
	string m_Plugin;
	string m_NameDe;
	string m_NameEn;
	ref array<string> m_DefaultKeys;
	ref array<string> m_When;
	string m_Group;
	bool m_Hold;
	bool m_ChangesSel;
	bool m_Listed = true;
	bool m_MustKeep;
	bool m_Tempo;
	bool m_Gesture;
	bool m_NoCtrl;
	string m_BlockedDefault;
	string m_BlockedBy;
	int m_Seq;

	string Name() { return DZS_Texts.Pick(m_NameDe, m_NameEn); }
}

class DZS_MenuEntry : Managed
{
	string m_Menu;
	string m_ActionId;
	int m_Order;
	bool m_Check;
}

// Content of a panel section; built and laid out by the screen, filled by the plugin.
class DZS_PanelContent : Managed
{
	// Part of the content the section shows (scroll offset and height, screen pixels), set before each Layout.
	float m_SectionTop;
	float m_SectionH = 100000;

	void Build(Widget parent) {}
	// Height the content wants at this width (screen pixels).
	float Layout(float width) { return 0; }
	void Update(float dt) {}
	// A content that fills its section takes all the height it gets and rolls itself (long lists).
	bool FillsView() { return false; }
	void SetView(float width, float height) {}
	bool Wheel(int delta) { return false; }
	// Knobs in the section head (right end): built once into the head, placed with the head's width and height;
	// returns the width they take.
	void BuildHead(Widget head) {}
	float LayoutHead(float right, float h) { return 0; }
}

class DZS_PanelInfo : Managed
{
	string m_Id;
	string m_Plugin;
	string m_TitleDe;
	string m_TitleEn;
	string m_Place;
	string m_Icon;
	string m_Meta;
	int m_Order;
	// fixed height in reference pixels, head included; 0 = shares the rest of the bar
	float m_FixedHeight;
	// a hidden section takes no place in its bar (shown by its plugin, e.g. only while a mode is on)
	bool m_Hidden;
	// the one main section of its bar: no collapse box, takes what the others leave
	bool m_Main;
	ref DZS_PanelContent m_Content;

	string Title() { return DZS_Texts.Pick(m_TitleDe, m_TitleEn); }
}

// A toolbar button for an action: group "tools" (exactly one active) or "switches" (on or off each).
class DZS_ToolInfo : Managed
{
	string m_Id;
	string m_Plugin;
	string m_NameDe;
	string m_NameEn;
	string m_Group;
	string m_Icon;
	int m_Order;
	int m_Seq;
	// action of the narrow arrow "▾" right of the tool ("" = none)
	string m_ArrowAction;
	// own tooltip instead of the name ("" = name), with the key of another action
	string m_TipDe;
	string m_TipEn;
	string m_TipKeyAction;
}

class DZS_KeyGroup : Managed
{
	string m_Key;
	string m_NameDe;
	string m_NameEn;
	int m_Order;
}

// Receives action triggers; a plugin answers for the actions it registered.
// A mode that takes over actions of other plugins while it runs (the loot editor: Del deletes points, Ctrl+V does
// nothing); Claims decides which.
class DZS_ActionOverride : Managed
{
	bool Claims(string actionId) { return false; }
	bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn) { return false; }
	void OnAction(string actionId) {}
}

class DZS_ActionHandler : Managed
{
	bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn) { return true; }
	void OnAction(string actionId) {}
	// Check mark in menus and "on" state of switches in the toolbar.
	bool IsActionChecked(string actionId) { return false; }
	// Optional text a menu tooltip shows instead of the name ("Undo: Move wooden crate").
	string GetActionTip(string actionId) { return ""; }
	// Optional name that follows the state ("Lock" / "Unlock"); "" = the registered name.
	string GetActionLabel(string actionId) { return ""; }
	// false = the entry is left out of menus for now (not just greyed out).
	bool IsActionListed(string actionId) { return true; }
}

class DZS_UiRegistry : Managed
{
	protected ref map<string, ref DZS_Action> m_Actions;
	protected ref array<string> m_ActionOrder;
	protected ref map<string, DZS_ActionHandler> m_Handlers;
	protected ref array<ref DZS_MenuEntry> m_Menu;
	protected ref map<string, ref DZS_PanelInfo> m_Panels;
	protected ref array<string> m_PanelOrder;
	protected ref map<string, ref DZS_ToolInfo> m_Tools;
	protected ref array<ref DZS_KeyGroup> m_Groups;
	protected ref map<string, bool> m_KeyWasDown;
	protected ref map<string, bool> m_HeldNow;
	protected ref map<string, bool> m_HeldWas;
	protected DZS_Settings m_Settings;
	protected bool m_InputBlocked;
	protected string m_Target = "scene";
	protected string m_SceneState = "none";
	protected string m_ActiveTool;
	protected int m_Seq;
	ref ScriptInvoker m_OnBindingsChanged;

	void DZS_UiRegistry(DZS_Settings settings)
	{
		m_Settings = settings;
		m_Actions = new map<string, ref DZS_Action>();
		m_ActionOrder = new array<string>();
		m_Handlers = new map<string, DZS_ActionHandler>();
		m_Menu = new array<ref DZS_MenuEntry>();
		m_Panels = new map<string, ref DZS_PanelInfo>();
		m_PanelOrder = new array<string>();
		m_Tools = new map<string, ref DZS_ToolInfo>();
		m_Groups = new array<ref DZS_KeyGroup>();
		m_KeyWasDown = new map<string, bool>();
		m_HeldNow = new map<string, bool>();
		m_HeldWas = new map<string, bool>();
		m_OnBindingsChanged = new ScriptInvoker();
		AddGroup("camera", "Kamera", "Camera", 10);
		AddGroup("ui", "Oberfläche", "Interface", 20);
		AddGroup("placement", "Platzieren", "Placing", 30);
		AddGroup("selection", "Auswahl", "Selection", 40);
		AddGroup("history", "Verlauf", "History", 50);
		AddGroup("projects", "Projekte", "Projects", 60);
		AddGroup("properties", "Eigenschaften", "Properties", 70);
		AddGroup("brush", "Pinsel", "Brush", 80);
		AddGroup("environment", "Wetter und Zeit", "Weather and time", 90);
	}

	protected void AddGroup(string key, string de, string en, int order)
	{
		DZS_KeyGroup g = new DZS_KeyGroup();
		g.m_Key = key;
		g.m_NameDe = de;
		g.m_NameEn = en;
		g.m_Order = order;
		m_Groups.Insert(g);
	}

	// A plugin with own keys outside the groups above gets a group with its name, after the bundled ones.
	void RegisterKeyGroup(string key, string de, string en)
	{
		if (FindGroup(key))
			return;
		AddGroup(key, de, en, 1000 + m_Groups.Count());
	}

	DZS_KeyGroup FindGroup(string key)
	{
		foreach (DZS_KeyGroup g : m_Groups)
		{
			if (g.m_Key == key)
				return g;
		}
		return null;
	}

	array<ref DZS_KeyGroup> GetGroups() { return m_Groups; }

	// Keys are position names of the game's key codes without "KC_", modifiers first: "Ctrl+Shift+Y".
	bool RegisterAction(string plugin, string id, string nameDe, string nameEn, array<string> defaultKeys, DZS_ActionHandler handler)
	{
		if (id.IndexOf(plugin + ".") != 0 || m_Actions.Contains(id))
		{
			DZS_Log.Error("action '" + id + "' rejected (must start with '" + plugin + ".' and be unique)");
			return false;
		}
		DZS_Action a = new DZS_Action();
		a.m_Id = id;
		a.m_Plugin = plugin;
		a.m_NameDe = nameDe;
		a.m_NameEn = nameEn;
		a.m_DefaultKeys = new array<string>();
		if (defaultKeys)
		{
			foreach (string k : defaultKeys)
			{
				if (k != "")
					a.m_DefaultKeys.Insert(k);
			}
		}
		a.m_When = new array<string>();
		a.m_When.Insert("scene");
		a.m_Group = plugin;
		m_Seq++;
		a.m_Seq = m_Seq;
		m_Actions.Set(id, a);
		m_ActionOrder.Insert(id);
		m_Handlers.Set(id, handler);
		return true;
	}

	// Where an action works: scene, hand, selection, map, fullsearch,
	// field, always (scene, hand, selection, map, full search and fields, never dialogs), character, mainmenu.
	void SetWhen(string id, string whenList)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return;
		a.m_When.Clear();
		whenList.Split(",", a.m_When);
	}

	void SetGroup(string id, string group)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
			a.m_Group = group;
	}

	// Mouse gestures and commands without key (menu, toolbar) do not appear in the key list.
	void SetListed(string id, bool listed)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
			a.m_Listed = listed;
	}

	void SetMustKeepKey(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
			a.m_MustKeep = true;
	}

	// Hold actions run while their key is down.
	void SetHold(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
			a.m_Hold = true;
	}

	// The speed keys ("Faster", "Slower"): never modifiers of a hold action.
	void SetTempo(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
		{
			a.m_Hold = true;
			a.m_Tempo = true;
		}
	}

	// Camera drives: Ctrl stops the camera, so no combination with Ctrl is taken in the recording.
	void SetNoCtrl(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
			a.m_NoCtrl = true;
	}

	// A fixed mouse gesture (turn with the right mouse button): held by its own rules, not rebindable.
	void SetGesture(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
		{
			a.m_Hold = true;
			a.m_Gesture = true;
			a.m_Listed = false;
		}
	}

	void AddMenuEntry(string menu, string actionId, int order, bool check = false)
	{
		DZS_MenuEntry e = new DZS_MenuEntry();
		e.m_Menu = menu;
		e.m_ActionId = actionId;
		e.m_Order = order;
		e.m_Check = check;
		m_Menu.Insert(e);
	}

	array<DZS_MenuEntry> GetMenuEntries(string menu)
	{
		array<DZS_MenuEntry> r = new array<DZS_MenuEntry>();
		foreach (DZS_MenuEntry e : m_Menu)
		{
			if (e.m_Menu != menu)
				continue;
			int at = 0;
			while (at < r.Count() && r[at].m_Order <= e.m_Order)
				at++;
			r.InsertAt(e, at);
		}
		return r;
	}

	// place: "left" or "right"; content may be null (header only).
	void RegisterPanel(string plugin, string id, string titleDe, string titleEn, string place, string icon = "puzzle", DZS_PanelContent content = null, int order = 0)
	{
		DZS_PanelInfo p = new DZS_PanelInfo();
		p.m_Id = id;
		p.m_Plugin = plugin;
		p.m_TitleDe = titleDe;
		p.m_TitleEn = titleEn;
		p.m_Place = place;
		p.m_Icon = icon;
		p.m_Order = order;
		p.m_Content = content;
		m_Panels.Set(id, p);
		m_PanelOrder.Insert(id);
	}

	// Marks a section as the main section of its bar; a second main section of the same bar is refused.
	bool SetPanelMain(string id)
	{
		DZS_PanelInfo p = m_Panels.Get(id);
		if (!p)
			return false;
		foreach (string oid : m_PanelOrder)
		{
			DZS_PanelInfo o = m_Panels.Get(oid);
			if (o && o != p && o.m_Main && o.m_Place == p.m_Place)
			{
				DZS_Log.Warn("panel " + id + ": " + p.m_Place + " bar has a main section already (" + oid + "), refused");
				return false;
			}
		}
		p.m_Main = true;
		return true;
	}

	// A section of fixed height keeps it at the bottom of its bar; the others share the rest.
	void SetPanelFixedHeight(string id, float refHeight)
	{
		DZS_PanelInfo p = m_Panels.Get(id);
		if (p)
			p.m_FixedHeight = refHeight;
	}

	void SetPanelShown(string id, bool shown)
	{
		DZS_PanelInfo p = m_Panels.Get(id);
		if (!p || p.m_Hidden == !shown)
			return;
		p.m_Hidden = !shown;
		DZS_Log.Info("panel " + id + " shown " + DZS_Str.Bool(shown));
	}

	bool IsPanelShown(string id)
	{
		DZS_PanelInfo p = m_Panels.Get(id);
		return p && !p.m_Hidden;
	}

	array<DZS_PanelInfo> GetPanels(string place)
	{
		array<DZS_PanelInfo> r = new array<DZS_PanelInfo>();
		foreach (string id : m_PanelOrder)
		{
			DZS_PanelInfo p = m_Panels.Get(id);
			if (p.m_Place != place)
				continue;
			int at = 0;
			while (at < r.Count() && r[at].m_Order <= p.m_Order)
				at++;
			r.InsertAt(p, at);
		}
		return r;
	}

	DZS_PanelInfo GetPanel(string id) { return m_Panels.Get(id); }

	// group "tools": exactly one of them is active; "switches": independent on/off (IsActionChecked).
	void RegisterTool(string plugin, string id, string nameDe, string nameEn, string group = "switches", string icon = "puzzle", int order = 0)
	{
		DZS_ToolInfo t = new DZS_ToolInfo();
		t.m_Id = id;
		t.m_Plugin = plugin;
		t.m_NameDe = nameDe;
		t.m_NameEn = nameEn;
		t.m_Group = group;
		t.m_Icon = icon;
		t.m_Order = order;
		m_Seq++;
		t.m_Seq = m_Seq;
		m_Tools.Set(id, t);
		m_ToolsRev++;
		if (group == "tools" && m_ActiveTool == "")
			m_ActiveTool = id;
	}

	array<DZS_ToolInfo> GetTools(string group)
	{
		array<DZS_ToolInfo> r = new array<DZS_ToolInfo>();
		foreach (string id, DZS_ToolInfo t : m_Tools)
		{
			if (t.m_Group != group)
				continue;
			int at = 0;
			while (at < r.Count() && (r[at].m_Order < t.m_Order || (r[at].m_Order == t.m_Order && r[at].m_Seq < t.m_Seq)))
				at++;
			r.InsertAt(t, at);
		}
		return r;
	}

	string GetActiveTool() { return m_ActiveTool; }

	// Bumped when tools or their arrows change, so the top bar rebuilds its buttons.
	int m_ToolsRev;
	// Screen rectangle of the arrow whose action runs right now (to open a menu below it).
	float m_ArrowX, m_ArrowY, m_ArrowW, m_ArrowH;
	bool m_FromArrow;

	void SetToolArrow(string id, string action)
	{
		DZS_ToolInfo t = m_Tools.Get(id);
		if (!t)
			return;
		t.m_ArrowAction = action;
		m_ToolsRev++;
	}

	void SetToolIcon(string id, string icon)
	{
		DZS_ToolInfo t = m_Tools.Get(id);
		if (t)
			t.m_Icon = icon;
	}

	DZS_ToolInfo GetTool(string id) { return m_Tools.Get(id); }

	void SetToolTip(string id, string de, string en, string keyAction)
	{
		DZS_ToolInfo t = m_Tools.Get(id);
		if (!t)
			return;
		t.m_TipDe = de;
		t.m_TipEn = en;
		t.m_TipKeyAction = keyAction;
	}

	void SetActiveTool(string id)
	{
		DZS_ToolInfo t = m_Tools.Get(id);
		if (!t || t.m_Group != "tools" || m_ActiveTool == id)
			return;
		m_ActiveTool = id;
		DZS_Log.Info("tool " + id);
	}

	DZS_Action GetAction(string id) { return m_Actions.Get(id); }
	array<string> GetActionIds() { return m_ActionOrder; }

	// ---- bindings ----

	// Valid form: modifiers Ctrl, Shift, Alt in this order, then one key the game knows; a modifier key
	// (LSHIFT …) only alone.
	static bool IsValidBinding(string binding)
	{
		if (binding == "")
			return false;
		array<string> parts = new array<string>();
		binding.Split("+", parts);
		int n = parts.Count();
		if (n == 0 || n > 4)
			return false;
		array<string> order = {"Ctrl", "Shift", "Alt"};
		int last = -1;
		for (int i = 0; i < n - 1; i++)
		{
			int idx = order.Find(parts[i]);
			if (idx <= last)
				return false;
			last = idx;
		}
		string key = parts[n - 1];
		if (order.Find(key) >= 0)
			return false;
		if (IsModifierKey(key) && n > 1)
			return false;
		if (key == "MouseMiddle" || key == "Mouse4" || key == "Mouse5")
			return true;
		if (key == "MouseLeft" || key == "MouseRight" || key == "Wheel")
			return false;
		if (key.IndexOf("KC_") == 0)
			return false;
		return KeyCodeOf(key) >= 0;
	}

	static bool IsModifierKey(string key)
	{
		return key == "LSHIFT" || key == "RSHIFT" || key == "LCONTROL" || key == "RCONTROL" || key == "LMENU" || key == "RMENU";
	}

	static string KeyOf(string binding)
	{
		array<string> parts = new array<string>();
		binding.Split("+", parts);
		if (parts.Count() == 0)
			return "";
		return parts[parts.Count() - 1];
	}

	static array<string> ModsOf(string binding)
	{
		array<string> parts = new array<string>();
		binding.Split("+", parts);
		array<string> r = new array<string>();
		for (int i = 0; i < parts.Count() - 1; i++)
			r.Insert(parts[i]);
		return r;
	}

	// The defaults an action really has: a default already taken when the action first appeared is dropped.
	array<string> GetEffectiveDefaults(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		array<string> r = new array<string>();
		if (!a)
			return r;
		foreach (string k : a.m_DefaultKeys)
		{
			if (k != a.m_BlockedDefault)
				r.Insert(k);
		}
		return r;
	}

	// Stored bindings of the user: invalid ones count as absent; none left means the default; [] is "no key".
	array<string> GetBindings(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return new array<string>();
		array<string> custom = m_Settings.GetKeys(id);
		if (custom)
		{
			if (custom.Count() == 0)
				return custom;
			array<string> valid = new array<string>();
			foreach (string c : custom)
			{
				if (IsValidBinding(c) && valid.Find(c) < 0 && valid.Count() < 2)
					valid.Insert(c);
			}
			if (valid.Count() > 0)
				return valid;
		}
		return GetEffectiveDefaults(id);
	}

	bool IsCustom(string id)
	{
		array<string> b = GetBindings(id);
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return false;
		if (b.Count() != a.m_DefaultKeys.Count())
			return true;
		for (int i = 0; i < b.Count(); i++)
		{
			if (b[i] != a.m_DefaultKeys[i])
				return true;
		}
		return false;
	}

	void SetBindings(string id, array<string> keys)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return;
		bool same = keys.Count() == a.m_DefaultKeys.Count();
		for (int i = 0; same && i < keys.Count(); i++)
			same = keys[i] == a.m_DefaultKeys[i];
		if (same)
			m_Settings.SetKeys(id, null);
		else
			m_Settings.SetKeys(id, keys);
		DZS_Log.Info("keys " + id + " = [" + Joined(keys) + "]");
		m_OnBindingsChanged.Invoke();
	}

	void ResetBindings(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return;
		m_Settings.SetKeys(id, null);
		a.m_BlockedDefault = "";
		a.m_BlockedBy = "";
		m_OnBindingsChanged.Invoke();
	}

	void ResetAllBindings()
	{
		foreach (string id : m_ActionOrder)
		{
			DZS_Action a = m_Actions.Get(id);
			a.m_BlockedDefault = "";
			a.m_BlockedBy = "";
			m_Settings.SetKeys(id, null);
		}
		m_OnBindingsChanged.Invoke();
	}

	static string Joined(array<string> keys)
	{
		string s = "";
		foreach (string k : keys)
		{
			if (s != "")
				s += ", ";
			s += k;
		}
		return s;
	}

	// The first binding of an action, for key caps in hints ("" if none).
	// The key for a tooltip: the first key, only while it works in the current state.
	string TipBinding(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a || !WorksNow(a))
			return "";
		return FirstBinding(id);
	}

	string FirstBinding(string id)
	{
		array<string> binds = GetBindings(id);
		if (!binds || binds.Count() == 0)
			return "";
		return binds[0];
	}

	// ---- when: which states two actions can share ----

	protected static void ExpandWhen(array<string> when, map<string, bool> states)
	{
		foreach (string w : when)
		{
			if (w == "scene")
			{
				states.Set("none", true);
				states.Set("hand", true);
				states.Set("selection", true);
				states.Set("drag", true);
			}
			else if (w == "selection")
			{
				states.Set("selection", true);
				states.Set("drag", true);
			}
			else if (w == "always")
			{
				states.Set("none", true);
				states.Set("hand", true);
				states.Set("selection", true);
				states.Set("map", true);
				states.Set("fullsearch", true);
				states.Set("field", true);
			}
			else
				states.Set(w, true);
		}
	}

	static bool WhenOverlap(DZS_Action a, DZS_Action b)
	{
		map<string, bool> sa = new map<string, bool>();
		map<string, bool> sb = new map<string, bool>();
		ExpandWhen(a.m_When, sa);
		ExpandWhen(b.m_When, sb);
		foreach (string k, bool v : sa)
		{
			if (sb.Contains(k))
				return true;
		}
		return false;
	}

	// Actions that share a binding with this one in a state where both apply.
	array<string> ConflictsOf(string id)
	{
		array<string> r = new array<string>();
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return r;
		array<string> mine = GetBindings(id);
		foreach (string oid : m_ActionOrder)
		{
			if (oid == id)
				continue;
			DZS_Action o = m_Actions.Get(oid);
			if (!o.m_Listed || !WhenOverlap(a, o))
				continue;
			array<string> theirs = GetBindings(oid);
			foreach (string k : mine)
			{
				if (theirs.Find(k) >= 0 && r.Find(oid) < 0)
					r.Insert(oid);
			}
		}
		return r;
	}

	// Number of key combinations bound to more than one action at once.
	int CountConflictCombos()
	{
		map<string, bool> combos = new map<string, bool>();
		foreach (string id : m_ActionOrder)
		{
			DZS_Action a = m_Actions.Get(id);
			if (!a.m_Listed)
				continue;
			array<string> cs = ConflictsOf(id);
			if (cs.Count() == 0)
				continue;
			array<string> mine = GetBindings(id);
			foreach (string oid : cs)
			{
				array<string> theirs = GetBindings(oid);
				foreach (string k : mine)
				{
					if (theirs.Find(k) >= 0)
						combos.Set(k, true);
				}
			}
		}
		return combos.Count();
	}

	// Called once all plugins are registered: a default key of an action that is new on this installation
	// and already taken (by the user or an earlier action) is not given to it.
	void SettleNewDefaults()
	{
		DZS_Json known = m_Settings.Get("core.knownActions");
		map<string, bool> seen = new map<string, bool>();
		bool first = !known || !known.IsArr();
		if (!first)
		{
			for (int i = 0; i < known.Count(); i++)
			{
				if (known.At(i).IsStr())
					seen.Set(known.At(i).m_Str, true);
			}
		}
		DZS_Json list = DZS_Json.Arr();
		bool changed = false;
		foreach (string id : m_ActionOrder)
		{
			DZS_Action a = m_Actions.Get(id);
			list.Add(DZS_Json.Str(id));
			if (!seen.Contains(id))
				changed = true;
			if (first || seen.Contains(id) || !a.m_Listed)
				continue;
			foreach (string k : a.m_DefaultKeys)
			{
				string by = TakenBy(a, k);
				if (by != "")
				{
					a.m_BlockedDefault = k;
					a.m_BlockedBy = by;
					array<string> rest = GetEffectiveDefaults(id);
					m_Settings.SetKeys(id, rest);
					DZS_Log.Info("default " + k + " of new action " + id + " is taken by " + by);
				}
			}
		}
		if (changed || first)
			m_Settings.Set("core.knownActions", list);
	}

	protected string TakenBy(DZS_Action a, string key)
	{
		foreach (string oid : m_ActionOrder)
		{
			if (oid == a.m_Id)
				continue;
			DZS_Action o = m_Actions.Get(oid);
			if (o.m_Seq > a.m_Seq && !m_Settings.GetKeys(oid))
				continue;
			if (!WhenOverlap(a, o))
				continue;
			if (GetBindings(oid).Find(key) >= 0)
				return oid;
		}
		return "";
	}

	// ---- triggering ----

	protected ref DZS_ActionOverride m_Override;

	void SetOverride(DZS_ActionOverride o)
	{
		m_Override = o;
	}

	// Pressed now with exactly the modifiers of one of its bindings.
	bool BindingHeld(string id)
	{
		array<string> bs = GetBindings(id);
		foreach (string b : bs)
		{
			if (BindingDown(b))
				return true;
		}
		return false;
	}

	// The action changes the selected objects at once (Del, H, L …): off while their change has to wait.
	void SetChangesSelection(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (a)
			a.m_ChangesSel = true;
	}

	bool IsActionEnabled(string id, out string reasonDe, out string reasonEn)
	{
		if (DZS_EditGate.ActionOff(id, reasonDe, reasonEn))
			return false;
		DZS_Action wa = m_Actions.Get(id);
		if (wa && wa.m_ChangesSel && DZS_ChangeWait.Reason() != "")
		{
			reasonDe = DZS_ChangeWait.Reason();
			reasonEn = reasonDe;
			return false;
		}
		if (m_Override && m_Override.Claims(id))
			return m_Override.IsActionEnabled(id, reasonDe, reasonEn);
		DZS_ActionHandler h = m_Handlers.Get(id);
		if (!h)
			return false;
		return h.IsActionEnabled(id, reasonDe, reasonEn);
	}

	bool IsActionChecked(string id)
	{
		DZS_ActionHandler h = m_Handlers.Get(id);
		return h && h.IsActionChecked(id);
	}

	string GetActionTip(string id)
	{
		DZS_ActionHandler h = m_Handlers.Get(id);
		if (!h)
			return "";
		return h.GetActionTip(id);
	}

	bool IsActionListed(string id)
	{
		DZS_ActionHandler h = m_Handlers.Get(id);
		return !h || h.IsActionListed(id);
	}

	string GetActionLabel(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return "";
		DZS_ActionHandler h = m_Handlers.Get(id);
		string label = "";
		if (h)
			label = h.GetActionLabel(id);
		if (label != "")
			return label;
		return a.Name();
	}

	bool Trigger(string id)
	{
		string gd, ge;
		if (DZS_EditGate.ActionOff(id, gd, ge))
			return false;
		if (m_Override && m_Override.Claims(id))
		{
			string od, oe;
			if (!m_Override.IsActionEnabled(id, od, oe))
				return false;
			DZS_Log.Info("action " + id + " (taken over)");
			m_Override.OnAction(id);
			return true;
		}
		DZS_ActionHandler h = m_Handlers.Get(id);
		string rd, re;
		if (!h || !h.IsActionEnabled(id, rd, re))
			return false;
		DZS_Log.Info("action " + id);
		h.OnAction(id);
		return true;
	}

	// The screen tells who has the keyboard: scene, map, fullsearch, field, dialog, menu, recording, mainmenu,
	// character; and inside the scene whether something is in the hand or selected (none, hand, selection).
	void SetKeyboardTarget(string target)
	{
		m_Target = target;
		m_InputBlocked = target != "scene";
	}

	string GetKeyboardTarget() { return m_Target; }

	void SetSceneState(string state)
	{
		m_SceneState = state;
	}

	// Whether an action may fire for the current keyboard target.
	bool WorksNow(DZS_Action a)
	{
		string t = m_Target;
		if (t == "dialog" || t == "menu" || t == "recording")
			return false;
		if (t == "scene")
		{
			string s = m_SceneState;
			foreach (string w : a.m_When)
			{
				if (w == "scene" || w == "always" || w == s)
					return true;
			}
			return false;
		}
		foreach (string w2 : a.m_When)
		{
			if (w2 == t)
				return true;
			if (w2 == "always" && (t == "map" || t == "fullsearch" || t == "field"))
				return true;
		}
		return false;
	}

	// Whether the keys of an action work while the map or the full-screen search has the keyboard.
	bool WorksIn(string id, string target)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return false;
		foreach (string w : a.m_When)
		{
			if (w == target || w == "always")
				return true;
		}
		return false;
	}

	// Kept for the camera of section 2: true while a field, dialog, menu or the map has the keyboard.
	void SetInputBlocked(bool blocked)
	{
		m_InputBlocked = blocked;
	}

	bool IsInputBlocked() { return m_InputBlocked; }

	static bool ModDown(string mod)
	{
		if (mod == "Ctrl")
			return KeyState(KeyCode.KC_LCONTROL) != 0 || KeyState(KeyCode.KC_RCONTROL) != 0;
		if (mod == "Shift")
			return KeyState(KeyCode.KC_LSHIFT) != 0 || KeyState(KeyCode.KC_RSHIFT) != 0;
		if (mod == "Alt")
			return KeyState(KeyCode.KC_LMENU) != 0 || KeyState(KeyCode.KC_RMENU) != 0;
		return false;
	}

	protected static ref map<string, int> s_KeyCodes;

	static map<string, int> KeyCodes()
	{
		if (!s_KeyCodes)
		{
			s_KeyCodes = new map<string, int>();
			typename e = KeyCode;
			int cnt = e.GetVariableCount();
			for (int i = 0; i < cnt; i++)
			{
				int val;
				if (e.GetVariableType(i) == int && e.GetVariableValue(null, i, val))
				{
					string n = e.GetVariableName(i);
					if (n.IndexOf("KC_") == 0)
						s_KeyCodes.Set(n.Substring(3, n.Length() - 3), val);
				}
			}
		}
		return s_KeyCodes;
	}

	static int KeyCodeOf(string keyName)
	{
		int code;
		if (!KeyCodes().Find(keyName, code))
			return -1;
		return code;
	}

	protected static UAInput s_Mouse4;
	protected static UAInput s_Mouse5;

	protected static UAInput MouseInput(string name, string combo)
	{
		UAInput inp = GetUApi().GetInputByName(name);
		if (!inp)
		{
			inp = GetUApi().RegisterInput(name, "dzstudio " + combo, "core");
			if (inp)
			{
				inp.BindCombo(combo);
				GetUApi().UpdateControls();
			}
		}
		return inp;
	}

	// Keys by position (game key code names) and the mouse buttons MouseLeft/Right/Middle, Mouse4, Mouse5.
	static bool KeyDown(string keyName)
	{
		if (keyName == "MouseLeft")
			return (GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) != 0;
		if (keyName == "MouseRight")
			return (GetMouseState(MouseState.RIGHT) & MB_PRESSED_MASK) != 0;
		if (keyName == "MouseMiddle")
			return (GetMouseState(MouseState.MIDDLE) & MB_PRESSED_MASK) != 0;
		if (keyName == "Mouse4")
		{
			if (!s_Mouse4)
				s_Mouse4 = MouseInput("UADzsMouse4", "mB4");
			return s_Mouse4 && s_Mouse4.LocalValue() > 0;
		}
		if (keyName == "Mouse5")
		{
			if (!s_Mouse5)
				s_Mouse5 = MouseInput("UADzsMouse5", "mB5");
			return s_Mouse5 && s_Mouse5.LocalValue() > 0;
		}
		int code = KeyCodeOf(keyName);
		return code >= 0 && KeyState(code) != 0;
	}

	// Pressed now with exactly these modifiers (both sides count; speed keys count like any modifier here).
	protected bool BindingDown(string binding)
	{
		string keyName = KeyOf(binding);
		if (keyName == "" || !KeyDown(keyName))
			return false;
		if (IsModifierKey(keyName))
			return true;
		array<string> mods = ModsOf(binding);
		bool wantCtrl = mods.Find("Ctrl") >= 0;
		bool wantShift = mods.Find("Shift") >= 0;
		bool wantAlt = mods.Find("Alt") >= 0;
		return ModDown("Ctrl") == wantCtrl && ModDown("Shift") == wantShift && ModDown("Alt") == wantAlt;
	}

	// Physical keys bound to the speed actions; they are never modifiers of a hold action.
	protected void TempoKeys(map<string, bool> keys)
	{
		foreach (string id : m_ActionOrder)
		{
			DZS_Action a = m_Actions.Get(id);
			if (!a.m_Tempo)
				continue;
			array<string> tb = GetBindings(id);
			foreach (string b : tb)
			{
				if (ModsOf(b).Count() == 0)
					keys.Set(KeyOf(b), true);
			}
		}
	}

	protected bool HoldModDown(string mod, map<string, bool> tempo)
	{
		if (mod == "Ctrl")
			return (KeyState(KeyCode.KC_LCONTROL) != 0 && !tempo.Contains("LCONTROL")) || (KeyState(KeyCode.KC_RCONTROL) != 0 && !tempo.Contains("RCONTROL"));
		if (mod == "Shift")
			return (KeyState(KeyCode.KC_LSHIFT) != 0 && !tempo.Contains("LSHIFT")) || (KeyState(KeyCode.KC_RSHIFT) != 0 && !tempo.Contains("RSHIFT"));
		if (mod == "Alt")
			return (KeyState(KeyCode.KC_LMENU) != 0 && !tempo.Contains("LMENU")) || (KeyState(KeyCode.KC_RMENU) != 0 && !tempo.Contains("RMENU"));
		return false;
	}

	// Hold actions: per held key the matching action with the most modifiers wins; held speed keys do not count.
	protected void ResolveHolds()
	{
		m_HeldNow.Clear();
		map<string, bool> tempo = new map<string, bool>();
		TempoKeys(tempo);
		map<string, int> best = new map<string, int>();
		map<string, ref array<string>> winners = new map<string, ref array<string>>();
		foreach (string id : m_ActionOrder)
		{
			DZS_Action a = m_Actions.Get(id);
			if (!a.m_Hold || a.m_Gesture)
				continue;
			if (!WorksNow(a))
				continue;
			array<string> hb = GetBindings(id);
			foreach (string b : hb)
			{
				string key = KeyOf(b);
				if (!KeyDown(key))
					continue;
				if (a.m_Tempo)
				{
					m_HeldNow.Set(id, true);
					continue;
				}
				array<string> mods = ModsOf(b);
				bool ok = true;
				foreach (string m : mods)
				{
					if (!HoldModDown(m, tempo))
						ok = false;
				}
				if (!ok)
					continue;
				int n = mods.Count();
				int cur = -1;
				if (best.Contains(key))
					cur = best.Get(key);
				if (n > cur)
				{
					best.Set(key, n);
					array<string> w = new array<string>();
					w.Insert(id);
					winners.Set(key, w);
				}
				else if (n == cur && winners.Get(key).Find(id) < 0)
					winners.Get(key).Insert(id);
			}
		}
		foreach (string k, array<string> ws : winners)
		{
			foreach (string wid : ws)
				m_HeldNow.Set(wid, true);
		}
	}

	// Gestures keep their own rule: key and all their modifiers down.
	protected bool GestureHeld(string id)
	{
		array<string> gb = GetBindings(id);
		foreach (string b : gb)
		{
			if (!KeyDown(KeyOf(b)))
				continue;
			bool ok = true;
			array<string> gm = ModsOf(b);
			foreach (string m : gm)
			{
				if (!ModDown(m))
					ok = false;
			}
			if (ok)
				return true;
		}
		return false;
	}

	bool IsHeld(string id)
	{
		DZS_Action a = m_Actions.Get(id);
		if (!a)
			return false;
		if (a.m_Gesture)
			return GestureHeld(id);
		return m_HeldNow.Contains(id);
	}

	void Update(float dt)
	{
		ResolveHolds();
		foreach (string id : m_ActionOrder)
		{
			DZS_Action act = m_Actions.Get(id);
			if (act.m_Hold)
				continue;
			array<string> binds = GetBindings(id);
			foreach (string b : binds)
			{
				bool down = BindingDown(b);
				string memo = id + "|" + b;
				bool was = m_KeyWasDown.Get(memo);
				m_KeyWasDown.Set(memo, down);
				if (down && !was && WorksNow(act) && GetGame().GetUIManager().GetMenu() == null)
				{
					DZS_Log.Info("key " + b + " -> " + id);
					Trigger(id);
				}
			}
		}
	}
}

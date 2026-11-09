// The microkernel of one editor session: scene model instances, the operation inlet with history,
// events, the plugin register, the UI frame and the settings. It knows no file formats and no storage.
class DZS_Kernel : DZS_ActionHandler
{
	protected static ref DZS_Kernel s_Kernel;

	protected string m_World;
	protected ref DZS_TypeRegistry m_Types;
	protected ref DZS_EventBus m_Events;
	protected ref DZS_Settings m_Settings;
	protected ref DZS_UiRegistry m_Ui;
	protected ref DZS_Screen m_Screen;
	protected ref array<ref DZS_Plugin> m_Plugins;
	protected ref map<string, DZS_Plugin> m_PluginsByName;
	protected ref array<ref DZS_Instance> m_Instances;
	protected DZS_Instance m_Shown;
	protected string m_AuthorId;
	protected string m_AuthorName;
	protected bool m_Protocol;
	protected ref map<string, bool> m_Gestures;
	protected ref map<string, bool> m_WheelOwners;
	protected int m_InstanceCount;
	protected DZS_ActionHandler m_SaveHandler;
	protected bool m_BrokenNotice;
	protected int m_PointerX = -1;
	protected int m_PointerY;
	protected ref array<ref DZS_Format> m_Formats;
	protected ref map<string, ref Managed> m_Services;
	protected ref DZS_GroupMaker m_GroupMaker;
	protected string m_StatusGroup;
	protected string m_SceneTool;
	protected ref map<string, bool> m_FramesTaken = new map<string, bool>();

	static DZS_Kernel Get() { return s_Kernel; }

	static DZS_Kernel Start(string world)
	{
		if (s_Kernel)
			s_Kernel.Shutdown();
		DZS_BarGroups.Clear();
		DZS_SheetParts.Clear();
		s_Kernel = new DZS_Kernel(world);
		s_Kernel.Boot();
		return s_Kernel;
	}

	void DZS_Kernel(string world)
	{
		m_World = world;
		m_Types = new DZS_TypeRegistry();
		m_Events = new DZS_EventBus();
		m_Settings = new DZS_Settings(m_Events);
		m_Ui = new DZS_UiRegistry(m_Settings);
		m_Plugins = new array<ref DZS_Plugin>();
		m_PluginsByName = new map<string, DZS_Plugin>();
		m_Instances = new array<ref DZS_Instance>();
		m_Gestures = new map<string, bool>();
		m_WheelOwners = new map<string, bool>();
		m_Formats = new array<ref DZS_Format>();
		m_Services = new map<string, ref Managed>();
	}

	// ---- services: a plugin offers an interface object under a name, others ask for it (catalog.classes) ----

	void SetService(string name, Managed service)
	{
		m_Services.Set(name, service);
	}

	Managed GetService(string name)
	{
		return m_Services.Get(name);
	}

	// ---- formats: plugins register file formats; a file becomes a group of the first that takes it ----

	void RegisterFormat(DZS_Format f)
	{
		if (f && !GetFormat(f.GetId()))
			m_Formats.Insert(f);
	}

	DZS_Format GetFormat(string id)
	{
		foreach (DZS_Format f : m_Formats)
		{
			if (f.GetId() == id)
				return f;
		}
		return null;
	}

	DZS_Format FormatForFile(string fileName)
	{
		foreach (DZS_Format f : m_Formats)
		{
			if (f.HandlesFile(fileName))
				return f;
		}
		return null;
	}

	bool HasWorld() { return m_World != ""; }

	string GetWorld() { return m_World; }
	DZS_TypeRegistry GetTypes() { return m_Types; }
	DZS_EventBus GetEvents() { return m_Events; }
	DZS_Settings GetSettings() { return m_Settings; }
	DZS_UiRegistry GetUi() { return m_Ui; }
	DZS_Screen GetScreen() { return m_Screen; }
	string GetAuthorId() { return m_AuthorId; }
	string GetAuthorName() { return m_AuthorName; }
	DZS_Plugin GetPlugin(string name) { return m_PluginsByName.Get(name); }

	protected void Boot()
	{
		DZS_Log.Info("kernel start, api " + DZS_API_VERSION + ", world " + m_World);
		DZS_Version.CheckConfig();
		DZS_CoreTexts.Register();
		m_Settings.Define("core.protocol", DZS_Json.Boolean(false));
		m_Settings.DefineToggle("core.pointerHints", "ui", "Hinweise am Zeiger", "Hints at the pointer", "Zeilen mit Tasten im Hinweis neben dem Mauszeiger", "Lines with keys in the hint next to the pointer", true);
		DZS_LegacyNames.Run();
		m_Settings.Load();
		InitAuthor();
		string cli;
		m_Protocol = GetCLIParam("dzsProtocol", cli) || (m_Settings.IsDeveloper() && m_Settings.GetBool("core.protocol"));

		RegisterCoreTypes();
		RegisterCoreActions();
		m_Screen = new DZS_Screen(this);
		m_Events.Subscribe(m_Screen);
		m_Screen.Build();
		m_BrokenNotice = m_Settings.WasBroken();
		LoadPlugins();
		m_Ui.SettleNewDefaults();
		foreach (DZS_Plugin p : m_Plugins)
			p.OnSessionStart(m_World);
		DZS_Log.Info("kernel ready, " + m_Plugins.Count() + " plugin(s), author " + m_AuthorId + " (" + m_AuthorName + ")");
	}

	protected void InitAuthor()
	{
		m_AuthorId = m_Settings.GetString("core.installationId");
		if (m_AuthorId == "")
		{
			m_AuthorId = DZS_Random.Code(16);
			m_Settings.Set("core.installationId", DZS_Json.Str(m_AuthorId));
			m_Settings.Flush();
		}
		GetGame().GetPlayerName(m_AuthorName);
	}

	protected void RegisterCoreTypes()
	{
		m_Types.RegisterElementKind("core", DZS_KIND_PROJECT, 1, null);
		array<string> underProject = {DZS_KIND_PROJECT};
		m_Types.RegisterElementKind("core", DZS_KIND_GROUP, 1, underProject);
		array<string> underGroup = {DZS_KIND_GROUP};
		m_Types.RegisterElementKind("core", DZS_KIND_OBJECT, 1, underGroup);
		m_Types.RegisterOperationType("core", "core:load", 1);
		m_Types.RegisterOperationType("core", "core:undo", 1);
		m_Types.RegisterOperationType("core", "core:redo", 1);
		m_Types.RegisterOperationType("core", "core:revert", 1);
	}

	protected void RegisterCoreActions()
	{
		array<string> undoKeys = {"Ctrl+Y"};
		array<string> redoKeys = {"Ctrl+Shift+Y", "Ctrl+Z"};
		m_Ui.RegisterAction("core", "core.undo", "Rückgängig", "Undo", undoKeys, this);
		m_Ui.RegisterAction("core", "core.redo", "Wiederholen", "Redo", redoKeys, this);
		m_Ui.SetGroup("core.undo", "history");
		m_Ui.SetGroup("core.redo", "history");
		m_Ui.AddMenuEntry("edit", "core.undo", 10);
		m_Ui.AddMenuEntry("edit", "core.redo", 20);
		array<string> saveKeys = {"Ctrl+S"};
		m_Ui.RegisterAction("core", "core.save", "Speichern", "Save", saveKeys, this);
		m_Ui.SetGroup("core.save", "ui");
		m_Ui.SetWhen("core.save", "always");
		m_Ui.AddMenuEntry("project", "core.save", 200);
		m_Ui.RegisterAction("core", "core.settings", "Einstellungen …", "Settings …", null, this);
		m_Ui.SetListed("core.settings", false);
		m_Ui.AddMenuEntry("project", "core.settings", 500);
		m_Ui.RegisterAction("core", "core.toggleLeft", "Katalog-Leiste", "Catalogue bar", null, this);
		m_Ui.RegisterAction("core", "core.toggleRight", "Projekt-Leiste", "Project bar", null, this);
		m_Ui.SetListed("core.toggleLeft", false);
		m_Ui.SetListed("core.toggleRight", false);
		m_Ui.SetWhen("core.toggleLeft", "scene,map");
		m_Ui.SetWhen("core.toggleRight", "scene,map");
		m_Ui.AddMenuEntry("view", "core.toggleLeft", 10, true);
		m_Ui.AddMenuEntry("view", "core.toggleRight", 20, true);
		if (m_Settings.IsDeveloper())
		{
			m_Ui.RegisterAction("core", "core.blocks", "Musterseite der Bausteine …", "Building blocks …", null, this);
			m_Ui.SetListed("core.blocks", false);
			m_Ui.SetWhen("core.blocks", "always");
			m_Ui.AddMenuEntry("view", "core.blocks", 900);
		}
	}

	// The storage provider of the open project answers "core.save"; the kernel only
	// offers the action, the knob and the key.
	// A plugin that takes over clicks in the scene (the brush): placing then selects, drags and highlights
	// nothing and shows no handles; keys on the selection still work.
	void SetSceneTool(string owner)
	{
		if (owner == m_SceneTool)
			return;
		m_SceneTool = owner;
		DZS_Log.Info("scene tool '" + owner + "'");
	}

	string GetSceneTool() { return m_SceneTool; }

	// The owner of the scene tool sets the key state ("selection", "none") itself, e.g. for points.
	protected bool m_StateClaimed;
	void ClaimSceneState(bool on) { m_StateClaimed = on; }
	bool IsSceneStateClaimed() { return m_StateClaimed; }

	// Objects whose frame another plugin draws this frame (the eraser's danger frames); the selection leaves them out.
	void SetFramesTaken(array<string> ids)
	{
		m_FramesTaken.Clear();
		if (!ids)
			return;
		foreach (string id : ids)
			m_FramesTaken.Set(id, true);
	}

	bool IsFrameTaken(string id) { return m_FramesTaken.Contains(id); }

	void SetGroupMaker(DZS_GroupMaker m) { m_GroupMaker = m; }
	DZS_GroupMaker GetGroupMaker() { return m_GroupMaker; }

	void SetSaveHandler(DZS_ActionHandler h)
	{
		m_SaveHandler = h;
	}

	protected void LoadPlugins()
	{
		array<typename> types = DZS_PluginRegistry.Types();
		foreach (typename t : types)
		{
			DZS_Plugin p = DZS_Plugin.Cast(t.Spawn());
			if (!p)
			{
				DZS_Log.Error("plugin class " + t.ToString() + " is not a DZS_Plugin");
				continue;
			}
			string name = p.GetName();
			if (name == "" || name == "core" || name.Contains(":") || name.Contains("."))
			{
				DZS_Log.Error("plugin " + t.ToString() + " has an invalid name '" + name + "'");
				continue;
			}
			if (p.GetRequiredApiVersion() > DZS_API_VERSION)
			{
				DZS_Log.Error("plugin " + name + " needs api " + p.GetRequiredApiVersion() + ", kernel has " + DZS_API_VERSION);
				continue;
			}
			if (m_PluginsByName.Contains(name))
			{
				DZS_Log.Error("plugin name " + name + " registered twice (" + t.ToString() + ")");
				continue;
			}
			p.Bind(this);
			m_Plugins.Insert(p);
			m_PluginsByName.Set(name, p);
			m_Events.Subscribe(p);
			p.OnRegister();
			DZS_Log.Info("plugin " + name + " v" + p.GetVersion() + " (" + t.ToString() + ")");
		}
	}

	void Shutdown()
	{
		DZS_Log.Info("kernel shutdown");
		DZS_Jobs.Clear();
		for (int i = m_Plugins.Count() - 1; i >= 0; i--)
			m_Plugins[i].OnSessionEnd();
		while (m_Instances.Count() > 0)
			CloseInstance(m_Instances[m_Instances.Count() - 1]);
		m_Settings.Flush();
		if (m_Screen)
			m_Screen.Destroy();
		DZS_BarGroups.Clear();
		m_Plugins.Clear();
		m_PluginsByName.Clear();
		if (s_Kernel == this)
			s_Kernel = null;
	}

	// Time per plugin and for the screen (bars, panels) while s_Prof is on; ticks of TickCount (0.1 µs).
	static bool s_Prof;
	// -dzsSlowLog: names every plugin update and screen update that takes 20 ms or more (0 = not asked yet)
	protected int m_Slow;
	static int s_ProfFrames;
	static ref map<string, int> s_ProfTicks = new map<string, int>();

	protected bool ClaimStep(bool redo)
	{
		DZS_SelectGuard g = DZS_SelectGuard.Get();
		if (!g)
			return false;
		array<string> ids = m_Shown.StepObjects(redo);
		return g.ClaimForStep(m_Shown, ids, redo);
	}

	// Time of the last input: every key down or up, click, wheel notch and mouse movement, in ms since the
	// game started (NowMs is the same clock).
	protected int m_LastInputMs;
	protected ref array<bool> m_KeyWas;
	protected int m_InMx = -100000;
	protected int m_InMy;
	protected int m_InButtons = -1;

	int GetLastInputMs() { return m_LastInputMs; }

	static int NowMs()
	{
		float ms = GetGame().GetTickTime() * 1000.0;
		int r = ms;
		return r;
	}

	protected void TrackInput()
	{
		bool any = false;
		if (!m_KeyWas)
		{
			m_KeyWas = new array<bool>();
			for (int k0 = 0; k0 < 256; k0++)
				m_KeyWas.Insert(false);
		}
		for (int k = 1; k < 238; k++)
		{
			KeyCode kc = k;
			bool down = KeyState(kc) != 0;
			if (down != m_KeyWas[k])
			{
				m_KeyWas[k] = down;
				any = true;
			}
		}
		int mx, my;
		GetMousePos(mx, my);
		if (mx != m_InMx || my != m_InMy)
		{
			if (m_InMx != -100000)
				any = true;
			m_InMx = mx;
			m_InMy = my;
		}
		int buttons = GetMouseState(MouseState.LEFT) + 2 * GetMouseState(MouseState.RIGHT) + 4 * GetMouseState(MouseState.MIDDLE);
		if (buttons != m_InButtons)
		{
			if (m_InButtons != -1)
				any = true;
			m_InButtons = buttons;
		}
		if (GetMouseState(MouseState.WHEEL) != 0 || GetMouseState(MouseState.X) != 0 || GetMouseState(MouseState.Y) != 0)
			any = true;
		if (any)
			m_LastInputMs = NowMs();
	}

	protected string m_SelSig = "-";
	protected ref array<string> m_SelCache = new array<string>();

	void Update(float dt)
	{
		TrackInput();
		DZS_SelectionApi sa = DZS_SelectionApi.Get();
		if (sa && DZS_SelectGuard.Get())
		{
			string sig = sa.Signature();
			if (sig != m_SelSig)
			{
				m_SelSig = sig;
				m_SelCache = sa.GetSelection();
			}
			DZS_ChangeWait.Refresh(m_SelCache, m_Shown);
		}
		m_Settings.Update(dt);
		m_Ui.Update(dt);
		DZS_WorldLines.Begin();
		if (m_Slow == 0)
		{
			string slowArg;
			m_Slow = 1;
			if (GetCLIParam("dzsSlowLog", slowArg))
				m_Slow = 2;
		}
		bool slow = m_Slow == 2;
		DZS_Jobs.Update(dt);
		if (!s_Prof && !slow)
		{
			foreach (DZS_Plugin p : m_Plugins)
				p.OnUpdate(dt);
			m_Screen.Update(dt);
		}
		else
		{
			s_ProfFrames++;
			foreach (DZS_Plugin pp : m_Plugins)
			{
				int t0 = TickCount(0);
				pp.OnUpdate(dt);
				int t1 = TickCount(0);
				string pn = pp.GetName();
				if (slow && t1 - t0 >= 200000)
					DZS_Log.Info("slow: plugin " + pn + " " + ((t1 - t0) / 10000) + " ms");
				int had = 0;
				s_ProfTicks.Find(pn, had);
				s_ProfTicks.Set(pn, had + t1 - t0);
			}
			int s0 = TickCount(0);
			m_Screen.Update(dt);
			int s1 = TickCount(0);
			if (slow && s1 - s0 >= 200000)
				DZS_Log.Info("slow: screen " + ((s1 - s0) / 10000) + " ms");
			int hadScreen = 0;
			s_ProfTicks.Find("screen", hadScreen);
			s_ProfTicks.Set("screen", hadScreen + s1 - s0);
		}
		UpdateStatusGroup();
		// shown at the first pointer movement: during the loading screen nobody would see it
		if (m_BrokenNotice)
		{
			int mx, my;
			GetMousePos(mx, my);
			if (m_PointerX < 0)
			{
				m_PointerX = mx;
				m_PointerY = my;
			}
			else if (mx != m_PointerX || my != m_PointerY)
			{
				m_BrokenNotice = false;
				m_Screen.ShowMessage("settings", "warning", DZS_Texts.Pick("Einstellungen nicht lesbar — Standard wird verwendet", "Settings unreadable — using defaults"), 2.0);
			}
		}
	}

	// ---- instances ----

	protected void UpdateStatusGroup()
	{
		string name = "";
		if (m_Shown && !m_Screen.IsHome())
			name = DZS_ActiveGroup.Name(m_Shown);
		if (name == m_StatusGroup)
			return;
		m_StatusGroup = name;
		m_Screen.SetStatusGroup(name);
	}

	protected DZS_Instance NewInstance()
	{
		m_InstanceCount++;
		string label = "I" + m_InstanceCount;
		DZS_Instance inst = new DZS_Instance(this, label);
		if (m_Protocol)
			inst.EnableProtocol();
		return inst;
	}

	DZS_Instance OpenEmpty()
	{
		DZS_Instance inst = NewInstance();
		DZS_Json root = DZS_Json.Obj();
		root.Set("textform", DZS_Json.Int(DZS_TEXTFORM_VERSION));
		DZS_Json st = DZS_Json.Obj();
		DZS_Json kinds = DZS_Json.Obj();
		kinds.Set(DZS_KIND_PROJECT, DZS_Json.Int(1));
		st.Set("kinds", kinds);
		DZS_ElementData proj = new DZS_ElementData();
		proj.m_Id = DZS_PROJECT_ID;
		proj.m_Kind = DZS_KIND_PROJECT;
		DZS_Json elems = DZS_Json.Arr();
		DZS_Json t337 = proj.ToJson();
		elems.Add(t337);
		st.Set("elements", elems);
		root.Set("state", st);
		string error;
		array<string> t341 = root.ToChunks();
		if (!inst.LoadState(t341, error))
		{
			DZS_Log.Error("open empty failed: " + error);
			return null;
		}
		AddInstance(inst);
		return inst;
	}

	// A fresh instance for a load built by a storage provider; AddLoaded shows it.
	DZS_Instance NewLoadInstance()
	{
		return NewInstance();
	}

	void AddLoaded(DZS_Instance inst)
	{
		AddInstance(inst);
	}

	// Opens a project instance from a whole state in text form (keeps its ids).
	DZS_Instance OpenFromState(array<string> chunks, out string error)
	{
		DZS_Instance inst = NewInstance();
		if (!inst.LoadState(chunks, error))
			return null;
		AddInstance(inst);
		return inst;
	}

	protected void AddInstance(DZS_Instance inst)
	{
		m_Instances.Insert(inst);
		DZS_Log.Info("instance " + inst.GetLabel() + " open, session code " + inst.GetSessionCode() + ", " + inst.CountElements() + " element(s)");
		m_Events.FireInstanceOpened(inst);
		if (!m_Shown)
			ShowInstance(inst);
	}

	void CloseInstance(DZS_Instance inst)
	{
		int idx = m_Instances.Find(inst);
		if (idx < 0)
			return;
		m_Events.FireInstanceClosing(inst);
		if (m_Shown == inst)
		{
			m_Shown = null;
			m_Events.FireShownInstanceChanged(null);
		}
		m_Instances.RemoveOrdered(idx);
	}

	void ShowInstance(DZS_Instance inst)
	{
		if (m_Shown == inst)
			return;
		m_Shown = inst;
		m_Events.FireShownInstanceChanged(inst);
	}

	DZS_Instance GetShown() { return m_Shown; }
	int CountInstances() { return m_Instances.Count(); }

	// ---- gestures: undo and redo wait until a running gesture has put its step into the history ----

	void SetGesture(string owner, bool active)
	{
		if (active)
			m_Gestures.Set(owner, true);
		else
			m_Gestures.Remove(owner);
	}

	bool IsGestureRunning() { return m_Gestures.Count() > 0; }

	// While a plugin claims the wheel (dragging: height), the camera speed stays.
	void ClaimWheel(string owner, bool on)
	{
		if (on)
			m_WheelOwners.Set(owner, true);
		else
			m_WheelOwners.Remove(owner);
	}

	bool IsWheelClaimed() { return m_WheelOwners.Count() > 0; }

	// ---- core actions ----

	override bool IsActionChecked(string actionId)
	{
		if (!m_Screen)
			return false;
		if (actionId == "core.toggleLeft")
			return !m_Screen.IsBarCollapsed("left");
		if (actionId == "core.toggleRight")
			return !m_Screen.IsBarCollapsed("right");
		return false;
	}

	override string GetActionTip(string actionId)
	{
		if (!m_Shown)
			return "";
		string de;
		string en;
		if (actionId == "core.undo" && m_Shown.CanUndo())
		{
			de = m_Shown.GetUndoName();
			en = m_Shown.GetUndoName(true);
			return DZS_Texts.Pick("Rückgängig: " + de, "Undo: " + en);
		}
		if (actionId == "core.redo" && m_Shown.CanRedo())
		{
			de = m_Shown.GetRedoName();
			en = m_Shown.GetRedoName(true);
			return DZS_Texts.Pick("Wiederholen: " + de, "Redo: " + en);
		}
		return "";
	}

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (actionId == "core.settings" || actionId == "core.toggleLeft" || actionId == "core.toggleRight" || actionId == "core.blocks")
			return true;
		if (actionId == "core.save")
		{
			DZS_SaveRules rules = DZS_SaveRules.Get();
			if (m_Shown && rules && rules.SaveBlocked(m_Shown, reasonDe, reasonEn))
				return false;
			bool unsaved;
			if (m_Shown && rules && rules.Unsaved(m_Shown, unsaved))
			{
				if (unsaved)
					return true;
				reasonDe = "Keine ungespeicherten Änderungen";
				reasonEn = "No unsaved changes";
				return false;
			}
			if (!m_Shown || !m_Shown.IsUnsaved())
			{
				reasonDe = "Keine ungespeicherten Änderungen";
				reasonEn = "No unsaved changes";
				return false;
			}
			return true;
		}
		if (!m_Shown)
			return false;
		if (actionId == "core.undo")
		{
			if (!m_Shown.CanUndo())
			{
				reasonDe = "Nichts zum Rückgängigmachen";
				reasonEn = "Nothing to undo";
				return false;
			}
			return !IsGestureRunning();
		}
		if (actionId == "core.redo")
		{
			if (!m_Shown.CanRedo())
			{
				reasonDe = "Nichts zum Wiederholen";
				reasonEn = "Nothing to redo";
				return false;
			}
			return !IsGestureRunning();
		}
		return false;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "core.settings")
		{
			m_Screen.OpenSettings();
			return;
		}
		if (actionId == "core.blocks")
		{
			DZS_BlockSheet sheet = new DZS_BlockSheet();
			sheet.Open();
			return;
		}
		if (actionId == "core.toggleLeft")
		{
			m_Screen.SetBarCollapsed("left", !m_Screen.IsBarCollapsed("left"));
			return;
		}
		if (actionId == "core.toggleRight")
		{
			m_Screen.SetBarCollapsed("right", !m_Screen.IsBarCollapsed("right"));
			return;
		}
		if (actionId == "core.save")
		{
			if (m_SaveHandler)
				m_SaveHandler.OnAction(actionId);
			else
				DZS_Log.Info("save: no storage provider yet");
			return;
		}
		if (!m_Shown)
			return;
		if (actionId == "core.undo")
		{
			if (!ClaimStep(false))
				m_Shown.Undo();
		}
		else if (actionId == "core.redo")
		{
			if (!ClaimStep(true))
				m_Shown.Redo();
		}
	}
}

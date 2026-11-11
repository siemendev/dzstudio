// The Spawns layer: vehicle spawn points of the base (vanilla or the mission folder chosen for the project) with the
// project's changes. A point is shown as the real car that spawns there (DZS_SpawnCars), in every layer as scenery and
// editable only in this one; nothing else marks an unselected point. Hover frames a car and names its event, the
// selection gets frame, ring and label. To find points: the right list grouped by event (an event selected marks all
// its points, with arrows at the screen edge to those out of view), Alt held or the switch "Markers" shows every point
// as a dot, a list row under the pointer highlights its car, and the map (M) always shows all points. The left list
// offers the events (a new point for one) and the vehicles (an own fixed event, also from the catalogue of the Objects
// layer); placed by a click in the scene or by dragging, like an object.
class DZS_SpawnsActions : DZS_ActionHandler
{
	DZS_SpawnsPlugin m_P;

	override bool IsActionChecked(string actionId)
	{
		if (actionId == "spawns.layer.spawns")
			return m_P.m_On;
		if (actionId == "spawns.markers")
			return m_P.MarkersSwitch();
		if (actionId == "spawns.cars")
			return m_P.CarsEverywhere();
		return false;
	}

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (!m_P.Kernel().GetShown())
		{
			reasonDe = "Kein Projekt offen";
			reasonEn = "No project open";
			return false;
		}
		return true;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "spawns.layer.spawns")
			m_P.SetOn(true);
		else if (actionId == "spawns.markers")
			m_P.FlipSetting("spawns.markers", m_P.MarkersSwitch());
		else if (actionId == "spawns.cars")
			m_P.FlipSetting("spawns.cars", m_P.CarsEverywhere());
	}
}

// While the layer is on, delete removes the selected point instead of objects.
class DZS_SpawnsOverride : DZS_ActionOverride
{
	DZS_SpawnsPlugin m_P;

	override bool Claims(string actionId)
	{
		return actionId == "placement.delete" || actionId == "placement.duplicate" || actionId == "placement.selectAll";
	}

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (actionId == "placement.delete")
			return m_P.m_Sel != "";
		return false;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "placement.delete")
			m_P.RemoveSelected();
	}
}

class DZS_SpawnsDivert : DZS_PlaceDivert
{
	DZS_SpawnsPlugin m_P;

	override bool Place(DZS_Instance inst, string name, vector pos, vector ypr)
	{
		if (!DZS_SpawnsBase.IsVehicle(name))
			return false;
		m_P.FixedVehicle(inst, name, pos, ypr[0]);
		return true;
	}
}

class DZS_SpawnsEsc : DZS_EscHandler
{
	DZS_SpawnsPlugin m_P;

	override bool IsEscActive()
	{
		return m_P.m_On;
	}

	override void OnEsc()
	{
		if (m_P.m_PlaceClass != "")
			m_P.Arm("", false);
		else if (m_P.m_Sel != "")
			m_P.Select("");
		else if (m_P.m_SelEvent != "")
			m_P.SelectEvent("");
		else
			m_P.SetOn(false);
	}
}

class DZS_SpawnsLayer : DZS_Layer
{
	DZS_SpawnsPlugin m_P;

	override string Id() { return "spawns"; }
	override bool IsOn() { return m_P.m_On; }
	override void TurnOff() { m_P.SetOn(false); }
	override string CatalogTitle() { return DZS_Texts.Pick("Events und Fahrzeuge", "Events and vehicles"); }
	override bool Offers(string cls) { return DZS_SpawnsBase.IsVehicle(cls); }
	override int ExtraRev() { return m_P.m_Events.Count() * 100000 + m_P.PointsShown(); }

	// Each event of base and project: a new spawn point for it.
	override void Extra(array<ref DZS_LayerEntry> into)
	{
		array<string> names = new array<string>();
		foreach (string n, DZS_SpawnEvent e : m_P.m_Events)
			names.Insert(n);
		names.Sort();
		foreach (string name : names)
		{
			DZS_LayerEntry le = new DZS_LayerEntry();
			le.m_Key = "ev:" + name;
			le.m_Name = name;
			string pts = DZS_SpawnsPlugin.PointsText(m_P.PointsOf(name));
			string sub = DZS_Texts.Pick("Neuer Spawnpunkt · ", "New spawn point · ");
			le.m_Sub = sub + pts;
			le.m_Icon = "car";
			into.Insert(le);
		}
	}
	override void Arm(string key, bool drag) { m_P.Arm(key, drag); }
	override string Armed() { return m_P.m_PlaceClass; }
}

// The right list: one header row per event ("g:<event>", sorted by name, with its count), the points of an open event
// below it. A header click selects the event (and opens it, or closes it when it was selected already).
class DZS_SpawnsList : DZS_LayerListSource
{
	DZS_SpawnsPlugin m_P;
	protected ref array<DZS_SpawnPoint> m_Rows = new array<DZS_SpawnPoint>();
	protected ref array<string> m_Heads = new array<string>();
	protected ref map<string, int> m_Counts = new map<string, int>();
	protected int m_RowsGen = -1;

	protected void Sync()
	{
		if (m_RowsGen == m_P.m_Gen)
			return;
		m_RowsGen = m_P.m_Gen;
		m_Rows.Clear();
		m_Heads.Clear();
		m_Counts.Clear();
		map<string, ref array<DZS_SpawnPoint>> by = new map<string, ref array<DZS_SpawnPoint>>();
		array<string> names = new array<string>();
		foreach (DZS_SpawnPoint p : m_P.m_Points)
		{
			if (p.m_Gone)
				continue;
			array<DZS_SpawnPoint> list = by.Get(p.m_Event);
			if (!list)
			{
				list = new array<DZS_SpawnPoint>();
				by.Set(p.m_Event, list);
				names.Insert(p.m_Event);
			}
			list.Insert(p);
		}
		names.Sort();
		foreach (string ev : names)
		{
			array<DZS_SpawnPoint> pts = by.Get(ev);
			m_Rows.Insert(null);
			m_Heads.Insert(ev);
			m_Counts.Set(ev, pts.Count());
			if (!m_P.IsOpen(ev))
				continue;
			foreach (DZS_SpawnPoint q : pts)
			{
				m_Rows.Insert(q);
				m_Heads.Insert(ev);
			}
		}
	}

	override int Gen() { return m_P.m_Gen; }
	override int RowCount() { Sync(); return m_Rows.Count(); }
	override bool RowHeader(int i) { return m_Rows[i] == null; }
	override bool RowOpen(int i) { return m_P.IsOpen(m_Heads[i]); }

	override string RowKey(int i)
	{
		if (!m_Rows[i])
			return "g:" + m_Heads[i];
		return m_Rows[i].Key();
	}

	override string RowTitle(int i)
	{
		if (!m_Rows[i])
			return m_Heads[i];
		return "x " + Math.Round(m_Rows[i].m_X) + " · z " + Math.Round(m_Rows[i].m_Z);
	}

	override string RowSub(int i)
	{
		DZS_SpawnPoint p = m_Rows[i];
		if (!p)
			return m_Counts.Get(m_Heads[i]).ToString();
		string s = Math.Round(p.m_A).ToString() + "°";
		if (p.m_Kept)
			s += DZS_Texts.Pick(" · außerhalb angelegt", " · made outside");
		else if (p.m_Own)
			s += DZS_Texts.Pick(" · eigener Punkt", " · own point");
		else if (p.m_Moved)
			s += DZS_Texts.Pick(" · verschoben", " · moved");
		int others = m_P.StackSize(p) - 1;
		if (others > 0)
		{
			string here = m_P.HereText(others + 1);
			s += " · " + here;
		}
		return s;
	}

	override int RowColor(int i)
	{
		if (!m_Rows[i])
			return DZS_Color.Get("textMuted");
		return m_P.ColorOf(m_Rows[i]);
	}

	override string SelectedKey()
	{
		if (m_P.m_Sel != "")
			return m_P.m_Sel;
		if (m_P.m_SelEvent != "")
			return "g:" + m_P.m_SelEvent;
		return "";
	}

	override void SelectRow(string key)
	{
		if (key.IndexOf("g:") == 0)
		{
			string ev = key.Substring(2, key.Length() - 2);
			if (m_P.m_SelEvent == ev)
				m_P.SetOpen(ev, !m_P.IsOpen(ev));
			else
				m_P.SetOpen(ev, true);
			m_P.SelectEvent(ev);
			return;
		}
		m_P.Select(key);
	}

	override void FocusRow(string key)
	{
		if (key.IndexOf("g:") == 0)
		{
			DZS_SpawnPoint n = m_P.NearestOf(key.Substring(2, key.Length() - 2));
			if (n)
				DZS_LayerRay.Focus(n.m_X, n.m_Z, 8);
			return;
		}
		DZS_SpawnPoint p = m_P.PointOf(key);
		if (p)
			DZS_LayerRay.Focus(p.m_X, p.m_Z, 8);
	}

	override void HoverRow(string key)
	{
		if (key.IndexOf("g:") == 0)
			key = "";
		m_P.m_ListHover = key;
	}

	override string EmptyText()
	{
		if (m_P.Loading())
			return DZS_Texts.Pick("Spawnpunkte werden gelesen …", "Reading spawn points …");
		if (!m_P.HasBase())
			return DZS_Texts.Pick("Keine Basis geladen", "No base loaded");
		return DZS_Texts.Pick("Keine Spawnpunkte", "No spawn points");
	}
}

// Double click on a spawn point of the map: into the layer, the point selected, the camera there.
class DZS_SpawnsMapOwner : DZS_MapMarkOwner
{
	DZS_SpawnsPlugin m_P;

	override void OnMapMarkAction(string markId)
	{
		m_P.GoTo(markId, true);
	}
}

[DZS_RegisterPlugin(DZS_SpawnsPlugin)]
class DZS_SpawnsPlugin : DZS_Plugin
{
	static const string TOOL = "spawns";
	static const string PANEL = "spawns.event";
	static const string LIST = "spawns.list";
	static const float LOAD_MS = 4;
	static const float PLAN_EVERY = 0.5;
	static const int ARROWS = 5;

	bool m_On;
	string m_Sel;
	// an event chosen in the list: all its points marked
	string m_SelEvent;
	string m_Hover;
	// the point row under the pointer in the right list
	string m_ListHover;
	// from the left list: "ev:<event>" (a new point of the event) or a vehicle class (a fixed vehicle); placed by the
	// next click, or where a drag from the list is let go
	string m_PlaceClass;
	protected bool m_PlaceDrag;
	int m_Gen;
	// bumped when the points change (stacks, cars and map marks follow)
	protected int m_PtsGen;
	protected bool m_World;
	protected ref DZS_SpawnsActions m_Actions;
	protected ref DZS_SpawnsOverride m_Override;
	protected ref DZS_SpawnsDivert m_Divert;
	protected ref DZS_SpawnsEsc m_Esc;
	protected ref DZS_SpawnsLayer m_Layer;
	protected ref DZS_SpawnsPanel m_Panel;
	protected ref DZS_SpawnsList m_ListSrc;
	protected ref DZS_SpawnsMapOwner m_MapOwner;
	protected ref DZS_SpawnsBase m_Base;
	protected string m_BaseFor;
	protected ref DZS_SpawnsLoad m_Load;
	ref map<string, ref DZS_SpawnEvent> m_Events = new map<string, ref DZS_SpawnEvent>();
	ref array<ref DZS_SpawnPoint> m_Points = new array<ref DZS_SpawnPoint>();
	protected int m_ViewVer = -1;
	protected int m_StateVer;
	protected CanvasWidget m_Canvas;
	protected ref DZS_PointerHint m_HoverHint;
	protected ref DZS_PointerHint m_SelPlate;
	protected ref DZS_SpawnCars m_Cars = new DZS_SpawnCars();
	// points on the same spot (rounded to the metre): the first one's car stands for all
	protected ref map<string, ref array<DZS_SpawnPoint>> m_Stacks = new map<string, ref array<DZS_SpawnPoint>>();
	protected ref array<string> m_StackOrder = new array<string>();
	protected ref map<string, string> m_StackOf = new map<string, string>();
	protected ref map<string, DZS_SpawnPoint> m_ByKey = new map<string, DZS_SpawnPoint>();
	protected ref map<string, int> m_Variant = new map<string, int>();
	protected int m_StacksGen = -1;
	protected ref map<string, bool> m_IsCar = new map<string, bool>();
	protected ref map<string, bool> m_Open = new map<string, bool>();
	protected float m_PlanWait;
	protected int m_PlanGen = -1;
	protected bool m_PlanOn;
	protected int m_MarksGen = -1;
	protected string m_MarksSel;
	protected string m_MarksEvent;
	protected bool m_LmbWas;
	protected bool m_Drag;
	protected float m_PressX;
	protected float m_PressY;
	protected bool m_Press;
	// a click on the selected car of a stack steps to the stack's next point when it ends without a drag
	protected bool m_Cycle;
	protected float m_ClickAt = -10;
	protected string m_ClickStack;
	// the points of m_Sel, m_Hover and m_ListHover while drawing
	protected DZS_SpawnPoint m_SelP;
	protected DZS_SpawnPoint m_HoverP;
	protected DZS_SpawnPoint m_ListP;

	override string GetName() { return "spawns"; }

	DZS_Kernel Kernel() { return m_Kernel; }

	override void OnRegister()
	{
		DZS_TypeRegistry types = m_Kernel.GetTypes();
		types.RegisterOperationType("spawns", DZS_SpawnsModel.OP, 1);
		array<string> parents = {DZS_KIND_PROJECT};
		types.RegisterElementKind("spawns", DZS_SpawnsModel.EVENT, 1, parents);
		types.RegisterElementKind("spawns", DZS_SpawnsModel.POINT, 1, parents);
		types.RegisterElementKind("spawns", DZS_SpawnsModel.SET, 1, parents);
		DZS_SpawnsSection section = new DZS_SpawnsSection();
		DZS_ProjectSections.Add(section);
		DZS_Settings st = m_Kernel.GetSettings();
		st.Define("spawns.markers", DZS_Json.Boolean(false));
		st.Define("spawns.cars", DZS_Json.Boolean(true));
		m_Actions = new DZS_SpawnsActions();
		m_Actions.m_P = this;
		m_Override = new DZS_SpawnsOverride();
		m_Override.m_P = this;
		m_Divert = new DZS_SpawnsDivert();
		m_Divert.m_P = this;
		DZS_PlaceDivert.Add(m_Divert);
		m_Layer = new DZS_SpawnsLayer();
		m_Layer.m_P = this;
		DZS_Layers.Add(m_Layer);
		m_MapOwner = new DZS_SpawnsMapOwner();
		m_MapOwner.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		DZS_LayersPlugin.Register(ui, "spawns", "spawns.layer.spawns", "spawns", "Spawns", "Spawns", "car", "2", 20, m_Actions);
		ui.RegisterAction("spawns", "spawns.tool.select", "Auswählen und verschieben", "Select and move", null, m_Actions);
		ui.SetListed("spawns.tool.select", false);
		ui.RegisterTool("spawns", "spawns.tool.select", "Auswählen und verschieben", "Select and move", "tools", "mouse-pointer-2", 10);
		ui.SetToolLayer("spawns.tool.select", "spawns");
		ui.SetToolTip("spawns.tool.select", "Auswählen und verschieben — Auto anklicken, ziehen", "Select and move — click a car, drag it", "");
		ui.SetActionLayer("spawns.tool.select", "spawns");
		ui.RegisterAction("spawns", "spawns.markers", "Markierungen", "Markers", null, m_Actions);
		ui.RegisterTool("spawns", "spawns.markers", "Markierungen", "Markers", "switches", "eye", 20);
		ui.SetToolLabelled("spawns.markers");
		ui.SetToolLayer("spawns.markers", "spawns");
		ui.SetToolTip("spawns.markers", "Markierungen — jeden Spawnpunkt als Punkt zeigen (Alt halten: nur solange gedrückt)", "Markers — show every spawn point as a dot (hold Alt: only while held)", "");
		ui.SetActionLayer("spawns.markers", "spawns");
		ui.RegisterAction("spawns", "spawns.cars", "Spawn-Autos in allen Ebenen", "Spawn cars in every layer", null, m_Actions);
		ui.AddMenuEntry("view", "spawns.cars", 340, true);
		m_ListSrc = new DZS_SpawnsList();
		m_ListSrc.m_P = this;
		ui.RegisterPanel("spawns", LIST, "Spawnpunkte", "Spawn points", "right", "car", new DZS_LayerList(m_ListSrc), 14);
		ui.SetPanelShown(LIST, false);
		m_Panel = new DZS_SpawnsPanel(this);
		ui.RegisterPanel("spawns", PANEL, "Event", "Event", "right", "car", m_Panel, 15);
		ui.SetPanelShown(PANEL, false);
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		if (!m_World)
			return;
		Widget layer = m_Kernel.GetScreen().GetSceneLayer();
		m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", layer));
		m_HoverHint = new DZS_PointerHint(layer);
		m_SelPlate = new DZS_PointerHint(layer);
		m_Esc = new DZS_SpawnsEsc();
		m_Esc.m_P = this;
		m_Kernel.GetScreen().AddEscHandler(m_Esc, 87);
	}

	override void OnSessionEnd()
	{
		SetOn(false);
		Reset();
		m_Cars.Clear();
		if (m_Canvas)
			m_Canvas.Unlink();
		m_Canvas = null;
		m_HoverHint = null;
		m_SelPlate = null;
		m_World = false;
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		SetOn(false);
		Reset();
		m_Base = null;
		m_BaseFor = "";
		m_Load = null;
		m_ViewVer = -1;
		m_Points.Clear();
		m_Events.Clear();
		m_Gen++;
		m_PtsGen++;
		if (inst)
			RestoreFromProject(inst);
	}

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		m_StateVer++;
	}

	// Cars parked, marks and hints gone (no project shown, another project, the session ends).
	protected void Reset()
	{
		m_Cars.ParkAll();
		DZS_Scenery.s_PointerOn = false;
		DZS_MapMarks.Clear("spawns");
		m_MarksGen = -1;
		m_PlanGen = -1;
		m_StacksGen = -1;
		m_Hover = "";
		m_ListHover = "";
		m_SelEvent = "";
		if (m_HoverHint)
			m_HoverHint.Hide();
		if (m_SelPlate)
			m_SelPlate.Hide();
		if (m_Canvas)
			m_Canvas.Clear();
	}

	// ---- opening ----

	// The layer's elements from project.plugins.spawns when the project has none yet (a project just opened).
	protected void RestoreFromProject(DZS_Instance inst)
	{
		if (DZS_SpawnsModel.Of(inst, DZS_SpawnsModel.EVENT).Count() > 0 || DZS_SpawnsModel.Of(inst, DZS_SpawnsModel.POINT).Count() > 0 || DZS_SpawnsModel.Of(inst, DZS_SpawnsModel.SET).Count() > 0)
			return;
		int v;
		string entries;
		if (!inst.GetCompanion(DZS_PROJECT_ID, DZS_LocalStore.GROUP_PLUGINS, v, entries))
			return;
		string text = DZS_ProjectSections.Stored(entries, DZS_SpawnsModel.PLUGIN);
		if (text == "")
			return;
		string err;
		DZS_Json frag = DZS_Json.ParseText(text, err);
		if (!frag)
		{
			DZS_Log.Warn("spawns: project.plugins.spawns unreadable: " + err);
			return;
		}
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		DZS_SpawnsModel.Restore(b, frag);
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		DZS_Log.Info("spawns: restored from the project file");
	}

	// ---- base and view ----

	// Outside the layer the base is only used when it is here already (no vanilla download for the cars alone).
	protected string BaseDir()
	{
		DZS_PatchPlugin pp = DZS_PatchPlugin.Cast(m_Kernel.GetPlugin("patcher"));
		if (!pp)
			return "";
		return pp.Applier().LayerBase(m_On);
	}

	void View(DZS_Instance inst)
	{
		string dir = BaseDir();
		bool loadingThis = m_Load && m_Load.m_Dir == dir;
		if (!loadingThis && (!m_Base || dir != m_BaseFor))
		{
			m_Load = new DZS_SpawnsLoad(dir);
			loadingThis = true;
		}
		if (loadingThis)
		{
			DZS_SpawnsBase b = m_Load.Step(LOAD_MS);
			if (!b)
				return;
			m_Base = b;
			m_BaseFor = dir;
			m_ViewVer = -1;
			DZS_Log.Info("spawns: base " + dir + ", " + b.m_Events.Count() + " vehicle events, " + b.m_Points.Count() + " points, read in " + m_Load.m_Frames + " frames, slowest " + m_Load.m_Slowest + " ms");
			m_Load = null;
		}
		if (m_ViewVer == m_StateVer)
			return;
		m_ViewVer = m_StateVer;
		m_Base.Effective(inst, m_Events, m_Points);
		m_Gen++;
		m_PtsGen++;
		if (m_Sel != "" && !PointOf(m_Sel))
			m_Sel = "";
		if (m_SelEvent != "" && !m_Events.Contains(m_SelEvent))
			m_SelEvent = "";
	}

	bool HasBase()
	{
		return m_Base && m_BaseFor != "";
	}

	bool Loading()
	{
		return m_Load != null;
	}

	DZS_SpawnPoint PointOf(string key)
	{
		if (key == "")
			return null;
		Stacks();
		return m_ByKey.Get(key);
	}

	// ---- stacks: points on the same spot share one car ----

	protected void Stacks()
	{
		if (m_StacksGen == m_PtsGen)
			return;
		m_StacksGen = m_PtsGen;
		m_Stacks.Clear();
		m_StackOrder.Clear();
		m_StackOf.Clear();
		m_ByKey.Clear();
		m_Variant.Clear();
		map<string, int> perEvent = new map<string, int>();
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.m_Gone)
				continue;
			string sk = Math.Round(p.m_X).ToString() + ":" + Math.Round(p.m_Z).ToString();
			array<DZS_SpawnPoint> st = m_Stacks.Get(sk);
			if (!st)
			{
				st = new array<DZS_SpawnPoint>();
				m_Stacks.Set(sk, st);
				m_StackOrder.Insert(sk);
			}
			st.Insert(p);
			string key = p.Key();
			m_StackOf.Set(key, sk);
			m_ByKey.Set(key, p);
			int n = perEvent.Get(p.m_Event);
			m_Variant.Set(key, n);
			perEvent.Set(p.m_Event, n + 1);
		}
	}

	string StackOf(DZS_SpawnPoint p)
	{
		Stacks();
		return m_StackOf.Get(p.Key());
	}

	int StackSize(DZS_SpawnPoint p)
	{
		Stacks();
		array<DZS_SpawnPoint> st = m_Stacks.Get(m_StackOf.Get(p.Key()));
		if (!st)
			return 1;
		return st.Count();
	}

	string HereText(int n)
	{
		string word = DZS_Texts.Pick(" Events hier", " events here");
		string r = n.ToString() + word;
		return r;
	}

	static string PointsText(int pts)
	{
		string r;
		if (pts == 1)
			r = DZS_Texts.Pick("1 Punkt", "1 point");
		else
		{
			string word = DZS_Texts.Pick(" Punkte", " points");
			r = pts.ToString() + word;
		}
		return r;
	}

	protected bool IsCar(string cls)
	{
		if (m_IsCar.Contains(cls))
			return m_IsCar.Get(cls);
		bool car = DZS_SpawnsBase.IsVehicle(cls) && GetGame().ConfigIsExisting("CfgVehicles " + cls);
		m_IsCar.Set(cls, car);
		return car;
	}

	// The class a point's car shows: the event's classes taken in turn over its points (colour variants).
	protected string ClassOf(DZS_SpawnPoint p)
	{
		DZS_SpawnEvent e = m_Events.Get(p.m_Event);
		if (!e)
			return "";
		array<string> cls = new array<string>();
		foreach (string t : e.m_Types)
		{
			if (IsCar(t) && !m_Cars.Failed(t))
				cls.Insert(t);
		}
		if (cls.Count() == 0)
			return "";
		return cls[m_Variant.Get(p.Key()) % cls.Count()];
	}

	// The built car standing for a point, or null.
	DZS_SpawnCar CarOf(DZS_SpawnPoint p)
	{
		if (!p)
			return null;
		return m_Cars.Of(StackOf(p));
	}

	protected DZS_SpawnPoint FirstOf(string stack)
	{
		array<DZS_SpawnPoint> st = m_Stacks.Get(stack);
		if (!st || st.Count() == 0)
			return null;
		return st[0];
	}

	// ---- cars ----

	bool MarkersSwitch()
	{
		return m_Kernel.GetSettings().GetBool("spawns.markers", false);
	}

	bool CarsEverywhere()
	{
		return m_Kernel.GetSettings().GetBool("spawns.cars", true);
	}

	void FlipSetting(string key, bool was)
	{
		m_Kernel.GetSettings().Set(key, DZS_Json.Boolean(!was));
		m_PlanGen = -1;
		DZS_Log.Info("spawns: " + key + " " + DZS_Str.Bool(!was));
	}

	protected bool ShowCars()
	{
		return m_On || CarsEverywhere();
	}

	// Which cars stand now: per stack within 1000 m (a car already standing stays up to 1100 m), the nearest 60.
	protected void Plan()
	{
		Stacks();
		m_PlanGen = m_PtsGen;
		m_PlanOn = ShowCars();
		array<ref DZS_SpawnCar> want = new array<ref DZS_SpawnCar>();
		if (!m_PlanOn)
		{
			m_Cars.Plan(want);
			return;
		}
		vector cam = GetGame().GetCurrentCameraPosition();
		array<string> order = new array<string>();
		map<string, vector> at = new map<string, vector>();
		foreach (string sk : m_StackOrder)
		{
			DZS_SpawnPoint p = FirstOf(sk);
			vector w = Ground(p.m_X, p.m_Z);
			float d = vector.Distance(w, cam);
			if (d > DZS_SpawnCars.RANGE_OUT || (d > DZS_SpawnCars.RANGE_IN && !m_Cars.Of(sk)))
				continue;
			int dm = d * 10;
			string ds = dm.ToString();
			while (ds.Length() < 6)
				ds = "0" + ds;
			order.Insert(ds + "|" + sk);
			at.Set(sk, w);
		}
		order.Sort();
		foreach (string o : order)
		{
			if (want.Count() >= DZS_SpawnCars.MAX)
				break;
			string stack = o.Substring(7, o.Length() - 7);
			DZS_SpawnPoint first = FirstOf(stack);
			string cls = ClassOf(first);
			if (cls == "")
				continue;
			DZS_SpawnCar c = new DZS_SpawnCar();
			c.m_Stack = stack;
			c.m_Cls = cls;
			c.m_Pos = at.Get(stack);
			c.m_A = first.m_A;
			c.m_Key = stack + "|" + cls + "|" + first.m_X + ":" + first.m_Z + ":" + first.m_A;
			want.Insert(c);
		}
		m_Cars.Plan(want);
	}

	protected void Cars(float dt)
	{
		m_PlanWait -= dt;
		if (m_PlanWait <= 0 || m_PlanGen != m_PtsGen || m_PlanOn != ShowCars())
		{
			m_PlanWait = PLAN_EVERY;
			Plan();
		}
		m_Cars.Step(m_Base);
	}

	// ---- on and off ----

	void SetOn(bool on)
	{
		if (on == m_On)
			return;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		if (on)
		{
			DZS_Layers.Only("spawns");
			DZS_LootPlugin lp = DZS_LootPlugin.Cast(m_Kernel.GetPlugin("loot"));
			if (lp && lp.m_Editor && lp.m_Editor.IsOn())
				lp.m_Editor.Stop("");
			DZS_BrushPlugin brush = DZS_BrushPlugin.Cast(m_Kernel.GetPlugin("brush"));
			if (brush && brush.m_On)
				brush.SetOn(false);
			DZS_CatalogApi cat = DZS_CatalogApi.Get();
			if (cat)
				cat.DropHand();
			DZS_SelectionApi sel = DZS_SelectionApi.Get();
			if (sel)
			{
				array<string> none = new array<string>();
				sel.SetSelection(none);
			}
			m_On = true;
			m_Sel = "";
			m_SelEvent = "";
			m_PlaceClass = "";
			m_Kernel.SetSceneTool(TOOL);
			m_Kernel.ClaimSceneState(true);
			ui.SetOverride(m_Override);
			ui.SetPanelShown(LIST, true);
			ui.SetPanelShown(PANEL, true);
			ui.SetPanelShown("properties.panel", false);
			DZS_Instance inst = m_Kernel.GetShown();
			if (inst)
				View(inst);
			if (!HasBase() && !Loading())
				m_Kernel.GetScreen().ShowMessage("spawns.base", "warning", DZS_Texts.Pick("Keine Basis geladen – im Menü Projekt „Auf Missionsordner anwenden …“ eine Basis wählen oder Vanilla laden", "No base loaded – choose a base or fetch vanilla under Project › Apply to mission folder …"), 8.0);
			DZS_Log.Info("spawns: layer on");
		}
		else
		{
			m_On = false;
			m_Drag = false;
			m_PlaceClass = "";
			m_Sel = "";
			m_SelEvent = "";
			m_Kernel.SetSceneTool("");
			m_Kernel.ClaimSceneState(false);
			ui.SetSceneState("none");
			ui.SetOverride(null);
			ui.SetPanelShown(LIST, false);
			ui.SetPanelShown(PANEL, false);
			ui.SetPanelShown("properties.panel", true);
			m_Kernel.GetScreen().SetHint("spawns", 30, "");
			if (m_SelPlate)
				m_SelPlate.Hide();
			if (m_Canvas)
				m_Canvas.Clear();
			DZS_Log.Info("spawns: layer off");
		}
		m_Gen++;
		DZS_Layers.Sync();
	}

	void Arm(string cls, bool drag)
	{
		m_PlaceClass = cls;
		m_PlaceDrag = drag && cls != "";
		m_Gen++;
		DZS_Log.Info("spawns: armed '" + cls + "' drag " + DZS_Str.Bool(drag));
	}

	int ColorOf(DZS_SpawnPoint p)
	{
		if (p.m_Kept)
			return DZS_Color.Get("info");
		if (p.m_Own)
			return DZS_Color.Get("success");
		if (p.m_Moved)
			return DZS_Color.Get("warning");
		return DZS_Color.Get("accent");
	}

	void Select(string key)
	{
		m_Sel = key;
		if (key != "")
		{
			m_SelEvent = "";
			DZS_SpawnPoint p = PointOf(key);
			if (p)
				m_Open.Set(p.m_Event, true);
		}
		m_Gen++;
	}

	void SelectEvent(string ev)
	{
		m_SelEvent = ev;
		if (ev != "")
			m_Sel = "";
		m_Gen++;
	}

	bool IsOpen(string ev)
	{
		return m_Open.Get(ev);
	}

	void SetOpen(string ev, bool open)
	{
		m_Open.Set(ev, open);
		m_Gen++;
	}

	DZS_SpawnPoint Selected()
	{
		if (m_Sel == "")
			return null;
		return PointOf(m_Sel);
	}

	DZS_SpawnPoint NearestOf(string ev)
	{
		vector cam = GetGame().GetCurrentCameraPosition();
		DZS_SpawnPoint best = null;
		float bestD = 1e12;
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.m_Gone || p.m_Event != ev)
				continue;
			float d = vector.DistanceSq(Vector(p.m_X, cam[1], p.m_Z), cam);
			if (d < bestD)
			{
				bestD = d;
				best = p;
			}
		}
		return best;
	}

	// Into the layer with the point selected (a double click on its car in another layer, or on the map).
	void GoTo(string key, bool fly)
	{
		if (!m_On)
			SetOn(true);
		DZS_SpawnPoint p = PointOf(key);
		if (!p)
			return;
		Select(key);
		if (fly)
			DZS_LayerRay.Focus(p.m_X, p.m_Z, 8);
		DZS_Log.Info("spawns: go to " + key);
	}

	// ---- every frame ----

	override void OnUpdate(float dt)
	{
		DZS_Scenery.s_PointerOn = false;
		if (!m_World)
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_Screen scr = m_Kernel.GetScreen();
		if (!inst || scr.IsHome())
		{
			if (m_On)
				SetOn(false);
			if (m_Cars.Count() > 0 || m_MarksGen >= 0)
				Reset();
			return;
		}
		View(inst);
		if (!m_Base)
			return;
		Stacks();
		Cars(dt);
		MapMarks();
		if (!m_On)
		{
			OtherLayer(scr);
			return;
		}
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat && cat.GetHand() != "")
			cat.DropHand();
		if (m_Sel != "")
			m_Kernel.GetUi().SetSceneState("selection");
		else
			m_Kernel.GetUi().SetSceneState("none");
		LayerInput(inst, scr);
		DZS_PanelInfo li = m_Kernel.GetUi().GetPanel(LIST);
		if (li)
			li.m_Meta = PointsShown().ToString();
		Draw(scr);
		string hint;
		if (m_PlaceClass.IndexOf("ev:") == 0)
			hint = DZS_Texts.Pick("Klick in die Szene setzt einen Spawnpunkt · Esc bricht ab", "Click in the scene to add a spawn point · Esc cancels");
		else if (m_PlaceClass != "")
			hint = DZS_Texts.Pick("Klick in die Szene setzt ein festes Fahrzeug · Esc bricht ab", "Click in the scene to add a fixed vehicle · Esc cancels");
		else if (Selected() && Selected().m_Kept)
			hint = DZS_KeptBlocks.ReadOnlyTitle() + " · " + DZS_KeptBlocks.ReadOnlyHint();
		else if (m_Sel == "")
			hint = DZS_Texts.Pick("Auto anklicken wählt seinen Spawnpunkt · Alt halten: alle Spawns zeigen · links ein Event oder Fahrzeug wählen und in die Szene setzen", "Click a car to select its spawn point · hold Alt: show all spawns · pick an event or vehicle on the left and place it in the scene");
		else
			hint = DZS_Texts.Pick("Ziehen verschiebt · Entf entfernt · Esc hebt die Auswahl auf · Alt halten: alle Spawns zeigen", "Drag to move · Del removes · Esc clears the selection · hold Alt: show all spawns");
		scr.SetHint("spawns", 30, hint);
	}

	protected float CameraYaw()
	{
		vector d = GetGame().GetCurrentCameraDirection();
		return Math.Atan2(d[0], d[2]) * Math.RAD2DEG;
	}

	int PointsShown()
	{
		int n = 0;
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (!p.m_Gone)
				n++;
		}
		return n;
	}

	protected vector Ground(float x, float z)
	{
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	protected vector PointerDir(DZS_Screen scr)
	{
		if (!scr.IsCursorVisible())
			return GetGame().GetCurrentCameraDirection();
		return GetGame().GetPointerDirection();
	}

	protected bool Blocked(DZS_Screen scr)
	{
		return scr.HasDialog() || scr.HasKeyboardOwner("map") || scr.HasKeyboardOwner("menu") || scr.HasKeyboardOwner("fullsearch");
	}

	// The stack whose car is the front thing under the pointer (nothing of the world nearer on the ray), or "".
	protected string CarUnderPointer(DZS_Screen scr)
	{
		if (m_Cars.Count() == 0)
			return "";
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = PointerDir(scr);
		float t;
		string sk = m_Cars.Under(from, dir, t);
		if (sk == "")
			return "";
		array<Object> none = new array<Object>();
		array<ref DZS_RayHit> hits = DZS_Rays.Ray(from, dir, t + 0.5, none);
		if (hits.Count() > 0 && hits[0].m_Dist < t - 0.3)
			return "";
		return sk;
	}

	// Markers on: the switch, or Alt held while no field or dialog has the keyboard.
	protected bool MarkersOn(DZS_Screen scr)
	{
		if (MarkersSwitch())
			return true;
		return DZS_UiRegistry.ModDown("Alt") && !Blocked(scr) && !scr.HasKeyboardOwner("field");
	}

	// Whether a point may get a dot (cheap, before its place is looked at): markers on, a point of the selected event,
	// or selected or under the pointer.
	protected bool DotCandidate(DZS_SpawnPoint p, bool all)
	{
		if (p.m_Gone)
			return false;
		if (all)
			return true;
		if (m_SelEvent != "" && p.m_Event == m_SelEvent)
			return true;
		return p == m_SelP || p == m_HoverP || p == m_ListP;
	}

	// Whether it gets one: every point near enough with markers on; the selected event's points at any distance;
	// selection and hover when no car stands for them.
	protected bool DotShown(DZS_SpawnPoint p, bool all, float dist)
	{
		if (all && dist <= 2500)
			return true;
		if (m_SelEvent != "" && p.m_Event == m_SelEvent)
			return true;
		if (p == m_SelP || p == m_HoverP || p == m_ListP)
			return !CarOf(p);
		return false;
	}

	protected void Specials()
	{
		m_SelP = PointOf(m_Sel);
		m_HoverP = PointOf(m_Hover);
		m_ListP = PointOf(m_ListHover);
	}

	// The nearest shown dot within 14 px of the mouse.
	protected string PickDot(float mx, float my, bool all)
	{
		string best = "";
		float bestD = DZS_Scale.Px(14);
		vector cam = GetGame().GetCurrentCameraPosition();
		Specials();
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (!DotCandidate(p, all))
				continue;
			vector w = Ground(p.m_X, p.m_Z) + "0 0.8 0";
			if (!DotShown(p, all, vector.Distance(w, cam)))
				continue;
			vector s = DZS_PlaceMath.Project(w);
			if (s[2] <= 0.5)
				continue;
			float d = Math.Sqrt((s[0] - mx) * (s[0] - mx) + (s[1] - my) * (s[1] - my));
			if (d < bestD)
			{
				bestD = d;
				best = p.Key();
			}
		}
		return best;
	}

	// Another layer: a car under the pointer names its point and that it is edited in Spawns; a double click goes there.
	protected void OtherLayer(DZS_Screen scr)
	{
		m_Hover = "";
		bool lmb = DZS_UiRegistry.KeyDown("MouseLeft");
		bool lDown = lmb && !m_LmbWas;
		m_LmbWas = lmb;
		string sk = "";
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		DZS_Layer cur = DZS_Layers.Current();
		bool armed = cur && cur.Armed() != "";
		bool hand = cat && cat.GetHand() != "";
		if (m_PlanOn && !Blocked(scr) && scr.IsCursorVisible() && scr.IsPointerOverScene() && !hand && !armed && (!lmb || lDown))
			sk = CarUnderPointer(scr);
		DZS_SpawnPoint p = null;
		if (sk != "")
			p = FirstOf(sk);
		ClearCanvas();
		if (!p || scr.HasKeyboardOwner("map"))
		{
			m_HoverHint.Hide();
			return;
		}
		m_Hover = p.Key();
		DZS_Scenery.s_PointerOn = true;
		if (lDown && !scr.IsMouseSwallowed())
		{
			float now = GetGame().GetTickTime();
			if (now - m_ClickAt < 0.4 && m_ClickStack == sk)
			{
				m_ClickAt = -10;
				GoTo(m_Hover, false);
				return;
			}
			m_ClickAt = now;
			m_ClickStack = sk;
		}
		DrawCarFrame("sp:h:" + sk, m_Cars.Of(sk), false);
		if (m_Canvas)
			DZS_WorldLines.Flush(m_Canvas);
		int mx, my;
		GetMousePos(mx, my);
		array<string> lines = new array<string>();
		lines.Insert(DZS_Texts.Pick("Spawnpunkt – in der Ebene Spawns bearbeiten (Doppelklick)", "Spawn point – edit it in the Spawns layer (double click)"));
		int n = StackSize(p);
		if (n > 1)
		{
			string here = HereText(n);
			lines.Insert(here);
		}
		m_HoverHint.Show(p.m_Event, lines, mx, my);
	}

	protected void LayerInput(DZS_Instance inst, DZS_Screen scr)
	{
		bool blocked = Blocked(scr);
		bool over = !blocked && (!scr.IsCursorVisible() || scr.IsPointerOverScene());
		bool lmb = DZS_UiRegistry.KeyDown("MouseLeft");
		bool lDown = lmb && !m_LmbWas;
		bool lUp = !lmb && m_LmbWas;
		m_LmbWas = lmb;
		int mx, my;
		GetMousePos(mx, my);
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = PointerDir(scr);
		m_Hover = "";
		string hoverStack = "";
		if (over && !m_Drag && m_PlaceClass == "")
		{
			m_Hover = PickDot(mx, my, MarkersOn(scr));
			if (m_Hover == "")
			{
				hoverStack = CarUnderPointer(scr);
				DZS_SpawnPoint first = FirstOf(hoverStack);
				if (first)
				{
					m_Hover = first.Key();
					DZS_SpawnPoint cur = Selected();
					if (cur && StackOf(cur) == hoverStack)
						m_Hover = m_Sel;
				}
			}
		}
		if (m_PlaceDrag && lUp)
		{
			vector dropAt;
			string dropKey = m_PlaceClass;
			Arm("", false);
			if (over && DZS_LayerRay.Ground(from, dir, dropAt))
				PlaceAt(inst, dropKey, dropAt);
			return;
		}
		if (lDown && over && !scr.IsMouseSwallowed())
		{
			if (m_PlaceClass != "" && !m_PlaceDrag)
			{
				vector clickAt;
				string clickKey = m_PlaceClass;
				Arm("", false);
				if (DZS_LayerRay.Ground(from, dir, clickAt))
					PlaceAt(inst, clickKey, clickAt);
				return;
			}
			m_Cycle = false;
			if (m_Hover != "" && m_Hover == m_Sel && hoverStack != "")
				m_Cycle = m_Stacks.Get(hoverStack).Count() > 1;
			if (m_Hover != m_Sel)
				Select(m_Hover);
			m_Press = m_Hover != "";
			m_PressX = mx;
			m_PressY = my;
		}
		if (m_Press && lmb && !m_Drag && Selected() && !Selected().m_Kept && (Math.AbsFloat(mx - m_PressX) > 4 || Math.AbsFloat(my - m_PressY) > 4))
		{
			m_Drag = true;
			m_Cycle = false;
		}
		DZS_SpawnPoint sp = Selected();
		vector to;
		if (m_Drag && sp && DZS_LayerRay.Ground(from, dir, to))
			m_Cars.Carry(StackOf(sp), to);
		if (m_Drag && !lmb)
		{
			m_Drag = false;
			m_Press = false;
			if (sp && DZS_LayerRay.Ground(from, dir, to))
				MovePoint(inst, sp, to[0], to[2], sp.m_A);
		}
		if (lUp && m_Cycle)
		{
			m_Cycle = false;
			NextInStack();
		}
		if (!lmb)
			m_Press = false;
	}

	// The selected point's stack: the next of its points.
	protected void NextInStack()
	{
		DZS_SpawnPoint p = Selected();
		if (!p)
			return;
		array<DZS_SpawnPoint> st = m_Stacks.Get(StackOf(p));
		if (!st || st.Count() < 2)
			return;
		int at = st.Find(p);
		Select(st[(at + 1) % st.Count()].Key());
		DZS_Log.Info("spawns: next point here " + m_Sel);
	}

	// ---- drawing ----

	protected void DrawCarFrame(string key, DZS_SpawnCar c, bool selected)
	{
		if (!c || !c.m_Obj)
			return;
		array<vector> corners = new array<vector>();
		DZS_SpawnCars.Corners(c, corners);
		int accent = DZS_Color.Get("accent");
		if (selected)
		{
			DZS_WorldLines.Frame(key, corners, Math.Max(2, DZS_Scale.Px(2)), accent, 0, DZS_WorldLines.PRIO_NOW);
			return;
		}
		int half = (accent & 0x00FFFFFF) | (128 << 24);
		DZS_WorldLines.Frame(key, corners, Math.Max(1, DZS_Scale.Px(1)), half, 0, DZS_WorldLines.PRIO_NOW);
	}

	// A ring on the ground around a car.
	protected void GroundRing(DZS_SpawnCar c, int color)
	{
		float lx = c.m_Max[0] - c.m_Min[0];
		float lz = c.m_Max[2] - c.m_Min[2];
		float r = 0.5 * Math.Sqrt(lx * lx + lz * lz) + 0.6;
		vector mid = c.m_Obj.ModelToWorld(Vector((c.m_Min[0] + c.m_Max[0]) * 0.5, 0, (c.m_Min[2] + c.m_Max[2]) * 0.5));
		float w = Math.Max(2, DZS_Scale.Px(2));
		int segs = 32;
		vector prev;
		bool havePrev = false;
		for (int i = 0; i <= segs; i++)
		{
			float a = Math.PI2 * i / segs;
			float x = mid[0] + Math.Cos(a) * r;
			float z = mid[2] + Math.Sin(a) * r;
			vector p = Vector(x, GetGame().SurfaceY(x, z) + 0.08, z);
			if (havePrev && DZS_WorldLines.Depth(prev) > DZS_WorldLines.NEAR && DZS_WorldLines.Depth(p) > DZS_WorldLines.NEAR)
			{
				vector s0 = DZS_PlaceMath.Project(prev);
				vector s1 = DZS_PlaceMath.Project(p);
				DZS_Clip.Line(m_Canvas, s0[0], s0[1], s1[0], s1[1], w, color);
			}
			prev = p;
			havePrev = true;
		}
	}

	protected void ClearCanvas()
	{
		if (!m_Canvas)
			return;
		m_Canvas.Clear();
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Canvas, 0, 0, sw, sh);
	}

	protected void Draw(DZS_Screen scr)
	{
		if (!m_Canvas)
			return;
		ClearCanvas();
		if (scr.IsUiHidden() || scr.HasKeyboardOwner("map"))
		{
			m_HoverHint.Hide();
			m_SelPlate.Hide();
			return;
		}
		vector cam = GetGame().GetCurrentCameraPosition();
		bool all = MarkersOn(scr);
		DZS_SpawnPoint sel = Selected();
		DZS_SpawnCar selCar = null;
		if (sel && !m_Drag)
			selCar = CarOf(sel);
		DZS_SpawnPoint hov = PointOf(m_Hover);
		DZS_SpawnCar hovCar = CarOf(hov);
		if (hovCar && hovCar != selCar)
			DrawCarFrame("sp:h:" + hovCar.m_Stack, hovCar, false);
		DZS_SpawnCar listCar = CarOf(PointOf(m_ListHover));
		if (listCar && listCar != selCar && listCar != hovCar)
			DrawCarFrame("sp:l:" + listCar.m_Stack, listCar, true);
		if (selCar)
			DrawCarFrame("sp:s:" + selCar.m_Stack, selCar, true);
		DZS_WorldLines.Flush(m_Canvas);
		if (selCar)
			GroundRing(selCar, DZS_Color.Get("accent"));
		array<DZS_SpawnPoint> off = new array<DZS_SpawnPoint>();
		Specials();
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (!DotCandidate(p, all))
				continue;
			vector w = Ground(p.m_X, p.m_Z) + "0 0.8 0";
			float dist = vector.Distance(w, cam);
			if (!DotShown(p, all, dist))
				continue;
			if (m_SelEvent != "" && p.m_Event == m_SelEvent && !OnScreen(w, scr))
				off.Insert(p);
			float d = DZS_Scale.Px(9);
			if (dist > 800)
				d = DZS_Scale.Px(6);
			bool ring = false;
			if (p == m_SelP || p == m_HoverP || p == m_ListP)
			{
				d = DZS_Scale.Px(16);
				ring = true;
			}
			if (m_Drag && p == m_SelP)
			{
				vector to;
				if (DZS_LayerRay.Ground(GetGame().GetCurrentCameraPosition(), PointerDir(scr), to))
					w = to + "0 0.8 0";
			}
			DZS_WorldLines.Dot(m_Canvas, w, d, ColorOf(p), ring);
		}
		EdgeArrows(off, scr);
		SelLabel(sel, selCar);
		HoverHint(hov);
	}

	protected bool OnScreen(vector w, DZS_Screen scr)
	{
		vector s = DZS_PlaceMath.Project(w);
		if (s[2] <= 0.5)
			return false;
		float x, y, rw, rh;
		scr.GetSceneRect(x, y, rw, rh);
		return s[0] >= x && s[0] <= x + rw && s[1] >= y && s[1] <= y + rh;
	}

	// Arrows at the edge of the scene towards the nearest points of the selected event out of view.
	protected void EdgeArrows(array<DZS_SpawnPoint> off, DZS_Screen scr)
	{
		if (off.Count() == 0)
			return;
		vector cam = GetGame().GetCurrentCameraPosition();
		vector fwd = GetGame().GetCurrentCameraDirection();
		float hl = Math.Sqrt(fwd[0] * fwd[0] + fwd[2] * fwd[2]);
		vector right = "1 0 0";
		if (hl > 0.0001)
			right = Vector(fwd[2] / hl, 0, -fwd[0] / hl);
		vector up = right * fwd;
		if (up[1] < 0)
			up = -up;
		float rx, ry, rw, rh;
		scr.GetSceneRect(rx, ry, rw, rh);
		float inset = DZS_Scale.Px(28);
		float x0 = rx + inset;
		float y0 = ry + inset;
		float x1 = rx + rw - inset;
		float y1 = ry + rh - inset;
		float cx = (x0 + x1) * 0.5;
		float cy = (y0 + y1) * 0.5;
		array<string> order = new array<string>();
		for (int i = 0; i < off.Count(); i++)
		{
			int dm = vector.Distance(Vector(off[i].m_X, cam[1], off[i].m_Z), cam);
			string ds = dm.ToString();
			while (ds.Length() < 7)
				ds = "0" + ds;
			order.Insert(ds + "|" + i);
		}
		order.Sort();
		int color = DZS_Color.Get("accent");
		for (int k = 0; k < order.Count() && k < ARROWS; k++)
		{
			string o = order[k];
			DZS_SpawnPoint p = off[o.Substring(8, o.Length() - 8).ToInt()];
			vector w = Ground(p.m_X, p.m_Z) + "0 0.8 0";
			vector rel = w - cam;
			float dx, dy;
			vector s = DZS_PlaceMath.Project(w);
			if (s[2] > 0.5)
			{
				dx = s[0] - cx;
				dy = s[1] - cy;
			}
			else
			{
				dx = vector.Dot(rel, right);
				dy = -vector.Dot(rel, up);
				if (Math.AbsFloat(dx) + Math.AbsFloat(dy) < 0.001)
					dy = 1;
			}
			float len = Math.Sqrt(dx * dx + dy * dy);
			dx /= len;
			dy /= len;
			float t = 1e9;
			if (dx > 0.0001)
				t = Math.Min(t, (x1 - cx) / dx);
			if (dx < -0.0001)
				t = Math.Min(t, (x0 - cx) / dx);
			if (dy > 0.0001)
				t = Math.Min(t, (y1 - cy) / dy);
			if (dy < -0.0001)
				t = Math.Min(t, (y0 - cy) / dy);
			float size = DZS_Scale.Px(24);
			int col = color;
			if (k > 0)
			{
				size = DZS_Scale.Px(16);
				col = (color & 0x00FFFFFF) | (170 << 24);
			}
			EdgeArrow(cx + dx * t, cy + dy * t, dx, dy, size, col);
		}
	}

	// A filled triangle with its tip at (x, y) pointing along (dx, dy).
	protected void EdgeArrow(float x, float y, float dx, float dy, float size, int color)
	{
		float bx = x - dx * size;
		float by = y - dy * size;
		float nx = -dy * size * 0.55;
		float ny = dx * size * 0.55;
		int steps = 8;
		for (int i = 0; i <= steps; i++)
		{
			float f = i;
			f = f * 2 / steps - 1;
			DZS_Clip.Line(m_Canvas, x, y, bx + nx * f, by + ny * f, Math.Max(2, DZS_Scale.Px(2)), color);
		}
	}

	// The selected point's label above its car (or its dot).
	protected void SelLabel(DZS_SpawnPoint sel, DZS_SpawnCar car)
	{
		if (!sel || m_Drag)
		{
			m_SelPlate.Hide();
			return;
		}
		vector top = Ground(sel.m_X, sel.m_Z) + "0 1.6 0";
		if (car && car.m_Obj)
			top = car.m_Obj.ModelToWorld(Vector((car.m_Min[0] + car.m_Max[0]) * 0.5, car.m_Max[1], (car.m_Min[2] + car.m_Max[2]) * 0.5)) + "0 0.4 0";
		float sx, sy;
		if (!DZS_PlaceMath.ToScreen(top, sx, sy))
		{
			m_SelPlate.Hide();
			return;
		}
		array<string> lines = new array<string>();
		string where = "x " + Math.Round(sel.m_X) + " · z " + Math.Round(sel.m_Z) + " · " + Math.Round(sel.m_A) + "°";
		if (sel.m_Kept)
			where += DZS_Texts.Pick(" · außerhalb angelegt", " · made outside");
		else if (sel.m_Own)
			where += DZS_Texts.Pick(" · eigener Punkt", " · own point");
		else if (sel.m_Moved)
			where += DZS_Texts.Pick(" · verschoben", " · moved");
		lines.Insert(where);
		if (car)
			lines.Insert(car.m_Cls);
		int n = StackSize(sel);
		if (n > 1)
		{
			string here = HereText(n);
			string again = DZS_Texts.Pick(" · erneut klicken wechselt", " · click again for the next");
			lines.Insert(here + again);
		}
		float lineH = DZS_Font.WidgetHeight(DZS_Font.READ, 13) + DZS_Scale.Px(4);
		float h = DZS_Scale.Px(16) + lineH * (1 + lines.Count());
		m_SelPlate.Show(sel.m_Event, lines, sx - DZS_Scale.Px(16), sy - h - DZS_Scale.Px(8), false);
	}

	protected void HoverHint(DZS_SpawnPoint hov)
	{
		if (!hov || hov.Key() == m_Sel || m_Drag)
		{
			m_HoverHint.Hide();
			return;
		}
		int mx, my;
		GetMousePos(mx, my);
		array<string> lines = new array<string>();
		int n = StackSize(hov);
		if (n > 1)
		{
			string here = HereText(n);
			lines.Insert(here);
		}
		m_HoverHint.Show(hov.m_Event, lines, mx, my);
	}

	// The map shows every spawn point, the selected one with a ring.
	protected void MapMarks()
	{
		if (m_MarksGen == m_PtsGen && m_MarksSel == m_Sel && m_MarksEvent == m_SelEvent)
			return;
		m_MarksGen = m_PtsGen;
		m_MarksSel = m_Sel;
		m_MarksEvent = m_SelEvent;
		array<ref DZS_MapMark> marks = new array<ref DZS_MapMark>();
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.m_Gone)
				continue;
			DZS_MapMark m = new DZS_MapMark();
			m.m_Id = p.Key();
			m.m_X = p.m_X;
			m.m_Z = p.m_Z;
			m.m_Color = ColorOf(p);
			m.m_Size = 6;
			if (m_SelEvent != "" && p.m_Event == m_SelEvent)
				m.m_Size = 10;
			if (m.m_Id == m_Sel)
			{
				m.m_Ring = true;
				m.m_Size = 12;
			}
			m.m_Tip = p.m_Event + " · x " + Math.Round(p.m_X) + " · z " + Math.Round(p.m_Z);
			marks.Insert(m);
		}
		DZS_MapMarks.Set("spawns", marks, m_MapOwner);
	}

	// ---- for tests (projtest "spawns …") ----

	string StatusLine()
	{
		string s = "on " + DZS_Str.Bool(m_On) + " base '" + m_BaseFor + "' loading " + DZS_Str.Bool(Loading()) + " events " + m_Events.Count() + " points " + PointsShown() + " stacks " + m_Stacks.Count();
		s += " cars " + m_Cars.Numbers() + ", waiting " + m_Cars.Waiting();
		s += " sel '" + m_Sel + "' event '" + m_SelEvent + "' hover '" + m_Hover + "'";
		return s;
	}

	// The point of the car nearest to the camera, selected.
	string SelectNearestCar()
	{
		vector cam = GetGame().GetCurrentCameraPosition();
		string best = "";
		float bestD = 1e12;
		foreach (string sk, DZS_SpawnCar c : m_Cars.m_Built)
		{
			float d = vector.Distance(c.m_Pos, cam);
			if (d < bestD && FirstOf(sk))
			{
				bestD = d;
				best = FirstOf(sk).Key();
			}
		}
		Select(best);
		return best;
	}

	// A stack of more than one point near the camera, its first point selected; "" if none within 1000 m.
	string SelectStacked()
	{
		vector cam = GetGame().GetCurrentCameraPosition();
		string best = "";
		float bestD = 1e12;
		foreach (string sk, array<DZS_SpawnPoint> st : m_Stacks)
		{
			if (st.Count() < 2)
				continue;
			float d = vector.Distance(Vector(st[0].m_X, cam[1], st[0].m_Z), cam);
			if (d < bestD)
			{
				bestD = d;
				best = st[0].Key();
			}
		}
		if (best != "")
			Select(best);
		return best;
	}

	// N own points of an event on a 12 m grid around the camera, one step (undo removes them).
	int TestGrid(int n, string ev)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !m_Events.Contains(ev))
			return 0;
		vector cam = GetGame().GetCurrentCameraPosition();
		int side = Math.Ceil(Math.Sqrt(n));
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		for (int i = 0; i < n; i++)
		{
			int col = i % side;
			int row = i / side;
			int half = side / 2;
			float x = cam[0] + (col - half) * 12;
			float z = cam[2] + (row - half) * 12;
			map<string, string> f = new map<string, string>();
			f.Set("event", ev);
			f.Set("origin", "own");
			f.Set("x", NumText(x));
			f.Set("z", NumText(z));
			f.Set("a", "0");
			b.Create(DZS_SpawnsModel.POINT, DZS_PROJECT_ID, DZS_END, f);
		}
		b.Commit("Spawnpunkte hinzufügen", "Add spawn points");
		return n;
	}

	// ---- changes ----

	protected string NumText(float f)
	{
		return DZS_PatchNum.Fixed6(f.ToString());
	}

	void MovePoint(DZS_Instance inst, DZS_SpawnPoint p, float x, float z, float a)
	{
		if (p.m_Kept)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		if (p.m_Elem != "")
		{
			b.SetField(p.m_Elem, "x", NumText(x));
			b.SetField(p.m_Elem, "z", NumText(z));
			b.SetField(p.m_Elem, "a", NumText(a));
		}
		else
		{
			map<string, string> f = new map<string, string>();
			f.Set("event", p.m_Event);
			f.Set("origin", "base");
			f.Set("bx", p.m_BX);
			f.Set("bz", p.m_BZ);
			f.Set("ba", p.m_BA);
			f.Set("x", NumText(x));
			f.Set("z", NumText(z));
			f.Set("a", NumText(a));
			string id = b.Create(DZS_SpawnsModel.POINT, DZS_PROJECT_ID, DZS_END, f);
			m_Sel = "e:" + id;
		}
		b.Commit("Spawnpunkt verschieben", "Move spawn point");
		DZS_Log.Info("spawns: moved a point of " + p.m_Event + " to " + x + " " + z);
	}

	void RemoveSelected()
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_SpawnPoint p = Selected();
		if (!inst || !p || p.m_Kept)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		if (p.m_Own)
		{
			b.Delete(p.m_Elem);
			DZS_SpawnEvent e = m_Events.Get(p.m_Event);
			if (e && e.m_Elem != "" && PointsOf(p.m_Event) <= 1)
				b.Delete(e.m_Elem);
		}
		else if (p.m_Elem != "")
			b.SetField(p.m_Elem, "gone", "1");
		else
		{
			map<string, string> f = new map<string, string>();
			f.Set("event", p.m_Event);
			f.Set("origin", "base");
			f.Set("bx", p.m_BX);
			f.Set("bz", p.m_BZ);
			f.Set("ba", p.m_BA);
			f.Set("x", p.m_BX);
			f.Set("z", p.m_BZ);
			f.Set("gone", "1");
			b.Create(DZS_SpawnsModel.POINT, DZS_PROJECT_ID, DZS_END, f);
		}
		b.Commit("Spawnpunkt entfernen", "Remove spawn point");
		m_Sel = "";
		DZS_Log.Info("spawns: removed a point of " + p.m_Event);
	}

	int PointsOf(string ev)
	{
		int n = 0;
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.m_Event == ev && !p.m_Gone)
				n++;
		}
		return n;
	}

	protected void PlaceAt(DZS_Instance inst, string key, vector pos)
	{
		if (key.IndexOf("ev:") == 0)
			AddPoint(inst, key.Substring(3, key.Length() - 3), pos[0], pos[2], CameraYaw());
		else
			FixedVehicle(inst, key, pos, CameraYaw());
	}

	// A new own point of an event at a ground position.
	void AddPoint(DZS_Instance inst, string ev, float x, float z, float yaw)
	{
		float a = yaw;
		if (a < 0)
			a += 360;
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		map<string, string> f = new map<string, string>();
		f.Set("event", ev);
		f.Set("origin", "own");
		f.Set("x", NumText(x));
		f.Set("z", NumText(z));
		f.Set("a", NumText(a));
		string id = b.Create(DZS_SpawnsModel.POINT, DZS_PROJECT_ID, DZS_END, f);
		b.Commit("Spawnpunkt hinzufügen", "Add spawn point");
		Select("e:" + id);
		DZS_Log.Info("spawns: added a point to " + ev + " at " + x + " " + z);
	}

	// A value of the selected point's event: nominal or "child:<type>:max". Own events keep nominal on the element.
	void SetEventValue(string ev, string field, string value)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_SpawnEvent e = m_Events.Get(ev);
		if (!inst || !e)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		if (e.m_Elem != "")
		{
			if (field != "nominal")
				return;
			b.SetField(e.m_Elem, "nominal", value);
		}
		else
		{
			string found = "";
			array<string> sets = DZS_SpawnsModel.Of(inst, DZS_SpawnsModel.SET);
			foreach (string sid : sets)
			{
				if (DZS_SpawnsModel.Field(inst, sid, "event") == ev && DZS_SpawnsModel.Field(inst, sid, "field") == field)
					found = sid;
			}
			string base = e.m_BaseNominal;
			if (field.IndexOf("child:") == 0)
			{
				array<string> parts = new array<string>();
				field.Split(":", parts);
				int at = e.m_Types.Find(parts[1]);
				if (at >= 0)
					base = e.m_BaseMax[at];
			}
			if (found != "" && value == base)
				b.Delete(found);
			else if (found != "")
				b.SetField(found, "to", value);
			else if (value != base)
			{
				map<string, string> f = new map<string, string>();
				f.Set("event", ev);
				f.Set("field", field);
				f.Set("from", base);
				f.Set("to", value);
				b.Create(DZS_SpawnsModel.SET, DZS_PROJECT_ID, DZS_END, f);
			}
		}
		b.Commit("Event ändern", "Change event");
		DZS_Log.Info("spawns: " + ev + " " + field + " = " + value);
	}

	// Placing a vehicle: an own event "exactly this car here" (nominal 1, one child, one point).
	void FixedVehicle(DZS_Instance inst, string cls, vector pos, float yaw)
	{
		string stem = "VehicleDzs" + cls;
		string name = stem;
		int n = 2;
		while (DZS_SpawnsModel.OwnEvent(inst, name) != "")
		{
			name = stem + "_" + n;
			n++;
		}
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		map<string, string> ef = new map<string, string>();
		ef.Set("name", name);
		ef.Set("class", cls);
		ef.Set("nominal", "1");
		b.Create(DZS_SpawnsModel.EVENT, DZS_PROJECT_ID, DZS_END, ef);
		map<string, string> pf = new map<string, string>();
		pf.Set("event", name);
		pf.Set("origin", "own");
		pf.Set("x", NumText(pos[0]));
		pf.Set("z", NumText(pos[2]));
		float a = yaw;
		if (a < 0)
			a += 360;
		pf.Set("a", NumText(a));
		string pid = b.Create(DZS_SpawnsModel.POINT, DZS_PROJECT_ID, DZS_END, pf);
		DZS_SubmitResult r = b.Commit("Festes Fahrzeug: " + cls, "Fixed vehicle: " + cls);
		DZS_Log.Info("spawns: fixed vehicle " + cls + " as event " + name + " at " + pos.ToString() + " ok " + r.Ok());
		if (m_On)
		{
			Select("e:" + pid);
			m_Kernel.GetScreen().ShowMessage("spawns.fixed", "info", DZS_Texts.Pick("Festes Fahrzeug angelegt (eigenes Event „", "Fixed vehicle added (own event '") + name + DZS_Texts.Pick("“)", "')"), 6.0);
		}
		else
			m_Kernel.GetScreen().ShowMessage("spawns.fixed", "info", DZS_Texts.Pick("Festes Fahrzeug angelegt (eigenes Event „", "Fixed vehicle added (own event '") + name + DZS_Texts.Pick("“) – bearbeiten in der Ebene Spawns", "') – edit it in the Spawns layer"), 6.0);
	}
}

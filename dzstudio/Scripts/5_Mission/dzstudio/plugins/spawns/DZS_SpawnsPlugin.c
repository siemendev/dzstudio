// The Spawns layer: vehicle spawn points of the base (vanilla or the mission folder chosen for the project) with the
// project's changes, ghost cars near the camera and dots further away. A point is selected and dragged like loot
// points (own picking, the layer owns the scene tool); its event shows in the side panel. Placing a vehicle class in
// the Objects layer makes an own fixed event. Layer tabs sit in the top bar.
class DZS_SpawnsActions : DZS_ActionHandler
{
	DZS_SpawnsPlugin m_P;

	override bool IsActionChecked(string actionId)
	{
		if (actionId == "spawns.layer.objects")
			return DZS_Layers.Active() == "";
		if (actionId == "spawns.layer.spawns")
			return m_P.m_On;
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
		if (actionId == "spawns.layer.objects")
			DZS_Layers.Only("");
		else if (actionId == "spawns.layer.spawns")
			m_P.SetOn(true);
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
		if (m_P.m_Adding)
			m_P.m_Adding = false;
		else if (m_P.m_Sel != "")
			m_P.Select("");
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
}

[DZS_RegisterPlugin(DZS_SpawnsPlugin)]
class DZS_SpawnsPlugin : DZS_Plugin
{
	static const string TOOL = "spawns";
	static const string PANEL = "spawns.event";
	static const float GHOST_RANGE = 150;
	static const int GHOST_MAX = 60;

	bool m_On;
	string m_Sel;
	string m_Hover;
	bool m_Adding;
	int m_Gen;
	protected bool m_World;
	protected ref DZS_SpawnsActions m_Actions;
	protected ref DZS_SpawnsOverride m_Override;
	protected ref DZS_SpawnsDivert m_Divert;
	protected ref DZS_SpawnsEsc m_Esc;
	protected ref DZS_SpawnsLayer m_Layer;
	protected ref DZS_SpawnsPanel m_Panel;
	protected ref DZS_SpawnsBase m_Base;
	protected string m_BaseFor;
	ref map<string, ref DZS_SpawnEvent> m_Events = new map<string, ref DZS_SpawnEvent>();
	ref array<ref DZS_SpawnPoint> m_Points = new array<ref DZS_SpawnPoint>();
	protected int m_ViewVer = -1;
	protected int m_StateVer;
	protected CanvasWidget m_Canvas;
	protected ref map<string, Object> m_Ghosts = new map<string, Object>();
	protected float m_GhostWait;
	protected bool m_LmbWas;
	protected bool m_Drag;
	protected float m_PressX;
	protected float m_PressY;
	protected bool m_Press;

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
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterAction("spawns", "spawns.layer.objects", "Objekte", "Objects", null, m_Actions);
		ui.RegisterTool("spawns", "spawns.layer.objects", "Objekte", "Objects", "switches", "box", -100);
		ui.SetToolLabelled("spawns.layer.objects");
		ui.SetToolTip("spawns.layer.objects", "Ebene Objekte: platzieren, verschieben, Loot", "Objects layer: place, move, loot", "spawns.layer.objects");
		ui.RegisterAction("spawns", "spawns.layer.spawns", "Spawns", "Spawns", null, m_Actions);
		ui.RegisterTool("spawns", "spawns.layer.spawns", "Spawns", "Spawns", "switches", "car", -99);
		ui.SetToolLabelled("spawns.layer.spawns");
		ui.SetToolTip("spawns.layer.spawns", "Ebene Spawns: Fahrzeug-Spawnpunkte und Events", "Spawns layer: vehicle spawn points and events", "spawns.layer.spawns");
		m_Panel = new DZS_SpawnsPanel(this);
		ui.RegisterPanel("spawns", PANEL, "Event", "Event", "right", "car", m_Panel, 15);
		ui.SetPanelShown(PANEL, false);
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		if (!m_World)
			return;
		m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", m_Kernel.GetScreen().GetSceneLayer()));
		m_Esc = new DZS_SpawnsEsc();
		m_Esc.m_P = this;
		m_Kernel.GetScreen().AddEscHandler(m_Esc, 87);
	}

	override void OnSessionEnd()
	{
		SetOn(false);
		ClearGhosts();
		if (m_Canvas)
			m_Canvas.Unlink();
		m_Canvas = null;
		m_World = false;
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		SetOn(false);
		m_Base = null;
		m_BaseFor = "";
		m_ViewVer = -1;
		if (inst)
			RestoreFromProject(inst);
	}

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		m_StateVer++;
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

	protected string BaseDir()
	{
		DZS_PatchPlugin pp = DZS_PatchPlugin.Cast(m_Kernel.GetPlugin("patcher"));
		if (!pp)
			return "";
		return pp.Applier().LayerBase();
	}

	void View(DZS_Instance inst)
	{
		string dir = BaseDir();
		if (!m_Base || dir != m_BaseFor)
		{
			m_Base = DZS_SpawnsBase.Load(dir);
			m_BaseFor = dir;
			m_ViewVer = -1;
			DZS_Log.Info("spawns: base " + dir + ", " + m_Base.m_Events.Count() + " vehicle events, " + m_Base.m_Points.Count() + " points");
		}
		if (m_ViewVer == m_StateVer)
			return;
		m_ViewVer = m_StateVer;
		m_Base.Effective(inst, m_Events, m_Points);
		m_Gen++;
		if (m_Sel != "" && !PointOf(m_Sel))
			m_Sel = "";
	}

	bool HasBase()
	{
		return m_Base && m_BaseFor != "";
	}

	DZS_SpawnPoint PointOf(string key)
	{
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.Key() == key)
				return p;
		}
		return null;
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
			m_Adding = false;
			m_ViewVer = -1;
			m_Kernel.SetSceneTool(TOOL);
			m_Kernel.ClaimSceneState(true);
			ui.SetOverride(m_Override);
			ui.SetPanelShown(PANEL, true);
			ui.SetPanelShown("properties.panel", false);
			DZS_Instance inst = m_Kernel.GetShown();
			if (inst)
				View(inst);
			if (!HasBase())
				m_Kernel.GetScreen().ShowMessage("spawns.base", "warning", DZS_Texts.Pick("Keine Basis geladen – im Menü Projekt „Auf Missionsordner anwenden …“ eine Basis wählen oder Vanilla laden", "No base loaded – choose a base or fetch vanilla under Project › Apply to mission folder …"), 8.0);
			DZS_Log.Info("spawns: layer on");
		}
		else
		{
			m_On = false;
			m_Drag = false;
			m_Adding = false;
			m_Kernel.SetSceneTool("");
			m_Kernel.ClaimSceneState(false);
			ui.SetSceneState("none");
			ui.SetOverride(null);
			ui.SetPanelShown(PANEL, false);
			ui.SetPanelShown("properties.panel", true);
			m_Kernel.GetScreen().SetHint("spawns", 30, "");
			ClearGhosts();
			if (m_Canvas)
				m_Canvas.Clear();
			DZS_Log.Info("spawns: layer off");
		}
		m_Gen++;
	}

	void Select(string key)
	{
		m_Sel = key;
		m_Gen++;
	}

	DZS_SpawnPoint Selected()
	{
		if (m_Sel == "")
			return null;
		return PointOf(m_Sel);
	}

	// ---- every frame ----

	override void OnUpdate(float dt)
	{
		if (!m_On || !m_World)
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_Screen scr = m_Kernel.GetScreen();
		if (!inst || scr.IsHome())
		{
			SetOn(false);
			return;
		}
		View(inst);
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat && cat.GetHand() != "")
			cat.DropHand();
		if (m_Sel != "")
			m_Kernel.GetUi().SetSceneState("selection");
		else
			m_Kernel.GetUi().SetSceneState("none");
		LayerInput(inst, scr);
		m_GhostWait -= dt;
		if (m_GhostWait <= 0)
		{
			m_GhostWait = 0.4;
			Ghosts();
		}
		Draw();
		string hint;
		if (m_Adding)
			hint = DZS_Texts.Pick("Klick in die Szene setzt einen Spawnpunkt · Esc bricht ab", "Click in the scene to add a spawn point · Esc cancels");
		else if (m_Sel == "")
			hint = DZS_Texts.Pick("Spawnpunkt anklicken · Fahrzeuge aus dem Katalog in der Ebene Objekte setzen", "Click a spawn point · place vehicles from the catalog in the Objects layer");
		else
			hint = DZS_Texts.Pick("Ziehen verschiebt · Entf entfernt · Esc hebt die Auswahl auf", "Drag to move · Del removes · Esc clears the selection");
		scr.SetHint("spawns", 30, hint);
	}

	protected vector Ground(float x, float z)
	{
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	// The nearest point whose dot is within 14 px of the mouse.
	protected string Pick(float mx, float my)
	{
		string best = "";
		float bestD = DZS_Scale.Px(14);
		vector cam = GetGame().GetCurrentCameraPosition();
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.m_Gone)
				continue;
			vector w = Ground(p.m_X, p.m_Z) + "0 0.8 0";
			if (vector.Distance(w, cam) > 2500)
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

	// Where the pointer ray meets the ground (stepping along it), or false.
	protected bool RayGround(vector from, vector dir, out vector hit)
	{
		float step = 2;
		vector prev = from;
		for (int i = 1; i < 1500; i++)
		{
			vector p = from + dir * (i * step);
			if (p[1] <= GetGame().SurfaceY(p[0], p[2]))
			{
				vector a = prev;
				vector b = p;
				for (int k = 0; k < 12; k++)
				{
					vector m = (a + b) * 0.5;
					if (m[1] <= GetGame().SurfaceY(m[0], m[2]))
						b = m;
					else
						a = m;
				}
				hit = b;
				return true;
			}
			prev = p;
		}
		return false;
	}

	protected void LayerInput(DZS_Instance inst, DZS_Screen scr)
	{
		bool blocked = scr.HasDialog() || scr.HasKeyboardOwner("map") || scr.HasKeyboardOwner("menu") || scr.HasKeyboardOwner("fullsearch");
		bool over = !blocked && (!scr.IsCursorVisible() || scr.IsPointerOverScene());
		bool lmb = DZS_UiRegistry.KeyDown("MouseLeft");
		bool lDown = lmb && !m_LmbWas;
		m_LmbWas = lmb;
		int mx, my;
		GetMousePos(mx, my);
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		if (!scr.IsCursorVisible())
			dir = GetGame().GetCurrentCameraDirection();
		m_Hover = "";
		if (over && !m_Drag)
			m_Hover = Pick(mx, my);
		if (lDown && over && !scr.IsMouseSwallowed())
		{
			if (m_Adding)
			{
				vector at;
				if (RayGround(from, dir, at))
					AddPoint(inst, at[0], at[2]);
				m_Adding = false;
				return;
			}
			Select(m_Hover);
			m_Press = m_Hover != "";
			m_PressX = mx;
			m_PressY = my;
		}
		if (m_Press && lmb && !m_Drag && (Math.AbsFloat(mx - m_PressX) > 4 || Math.AbsFloat(my - m_PressY) > 4))
			m_Drag = true;
		if (m_Drag && !lmb)
		{
			m_Drag = false;
			m_Press = false;
			vector to;
			DZS_SpawnPoint sp = Selected();
			if (sp && RayGround(from, dir, to))
				MovePoint(inst, sp, to[0], to[2], sp.m_A);
		}
		if (!lmb)
			m_Press = false;
	}

	// ---- drawing ----

	protected void Draw()
	{
		if (!m_Canvas)
			return;
		m_Canvas.Clear();
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Canvas, 0, 0, sw, sh);
		vector cam = GetGame().GetCurrentCameraPosition();
		int normal = DZS_Color.Get("accent");
		int own = DZS_Color.Get("success");
		int moved = DZS_Color.Get("warning");
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.m_Gone)
				continue;
			vector w = Ground(p.m_X, p.m_Z) + "0 0.8 0";
			float dist = vector.Distance(w, cam);
			if (dist > 2500)
				continue;
			string key = p.Key();
			int col = normal;
			if (p.m_Own)
				col = own;
			else if (p.m_Moved)
				col = moved;
			float d = DZS_Scale.Px(9);
			if (dist > 800)
				d = DZS_Scale.Px(6);
			bool ring = false;
			if (key == m_Sel || key == m_Hover)
			{
				d = DZS_Scale.Px(16);
				ring = true;
			}
			if (m_Drag && key == m_Sel)
			{
				int mx, my;
				GetMousePos(mx, my);
				vector from = GetGame().GetCurrentCameraPosition();
				vector dir = GetGame().GetPointerDirection();
				vector to;
				if (RayGround(from, dir, to))
					w = to + "0 0.8 0";
			}
			DZS_WorldLines.Dot(m_Canvas, w, d, col, ring);
		}
	}

	// Ghost cars for the points within GHOST_RANGE of the camera (the event's first class, heading a).
	protected void Ghosts()
	{
		vector cam = GetGame().GetCurrentCameraPosition();
		map<string, bool> want = new map<string, bool>();
		int n = 0;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		foreach (DZS_SpawnPoint p : m_Points)
		{
			if (p.m_Gone || n >= GHOST_MAX)
				continue;
			vector w = Ground(p.m_X, p.m_Z);
			if (vector.Distance(w, cam) > GHOST_RANGE)
				continue;
			DZS_SpawnEvent e = m_Events.Get(p.m_Event);
			if (!e || e.m_Types.Count() == 0)
				continue;
			string key = p.Key() + "@" + p.m_X + ":" + p.m_Z + ":" + p.m_A;
			want.Set(key, true);
			n++;
			if (m_Ghosts.Contains(key))
				continue;
			Object o = null;
			if (scene)
				o = scene.MakeTemp(e.m_Types[0], w, Vector(p.m_A, 0, 0), 1);
			m_Ghosts.Set(key, o);
		}
		array<string> drop = new array<string>();
		foreach (string gk, Object go : m_Ghosts)
		{
			if (!want.Contains(gk))
				drop.Insert(gk);
		}
		foreach (string dk : drop)
		{
			Object old = m_Ghosts.Get(dk);
			if (old)
				GetGame().ObjectDelete(old);
			m_Ghosts.Remove(dk);
		}
	}

	protected void ClearGhosts()
	{
		foreach (string gk, Object go : m_Ghosts)
		{
			if (go)
				GetGame().ObjectDelete(go);
		}
		m_Ghosts.Clear();
	}

	// ---- changes ----

	protected string NumText(float f)
	{
		return DZS_PatchNum.Fixed6(f.ToString());
	}

	void MovePoint(DZS_Instance inst, DZS_SpawnPoint p, float x, float z, float a)
	{
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
		if (!inst || !p)
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

	// A new point for the selected point's event at a ground position.
	void AddPoint(DZS_Instance inst, float x, float z)
	{
		DZS_SpawnPoint s = Selected();
		if (!s)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_SpawnsModel.OP, 1);
		map<string, string> f = new map<string, string>();
		f.Set("event", s.m_Event);
		f.Set("origin", "own");
		f.Set("x", NumText(x));
		f.Set("z", NumText(z));
		f.Set("a", NumText(s.m_A));
		string id = b.Create(DZS_SpawnsModel.POINT, DZS_PROJECT_ID, DZS_END, f);
		b.Commit("Spawnpunkt hinzufügen", "Add spawn point");
		m_Sel = "e:" + id;
		DZS_Log.Info("spawns: added a point to " + s.m_Event + " at " + x + " " + z);
	}

	void StartAdding()
	{
		if (Selected())
			m_Adding = true;
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
		b.Create(DZS_SpawnsModel.POINT, DZS_PROJECT_ID, DZS_END, pf);
		DZS_SubmitResult r = b.Commit("Festes Fahrzeug: " + cls, "Fixed vehicle: " + cls);
		DZS_Log.Info("spawns: fixed vehicle " + cls + " as event " + name + " at " + pos.ToString() + " ok " + r.Ok());
		m_Kernel.GetScreen().ShowMessage("spawns.fixed", "info", DZS_Texts.Pick("Festes Fahrzeug angelegt (eigenes Event „", "Fixed vehicle added (own event '") + name + DZS_Texts.Pick("“) – sichtbar in der Ebene Spawns", "') – shown in the Spawns layer"), 6.0);
	}
}

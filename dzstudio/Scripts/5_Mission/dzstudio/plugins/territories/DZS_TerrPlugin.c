// The Territories layer: zones of herds, ambient animals and infected as circles (one colour per kind). Select a zone
// to see its kind (count on the map, for infected the numbers per zone), drag to move, drag the edge to resize, Del
// removes. Placing an animal or an infected from the catalog in the Objects layer makes a zone of its kind there.
class DZS_TerrActions : DZS_ActionHandler
{
	DZS_TerrPlugin m_P;

	override bool IsActionChecked(string actionId) { return actionId == "territories.layer" && m_P.m_On; }

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
		if (actionId == "territories.layer")
			m_P.SetOn(true);
	}
}

class DZS_TerrOverride : DZS_ActionOverride
{
	DZS_TerrPlugin m_P;

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

class DZS_TerrEsc : DZS_EscHandler
{
	DZS_TerrPlugin m_P;

	override bool IsEscActive() { return m_P.m_On; }

	override void OnEsc()
	{
		if (m_P.m_Sel != "")
			m_P.Select("");
		else
			m_P.SetOn(false);
	}
}

class DZS_TerrLayer : DZS_Layer
{
	DZS_TerrPlugin m_P;

	override string Id() { return "territories"; }
	override bool IsOn() { return m_P.m_On; }
	override void TurnOff() { m_P.SetOn(false); }
}

class DZS_TerrDivert : DZS_PlaceDivert
{
	DZS_TerrPlugin m_P;

	override bool Place(DZS_Instance inst, string name, vector pos, vector ypr)
	{
		if (m_P.ZoneForClass(inst, name, pos))
			return true;
		if (!GetGame().IsKindOf(name, "DZ_LightAI"))
			return false;
		// an animal or infected is never a spawner object; without a territory of its kind nothing is placed
		DZS_Log.Info("territories: no zone kind spawns " + name + ", nothing placed");
		m_P.Kernel().GetScreen().ShowMessage("territories.class", "warning", DZS_Texts.Pick("Kein Gebiet der Basis lässt " + name + " spawnen — nichts platziert", "No territory of the base spawns " + name + " — nothing placed"), 6.0);
		return true;
	}
}

[DZS_RegisterPlugin(DZS_TerrPlugin)]
class DZS_TerrPlugin : DZS_Plugin
{
	static const string TOOL = "territories";
	static const string PANEL = "territories.zone";

	bool m_On;
	string m_Sel;
	string m_Hover;
	int m_Gen;
	ref array<ref DZS_TerrZone> m_Zones = new array<ref DZS_TerrZone>();
	ref map<string, ref DZS_TerrKind> m_Kinds = new map<string, ref DZS_TerrKind>();
	protected bool m_World;
	protected ref DZS_TerrActions m_Actions;
	protected ref DZS_TerrOverride m_Override;
	protected ref DZS_TerrEsc m_Esc;
	protected ref DZS_TerrLayer m_Layer;
	protected ref DZS_TerrDivert m_Divert;
	protected ref DZS_TerrPanel m_Panel;
	protected ref DZS_TerrBase m_Base;
	protected string m_BaseFor;
	protected int m_ViewVer = -1;
	protected int m_StateVer;
	protected CanvasWidget m_Canvas;
	protected bool m_LmbWas;
	protected bool m_Press;
	protected float m_PressX;
	protected float m_PressY;
	protected vector m_PressGround;
	protected string m_Drag;

	override string GetName() { return "territories"; }

	DZS_Kernel Kernel() { return m_Kernel; }

	override void OnRegister()
	{
		DZS_TypeRegistry types = m_Kernel.GetTypes();
		types.RegisterOperationType("territories", DZS_TerrModel.OP, 1);
		array<string> parents = {DZS_KIND_PROJECT};
		types.RegisterElementKind("territories", DZS_TerrModel.ZONE, 1, parents);
		types.RegisterElementKind("territories", DZS_TerrModel.TERR, 1, parents);
		types.RegisterElementKind("territories", DZS_TerrModel.CHANGE, 1, parents);
		types.RegisterElementKind("territories", "territories:nominal", 1, parents);
		DZS_TerrSection section = new DZS_TerrSection();
		DZS_ProjectSections.Add(section);
		m_Actions = new DZS_TerrActions();
		m_Actions.m_P = this;
		m_Override = new DZS_TerrOverride();
		m_Override.m_P = this;
		m_Layer = new DZS_TerrLayer();
		m_Layer.m_P = this;
		DZS_Layers.Add(m_Layer);
		m_Divert = new DZS_TerrDivert();
		m_Divert.m_P = this;
		DZS_PlaceDivert.Add(m_Divert);
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterAction("territories", "territories.layer", "Gebiete", "Territories", null, m_Actions);
		ui.RegisterTool("territories", "territories.layer", "Gebiete", "Territories", "switches", "paw-print", -97);
		ui.SetToolLabelled("territories.layer");
		ui.SetToolTip("territories.layer", "Ebene Gebiete: Tierherden und Zombiezonen", "Territories layer: animal herds and infected zones", "territories.layer");
		m_Panel = new DZS_TerrPanel(this);
		ui.RegisterPanel("territories", PANEL, "Gebiet", "Territory", "right", "paw-print", m_Panel, 17);
		ui.SetPanelShown(PANEL, false);
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		if (!m_World)
			return;
		m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", m_Kernel.GetScreen().GetSceneLayer()));
		m_Esc = new DZS_TerrEsc();
		m_Esc.m_P = this;
		m_Kernel.GetScreen().AddEscHandler(m_Esc, 85);
	}

	override void OnSessionEnd()
	{
		SetOn(false);
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

	protected void RestoreFromProject(DZS_Instance inst)
	{
		if (DZS_TerrModel.Of(inst, DZS_TerrModel.ZONE).Count() > 0 || DZS_TerrModel.Of(inst, DZS_TerrModel.CHANGE).Count() > 0 || DZS_TerrModel.Of(inst, DZS_TerrModel.TERR).Count() > 0)
			return;
		int v;
		string entries;
		if (!inst.GetCompanion(DZS_PROJECT_ID, DZS_LocalStore.GROUP_PLUGINS, v, entries))
			return;
		string text = DZS_ProjectSections.Stored(entries, DZS_TerrModel.PLUGIN);
		if (text == "")
			return;
		string err;
		DZS_Json frag = DZS_Json.ParseText(text, err);
		if (!frag)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_TerrModel.OP, 1);
		DZS_TerrModel.Restore(b, frag);
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		DZS_Log.Info("territories: restored from the project file");
	}

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
			m_Base = DZS_TerrBase.Load(dir);
			m_BaseFor = dir;
			m_ViewVer = -1;
			DZS_Log.Info("territories: base " + dir + ", " + m_Base.m_Kinds.Count() + " kinds, " + m_Base.m_Zones.Count() + " zones");
		}
		if (m_ViewVer == m_StateVer)
			return;
		m_ViewVer = m_StateVer;
		m_Base.Effective(inst, m_Zones, m_Kinds);
		m_Gen++;
		if (m_Sel != "" && !ZoneOf(m_Sel))
			m_Sel = "";
	}

	DZS_TerrZone ZoneOf(string key)
	{
		foreach (DZS_TerrZone z : m_Zones)
		{
			if (z.Key() == key)
				return z;
		}
		return null;
	}

	DZS_TerrZone Selected()
	{
		if (m_Sel == "")
			return null;
		return ZoneOf(m_Sel);
	}

	void Select(string key)
	{
		m_Sel = key;
		m_Gen++;
	}

	void SetOn(bool on)
	{
		if (on == m_On)
			return;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		if (on)
		{
			DZS_Layers.Only("territories");
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
			m_ViewVer = -1;
			m_Kernel.SetSceneTool(TOOL);
			m_Kernel.ClaimSceneState(true);
			ui.SetOverride(m_Override);
			ui.SetPanelShown(PANEL, true);
			ui.SetPanelShown("properties.panel", false);
			DZS_Instance inst = m_Kernel.GetShown();
			if (inst)
				View(inst);
			DZS_Log.Info("territories: layer on");
		}
		else
		{
			m_On = false;
			m_Drag = "";
			m_Kernel.SetSceneTool("");
			m_Kernel.ClaimSceneState(false);
			ui.SetSceneState("none");
			ui.SetOverride(null);
			ui.SetPanelShown(PANEL, false);
			ui.SetPanelShown("properties.panel", true);
			m_Kernel.GetScreen().SetHint("territories", 30, "");
			if (m_Canvas)
				m_Canvas.Clear();
			DZS_Log.Info("territories: layer off");
		}
		m_Gen++;
	}

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
		Draw();
		string hint;
		if (m_Sel == "")
			hint = DZS_Texts.Pick("Zone anklicken · Tiere und Zombies aus dem Katalog in der Ebene Objekte setzen", "Click a zone · place animals and infected from the catalog in the Objects layer");
		else
			hint = DZS_Texts.Pick("Ziehen verschiebt · Rand ziehen ändert die Größe · Entf entfernt", "Drag to move · drag the edge to resize · Del removes");
		scr.SetHint("territories", 30, hint);
	}

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

	protected bool PointerGround(DZS_Screen scr, out vector hit)
	{
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		if (!scr.IsCursorVisible())
			dir = GetGame().GetCurrentCameraDirection();
		return RayGround(from, dir, hit);
	}

	protected string PickAt(vector g)
	{
		string best = "";
		float bestR = 1000000;
		foreach (DZS_TerrZone z : m_Zones)
		{
			if (z.m_Gone)
				continue;
			float d = Math.Sqrt((g[0] - z.m_X) * (g[0] - z.m_X) + (g[2] - z.m_Z) * (g[2] - z.m_Z));
			if (d <= z.m_R && z.m_R < bestR)
			{
				bestR = z.m_R;
				best = z.Key();
			}
		}
		return best;
	}

	protected bool OnEdge(DZS_TerrZone z, vector g)
	{
		float d = Math.Sqrt((g[0] - z.m_X) * (g[0] - z.m_X) + (g[2] - z.m_Z) * (g[2] - z.m_Z));
		return Math.AbsFloat(d - z.m_R) <= Math.Max(2, z.m_R * 0.06);
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
		vector g;
		bool hasG = over && PointerGround(scr, g);
		m_Hover = "";
		if (hasG && m_Drag == "")
			m_Hover = PickAt(g);
		if (lDown && hasG && !scr.IsMouseSwallowed())
		{
			m_Press = true;
			m_PressX = mx;
			m_PressY = my;
			m_PressGround = g;
			DZS_TerrZone sel = Selected();
			if (sel && OnEdge(sel, g))
				m_Drag = "resize";
			else
			{
				Select(m_Hover);
				m_Drag = "";
			}
		}
		if (m_Press && lmb && m_Drag == "" && m_Sel != "" && (Math.AbsFloat(mx - m_PressX) > 4 || Math.AbsFloat(my - m_PressY) > 4))
			m_Drag = "move";
		if (!lmb && m_Press)
		{
			m_Press = false;
			string what = m_Drag;
			m_Drag = "";
			if (!hasG)
				return;
			DZS_TerrZone z = Selected();
			if (!z)
				return;
			if (what == "move")
				Change(inst, z, z.m_X + g[0] - m_PressGround[0], z.m_Z + g[2] - m_PressGround[2], z.m_R, z.m_Dmin, z.m_Dmax);
			else if (what == "resize")
			{
				float nr = Math.Max(5, Math.Sqrt((g[0] - z.m_X) * (g[0] - z.m_X) + (g[2] - z.m_Z) * (g[2] - z.m_Z)));
				Change(inst, z, z.m_X, z.m_Z, nr, z.m_Dmin, z.m_Dmax);
			}
		}
	}

	static int ColorOf(string kind)
	{
		if (kind.IndexOf("infected:") == 0)
			return ARGB(255, 248, 113, 113);
		if (kind.IndexOf("ambient:") == 0)
			return ARGB(255, 250, 204, 21);
		if (kind == "herd:Wolf")
			return ARGB(255, 148, 163, 184);
		if (kind == "herd:Bear")
			return ARGB(255, 180, 120, 70);
		return ARGB(255, 96, 165, 250);
	}

	protected void Ring(float x, float z, float r, int color, float width)
	{
		int segs = 48;
		for (int i = 0; i < segs; i++)
		{
			float a0 = Math.PI2 * i / segs;
			float a1 = Math.PI2 * (i + 1) / segs;
			float x0 = x + Math.Cos(a0) * r;
			float z0 = z + Math.Sin(a0) * r;
			float x1 = x + Math.Cos(a1) * r;
			float z1 = z + Math.Sin(a1) * r;
			vector w0 = Vector(x0, GetGame().SurfaceY(x0, z0) + 0.5, z0);
			vector w1 = Vector(x1, GetGame().SurfaceY(x1, z1) + 0.5, z1);
			vector s0 = DZS_PlaceMath.Project(w0);
			vector s1 = DZS_PlaceMath.Project(w1);
			if (s0[2] <= 0.5 || s1[2] <= 0.5)
				continue;
			DZS_Clip.Line(m_Canvas, s0[0], s0[1], s1[0], s1[1], width, color);
		}
	}

	protected void Draw()
	{
		if (!m_Canvas)
			return;
		m_Canvas.Clear();
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Canvas, 0, 0, sw, sh);
		vector cam = GetGame().GetCurrentCameraPosition();
		DZS_Screen scr = m_Kernel.GetScreen();
		vector g;
		bool hasG = PointerGround(scr, g);
		foreach (DZS_TerrZone z : m_Zones)
		{
			if (z.m_Gone)
				continue;
			if (vector.Distance(Vector(z.m_X, cam[1], z.m_Z), cam) > 1500 + z.m_R)
				continue;
			float x = z.m_X;
			float zz = z.m_Z;
			float r = Math.Max(z.m_R, 1);
			string key = z.Key();
			if (key == m_Sel && hasG && m_Drag == "move")
			{
				x += g[0] - m_PressGround[0];
				zz += g[2] - m_PressGround[2];
			}
			if (key == m_Sel && hasG && m_Drag == "resize")
				r = Math.Max(5, Math.Sqrt((g[0] - x) * (g[0] - x) + (g[2] - zz) * (g[2] - zz)));
			float w = DZS_Scale.Px(2);
			if (key == m_Sel)
				w = DZS_Scale.Px(4);
			else if (key == m_Hover)
				w = DZS_Scale.Px(3);
			Ring(x, zz, r, ColorOf(z.m_Kind), w);
		}
	}

	// ---- changes ----

	protected static string NumText(float f)
	{
		return DZS_PatchNum.Canon(f.ToString());
	}

	void Change(DZS_Instance inst, DZS_TerrZone z, float x, float zz, float r, string dmin, string dmax)
	{
		DZS_OpBuilder b = inst.BeginOperation(DZS_TerrModel.OP, 1);
		if (z.m_Own)
		{
			b.SetField(z.m_Elem, "x", NumText(x));
			b.SetField(z.m_Elem, "z", NumText(zz));
			b.SetField(z.m_Elem, "r", NumText(r));
			b.SetField(z.m_Elem, "dmin", dmin);
			b.SetField(z.m_Elem, "dmax", dmax);
		}
		else
		{
			map<string, string> f = new map<string, string>();
			f.Set("file", z.m_File);
			f.Set("name", z.m_Name);
			f.Set("bx", z.m_BX);
			f.Set("bz", z.m_BZ);
			f.Set("br", z.m_BR);
			f.Set("bdmin", z.m_BDmin);
			f.Set("bdmax", z.m_BDmax);
			string xs = NumText(x);
			string zs = NumText(zz);
			if (Math.AbsFloat(x - z.m_BX.ToFloat()) < 0.01)
				xs = z.m_BX;
			if (Math.AbsFloat(zz - z.m_BZ.ToFloat()) < 0.01)
				zs = z.m_BZ;
			string rs = NumText(r);
			if (Math.AbsFloat(r - z.m_BR.ToFloat()) < 0.01)
				rs = z.m_BR;
			f.Set("x", xs);
			f.Set("z", zs);
			f.Set("r", rs);
			f.Set("dmin", dmin);
			f.Set("dmax", dmax);
			if (z.m_Elem != "")
			{
				foreach (string k, string v : f)
					b.SetField(z.m_Elem, k, v);
			}
			else
				b.Create(DZS_TerrModel.CHANGE, DZS_PROJECT_ID, DZS_END, f);
		}
		b.Commit("Zone ändern", "Change zone");
		DZS_Log.Info("territories: changed " + z.m_Name + " in " + z.m_File);
	}

	void RemoveSelected()
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_TerrZone z = Selected();
		if (!inst || !z)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_TerrModel.OP, 1);
		if (z.m_Own)
			b.Delete(z.m_Elem);
		else if (z.m_Elem != "")
			b.SetField(z.m_Elem, "gone", "1");
		else
		{
			map<string, string> f = new map<string, string>();
			f.Set("file", z.m_File);
			f.Set("name", z.m_Name);
			f.Set("bx", z.m_BX);
			f.Set("bz", z.m_BZ);
			f.Set("gone", "1");
			b.Create(DZS_TerrModel.CHANGE, DZS_PROJECT_ID, DZS_END, f);
		}
		b.Commit("Zone entfernen", "Remove zone");
		m_Sel = "";
		DZS_Log.Info("territories: removed " + z.m_Name + " in " + z.m_File);
	}

	void SetZoneValue(string field, string value)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_TerrZone z = Selected();
		if (!inst || !z)
			return;
		float r = z.m_R;
		string dmin = z.m_Dmin;
		string dmax = z.m_Dmax;
		if (field == "r")
			r = value.ToFloat();
		else if (field == "dmin")
			dmin = value;
		else if (field == "dmax")
			dmax = value;
		Change(inst, z, z.m_X, z.m_Z, r, dmin, dmax);
	}

	// The count on the map of the selected zone's kind (its event's nominal: herds of that kind, or infected events).
	void SetKindCount(string value)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_TerrZone z = Selected();
		if (!inst || !z)
			return;
		DZS_TerrKind k = m_Kinds.Get(z.m_Kind);
		if (!k || k.m_Event == "")
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_TerrModel.OP, 1);
		string found = "";
		array<string> noms = DZS_TerrModel.Of(inst, "territories:nominal");
		foreach (string nid : noms)
		{
			if (DZS_TerrModel.Field(inst, nid, "event") == k.m_Event)
				found = nid;
		}
		if (found != "" && value == k.m_BaseNominal)
			b.Delete(found);
		else if (found != "")
			b.SetField(found, "to", value);
		else
		{
			map<string, string> f = new map<string, string>();
			f.Set("event", k.m_Event);
			f.Set("from", k.m_BaseNominal);
			f.Set("to", value);
			b.Create("territories:nominal", DZS_PROJECT_ID, DZS_END, f);
		}
		b.Commit("Anzahl ändern", "Change count");
	}

	// Placing an animal or an infected class: a zone of its kind at that spot (3A). False when the class has no kind.
	bool ZoneForClass(DZS_Instance inst, string cls, vector pos)
	{
		if (!m_Base || m_BaseFor != BaseDir())
		{
			m_Base = DZS_TerrBase.Load(BaseDir());
			m_BaseFor = BaseDir();
		}
		string kid = m_Base.KindOfClass(cls);
		if (kid == "")
			return false;
		DZS_TerrKind k = m_Base.m_Kinds.Get(kid);
		DZS_OpBuilder b = inst.BeginOperation(DZS_TerrModel.OP, 1);
		string x = NumText(pos[0]);
		string z = NumText(pos[2]);
		if (kid.IndexOf("infected:") == 0)
		{
			map<string, string> zf = new map<string, string>();
			zf.Set("name", k.m_Event);
			zf.Set("dmin", "1");
			zf.Set("dmax", "3");
			zf.Set("x", x);
			zf.Set("z", z);
			zf.Set("r", "50");
			if (k.m_Color != "")
			{
				zf.Set("file", k.m_File);
				zf.Set("color", k.m_Color);
			}
			else
			{
				map<string, string> tf = new map<string, string>();
				tf.Set("file", k.m_File);
				tf.Set("color", "4278190335");
				string tid = b.Create(DZS_TerrModel.TERR, DZS_PROJECT_ID, DZS_END, tf);
				zf.Set("territory", tid);
			}
			b.Create(DZS_TerrModel.ZONE, DZS_PROJECT_ID, DZS_END, zf);
		}
		else if (kid.IndexOf("herd:") == 0)
		{
			map<string, string> hf = new map<string, string>();
			hf.Set("file", k.m_File);
			hf.Set("color", "4291611852");
			string hid = b.Create(DZS_TerrModel.TERR, DZS_PROJECT_ID, DZS_END, hf);
			array<string> names = {"Graze", "Rest"};
			array<string> radii = {"150", "80"};
			for (int i = 0; i < 2; i++)
			{
				map<string, string> gz = new map<string, string>();
				gz.Set("territory", hid);
				gz.Set("name", names[i]);
				gz.Set("x", x);
				gz.Set("z", z);
				gz.Set("r", radii[i]);
				b.Create(DZS_TerrModel.ZONE, DZS_PROJECT_ID, DZS_END, gz);
			}
		}
		else
		{
			// ambient animals: one more zone in the file's territory, named like its other zones
			string color = "";
			string zname = "";
			foreach (DZS_TerrZone bz : m_Base.m_Zones)
			{
				if (bz.m_File == k.m_File && color == "")
				{
					color = bz.m_Color;
					zname = bz.m_Name;
				}
			}
			if (color == "")
				return false;
			map<string, string> af = new map<string, string>();
			af.Set("file", k.m_File);
			af.Set("color", color);
			af.Set("name", zname);
			af.Set("dmax", "2");
			af.Set("x", x);
			af.Set("z", z);
			af.Set("r", "100");
			b.Create(DZS_TerrModel.ZONE, DZS_PROJECT_ID, DZS_END, af);
		}
		b.Commit("Gebiet für " + cls, "Territory for " + cls);
		m_Kernel.GetScreen().ShowMessage("territories.placed", "info", DZS_Texts.Pick("Gebiet angelegt (", "Territory added (") + k.m_Label + DZS_Texts.Pick(") – sichtbar in der Ebene Gebiete", ") – shown in the Territories layer"), 6.0);
		DZS_Log.Info("territories: " + cls + " placed as a zone of " + kid + " at " + x + " " + z);
		return true;
	}
}

class DZS_TerrPanel : DZS_PanelContent
{
	protected DZS_TerrPlugin m_P;
	protected Widget m_Parent;
	protected Widget m_Root;
	protected int m_BuiltGen = -1;
	protected float m_Width;
	protected float m_H;
	protected float m_Y;
	protected ref array<ref DZS_Button> m_Buttons = new array<ref DZS_Button>();
	protected ref array<ref DZS_NumberField> m_Fields = new array<ref DZS_NumberField>();

	void DZS_TerrPanel(DZS_TerrPlugin p)
	{
		m_P = p;
	}

	override void Build(Widget parent)
	{
		m_Parent = parent;
	}

	override float Layout(float width)
	{
		bool changed = Math.AbsFloat(width - m_Width) > 0.5;
		m_Width = width;
		Rebuild(changed);
		return m_H;
	}

	override void Update(float dt)
	{
		if (m_BuiltGen != m_P.m_Gen)
			Rebuild(false);
		foreach (DZS_NumberField f : m_Fields)
			f.Update(dt);
	}

	protected bool AnyFocused()
	{
		foreach (DZS_NumberField f : m_Fields)
		{
			if (f.IsFocused() || f.IsDragging())
				return true;
		}
		return false;
	}

	protected void Clear()
	{
		foreach (DZS_Button b : m_Buttons)
			b.Destroy();
		foreach (DZS_NumberField f : m_Fields)
			f.Destroy();
		m_Buttons.Clear();
		m_Fields.Clear();
		if (m_Root)
			m_Root.Unlink();
		m_Root = null;
	}

	protected float Pad() { return DZS_Scale.Px(12); }

	protected void Line(string text, string color, float size, float h)
	{
		TextWidget t = DZS_W.Text(m_Root, DZS_Font.READ, size, DZS_Color.Get(color));
		t.SetText(DZS_W.Literal(text));
		DZS_W.Place(t, Pad(), m_Y, m_Width - 2 * Pad(), DZS_Scale.Px(h));
		m_Y += DZS_Scale.Px(h);
	}

	protected void Field(string label, string tag, string value)
	{
		float rowH = DZS_Scale.Px(26);
		TextWidget l = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("text"));
		l.SetText(label);
		DZS_W.Place(l, Pad(), m_Y, m_Width * 0.55, rowH);
		DZS_NumberField f = new DZS_NumberField("territories." + tag, "", "textMuted", 70);
		f.Build(m_Root);
		f.m_Tag = tag;
		f.m_OnCommit.Insert(OnValue);
		f.ShowValue(value);
		f.Place(m_Width - Pad() - f.GetWidth(), m_Y, rowH);
		m_Fields.Insert(f);
		m_Y += rowH + DZS_Scale.Px(4);
	}

	protected void Rebuild(bool force)
	{
		if (!m_Parent || m_Width <= 0)
			return;
		if (!force && m_BuiltGen == m_P.m_Gen)
			return;
		if (AnyFocused() && !force)
			return;
		m_BuiltGen = m_P.m_Gen;
		Clear();
		m_Root = DZS_W.Frame(m_Parent);
		m_Y = DZS_Scale.Px(8);
		DZS_TerrZone z = m_P.Selected();
		if (!z)
		{
			int n = 0;
			foreach (DZS_TerrZone c : m_P.m_Zones)
			{
				if (!c.m_Gone)
					n++;
			}
			Line(DZS_Texts.Pick("Tierherden und Zombiezonen", "Animal herds and infected zones"), "textMuted", 12, 18);
			Line(m_P.m_Kinds.Count().ToString() + DZS_Texts.Pick(" Arten · ", " kinds · ") + n + DZS_Texts.Pick(" Zonen", " zones"), "text", 14, 22);
			Line(DZS_Texts.Pick("Rot: Zombies · Blau/Grau/Braun: Herden · Gelb: Kleintiere", "Red: infected · blue/grey/brown: herds · yellow: small animals"), "textQuiet", 13, 20);
		}
		else
		{
			DZS_TerrKind k = m_P.m_Kinds.Get(z.m_Kind);
			string kindLabel = z.m_Kind;
			if (k)
				kindLabel = k.m_Label;
			Line(DZS_Texts.Pick("Art", "Kind"), "textMuted", 12, 18);
			Line(kindLabel, "text", 15, 24);
			string where = z.m_Name + " · " + z.m_File;
			if (z.m_Own)
				where += DZS_Texts.Pick(" · eigene Zone", " · own zone");
			else if (z.m_Changed)
				where += DZS_Texts.Pick(" · geändert", " · changed");
			Line(where, "textQuiet", 13, 20);
			m_Y += DZS_Scale.Px(6);
			if (k && k.m_Nominal != "")
			{
				string countLabel = DZS_Texts.Pick("Anzahl auf der Karte", "Count on the map");
				if (z.m_Kind.IndexOf("herd:") == 0)
					countLabel = DZS_Texts.Pick("Herden auf der Karte", "Herds on the map");
				Field(countLabel, "count", k.m_Nominal);
			}
			Field(DZS_Texts.Pick("Radius (m)", "Radius (m)"), "r", DZS_PatchNum.Canon(z.m_R.ToString()));
			if (z.m_Kind.IndexOf("infected:") == 0)
			{
				Field(DZS_Texts.Pick("Zombies mindestens", "Infected at least"), "dmin", z.m_Dmin);
				Field(DZS_Texts.Pick("Zombies höchstens", "Infected at most"), "dmax", z.m_Dmax);
			}
			m_Y += DZS_Scale.Px(6);
			DZS_Button rem = new DZS_Button(m_Root, "danger", DZS_Texts.Pick("Zone entfernen", "Remove zone"), "trash-2");
			rem.Place(Pad(), m_Y);
			rem.m_OnClick.Insert(OnRemove);
			m_Buttons.Insert(rem);
			m_Y += DZS_Scale.Px(32);
		}
		m_H = m_Y + DZS_Scale.Px(8);
		DZS_W.Place(m_Root, 0, 0, m_Width, m_H);
	}

	void OnValue(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		string t = typed;
		t.Trim();
		float num;
		r.m_Ok = DZS_Num.Parse(t, num) && num >= 0;
		if (!r.m_Ok)
			return;
		if (f.m_Tag == "count")
		{
			int c = Math.Round(num);
			m_P.SetKindCount(c.ToString());
		}
		else if (f.m_Tag == "r")
			m_P.SetZoneValue("r", DZS_PatchNum.Canon(num.ToString()));
		else
		{
			int d = Math.Round(num);
			m_P.SetZoneValue(f.m_Tag, d.ToString());
		}
	}

	void OnRemove(DZS_Button b)
	{
		m_P.RemoveSelected();
	}
}

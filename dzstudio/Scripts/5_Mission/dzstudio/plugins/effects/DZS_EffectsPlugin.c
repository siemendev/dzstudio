// The Effects layer: effect areas of the base and the project as circles on the ground. Pick a preset in the panel,
// then press in the scene and drag to the radius; drag a circle to move it, its edge to resize it; Del removes it.
class DZS_EffectsActions : DZS_ActionHandler
{
	DZS_EffectsPlugin m_P;

	override bool IsActionChecked(string actionId) { return actionId == "effects.layer" && m_P.m_On; }

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
		if (actionId == "effects.layer")
			m_P.SetOn(true);
	}
}

class DZS_EffectsOverride : DZS_ActionOverride
{
	DZS_EffectsPlugin m_P;

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

class DZS_EffectsEsc : DZS_EscHandler
{
	DZS_EffectsPlugin m_P;

	override bool IsEscActive() { return m_P.m_On; }

	override void OnEsc()
	{
		if (m_P.m_AddPreset != "")
		{
			m_P.m_AddPreset = "";
			m_P.m_Gen++;
		}
		else if (m_P.m_Sel != "")
			m_P.Select("");
		else
			m_P.SetOn(false);
	}
}

class DZS_EffectsLayer : DZS_Layer
{
	DZS_EffectsPlugin m_P;

	override string Id() { return "effects"; }
	override bool IsOn() { return m_P.m_On; }
	override void TurnOff() { m_P.SetOn(false); }
}

[DZS_RegisterPlugin(DZS_EffectsPlugin)]
class DZS_EffectsPlugin : DZS_Plugin
{
	static const string TOOL = "effects";
	static const string PANEL = "effects.area";

	bool m_On;
	string m_Sel;
	string m_Hover;
	string m_AddPreset;
	int m_Gen;
	ref array<ref DZS_FxArea> m_Areas = new array<ref DZS_FxArea>();
	protected bool m_World;
	protected ref DZS_EffectsActions m_Actions;
	protected ref DZS_EffectsOverride m_Override;
	protected ref DZS_EffectsEsc m_Esc;
	protected ref DZS_EffectsLayer m_Layer;
	protected ref DZS_EffectsPanel m_Panel;
	protected ref DZS_EffectsBase m_Base;
	protected string m_BaseFor;
	protected int m_ViewVer = -1;
	protected int m_StateVer;
	protected CanvasWidget m_Canvas;
	protected bool m_LmbWas;
	protected bool m_Press;
	protected float m_PressX;
	protected float m_PressY;
	protected vector m_PressGround;
	// "move", "resize", "create" while the left button is held
	protected string m_Drag;

	override string GetName() { return "effects"; }

	DZS_Kernel Kernel() { return m_Kernel; }

	override void OnRegister()
	{
		DZS_TypeRegistry types = m_Kernel.GetTypes();
		types.RegisterOperationType("effects", DZS_EffectsModel.OP, 1);
		array<string> parents = {DZS_KIND_PROJECT};
		types.RegisterElementKind("effects", DZS_EffectsModel.AREA, 1, parents);
		types.RegisterElementKind("effects", DZS_EffectsModel.CHANGE, 1, parents);
		DZS_EffectsSection section = new DZS_EffectsSection();
		DZS_ProjectSections.Add(section);
		m_Actions = new DZS_EffectsActions();
		m_Actions.m_P = this;
		m_Override = new DZS_EffectsOverride();
		m_Override.m_P = this;
		m_Layer = new DZS_EffectsLayer();
		m_Layer.m_P = this;
		DZS_Layers.Add(m_Layer);
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterAction("effects", "effects.layer", "Effekte", "Effects", null, m_Actions);
		ui.RegisterTool("effects", "effects.layer", "Effekte", "Effects", "switches", "flame", -98);
		ui.SetToolLabelled("effects.layer");
		ui.SetToolTip("effects.layer", "Ebene Effekte: Gas, Geysire, Herzchen, Rauch", "Effects layer: gas, geysers, hearts, smoke", "effects.layer");
		m_Panel = new DZS_EffectsPanel(this);
		ui.RegisterPanel("effects", PANEL, "Effekt", "Effect", "right", "flame", m_Panel, 16);
		ui.SetPanelShown(PANEL, false);
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		if (!m_World)
			return;
		m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", m_Kernel.GetScreen().GetSceneLayer()));
		m_Esc = new DZS_EffectsEsc();
		m_Esc.m_P = this;
		m_Kernel.GetScreen().AddEscHandler(m_Esc, 86);
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
		if (DZS_EffectsModel.Of(inst, DZS_EffectsModel.AREA).Count() > 0 || DZS_EffectsModel.Of(inst, DZS_EffectsModel.CHANGE).Count() > 0)
			return;
		int v;
		string entries;
		if (!inst.GetCompanion(DZS_PROJECT_ID, DZS_LocalStore.GROUP_PLUGINS, v, entries))
			return;
		string text = DZS_ProjectSections.Stored(entries, DZS_EffectsModel.PLUGIN);
		if (text == "")
			return;
		string err;
		DZS_Json frag = DZS_Json.ParseText(text, err);
		if (!frag)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_EffectsModel.OP, 1);
		DZS_EffectsModel.Restore(b, frag);
		DZS_Operation op = b.GetOperation();
		inst.SubmitSystem(op);
		DZS_Log.Info("effects: restored from the project file");
	}

	protected string BaseDir()
	{
		DZS_PatchPlugin pp = DZS_PatchPlugin.Cast(m_Kernel.GetPlugin("patcher"));
		DZS_ProjectsPlugin pr = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pr || !pr.m_Open)
			return "";
		DZS_PatchApply a = pp.Applier();
		string mapKey = a.MapKey();
		string key = a.Remembered(pr.m_Open.m_Folder);
		if (key.IndexOf("mission:") == 0)
			return DZS_PatchApply.MISSIONS + "\\" + key.Substring(8, key.Length() - 8);
		if (DZS_LootVanilla.HaveMission(mapKey))
			return DZS_LootVanilla.MissionDir(mapKey);
		return "";
	}

	void View(DZS_Instance inst)
	{
		string dir = BaseDir();
		if (!m_Base || dir != m_BaseFor)
		{
			m_Base = DZS_EffectsBase.Load(dir);
			m_BaseFor = dir;
			m_ViewVer = -1;
			DZS_Log.Info("effects: base " + dir + ", " + m_Base.m_Areas.Count() + " areas");
		}
		if (m_ViewVer == m_StateVer)
			return;
		m_ViewVer = m_StateVer;
		m_Base.Effective(inst, m_Areas);
		m_Gen++;
		if (m_Sel != "" && !AreaOf(m_Sel))
			m_Sel = "";
	}

	DZS_FxArea AreaOf(string key)
	{
		foreach (DZS_FxArea a : m_Areas)
		{
			if (a.Key() == key)
				return a;
		}
		return null;
	}

	DZS_FxArea Selected()
	{
		if (m_Sel == "")
			return null;
		return AreaOf(m_Sel);
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
			DZS_Layers.Only("effects");
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
			m_AddPreset = "";
			m_ViewVer = -1;
			m_Kernel.SetSceneTool(TOOL);
			m_Kernel.ClaimSceneState(true);
			ui.SetOverride(m_Override);
			ui.SetPanelShown(PANEL, true);
			ui.SetPanelShown("properties.panel", false);
			DZS_Instance inst = m_Kernel.GetShown();
			if (inst)
				View(inst);
			DZS_Log.Info("effects: layer on");
		}
		else
		{
			m_On = false;
			m_Drag = "";
			m_AddPreset = "";
			m_Kernel.SetSceneTool("");
			m_Kernel.ClaimSceneState(false);
			ui.SetSceneState("none");
			ui.SetOverride(null);
			ui.SetPanelShown(PANEL, false);
			ui.SetPanelShown("properties.panel", true);
			m_Kernel.GetScreen().SetHint("effects", 30, "");
			if (m_Canvas)
				m_Canvas.Clear();
			DZS_Log.Info("effects: layer off");
		}
		m_Gen++;
	}

	void StartAdding(string preset)
	{
		m_AddPreset = preset;
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
		Input(inst, scr);
		Draw();
		string hint;
		if (m_AddPreset != "")
			hint = DZS_Texts.Pick("Mitte drücken, auf den Radius ziehen, loslassen · Esc bricht ab", "Press at the centre, drag to the radius, release · Esc cancels");
		else if (m_Sel == "")
			hint = DZS_Texts.Pick("Zone anklicken · neue Zone: Vorlage im Panel wählen", "Click an area · new area: pick a preset in the panel");
		else
			hint = DZS_Texts.Pick("Ziehen verschiebt · Rand ziehen ändert die Größe · Entf entfernt", "Drag to move · drag the edge to resize · Del removes");
		scr.SetHint("effects", 30, hint);
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

	// The smallest area the ground point lies in.
	protected string PickAt(vector g)
	{
		string best = "";
		float bestR = 1000000;
		foreach (DZS_FxArea a : m_Areas)
		{
			if (a.m_Gone)
				continue;
			float d = Math.Sqrt((g[0] - a.m_X) * (g[0] - a.m_X) + (g[2] - a.m_Z) * (g[2] - a.m_Z));
			if (d <= Math.Max(a.m_R, 2) && a.m_R < bestR)
			{
				bestR = a.m_R;
				best = a.Key();
			}
		}
		return best;
	}

	protected bool OnEdge(DZS_FxArea a, vector g)
	{
		float d = Math.Sqrt((g[0] - a.m_X) * (g[0] - a.m_X) + (g[2] - a.m_Z) * (g[2] - a.m_Z));
		return Math.AbsFloat(d - a.m_R) <= Math.Max(1.5, a.m_R * 0.08);
	}

	protected void Input(DZS_Instance inst, DZS_Screen scr)
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
			if (m_AddPreset != "")
				m_Drag = "create";
			else
			{
				DZS_FxArea sel = Selected();
				if (sel && OnEdge(sel, g))
					m_Drag = "resize";
				else
				{
					Select(m_Hover);
					m_Drag = "";
				}
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
			if (what == "create")
			{
				float r = Math.Sqrt((g[0] - m_PressGround[0]) * (g[0] - m_PressGround[0]) + (g[2] - m_PressGround[2]) * (g[2] - m_PressGround[2]));
				if (r < 1)
					r = DZS_EffectsModel.PresetRadius(m_AddPreset);
				Create(inst, m_AddPreset, m_PressGround[0], m_PressGround[2], r);
				m_AddPreset = "";
			}
			else if (what == "move")
			{
				DZS_FxArea mv = Selected();
				if (mv)
					Change(inst, mv, mv.m_X + g[0] - m_PressGround[0], mv.m_Z + g[2] - m_PressGround[2], mv.m_R);
			}
			else if (what == "resize")
			{
				DZS_FxArea rs = Selected();
				if (rs)
				{
					float nr = Math.Max(1, Math.Sqrt((g[0] - rs.m_X) * (g[0] - rs.m_X) + (g[2] - rs.m_Z) * (g[2] - rs.m_Z)));
					Change(inst, rs, rs.m_X, rs.m_Z, nr);
				}
			}
		}
	}

	static int ColorOf(string preset)
	{
		if (preset == "gas")
			return ARGB(255, 163, 230, 53);
		if (preset == "geyser")
			return ARGB(255, 96, 165, 250);
		if (preset == "spring")
			return ARGB(255, 45, 212, 191);
		if (preset == "volcanic")
			return ARGB(255, 251, 146, 60);
		if (preset == "hearts")
			return ARGB(255, 244, 114, 182);
		return ARGB(255, 200, 200, 200);
	}

	protected void Ring(float x, float z, float r, int color, float width)
	{
		int segs = 48;
		vector cam = GetGame().GetCurrentCameraPosition();
		for (int i = 0; i < segs; i++)
		{
			float a0 = Math.PI2 * i / segs;
			float a1 = Math.PI2 * (i + 1) / segs;
			float x0 = x + Math.Cos(a0) * r;
			float z0 = z + Math.Sin(a0) * r;
			float x1 = x + Math.Cos(a1) * r;
			float z1 = z + Math.Sin(a1) * r;
			vector w0 = Vector(x0, GetGame().SurfaceY(x0, z0) + 0.3, z0);
			vector w1 = Vector(x1, GetGame().SurfaceY(x1, z1) + 0.3, z1);
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
		foreach (DZS_FxArea a : m_Areas)
		{
			if (a.m_Gone)
				continue;
			if (vector.Distance(Vector(a.m_X, cam[1], a.m_Z), cam) > 3000 + a.m_R)
				continue;
			float x = a.m_X;
			float z = a.m_Z;
			float r = Math.Max(a.m_R, 1);
			string key = a.Key();
			if (key == m_Sel && hasG && m_Drag == "move")
			{
				x += g[0] - m_PressGround[0];
				z += g[2] - m_PressGround[2];
			}
			if (key == m_Sel && hasG && m_Drag == "resize")
				r = Math.Max(1, Math.Sqrt((g[0] - x) * (g[0] - x) + (g[2] - z) * (g[2] - z)));
			float w = DZS_Scale.Px(2);
			if (key == m_Sel)
				w = DZS_Scale.Px(4);
			else if (key == m_Hover)
				w = DZS_Scale.Px(3);
			Ring(x, z, r, ColorOf(a.m_Preset), w);
			DZS_WorldLines.Dot(m_Canvas, Vector(x, GetGame().SurfaceY(x, z) + 0.5, z), DZS_Scale.Px(7), ColorOf(a.m_Preset), key == m_Sel);
		}
		if (m_Drag == "create" && hasG)
		{
			float cr = Math.Sqrt((g[0] - m_PressGround[0]) * (g[0] - m_PressGround[0]) + (g[2] - m_PressGround[2]) * (g[2] - m_PressGround[2]));
			Ring(m_PressGround[0], m_PressGround[2], Math.Max(cr, 1), ColorOf(m_AddPreset), DZS_Scale.Px(3));
		}
	}

	// ---- changes ----

	protected static string NumText(float f)
	{
		return DZS_PatchNum.Canon(f.ToString());
	}

	void Create(DZS_Instance inst, string preset, float x, float z, float r)
	{
		string stem = "Dzs" + DZS_EffectsModel.PresetName(preset);
		stem.Replace(" ", "");
		string name = stem + "1";
		int n = 2;
		while (AreaNamed(name))
		{
			name = stem + n;
			n++;
		}
		DZS_OpBuilder b = inst.BeginOperation(DZS_EffectsModel.OP, 1);
		map<string, string> f = new map<string, string>();
		f.Set("name", name);
		f.Set("preset", preset);
		f.Set("x", NumText(x));
		f.Set("z", NumText(z));
		f.Set("r", NumText(r));
		string id = b.Create(DZS_EffectsModel.AREA, DZS_PROJECT_ID, DZS_END, f);
		b.Commit("Effektzone anlegen", "Add effect area");
		m_Sel = "e:" + id;
		DZS_Log.Info("effects: added " + preset + " " + name + " at " + x + " " + z + " r " + r);
	}

	protected bool AreaNamed(string name)
	{
		foreach (DZS_FxArea a : m_Areas)
		{
			if (a.m_Name == name)
				return true;
		}
		return false;
	}

	// Moves or resizes an area: an own one directly, a base one through its change element.
	void Change(DZS_Instance inst, DZS_FxArea a, float x, float z, float r)
	{
		DZS_OpBuilder b = inst.BeginOperation(DZS_EffectsModel.OP, 1);
		if (a.m_Own)
		{
			b.SetField(a.m_Elem, "x", NumText(x));
			b.SetField(a.m_Elem, "z", NumText(z));
			b.SetField(a.m_Elem, "r", NumText(r));
		}
		else
		{
			map<string, string> f = new map<string, string>();
			f.Set("area", a.m_Name);
			f.Set("bx", a.m_BX);
			f.Set("by", a.m_BY);
			f.Set("bz", a.m_BZ);
			f.Set("br", a.m_BR);
			f.Set("x", NumText(x));
			f.Set("z", NumText(z));
			f.Set("r", NumText(r));
			if (Math.AbsFloat(x - a.m_BX.ToFloat()) < 0.001 && Math.AbsFloat(z - a.m_BZ.ToFloat()) < 0.001)
			{
				f.Set("x", a.m_BX);
				f.Set("z", a.m_BZ);
			}
			if (Math.AbsFloat(r - a.m_BR.ToFloat()) < 0.001)
				f.Set("r", a.m_BR);
			if (a.m_Elem != "")
			{
				foreach (string k, string v : f)
					b.SetField(a.m_Elem, k, v);
			}
			else
				b.Create(DZS_EffectsModel.CHANGE, DZS_PROJECT_ID, DZS_END, f);
		}
		b.Commit("Effektzone ändern", "Change effect area");
		DZS_Log.Info("effects: changed " + a.m_Name + " to " + x + " " + z + " r " + r);
	}

	void RemoveSelected()
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_FxArea a = Selected();
		if (!inst || !a)
			return;
		DZS_OpBuilder b = inst.BeginOperation(DZS_EffectsModel.OP, 1);
		if (a.m_Own)
			b.Delete(a.m_Elem);
		else if (a.m_Elem != "")
			b.SetField(a.m_Elem, "gone", "1");
		else
		{
			map<string, string> f = new map<string, string>();
			f.Set("area", a.m_Name);
			f.Set("gone", "1");
			b.Create(DZS_EffectsModel.CHANGE, DZS_PROJECT_ID, DZS_END, f);
		}
		b.Commit("Effektzone entfernen", "Remove effect area");
		m_Sel = "";
		DZS_Log.Info("effects: removed " + a.m_Name);
	}

	void SetRadius(float r)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_FxArea a = Selected();
		if (inst && a)
			Change(inst, a, a.m_X, a.m_Z, r);
	}
}

class DZS_EffectsPanel : DZS_PanelContent
{
	protected DZS_EffectsPlugin m_P;
	protected Widget m_Parent;
	protected Widget m_Root;
	protected int m_BuiltGen = -1;
	protected float m_Width;
	protected float m_H;
	protected float m_Y;
	protected ref array<ref DZS_Button> m_Buttons = new array<ref DZS_Button>();
	protected ref array<ref DZS_NumberField> m_Fields = new array<ref DZS_NumberField>();

	void DZS_EffectsPanel(DZS_EffectsPlugin p)
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
		Line(DZS_Texts.Pick("Neue Zone", "New area"), "textMuted", 12, 20);
		float x = Pad();
		array<string> presets = DZS_EffectsModel.Presets();
		foreach (string p : presets)
		{
			string kind = "side";
			if (p == m_P.m_AddPreset)
				kind = "main";
			DZS_Button b = new DZS_Button(m_Root, kind, DZS_EffectsModel.PresetName(p));
			if (x + b.GetWidth() > m_Width - Pad())
			{
				x = Pad();
				m_Y += DZS_Scale.Px(30);
			}
			b.Place(x, m_Y);
			b.m_Tag = p;
			b.m_OnClick.Insert(OnPreset);
			m_Buttons.Insert(b);
			x += b.GetWidth() + DZS_Scale.Px(6);
		}
		m_Y += DZS_Scale.Px(38);
		DZS_FxArea a = m_P.Selected();
		if (!a)
		{
			int n = 0;
			foreach (DZS_FxArea c : m_P.m_Areas)
			{
				if (!c.m_Gone)
					n++;
			}
			Line(n.ToString() + DZS_Texts.Pick(" Zonen in Basis und Projekt", " areas in base and project"), "text", 14, 22);
			Line(DZS_Texts.Pick("Zone anklicken zum Bearbeiten.", "Click an area to edit it."), "textQuiet", 13, 20);
		}
		else
		{
			string kind2 = DZS_Texts.Pick("Zone der Basis", "Base area");
			if (a.m_Own)
				kind2 = DZS_Texts.Pick("Eigene Zone", "Own area");
			else if (a.m_Changed)
				kind2 = DZS_Texts.Pick("Zone der Basis, geändert", "Base area, changed");
			Line(kind2, "textMuted", 12, 18);
			Line(a.m_Name, "text", 15, 24);
			string type = DZS_EffectsModel.PresetName(a.m_Preset);
			if (a.m_Preset == "")
				type = a.m_Type;
			Line(type + " · x " + Math.Round(a.m_X) + " · z " + Math.Round(a.m_Z), "textQuiet", 13, 20);
			float rowH = DZS_Scale.Px(26);
			TextWidget l = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("text"));
			l.SetText(DZS_Texts.Pick("Radius (m)", "Radius (m)"));
			DZS_W.Place(l, Pad(), m_Y, m_Width * 0.5, rowH);
			DZS_NumberField f = new DZS_NumberField("effects.radius", "", "textMuted", 70);
			f.Build(m_Root);
			f.m_OnCommit.Insert(OnRadius);
			f.ShowValue(DZS_PatchNum.Canon(a.m_R.ToString()));
			f.Place(m_Width - Pad() - f.GetWidth(), m_Y, rowH);
			m_Fields.Insert(f);
			m_Y += rowH + DZS_Scale.Px(8);
			DZS_Button rem = new DZS_Button(m_Root, "danger", DZS_Texts.Pick("Zone entfernen", "Remove area"), "trash-2");
			rem.Place(Pad(), m_Y);
			rem.m_OnClick.Insert(OnRemove);
			m_Buttons.Insert(rem);
			m_Y += DZS_Scale.Px(32);
		}
		m_H = m_Y + DZS_Scale.Px(8);
		DZS_W.Place(m_Root, 0, 0, m_Width, m_H);
	}

	void OnPreset(DZS_Button b)
	{
		m_P.StartAdding(b.m_Tag);
	}

	void OnRadius(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		string t = typed;
		t.Trim();
		float num;
		r.m_Ok = DZS_Num.Parse(t, num) && num > 0;
		if (!r.m_Ok)
			return;
		m_P.SetRadius(num);
	}

	void OnRemove(DZS_Button b)
	{
		m_P.RemoveSelected();
	}
}

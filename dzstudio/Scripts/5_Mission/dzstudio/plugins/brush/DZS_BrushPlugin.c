// Brush: sets many objects with one stroke, or erases objects of the set's kinds. A stroke is one
// operation with fixed values; everything else here (on/off, mode, set, radius, density, Untergrund) is view or
// a Merkwert of the user. Uses only the public interface: kernel, scene, selection and catalog services.
class DZS_BrushActions : DZS_ActionHandler
{
	DZS_BrushPlugin m_P;

	override bool IsActionChecked(string actionId) { return actionId == "brush.toggle" && m_P.m_On; }

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (actionId == "brush.fromSelection")
		{
			DZS_SelectionApi sel = DZS_SelectionApi.Get();
			if (!sel || sel.GetSelection().Count() == 0)
			{
				reasonDe = "Nichts ausgewählt";
				reasonEn = "Nothing selected";
				return false;
			}
		}
		return true;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "brush.toggle")
			m_P.Toggle();
		else if (actionId == "brush.fromSelection")
			m_P.AskFromSelection();
	}
}

class DZS_BrushEsc : DZS_EscHandler
{
	DZS_BrushPlugin m_P;
	bool m_Stroke;

	override bool IsEscActive()
	{
		if (m_Stroke)
			return m_P.m_Stroke;
		return m_P.m_On && !m_P.m_Stroke;
	}

	override void OnEsc()
	{
		if (m_Stroke)
			m_P.CancelStroke("esc");
		else
			m_P.SetOn(false);
	}
}

// One object of a running stroke: its fixed values and the model shown until the stroke ends.
class DZS_BrushItem : Managed
{
	string m_Name;
	vector m_Pos;
	vector m_Ypr;
	float m_Scale;
	Object m_Obj;
	string m_PosText;
	string m_YprText;
	string m_ScaleText;
	bool m_Shown;
}

[DZS_RegisterPlugin(DZS_BrushPlugin)]
class DZS_BrushPlugin : DZS_Plugin
{
	static const float R_MIN = 1;
	static const float R_MAX = 50;
	static const float D_MIN = 0.1;
	static const float D_MAX = 5;
	static const string PANEL = "brush.panel";

	bool m_On;
	bool m_EraseMode;
	float m_Radius = 10;
	float m_Density = 2;
	bool m_OnObjects;
	protected int m_SetIdx;
	protected ref array<ref DZS_BrushSet> m_Sets;
	protected ref array<ref DZS_BrushSet> m_Bundled;
	protected ref array<ref DZS_BrushSet> m_Own;
	protected ref DZS_BrushCatalogMenu m_CatMenu;
	protected string m_DeleteKey;
	protected ref DZS_BrushActions m_Actions;
	protected ref DZS_BrushPanel m_Panel;
	protected ref DZS_BrushEsc m_EscStroke;
	protected ref DZS_BrushEsc m_EscOff;
	protected bool m_World;
	protected CanvasWidget m_Canvas;
	protected ref DZS_PointerHint m_Hint;
	protected bool m_LmbWas;
	protected bool m_HasHit;
	protected vector m_Hit;
	protected ref array<string> m_Marked = new array<string>();

	// stroke
	bool m_Stroke;
	protected bool m_StrokeErase;
	protected ref DZS_BrushSet m_StrokeSet;
	protected ref array<vector> m_Centers = new array<vector>();
	protected ref array<float> m_CenterR = new array<float>();
	protected float m_Carry;
	protected ref array<ref DZS_BrushItem> m_Items = new array<ref DZS_BrushItem>();
	protected ref array<string> m_Erased = new array<string>();
	protected ref map<string, bool> m_ErasedSet = new map<string, bool>();
	protected float m_StrokeStart;
	protected float m_Time;
	protected float m_LogAt;
	protected float m_LastDt;
	protected float m_MinFps = 1000;
	protected bool m_LogNextDt;
	protected float m_GroundLogAt;
	protected float m_FpsAt;
	protected int m_FpsFrames;
	protected float m_FpsWorst;
	protected int m_Prof[6];

	override string GetName() { return "brush"; }

	override void OnRegister()
	{
		RegisterTexts();
		m_Kernel.GetTypes().RegisterOperationType("brush", "brush:edit", 1);
		m_Actions = new DZS_BrushActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("brush", "Pinsel", "Brush");
		array<string> keys = {"B"};
		ui.RegisterAction("brush", "brush.toggle", "Pinsel an/aus", "Brush on/off", keys, m_Actions);
		ui.SetWhen("brush.toggle", "scene");
		ui.SetGroup("brush.toggle", "brush");
		ui.RegisterTool("brush", "brush.toggle", "Pinsel", "Brush", "switches", "paintbrush", 40);
		ui.AddMenuEntry("view", "brush.toggle", 340, true);
		ui.RegisterAction("brush", "brush.fromSelection", "Auswahl als Pinsel-Set …", "Selection as brush set …", null, m_Actions);
		ui.SetGroup("brush.fromSelection", "brush");
		ui.AddMenuEntry("edit", "brush.fromSelection", 904);
		m_Panel = new DZS_BrushPanel(this);
		ui.RegisterPanel("brush", PANEL, "Pinsel", "Brush", "right", "paintbrush", m_Panel, 500);
		ui.SetPanelShown(PANEL, false);
		DZS_Settings st = m_Kernel.GetSettings();
		m_Radius = Snap(st.GetNumber("brush.radius", 10), R_MIN, R_MAX);
		m_Density = Snap(st.GetNumber("brush.density", 2), D_MIN, D_MAX);
		m_OnObjects = st.GetBool("brush.onObjects", false);
	}

	protected void RegisterTexts()
	{
		DZS_Texts.Add("brush.mode.set", "Setzen", "Place");
		DZS_Texts.Add("brush.mode.erase", "Radieren", "Erase");
		DZS_Texts.Add("brush.kinds", "%1 Arten", "%1 kinds");
		DZS_Texts.Add("brush.radius", "Radius", "Radius");
		DZS_Texts.Add("brush.density", "Dichte", "Density");
		DZS_Texts.Add("brush.ground", "Untergrund", "Ground");
		DZS_Texts.Add("brush.ground.terrain", "Gelände", "Terrain");
		DZS_Texts.Add("brush.ground.all", "Gelände und Bauwerke", "Terrain and structures");
		DZS_Texts.Add("brush.set.noneInCatalog", "Kein Objekt dieses Sets steht im Katalog", "No object of this set is in the catalogue");
		DZS_Texts.Add("brush.more", "+ %1 weitere", "+ %1 more");
		DZS_Texts.Add("brush.hint.title", "Pinsel %1 · Gruppe %2", "Brush %1 · group %2");
		DZS_Texts.Add("brush.hint.eraseTitle", "Radieren %1 · nur Objekte des Projekts", "Erase %1 · only objects of the project");
		DZS_Texts.Add("brush.hint.drag", "[Ziehen] pinseln", "[Drag] brush");
		DZS_Texts.Add("brush.hint.dragErase", "[Ziehen] radieren", "[Drag] erase");
		DZS_Texts.Add("brush.hint.ctrlSet", "[Ctrl+Ziehen] setzen", "[Ctrl+Drag] place");
		DZS_Texts.Add("brush.hint.radius", "[Ctrl+Wheel] Radius %1 m", "[Ctrl+Wheel] radius %1 m");
		DZS_Texts.Add("brush.hint.density", "[Alt+Wheel] Dichte %1", "[Alt+Wheel] density %1");
		DZS_Texts.Add("brush.hint.perStamp", "≈ %1 je Abdruck", "≈ %1 per stamp");
		DZS_Texts.Add("brush.hint.objects", "%1 Objekte", "%1 objects");
		DZS_Texts.Add("brush.hint.inCircle", "%1 im Kreis", "%1 in the circle");
		DZS_Texts.Add("brush.hint.removed", "%1 Objekte entfernt", "%1 objects removed");
		DZS_Texts.Add("brush.hint.allZero", "Alle Anteile sind 0", "All shares are 0");
		DZS_Texts.Add("brush.context.set", "[Ziehen] pinseln · [Ctrl+Wheel] Radius · [Alt+Wheel] Dichte · [Ctrl+Ziehen] radieren · [%1] Pinsel aus · [Esc] beenden", "[Drag] brush · [Ctrl+Wheel] radius · [Alt+Wheel] density · [Ctrl+Drag] erase · [%1] brush off · [Esc] end");
		DZS_Texts.Add("brush.context.erase", "[Ziehen] radieren · [Ctrl+Ziehen] setzen · [Ctrl+Wheel] Radius · [%1] Pinsel aus · [Esc] beenden", "[Drag] erase · [Ctrl+Drag] place · [Ctrl+Wheel] radius · [%1] brush off · [Esc] end");
		DZS_Texts.Add("brush.op.paint", "%1 Objekte pinseln", "Brush %1 objects");
		DZS_Texts.Add("brush.op.paintOne", "%1 pinseln", "Brush %1");
		DZS_Texts.Add("brush.op.erase", "%1 Objekte radieren", "Erase %1 objects");
		DZS_Texts.Add("brush.op.eraseOne", "%1 radieren", "Erase %1");
		DZS_Texts.Add("brush.groupLocked", "Gruppe %1 ist gesperrt", "Group %1 is locked");
		DZS_Texts.Add("brush.groupHidden", "Gruppe %1 ist ausgeblendet", "Group %1 is hidden");
		DZS_Texts.Add("brush.noGroup", "Keine Gruppe, die Objekte aufnimmt", "No group that takes objects");
		DZS_Texts.Add("brush.kindsBundled", "mitgeliefert · %1 Arten", "bundled · %1 kinds");
		DZS_Texts.Add("brush.own.kicker", "Pinsel", "Brush");
		DZS_Texts.Add("brush.own.edit", "Set bearbeiten …", "Edit set …");
		DZS_Texts.Add("brush.own.copy", "Als eigenes Set kopieren …", "Copy as own set …");
		DZS_Texts.Add("brush.own.rename", "Umbenennen …", "Rename …");
		DZS_Texts.Add("brush.own.delete", "Löschen …", "Delete …");
		DZS_Texts.Add("brush.own.bundledLocked", "Mitgelieferte Sets lassen sich nicht ändern", "Bundled sets cannot be changed");
		DZS_Texts.Add("brush.own.newTitle", "Neues Pinsel-Set", "New brush set");
		DZS_Texts.Add("brush.own.renameTitle", "Pinsel-Set umbenennen", "Rename brush set");
		DZS_Texts.Add("brush.own.editTitle", "Set bearbeiten", "Edit set");
		DZS_Texts.Add("brush.own.create", "Anlegen", "Create");
		DZS_Texts.Add("brush.own.renameBtn", "Umbenennen", "Rename");
		DZS_Texts.Add("brush.own.name", "Name", "Name");
		DZS_Texts.Add("brush.own.noName", "Der Name braucht 1 bis 60 Zeichen", "The name needs 1 to 60 characters");
		DZS_Texts.Add("brush.own.taken", "Diesen Namen trägt schon ein Set", "A set already has this name");
		DZS_Texts.Add("brush.own.newWith", "Neues Pinsel-Set mit diesem Objekt …", "New brush set with this object …");
		DZS_Texts.Add("brush.own.addTo", "Zu Pinsel-Set %1 hinzufügen", "Add to brush set %1");
		DZS_Texts.Add("brush.own.already", "Steht schon im Set", "Already in the set");
		DZS_Texts.Add("brush.own.full", "Ein Set fasst höchstens 50 Arten", "A set holds at most 50 kinds");
		DZS_Texts.Add("brush.own.selection", "Auswahl", "Selection");
		DZS_Texts.Add("brush.own.copyName", "%1 Kopie", "%1 copy");
		DZS_Texts.Add("brush.own.unknown", "%1 Objekte stehen nicht im Katalog und kommen nicht ins Set", "%1 objects are not in the catalogue and stay out of the set");
		DZS_Texts.Add("brush.own.unknownOne", "1 Objekt steht nicht im Katalog und kommt nicht ins Set", "1 object is not in the catalogue and stays out of the set");
		DZS_Texts.Add("brush.own.nothingLeft", "Kein ausgewähltes Objekt steht im Katalog", "No selected object is in the catalogue");
		DZS_Texts.Add("brush.own.deleteTitle", "Pinsel-Set löschen", "Delete brush set");
		DZS_Texts.Add("brush.own.deleteQ", "Pinsel-Set %1 löschen? Das lässt sich nicht rückgängig machen.", "Delete brush set %1? This cannot be undone.");
		DZS_Texts.Add("brush.own.deleteBtn", "Löschen", "Delete");
		DZS_Texts.Add("brush.own.headBundled", "// Mitgeliefert", "// Bundled");
		DZS_Texts.Add("brush.own.headOwn", "// Eigene", "// Own");
		DZS_Texts.Add("brush.set.filter", "Set filtern", "Filter sets");
		DZS_Texts.Add("brush.set.noMatch", "Kein Set gefunden", "No set found");
		DZS_Texts.Add("brush.own.notInCatalog", "Steht nicht im Katalog — wird beim Pinseln übergangen", "Not in the catalogue — skipped when brushing");
		DZS_Texts.Add("brush.own.footer", "Objekte fügst du im Katalog über das Kontextmenü hinzu.", "Add objects in the catalogue via the context menu.");
		DZS_Texts.Add("brush.own.remove", "Entfernen", "Remove");
		DZS_Texts.Add("brush.own.lastOne", "Ein Set braucht mindestens eine Art", "A set needs at least one kind");
		DZS_Texts.Add("brush.own.share", "Anteil", "Share");
		DZS_Texts.Add("brush.own.randomYaw", "Zufällig drehen", "Random yaw");
		DZS_Texts.Add("brush.own.tilt", "Neigung bis", "Tilt up to");
		DZS_Texts.Add("brush.own.slope", "Mit dem Hang kippen", "Tilt with slope");
		DZS_Texts.Add("brush.own.offset", "Höhenversatz", "Height offset");
		DZS_Texts.Add("brush.own.scale", "Größe", "Scale");
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		m_Bundled = DZS_BrushStore.LoadBundled();
		m_Own = DZS_BrushOwnStore.LoadAll();
		RebuildList(null);
		PickRememberedSet();
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat && !m_CatMenu)
		{
			m_CatMenu = new DZS_BrushCatalogMenu();
			m_CatMenu.m_P = this;
			cat.AddMenuSource(m_CatMenu);
		}
		if (!m_World)
			return;
		Widget layer = m_Kernel.GetScreen().GetSceneLayer();
		m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", layer));
		m_Hint = new DZS_PointerHint(layer);
		m_EscStroke = new DZS_BrushEsc();
		m_EscStroke.m_P = this;
		m_EscStroke.m_Stroke = true;
		m_Kernel.GetScreen().AddEscHandler(m_EscStroke, 39);
		m_EscOff = new DZS_BrushEsc();
		m_EscOff.m_P = this;
		m_Kernel.GetScreen().AddEscHandler(m_EscOff, 85);
	}

	// -dzsBrushBoxes: model boxes of every entry of every set into $profile:dzstudio\brush-boxes.txt (input of
	// editor/mod-tools/brush/build_sets.py for the height offsets); one model per frame.
	protected int m_BoxDump = -1;
	protected ref array<string> m_BoxNames;

	protected void DumpBoxes()
	{
		if (m_BoxDump < 0)
		{
			string v;
			if (!GetCLIParam("dzsBrushBoxes", v) || !m_Sets)
				return;
			m_BoxNames = new array<string>();
			foreach (DZS_BrushSet s : m_Sets)
			{
				foreach (DZS_BrushEntry e : s.m_Entries)
				{
					if (m_BoxNames.Find(e.m_Name) < 0)
						m_BoxNames.Insert(e.m_Name);
				}
			}
			m_BoxDump = 0;
			FileHandle clear = OpenFile("$profile:dzstudio\\brush-boxes.txt", FileMode.WRITE);
			if (clear)
				CloseFile(clear);
		}
		if (m_BoxDump >= m_BoxNames.Count())
			return;
		string name = m_BoxNames[m_BoxDump];
		m_BoxDump++;
		vector a, b;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (!scene || !scene.GetBox(name, a, b))
			return;
		float trunk = TrunkRadius(name, a, b);
		FileHandle fh = OpenFile("$profile:dzstudio\\brush-boxes.txt", FileMode.APPEND);
		if (!fh)
			return;
		FPrintln(fh, name + "\t" + a[0] + "\t" + a[1] + "\t" + a[2] + "\t" + b[0] + "\t" + b[1] + "\t" + b[2] + "\t" + trunk);
		CloseFile(fh);
		if (m_BoxDump == m_BoxNames.Count())
			DZS_Log.Info("brush: boxes of " + m_BoxNames.Count() + " models written");
	}

	// Stem radius at 0.3 m above the foot: the model alone high in the sky, eight level rays from 10 m outside
	// towards the box's middle axis against the geometry; the farthest hit from the axis.
	protected float TrunkRadius(string name, vector a, vector b)
	{
		DZS_SceneApi scene = DZS_SceneApi.Get();
		vector at = "7000 1500 7000";
		Object o = scene.MakeTemp(name, at, "0 0 0", 1);
		if (!o)
			return -1;
		vector axis = at + Vector((a[0] + b[0]) * 0.5, a[1] + 0.3, (a[2] + b[2]) * 0.5);
		float r = 0;
		for (int i = 0; i < 8; i++)
		{
			float ang = i * Math.PI2 / 8;
			vector dir = Vector(Math.Cos(ang), 0, Math.Sin(ang));
			vector from = axis + dir * 10;
			vector cp, cd;
			int comp;
			set<Object> res = new set<Object>();
			if (DayZPhysics.RaycastRV(from, axis, cp, cd, comp, res, null, null, true, false, ObjIntersectGeom, 0))
			{
				float d = vector.Distance(Vector(cp[0], 0, cp[2]), Vector(axis[0], 0, axis[2]));
				if (d > r)
					r = d;
			}
		}
		GetGame().ObjectDelete(o);
		return r;
	}

	protected void PickRememberedSet()
	{
		m_SetIdx = 0;
		string key = m_Kernel.GetSettings().GetString("brush.set", "");
		for (int i = 0; i < m_Sets.Count(); i++)
		{
			if (m_Sets[i].Key() == key)
				m_SetIdx = i;
		}
	}

	DZS_BrushSet CurrentSet()
	{
		if (!m_Sets || m_Sets.Count() == 0)
			return null;
		return m_Sets[Math.Clamp(m_SetIdx, 0, m_Sets.Count() - 1)];
	}

	// ---- view and Merkwerte ----

	static float Snap(float v, float lo, float hi)
	{
		v = Math.Clamp(v, lo, hi);
		return Math.Round(v * 10) / 10;
	}

	void Toggle()
	{
		SetOn(!m_On);
	}

	void SetOn(bool on)
	{
		if (on == m_On)
			return;
		if (!on && m_Stroke)
			CancelStroke("off");
		if (on)
		{
			DZS_CatalogApi cat = DZS_CatalogApi.Get();
			if (cat && cat.GetHand() != "")
				cat.DropHand();
			m_EraseMode = false;
		}
		m_On = on;
		m_Kernel.SetSceneTool("");
		if (on)
			m_Kernel.SetSceneTool("brush");
		m_Kernel.GetUi().SetPanelShown(PANEL, on);
		if (!on)
		{
			ClearView();
			m_Kernel.GetScreen().SetHint("brush", 30, "");
		}
		DZS_Log.Info("brush: on " + DZS_Str.Bool(on));
	}

	void SetEraseMode(bool erase)
	{
		m_EraseMode = erase;
		DZS_Log.Info("brush: mode " + ModeName(erase));
	}

	protected string ModeName(bool erase)
	{
		if (erase)
			return "erase";
		return "set";
	}

	string GroundName()
	{
		if (m_OnObjects)
			return "terrain+structures";
		return "terrain";
	}

	void ChooseSet(int idx)
	{
		if (idx < 0 || idx >= m_Sets.Count())
			return;
		m_SetIdx = idx;
		RememberSet();
	}

	protected void RememberSet()
	{
		DZS_BrushSet s = CurrentSet();
		if (!s)
			return;
		m_Kernel.GetSettings().Set("brush.set", DZS_Json.Str(s.Key()));
		DZS_Log.Info("brush: set '" + s.m_NameDe + "' " + s.Kinds() + " kinds");
	}

	void SetRadius(float v)
	{
		m_Radius = Snap(v, R_MIN, R_MAX);
		m_Kernel.GetSettings().Set("brush.radius", DZS_Json.Num(DZS_Num.Fixed(m_Radius, 1, true)));
	}

	void SetDensity(float v)
	{
		m_Density = Snap(v, D_MIN, D_MAX);
		m_Kernel.GetSettings().Set("brush.density", DZS_Json.Num(DZS_Num.Fixed(m_Density, 1, true)));
	}

	void SetOnObjects(bool on)
	{
		m_OnObjects = on;
		m_Kernel.GetSettings().Set("brush.onObjects", DZS_Json.Boolean(on));
	}

	// Game names of the kinds of the set (tooltip of the name): ten at most, then "+ n weitere".
	string KindsTip()
	{
		DZS_BrushSet s = CurrentSet();
		if (!s)
			return "";
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		string tip = "";
		int n = s.m_Entries.Count();
		for (int i = 0; i < n && i < 10; i++)
		{
			string shown = s.m_Entries[i].m_Name;
			if (cat)
				shown = cat.ShownName(shown);
			if (tip != "")
				tip += ", ";
			tip += shown;
		}
		if (n > 10)
		{
			string more = DZS_Texts.Fmt("brush.more", (n - 10).ToString());
			tip += " " + more;
		}
		return tip;
	}

	// Choosing a set: the list under the drop-down with a filter field that has the focus at once.
	void OpenSetList(float x, float y, float w)
	{
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		for (int i = 0; i < m_Sets.Count(); i++)
		{
			DZS_BrushSet s = m_Sets[i];
			if (i == 0 || (!s.m_Bundled && m_Sets[i - 1].m_Bundled))
			{
				DZS_MenuItem head = new DZS_MenuItem();
				head.m_Text = DZS_Texts.Get("brush.own.headBundled");
				if (!s.m_Bundled)
					head.m_Text = DZS_Texts.Get("brush.own.headOwn");
				head.m_Enabled = false;
				head.m_Heading = true;
				head.m_Tag = "";
				items.Insert(head);
			}
			DZS_MenuItem it = new DZS_MenuItem();
			it.m_Text = s.Name();
			it.m_Tag = i.ToString();
			it.m_Check = true;
			it.m_Checked = i == m_SetIdx;
			if (s.Usable(false).Count() == 0)
			{
				it.m_Icon = "triangle-alert";
				it.m_Tip = DZS_Texts.Get("brush.set.noneInCatalog");
			}
			items.Insert(it);
		}
		DZS_Screen scr = m_Kernel.GetScreen();
		DZS_Popup pop = scr.OpenContextMenu(items);
		if (!pop)
			return;
		pop.EnableFilter(DZS_Texts.Get("brush.set.filter"), DZS_Texts.Get("brush.set.noMatch"), w / DZS_Scale.Px(1));
		pop.Open(x, y);
		scr.FocusField(pop.GetFilter());
		pop.m_OnChoose.Insert(OnSetChosen);
		DZS_Log.Info("brush: set list open, " + m_Sets.Count() + " sets");
	}

	void OnSetChosen(DZS_Popup p, DZS_MenuItem it)
	{
		m_Kernel.GetScreen().CloseMenu();
		if (it.m_Tag == "")
			return;
		ChooseSet(it.m_Tag.ToInt());
	}

	// ---- own sets ----

	// Bundled sets in their fixed order, then the own ones by name regardless of case; keeps 'keep' chosen.
	protected void RebuildList(DZS_BrushSet keep)
	{
		array<string> keys = new array<string>();
		map<string, DZS_BrushSet> byKey = new map<string, DZS_BrushSet>();
		for (int i = 0; i < m_Own.Count(); i++)
		{
			string low = m_Own[i].m_NameDe;
			low.ToLower();
			string k = low + "|" + m_Own[i].m_Id;
			keys.Insert(k);
			byKey.Set(k, m_Own[i]);
		}
		keys.Sort();
		m_Sets = new array<ref DZS_BrushSet>();
		foreach (DZS_BrushSet b : m_Bundled)
			m_Sets.Insert(b);
		foreach (string key : keys)
			m_Sets.Insert(byKey.Get(key));
		if (keep)
		{
			int at = m_Sets.Find(keep);
			if (at >= 0)
				m_SetIdx = at;
		}
		m_SetIdx = Math.Clamp(m_SetIdx, 0, Math.Max(0, m_Sets.Count() - 1));
	}

	string NameProblem(string name, DZS_BrushSet own)
	{
		string t = name;
		t.TrimInPlace();
		int chars = DZS_Names.Chars(t);
		if (chars < 1 || chars > 60)
			return DZS_Texts.Get("brush.own.noName");
		string low = t;
		low.ToLower();
		foreach (DZS_BrushSet s : m_Sets)
		{
			if (s == own)
				continue;
			string a = s.m_NameDe;
			string b = s.m_NameEn;
			a.ToLower();
			b.ToLower();
			if (a == low || b == low)
				return DZS_Texts.Get("brush.own.taken");
		}
		return "";
	}

	protected static DZS_BrushEntry NewEntry(string name)
	{
		DZS_BrushEntry e = new DZS_BrushEntry();
		e.m_Name = name;
		return e;
	}

	protected static DZS_BrushEntry CopyEntry(DZS_BrushEntry o)
	{
		DZS_BrushEntry e = new DZS_BrushEntry();
		e.m_Name = o.m_Name;
		e.m_Share = o.m_Share;
		e.m_RandomYaw = o.m_RandomYaw;
		e.m_Tilt = o.m_Tilt;
		e.m_Slope = o.m_Slope;
		e.m_SlopeMax = o.m_SlopeMax;
		e.m_Offset = o.m_Offset;
		e.m_ScaleMin = o.m_ScaleMin;
		e.m_ScaleMax = o.m_ScaleMax;
		return e;
	}

	void CreateOwn(string name, array<ref DZS_BrushEntry> entries)
	{
		DZS_BrushSet s = new DZS_BrushSet();
		s.m_Bundled = false;
		s.m_NameDe = name;
		s.m_NameEn = name;
		for (int i = 0; i < entries.Count() && i < DZS_BrushOwnStore.MAX_ENTRIES; i++)
			s.m_Entries.Insert(entries[i]);
		DZS_BrushOwnStore.Save(s);
		m_Own.Insert(s);
		RebuildList(s);
		RememberSet();
		DZS_Log.Info("brush: own set '" + name + "' created, " + s.m_Entries.Count() + " entries, file " + s.m_Id);
	}

	void AskNewWith(string entry)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		string shown = entry;
		if (cat)
			shown = cat.ShownName(entry);
		DZS_BrushNameDialog d = new DZS_BrushNameDialog(this, "new", Unique(shown), "", null);
		d.m_Entries = new array<ref DZS_BrushEntry>();
		DZS_BrushEntry t603 = NewEntry(entry);
		d.m_Entries.Insert(t603);
		d.Open();
	}

	// The suggestion itself if free, else with " (2)", " (3)" …
	protected string Unique(string name)
	{
		string t = name;
		if (t.Length() > 60)
			t = t.Substring(0, 60);
		if (NameProblem(t, null) == "")
			return t;
		for (int n = 2; n < 100; n++)
		{
			string c = t + " (" + n + ")";
			if (NameProblem(c, null) == "")
				return c;
		}
		return t;
	}

	void AddToCurrent(string entry)
	{
		DZS_BrushSet s = CurrentSet();
		if (!s || s.m_Bundled || s.HasName(entry) || s.m_Entries.Count() >= DZS_BrushOwnStore.MAX_ENTRIES)
			return;
		DZS_BrushEntry t629 = NewEntry(entry);
		s.m_Entries.Insert(t629);
		s.ForgetNames();
		DZS_BrushOwnStore.Save(s);
		DZS_Log.Info("brush: own set '" + s.m_NameDe + "' + " + entry + ", " + s.m_Entries.Count() + " entries");
	}

	void AskFromSelection()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!sel || !inst)
			return;
		array<string> ids = sel.GetSelection();
		array<string> order = new array<string>();
		map<string, int> counts = new map<string, int>();
		map<string, string> spelling = new map<string, string>();
		int unknown = 0;
		foreach (string id : ids)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			if (cat && !cat.InCatalog(name))
			{
				unknown++;
				continue;
			}
			string low = name;
			low.ToLower();
			if (!counts.Contains(low))
			{
				order.Insert(low);
				spelling.Set(low, name);
				counts.Set(low, 0);
			}
			int c = counts.Get(low);
			counts.Set(low, c + 1);
		}
		array<ref DZS_BrushEntry> entries = new array<ref DZS_BrushEntry>();
		foreach (string key : order)
		{
			if (entries.Count() >= DZS_BrushOwnStore.MAX_ENTRIES)
				break;
			DZS_BrushEntry e = NewEntry(spelling.Get(key));
			e.m_Share = Math.Min(counts.Get(key), 100);
			entries.Insert(e);
		}
		string note = "";
		if (unknown == 1)
			note = DZS_Texts.Get("brush.own.unknownOne");
		else if (unknown > 1)
			note = DZS_Texts.Fmt("brush.own.unknown", unknown.ToString());
		string selName = DZS_Texts.Get("brush.own.selection");
		DZS_BrushNameDialog d = new DZS_BrushNameDialog(this, "new", Unique(selName), note, null);
		d.m_Entries = entries;
		d.Open();
		DZS_Log.Info("brush: selection as set: " + entries.Count() + " kinds, " + unknown + " not in the catalogue");
	}

	void AskCopy()
	{
		DZS_BrushSet s = CurrentSet();
		if (!s)
			return;
		string copyName = DZS_Texts.Fmt("brush.own.copyName", s.Name());
		DZS_BrushNameDialog d = new DZS_BrushNameDialog(this, "copy", Unique(copyName), "", null);
		d.Open();
	}

	void CopyCurrentAs(string name)
	{
		DZS_BrushSet s = CurrentSet();
		if (!s)
			return;
		array<ref DZS_BrushEntry> entries = new array<ref DZS_BrushEntry>();
		foreach (DZS_BrushEntry e : s.m_Entries)
		{
			DZS_BrushEntry t705 = CopyEntry(e);
			entries.Insert(t705);
		}
		CreateOwn(name, entries);
	}

	void RenameOwn(DZS_BrushSet s, string name)
	{
		if (!s || s.m_Bundled || NameProblem(name, s) != "")
			return;
		s.m_NameDe = name;
		s.m_NameEn = name;
		DZS_BrushOwnStore.Save(s);
		DZS_BrushSet cur = CurrentSet();
		RebuildList(cur);
		RememberSet();
		DZS_Log.Info("brush: own set renamed to '" + name + "'");
	}

	void OwnChanged(DZS_BrushSet s, string what)
	{
		if (!s || s.m_Bundled)
			return;
		DZS_BrushOwnStore.Save(s);
		DZS_Log.Info("brush: own set '" + s.m_NameDe + "' changed (" + what + "), shares " + s.ShareSum());
	}

	void AskRename()
	{
		DZS_BrushSet s = CurrentSet();
		if (!s || s.m_Bundled)
			return;
		DZS_BrushNameDialog d = new DZS_BrushNameDialog(this, "rename", s.m_NameDe, "", s);
		d.Open();
	}

	void AskDelete()
	{
		DZS_BrushSet s = CurrentSet();
		if (!s || s.m_Bundled)
			return;
		m_DeleteKey = s.Key();
		string q = DZS_Texts.Fmt("brush.own.deleteQ", s.Name());
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(DZS_Texts.Get("brush.own.kicker"), DZS_Texts.Get("brush.own.deleteTitle"), q);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("delete", DZS_Texts.Get("brush.own.deleteBtn"), "danger");
		c.m_OnResult.Insert(OnDeleteAnswer);
		c.Open();
	}

	void OnDeleteAnswer(DZS_Dialog d, string result)
	{
		if (result != "delete")
			return;
		DZS_BrushSet s = CurrentSet();
		if (!s || s.m_Bundled || s.Key() != m_DeleteKey)
			return;
		int at = m_SetIdx;
		DZS_BrushOwnStore.Delete(s);
		string name = s.m_NameDe;
		m_Own.RemoveItem(s);
		m_SetIdx = Math.Max(0, at - 1);
		RebuildList(null);
		RememberSet();
		DZS_Log.Info("brush: own set '" + name + "' deleted");
	}

	void OpenMoreMenu()
	{
		DZS_BrushSet s = CurrentSet();
		bool own = s && !s.m_Bundled;
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		DZS_MenuItem a = new DZS_MenuItem();
		a.m_Text = DZS_Texts.Get("brush.own.copy");
		a.m_Tag = "copy";
		a.m_Icon = "copy";
		items.Insert(a);
		DZS_MenuItem b = new DZS_MenuItem();
		b.m_Text = DZS_Texts.Get("brush.own.rename");
		b.m_Tag = "rename";
		b.m_Enabled = own;
		if (!own)
			b.m_Reason = DZS_Texts.Get("brush.own.bundledLocked");
		items.Insert(b);
		DZS_MenuItem c = new DZS_MenuItem();
		c.m_Text = DZS_Texts.Get("brush.own.delete");
		c.m_Tag = "delete";
		c.m_Icon = "trash";
		c.m_Enabled = own;
		if (!own)
			c.m_Reason = DZS_Texts.Get("brush.own.bundledLocked");
		items.Insert(c);
		DZS_Popup pop = m_Kernel.GetScreen().OpenContextMenu(items);
		if (pop)
			pop.m_OnChoose.Insert(OnMoreChosen);
	}

	void OnMoreChosen(DZS_Popup p, DZS_MenuItem it)
	{
		m_Kernel.GetScreen().CloseMenu();
		if (it.m_Tag == "copy")
			AskCopy();
		else if (it.m_Tag == "rename")
			AskRename();
		else if (it.m_Tag == "delete")
			AskDelete();
	}

	void OpenEditDialog()
	{
		DZS_BrushSet s = CurrentSet();
		if (!s)
			return;
		DZS_BrushEditDialog d = new DZS_BrushEditDialog(this, s);
		d.Open();
	}

	// ---- per frame ----

	override void OnUpdate(float dt)
	{
		m_Time += dt;
		m_LastDt = dt;
		string fl;
		if (GetCLIParam("dzsFpsLog", fl))
		{
			m_FpsFrames++;
			m_FpsWorst = Math.Max(m_FpsWorst, dt);
			if (m_Time - m_FpsAt >= 2)
			{
				int nf = Math.Max(1, m_FpsFrames);
				string prof = "hit " + (m_Prof[0] / nf) + " stroke " + (m_Prof[1] / nf) + " cand " + (m_Prof[2] / nf);
				prof = prof + " taken " + (m_Prof[3] / nf) + " draw " + (m_Prof[4] / nf) + " hints " + (m_Prof[5] / nf);
				int fps = Math.Round(m_FpsFrames / (m_Time - m_FpsAt));
				int worst = Math.Round(m_FpsWorst * 1000);
				DZS_Log.Info("fps: " + fps + " worst frame " + worst + " ms, stroke " + DZS_Str.Bool(m_Stroke) + " ticks/frame " + prof);
				for (int pi = 0; pi < 6; pi++)
					m_Prof[pi] = 0;
				m_FpsAt = m_Time;
				m_FpsFrames = 0;
				m_FpsWorst = 0;
			}
		}
		if (m_LogNextDt)
		{
			m_LogNextDt = false;
			DZS_Log.Info("brush: frame after the release " + Math.Round(dt * 1000) + " ms");
		}
		if (!m_World)
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_Screen scr = m_Kernel.GetScreen();
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (m_On && cat && cat.GetHand() != "")
			SetOn(false);
		if (inst && !scr.IsHome() && cat && cat.IsReady())
			DumpBoxes();
		m_Canvas.Clear();
		int sw, sh;
		GetScreenSize(sw, sh);
		DZS_W.Place(m_Canvas, 0, 0, sw, sh);
		if (!m_On || !inst || scr.IsHome())
		{
			m_Hint.Hide();
			m_Kernel.SetFramesTaken(null);
			return;
		}
		WarmBoxes();
		ShowItems();
		bool blocked = Blocked();
		if (blocked && m_Stroke)
			CancelStroke("dialog");
		bool over = !blocked && (!scr.IsCursorVisible() || scr.IsPointerOverScene());
		bool lmb = DZS_UiRegistry.KeyDown("MouseLeft");
		bool lDown = lmb && !m_LmbWas;
		bool lUp = !lmb && m_LmbWas;
		m_LmbWas = lmb;
		if (over)
			Wheel();
		int p0 = TickCount(0);
		m_HasHit = over && FindHit(m_Hit);
		m_Prof[0] = m_Prof[0] + TickCount(p0);
		if (lDown && over && !scr.IsMouseSwallowed())
			BeginStroke(inst);
		int p1 = TickCount(0);
		if (m_Stroke)
		{
			if (m_HasHit)
			{
				if (m_StrokeErase)
					EraseAt(inst, m_Hit);
				else
					StampIfMoved(inst, m_Hit);
			}
			else if (m_StrokeErase)
				HideErased();
			m_MinFps = Math.Min(m_MinFps, 1 / Math.Max(0.001, dt));
			if (lUp || !lmb)
				EndStroke(inst);
		}
		bool erase = m_EraseMode;
		if (m_Stroke)
			erase = m_StrokeErase;
		else if (DZS_UiRegistry.ModDown("Ctrl") && lmb)
			erase = !m_EraseMode;
		m_Prof[1] = m_Prof[1] + TickCount(p1);
		int p2 = TickCount(0);
		m_Marked.Clear();
		if (m_HasHit && erase && !m_Stroke)
			Candidates(inst, m_Hit, m_Marked);
		m_Prof[2] = m_Prof[2] + TickCount(p2);
		int p3 = TickCount(0);
		array<string> taken = new array<string>();
		taken.Copy(m_Marked);
		foreach (string eid : m_Erased)
			taken.Insert(eid);
		m_Kernel.SetFramesTaken(taken);
		m_Prof[3] = m_Prof[3] + TickCount(p3);
		int p4 = TickCount(0);
		Draw(inst, erase);
		m_Prof[4] = m_Prof[4] + TickCount(p4);
		int p5 = TickCount(0);
		Hints(inst, erase, over);
		m_Prof[5] = m_Prof[5] + TickCount(p5);
		if (m_Time - m_LogAt > 2 && m_Stroke)
		{
			m_LogAt = m_Time;
			DZS_Log.Info("brush: stroke " + m_Items.Count() + " objects, erased " + m_Erased.Count() + ", fps " + Math.Round(1 / Math.Max(0.001, dt)) + " min " + Math.Round(m_MinFps));
		}
	}

	// Model boxes of the chosen set, one per frame before the first stroke needs them (measuring makes an object).
	protected ref map<string, bool> m_Warm = new map<string, bool>();

	protected void WarmBoxes()
	{
		DZS_BrushSet s = CurrentSet();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (!s || !scene)
			return;
		foreach (DZS_BrushEntry e : s.m_Entries)
		{
			if (m_Warm.Contains(e.m_Name))
				continue;
			m_Warm.Set(e.m_Name, true);
			vector a, b;
			scene.GetBox(e.m_Name, a, b);
			return;
		}
	}

	// The stroke's models appear at most 40 per frame (a large stamp would otherwise stop the game for a moment).
	protected void ShowItems()
	{
		if (!m_Stroke)
			return;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (!scene)
			return;
		int made = 0;
		foreach (DZS_BrushItem it : m_Items)
		{
			if (it.m_Obj || it.m_Shown)
				continue;
			Finalize(it);
			it.m_Obj = scene.MakeTemp(it.m_Name, it.m_Pos, it.m_Ypr, it.m_Scale);
			it.m_Shown = true;
			made++;
			if (made >= 40)
				return;
		}
	}

	// Exact float32 text of the values; the shown model and the operation use the same numbers.
	protected void Finalize(DZS_BrushItem it)
	{
		if (it.m_PosText != "")
			return;
		it.m_PosText = DZS_Obj.Vec(it.m_Pos[0], it.m_Pos[1], it.m_Pos[2]);
		it.m_YprText = DZS_Obj.Vec(it.m_Ypr[0], it.m_Ypr[1], it.m_Ypr[2]);
		it.m_ScaleText = DZS_Obj.Num(it.m_Scale);
		it.m_Pos = DZS_Obj.QuickVec(it.m_PosText);
		it.m_Ypr = DZS_Obj.QuickVec(it.m_YprText);
	}

	protected bool Blocked()
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		return scr.HasDialog() || scr.HasKeyboardOwner("map") || scr.HasKeyboardOwner("menu") || scr.HasKeyboardOwner("fullsearch") || GetGame().GetUIManager().GetMenu() != null;
	}

	// Ctrl+wheel radius (Shift: tenth), Alt+wheel density (Shift: 0.1) — fixed keys.
	protected void Wheel()
	{
		int w = GetMouseState(MouseState.WHEEL);
		if (w == 0)
			return;
		bool shift = DZS_UiRegistry.ModDown("Shift");
		if (DZS_UiRegistry.ModDown("Ctrl"))
		{
			float rs = 1;
			if (shift)
				rs = 0.1;
			SetRadius(m_Radius + w * rs);
			DZS_Log.Info("brush: radius " + DZS_Num.Fixed(m_Radius, 1, true));
		}
		else if (DZS_UiRegistry.ModDown("Alt"))
		{
			float ds = 0.5;
			if (shift)
				ds = 0.1;
			SetDensity(m_Density + w * ds);
			DZS_Log.Info("brush: density " + DZS_Num.Fixed(m_Density, 1, true));
		}
	}

	protected vector PointerDir()
	{
		if (!m_Kernel.GetScreen().IsCursorVisible())
			return GetGame().GetCurrentCameraDirection();
		return GetGame().GetPointerDirection();
	}

	protected float ViewDist()
	{
		float d = m_Kernel.GetSettings().GetSettingNumber("camera.viewDistance");
		if (d <= 0)
			d = 1500;
		return d;
	}

	// ---- what counts as ground ----

	// Objects that never count as ground: the stroke's own and those of the set's kinds (also map objects of that model).
	protected bool Skipped(Object o, DZS_BrushSet s)
	{
		if (!o)
			return false;
		foreach (DZS_BrushItem it : m_Items)
		{
			if (it.m_Obj == o)
				return true;
		}
		if (!s)
			return false;
		string t = o.GetType();
		if (t != "" && s.HasName(t))
			return true;
		string shape = o.GetShapeName();
		shape.ToLower();
		if (shape == "")
			return false;
		foreach (DZS_BrushEntry e : s.m_Entries)
		{
			string n = e.m_Name;
			n.ToLower();
			int cut = n.LastIndexOf("\\");
			if (cut >= 0)
				n = n.Substring(cut + 1, n.Length() - cut - 1);
			if (shape.Contains(n))
				return true;
		}
		return false;
	}

	protected DZS_BrushSet GroundSet()
	{
		if (m_Stroke && m_StrokeSet)
			return m_StrokeSet;
		return CurrentSet();
	}

	// Point under the pointer: the terrain (objects are passed through) or, with Untergrund "Gelände und Bauwerke", the first surface
	// that is not one of the stroke's or the set's objects. Water does not count.
	protected bool FindHit(out vector hit)
	{
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = PointerDir();
		float len = ViewDist();
		if (!m_OnObjects)
		{
			vector cp, cd;
			int comp;
			if (!DayZPhysics.RaycastRV(from, from + dir * len, cp, cd, comp, null, null, null, false, true, ObjIntersectView, 0))
				return false;
			hit = cp;
			return true;
		}
		DZS_BrushSet s = GroundSet();
		array<ref DZS_RayHit> hits = DZS_Rays.Ray(from, dir, len, new array<Object>());
		foreach (DZS_RayHit h : hits)
		{
			if (Skipped(h.m_Obj, s))
				continue;
			hit = h.m_Pos;
			return true;
		}
		return false;
	}

	// Ground straight below a point, searched from 1000 m above it; false where nothing is found.
	protected bool GroundAt(float x, float z, out vector ground, out vector normal)
	{
		float ty = GetGame().SurfaceY(x, z);
		if (!m_OnObjects)
		{
			ground = Vector(x, ty, z);
			normal = GetGame().SurfaceGetNormal(x, z);
			return true;
		}
		DZS_BrushSet s = GroundSet();
		array<ref DZS_RayHit> hits;
		float above = 1000;
		while (true)
		{
			vector top = Vector(x, ty + above, z);
			hits = DZS_Rays.Ray(top, "0 -1 0", above + 50, new array<Object>());
			if (hits.Count() > 0 || above < 100)
				break;
			above = above / 2;
		}
		if (m_GroundLogAt < m_Time - 1)
		{
			m_GroundLogAt = m_Time;
			DZS_Log.Info("brush: ground ray from " + above + " m above: " + hits.Count() + " hit(s)");
		}
		foreach (DZS_RayHit h : hits)
		{
			if (Skipped(h.m_Obj, s))
				continue;
			ground = h.m_Pos;
			normal = h.m_Normal;
			return true;
		}
		// the ray met only the set's own kinds (a forest of them): the terrain below
		ground = Vector(x, ty, z);
		normal = GetGame().SurfaceGetNormal(x, z);
		DZS_Log.Info("brush: ground ray " + hits.Count() + " hit(s), all skipped; terrain");
		return true;
	}

	// ---- strokes ----

	protected void BeginStroke(DZS_Instance inst)
	{
		bool erase = m_EraseMode;
		if (DZS_UiRegistry.ModDown("Ctrl"))
			erase = !erase;
		DZS_BrushSet s = CurrentSet();
		if (!s)
			return;
		if (!erase && !ActiveGroupOpen(inst))
			return;
		m_Stroke = true;
		m_StrokeErase = erase;
		m_StrokeSet = s;
		m_Centers.Clear();
		m_CenterR.Clear();
		m_Carry = 0;
		m_Items.Clear();
		m_Erased.Clear();
		m_ErasedSet.Clear();
		m_Hidden = 0;
		m_StrokeStart = m_Time;
		m_MinFps = 1000;
		m_Kernel.SetGesture("brush", true);
		DZS_Log.Info("brush: stroke start " + ModeName(erase) + " set '" + s.m_NameDe + "' radius " + DZS_Num.Fixed(m_Radius, 1, true) + " density " + DZS_Num.Fixed(m_Density, 1, true) + " ground " + GroundName());
		if (m_HasHit)
		{
			if (erase)
				EraseAt(inst, m_Hit);
			else
				Stamp(inst, m_Hit);
		}
	}

	protected bool ActiveGroupOpen(DZS_Instance inst)
	{
		string g = DZS_ActiveGroup.Get(inst);
		if (g == "")
		{
			if (m_Kernel.GetGroupMaker())
				return true;
			m_Kernel.GetScreen().ShowMessage("brush.group", "warning", DZS_Texts.Get("brush.noGroup"), 2);
			return false;
		}
		string name;
		inst.GetField(g, DZS_Obj.GROUP_NAME, name);
		if (inst.IsLocked(g))
		{
			m_Kernel.GetScreen().ShowMessage("brush.group", "warning", DZS_Texts.Fmt("brush.groupLocked", name), 2);
			return false;
		}
		if (inst.IsHidden(g))
		{
			m_Kernel.GetScreen().ShowMessage("brush.group", "warning", DZS_Texts.Fmt("brush.groupHidden", name), 2);
			return false;
		}
		return true;
	}

	protected void StampIfMoved(DZS_Instance inst, vector c)
	{
		if (m_Centers.Count() == 0)
		{
			Stamp(inst, c);
			return;
		}
		vector last = m_Centers[m_Centers.Count() - 1];
		float dx = c[0] - last[0];
		float dz = c[2] - last[2];
		if (Math.Sqrt(dx * dx + dz * dz) >= m_Radius * 0.5)
			Stamp(inst, c);
	}

	protected bool Covered(float x, float z)
	{
		for (int i = 0; i < m_Centers.Count(); i++)
		{
			vector p = m_Centers[i];
			float dx = x - p[0];
			float dz = z - p[2];
			float r = m_CenterR[i];
			if (dx * dx + dz * dz <= r * r)
				return true;
		}
		return false;
	}

	// One stamp: the part of the circle no earlier stamp of this stroke covered gets density × area / 100 m²
	// objects; the rest after the comma goes on to the next stamp.
	protected void Stamp(DZS_Instance inst, vector c)
	{
		float r = m_Radius;
		int samples = 600;
		int free = 0;
		float golden = Math.PI * (3 - Math.Sqrt(5));
		for (int i = 0; i < samples; i++)
		{
			float rr = r * Math.Sqrt((i + 0.5) / samples);
			float a = i * golden;
			if (!Covered(c[0] + rr * Math.Cos(a), c[2] + rr * Math.Sin(a)))
				free++;
		}
		float area = Math.PI * r * r * free / samples;
		float want = m_Density * area / 100 + m_Carry;
		int n = Math.Round(want);
		m_Carry = want - n;
		array<DZS_BrushEntry> usable = m_StrokeSet.Usable(true);
		int made = 0;
		for (int k = 0; k < n && usable.Count() > 0; k++)
		{
			for (int t = 0; t < 400; t++)
			{
				float pr = r * Math.Sqrt(Math.RandomFloat01());
				float pa = Math.RandomFloat01() * Math.PI2;
				float px = c[0] + pr * Math.Cos(pa);
				float pz = c[2] + pr * Math.Sin(pa);
				if (Covered(px, pz))
					continue;
				if (PlaceOne(inst, px, pz, usable))
					made++;
				break;
			}
		}
		m_Centers.Insert(c);
		m_CenterR.Insert(r);
		DZS_Log.Info("brush: stamp " + m_Centers.Count() + " at " + c.ToString() + " new area " + DZS_Num.Fixed(area, 1, true) + " want " + DZS_Num.Fixed(want, 2, true) + " made " + made + " total " + m_Items.Count());
	}

	protected DZS_BrushEntry PickEntry(array<DZS_BrushEntry> usable)
	{
		int total = 0;
		foreach (DZS_BrushEntry e : usable)
			total += e.m_Share;
		float pick = Math.RandomFloat01() * total;
		foreach (DZS_BrushEntry e2 : usable)
		{
			pick -= e2.m_Share;
			if (pick < 0)
				return e2;
		}
		return usable[usable.Count() - 1];
	}

	// All chance is decided here and stays as fixed values.
	protected bool PlaceOne(DZS_Instance inst, float x, float z, array<DZS_BrushEntry> usable)
	{
		if (GetGame().SurfaceY(x, z) < GetGame().SurfaceGetSeaLevel())
			return false;
		vector ground, normal;
		if (!GroundAt(x, z, ground, normal))
		{
			DZS_Log.Info("brush: no ground at " + x + " " + z);
			return false;
		}
		DZS_BrushEntry e = PickEntry(usable);
		float yaw = 0;
		if (e.m_RandomYaw)
			yaw = Math.RandomFloat01() * 360;
		float pitch = 0;
		float roll = 0;
		if (e.m_Tilt > 0)
		{
			pitch = Math.RandomFloatInclusive(-e.m_Tilt, e.m_Tilt);
			roll = Math.RandomFloatInclusive(-e.m_Tilt, e.m_Tilt);
		}
		float scale = e.m_ScaleMin;
		if (e.m_ScaleMax > e.m_ScaleMin)
			scale = Math.RandomFloatInclusive(e.m_ScaleMin, e.m_ScaleMax);
		vector ypr = Vector(DZS_PlaceMath.Norm180(yaw), pitch, roll);
		vector rx, ux, fx;
		DZS_PlaceMath.Axes(ypr, rx, ux, fx);
		if (e.m_Slope && normal.Length() > 0.5)
		{
			vector n = normal.Normalized();
			float ang = Math.Acos(Math.Clamp(n[1], -1, 1)) * Math.RAD2DEG;
			vector upv = "0 1 0";
			vector axis = upv * n;
			if (axis.Length() > 0.0001 && ang > 0.0001)
			{
				axis.Normalize();
				float use = Math.Min(ang, e.m_SlopeMax);
				rx = DZS_PlaceMath.Rot(rx, axis, use);
				ux = DZS_PlaceMath.Rot(ux, axis, use);
				fx = DZS_PlaceMath.Rot(fx, axis, use);
				ypr = DZS_PlaceMath.Angles(rx, ux, fx);
				DZS_PlaceMath.Axes(ypr, rx, ux, fx);
			}
		}
		vector bmin = "-0.25 0 -0.25";
		vector bmax = "0.25 0.5 0.25";
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (scene)
			scene.GetBox(e.m_Name, bmin, bmax);
		float cx = (bmin[0] + bmax[0]) * 0.5;
		float cz = (bmin[2] + bmax[2]) * 0.5;
		vector footLocal = rx * (cx * scale) + ux * (bmin[1] * scale) + fx * (cz * scale);
		vector foot = Vector(x, ground[1] + e.m_Offset, z);
		DZS_BrushItem it = new DZS_BrushItem();
		it.m_Name = e.m_Name;
		it.m_Pos = foot - footLocal;
		it.m_Ypr = ypr;
		it.m_Scale = scale;
		// the shown model and the operation get the same float32 values
		m_Items.Insert(it);
		return true;
	}

	// Objects of the project an eraser stroke would take at this point. The objects of the
	// set's kinds that are neither hidden nor locked are listed with their foot points once per stroke (and every
	// 0.5 s otherwise); each frame then only compares numbers.
	protected ref array<string> m_IdxIds = new array<string>();
	protected ref array<vector> m_IdxFoot = new array<vector>();
	protected float m_IdxAt = -100;
	protected DZS_BrushSet m_IdxSet;
	protected DZS_Instance m_IdxInst;

	protected void BuildIndex(DZS_Instance inst, DZS_BrushSet s)
	{
		m_IdxIds.Clear();
		m_IdxFoot.Clear();
		m_IdxAt = m_Time;
		m_IdxSet = s;
		m_IdxInst = inst;
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			bool usable = inst.GetKind(g) == DZS_KIND_GROUP && !inst.IsHidden(g) && !inst.IsLocked(g);
			if (usable)
				IndexGroup(inst, s, g);
		}
	}

	protected void IndexGroup(DZS_Instance inst, DZS_BrushSet s, string g)
	{
		array<string> ids = inst.GetChildren(g);
		foreach (string id : ids)
		{
			bool free = !inst.IsHidden(id) && !inst.IsLocked(id) && inst.GetKind(id) == DZS_KIND_OBJECT;
			if (free)
			{
				string name, pos;
				inst.GetField(id, DZS_Obj.NAME, name);
				if (s.HasName(name))
				{
					inst.GetField(id, DZS_Obj.POS, pos);
					vector foot = FootOf(inst, id, pos);
					m_IdxIds.Insert(id);
					m_IdxFoot.Insert(foot);
				}
			}
		}
	}

	protected void Candidates(DZS_Instance inst, vector c, array<string> into)
	{
		DZS_BrushSet s = GroundSet();
		if (!s)
			return;
		if (m_IdxAt < 0 || m_IdxInst != inst || m_IdxSet != s || (!m_Stroke && m_Time - m_IdxAt > 0.5))
			BuildIndex(inst, s);
		float r = m_Radius;
		float r2 = r * r;
		float up = Math.Max(r, 12);
		int n = m_IdxIds.Count();
		for (int i = 0; i < n; i++)
		{
			vector foot = m_IdxFoot[i];
			float dx = foot[0] - c[0];
			float dz = foot[2] - c[2];
			if (dx * dx + dz * dz <= r2 && Math.AbsFloat(foot[1] - c[1]) <= up)
			{
				string id = m_IdxIds[i];
				if (!m_ErasedSet.Contains(id))
					into.Insert(id);
			}
		}
	}

	protected void Corners(DZS_Instance inst, string id, array<vector> c, out vector foot)
	{
		string name, pos, ypr, scale;
		inst.GetField(id, DZS_Obj.NAME, name);
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		float s = 1;
		if (scale != "")
			s = scale.ToFloat();
		vector p = DZS_Obj.QuickVec(pos);
		vector rx, ux, fx;
		DZS_PlaceMath.Axes(DZS_Obj.QuickVec(ypr), rx, ux, fx);
		vector bmin = "-0.25 0 -0.25";
		vector bmax = "0.25 0.5 0.25";
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (scene)
			scene.GetBox(name, bmin, bmax);
		c.Clear();
		for (int i = 0; i < 8; i++)
		{
			float lx = bmin[0];
			if ((i & 1) != 0)
				lx = bmax[0];
			float ly = bmin[1];
			if ((i & 2) != 0)
				ly = bmax[1];
			float lz = bmin[2];
			if ((i & 4) != 0)
				lz = bmax[2];
			c.Insert(p + rx * (lx * s) + ux * (ly * s) + fx * (lz * s));
		}
		foot = p + rx * ((bmin[0] + bmax[0]) * 0.5 * s) + ux * (bmin[1] * s) + fx * ((bmin[2] + bmax[2]) * 0.5 * s);
	}

	// Foot points by id, valid while the position text stays the same.
	protected ref map<string, vector> m_FootCache = new map<string, vector>();
	protected ref map<string, string> m_FootPos = new map<string, string>();

	protected vector FootOf(DZS_Instance inst, string id, string pos)
	{
		string was;
		if (m_FootPos.Find(id, was) && was == pos)
			return m_FootCache.Get(id);
		array<vector> c = new array<vector>();
		vector foot;
		Corners(inst, id, c, foot);
		m_FootPos.Set(id, pos);
		m_FootCache.Set(id, foot);
		return foot;
	}

	// In every frame of an eraser stroke, all objects now under the circle vanish (view until the release).
	protected void EraseAt(DZS_Instance inst, vector c)
	{
		array<string> ids = new array<string>();
		Candidates(inst, c, ids);
		foreach (string id : ids)
		{
			m_Erased.Insert(id);
			m_ErasedSet.Set(id, true);
		}
		HideErased();
	}

	// Erased models vanish at most 25 per frame (deleting hundreds at once stops the game for a moment).
	protected int m_Hidden;

	protected void HideErased()
	{
		DZS_SceneApi scene = DZS_SceneApi.Get();
		int n = 0;
		while (m_Hidden < m_Erased.Count() && n < 25)
		{
			if (scene)
				scene.HideView(m_Erased[m_Hidden]);
			m_Hidden++;
			n++;
		}
	}

	protected void EndStroke(DZS_Instance inst)
	{
		m_Stroke = false;
		m_LogNextDt = true;
		m_Kernel.SetGesture("brush", false);
		if (m_StrokeErase)
			CommitErase(inst);
		else
			CommitSet(inst);
	}

	void CancelStroke(string why)
	{
		if (!m_Stroke)
			return;
		m_Stroke = false;
		m_Kernel.SetGesture("brush", false);
		DZS_SceneApi scene = DZS_SceneApi.Get();
		foreach (DZS_BrushItem it : m_Items)
		{
			if (it.m_Obj)
				GetGame().ObjectDelete(it.m_Obj);
		}
		for (int i = 0; i < m_Hidden && i < m_Erased.Count(); i++)
		{
			if (scene)
				scene.ClearView(m_Erased[i]);
		}
		DZS_Log.Info("brush: stroke cancelled (" + why + "), " + m_Items.Count() + " objects and " + m_Erased.Count() + " erased given back");
		m_Items.Clear();
		m_Erased.Clear();
		m_ErasedSet.Clear();
	}

	protected string Shown(string name)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat)
			return cat.ShownName(name);
		return name;
	}

	protected void CommitSet(DZS_Instance inst)
	{
		int n = m_Items.Count();
		if (n == 0)
		{
			DZS_Log.Info("brush: stroke without objects, no operation");
			return;
		}
		int t0 = TickCount(0);
		DZS_SceneApi scene = DZS_SceneApi.Get();
		DZS_OpBuilder b = inst.BeginOperation("brush:edit", 1);
		string g = DZS_ActiveGroup.ForNewObjects(inst, b);
		foreach (DZS_BrushItem it : m_Items)
		{
			Finalize(it);
			map<string, string> f = new map<string, string>();
			f.Set(DZS_Obj.NAME, it.m_Name);
			f.Set(DZS_Obj.POS, it.m_PosText);
			f.Set(DZS_Obj.YPR, it.m_YprText);
			f.Set(DZS_Obj.SCALE, it.m_ScaleText);
			f.Set(DZS_Obj.PERSIST, "0");
			f.Set(DZS_Obj.CUSTOM, "");
			string nid = b.Create(DZS_KIND_OBJECT, g, DZS_END, f);
			if (it.m_Obj && DZS_Obj.IsModelPath(it.m_Name) && scene)
			{
				scene.Adopt(nid, it.m_Obj);
				it.m_Obj = null;
			}
		}
		string de, en;
		if (n == 1)
		{
			string shown = Shown(m_Items[0].m_Name);
			de = DZS_Texts.FmtIn("brush.op.paintOne", true, shown);
			en = DZS_Texts.FmtIn("brush.op.paintOne", false, shown);
		}
		else
		{
			de = DZS_Texts.FmtIn("brush.op.paint", true, n.ToString());
			en = DZS_Texts.FmtIn("brush.op.paint", false, n.ToString());
		}
		int tBuild = TickCount(t0);
		DZS_SubmitResult r = b.Commit(de, en);
		int tCommit = TickCount(t0);
		foreach (DZS_BrushItem it2 : m_Items)
		{
			if (it2.m_Obj)
				GetGame().ObjectDelete(it2.m_Obj);
		}
		int ticks = TickCount(t0);
		DZS_Log.Info("brush: ticks build " + tBuild + " commit " + tCommit + " delete " + ticks);
		DZS_Log.Info("brush: committed " + n + " objects ok " + DZS_Str.Bool(r.Ok()) + " group " + g + " ticks " + ticks + " min fps " + Math.Round(m_MinFps));
		for (int i = 0; i < n && i < 3; i++)
			DZS_Log.Info("brush: object " + m_Items[i].m_Name + " pos " + m_Items[i].m_Pos.ToString() + " ypr " + m_Items[i].m_Ypr.ToString() + " scale " + m_Items[i].m_Scale);
		m_Items.Clear();
	}

	protected void CommitErase(DZS_Instance inst)
	{
		int n = m_Erased.Count();
		if (n == 0)
		{
			DZS_Log.Info("brush: erase stroke hit nothing, no operation");
			return;
		}
		string firstName;
		inst.GetField(m_Erased[0], DZS_Obj.NAME, firstName);
		DZS_OpBuilder b = inst.BeginOperation("brush:edit", 1);
		foreach (string id : m_Erased)
			b.Delete(id);
		string de, en;
		if (n == 1)
		{
			string shown = Shown(firstName);
			de = DZS_Texts.FmtIn("brush.op.eraseOne", true, shown);
			en = DZS_Texts.FmtIn("brush.op.eraseOne", false, shown);
		}
		else
		{
			de = DZS_Texts.FmtIn("brush.op.erase", true, n.ToString());
			en = DZS_Texts.FmtIn("brush.op.erase", false, n.ToString());
		}
		DZS_SubmitResult r = b.Commit(de, en);
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel)
			sel.Remove(m_Erased);
		DZS_Log.Info("brush: erased " + n + " objects ok " + DZS_Str.Bool(r.Ok()) + " min fps " + Math.Round(m_MinFps));
		m_Erased.Clear();
		m_ErasedSet.Clear();
	}

	// ---- drawing and hints ----

	protected void ClearView()
	{
		if (m_Canvas)
			m_Canvas.Clear();
		if (m_Hint)
			m_Hint.Hide();
		m_Kernel.SetFramesTaken(null);
	}

	protected void Draw(DZS_Instance inst, bool erase)
	{
		int col = DZS_Color.Get("accent");
		if (erase)
			col = DZS_Color.Get("danger");
		float w2 = Math.Max(2, DZS_Scale.Px(2));
		float w4 = Math.Max(4, DZS_Scale.Px(4));
		if (m_HasHit)
		{
			vector prev;
			bool havePrev = false;
			for (int i = 0; i <= 64; i++)
			{
				float a = i * Math.PI2 / 64;
				float x = m_Hit[0] + m_Radius * Math.Cos(a);
				float z = m_Hit[2] + m_Radius * Math.Sin(a);
				vector g, n;
				vector p;
				if (CircleGround(x, z, g))
					p = Vector(x, g[1] + 0.05, z);
				else
					p = Vector(x, m_Hit[1], z);
				if (havePrev)
					Seg(prev, p, w2, col);
				prev = p;
				havePrev = true;
			}
			float sx, sy;
			if (DZS_PlaceMath.ToScreen(m_Hit, sx, sy))
			{
				float cs = DZS_Scale.Px(6);
				DZS_Clip.Line(m_Canvas, sx - cs, sy, sx + cs, sy, w2, col);
				DZS_Clip.Line(m_Canvas, sx, sy - cs, sx, sy + cs, w2, col);
			}
		}
		if (m_Marked.Count() == 0)
			return;
		int danger = DZS_Color.Get("danger");
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		string lead = "";
		if (sel)
			lead = sel.GetLead();
		array<vector> c = new array<vector>();
		vector foot;
		if (m_Marked.Count() <= 20)
		{
			foreach (string id : m_Marked)
			{
				Corners(inst, id, c, foot);
				float w = w2;
				if (id == lead && sel.IsSelected(id))
					w = w4;
				DZS_WorldLines.Frame("e:" + id, c, w, danger, 0, DZS_WorldLines.PRIO_NEW);
			}
			DZS_WorldLines.Flush(m_Canvas);
			return;
		}
		// more than 20 under the eraser: one frame around them and a dot per object
		array<vector> all = new array<vector>();
		array<vector> centres = new array<vector>();
		foreach (string mid : m_Marked)
		{
			Corners(inst, mid, c, foot);
			vector sum = "0 0 0";
			foreach (vector v : c)
			{
				all.Insert(v);
				sum = sum + v;
			}
			centres.Insert(sum * 0.125);
		}
		array<vector> around = new array<vector>();
		DZS_WorldLines.Around(all, around);
		DZS_WorldLines.Frame("e:big", around, w4, danger, 0, DZS_WorldLines.PRIO_NOW);
		DZS_WorldLines.Flush(m_Canvas);
		float d6 = DZS_Scale.Px(6);
		float d10 = DZS_Scale.Px(10);
		for (int k = 0; k < centres.Count(); k++)
		{
			float dd = d6;
			if (m_Marked[k] == lead && sel.IsSelected(lead))
				dd = d10;
			DZS_WorldLines.Dot(m_Canvas, centres[k], dd, danger, false);
		}
	}

	protected bool CircleGround(float x, float z, out vector g)
	{
		if (!m_OnObjects)
		{
			g = Vector(x, GetGame().SurfaceY(x, z), z);
			return true;
		}
		vector n;
		return GroundAt(x, z, g, n);
	}

	protected void Seg(vector a, vector b, float w, int col)
	{
		float x1, y1, x2, y2;
		if (DZS_PlaceMath.Segment(a, b, x1, y1, x2, y2))
			DZS_Clip.Line(m_Canvas, x1, y1, x2, y2, w, col);
	}

	protected void Hints(DZS_Instance inst, bool erase, bool over)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		string key = m_Kernel.GetUi().FirstBinding("brush.toggle");
		if (erase)
			scr.SetHint("brush", 30, DZS_Texts.Fmt("brush.context.erase", key));
		else
			scr.SetHint("brush", 30, DZS_Texts.Fmt("brush.context.set", key));
		if (!over || !m_HasHit || scr.IsUiHidden())
		{
			m_Hint.Hide();
			return;
		}
		DZS_BrushSet s = CurrentSet();
		if (m_Stroke)
			s = m_StrokeSet;
		string setName = "—";
		if (s)
			setName = s.Name();
		string title;
		array<string> lines = new array<string>();
		string radius = DZS_Texts.Fmt("brush.hint.radius", DZS_Num.Fixed(m_Radius, 1));
		if (erase)
		{
			title = DZS_Texts.Fmt("brush.hint.eraseTitle", setName);
			lines.Insert(DZS_Texts.Get("brush.hint.dragErase"));
			lines.Insert(DZS_Texts.Get("brush.hint.ctrlSet"));
			lines.Insert(radius);
			if (m_Stroke)
				lines.Insert(DZS_Texts.Fmt("brush.hint.removed", m_Erased.Count().ToString()));
			else
				lines.Insert(DZS_Texts.Fmt("brush.hint.inCircle", m_Marked.Count().ToString()));
		}
		else
		{
			title = DZS_Texts.Fmt("brush.hint.title", setName, DZS_ActiveGroup.Name(inst));
			lines.Insert(DZS_Texts.Get("brush.hint.drag"));
			lines.Insert(radius);
			lines.Insert(DZS_Texts.Fmt("brush.hint.density", DZS_Num.Fixed(m_Density, 1)));
			if (s && s.Usable(true).Count() == 0 && s.Usable(false).Count() > 0)
				lines.Insert(DZS_Texts.Get("brush.hint.allZero"));
			else if (m_Stroke)
				lines.Insert(DZS_Texts.Fmt("brush.hint.objects", m_Items.Count().ToString()));
			else
			{
				int per = Math.Round(m_Density * Math.PI * m_Radius * m_Radius / 100);
				lines.Insert(DZS_Texts.Fmt("brush.hint.perStamp", per.ToString()));
			}
		}
		int mx, my;
		GetMousePos(mx, my);
		m_Hint.Show(title, lines, mx, my, true);
	}
}

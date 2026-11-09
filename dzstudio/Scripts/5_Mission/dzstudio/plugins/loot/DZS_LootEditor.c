// Loot editing at a building of the scene: banner, other objects
// locked, point selection, setting, dragging, deleting and duplicating points, the section "Loot entry". Every
// change is one operation "Change loot ‹type›" on the type's element; the first change of an entry "as vanilla"
// asks first (first intervention). Entering, leaving, selecting points and the active container are view.
class DZS_LootEsc : DZS_EscHandler
{
	DZS_LootEditor m_E;
	int m_Level;

	override bool IsEscActive()
	{
		if (m_Level == 0)
			return m_E.IsDragging();
		if (m_Level == 1)
			return m_E.IsOn() && m_E.m_Sel.Count() > 0;
		return m_E.IsOn();
	}

	override void OnEsc()
	{
		if (m_Level == 0)
			m_E.CancelDrag();
		else if (m_Level == 1)
			m_E.ClearPoints();
		else
			m_E.Stop("");
	}
}

class DZS_LootOverride : DZS_ActionOverride
{
	DZS_LootEditor m_E;

	override bool Claims(string actionId)
	{
		if (!m_E.IsOn())
			return false;
		if (actionId == "placement.tool.select" || actionId == "placement.tool.move")
			return false;
		return actionId.IndexOf("placement.") == 0 || actionId == "brush.toggle" || actionId == "loot.edit" || actionId == "properties.details" || actionId == "catalog.saveTemplate";
	}

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (actionId == "placement.delete" || actionId == "placement.duplicate")
		{
			if (m_E.m_Sel.Count() == 0)
			{
				reasonDe = "Erst Punkte auswählen";
				reasonEn = "Select points first";
				return false;
			}
			return true;
		}
		if (actionId == "placement.selectAll" || actionId == "placement.copy" || actionId == "placement.tab")
			return actionId != "placement.copy";
		reasonDe = "Während der Loot-Bearbeitung nicht verfügbar";
		reasonEn = "Not available while editing loot";
		return false;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "placement.delete")
			m_E.DeletePoints();
		else if (actionId == "placement.duplicate")
			m_E.DuplicatePoints();
		else if (actionId == "placement.selectAll")
			m_E.SelectAllPoints();
		else if (actionId == "placement.tab")
			m_E.TabStep();
	}
}

class DZS_LootEditor : Managed
{
	static const string PANEL = "loot.entry";
	static const string TOOL = "loot";
	// rounded median of the lootFloor points in the vanilla mapgroupproto.xml of Chernarus (3974 points: 0.84 / 1.99 m)
	static const float NEW_RANGE = 0.8;
	static const float NEW_HEIGHT = 2.0;

	DZS_LootPlugin m_P;
	protected DZS_Kernel m_Kernel;
	protected bool m_On;
	protected bool m_World;
	string m_Obj;
	// a building of the map instead of a project object
	Object m_MapObj;
	string m_Type;
	// the entry as shown: the effective one, or a working copy during a gesture
	ref DZS_LootEEntry m_Entry;
	protected bool m_Working;
	ref map<string, bool> m_Sel = new map<string, bool>();
	int m_Active;
	string m_Hover;
	int m_Gen;
	protected int m_EntryVer = -1;
	protected ref DZS_LootEntryPanel m_Panel;
	protected ref DZS_LootEsc m_EscDrag;
	protected ref DZS_LootEsc m_EscSel;
	protected ref DZS_LootEsc m_EscStop;
	protected ref DZS_LootOverride m_Override;
	protected bool m_LmbWas;
	protected float m_Time;
	protected float m_LastClick = -10;
	protected float m_LastClickX;
	protected float m_LastClickY;
	protected bool m_Press;
	protected bool m_Drag;
	protected float m_DragLift;
	protected vector m_DragTarget;
	protected bool m_DragHasTarget;
	protected float m_PressX;
	protected float m_PressY;
	protected string m_PressKey;
	protected ref DZS_LootEEntry m_DragStart;
	protected vector m_GrabWorld;
	protected ref DZS_LootEEntry m_PendingEntry;
	protected string m_PendingSel;
	protected int m_PendingActive;
	float m_LastRange = -1;
	float m_LastHeight = -1;
	protected bool m_Rect;
	protected int m_TabDepth;
	protected float m_TabX;
	protected float m_TabY;
	protected bool m_KeyGesture;
	protected vector m_KeyMove;
	protected float m_KeyFactor = 1;
	protected ref DZS_LootEEntry m_KeyStart;
	protected bool m_KeyNeedRelease;
	protected int m_Handle = -1;
	protected vector m_HandleC;
	protected vector m_HandleA;
	protected float m_HandleT0;
	protected ref DZS_LootEEntry m_HandleStart;
	protected vector m_HandleCenter;
	protected ref array<vector> m_HandleTips = new array<vector>();
	// test items by point key (view only)
	protected ref map<string, Object> m_Tests = new map<string, Object>();

	void DZS_LootEditor(DZS_LootPlugin p, DZS_Kernel k)
	{
		m_P = p;
		m_Kernel = k;
	}

	void Register()
	{
		DZS_UiRegistry ui = m_Kernel.GetUi();
		m_Panel = new DZS_LootEntryPanel(this);
		ui.RegisterPanel("loot", PANEL, "Loot-Eintrag", "Loot entry", "right", "package", m_Panel, 850);
		ui.SetPanelShown(PANEL, false);
		m_Override = new DZS_LootOverride();
		m_Override.m_E = this;
		DZS_Texts.Add("loot.banner", "Loot: %1 · betrifft %2 auf der Karte und %3 im Projekt · Esc beendet", "Loot: %1 · affects %2 on the map and %3 in the project · Esc ends");
		DZS_Texts.Add("loot.ended", "Loot-Bearbeitung beendet: Gebäude bzw. Eintrag nicht mehr da", "Loot editing ended: building or entry gone");
		DZS_Texts.Add("loot.step", "Loot %1 ändern", "Change loot %1");
		DZS_Texts.Add("loot.stepOwn", "Loot %1: eigene Punkte", "Loot %1: Custom points");
		DZS_Texts.Add("loot.stepReset", "Loot %1 zurücksetzen", "Reset loot %1");
		DZS_Texts.Add("loot.stepDelete", "Loot %1 löschen", "Delete loot %1");
		DZS_Texts.Add("loot.hint.none", "Klick Punkt wählen · Doppelklick neuer Punkt · [Esc] fertig", "Click selects a point · Double-click new point · [Esc] done");
		DZS_Texts.Add("loot.hint.sel", "Klick Punkt wählen · Ziehen verschieben · Doppelklick neuer Punkt · [%1] löschen · [Esc] Auswahl aufheben", "Click selects a point · Drag moves · Double-click new point · [%1] delete · [Esc] deselect");
		DZS_Texts.Add("loot.firstTitle", "Eigene Punkte", "Own points");
		DZS_Texts.Add("loot.firstQ", "Eigene Punkte für %1? Änderungen gelten für %2 auf der Karte und %3 im Projekt. Alle Objekte dieses Typs mit Loot stellen auf „eigene Punkte“.", "Own points for %1? Changes apply to %2 on the map and %3 in the project. All objects of this type with loot switch to \"own points\".");
		DZS_Texts.Add("loot.firstGo", "Eigene Punkte anlegen", "Create own points");
		DZS_Texts.Add("loot.resetQ", "Änderungen dieses Projekts an %1 verwerfen? Der Eintrag wird wieder wie vor dem Projekt; das betrifft %2 auf der Karte und %3 im Projekt.", "Discard this project’s changes to %1? The entry becomes as before the project again; this affects %2 on the map and %3 in the project.");
		DZS_Texts.Add("loot.vanSetQ", "%1 auf den Vanilla-Stand setzen? Der Eintrag bekommt Inhalt und Text aus Vanilla und ist danach von diesem Projekt; Zurücksetzen bringt den vorher angepassten Stand zurück. Das betrifft %2 auf der Karte und %3 im Projekt.", "Set %1 to the vanilla state? The entry gets content and text from vanilla and is then from this project; resetting brings the state customised before back. This affects %2 on the map and %3 in the project.");
		DZS_Texts.Add("loot.vanSetGo", "Auf Vanilla setzen", "Set to vanilla");
		DZS_Texts.Add("loot.stepVanilla", "Loot %1 auf Vanilla setzen", "Set loot %1 to vanilla");
		DZS_Texts.Add("loot.resetGo", "Zurücksetzen", "Reset");
	}

	void OnSessionStart(bool world)
	{
		m_World = world;
		if (!world)
			return;
		DZS_Screen scr = m_Kernel.GetScreen();
		m_EscDrag = new DZS_LootEsc();
		m_EscDrag.m_E = this;
		m_EscDrag.m_Level = 0;
		scr.AddEscHandler(m_EscDrag, 41);
		m_EscSel = new DZS_LootEsc();
		m_EscSel.m_E = this;
		m_EscSel.m_Level = 1;
		scr.AddEscHandler(m_EscSel, 88);
		m_EscStop = new DZS_LootEsc();
		m_EscStop.m_E = this;
		m_EscStop.m_Level = 2;
		scr.AddEscHandler(m_EscStop, 89);
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel)
			sel.AddBarButton("loot.edit", "package", "Loot bearbeiten", "Edit loot", 70, true);
	}

	bool IsOn() { return m_On; }
	bool IsDragging() { return m_Drag || m_KeyGesture || m_Handle >= 0 || m_Rect; }

	bool CanStart(out string reasonDe, out string reasonEn)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return false;
		string lh, ll;
		inst.GetField(DZS_PROJECT_ID, "lootHidden", lh);
		inst.GetField(DZS_PROJECT_ID, "lootLocked", ll);
		if (lh == "true")
		{
			reasonDe = "Loot-Gruppe ist ausgeblendet";
			reasonEn = "Loot group is hidden";
			return false;
		}
		if (ll == "true")
		{
			reasonDe = "Loot-Gruppe ist gesperrt";
			reasonEn = "Loot group is locked";
			return false;
		}
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat && cat.GetHand() != "")
		{
			reasonDe = "Objekt in der Hand";
			reasonEn = "Object in the hand";
			return false;
		}
		return true;
	}

	void StartAtObject(string id)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return;
		string name;
		inst.GetField(id, DZS_Obj.NAME, name);
		Start(inst, id, null, name);
	}

	void StartAtMap(Object o)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !o)
			return;
		string name = o.GetType();
		Start(inst, "", o, name);
	}

	protected void Start(DZS_Instance inst, string id, Object mapObj, string name)
	{
		if (m_On)
			Stop("");
		m_MapObj = mapObj;
		DZS_LootEEntry e = m_P.Effective(inst, name);
		if (!e)
		{
			e = m_P.StartOwn(name);
			e.m_Containers.Clear();
		}
		DZS_BrushPlugin brush = DZS_BrushPlugin.Cast(m_Kernel.GetPlugin("brush"));
		if (brush && brush.m_On)
			brush.SetOn(false);
		m_On = true;
		m_Obj = id;
		m_Type = name;
		m_Sel.Clear();
		m_Active = 0;
		m_Working = false;
		Refresh(inst);
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel)
		{
			array<string> none = new array<string>();
			sel.SetSelection(none);
		}
		m_Kernel.SetSceneTool(TOOL);
		m_Kernel.ClaimSceneState(true);
		m_Kernel.GetUi().SetOverride(m_Override);
		m_Kernel.GetUi().SetPanelShown(PANEL, true);
		m_Kernel.GetUi().SetPanelShown("properties.panel", false);
		UpdateBanner(inst);
		DZS_WorldLines.Moved();
		string at = id;
		if (mapObj)
			at = "map " + mapObj.GetPosition().ToString();
		DZS_Log.Info("loot: editing " + name + " at " + at + ", " + m_Entry.CountPoints() + " points, status " + m_P.Status(inst, name));
	}

	void Stop(string why)
	{
		if (!m_On)
			return;
		CancelDrag();
		RemoveTests();
		HideWarnings(0);
		m_On = false;
		m_Kernel.SetSceneTool("");
		m_Kernel.ClaimSceneState(false);
		m_Kernel.GetUi().SetSceneState("none");
		m_Kernel.GetUi().SetOverride(null);
		m_Kernel.GetUi().SetPanelShown(PANEL, false);
		m_Kernel.GetUi().SetPanelShown("properties.panel", true);
		m_Kernel.GetScreen().HideBanner("loot.edit");
		m_Kernel.GetScreen().SetHint("loot", 30, "");
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel && inst && m_Obj != "" && inst.Has(m_Obj))
		{
			array<string> one = {m_Obj};
			sel.SetSelection(one, m_Obj);
		}
		m_Sel.Clear();
		m_MapObj = null;
		if (why != "")
			m_Kernel.GetScreen().ShowMessage("loot.ended", "warning", why, 5.0);
		DZS_Log.Info("loot: editing ended " + why);
	}

	// The entry from the state again (after an operation, an undo or a save).
	protected void Refresh(DZS_Instance inst)
	{
		DZS_LootEEntry e = m_P.Effective(inst, m_Type);
		if (!e)
		{
			e = new DZS_LootEEntry();
			e.m_Name = m_Type;
		}
		m_Entry = e;
		m_Working = false;
		m_Gen++;
		if (m_Active >= m_Entry.m_Containers.Count())
			m_Active = 0;
		array<string> drop = new array<string>();
		for (int i = 0; i < m_Sel.Count(); i++)
		{
			string k = m_Sel.GetKey(i);
			if (!PointOf(k))
				drop.Insert(k);
		}
		foreach (string d : drop)
			m_Sel.Remove(d);
		array<string> goneTests = new array<string>();
		for (int t = 0; t < m_Tests.Count(); t++)
		{
			string tk = m_Tests.GetKey(t);
			if (!PointOf(tk))
				goneTests.Insert(tk);
		}
		foreach (string gt : goneTests)
			RemoveTest(gt);
	}

	void OnStateChanged(DZS_Instance inst)
	{
		if (!m_On || !inst)
			return;
		if (!m_MapObj && !inst.Has(m_Obj))
		{
			Stop(DZS_Texts.Get("loot.ended"));
			return;
		}
		string name = m_Type;
		if (!m_MapObj)
			inst.GetField(m_Obj, DZS_Obj.NAME, name);
		string a = name;
		a.ToLower();
		string b = m_Type;
		b.ToLower();
		if (a != b || !m_P.IsOn())
		{
			Stop(DZS_Texts.Get("loot.ended"));
			return;
		}
		if (m_Drag)
			return;
		Refresh(inst);
		UpdateBanner(inst);
	}

	protected void UpdateBanner(DZS_Instance inst)
	{
		int pts, mapRows, inProj;
		m_P.Facts(inst, m_Type, pts, mapRows, inProj);
		string mapText = DZS_LootPlugin.Plural(mapRows, "Gebäude", "Gebäude", "building", "buildings");
		string projText = DZS_LootPlugin.Plural(inProj, "Objekt", "Objekte", "object", "objects");
		string shown = m_P.ShownName(m_Type);
		m_Kernel.GetScreen().ShowBanner("loot.edit", 10, "info", DZS_Texts.Fmt("loot.banner", shown, mapText, projText));
	}

	string StepName(string key, bool de)
	{
		string shown = m_P.ShownName(m_Type);
		return DZS_Texts.FmtIn(key, de, shown);
	}

	// ---- the building ----

	bool Frame(out vector origin, out vector ypr)
	{
		if (m_MapObj)
		{
			origin = m_MapObj.GetPosition();
			ypr = m_MapObj.GetOrientation();
			return true;
		}
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !inst.Has(m_Obj))
			return false;
		float scale;
		vector vp, vy;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (scene && scene.GetView(m_Obj, vp, vy, scale))
		{
			origin = vp;
			ypr = vy;
			return true;
		}
		string pos, yp;
		inst.GetField(m_Obj, DZS_Obj.POS, pos);
		inst.GetField(m_Obj, DZS_Obj.YPR, yp);
		origin = DZS_Obj.QuickVec(pos);
		ypr = DZS_Obj.QuickVec(yp);
		return true;
	}

	DZS_LootEPoint PointOf(string key)
	{
		int c, i;
		if (!Split(key, c, i) || !m_Entry)
			return null;
		if (c >= m_Entry.m_Containers.Count())
			return null;
		DZS_LootEContainer lc = m_Entry.m_Containers[c];
		if (i >= lc.m_Points.Count())
			return null;
		return lc.m_Points[i];
	}

	static string Key(int c, int i)
	{
		string a = c.ToString();
		string b = i.ToString();
		return a + ":" + b;
	}

	static bool Split(string key, out int c, out int i)
	{
		int colon = key.IndexOf(":");
		if (colon < 0)
			return false;
		string a = key.Substring(0, colon);
		string b = key.Substring(colon + 1, key.Length() - colon - 1);
		c = a.ToInt();
		i = b.ToInt();
		return true;
	}

	vector WorldOfPoint(DZS_LootEPoint p)
	{
		vector o, ypr, r, u, f;
		Frame(o, ypr);
		DZS_PlaceMath.Axes(ypr, r, u, f);
		return DZS_LootPlugin.WorldOf(o, r, u, f, p.P());
	}

	vector LootOfWorld(vector w)
	{
		vector o, ypr, r, u, f;
		Frame(o, ypr);
		DZS_PlaceMath.Axes(ypr, r, u, f);
		return DZS_LootPlugin.LootOf(o, r, u, f, w);
	}

	// ---- every frame ----

	void Update(float dt)
	{
		m_Time += dt;
		if (!m_On || !m_World)
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_Screen scr = m_Kernel.GetScreen();
		if (!inst || scr.IsHome())
		{
			Stop("");
			return;
		}
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat && cat.GetHand() != "")
			cat.DropHand();
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		array<string> objSel = null;
		if (sel)
			objSel = sel.GetSelection();
		if (objSel && objSel.Count() > 0)
		{
			array<string> none = new array<string>();
			sel.SetSelection(none);
		}
		if (m_Sel.Count() > 0)
			m_Kernel.GetUi().SetSceneState("selection");
		else
			m_Kernel.GetUi().SetSceneState("none");
		KeyGesture(dt);
		bool blocked = scr.HasDialog() || scr.HasKeyboardOwner("map") || scr.HasKeyboardOwner("menu") || scr.HasKeyboardOwner("fullsearch");
		bool over = !blocked && (!scr.IsCursorVisible() || scr.IsPointerOverScene());
		bool lmb = DZS_UiRegistry.KeyDown("MouseLeft");
		bool lDown = lmb && !m_LmbWas;
		bool lUp = !lmb && m_LmbWas;
		m_LmbWas = lmb;
		int mx, my;
		GetMousePos(mx, my);
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		if (!scr.IsCursorVisible())
			dir = GetGame().GetCurrentCameraDirection();
		if (Math.AbsFloat(mx - m_TabX) > 3 || Math.AbsFloat(my - m_TabY) > 3)
		{
			m_TabDepth = 0;
			m_TabX = mx;
			m_TabY = my;
		}
		m_Hover = "";
		if (over && !m_Drag && m_Handle < 0 && !m_Rect)
		{
			array<string> picks = PickPoints(from, dir);
			if (picks.Count() > 0)
				m_Hover = picks[m_TabDepth % picks.Count()];
		}
		if (lDown && over && !scr.IsMouseSwallowed() && !m_KeyGesture)
		{
			int hh = HandleAt(mx, my);
			if (hh >= 0)
				BeginHandle(hh, from, dir);
			else
			{
				bool dbl = m_Time - m_LastClick < 0.35 && Math.AbsFloat(mx - m_LastClickX) < 6 && Math.AbsFloat(my - m_LastClickY) < 6;
				m_LastClick = m_Time;
				m_LastClickX = mx;
				m_LastClickY = my;
				if (dbl && m_Hover == "")
				{
					m_LastClick = -10;
					SetPointAt(from, dir);
				}
				else
					Press(m_Hover, mx, my);
			}
		}
		if (m_Handle >= 0)
		{
			if (lmb)
				HandleTo(from, dir);
			else
				EndHandle();
		}
		bool pressMoved = Math.AbsFloat(mx - m_PressX) > 4 || Math.AbsFloat(my - m_PressY) > 4;
		if (m_Press && lmb && !m_Drag && !m_Rect && m_Handle < 0 && pressMoved)
		{
			if (m_PressKey != "")
				BeginDrag();
			else
				m_Rect = true;
		}
		if (m_Drag)
		{
			if (lmb)
				DragTo(from, dir);
			else
				EndDrag();
		}
		if (m_Rect && !lmb)
			EndRect(mx, my);
		if (lUp)
			m_Press = false;
		UpdateTests();
		string hint;
		if (m_Sel.Count() == 0)
			hint = DZS_Texts.Get("loot.hint.none");
		else
			hint = DZS_Texts.Fmt("loot.hint.sel", m_Kernel.GetUi().FirstBinding("placement.delete"));
		scr.SetHint("loot", 30, hint);
	}

	// All cylinders the ray meets, nearest first (Tab steps through them).
	array<string> PickPoints(vector from, vector dir)
	{
		array<string> keys = new array<string>();
		array<float> ts = new array<float>();
		if (!m_Entry)
			return keys;
		for (int c = 0; c < m_Entry.m_Containers.Count(); c++)
		{
			DZS_LootEContainer lc = m_Entry.m_Containers[c];
			for (int i = 0; i < lc.m_Points.Count(); i++)
			{
				DZS_LootEPoint p = lc.m_Points[i];
				if (!p.Valid())
					continue;
				vector b = WorldOfPoint(p);
				float t;
				if (!RayCylinder(from, dir, b, p.Range(), p.Height(), t))
					continue;
				int at = ts.Count();
				while (at > 0 && ts[at - 1] > t)
					at--;
				ts.InsertAt(t, at);
				string k = Key(c, i);
				keys.InsertAt(k, at);
			}
		}
		return keys;
	}

	void TabStep()
	{
		m_TabDepth++;
		DZS_Log.Info("loot: tab depth " + m_TabDepth);
	}

	// The front-most cylinder the ray meets (side, inside or caps; computed, also behind walls).
	string PickPoint(vector from, vector dir)
	{
		if (!m_Entry)
			return "";
		float best = 1000000;
		string bestKey = "";
		for (int c = 0; c < m_Entry.m_Containers.Count(); c++)
		{
			DZS_LootEContainer lc = m_Entry.m_Containers[c];
			for (int i = 0; i < lc.m_Points.Count(); i++)
			{
				DZS_LootEPoint p = lc.m_Points[i];
				if (!p.Valid())
					continue;
				vector b = WorldOfPoint(p);
				float t;
				if (RayCylinder(from, dir, b, p.Range(), p.Height(), t) && t < best)
				{
					best = t;
					bestKey = Key(c, i);
				}
			}
		}
		return bestKey;
	}

	static bool RayCylinder(vector o, vector d, vector base, float r, float h, out float tHit)
	{
		float ox = o[0] - base[0];
		float oz = o[2] - base[2];
		float a = d[0] * d[0] + d[2] * d[2];
		float t0 = -1000000;
		float t1 = 1000000;
		if (a > 0.0000001)
		{
			float bq = 2 * (ox * d[0] + oz * d[2]);
			float cq = ox * ox + oz * oz - r * r;
			float disc = bq * bq - 4 * a * cq;
			if (disc < 0)
				return false;
			float sq = Math.Sqrt(disc);
			t0 = (-bq - sq) / (2 * a);
			t1 = (-bq + sq) / (2 * a);
		}
		else if (ox * ox + oz * oz > r * r)
			return false;
		float y0 = base[1];
		float y1 = base[1] + h;
		float ta = -1000000;
		float tb = 1000000;
		if (Math.AbsFloat(d[1]) > 0.0000001)
		{
			float u0 = (y0 - o[1]) / d[1];
			float u1 = (y1 - o[1]) / d[1];
			ta = Math.Min(u0, u1);
			tb = Math.Max(u0, u1);
		}
		else if (o[1] < y0 || o[1] > y1)
			return false;
		float lo = Math.Max(t0, ta);
		float hi = Math.Min(t1, tb);
		if (lo > hi || hi < 0)
			return false;
		tHit = Math.Max(lo, 0);
		return true;
	}

	protected void Press(string key, float mx, float my)
	{
		m_Press = true;
		m_PressX = mx;
		m_PressY = my;
		m_PressKey = key;
		bool shift = DZS_UiRegistry.ModDown("Shift");
		bool ctrl = DZS_UiRegistry.ModDown("Ctrl");
		if (key == "")
		{
			if (!shift && !ctrl)
				ClearPoints();
			return;
		}
		if (ctrl)
		{
			if (m_Sel.Contains(key))
				m_Sel.Remove(key);
			else
				m_Sel.Set(key, true);
			m_PressKey = "";
		}
		else if (shift)
			m_Sel.Set(key, true);
		else if (!m_Sel.Contains(key))
		{
			m_Sel.Clear();
			m_Sel.Set(key, true);
		}
		int c, i;
		Split(key, c, i);
		m_Active = c;
		DZS_LootEPoint p = PointOf(key);
		if (p)
		{
			m_LastRange = p.Range();
			m_LastHeight = p.Height();
		}
		m_Gen++;
		DZS_WorldLines.Moved();
		DZS_Log.Info("loot: points selected " + m_Sel.Count() + " (" + key + ")");
	}

	void ClearPoints()
	{
		if (m_Sel.Count() == 0)
			return;
		m_Sel.Clear();
		m_Gen++;
		DZS_Log.Info("loot: point selection cleared");
	}

	// Points whose base lies outside the model box widened by their range.
	void OutsidePoints(array<string> into)
	{
		if (!m_Entry)
			return;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		vector bmin, bmax;
		if (!scene || !scene.GetBox(m_Type, bmin, bmax))
			return;
		for (int c = 0; c < m_Entry.m_Containers.Count(); c++)
		{
			DZS_LootEContainer lc = m_Entry.m_Containers[c];
			for (int i = 0; i < lc.m_Points.Count(); i++)
			{
				DZS_LootEPoint p = lc.m_Points[i];
				vector lp = p.P();
				float r = p.m_Range.ToFloat();
				if (!p.m_HasRange)
					r = 0.1;
				float mx = -lp[2];
				float my = lp[1];
				float mz = lp[0];
				bool isOut = mx < bmin[0] - r || mx > bmax[0] + r;
				if (my < bmin[1] - r || my > bmax[1] + r)
					isOut = true;
				if (mz < bmin[2] - r || mz > bmax[2] + r)
					isOut = true;
				if (isOut)
				{
					string k = Key(c, i);
					into.Insert(k);
				}
			}
		}
	}

	void SelectKeys(array<string> keys)
	{
		m_Sel.Clear();
		foreach (string k : keys)
			m_Sel.Set(k, true);
		m_Gen++;
		DZS_Log.Info("loot: points selected " + m_Sel.Count() + " (Show)");
	}

	void SelectAllPoints()
	{
		m_Sel.Clear();
		for (int c = 0; c < m_Entry.m_Containers.Count(); c++)
		{
			DZS_LootEContainer lc = m_Entry.m_Containers[c];
			for (int i = 0; i < lc.m_Points.Count(); i++)
			{
				string tk1 = Key(c, i);
				m_Sel.Set(tk1, true);
			}
		}
		m_Gen++;
		DZS_Log.Info("loot: all " + m_Sel.Count() + " points selected");
	}

	void SelectContainer(int c)
	{
		m_Sel.Clear();
		if (c < m_Entry.m_Containers.Count())
		{
			DZS_LootEContainer lc = m_Entry.m_Containers[c];
			for (int i = 0; i < lc.m_Points.Count(); i++)
			{
				string tk2 = Key(c, i);
				m_Sel.Set(tk2, true);
			}
		}
		m_Active = c;
		m_Gen++;
		DZS_Log.Info("loot: points of container " + c + " selected: " + m_Sel.Count());
	}

	// ---- changes ----

	// Commits a new entry: first change of an entry "as vanilla" asks (first intervention); selKeys select afterwards.
	void Change(DZS_LootEEntry e, string selKeys = "")
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return;
		string st = m_P.Status(inst, m_Type);
		if (st == "base")
		{
			m_PendingEntry = e;
			m_PendingSel = selKeys;
			m_PendingActive = m_Active;
			int pts, mapRows, inProj;
			m_P.Facts(inst, m_Type, pts, mapRows, inProj);
			string mapText = DZS_LootPlugin.Plural(mapRows, "Gebäude", "Gebäude", "building", "buildings");
			string projText = DZS_LootPlugin.Plural(inProj, "Objekt", "Objekte", "object", "objects");
			string shown = m_P.ShownName(m_Type);
			string q = DZS_Texts.Fmt("loot.firstQ", shown, mapText, projText);
			string kick = DZS_Texts.Get("loot.row");
			string ttl = DZS_Texts.Get("loot.firstTitle");
			DZS_LootTextDialog d = new DZS_LootTextDialog(kick, ttl, q);
			d.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
			d.AddFootButton("own", DZS_Texts.Get("loot.firstGo"), "main");
			d.m_OnResult.Insert(OnFirstAnswer);
			d.Open();
			DZS_Log.Info("loot: first change asked: " + q);
			return;
		}
		Commit(inst, e, selKeys);
	}

	void OnFirstAnswer(DZS_Dialog d, string result)
	{
		DZS_Log.Info("loot: first change answer " + result);
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_LootEEntry e = m_PendingEntry;
		m_PendingEntry = null;
		if (!inst || result != "own" || !e)
		{
			Refresh(inst);
			return;
		}
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		DZS_LootEEntry start = m_P.Effective(inst, m_Type);
		m_P.SetEntry(b, inst, m_Type, start);
		array<string> objs = m_P.ClassObjects(inst);
		string low = m_Type;
		low.ToLower();
		foreach (string id : objs)
		{
			string name;
			inst.GetField(id, DZS_Obj.NAME, name);
			name.ToLower();
			if (name != low)
				continue;
			string s = m_P.SettingOf(inst, id);
			if (s != DZS_LootPlugin.NONE)
				b.SetCompanion(id, DZS_LootPlugin.SETTING, 1, DZS_LootPlugin.OWN);
		}
		string de = StepName("loot.stepOwn", true);
		string en = StepName("loot.stepOwn", false);
		DZS_SubmitResult r = b.Commit(de, en);
		DZS_Log.Info("loot: step '" + de + "' ok " + r.Ok());
		m_Active = m_PendingActive;
		Commit(inst, e, m_PendingSel);
		m_Active = Math.Min(m_PendingActive, m_Entry.m_Containers.Count() - 1);
		if (m_Active < 0)
			m_Active = 0;
	}

	protected void Commit(DZS_Instance inst, DZS_LootEEntry e, string selKeys)
	{
		DZS_OpBuilder b = inst.BeginOperation("loot:edit", 1);
		m_P.SetEntry(b, inst, m_Type, e);
		string de = StepName("loot.step", true);
		string en = StepName("loot.step", false);
		DZS_SubmitResult r = b.Commit(de, en);
		DZS_Log.Info("loot: step '" + de + "' ok " + r.Ok() + ", " + e.CountPoints() + " points in " + e.m_Containers.Count() + " container(s)");
		if (selKeys != "")
		{
			m_Sel.Clear();
			array<string> ks = new array<string>();
			selKeys.Split(",", ks);
			foreach (string k : ks)
				m_Sel.Set(k, true);
		}
		Refresh(inst);
		UpdateBanner(inst);
	}

	void AskReset()
	{
		if (m_On && m_Panel)
			m_Panel.OnReset(null);
	}

	bool IsMapBuilding() { return m_MapObj != null; }

	DZS_LootEEntry Copy()
	{
		return m_Entry.Copy();
	}

	protected float ViewDist()
	{
		float len = m_Kernel.GetSettings().GetSettingNumber("camera.viewDistance");
		if (len <= 0)
			len = 1500;
		return len;
	}

	// Double click: a new point where the ray first meets a surface, in the active container, at its end.
	protected void SetPointAt(vector from, vector dir)
	{
		float len = m_Kernel.GetSettings().GetSettingNumber("camera.viewDistance");
		if (len <= 0)
			len = 1500;
		array<Object> none = TestObjects();
		array<ref DZS_RayHit> hits = DZS_Rays.Ray(from, dir, len, none);
		if (hits.Count() == 0)
		{
			DZS_Log.Info("loot: double click hit nothing");
			return;
		}
		vector hit = hits[0].m_Pos;
		DZS_LootEEntry e = Copy();
		if (e.m_Containers.Count() == 0)
		{
			DZS_LootEContainer nc = new DZS_LootEContainer();
			nc.m_Name = "lootFloor";
			e.m_Containers.Insert(nc);
			m_Active = 0;
		}
		if (m_Active >= e.m_Containers.Count())
			m_Active = 0;
		DZS_LootEContainer lc = e.m_Containers[m_Active];
		float range = NEW_RANGE;
		float height = NEW_HEIGHT;
		if (m_LastRange > 0)
		{
			range = m_LastRange;
			height = m_LastHeight;
		}
		else if (lc.m_Points.Count() > 0)
		{
			DZS_LootEPoint last = lc.m_Points[lc.m_Points.Count() - 1];
			range = last.Range();
			height = last.Height();
		}
		DZS_LootEPoint p = new DZS_LootEPoint();
		p.SetP(LootOfWorld(hit));
		p.SetRange(range);
		p.SetHeight(height);
		lc.m_Points.Insert(p);
		string key = Key(m_Active, lc.m_Points.Count() - 1);
		DZS_Log.Info("loot: point set at " + hit + " = " + p.m_Pos + " range " + p.m_Range + " height " + p.m_Height + " in " + lc.m_Name);
		Change(e, key);
	}

	protected void BeginDrag()
	{
		m_Drag = true;
		m_DragLift = 0;
		m_DragHasTarget = false;
		m_Kernel.ClaimWheel("loot", true);
		m_DragStart = Copy();
		m_Working = true;
		DZS_LootEPoint p = PointOf(m_PressKey);
		if (p)
			m_GrabWorld = WorldOfPoint(p);
		DZS_Log.Info("loot: drag of " + m_Sel.Count() + " point(s) starts");
	}

	protected void DragTo(vector from, vector dir)
	{
		// wheel: up and down by 0.1 m, with Shift 0.01 m
		int wheel = GetMouseState(MouseState.WHEEL);
		if (wheel != 0)
		{
			float step = 0.1;
			if (DZS_UiRegistry.ModDown("Shift"))
				step = 0.01;
			m_DragLift = Math.Round((m_DragLift + wheel * step) * 1000) / 1000;
			DZS_Log.Info("loot: drag lift " + m_DragLift);
		}
		array<Object> none = TestObjects();
		array<ref DZS_RayHit> hits = DZS_Rays.Ray(from, dir, ViewDist(), none);
		if (hits.Count() > 0)
		{
			m_DragTarget = hits[0].m_Pos;
			m_DragHasTarget = true;
		}
		if (!m_DragHasTarget)
			return;
		vector target = m_DragTarget + Vector(0, m_DragLift, 0);
		vector delta = LootOfWorld(target) - LootOfWorld(m_GrabWorld);
		DZS_LootEEntry e = m_DragStart.Copy();
		for (int k = 0; k < m_Sel.Count(); k++)
		{
			int c, i;
			Split(m_Sel.GetKey(k), c, i);
			if (c >= e.m_Containers.Count() || i >= e.m_Containers[c].m_Points.Count())
				continue;
			DZS_LootEPoint p = e.m_Containers[c].m_Points[i];
			vector np = p.P() + delta;
			p.SetP(np);
		}
		m_Entry = e;
		DZS_WorldLines.Moved();
	}

	protected void EndDrag()
	{
		m_Drag = false;
		m_Kernel.ClaimWheel("loot", false);
		m_Press = false;
		DZS_LootEEntry e = m_Entry;
		m_Entry = m_DragStart;
		m_DragStart = null;
		string sigNew = e.Sig();
		string sigOld = m_Entry.Sig();
		if (sigNew == sigOld)
		{
			Refresh(m_Kernel.GetShown());
			return;
		}
		DZS_Log.Info("loot: drag ends");
		Change(e);
	}

	void CancelDrag()
	{
		if (m_Rect)
		{
			m_Rect = false;
			m_Press = false;
			DZS_Log.Info("loot: rectangle cancelled");
		}
		if (m_KeyGesture)
		{
			m_KeyGesture = false;
			if (m_KeyStart)
				m_Entry = m_KeyStart;
			m_KeyStart = null;
			m_Working = false;
			m_KeyNeedRelease = true;
			DZS_WorldLines.Moved();
			DZS_Log.Info("loot: key gesture cancelled");
		}
		if (m_Handle >= 0)
		{
			m_Handle = -1;
			if (m_HandleStart)
				m_Entry = m_HandleStart;
			m_HandleStart = null;
			m_Working = false;
			DZS_WorldLines.Moved();
			DZS_Log.Info("loot: handle cancelled");
		}
		if (!m_Drag)
			return;
		m_Drag = false;
		m_Kernel.ClaimWheel("loot", false);
		m_Press = false;
		if (m_DragStart)
			m_Entry = m_DragStart;
		m_DragStart = null;
		m_Working = false;
		DZS_WorldLines.Moved();
		DZS_Log.Info("loot: drag cancelled");
	}

	// Keys of the selected points, ordered by container and from the last point back (safe for removing).
	protected array<string> SelectedDescending()
	{
		array<int> codes = new array<int>();
		for (int k = 0; k < m_Sel.Count(); k++)
		{
			int c, i;
			Split(m_Sel.GetKey(k), c, i);
			codes.Insert(c * 100000 + i);
		}
		codes.Sort(true);
		array<string> r = new array<string>();
		foreach (int code : codes)
		{
			int cc = code / 100000;
			int ii = code % 100000;
			string rk = Key(cc, ii);
			r.Insert(rk);
		}
		return r;
	}

	void DeletePoints()
	{
		if (m_Sel.Count() == 0)
			return;
		DZS_LootEEntry e = Copy();
		array<string> keys = SelectedDescending();
		foreach (string k : keys)
		{
			int c, i;
			Split(k, c, i);
			if (c < e.m_Containers.Count() && i < e.m_Containers[c].m_Points.Count())
				e.m_Containers[c].m_Points.RemoveOrdered(i);
		}
		DZS_Log.Info("loot: delete " + keys.Count() + " point(s)");
		foreach (string dk : keys)
			ShiftTests(dk, -1);
		m_Sel.Clear();
		Change(e);
	}

	void DuplicatePoints()
	{
		if (m_Sel.Count() == 0)
			return;
		DZS_LootEEntry e = Copy();
		array<string> keys = SelectedDescending();
		map<int, int> added = new map<int, int>();
		foreach (string k : keys)
		{
			int c, i;
			Split(k, c, i);
			if (c >= e.m_Containers.Count() || i >= e.m_Containers[c].m_Points.Count())
				continue;
			DZS_LootEContainer lc = e.m_Containers[c];
			DZS_LootEPoint p = lc.m_Points[i].Copy();
			p.m_Src = -1;
			p.m_Flags = "";
			p.m_Uid = "";
			p.m_Ext = DZS_ExtCopy.Text(p.m_Ext, "block", "lootPoint", "", true);
			lc.m_Points.InsertAt(p, i + 1);
		}
		// the copies: each original's index shifted by the copies before it
		string sel = "";
		array<string> asc = new array<string>();
		for (int a = keys.Count() - 1; a >= 0; a--)
			asc.Insert(keys[a]);
		map<int, int> before = new map<int, int>();
		foreach (string k2 : asc)
		{
			int c2, i2;
			Split(k2, c2, i2);
			int n = 0;
			before.Find(c2, n);
			int copyAt = i2 + n + 1;
			before.Set(c2, n + 1);
			if (sel != "")
				sel += ",";
			string ck = Key(c2, copyAt);
			sel += ck;
		}
		foreach (string uk : keys)
			ShiftTests(uk, 1);
		DZS_Log.Info("loot: duplicate " + keys.Count() + " point(s)");
		Change(e, sel);
	}

	// ---- rectangle, keys, handles ----

	// Points whose ground centre lies in the rectangle (Shift adds, Ctrl removes), like selecting objects.
	protected void EndRect(float mx, float my)
	{
		m_Rect = false;
		m_Press = false;
		float x0 = Math.Min(mx, m_PressX);
		float x1 = Math.Max(mx, m_PressX);
		float y0 = Math.Min(my, m_PressY);
		float y1 = Math.Max(my, m_PressY);
		bool shift = DZS_UiRegistry.ModDown("Shift");
		bool ctrl = DZS_UiRegistry.ModDown("Ctrl");
		if (!shift && !ctrl)
			m_Sel.Clear();
		vector cam = GetGame().GetCurrentCameraPosition();
		vector cdir = GetGame().GetCurrentCameraDirection();
		int n = 0;
		for (int c = 0; c < m_Entry.m_Containers.Count(); c++)
		{
			DZS_LootEContainer lc = m_Entry.m_Containers[c];
			for (int i = 0; i < lc.m_Points.Count(); i++)
			{
				DZS_LootEPoint p = lc.m_Points[i];
				if (!p.Valid())
					continue;
				vector w = WorldOfPoint(p);
				if (vector.Dot(w - cam, cdir) <= 0.05)
					continue;
				vector sp = DZS_PlaceMath.Project(w);
				if (sp[0] < x0 || sp[0] > x1 || sp[1] < y0 || sp[1] > y1)
					continue;
				string k = Key(c, i);
				if (ctrl)
					m_Sel.Remove(k);
				else
					m_Sel.Set(k, true);
				n++;
			}
		}
		m_Gen++;
		DZS_WorldLines.Moved();
		DZS_Log.Info("loot: rectangle " + x0 + "," + y0 + " - " + x1 + "," + y1 + ": " + n + " point(s), selected " + m_Sel.Count());
	}

	// Arrow keys and PgUp/PgDn move the selected points along the building's axes, period/comma change their radius;
	// one step until all are released.
	protected void KeyGesture(float dt)
	{
		DZS_UiRegistry ui = m_Kernel.GetUi();
		DZS_Screen scr = m_Kernel.GetScreen();
		bool blocked = scr.IsSceneKeyboardBlocked() || m_Sel.Count() == 0 || m_Drag || m_Handle >= 0 || m_Rect;
		vector mv = "0 0 0";
		float grow = 0;
		if (!blocked)
		{
			if (ui.IsHeld("placement.key.fwd")) mv[2] = mv[2] + 1;
			if (ui.IsHeld("placement.key.back")) mv[2] = mv[2] - 1;
			if (ui.IsHeld("placement.key.right")) mv[0] = mv[0] + 1;
			if (ui.IsHeld("placement.key.left")) mv[0] = mv[0] - 1;
			if (ui.IsHeld("placement.key.up")) mv[1] = mv[1] + 1;
			if (ui.IsHeld("placement.key.down")) mv[1] = mv[1] - 1;
			if (ui.IsHeld("placement.key.grow")) grow += 1;
			if (ui.IsHeld("placement.key.shrink")) grow -= 1;
		}
		bool any = mv.Length() > 0 || grow != 0;
		if (m_KeyNeedRelease)
		{
			if (!any)
				m_KeyNeedRelease = false;
			return;
		}
		if (!m_KeyGesture)
		{
			if (!any)
				return;
			m_KeyGesture = true;
			m_KeyStart = Copy();
			m_KeyMove = "0 0 0";
			m_KeyFactor = 1;
			m_Working = true;
			DZS_Log.Info("loot: key gesture start");
		}
		if (!any)
		{
			m_KeyGesture = false;
			DZS_LootEEntry done = m_Entry;
			m_Entry = m_KeyStart;
			m_KeyStart = null;
			DZS_Log.Info("loot: key gesture end, move " + m_KeyMove.ToString() + " factor " + m_KeyFactor);
			string sigNew = done.Sig();
			string sigOld = m_Entry.Sig();
			if (sigNew == sigOld)
			{
				Refresh(m_Kernel.GetShown());
				return;
			}
			Change(done);
			return;
		}
		bool fast = ui.IsHeld("camera.faster");
		bool slow = ui.IsHeld("camera.slower");
		float mul = 1;
		if (fast && !slow)
			mul = 5;
		else if (slow && !fast)
			mul = 0.2;
		float speed = m_Kernel.GetSettings().GetSettingNumber("placement.keySpeed");
		if (speed <= 0)
			speed = 2;
		m_KeyMove = m_KeyMove + mv * (speed * mul * dt);
		if (grow != 0)
		{
			float rate = 2;
			if (fast && !slow)
				rate = 5;
			else if (slow && !fast)
				rate = 1.15;
			float want = m_KeyFactor * Math.Pow(rate, grow * dt);
			float lo = 0;
			float hi = 1000000;
			for (int k = 0; k < m_Sel.Count(); k++)
			{
				DZS_LootEPoint sp = PointIn(m_KeyStart, m_Sel.GetKey(k));
				if (!sp || sp.Range() <= 0)
					continue;
				float r0 = sp.Range();
				lo = Math.Max(lo, 0.01 / r0);
				hi = Math.Min(hi, 10.0 / r0);
			}
			if (want < lo)
				want = lo;
			if (want > hi)
				want = hi;
			m_KeyFactor = want;
		}
		// building axes right/up/forward are -pz/py/px of a loot point
		vector delta = Vector(m_KeyMove[2], m_KeyMove[1], -m_KeyMove[0]);
		DZS_LootEEntry e = m_KeyStart.Copy();
		for (int j = 0; j < m_Sel.Count(); j++)
		{
			DZS_LootEPoint p = PointIn(e, m_Sel.GetKey(j));
			if (!p)
				continue;
			if (delta.Length() > 0)
			{
				vector np = p.P() + delta;
				p.SetP(np);
			}
			if (m_KeyFactor != 1)
			{
				float nr = p.Range() * m_KeyFactor;
				p.SetRange(nr);
			}
		}
		m_Entry = e;
		DZS_WorldLines.Moved();
	}

	protected DZS_LootEPoint PointIn(DZS_LootEEntry e, string key)
	{
		int c, i;
		if (!e || !Split(key, c, i))
			return null;
		if (c >= e.m_Containers.Count() || i >= e.m_Containers[c].m_Points.Count())
			return null;
		return e.m_Containers[c].m_Points[i];
	}

	// Move tool: arrows at the centre of the selected points along the building's axes.
	protected bool HandlesShown()
	{
		return m_Kernel.GetUi().GetActiveTool() == "placement.tool.move" && m_Sel.Count() > 0 && !m_Drag && !m_KeyGesture;
	}

	protected void UpdateHandles()
	{
		m_HandleTips.Clear();
		if (!HandlesShown() && m_Handle < 0)
			return;
		vector sum = "0 0 0";
		int n = 0;
		for (int k = 0; k < m_Sel.Count(); k++)
		{
			DZS_LootEPoint p = PointOf(m_Sel.GetKey(k));
			if (!p)
				continue;
			sum = sum + WorldOfPoint(p);
			n++;
		}
		if (n == 0)
			return;
		m_HandleCenter = sum * (1.0 / n);
		vector o, ypr, r, u, f;
		Frame(o, ypr);
		DZS_PlaceMath.Axes(ypr, r, u, f);
		float len = vector.Distance(GetGame().GetCurrentCameraPosition(), m_HandleCenter) * 0.12;
		m_HandleTips.Insert(m_HandleCenter + r * len);
		m_HandleTips.Insert(m_HandleCenter + u * len);
		m_HandleTips.Insert(m_HandleCenter + f * len);
	}

	protected int HandleAt(float mx, float my)
	{
		UpdateHandles();
		if (!HandlesShown() || m_HandleTips.Count() != 3)
			return -1;
		vector c = DZS_PlaceMath.Project(m_HandleCenter);
		if (c[2] <= 0)
			return -1;
		int best = -1;
		float bestD = DZS_Scale.Px(14);
		for (int i = 0; i < 3; i++)
		{
			float tdx, tdy;
			DZS_PlaceMath.Flat(m_HandleCenter, m_HandleTips[i] - m_HandleCenter, tdx, tdy);
			vector t = Vector(c[0] + tdx, c[1] + tdy, 0);
			float d = SegDist(mx, my, c[0], c[1], t[0], t[1]);
			if (d < bestD)
			{
				bestD = d;
				best = i;
			}
		}
		return best;
	}

	static float SegDist(float px, float py, float ax, float ay, float bx, float by)
	{
		float dx = bx - ax;
		float dy = by - ay;
		float l2 = dx * dx + dy * dy;
		float t = 0;
		if (l2 > 0.0001)
			t = Math.Clamp(((px - ax) * dx + (py - ay) * dy) / l2, 0, 1);
		float qx = ax + dx * t - px;
		float qy = ay + dy * t - py;
		return Math.Sqrt(qx * qx + qy * qy);
	}

	// Parameter of the point on the line c + t·a nearest to the ray from + s·dir.
	static float AxisParam(vector c, vector a, vector from, vector dir)
	{
		vector w0 = c - from;
		float b = vector.Dot(a, dir);
		float d = vector.Dot(a, w0);
		float e = vector.Dot(dir, w0);
		float den = 1 - b * b;
		if (Math.AbsFloat(den) < 0.000001)
			return 0;
		return (b * e - d) / den;
	}

	protected void BeginHandle(int h, vector from, vector dir)
	{
		vector o, ypr, r, u, f;
		Frame(o, ypr);
		DZS_PlaceMath.Axes(ypr, r, u, f);
		if (h == 0)
			m_HandleA = r;
		else if (h == 1)
			m_HandleA = u;
		else
			m_HandleA = f;
		m_Handle = h;
		m_HandleC = m_HandleCenter;
		m_HandleT0 = AxisParam(m_HandleC, m_HandleA, from, dir);
		m_HandleStart = Copy();
		m_Working = true;
		DZS_Log.Info("loot: handle " + h + " held");
	}

	protected void HandleTo(vector from, vector dir)
	{
		float d = AxisParam(m_HandleC, m_HandleA, from, dir) - m_HandleT0;
		vector delta = Vector(d, 0, 0);
		if (m_Handle == 0)
			delta = Vector(0, 0, -d);
		else if (m_Handle == 1)
			delta = Vector(0, d, 0);
		DZS_LootEEntry e = m_HandleStart.Copy();
		for (int k = 0; k < m_Sel.Count(); k++)
		{
			DZS_LootEPoint p = PointIn(e, m_Sel.GetKey(k));
			if (!p)
				continue;
			vector np = p.P() + delta;
			p.SetP(np);
		}
		m_Entry = e;
		DZS_WorldLines.Moved();
	}

	protected void EndHandle()
	{
		DZS_LootEEntry done = m_Entry;
		m_Entry = m_HandleStart;
		m_HandleStart = null;
		m_Handle = -1;
		DZS_Log.Info("loot: handle released");
		string sigNew = done.Sig();
		string sigOld = m_Entry.Sig();
		if (sigNew == sigOld)
		{
			Refresh(m_Kernel.GetShown());
			return;
		}
		Change(done);
	}

	protected void DrawExtras(CanvasWidget cv)
	{
		if (m_Rect)
		{
			int mx, my;
			GetMousePos(mx, my);
			int ac = DZS_Color.Get("accent");
			float w = Math.Max(1, DZS_Scale.Px(1));
			DZS_Clip.Line(cv, m_PressX, m_PressY, mx, m_PressY, w, ac);
			DZS_Clip.Line(cv, mx, m_PressY, mx, my, w, ac);
			DZS_Clip.Line(cv, mx, my, m_PressX, my, w, ac);
			DZS_Clip.Line(cv, m_PressX, my, m_PressX, m_PressY, w, ac);
		}
		UpdateHandles();
		if (m_HandleTips.Count() != 3)
			return;
		vector c = DZS_PlaceMath.Project(m_HandleCenter);
		if (c[2] <= 0)
			return;
		array<string> cols = {"axisX", "axisY", "axisZ"};
		float hw = Math.Max(2, DZS_Scale.Px(2));
		for (int i = 0; i < 3; i++)
		{
			float tdx, tdy;
			DZS_PlaceMath.Flat(m_HandleCenter, m_HandleTips[i] - m_HandleCenter, tdx, tdy);
			vector t = Vector(c[0] + tdx, c[1] + tdy, 0);
			int col = DZS_Color.Get(cols[i]);
			if (m_Handle == i)
				col = DZS_LootPlugin.Lighter(col);
			DZS_Clip.Line(cv, c[0], c[1], t[0], t[1], hw, col);
			float dx = t[0] - c[0];
			float dy = t[1] - c[1];
			float l = Math.Sqrt(dx * dx + dy * dy);
			if (l < 1)
				continue;
			dx /= l;
			dy /= l;
			float s = DZS_Scale.Px(7);
			DZS_Clip.Line(cv, t[0], t[1], t[0] - dx * s - dy * s * 0.6, t[1] - dy * s + dx * s * 0.6, hw, col);
			DZS_Clip.Line(cv, t[0], t[1], t[0] - dx * s + dy * s * 0.6, t[1] - dy * s - dx * s * 0.6, hw, col);
		}
	}

	// ---- test items ----

	bool HasTests() { return m_Tests.Count() > 0; }

	array<Object> TestObjects()
	{
		array<Object> r = new array<Object>();
		for (int i = 0; i < m_Tests.Count(); i++)
		{
			Object o = m_Tests.GetElement(i);
			if (o)
				r.Insert(o);
		}
		return r;
	}

	void OpenTestDialog()
	{
		if (m_Sel.Count() == 0)
			return;
		array<string> first = new array<string>();
		int c, i;
		string k0 = m_Sel.GetKey(0);
		if (Split(k0, c, i) && c < m_Entry.m_Containers.Count())
			first.Copy(m_Entry.m_Containers[c].m_Cats);
		DZS_LootTestDialog d = new DZS_LootTestDialog(this, first);
		d.Open();
	}

	// One item of the class at every selected point, replacing an earlier one there.
	void PlaceTests(string cls)
	{
		int made = 0;
		for (int k = 0; k < m_Sel.Count(); k++)
		{
			string key = m_Sel.GetKey(k);
			RemoveTest(key);
			Object o = GetGame().CreateObjectEx(cls, GetGame().GetCurrentCameraPosition(), ECE_LOCAL | ECE_KEEPHEIGHT | ECE_NOLIFETIME, RF_IGNORE);
			if (!o)
			{
				string what = DZS_Texts.Pick(" lässt sich nicht anzeigen", " cannot be shown");
				m_Kernel.GetScreen().ShowMessage("loot.test", "warning", cls + what, 4.0);
				DZS_Log.Info("loot: test item " + cls + " not created");
				break;
			}
			m_Tests.Set(key, o);
			made++;
		}
		UpdateTests();
		array<Object> pass = TestObjects();
		DZS_WorldLines.SetPass(pass);
		m_Gen++;
		DZS_Log.Info("loot: test item " + cls + " at " + made + " point(s), " + m_Tests.Count() + " in the scene");
	}

	void RemoveTests()
	{
		array<string> keys = new array<string>();
		for (int i = 0; i < m_Tests.Count(); i++)
			keys.Insert(m_Tests.GetKey(i));
		foreach (string k : keys)
			RemoveTest(k);
		m_Gen++;
	}

	protected void RemoveTest(string key)
	{
		Object o;
		if (!m_Tests.Find(key, o))
			return;
		m_Tests.Remove(key);
		if (o)
			GetGame().ObjectDelete(o);
		DZS_Log.Info("loot: test item at " + key + " removed");
	}

	// After deleting (dir -1) or duplicating (+1) the point at key: later test keys of its container move along.
	protected void ShiftTests(string key, int dir)
	{
		int c, i;
		if (!Split(key, c, i))
			return;
		if (dir < 0)
			RemoveTest(key);
		map<string, Object> moved = new map<string, Object>();
		array<string> old = new array<string>();
		for (int t = 0; t < m_Tests.Count(); t++)
		{
			string tk = m_Tests.GetKey(t);
			int tc, ti;
			Split(tk, tc, ti);
			if (tc == c && ti > i)
			{
				old.Insert(tk);
				string nk = Key(tc, ti + dir);
				moved.Set(nk, m_Tests.GetElement(t));
			}
		}
		foreach (string ok : old)
			m_Tests.Remove(ok);
		for (int m = 0; m < moved.Count(); m++)
			m_Tests.Set(moved.GetKey(m), moved.GetElement(m));
	}

	// Centred on the ground circle, upright, yaw like the building; follows its point (also while dragging).
	protected void UpdateTests()
	{
		if (m_Tests.Count() == 0)
			return;
		vector o, ypr;
		if (!Frame(o, ypr))
			return;
		for (int i = 0; i < m_Tests.Count(); i++)
		{
			Object t = m_Tests.GetElement(i);
			DZS_LootEPoint p = PointOf(m_Tests.GetKey(i));
			if (!t || !p)
				continue;
			vector base = WorldOfPoint(p);
			vector mm[2];
			t.ClippingInfo(mm);
			vector at = base;
			at[1] = base[1] - mm[0][1];
			t.SetOrientation(Vector(ypr[0], 0, 0));
			t.SetPosition(at);
		}
	}

	// ---- drawing ----

	void Draw(CanvasWidget cv)
	{
		vector o, ypr;
		if (!m_Entry || !Frame(o, ypr))
		{
			HideWarnings(0);
			return;
		}
		m_P.DrawEntry(cv, "ed:" + m_Type, m_Entry, o, ypr, 1.0, m_Sel, m_Hover);
		DrawExtras(cv);
		DrawWarnings(cv, o, ypr);
	}

	// A point outside the building carries the warning symbol above its
	// cylinder.
	protected ref array<ImageWidget> m_Warn = new array<ImageWidget>();

	protected void DrawWarnings(CanvasWidget cv, vector o, vector ypr)
	{
		array<string> outside = new array<string>();
		OutsidePoints(outside);
		Widget layer = cv.GetParent();
		float lx, ly;
		layer.GetScreenPos(lx, ly);
		vector r, u, f;
		DZS_PlaceMath.Axes(ypr, r, u, f);
		float size = DZS_Scale.Px(16);
		int shown = 0;
		foreach (string k : outside)
		{
			int c, i;
			if (!Split(k, c, i))
				continue;
			DZS_LootEPoint p = m_Entry.m_Containers[c].m_Points[i];
			if (!p.Valid())
				continue;
			vector b = DZS_LootPlugin.WorldOf(o, r, u, f, p.P());
			vector top = b + Vector(0, p.Height(), 0);
			vector sp = DZS_PlaceMath.Project(top);
			if (sp[2] <= 0)
				continue;
			float x = sp[0] - size / 2;
			float y = sp[1] - size - DZS_Scale.Px(4);
			if (!DZS_Clip.Inside(x + size / 2, y + size / 2))
				continue;
			if (shown >= m_Warn.Count())
			{
				ImageWidget im = DZS_W.Icon(layer, "triangle-alert", DZS_Color.Get("warning"));
				m_Warn.Insert(im);
			}
			ImageWidget w = m_Warn[shown];
			w.SetPos(x - lx, y - ly);
			w.SetSize(size, size);
			w.Show(true);
			shown++;
		}
		HideWarnings(shown);
	}

	protected void HideWarnings(int from)
	{
		for (int n = from; n < m_Warn.Count(); n++)
			m_Warn[n].Show(false);
	}
}

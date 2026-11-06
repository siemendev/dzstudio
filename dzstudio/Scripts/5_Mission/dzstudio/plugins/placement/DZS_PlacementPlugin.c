// Placing and selecting: a bundled plugin on the public interface only.
// Gestures are views until they end; every end of a gesture is one operation "placement:edit".
class DZS_SelectionService : DZS_SelectionApi
{
	DZS_PlacementPlugin m_P;

	override array<string> GetSelection()
	{
		array<string> c = new array<string>();
		c.Copy(m_P.m_Sel);
		return c;
	}
	override string GetLead() { return m_P.m_Lead; }
	override bool HasHand() { return m_P.HandFull(); }
	override bool HandIsTemplate() { return m_P.HandTemplate(); }
	override bool PlaceHandAt(float x, float z) { return m_P.PlaceOnGround(x, z); }
	override bool IsSelected(string id) { return m_P.IsSelectedFast(id); }
	override string Signature() { return m_P.SelectionSignature(); }
	override void SetSelection(array<string> ids, string lead = "") { m_P.SetSelection(ids, lead); }
	override void Add(array<string> ids) { m_P.AddToSelection(ids, ""); }
	override void Remove(array<string> ids) { m_P.RemoveFromSelection(ids); }
	override string GetHover() { return m_P.m_Hover; }
	override void SetListHover(string id) { m_P.m_ListHover = id; }
	override string GetActiveGroup() { return DZS_ActiveGroup.Get(DZS_Kernel.Get().GetShown()); }
	override void SetActiveGroup(string groupId) { m_P.SetActiveGroup(groupId); }
	override bool IsSwapping() { return m_P.m_Swapping; }
	override void SwapTo(string name) { m_P.SwapTo(name); }
	override void StartSwap() { m_P.OnAction("placement.swap"); }
	override bool MeasureHeightOffset(string id, out float offset) { return m_P.MeasureOffset(id, offset); }
	override bool GetBounds(array<string> ids, out vector boxMin, out vector boxMax) { return m_P.BoundsOfIds(ids, boxMin, boxMax); }
	override void AddBarButton(string actionId, string icon, string labelDe, string labelEn, int order, bool singleOnly = false) { m_P.AddForeignBarButton(actionId, icon, labelDe, labelEn, order, singleOnly); }
}

class DZS_BarEntry : Managed
{
	string m_Action;
	string m_Icon;
	string m_De;
	string m_En;
	int m_Order;
	bool m_SingleOnly;
}

class DZS_PlaceActions : DZS_ActionHandler
{
	DZS_PlacementPlugin m_P;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn) { return m_P.ActionEnabled(actionId, reasonDe, reasonEn); }
	override void OnAction(string actionId) { m_P.OnAction(actionId); }
	override bool IsActionChecked(string actionId) { return m_P.ActionChecked(actionId); }
	override string GetActionLabel(string actionId) { return m_P.ActionLabel(actionId); }
}

class DZS_PlaceEsc : DZS_EscHandler
{
	DZS_PlacementPlugin m_P;
	string m_What;

	override bool IsEscActive() { return m_P.EscActive(m_What); }
	override void OnEsc() { m_P.Esc(m_What); }
}

[DZS_RegisterPlugin(DZS_PlacementPlugin)]
class DZS_PlacementPlugin : DZS_Plugin
{
	static const float DRAG_PX = 4;
	static const float HANDLE_PX = 120;
	static const float HANDLE_HIT = 16;
	static const int G_NONE = 0;
	static const int G_PRESS_OBJ = 1;
	static const int G_DRAG = 2;
	static const int G_PRESS_EMPTY = 3;
	static const int G_LASSO = 4;
	static const int G_HANDLE = 5;
	static const int G_CARRY = 6;
	static const int DM_GROUND = 0;
	static const int DM_ROTATE = 2;
	static const int DM_PLANE = 3;
	static const int DM_HEIGHT = 4;

	ref array<string> m_Sel;
	string m_Lead;
	string m_Hover;
	string m_ListHover;
	bool m_Swapping;
	protected string m_Tool = "placement.tool.select";
	protected ref DZS_PlaceActions m_Actions;
	protected ref DZS_SelectionService m_Service;
	protected ref array<ref DZS_PlaceEsc> m_Escs;
	protected bool m_World;
	protected bool m_Ready;
	protected ref array<ref DZS_BarEntry> m_ForeignBar;

	// hand
	protected string m_Hand;
	protected Object m_Preview;
	protected ref map<string, bool> m_SelSet = new map<string, bool>();
	protected int m_SelSetFrame = -1;
	protected int m_SelSetCount = -1;
	static const int BIG_SELECTION = 20;
	protected string m_BigSig;
	protected ref array<vector> m_BigBox = new array<vector>();
	protected ref array<vector> m_BigCenters = new array<vector>();
	protected ref array<bool> m_BigHidden = new array<bool>();
	protected vector m_BigMid;
	protected int m_GeomVer;
	protected int m_GeomCacheVer = -1;
	protected DZS_Instance m_GeomCacheInst;
	protected ref map<string, ref DZS_PlaceObj> m_ObjCache = new map<string, ref DZS_PlaceObj>();
	protected ref map<string, ref array<vector>> m_CornerCache = new map<string, ref array<vector>>();
	protected ref map<string, vector> m_CenterCache = new map<string, vector>();
	protected string m_PreviewName;
	protected ref DZS_PlaceObj m_HandObj;
	protected vector m_HandYpr;
	protected float m_HandOffset;
	protected float m_HandScale = 1;
	protected ref DZS_ElementData m_HandCopy;
	protected string m_HandCopyKey;
	protected string m_MenuObjId;
	protected int m_Axis;
	protected bool m_HandVisible;
	protected bool m_HandNoGround;
	protected bool m_Series;
	protected bool m_CatDrag;

	// pointer
	protected int m_Gesture;
	protected bool m_LmbWas;
	protected bool m_RmbWas;
	protected bool m_MmbWas;
	protected int m_PressX;
	protected int m_PressY;
	protected bool m_RPressScene;
	protected bool m_LastInScene;
	protected string m_RPressHover;
	protected Object m_RPressMapObj;
	protected Object m_HoverMapObj;
	protected Object m_MenuMapObj;
	protected string m_PressId;
	protected bool m_PressWasSel;
	protected bool m_PressShift;
	protected bool m_PressCtrl;
	protected bool m_PressAlt;
	protected Object m_PressMapObj;
	protected ref array<string> m_HoverList;
	protected int m_HoverIdx;
	protected int m_HoverAnchorX;
	protected int m_HoverAnchorY;
	protected ref array<float> m_Lasso;
	protected ref array<string> m_LassoHits;
	protected float m_LastLassoX;
	protected ref array<float> m_LassoPoly;
	protected float m_LassoDir;
	protected bool m_LassoDirSet;
	static const float LASSO_MIN_PX = 6;
	static const float LASSO_TURN_DEG = 3;
	protected float m_LastLassoY;
	// the kind of this drag-out: rectangle or lasso (fixed at its start)
	protected bool m_BoxRect;
	protected float m_LassoPrevX;
	protected float m_LassoPrevY;
	protected string m_BoxIcon;

	// gesture working set
	protected ref array<ref DZS_PlaceObj> m_Work;
	protected ref array<ref DZS_PlaceObj> m_Start;
	protected ref array<Object> m_WorkObjs;
	protected int m_DragMode;
	protected vector m_Grab;
	protected vector m_T0;
	protected bool m_T0Set;
	protected vector m_DeltaBase;
	protected int m_SurfLevel;
	protected int m_SurfCount;
	protected vector m_Delta;
	protected float m_RotAngle;
	protected float m_Lift;
	protected float m_LiftAnchor;
	protected float m_LastScreenAngle;
	protected float m_Time;
	protected float m_DbgAt;
	protected float m_SettleUntil;
	protected string m_GestureLabel;
	protected string m_GestureKind;

	// handles
	protected int m_HandleHover = -1;
	protected int m_HandleHeld = -1;
	protected vector m_HAxis;
	protected float m_HStartParam;
	protected float m_HAccum;
	protected float m_HLastRaw;
	protected vector m_HRefVec;
	protected bool m_HEdgeOn;
	protected float m_HTanX;
	protected float m_HTanY;
	protected int m_HLastX;
	protected int m_HLastY;
	protected float m_HStartDist;
	protected float m_HFactor;

	// pivot
	protected bool m_PivotSet;
	protected vector m_Pivot;
	protected string m_SelSig;

	// keys
	protected bool m_KeyGesture;
	protected bool m_KeyNeedRelease;
	protected vector m_KeyMove;
	protected float m_KeyYaw;
	protected float m_KeyRoll;
	protected float m_KeyPitch;
	protected float m_KeyFactor;

	// pick a target (Match height, Align alike)
	protected string m_PickFor;

	protected ref DZS_PlaceCanvas m_Canvas;
	protected float m_DepthLogAt;
	protected float m_LastDt;
	protected ref DZS_PointerHint m_PHint;
	protected ref DZS_ObjectBar m_Bar;

	override string GetName() { return "placement"; }

	override void OnRegister()
	{
		m_Sel = new array<string>();
		m_HoverList = new array<string>();
		m_Lasso = new array<float>();
		m_LassoPoly = new array<float>();
		m_LassoHits = new array<string>();
		m_Work = new array<ref DZS_PlaceObj>();
		m_Start = new array<ref DZS_PlaceObj>();
		m_WorkObjs = new array<Object>();
		m_Escs = new array<ref DZS_PlaceEsc>();
		m_Actions = new DZS_PlaceActions();
		m_Actions.m_P = this;
		m_ForeignBar = new array<ref DZS_BarEntry>();
		m_Service = new DZS_SelectionService();
		m_Service.m_P = this;
		m_Kernel.SetService("selection", m_Service);
		m_Kernel.GetTypes().RegisterOperationType("placement", "placement:edit", 1);
		RegisterTexts();

		DZS_Settings st = m_Kernel.GetSettings();
		st.RegisterTab("placement", "Platzieren", "Placing");
		st.DefineSlider("placement.keySpeed", "placement", "Tempo beim Schieben mit Tasten", "Speed when moving with keys", "Wie schnell Pfeiltasten und Bild↑/↓ die Auswahl schieben", "How fast arrow keys and PgUp/PgDn move the selection", 0.25, 20, 0.25, 2, " m/s", 2);
		st.DefineToggle("placement.straightenPitch", "placement", "Gerade stellen: Nicken", "Straighten: pitch", "Gerade stellen setzt Nicken auf 0", "Straighten sets pitch to 0", true);
		st.DefineToggle("placement.straightenRoll", "placement", "Gerade stellen: Rollen", "Straighten: roll", "Gerade stellen setzt Rollen auf 0", "Straighten sets roll to 0", true);
		st.DefineToggle("placement.straightenYaw", "placement", "Gerade stellen: Gieren", "Straighten: yaw", "Gerade stellen setzt auch Gieren auf 0", "Straighten also sets yaw to 0", false);
		st.Define("placement.boxKind", DZS_Json.Str("rect"));
		st.DefineToggle("placement.followSlope", "placement", "Hang folgen", "Follow slope", "Objekt in der Hand kippt mit der Fläche darunter", "The object in the hand tilts with the surface below", false);

		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("placement", "Platzieren", "Placing");
		ui.RegisterKeyGroup("selection", "Auswahl", "Selection");
		Tool(ui, "placement.tool.select", "Auswahl", "Select", "mouse-pointer-2", 10);
		Tool(ui, "placement.tool.move", "Verschieben", "Move", "move-3d", 20);
		Tool(ui, "placement.tool.rotate", "Drehen", "Rotate", "rotate-3d", 30);
		Tool(ui, "placement.tool.scale", "Skalieren", "Scale", "scale-3d", 40);
		Press(ui, "placement.pipette", "Pipette", "Eyedropper", "I", "scene", "placement");
		ui.RegisterTool("placement", "placement.pipette", "Pipette", "Eyedropper", "switches", "pipette", 5);
		ui.SetToolTip("placement.pipette", "Pipette — Objekt anklicken, eine Kopie kommt in die Hand", "Eyedropper — click an object, a copy goes into the hand", "placement.pipette");
		Press2(ui, "placement.toolCycle", "Verschieben / Drehen / Skalieren reihum", "Cycle move / rotate / scale", "E", "G", "scene", "placement");
		Press(ui, "placement.axis", "Drehachse reihum", "Cycle axis", "R", "hand", "placement");
		Press(ui, "placement.tab", "Durchsteigen", "Step through", "TAB", "scene", "selection");
		Press(ui, "placement.drop", "Fallen lassen", "Drop", "END", "selection", "placement");
		Press(ui, "placement.delete", "Löschen", "Delete", "DELETE", "selection", "placement");
		Press2(ui, "placement.duplicate", "Duplizieren", "Duplicate", "Ctrl+D", "Ctrl+J", "selection", "placement");
		Press(ui, "placement.copy", "Kopieren", "Copy", "Ctrl+C", "scene,map", "placement");
		Press(ui, "placement.cut", "Ausschneiden", "Cut", "Ctrl+X", "selection", "placement");
		Press(ui, "placement.paste", "Einfügen", "Paste", "Ctrl+V", "scene", "placement");
		Press(ui, "placement.pasteOrigin", "An Originalstelle einfügen", "Paste in place", "Ctrl+Shift+V", "scene", "placement");
		Press(ui, "placement.copyText", "Als Text kopieren", "Copy as text", "P", "selection", "placement");
		Press(ui, "placement.boxKind", "Art des Aufziehens", "Selection shape", "V", "scene", "selection");
		ui.SetToolArrow("placement.tool.select", "placement.boxKind");
		ui.SetToolTip("placement.tool.select", "Auswahl — anklicken oder aufziehen", "Select — click or drag out", "placement.toolCycle");
		SyncBoxIcon();
		Press(ui, "placement.lock", "Sperren umschalten", "Toggle lock", "L", "selection", "selection");
		Press(ui, "placement.hide", "Ausblenden umschalten", "Toggle hide", "H", "selection", "selection");
		Press(ui, "placement.unlock", "Entsperren", "Unlock", "Ctrl+L", "selection", "selection");
		Press(ui, "placement.unhide", "Einblenden", "Show", "Ctrl+H", "selection", "selection");
		Press(ui, "placement.selectAll", "Alles auswählen", "Select all", "Ctrl+A", "scene", "selection");
		Press(ui, "placement.invert", "Auswahl umkehren", "Invert selection", "Ctrl+I", "scene", "selection");
		Press(ui, "placement.selectSame", "Gleiche auswählen", "Select same", "Ctrl+Shift+A", "selection", "selection");
		Press(ui, "placement.deselect", "Auswahl aufheben", "Deselect", "", "selection", "selection");
		Press(ui, "placement.straighten", "Gerade stellen", "Straighten", "", "selection", "placement");
		Press(ui, "placement.surface", "An Oberfläche", "Settle on ground", "", "selection", "placement");
		Press(ui, "placement.alignHeight", "Auf Höhe von …", "Match height …", "", "selection", "placement");
		Press(ui, "placement.alignYaw", "Gleich ausrichten …", "Match direction …", "", "selection", "placement");
		Press(ui, "placement.randomYaw", "Zufällig drehen", "Random yaw", "", "selection", "placement");
		Press(ui, "placement.align", "Ausrichten", "Align", "", "selection", "placement");
		Press(ui, "placement.swap", "Austauschen", "Replace", "", "selection", "placement");
		Press(ui, "placement.focus", "Fokus", "Focus", "", "selection", "placement");
		Press(ui, "placement.followSlope", "Hang folgen", "Follow slope", "", "scene", "placement");
		Press(ui, "placement.position", "Position …", "Position …", "", "selection", "placement");
		Press(ui, "placement.dupes", "Doppelte aufräumen …", "Clean up duplicates …", "", "scene", "placement");
		Press(ui, "placement.carry", "Versetzen", "Move to cursor", "", "selection", "placement");
		Press(ui, "placement.pipetteObj", "Davon eins in die Hand", "Take one into the hand", "", "selection", "placement");
		array<string> menuOnly = {"placement.pipetteObj", "placement.carry", "placement.position", "placement.dupes", "placement.deselect", "placement.straighten", "placement.surface", "placement.alignHeight", "placement.alignYaw", "placement.randomYaw", "placement.align", "placement.swap", "placement.focus", "placement.followSlope"};
		foreach (string mo : menuOnly)
			ui.SetListed(mo, false);
		Hold(ui, "placement.key.fwd", "Auswahl vor", "Selection forward", "UP");
		Hold(ui, "placement.key.back", "Auswahl zurück", "Selection back", "DOWN");
		Hold(ui, "placement.key.left", "Auswahl links", "Selection left", "LEFT");
		Hold(ui, "placement.key.right", "Auswahl rechts", "Selection right", "RIGHT");
		Hold(ui, "placement.key.up", "Auswahl hoch", "Selection up", "PRIOR");
		Hold(ui, "placement.key.down", "Auswahl runter", "Selection down", "NEXT");
		Hold(ui, "placement.key.yawL", "Auswahl gieren links", "Selection yaw left", "Ctrl+LEFT");
		Hold(ui, "placement.key.yawR", "Auswahl gieren rechts", "Selection yaw right", "Ctrl+RIGHT");
		Hold(ui, "placement.key.pitchD", "Auswahl nicken −", "Selection pitch −", "Ctrl+NEXT");
		Hold(ui, "placement.key.pitchU", "Auswahl nicken +", "Selection pitch +", "Ctrl+PRIOR");
		Hold(ui, "placement.key.rollB", "Auswahl rollen −", "Selection roll −", "Ctrl+DOWN");
		Hold(ui, "placement.key.rollF", "Auswahl rollen +", "Selection roll +", "Ctrl+UP");
		Hold2(ui, "placement.key.grow", "Auswahl größer", "Selection larger", "PERIOD", "ADD");
		Hold2(ui, "placement.key.shrink", "Auswahl kleiner", "Selection smaller", "COMMA", "SUBTRACT");

		ui.AddMenuEntry("edit", "placement.cut", 100);
		ui.AddMenuEntry("edit", "placement.copy", 104);
		ui.AddMenuEntry("edit", "placement.copyText", 105);
		ui.AddMenuEntry("edit", "placement.paste", 110);
		ui.AddMenuEntry("edit", "placement.pasteOrigin", 111);
		ui.AddMenuEntry("edit", "placement.duplicate", 112);
		ui.AddMenuEntry("edit", "placement.delete", 113);
		ui.AddMenuEntry("edit", "placement.selectAll", 200);
		ui.AddMenuEntry("edit", "placement.invert", 201);
		ui.AddMenuEntry("edit", "placement.selectSame", 202);
		ui.AddMenuEntry("edit", "placement.deselect", 203);
		ui.AddMenuEntry("edit", "placement.lock", 300);
		ui.AddMenuEntry("edit", "placement.hide", 301);
		ui.AddMenuEntry("edit", "placement.straighten", 900);
		ui.AddMenuEntry("edit", "placement.dupes", 901);
		ui.AddMenuEntry("view", "placement.followSlope", 330, true);
		ui.RegisterTool("placement", "placement.followSlope", "Hang folgen", "Follow slope", "switches", "mountain", 30);

		AddEsc("gesture", 40);
		AddEsc("pick", 45);
		AddEsc("selection", 90);
	}

	protected void Tool(DZS_UiRegistry ui, string id, string de, string en, string icon, int order)
	{
		ui.RegisterAction("placement", id, de, en, null, m_Actions);
		ui.SetListed(id, false);
		ui.RegisterTool("placement", id, de, en, "tools", icon, order);
	}

	protected void Press(DZS_UiRegistry ui, string id, string de, string en, string key, string when, string group)
	{
		array<string> keys = new array<string>();
		if (key != "")
			keys.Insert(key);
		ui.RegisterAction("placement", id, de, en, keys, m_Actions);
		ui.SetWhen(id, when);
		ui.SetGroup(id, group);
	}

	protected void Press2(DZS_UiRegistry ui, string id, string de, string en, string key1, string key2, string when, string group)
	{
		array<string> keys = {key1, key2};
		ui.RegisterAction("placement", id, de, en, keys, m_Actions);
		ui.SetWhen(id, when);
		ui.SetGroup(id, group);
	}

	protected void Hold(DZS_UiRegistry ui, string id, string de, string en, string key)
	{
		array<string> keys = {key};
		ui.RegisterAction("placement", id, de, en, keys, m_Actions);
		ui.SetHold(id);
		ui.SetWhen(id, "selection");
		ui.SetGroup(id, "placement");
	}

	protected void Hold2(DZS_UiRegistry ui, string id, string de, string en, string key1, string key2)
	{
		array<string> keys = {key1, key2};
		ui.RegisterAction("placement", id, de, en, keys, m_Actions);
		ui.SetHold(id);
		ui.SetWhen(id, "selection");
		ui.SetGroup(id, "placement");
	}

	protected void AddEsc(string what, int level)
	{
		DZS_PlaceEsc e = new DZS_PlaceEsc();
		e.m_P = this;
		e.m_What = what;
		m_Escs.Insert(e);
		m_Kernel.GetScreen().AddEscHandler(e, level);
	}

	protected void RegisterTexts()
	{
		DZS_Texts.Add("placement.op.place", "Objekt platzieren", "Place object");
		DZS_Texts.Add("placement.op.move", "Verschieben", "Move");
		DZS_Texts.Add("placement.op.rotate", "Drehen", "Rotate");
		DZS_Texts.Add("placement.op.scale", "Skalieren", "Scale");
		DZS_Texts.Add("placement.hand.title", "%1 · Gruppe %2", "%1 · group %2");
		DZS_Texts.Add("placement.hand.click", "Klick absetzen", "Click to place");
		DZS_Texts.Add("placement.noGroundHere", "Kein Untergrund", "No ground");
		DZS_Texts.Add("placement.hand.axis", "[%1] Achse: %2", "[%1] axis: %2");
		DZS_Texts.Add("placement.hand.wheel", "[Wheel] drehen %1", "[Wheel] turn %1");
		DZS_Texts.Add("placement.hand.height", "[Ctrl+Wheel] Höhe %1", "[Ctrl+Wheel] height %1");
		DZS_Texts.Add("placement.axis.0", "Gieren", "Yaw");
		DZS_Texts.Add("placement.axis.1", "Nicken", "Pitch");
		DZS_Texts.Add("placement.axis.2", "Rollen", "Roll");
		DZS_Texts.Add("placement.hint.hand", "Klick absetzen · [Shift]+Klick mehrere · [%1] Achse · [Wheel] drehen · [Ctrl+Wheel] Höhe · [Esc] abbrechen", "Click to place · [Shift]+click several · [%1] axis · [Wheel] turn · [Ctrl+Wheel] height · [Esc] cancel");
		DZS_Texts.Add("placement.hint.carry", "Klick absetzen · [%1] Achse · [Wheel] drehen · [Ctrl+Wheel] Höhe · [Esc] oder Rechtsklick abbrechen", "Click to place · [%1] axis · [Wheel] turn · [Ctrl+Wheel] height · [Esc] or right click cancels");
		DZS_Texts.Add("placement.carry.locked", "Gesperrt", "Locked");
		DZS_Texts.Add("placement.clickToSelect", "Klicken zum Auswählen", "Click to select");
		DZS_Texts.Add("placement.hint.sel", "[%1] sperren · [%2] ausblenden · [%3] duplizieren · [Esc] Auswahl aufheben", "[%1] lock · [%2] hide · [%3] duplicate · [Esc] deselect");
		DZS_Texts.Add("placement.groupLocked", "Gruppe %1 ist gesperrt", "Group %1 is locked");
		DZS_Texts.Add("placement.groupHidden", "Gruppe %1 ist ausgeblendet", "Group %1 is hidden");
		DZS_Texts.Add("placement.noGroup", "Keine Gruppe, die Objekte aufnimmt", "No group that takes objects");
		DZS_Texts.Add("placement.objects", "%1 Objekte", "%1 objects");
		DZS_Texts.Add("placement.moveN", "%1 verschieben", "Move %1");
		DZS_Texts.Add("placement.rotateN", "%1 drehen · %2", "Rotate %1 · %2");
		DZS_Texts.Add("placement.scaleN", "%1 skalieren", "Scale %1");
		DZS_Texts.Add("placement.selectionN", "Auswahl", "Selection");
		DZS_Texts.Add("placement.lockedLeft", "%1 Objekte gelöscht · %2 gesperrte bleiben", "%1 objects deleted · %2 locked stay");
		DZS_Texts.Add("placement.lockedOne", "1 gesperrtes bleibt", "1 locked stays");
		DZS_Texts.Add("placement.lockedMany", "%1 gesperrte bleiben", "%1 locked stay");
		DZS_Texts.Add("placement.copied", "%1 Objekte kopiert", "%1 objects copied");
		DZS_Texts.Add("placement.copied1", "1 Objekt kopiert", "1 object copied");
		DZS_Texts.Add("placement.cutN", "%1 Objekte ausgeschnitten", "%1 objects cut");
		DZS_Texts.Add("placement.nothingToPaste", "Nichts zum Einfügen gefunden", "Nothing to paste found");
		DZS_Texts.Add("placement.textCopied", "%1 Objekte als Text kopiert", "%1 objects copied as text");
		DZS_Texts.Add("placement.textCopied1", "1 Objekt als Text kopiert", "1 object copied as text");
		DZS_Texts.Add("placement.copyText.none", "Nichts ausgewählt — ohne Auswahl kopiert Kopieren die Kameraposition", "Nothing selected — without a selection Copy copies the camera position");
		DZS_Texts.Add("placement.alreadyDown", "Liegt schon auf", "Already resting");
		DZS_Texts.Add("placement.noGround", "Kein Untergrund gefunden", "No ground found");
		DZS_Texts.Add("placement.scaleOut", "%1 Objekte mit Größe außerhalb 0,05–20 — erst in den Eigenschaften anpassen", "%1 objects with size outside 0.05–20 — adjust them in the properties first");
		DZS_Texts.Add("placement.notSpawnable", "%1 spawnt auf Konsole nicht", "%1 does not spawn on console");
		DZS_Texts.Add("placement.takeOver", "[%2]+Klick: ein %1 in die Hand", "[%2]+click: one %1 into the hand");
		DZS_Texts.Add("placement.takeOverTool", "Klick: ein %1 in die Hand", "Click: one %1 into the hand");
		DZS_Texts.Add("placement.pipette.menu", "Davon eins in die Hand", "Take one into the hand");
		DZS_Texts.Add("placement.pick", "Objekt anklicken, dessen Höhe übernommen wird · [Esc] abbrechen", "Click the object whose height is taken · [Esc] cancel");
		DZS_Texts.Add("placement.pickPipette", "Objekt anklicken: eine Kopie kommt in die Hand · [Esc] abbrechen", "Click an object: a copy goes into the hand · [Esc] cancel");
		DZS_Texts.Add("placement.notInCatalog", "%1 steht nicht im Katalog — mit Duplizieren kopieren", "%1 is not in the catalogue — copy it with Duplicate");
		DZS_Texts.Add("placement.pickYaw", "Objekt anklicken, dessen Richtung übernommen wird · [Esc] abbrechen", "Click the object whose direction is taken · [Esc] cancel");
		DZS_Texts.Add("placement.swapBanner", "Austauschen: neues Objekt im Katalog wählen · [Esc] abbrechen", "Replace: choose the new object in the catalogue · [Esc] cancel");
		DZS_Texts.Add("placement.sameN", "%1 %2 ausgewählt", "%1 %2 selected");
		DZS_Texts.Add("placement.sameKinds", "%1 Objekte in %2 Arten ausgewählt", "%1 objects in %2 kinds selected");
		DZS_Texts.Add("placement.noAxis", "In den Einstellungen ist keine Achse gewählt", "No axis is chosen in the settings");
		DZS_Texts.Add("placement.selectFirst", "Erst Objekte auswählen", "Select objects first");
		DZS_Texts.Add("placement.allLocked", "Alle ausgewählten Objekte sind gesperrt", "All selected objects are locked");
		DZS_Texts.Add("placement.nothingInClip", "Nichts zum Einfügen", "Nothing to paste");
	}

	override void OnSessionStart(string world)
	{
		array<string> changing = {"placement.cut", "placement.delete", "placement.lock", "placement.hide", "placement.unlock", "placement.unhide", "placement.straighten", "placement.surface", "placement.align", "placement.alignHeight", "placement.alignYaw", "placement.randomYaw", "placement.swap", "placement.drop", "placement.position"};
		foreach (string chg : changing)
			m_Kernel.GetUi().SetChangesSelection(chg);
		m_World = world != "";
		if (!m_World)
			return;
		DZS_PlaceMath.Calibrate();
		Widget layer = m_Kernel.GetScreen().GetSceneLayer();
		m_Canvas = new DZS_PlaceCanvas(layer);
		m_PHint = new DZS_PointerHint(layer);
		m_Bar = new DZS_ObjectBar(layer, this);
		m_Bar.AddButton("placement.straighten", "ruler", Txt("Gerade stellen", "Straighten"), 10);
		m_Bar.AddButton("placement.surface", "arrow-down-to-dot", Txt("An Oberfläche", "Settle on ground"), 20);
		m_Bar.AddButton("placement.swap", "replace", Txt("Austauschen", "Replace"), 40);
		m_Bar.AddButton("placement.lock", "lock", Txt("Sperren", "Lock"), 50);
		m_Bar.AddButton("placement.hide", "eye-off", Txt("Ausblenden", "Hide"), 60);
		m_Bar.AddButton("placement.duplicate", "copy-plus", Txt("Duplizieren", "Duplicate"), 70);
		m_Bar.AddButton("placement.align", "align-horizontal-distribute-center", Txt("Ausrichten", "Align"), 100);
		m_Bar.AddButton("placement.position", "crosshair", Txt("Position …", "Position …"), 110);
		m_Bar.AddButton("catalog.saveTemplate", "layout-template", Txt("Als Vorlage", "As template"), 120);
		foreach (DZS_BarEntry be : m_ForeignBar)
			m_Bar.AddButton(be.m_Action, be.m_Icon, be.m_De, be.m_Order, be.m_SingleOnly, be.m_En);
		m_Ready = true;
	}

	override void OnSessionEnd()
	{
		DropPreview();
		m_Ready = false;
		m_Sel.Clear();
		m_Lead = "";
	}

	static string Txt(string de, string en)
	{
		return DZS_Texts.Pick(de, en);
	}

	DZS_Kernel Kernel() { return m_Kernel; }

	protected DZS_CameraPlugin Cam()
	{
		return DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
	}

	protected float ViewDist()
	{
		DZS_CameraPlugin cam = Cam();
		if (cam)
			return cam.ViewDistance();
		return 1500;
	}

	// ---- selection ----

	void SetSelection(array<string> ids, string lead)
	{
		array<string> use = new array<string>();
		int asked = 0;
		foreach (string fid : ids)
		{
			asked++;
			if (!DZS_SelectGuard.IsBlocked(fid) || m_Sel.Find(fid) >= 0)
				use.Insert(fid);
		}
		if (asked > 0 && use.Count() == 0)
			return;
		m_Sel.Clear();
		foreach (string id : use)
		{
			if (m_Sel.Find(id) < 0)
				m_Sel.Insert(id);
		}
		if (lead != "" && m_Sel.Find(lead) >= 0)
			m_Lead = lead;
		else if (m_Sel.Count() > 0)
			m_Lead = m_Sel[m_Sel.Count() - 1];
		else
			m_Lead = "";
		SelectionChanged();
	}

	// Selection after undo and redo: exactly the touched objects that exist now; if none, only the gone
	// ones leave the selection.
	protected void SelectAfterStep(DZS_StepObjects so)
	{
		array<string> now = new array<string>();
		array<string> gone = new array<string>();
		foreach (string id : so.m_Ids)
		{
			if (so.m_Inst.Has(id) && so.m_Inst.GetKind(id) == DZS_KIND_OBJECT)
				now.Insert(id);
			else
				gone.Insert(id);
		}
		if (now.Count() > 0)
			SetSelection(now, "");
		else
		{
			array<string> keep = new array<string>();
			foreach (string sid : m_Sel)
			{
				if (gone.Find(sid) < 0)
					keep.Insert(sid);
			}
			if (keep.Count() == 0)
			{
				m_Sel.Clear();
				m_Lead = "";
				SelectionChanged();
			}
			else
				SetSelection(keep, m_Lead);
		}
		DZS_Log.Info("placement: selection after " + DZS_Str.Bool(so.m_Redo) + " step: " + m_Sel.Count() + " of " + so.m_Ids.Count() + " touched");
	}

	void AddToSelection(array<string> ids, string lead)
	{
		foreach (string id : ids)
		{
			if (DZS_SelectGuard.IsBlocked(id) && m_Sel.Find(id) < 0)
				continue;
			int i = m_Sel.Find(id);
			if (i >= 0)
				m_Sel.RemoveOrdered(i);
			m_Sel.Insert(id);
		}
		if (lead != "")
			m_Lead = lead;
		else if (m_Lead == "" && m_Sel.Count() > 0)
			m_Lead = m_Sel[m_Sel.Count() - 1];
		SelectionChanged();
	}

	void RemoveFromSelection(array<string> ids)
	{
		foreach (string id : ids)
		{
			int i = m_Sel.Find(id);
			if (i >= 0)
				m_Sel.RemoveOrdered(i);
		}
		if (m_Sel.Find(m_Lead) < 0)
		{
			m_Lead = "";
			if (m_Sel.Count() > 0)
				m_Lead = m_Sel[m_Sel.Count() - 1];
		}
		SelectionChanged();
	}

	protected void ClearSelection()
	{
		if (m_Sel.Count() == 0)
			return;
		m_Sel.Clear();
		m_Lead = "";
		SelectionChanged();
	}

	protected void SelectionChanged()
	{
		string sig = "";
		foreach (string id : m_Sel)
			sig += id + ",";
		if (sig != m_SelSig)
		{
			m_SelSig = sig;
			m_PivotSet = false;
			DZS_Log.Info("selection: " + m_Sel.Count() + " lead " + m_Lead + " [" + sig + "]");
		}
		DZS_SelectGuard guard = DZS_SelectGuard.Get();
		if (guard)
			guard.SelectionChanged(m_Sel);
		m_Kernel.GetEvents().FirePluginEvent("selection.changed", null);
	}

	// Objects that vanished or whose group became hidden or locked fall out of the selection.
	protected void ValidateSelection(DZS_Instance inst)
	{
		array<string> gone = new array<string>();
		foreach (string id : m_Sel)
		{
			if (!inst.Has(id) || inst.GetKind(id) != DZS_KIND_OBJECT)
			{
				gone.Insert(id);
				continue;
			}
			string g = inst.GetParent(id);
			if (inst.IsHidden(g) || inst.IsLocked(g))
				gone.Insert(id);
		}
		if (gone.Count() > 0)
			RemoveFromSelection(gone);
	}

	// Hittable in the scene: drawn, not locked (itself or group).
	protected bool Selectable(DZS_Instance inst, string id)
	{
		if (id == "" || !inst.Has(id))
			return false;
		string g = inst.GetParent(id);
		return !inst.IsLocked(id) && !inst.IsLocked(g) && !inst.IsHidden(id) && !inst.IsHidden(g);
	}

	// Selectable objects of visible, unlocked groups in group order.
	protected array<string> AllSelectable(DZS_Instance inst)
	{
		array<string> all = new array<string>();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			if (inst.GetKind(g) != DZS_KIND_GROUP)
				continue;
			array<string> ids = inst.GetChildren(g);
			foreach (string id : ids)
			{
				if (Selectable(inst, id))
					all.Insert(id);
			}
		}
		return all;
	}

	// ---- active group ----

	void SetActiveGroup(string groupId)
	{
		DZS_ActiveGroup.Set(m_Kernel.GetShown(), groupId);
	}

	protected string GroupName(DZS_Instance inst, string g)
	{
		string name;
		inst.GetField(g, DZS_Obj.GROUP_NAME, name);
		return name;
	}

	// Message and false when the active group cannot take objects now.
	protected bool ActiveGroupOpen(DZS_Instance inst)
	{
		string active = DZS_ActiveGroup.Get(inst);
		if (active == "")
		{
			if (m_Kernel.GetGroupMaker())
				return true;
			Msg("placement.group", "warning", DZS_Texts.Get("placement.noGroup"));
			return false;
		}
		string gname = GroupName(inst, active);
		if (inst.IsLocked(active))
		{
			Msg("placement.group", "warning", DZS_Texts.Fmt("placement.groupLocked", gname));
			return false;
		}
		if (inst.IsHidden(active))
		{
			Msg("placement.group", "warning", DZS_Texts.Fmt("placement.groupHidden", gname));
			return false;
		}
		return true;
	}

	protected void Msg(string kind, string tone, string text, float seconds = 2.0)
	{
		m_Kernel.GetScreen().ShowMessage(kind, tone, text, seconds);
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		m_SettleUntil = m_Time + 2;
		m_Sel.Clear();
		m_Lead = "";
		m_Gesture = G_NONE;
		m_KeyGesture = false;
		m_Swapping = false;
		m_PickFor = "";
		SelectionChanged();
	}

	// ---- per frame ----

	override void OnUpdate(float dt)
	{
		m_Time += dt;
		m_LastDt = dt;
		string cli;
		if (!m_Ready || GetCLIParam("dzsNoPlaceUpdate", cli))
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		DZS_Screen scr = m_Kernel.GetScreen();
		// no rays or temporary objects while the world is still loading (heap corruption in DayZ 1.30)
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!cat || !cat.IsReady() || GetGame().IsLoading())
			m_SettleUntil = m_Time + 2;
		if (!inst || scr.IsHome() || m_Time < m_SettleUntil)
		{
			m_Canvas.Show(false);
			m_PHint.Hide();
			m_Bar.Hide();
			scr.SetTopReserve(0);
			DropPreview();
			return;
		}
		string skip;
		GetCLIParam("dzsPlaceSkip", skip);
		ValidateSelection(inst);
		SyncHand(inst);
		if (m_Series && m_Hand != "" && !DZS_UiRegistry.ModDown("Shift"))
			EndSeries(inst);
		UpdateSceneState();
		bool waiting = WaitingCommit(inst);
		if (!skip.Contains("pointer") && !waiting)
			UpdatePointer(inst, dt);
		if (!skip.Contains("keys") && !waiting)
			UpdateKeys(inst, dt);
		UpdatePreview(inst);
		if (!skip.Contains("draw"))
			Draw(inst);
		if (!skip.Contains("hints"))
			UpdateHints(inst);
	}

	protected void UpdateSceneState()
	{
		if (m_Kernel.IsSceneStateClaimed())
			return;
		string state = "none";
		if (m_Hand != "" || m_Gesture == G_CARRY)
			state = "hand";
		else if (m_Sel.Count() > 0)
			state = "selection";
		m_Kernel.GetUi().SetSceneState(state);
	}

	protected bool SceneMouseBlocked()
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		return scr.HasDialog() || scr.HasKeyboardOwner("map") || scr.HasKeyboardOwner("menu") || scr.HasKeyboardOwner("fullsearch") || GetGame().GetUIManager().GetMenu() != null;
	}

	protected bool PointerInScene()
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		DZS_CameraPlugin cam = Cam();
		if (cam && cam.IsFreeLook())
			return true;
		return scr.IsPointerOverScene();
	}

	protected vector PointerDir()
	{
		DZS_CameraPlugin cam = Cam();
		if ((cam && cam.IsFreeLook()) || !m_Kernel.GetScreen().IsCursorVisible())
			return GetGame().GetCurrentCameraDirection();
		return GetGame().GetPointerDirection();
	}

	protected void PointerPos(out int x, out int y)
	{
		DZS_CameraPlugin cam = Cam();
		if (cam && cam.IsFreeLook())
		{
			int sw, sh;
			GetScreenSize(sw, sh);
			x = sw / 2;
			y = sh / 2;
			return;
		}
		GetMousePos(x, y);
	}

	// Objects a ray must not meet: preview, objects being moved.
	// The pointer ray for picking: game hits plus the frames of objects the game's rays cannot meet.
	protected array<ref DZS_RayHit> PickRay(array<Object> ex)
	{
		return PickRayFrom(GetGame().GetCurrentCameraPosition(), PointerDir(), ex);
	}

	protected array<ref DZS_RayHit> PickRayFrom(vector from, vector dir, array<Object> ex)
	{
		array<ref DZS_RayHit> hits = DZS_Rays.Ray(from, dir, ViewDist(), ex, false);
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (scene)
		{
			scene.AddBoxHits(from, dir, ViewDist(), ex, hits);
			DZS_Rays.Sort(hits);
		}
		return hits;
	}

	protected array<Object> Excluded()
	{
		array<Object> ex = new array<Object>();
		if (m_Preview)
			ex.Insert(m_Preview);
		foreach (Object o : m_WorkObjs)
		{
			if (o)
				ex.Insert(o);
		}
		return ex;
	}

	// ---- hand ----

	protected void SyncHand(DZS_Instance inst)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		string h = "";
		if (cat)
			h = cat.GetHand();
		if (h == m_Hand)
			return;
		string was = m_Hand;
		m_Hand = h;
		DropPreview();
		if (h == "")
		{
			m_Series = false;
			m_CatDrag = false;
			ResetSurfLevel();
			DZS_Log.Info("placement: hand empty");
			return;
		}
		bool stepping = was != "" && (DZS_UiRegistry.KeyDown("UP") || DZS_UiRegistry.KeyDown("DOWN") || DZS_UiRegistry.KeyDown("PRIOR") || DZS_UiRegistry.KeyDown("NEXT"));
		if (!stepping && !m_KeepHandValues)
		{
			m_HandYpr = "0 0 0";
			m_HandOffset = 0;
			m_HandScale = 1;
		}
		string handLow = h;
		handLow.ToLower();
		if (!m_KeepHandValues || handLow != m_HandCopyKey)
			m_HandCopy = null;
		m_KeepHandValues = false;
		if (was == "" && !DZS_UiRegistry.ModDown("Shift"))
			ClearSelection();
		CancelGesture();
		if (DZS_UiRegistry.KeyDown("MouseLeft") && !PointerInScene())
			m_CatDrag = true;
		MakePreview(inst);
		DZS_Log.Info("placement: in hand " + h + ", ypr " + m_HandYpr.ToString() + ", scale " + m_HandScale + ", offset " + m_HandOffset + ", catalog drag " + m_CatDrag);
	}

	protected bool m_KeepHandValues;

	protected void MakePreview(DZS_Instance inst)
	{
		string name = m_Hand;
		if (name.IndexOf("tpl:") == 0)
			return;
		m_HandObj = new DZS_PlaceObj();
		m_HandObj.m_Name = name;
		m_HandObj.m_Ypr = m_HandYpr;
		m_HandObj.m_Scale = m_HandScale;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		vector bmin = "-0.25 0 -0.25";
		vector bmax = "0.25 0.5 0.25";
		if (scene)
			scene.GetBox(name, bmin, bmax);
		m_HandObj.m_BMin = bmin;
		m_HandObj.m_BMax = bmax;
		m_HandObj.UpdateAxes();
		m_Preview = DZS_ScenePlugin.Make(name, "0 -1000 0", m_HandYpr, m_HandScale, false);
		m_PreviewName = name;
		m_HandVisible = false;
	}

	protected void DropPreview()
	{
		if (m_Preview)
			GetGame().ObjectDelete(m_Preview);
		m_Preview = null;
		m_PreviewName = "";
		m_HandObj = null;
		m_HandVisible = false;
	}

	// Preview where a click would put the object: foot point on the surface under the pointer, raised by
	// the height offset; without a hit no preview and no placing.
	protected void UpdatePreview(DZS_Instance inst)
	{
		if (m_Hand == "" || !m_HandObj)
			return;
		bool over = PointerInScene() && !SceneMouseBlocked();
		if (m_Bar.IsShown() && !m_Kernel.GetScreen().IsPointerOverScene())
			over = false;
		m_HandVisible = over;
		if (!over)
		{
			if (m_Preview)
				m_Preview.SetPosition("0 -1000 0");
			return;
		}
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = PointerDir();
		DZS_RayHit hit = PickSurface(from, dir, Excluded());
		m_HandNoGround = !hit;
		if (!hit)
		{
			if (m_Preview)
				m_Preview.SetPosition("0 -1000 0");
			return;
		}
		vector foot = hit.m_Pos + Vector(0, m_HandOffset, 0);
		vector normal = hit.m_Normal;
		m_HandObj.m_Ypr = m_HandYpr;
		m_HandObj.m_Scale = m_HandScale;
		m_HandObj.UpdateAxes();
		if (FollowSlope() && hit && normal.Length() > 0.5)
			TiltToNormal(m_HandObj, normal.Normalized());
		m_HandObj.m_Pos = m_HandObj.OriginForFoot(foot);
		if (m_Preview)
		{
			if (vector.DistanceSq(m_Preview.GetPosition(), m_HandObj.m_Pos) > 0.000001)
				DZS_WorldLines.Moved();
			m_Preview.SetPosition(m_HandObj.m_Pos);
			m_Preview.SetOrientation(m_HandObj.m_Ypr);
			if (m_Preview.GetScale() != m_HandScale)
				m_Preview.SetScale(m_HandScale);
			m_Preview.Update();
		}
	}

	// Surfaces along a ray: flat ones (at most 60° steep) front to back;
	// a steep first hit gives way to the first surface straight below it, searched from 5 cm in front of the hit.
	protected array<ref DZS_RayHit> SurfaceList(vector from, vector dir, array<Object> ex)
	{
		array<ref DZS_RayHit> hits = DZS_Rays.Ray(from, dir, ViewDist(), ex);
		array<ref DZS_RayHit> list = new array<ref DZS_RayHit>();
		for (int i = 0; i < hits.Count(); i++)
		{
			DZS_RayHit h = hits[i];
			vector n = h.m_Normal;
			if (n.Length() > 0.001)
				n = n.Normalized();
			if (n[1] >= 0.4999)
			{
				list.Insert(h);
				continue;
			}
			if (i > 0)
				continue;
			vector bp, bn;
			if (DZS_Rays.Below(h.m_Pos - dir * 0.05, 0, ex, bp, bn))
			{
				DZS_RayHit b = new DZS_RayHit();
				b.m_Pos = bp;
				b.m_Normal = bn;
				b.m_Dist = vector.Distance(from, bp);
				list.Insert(b);
			}
		}
		return list;
	}

	// The surface of the chosen level (Tab) under a ray; the last one if there are fewer; null if none.
	protected DZS_RayHit PickSurface(vector from, vector dir, array<Object> ex)
	{
		array<ref DZS_RayHit> list = SurfaceList(from, dir, ex);
		m_SurfCount = list.Count();
		if (list.Count() == 0)
			return null;
		int i = m_SurfLevel;
		if (i > list.Count() - 1)
			i = list.Count() - 1;
		return list[i];
	}

	// "· surface 2/3" while not the first surface is chosen.
	protected string SurfSuffix()
	{
		if (m_SurfLevel == 0 || m_SurfCount < 2)
			return "";
		int lvl = m_SurfLevel;
		if (lvl > m_SurfCount - 1)
			lvl = m_SurfCount - 1;
		return Txt(" · Fläche ", " · surface ") + (lvl + 1) + "/" + m_SurfCount;
	}

	protected void ResetSurfLevel()
	{
		if (m_SurfLevel != 0)
			DZS_Log.Info("placement: surface level back to 1");
		m_SurfLevel = 0;
	}

	protected bool FollowSlope()
	{
		return m_Kernel.GetSettings().GetBool("placement.followSlope", false);
	}

	// Tilts an object so that its up axis follows a surface normal; its own pitch and roll stay on top.
	protected void TiltToNormal(DZS_PlaceObj o, vector n)
	{
		vector up = "0 1 0";
		vector axis = up * n;
		float s = axis.Length();
		if (s < 0.0001)
			return;
		axis = axis * (1.0 / s);
		float ang = Math.Atan2(s, vector.Dot(up, n)) * Math.RAD2DEG;
		o.m_R = DZS_PlaceMath.Rot(o.m_R, axis, ang);
		o.m_U = DZS_PlaceMath.Rot(o.m_U, axis, ang);
		o.m_F = DZS_PlaceMath.Rot(o.m_F, axis, ang);
		vector a = DZS_PlaceMath.Angles(o.m_R, o.m_U, o.m_F);
		o.m_Ypr = Vector(m_HandYpr[0], a[1], a[2]);
		o.UpdateAxes();
	}

	protected void PlaceFromHand(DZS_Instance inst, bool keep)
	{
		if (!m_HandObj)
		{
			if (m_Hand.IndexOf("tpl:") == 0)
				PlaceTemplate(inst, keep);
			return;
		}
		if (m_HandNoGround)
		{
			DZS_Log.Info("placement: no ground under the pointer, nothing placed");
			return;
		}
		if (!ActiveGroupOpen(inst))
			return;
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		string group = DZS_ActiveGroup.ForNewObjects(inst, b);
		string id;
		if (m_HandCopy)
		{
			DZS_ElementData d = m_HandCopy.Copy();
			map<string, string> placed = Fields(m_HandObj.m_Name, m_HandObj.m_Pos, m_HandObj.m_Ypr, m_HandScale);
			d.m_Fields.Set(DZS_Obj.POS, placed.Get(DZS_Obj.POS));
			d.m_Fields.Set(DZS_Obj.YPR, placed.Get(DZS_Obj.YPR));
			d.m_Fields.Set(DZS_Obj.SCALE, placed.Get(DZS_Obj.SCALE));
			array<ref DZS_Companion> comps = new array<ref DZS_Companion>();
			for (int ci = 0; ci < d.m_Companions.Count(); ci++)
				comps.Insert(d.m_Companions.GetElement(ci));
			id = b.Create(DZS_KIND_OBJECT, group, DZS_END, d.m_Fields, false, false, comps);
		}
		else
			id = b.Create(DZS_KIND_OBJECT, group, DZS_END, Fields(m_HandObj.m_Name, m_HandObj.m_Pos, m_HandObj.m_Ypr, m_HandScale));
		DZS_SubmitResult r = b.Commit(DZS_Texts.Get("placement.op.place") + ": " + Shown(m_HandObj.m_Name), "Place: " + m_HandObj.m_Name);
		if (!r.Ok())
			return;
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat)
			cat.NoteUsed(m_HandObj.m_Name);
		DZS_Log.Info("placement: placed " + m_HandObj.m_Name + " id " + id + " pos " + m_HandObj.m_Pos.ToString() + " ypr " + m_HandObj.m_Ypr.ToString() + " scale " + m_HandScale + " foot " + m_HandObj.Foot().ToString() + " offset " + m_HandOffset + " keep " + keep);
		array<string> one = {id};
		if (keep || m_Series)
			AddToSelection(one, id);
		else
			SetSelection(one, id);
		if (keep)
			m_Series = true;
		else
		{
			m_Series = false;
			if (cat)
				cat.DropHand();
			SyncHand(inst);
		}
	}

	// Releasing Shift ends a series: the hand empties, the series stays selected.
	protected void EndSeries(DZS_Instance inst)
	{
		m_Series = false;
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat)
			cat.DropHand();
		SyncHand(inst);
		DZS_Log.Info("placement: series ended, " + m_Sel.Count() + " selected");
	}

	bool HandFull() { return m_Hand != ""; }
	bool HandTemplate() { return m_Hand.IndexOf("tpl:") == 0; }

	// Place on the map: foot on the terrain only (no roofs), hand turns and height offset, no slope tilt.
	bool PlaceOnGround(float x, float z)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || m_Hand == "" || !m_HandObj)
			return false;
		vector foot = Vector(x, GetGame().SurfaceY(x, z) + m_HandOffset, z);
		m_HandObj.m_Ypr = m_HandYpr;
		m_HandObj.m_Scale = m_HandScale;
		m_HandObj.UpdateAxes();
		m_HandObj.m_Pos = m_HandObj.OriginForFoot(foot);
		m_HandNoGround = false;
		DZS_Log.Info("placement: on the map at " + x + " " + z + ", foot " + foot.ToString());
		PlaceFromHand(inst, DZS_UiRegistry.ModDown("Shift"));
		return true;
	}

	protected void PlaceTemplate(DZS_Instance inst, bool keep)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!cat)
			return;
		string tid = m_Hand.Substring(4, m_Hand.Length() - 4);
		array<ref DZS_TemplateObject> tobjs = cat.TemplateObjects(tid);
		if (!tobjs || tobjs.Count() == 0)
			return;
		array<ref DZS_PlaceObj> objs = new array<ref DZS_PlaceObj>();
		foreach (DZS_TemplateObject t : tobjs)
		{
			DZS_PlaceObj o = NewObj(t.m_Name, t.m_Pos, t.m_Ypr, t.m_Scale);
			objs.Insert(o);
		}
		array<ref map<string, string>> extra = new array<ref map<string, string>>();
		foreach (DZS_TemplateObject t2 : tobjs)
		{
			map<string, string> ef = new map<string, string>();
			string persist = "0";
			if (t2.m_Persist)
				persist = "1";
			ef.Set(DZS_Obj.PERSIST, persist);
			ef.Set(DZS_Obj.CUSTOM, t2.m_Custom);
			extra.Insert(ef);
		}
		if (!InsertSet(inst, objs, null, extra, true, DZS_Texts.Pick("Vorlage setzen", "Place template")))
			return;
		m_Series = keep;
		if (!keep)
			cat.DropHand();
	}

	protected DZS_PlaceObj NewObj(string name, vector pos, vector ypr, float scale)
	{
		DZS_PlaceObj o = new DZS_PlaceObj();
		o.m_Name = name;
		o.m_Pos = pos;
		o.m_Ypr = ypr;
		o.m_Scale = scale;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		vector bmin = "-0.25 0 -0.25";
		vector bmax = "0.25 0.5 0.25";
		if (scene)
			scene.GetBox(name, bmin, bmax);
		o.m_BMin = bmin;
		o.m_BMax = bmax;
		o.UpdateAxes();
		return o;
	}

	static map<string, string> Fields(string name, vector pos, vector ypr, float scale)
	{
		map<string, string> f = new map<string, string>();
		f.Set(DZS_Obj.NAME, name);
		f.Set(DZS_Obj.POS, DZS_Obj.Vec(pos[0], pos[1], pos[2]));
		f.Set(DZS_Obj.YPR, DZS_Obj.Vec(DZS_PlaceMath.Norm180(ypr[0]), DZS_PlaceMath.Norm180(ypr[1]), DZS_PlaceMath.Norm180(ypr[2])));
		f.Set(DZS_Obj.SCALE, DZS_Obj.Num(scale));
		f.Set(DZS_Obj.PERSIST, "0");
		f.Set(DZS_Obj.CUSTOM, "");
		return f;
	}

	// Name of one object in a step name: its game name in the language shown now, else the
	// class or model file name.
	protected string StepName(string name, bool german)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		string t = "";
		if (cat && cat.InCatalog(name))
			t = cat.ShownName(name);
		if (t == "")
		{
			t = name;
			int slash = t.LastIndexOf("\\");
			if (slash >= 0)
				t = t.Substring(slash + 1, t.Length() - slash - 1);
		}
		return t;
	}

	protected string Shown(string name)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (cat)
			return cat.ShownName(name);
		return name;
	}

	// ---- pointer ----

	protected void UpdatePointer(DZS_Instance inst, float dt)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		bool lmb = DZS_UiRegistry.KeyDown("MouseLeft");
		bool rmb = DZS_UiRegistry.KeyDown("MouseRight");
		bool mmb = DZS_UiRegistry.KeyDown("MouseMiddle");
		bool lDown = lmb && !m_LmbWas;
		bool lUp = !lmb && m_LmbWas;
		bool rDown = rmb && !m_RmbWas;
		bool mDown = mmb && !m_MmbWas;
		m_LmbWas = lmb;
		m_RmbWas = rmb;
		m_MmbWas = mmb;
		if (m_Kernel.GetSceneTool() != "")
		{
			m_Hover = "";
			m_HoverList.Clear();
			m_HandleHover = -1;
			return;
		}
		SyncBoxIcon();
		int mx, my;
		PointerPos(mx, my);
		bool blocked = SceneMouseBlocked();
		bool inScene = PointerInScene() && !blocked;

		// drag from the catalog: releasing over the scene places, over a surface lays it down
		if (m_CatDrag)
		{
			if (lUp)
			{
				m_CatDrag = false;
				if (inScene && m_Hand != "")
					PlaceFromHand(inst, DZS_UiRegistry.ModDown("Shift"));
				else
				{
					DZS_CatalogApi cat0 = DZS_CatalogApi.Get();
					if (cat0)
						cat0.DropHand();
				}
			}
			return;
		}

		if (m_Gesture == G_CARRY)
		{
			if (m_Hand != "")
			{
				CancelGesture();
				return;
			}
			UpdateCarry(inst, inScene);
			if (lDown && inScene && !scr.IsMouseSwallowed())
				CommitCarry(inst);
			m_Hover = "";
			return;
		}

		if (mDown && inScene && DZS_UiRegistry.ModDown("Alt") && m_Sel.Count() > 0)
			SetPivotAtPointer();

		if (m_Hand != "")
		{
			if (lDown && inScene && !scr.IsMouseSwallowed())
			{
				if (DZS_UiRegistry.ModDown("Alt") && TakeOverUnderPointer(inst, true))
					return;
				PlaceFromHand(inst, DZS_UiRegistry.ModDown("Shift"));
			}
			m_Hover = "";
			return;
		}

		if (m_Gesture == G_NONE)
		{
			// the camera hides the pointer on a right press: keep what was under it the frame before
			if (rDown)
			{
				m_RPressScene = m_LastInScene;
				m_RPressHover = m_Hover;
				m_RPressMapObj = null;
				if (m_LastInScene && m_Hover == "")
					m_RPressMapObj = m_HoverMapObj;
			}
			m_LastInScene = inScene;
			UpdateHover(inst, mx, my, inScene);
			if (lDown && inScene && !scr.IsMouseSwallowed())
				Press(inst, mx, my);
			return;
		}

		bool moved = Math.AbsInt(mx - m_PressX) + Math.AbsInt(my - m_PressY) > DZS_Scale.Px(DRAG_PX);
		if (m_Gesture == G_PRESS_OBJ && moved && lmb && m_PressWasSel)
			BeginDrag(inst);
		else if (m_Gesture == G_PRESS_EMPTY && moved && lmb)
			BeginLasso(mx, my);

		if (m_Gesture == G_DRAG)
			UpdateDrag(inst, mx, my);
		else if (m_Gesture == G_LASSO)
			UpdateLasso(inst, mx, my);
		else if (m_Gesture == G_HANDLE)
			UpdateHandle(inst, mx, my);

		if (lUp)
			Release(inst, mx, my);
	}

	// Hits of the pointer ray on placed, selectable objects, front to back (click-through).
	protected void UpdateHover(DZS_Instance inst, int mx, int my, bool inScene)
	{
		m_HandleHover = -1;
		if (!inScene || m_Kernel.GetScreen().IsUiHidden())
		{
			m_Hover = "";
			m_HoverMapObj = null;
			if (m_ListHover != "" && Selectable(inst, m_ListHover))
				m_Hover = m_ListHover;
			m_HoverList.Clear();
			return;
		}
		if (m_PickFor == "")
			m_HandleHover = HandleUnder(inst, mx, my);
		if (Math.AbsInt(mx - m_HoverAnchorX) + Math.AbsInt(my - m_HoverAnchorY) > DZS_Scale.Px(DRAG_PX))
		{
			m_HoverIdx = 0;
			m_HoverAnchorX = mx;
			m_HoverAnchorY = my;
		}
		m_HoverList.Clear();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		array<ref DZS_RayHit> hits = PickRay(Excluded());
		m_HoverMapObj = null;
		if (hits.Count() > 0 && hits[0].m_Obj && scene.IdOf(hits[0].m_Obj) == "")
			m_HoverMapObj = hits[0].m_Obj;
		foreach (DZS_RayHit h : hits)
		{
			string id = scene.IdOf(h.m_Obj);
			if (id != "" && Selectable(inst, id) && m_HoverList.Find(id) < 0)
				m_HoverList.Insert(id);
		}
		// objects standing exactly on top of each other: the selected one counts as the front one
		if (m_HoverList.Count() > 1 && m_Sel.Find(m_HoverList[0]) < 0)
		{
			for (int si = 1; si < m_HoverList.Count(); si++)
			{
				if (m_Sel.Find(m_HoverList[si]) >= 0 && SameSpot(inst, m_HoverList[0], m_HoverList[si]))
				{
					string front = m_HoverList[si];
					m_HoverList.RemoveOrdered(si);
					m_HoverList.InsertAt(front, 0);
					break;
				}
			}
		}
		if (m_HoverIdx >= m_HoverList.Count())
			m_HoverIdx = 0;
		m_Hover = "";
		if (m_HandleHover >= 0)
			return;
		if (m_HoverList.Count() > 0)
			m_Hover = m_HoverList[m_HoverIdx];
	}

	protected bool SameSpot(DZS_Instance inst, string a, string b)
	{
		DZS_PlaceObj pa = DZS_PlaceObj.FromState(inst, a);
		DZS_PlaceObj pb = DZS_PlaceObj.FromState(inst, b);
		return vector.Distance(pa.m_Pos, pb.m_Pos) < 0.01 && TurnBetween(pa, pb) < 0.5;
	}

	protected void Press(DZS_Instance inst, int mx, int my)
	{
		m_PressX = mx;
		m_PressY = my;
		m_PressShift = DZS_UiRegistry.ModDown("Shift");
		m_PressCtrl = DZS_UiRegistry.ModDown("Ctrl");
		m_PressAlt = DZS_UiRegistry.ModDown("Alt");
		if (m_PickFor != "")
		{
			PickTarget(inst);
			return;
		}
		if (m_HandleHover >= 0)
		{
			BeginHandle(inst, mx, my);
			return;
		}
		string id = m_Hover;
		m_PressMapObj = null;
		if (id != "")
		{
			m_PressId = id;
			m_PressWasSel = m_Sel.Find(id) >= 0;
			array<string> one = {id};
			if (m_PressCtrl)
			{
				if (!m_PressWasSel)
					AddToSelection(one, id);
			}
			else if (m_PressShift)
				AddToSelection(one, id);
			else if (!m_PressWasSel)
				SetSelection(one, id);
			m_Gesture = G_PRESS_OBJ;
			return;
		}
		array<ref DZS_RayHit> hits = PickRay(Excluded());
		if (hits.Count() > 0)
			m_PressMapObj = hits[0].m_Obj;
		m_Gesture = G_PRESS_EMPTY;
	}

	protected void Release(DZS_Instance inst, int mx, int my)
	{
		int g = m_Gesture;
		m_Gesture = G_NONE;
		if (g == G_PRESS_OBJ)
		{
			if (m_PressAlt && !m_PressCtrl && PipettePlaced(inst, m_PressId, false))
				return;
			array<string> one = {m_PressId};
			if (m_PressCtrl && m_PressWasSel)
				RemoveFromSelection(one);
			else if (!m_PressCtrl && !m_PressShift && m_PressWasSel)
				SetSelection(one, m_PressId);
		}
		else if (g == G_PRESS_EMPTY)
		{
			if (m_PressAlt || m_PressCtrl)
			{
				if (!TakeOverUnderPointer(inst, false) && !m_PressShift && !m_PressCtrl)
					ClearSelection();
			}
			else if (!m_PressShift && !m_PressCtrl)
				ClearSelection();
		}
		else if (g == G_DRAG || g == G_HANDLE)
		{
			CommitWork(inst);
			ResetSurfLevel();
		}
		else if (g == G_LASSO)
			FinishLasso(inst);
		m_HandleHeld = -1;
	}

	// Alt+click or the pipette tool: the first object under the pointer, of the map or placed.
	protected bool TakeOverUnderPointer(DZS_Instance inst, bool inHand)
	{
		array<ref DZS_RayHit> hits = PickRay(Excluded());
		if (hits.Count() == 0 || !hits[0].m_Obj)
			return false;
		string id = DZS_SceneApi.Get().IdOf(hits[0].m_Obj);
		if (id != "")
			return PipettePlaced(inst, id, inHand);
		return Pipette(hits[0].m_Obj, inHand);
	}

	// A copy of a placed object into the hand: name, turn, size and every other field and companion.
	protected bool PipettePlaced(DZS_Instance inst, string id, bool inHand)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		string name;
		inst.GetField(id, DZS_Obj.NAME, name);
		if (!cat || !cat.InCatalog(name))
		{
			if (inHand)
				return false;
			Msg("placement.takeOver", "warning", DZS_Texts.Fmt("placement.notInCatalog", Shown(name)));
			return true;
		}
		DZS_PlaceObj src = DZS_PlaceObj.FromState(inst, id);
		if (!DZS_UiRegistry.ModDown("Shift"))
			ClearSelection();
		m_KeepHandValues = true;
		m_HandYpr = src.m_Ypr;
		m_HandOffset = 0;
		m_HandScale = Math.Clamp(src.m_Scale, DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX);
		m_HandCopy = inst.GetElementCopy(id);
		m_HandCopyKey = name;
		m_HandCopyKey.ToLower();
		cat.TakeInHand(name);
		DZS_Log.Info("placement: pipette copy of " + id + " (" + name + ") ypr " + m_HandYpr.ToString() + " scale " + m_HandScale + ", " + m_HandCopy.m_Fields.Count() + " fields, " + m_HandCopy.m_Companions.Count() + " companions");
		return true;
	}

	// Catalog entry of a map object: its class, else its model path (rocks and plants have no class).
	protected string CatalogNameOf(Object o)
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!o || !cat)
			return "";
		string t = o.GetType();
		if (t != "" && cat.InCatalog(t))
			return t;
		string shape = o.GetShapeName();
		if (shape == "")
			return "";
		if (cat.InCatalog(shape))
			return shape;
		string withExt = shape + ".p3d";
		if (shape.IndexOf(".p3d") < 0 && cat.InCatalog(withExt))
			return withExt;
		return "";
	}

	protected bool Pipette(Object o, bool inHand)
	{
		string name = CatalogNameOf(o);
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		DZS_Log.Info("placement: pipette at type '" + o.GetType() + "' shape '" + o.GetShapeName() + "' scale " + o.GetScale() + " -> '" + name + "'");
		if (name == "")
		{
			if (inHand)
				return false;
			Msg("placement.takeOver", "warning", DZS_Texts.Fmt("placement.notSpawnable", MapObjectShown(o)));
			return true;
		}
		vector ypr = o.GetOrientation();
		if (!DZS_UiRegistry.ModDown("Shift"))
			ClearSelection();
		m_KeepHandValues = true;
		m_HandYpr = ypr;
		m_HandOffset = 0;
		m_HandScale = Math.Clamp(o.GetScale(), DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX);
		m_HandCopy = null;
		m_HandCopyKey = "";
		cat.TakeInHand(name);
		DZS_Log.Info("placement: taken over " + name + " ypr " + ypr.ToString() + " scale " + m_HandScale);
		return true;
	}

	// ---- moving a selection (drag) ----

	protected void Snapshot(DZS_Instance inst)
	{
		m_Work.Clear();
		m_Start.Clear();
		m_WorkObjs.Clear();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		foreach (string id : m_Sel)
		{
			DZS_PlaceObj o = DZS_PlaceObj.FromState(inst, id);
			if (o.m_Locked)
				continue;
			m_Work.Insert(o);
			DZS_PlaceObj t1298 = o.Copy();
			m_Start.Insert(t1298);
			Object go = scene.GetObject(id);
			if (go)
				m_WorkObjs.Insert(go);
		}
	}

	protected DZS_PlaceObj WorkOf(string id)
	{
		foreach (DZS_PlaceObj o : m_Work)
		{
			if (o.m_Id == id)
				return o;
		}
		return null;
	}

	// Lead object for moving: the lead unless locked, then the last added free one.
	protected DZS_PlaceObj WorkLead()
	{
		DZS_PlaceObj l = WorkOf(m_Lead);
		if (l)
			return l;
		for (int i = m_Sel.Count() - 1; i >= 0; i--)
		{
			DZS_PlaceObj o = WorkOf(m_Sel[i]);
			if (o)
				return o;
		}
		return null;
	}

	protected void BeginDrag(DZS_Instance inst)
	{
		Snapshot(inst);
		if (m_Work.Count() == 0)
		{
			m_Gesture = G_NONE;
			return;
		}
		m_Gesture = G_DRAG;
		m_Kernel.SetGesture("placement", true);
		DZS_PlaceObj grabbed = WorkOf(m_PressId);
		if (!grabbed)
			grabbed = WorkLead();
		m_GestureLabel = grabbed.m_Id;
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		Object go = scene.GetObject(grabbed.m_Id);
		m_Grab = grabbed.Center();
		array<ref DZS_RayHit> hits = PickRayFrom(from, dir, new array<Object>());
		foreach (DZS_RayHit h : hits)
		{
			if (h.m_Obj == go)
			{
				m_Grab = h.m_Pos;
				break;
			}
		}
		m_Delta = "0 0 0";
		m_RotAngle = 0;
		m_Lift = 0;
		ResetSurfLevel();
		Reanchor(grabbed);
		DZS_RayHit t0 = PickSurface(from, dir, Excluded());
		if (t0)
		{
			m_T0 = t0.m_Pos;
			m_T0Set = true;
		}
		m_DragMode = CurrentDragMode();
		DZS_Log.Info("placement: drag start " + m_Work.Count() + " object(s), grab " + m_Grab.ToString() + ", T0 " + m_T0.ToString() + " set " + m_T0Set);
	}

	// Hold keys while dragging: Alt before Ctrl before Shift.
	protected int CurrentDragMode()
	{
		if (DZS_UiRegistry.ModDown("Alt"))
			return DM_HEIGHT;
		if (DZS_UiRegistry.ModDown("Ctrl"))
			return DM_PLANE;
		if (DZS_UiRegistry.ModDown("Shift"))
			return DM_ROTATE;
		return DM_GROUND;
	}

	// Ground dragging starts counting again from the next hit (T0); the angle at the
	// pivot for Shift.
	protected void Reanchor(DZS_PlaceObj grabbed)
	{
		m_T0Set = false;
		m_DeltaBase = m_Delta - Vector(0, m_Lift, 0);
		m_LiftAnchor = m_Lift;
		int mx, my;
		GetMousePos(mx, my);
		float sx, sy;
		vector pivot = WorkPivot();
		if (DZS_PlaceMath.ToScreen(pivot, sx, sy))
			m_LastScreenAngle = Math.Atan2(my - sy, mx - sx) * Math.RAD2DEG;
	}

	protected vector WorkPivot()
	{
		if (m_PivotSet)
			return m_Pivot;
		vector mn, mx;
		BoundsOf(m_Work, mn, mx);
		return (mn + mx) * 0.5;
	}

	// Measured with plain locals: building the working copy here (DZS_PlaceObj.FromStateQuick) for the properties
	// part corrupted the heap of DayZ 1.30 at the next project close (heap search of section 9, see the README).
	bool MeasureOffset(string id, out float offset)
	{
		offset = 0;
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !inst.Has(id))
			return false;
		string name, pos, ypr, scale;
		inst.GetField(id, DZS_Obj.NAME, name);
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		vector p = DZS_Obj.QuickVec(pos);
		vector a = DZS_Obj.QuickVec(ypr);
		float sc = 1;
		if (scale != "")
			sc = scale.ToFloat();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		vector vp, vy;
		float vs;
		if (scene && scene.GetView(id, vp, vy, vs))
		{
			p = vp;
			a = vy;
			sc = vs;
		}
		vector bmin = "-0.25 0 -0.25";
		vector bmax = "0.25 0.5 0.25";
		if (scene)
			scene.GetBox(name, bmin, bmax);
		vector r, u, f;
		DZS_PlaceMath.Axes(a, r, u, f);
		float lx = (bmin[0] + bmax[0]) * 0.5 * sc;
		float ly = bmin[1] * sc;
		float lz = (bmin[2] + bmax[2]) * 0.5 * sc;
		vector foot = p + r * lx + u * ly + f * lz;
		array<Object> ex = new array<Object>();
		Object so = null;
		if (scene)
			so = scene.GetObject(id);
		if (so)
			ex.Insert(so);
		bool ok = MeasureFoot(foot, ex, offset);
		return ok;
	}

	// Height offset of a foot point: above the first surface below it, searched from
	// 5 cm over it (the terrain counts); only with nothing below, against the terrain above it (negative).
	// A ray that starts inside geometry reports its own start: such contacts (within 2 mm) are not surfaces.
	static bool MeasureFoot(vector foot, array<Object> ex, out float offset)
	{
		vector start = foot + "0 0.05 0";
		float ground = GetGame().SurfaceY(foot[0], foot[2]);
		bool underGround = start[1] < ground - 0.002;
		array<ref DZS_RayHit> hs = DZS_Rays.Ray(start, "0 -1 0", 1000, ex, !underGround);
		foreach (DZS_RayHit h : hs)
		{
			if (h.m_Dist < 0.002)
				continue;
			offset = foot[1] - h.m_Pos[1];
			return true;
		}
		if (ground > foot[1])
		{
			offset = foot[1] - ground;
			return true;
		}
		return false;
	}

	bool BoundsOfIds(array<string> ids, out vector mn, out vector mx)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return false;
		array<ref DZS_PlaceObj> list = new array<ref DZS_PlaceObj>();
		foreach (string id : ids)
		{
			DZS_PlaceObj t1461 = CachedObj(inst, id);
			if (t1461)
				list.Insert(t1461);
		}
		if (list.Count() == 0)
			return false;
		BoundsOf(list, mn, mx);
		return true;
	}

	protected void BoundsOf(array<ref DZS_PlaceObj> objs, out vector mn, out vector mx)
	{
		mn = Vector(1000000000.0, 1000000000.0, 1000000000.0);
		mx = Vector(-1000000000.0, -1000000000.0, -1000000000.0);
		array<vector> c = new array<vector>();
		foreach (DZS_PlaceObj o : objs)
		{
			o.Corners(c);
			foreach (vector p : c)
			{
				mn = Vector(Math.Min(mn[0], p[0]), Math.Min(mn[1], p[1]), Math.Min(mn[2], p[2]));
				mx = Vector(Math.Max(mx[0], p[0]), Math.Max(mx[1], p[1]), Math.Max(mx[2], p[2]));
			}
		}
	}

	protected void UpdateDrag(DZS_Instance inst, int mx, int my)
	{
		int mode = CurrentDragMode();
		DZS_PlaceObj grabbed = WorkOf(m_GestureLabel);
		if (!grabbed)
			return;
		if (mode != m_DragMode)
		{
			Reanchor(grabbed);
			m_DragMode = mode;
		}
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		vector move = "0 0 0";
		float lift = 0;
		if (mode == DM_GROUND)
		{
			DZS_RayHit sh = PickSurface(from, dir, Excluded());
			if (sh)
			{
				if (!m_T0Set)
				{
					m_T0 = sh.m_Pos;
					m_T0Set = true;
				}
				move = sh.m_Pos - m_T0 + m_DeltaBase + Vector(0, m_Lift, 0) - m_Delta;
			}
			else
				move = Vector(0, lift, 0);
		}
		else if (mode == DM_PLANE)
		{
			vector pp;
			if (DZS_PlaceMath.RayPlane(from, dir, m_Grab, GetGame().GetCurrentCameraDirection(), pp))
				move = pp - m_Grab;
		}
		else if (mode == DM_HEIGHT)
		{
			vector cd = GetGame().GetCurrentCameraDirection();
			vector flat = Vector(cd[0], 0, cd[2]);
			if (flat.Length() < 0.001)
				flat = "0 0 1";
			flat.Normalize();
			vector hp;
			if (DZS_PlaceMath.RayPlane(from, dir, m_Grab, flat, hp))
				move = Vector(0, hp[1] - m_Grab[1], 0);
		}
		else if (mode == DM_ROTATE)
		{
			vector pivot = WorkPivot();
			float sx, sy;
			if (DZS_PlaceMath.ToScreen(pivot, sx, sy))
			{
				float a = Math.Atan2(my - sy, mx - sx) * Math.RAD2DEG;
				float d = DZS_PlaceMath.Norm180(a - m_LastScreenAngle);
				m_LastScreenAngle = a;
				if (d != 0)
				{
					foreach (DZS_PlaceObj ro : m_Work)
						ro.RotateAround(pivot, "0 1 0", DZS_PlaceMath.YawRot(d));
					m_RotAngle += d;
				}
			}
			move = Vector(0, lift, 0);
		}
		if (move.Length() > 0)
		{
			foreach (DZS_PlaceObj o : m_Work)
				o.m_Pos = o.m_Pos + move;
			m_Grab = m_Grab + move;
			m_Delta = m_Delta + move;
			if (m_PivotSet)
				m_Pivot = m_Pivot + move;
		}
		if (mode == DM_GROUND && m_Work.Count() == 1 && FollowSlope())
		{
			vector below, n;
			if (DZS_Rays.Below(grabbed.Foot(), 0.5, Excluded(), below, n) && n.Length() > 0.5)
			{
				DZS_PlaceObj st = m_Start[0];
				grabbed.m_Ypr = st.m_Ypr;
				grabbed.UpdateAxes();
				vector keepFoot = grabbed.Foot();
				m_HandYpr = st.m_Ypr;
				TiltToNormal(grabbed, n.Normalized());
				grabbed.m_Pos = grabbed.OriginForFoot(keepFoot);
			}
		}
		ShowWork();
		if (m_Time - m_DbgAt > 0.25)
		{
			m_DbgAt = m_Time;
			DZS_PlaceObj dl = WorkLead();
			DZS_PlaceObj ds = LeadOfList(m_Start);
			DZS_Log.Info("placement: drag frame move " + move.ToString() + " grabbed " + grabbed.m_Pos.ToString() + " lead " + dl.m_Pos.ToString() + " start " + ds.m_Pos.ToString() + " work " + m_Work.Count());
		}
		if (mode == DM_ROTATE)
			m_GestureKind = "rotate";
		else
			m_GestureKind = "move";
	}

	protected void ShowWork()
	{
		DZS_SceneApi scene = DZS_SceneApi.Get();
		array<string> lids = new array<string>();
		array<vector> lpos = new array<vector>();
		array<vector> lypr = new array<vector>();
		array<float> lsc = new array<float>();
		foreach (DZS_PlaceObj o : m_Work)
		{
			scene.SetView(o.m_Id, o.m_Pos, o.m_Ypr, o.m_Scale);
			lids.Insert(o.m_Id);
			lpos.Insert(o.m_Pos);
			lypr.Insert(o.m_Ypr);
			lsc.Insert(o.m_Scale);
		}
		DZS_LiveFeed.Report(lids, lpos, lypr, lsc);
		m_WorkObjs.Clear();
		foreach (DZS_PlaceObj o2 : m_Work)
		{
			Object go = scene.GetObject(o2.m_Id);
			if (go)
				m_WorkObjs.Insert(go);
		}
	}

	// "Claiming …": the step of a gesture on objects whose claim is still
	// unconfirmed waits; the result stays as a view until the claim comes or the objects fall out of the selection.
	protected bool m_CommitWait;

	protected array<string> WorkIds()
	{
		array<string> ids = new array<string>();
		foreach (DZS_PlaceObj o : m_Work)
			ids.Insert(o.m_Id);
		return ids;
	}

	protected bool WaitingCommit(DZS_Instance inst)
	{
		if (!m_CommitWait && (m_Gesture == G_DRAG || m_Gesture == G_HANDLE || m_KeyGesture))
		{
			foreach (DZS_PlaceObj w : m_Work)
			{
				if (!IsSelectedFast(w.m_Id))
				{
					DZS_Log.Info("placement: gesture ends, objects left the selection");
					CancelGesture();
					break;
				}
			}
		}
		if (!m_CommitWait)
			return false;
		foreach (DZS_PlaceObj o : m_Work)
		{
			if (!IsSelectedFast(o.m_Id))
			{
				m_CommitWait = false;
				DZS_LiveFeed.End(false);
				m_GestureKind = "";
				DZS_SceneApi scene = DZS_SceneApi.Get();
				foreach (DZS_PlaceObj c : m_Work)
					scene.ClearView(c.m_Id);
				m_Kernel.SetGesture("placement", false);
				m_Work.Clear();
				m_Start.Clear();
				m_WorkObjs.Clear();
				DZS_Log.Info("placement: waiting step dropped, objects left the selection");
				return false;
			}
		}
		if (DZS_SelectGuard.Waits(WorkIds()) != "")
			return true;
		m_CommitWait = false;
		DZS_Log.Info("placement: claim confirmed, step now");
		CommitWork(inst);
		return false;
	}

	protected void CommitWork(DZS_Instance inst)
	{
		if (DZS_SelectGuard.Waits(WorkIds()) != "")
		{
			m_CommitWait = true;
			DZS_Log.Info("placement: step waits for the claim of " + m_Work.Count() + " object(s)");
			return;
		}
		m_Kernel.SetGesture("placement", false);
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		int n = 0;
		for (int i = 0; i < m_Work.Count(); i++)
		{
			DZS_PlaceObj o = m_Work[i];
			DZS_PlaceObj s = m_Start[i];
			bool changed = false;
			if (vector.Distance(o.m_Pos, s.m_Pos) > 0.0000001)
			{
				b.SetField(o.m_Id, DZS_Obj.POS, DZS_Obj.Vec(o.m_Pos[0], o.m_Pos[1], o.m_Pos[2]));
				changed = true;
			}
			if (vector.Distance(o.m_Ypr, s.m_Ypr) > 0.0000001)
			{
				b.SetField(o.m_Id, DZS_Obj.YPR, DZS_Obj.Vec(o.m_Ypr[0], o.m_Ypr[1], o.m_Ypr[2]));
				changed = true;
			}
			if (o.m_Scale != s.m_Scale)
			{
				b.SetField(o.m_Id, DZS_Obj.SCALE, DZS_Obj.Num(o.m_Scale));
				changed = true;
			}
			if (changed)
				n++;
		}
		string kind = m_GestureKind;
		m_GestureKind = "";
		DZS_SceneApi scene = DZS_SceneApi.Get();
		DZS_LiveFeed.End(n > 0);
		if (n == 0)
		{
			foreach (DZS_PlaceObj c : m_Work)
				scene.ClearView(c.m_Id);
			m_Work.Clear();
			m_Start.Clear();
			m_WorkObjs.Clear();
			DZS_Log.Info("placement: gesture without change");
			return;
		}
		string whoDe = n.ToString() + " Objekte";
		string whoEn = n.ToString() + " objects";
		if (n == 1)
		{
			string oneName;
			for (int wi = 0; wi < m_Work.Count(); wi++)
			{
				if (m_Work[wi].m_Pos != m_Start[wi].m_Pos || m_Work[wi].m_Ypr != m_Start[wi].m_Ypr || m_Work[wi].m_Scale != m_Start[wi].m_Scale)
					oneName = m_Work[wi].m_Name;
			}
			whoDe = StepName(oneName, true);
			whoEn = StepName(oneName, false);
		}
		string de = whoDe + " verschieben";
		string en = "Move " + whoEn;
		if (kind == "rotate")
		{
			de = whoDe + " drehen";
			en = "Rotate " + whoEn;
		}
		else if (kind == "scale")
		{
			de = whoDe + " skalieren";
			en = "Scale " + whoEn;
		}
		DZS_SubmitResult r = b.Commit(de, en);
		DZS_PlaceObj l = WorkLead();
		string leadText = "";
		if (l)
			leadText = " lead " + l.m_Id + " pos " + l.m_Pos.ToString() + " ypr " + l.m_Ypr.ToString() + " scale " + l.m_Scale + " foot " + l.Foot().ToString();
		DZS_Log.Info("placement: " + kind + " committed " + n + " object(s) ok " + r.Ok() + leadText);
		if (!r.Ok())
		{
			foreach (DZS_PlaceObj c2 : m_Work)
				scene.ClearView(c2.m_Id);
		}
		m_Work.Clear();
		m_Start.Clear();
		m_WorkObjs.Clear();
	}

	protected void CancelGesture()
	{
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (m_Gesture == G_DRAG)
			ResetSurfLevel();
		if (m_Gesture == G_CARRY)
		{
			ResetSurfLevel();
			m_Kernel.ClaimWheel("placement", false);
			DZS_Log.Info("placement: carry cancelled");
		}
		if (m_Gesture == G_DRAG || m_Gesture == G_HANDLE || m_Gesture == G_CARRY || m_KeyGesture)
		{
			foreach (DZS_PlaceObj o : m_Work)
				scene.ClearView(o.m_Id);
			m_Kernel.SetGesture("placement", false);
			DZS_LiveFeed.End(false);
			DZS_Log.Info("placement: gesture cancelled");
		}
		if (m_KeyGesture)
			m_KeyNeedRelease = true;
		m_KeyGesture = false;
		m_Work.Clear();
		m_Start.Clear();
		m_WorkObjs.Clear();
		m_Gesture = G_NONE;
		m_HandleHeld = -1;
		m_Lasso.Clear();
		m_LassoHits.Clear();
		m_GestureKind = "";
	}

	// ---- drag-select ----

	bool BoxKindRect()
	{
		return m_Kernel.GetSettings().GetString("placement.boxKind", "rect") != "lasso";
	}

	protected void SetBoxKind(string kind)
	{
		m_Kernel.GetSettings().Set("placement.boxKind", DZS_Json.Str(kind));
		SyncBoxIcon();
		DZS_Log.Info("placement: box kind " + kind + ", tool " + m_Tool);
	}

	protected void SyncBoxIcon()
	{
		string icon = "lasso-select";
		if (BoxKindRect())
			icon = "square-dashed-mouse-pointer";
		if (icon == m_BoxIcon)
			return;
		m_BoxIcon = icon;
		m_Kernel.GetUi().SetToolIcon("placement.tool.select", icon);
	}

	protected void OpenBoxKindMenu()
	{
		DZS_UiRegistry ui = m_Kernel.GetUi();
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		bool rect = BoxKindRect();
		DZS_MenuItem r = new DZS_MenuItem();
		r.m_Text = DZS_Texts.Pick("Rechteck", "Rectangle");
		r.m_Check = true;
		r.m_Checked = rect;
		r.m_Tag = "rect";
		items.Insert(r);
		DZS_MenuItem l = new DZS_MenuItem();
		l.m_Text = "Lasso";
		l.m_Check = true;
		l.m_Checked = !rect;
		l.m_Tag = "lasso";
		items.Insert(l);
		DZS_Popup pop = m_Kernel.GetScreen().OpenContextMenu(items, ui.m_ArrowX, ui.m_ArrowY + ui.m_ArrowH + DZS_Scale.Px(2));
		pop.m_OnChoose.Insert(OnBoxKindChoose);
		DZS_Log.Info("placement: box kind menu open, rect " + rect);
	}

	void OnBoxKindChoose(DZS_Popup p, DZS_MenuItem it)
	{
		m_Kernel.GetScreen().CloseMenu();
		SetBoxKind(it.m_Tag);
	}

	protected void BeginLasso(int mx, int my)
	{
		m_Gesture = G_LASSO;
		m_Lasso.Clear();
		m_Lasso.Insert(m_PressX);
		m_Lasso.Insert(m_PressY);
		m_LastLassoX = m_PressX;
		m_LastLassoY = m_PressY;
		m_LassoDirSet = false;
		m_LassoPrevX = m_PressX;
		m_LassoPrevY = m_PressY;
		m_LassoPoly.Clear();
		m_BoxRect = BoxKindRect();
		m_Kernel.SetGesture("placement", true);
	}

	protected void UpdateLasso(DZS_Instance inst, int mx, int my)
	{
		// a new point only at least 6 px from the last one and more than 3° off the last stretch;
		// the last piece always reaches to the pointer
		float dx = mx - m_LastLassoX;
		float dy = my - m_LastLassoY;
		if (m_BoxRect)
		{
			m_Lasso.Clear();
			m_Lasso.Insert(m_PressX);
			m_Lasso.Insert(m_PressY);
			m_Lasso.Insert(mx);
			m_Lasso.Insert(m_PressY);
			m_Lasso.Insert(mx);
			m_Lasso.Insert(my);
			m_LassoPoly.Clear();
			m_LassoPoly.Copy(m_Lasso);
			m_LassoPoly.Insert(m_PressX);
			m_LassoPoly.Insert(my);
		}
		else if (!m_LassoDirSet)
		{
			if (Math.Sqrt(dx * dx + dy * dy) >= DZS_Scale.Px(LASSO_MIN_PX))
			{
				m_LassoDir = Math.Atan2(dy, dx) * Math.RAD2DEG;
				m_LassoDirSet = true;
			}
		}
		else
		{
			// the pointer's own direction (last frame to this one) against the last stretch: a turn puts a point
			// where the pointer was before it turned, so corners stay
			float tx = mx - m_LassoPrevX;
			float ty = my - m_LassoPrevY;
			float bx = m_LassoPrevX - m_LastLassoX;
			float by = m_LassoPrevY - m_LastLassoY;
			if (Math.Sqrt(tx * tx + ty * ty) >= 1 && Math.Sqrt(bx * bx + by * by) >= DZS_Scale.Px(LASSO_MIN_PX))
			{
				float stepDir = Math.Atan2(ty, tx) * Math.RAD2DEG;
				if (Math.AbsFloat(DZS_PlaceMath.Norm180(stepDir - m_LassoDir)) > LASSO_TURN_DEG)
				{
					m_Lasso.Insert(m_LassoPrevX);
					m_Lasso.Insert(m_LassoPrevY);
					m_LastLassoX = m_LassoPrevX;
					m_LastLassoY = m_LassoPrevY;
					m_LassoDirSet = false;
				}
			}
		}
		m_LassoPrevX = mx;
		m_LassoPrevY = my;
		if (!m_BoxRect)
		{
			m_LassoPoly.Clear();
			m_LassoPoly.Copy(m_Lasso);
			m_LassoPoly.Insert(mx);
			m_LassoPoly.Insert(my);
		}
		m_LassoHits.Clear();
		if (m_LassoPoly.Count() < 6)
			return;
		vector cam = GetGame().GetCurrentCameraPosition();
		float vd = ViewDist();
		array<string> all = AllSelectable(inst);
		foreach (string id : all)
		{
			DZS_PlaceObj o = DZS_PlaceObj.FromState(inst, id);
			vector c = o.Center();
			if (vector.Distance(c, cam) > vd)
				continue;
			float sx, sy;
			if (!DZS_PlaceMath.ToScreen(c, sx, sy))
				continue;
			if (DZS_PlaceMath.InPolygon(sx, sy, m_LassoPoly))
				m_LassoHits.Insert(id);
		}
	}

	protected void FinishLasso(DZS_Instance inst)
	{
		m_Kernel.SetGesture("placement", false);
		array<string> hits = new array<string>();
		hits.Copy(m_LassoHits);
		bool shift = DZS_UiRegistry.ModDown("Shift");
		bool ctrl = DZS_UiRegistry.ModDown("Ctrl");
		if (ctrl)
			RemoveFromSelection(hits);
		else if (shift)
		{
			string keep = m_Lead;
			AddToSelection(hits, keep);
		}
		else
		{
			string lead = "";
			float best = 1000000000.0;
			foreach (string id : hits)
			{
				DZS_PlaceObj o = DZS_PlaceObj.FromState(inst, id);
				float sx, sy;
				if (DZS_PlaceMath.ToScreen(o.Center(), sx, sy))
				{
					float d = (sx - m_PressX) * (sx - m_PressX) + (sy - m_PressY) * (sy - m_PressY);
					if (d < best)
					{
						best = d;
						lead = id;
					}
				}
			}
			SetSelection(hits, lead);
		}
		float gap = 1000000000.0;
		int pn = m_Lasso.Count() / 2;
		for (int pi = 1; pi < pn; pi++)
		{
			float gx = m_Lasso[2 * pi] - m_Lasso[2 * pi - 2];
			float gy = m_Lasso[2 * pi + 1] - m_Lasso[2 * pi - 1];
			gap = Math.Min(gap, Math.Sqrt(gx * gx + gy * gy));
		}
		string boxKind = "lasso";
		if (m_BoxRect)
			boxKind = "rect";
		DZS_Log.Info("placement: " + boxKind + " " + pn + " points (min gap " + gap + " px, scale " + DZS_Scale.Px(1) + "), " + hits.Count() + " hit(s), shift " + shift + ", ctrl " + ctrl);
		m_Lasso.Clear();
		m_LassoPoly.Clear();
		m_LassoHits.Clear();
	}

	// ---- handles ----

	protected bool HandlesShown()
	{
		return m_Tool != "placement.tool.select" && m_Hand == "" && m_Sel.Count() > 0 && !m_Kernel.GetScreen().IsUiHidden() && !SceneMouseBlocked() && m_Kernel.GetSceneTool() == "";
	}

	// Free objects of the selection as working copies (without starting a gesture).
	protected array<ref DZS_PlaceObj> FreeSelection(DZS_Instance inst)
	{
		array<ref DZS_PlaceObj> list = new array<ref DZS_PlaceObj>();
		foreach (string id : m_Sel)
		{
			DZS_PlaceObj cached = CachedObj(inst, id);
			if (cached && !cached.m_Locked)
			{
				DZS_PlaceObj o = cached.Copy();
				list.Insert(o);
			}
		}
		return list;
	}

	protected DZS_PlaceObj LeadOf(array<ref DZS_PlaceObj> list)
	{
		foreach (DZS_PlaceObj o : list)
		{
			if (o.m_Id == m_Lead)
				return o;
		}
		for (int i = m_Sel.Count() - 1; i >= 0; i--)
		{
			foreach (DZS_PlaceObj o2 : list)
			{
				if (o2.m_Id == m_Sel[i])
					return o2;
			}
		}
		return null;
	}

	protected vector PivotOf(array<ref DZS_PlaceObj> list)
	{
		if (m_PivotSet)
			return m_Pivot;
		vector mn, mx;
		BoundsOf(list, mn, mx);
		return (mn + mx) * 0.5;
	}

	// World length that shows as 'px' screen pixels at the pivot.
	protected float WorldLen(vector pivot, float px)
	{
		vector cam = GetGame().GetCurrentCameraPosition();
		vector camF = GetGame().GetCurrentCameraDirection();
		vector side = camF * Vector(0, 1, 0);
		if (side.Length() < 0.001)
			side = Vector(1, 0, 0);
		side = side.Normalized();
		float x1, y1, x2, y2;
		if (!DZS_PlaceMath.ToScreen(pivot, x1, y1) || !DZS_PlaceMath.ToScreen(pivot + side, x2, y2))
			return 1;
		float ppm = Math.Sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
		if (ppm < 0.0001)
			return 1;
		return px / ppm;
	}

	protected void HandleAxes(DZS_PlaceObj lead, array<vector> axes)
	{
		axes.Clear();
		axes.Insert(lead.m_R);
		axes.Insert(lead.m_U);
		axes.Insert(lead.m_F);
	}

	// Handle under the pointer: 0–2 arrows / rings (X, Y, Z), 3 the scale square; −1 none.
	protected int HandleUnder(DZS_Instance inst, int mx, int my)
	{
		if (!HandlesShown())
			return -1;
		array<ref DZS_PlaceObj> list = FreeSelection(inst);
		DZS_PlaceObj lead = LeadOf(list);
		if (!lead)
			return -1;
		vector pivot = PivotOf(list);
		float px, py;
		if (!DZS_PlaceMath.ToScreen(pivot, px, py))
			return -1;
		float hit = DZS_Scale.Px(HANDLE_HIT);
		if (m_Tool == "placement.tool.scale")
		{
			float off = DZS_Scale.Px(72) * 0.70710678;
			float half = DZS_Scale.Px(10) + hit / 2;
			if (Math.AbsFloat(mx - (px + off)) <= half && Math.AbsFloat(my - (py - off)) <= half)
				return 3;
			return -1;
		}
		array<vector> axes = new array<vector>();
		HandleAxes(lead, axes);
		float len = WorldLen(pivot, DZS_Scale.Px(HANDLE_PX));
		int best = -1;
		float bestD = hit;
		for (int i = 0; i < 3; i++)
		{
			float d = 1000000000.0;
			if (m_Tool == "placement.tool.move")
			{
				float ex, ey;
				if (!ArrowTip(pivot, axes[i] * len, px, py, ex, ey))
					continue;
				d = DZS_PlaceMath.DistToSegment(mx, my, px, py, ex, ey);
			}
			else
			{
				vector a, b;
				RingPlane(axes, i, a, b);
				float r = len * 0.75;
				float lx, ly;
				bool hasLast = false;
				for (int k = 0; k <= 48; k++)
				{
					float sx, sy;
					RingPoint(pivot, a, b, r, k, px, py, sx, sy);
					if (hasLast)
						d = Math.Min(d, DZS_PlaceMath.DistToSegment(mx, my, lx, ly, sx, sy));
					lx = sx;
					ly = sy;
					hasLast = true;
				}
			}
			if (d <= bestD)
			{
				bestD = d;
				best = i;
			}
		}
		return best;
	}

	// Screen end of a move arrow drawn flat at the pivot; false when it points almost at the camera.
	protected bool ArrowTip(vector pivot, vector d, float px, float py, out float ex, out float ey)
	{
		float dx, dy;
		DZS_PlaceMath.Flat(pivot, d, dx, dy);
		ex = px + dx;
		ey = py + dy;
		return Math.Sqrt(dx * dx + dy * dy) >= DZS_Scale.Px(24);
	}

	// Point k of 48 on a rotate ring, drawn flat at the pivot.
	protected void RingPoint(vector pivot, vector a, vector b, float r, int k, float px, float py, out float x, out float y)
	{
		float ang = k * Math.PI2 / 48;
		float dx, dy;
		DZS_PlaceMath.Flat(pivot, a * (r * Math.Cos(ang)) + b * (r * Math.Sin(ang)), dx, dy);
		x = px + dx;
		y = py + dy;
	}

	protected void RingPlane(array<vector> axes, int i, out vector a, out vector b)
	{
		if (i == 0)
		{
			a = axes[1];
			b = axes[2];
		}
		else if (i == 1)
		{
			a = axes[2];
			b = axes[0];
		}
		else
		{
			a = axes[0];
			b = axes[1];
		}
	}

	protected void BeginHandle(DZS_Instance inst, int mx, int my)
	{
		Snapshot(inst);
		DZS_PlaceObj lead = WorkLead();
		if (!lead)
			return;
		vector bad = OutOfRangeScale();
		if (m_Tool == "placement.tool.scale" && bad[0] > 0)
		{
			int nBad = Math.Round(bad[0]);
			Msg("placement.scale", "warning", DZS_Texts.Fmt("placement.scaleOut", nBad.ToString()));
			m_Work.Clear();
			m_Start.Clear();
			return;
		}
		m_Gesture = G_HANDLE;
		m_HandleHeld = m_HandleHover;
		m_Kernel.SetGesture("placement", true);
		m_HAccum = 0;
		m_HLastX = mx;
		m_HLastY = my;
		vector pivot = WorkPivot();
		array<vector> axes = new array<vector>();
		HandleAxes(lead, axes);
		if (m_HandleHeld < 3)
			m_HAxis = axes[m_HandleHeld];
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		if (m_Tool == "placement.tool.move")
		{
			m_HLastRaw = AxisParam(from, dir, pivot, m_HAxis);
			m_GestureKind = "move";
		}
		else if (m_Tool == "placement.tool.rotate")
		{
			m_GestureKind = "rotate";
			vector camF = GetGame().GetCurrentCameraDirection();
			m_HEdgeOn = Math.AbsFloat(vector.Dot(camF, m_HAxis)) < 0.2588;
			vector hp;
			if (!m_HEdgeOn && DZS_PlaceMath.RayPlane(from, dir, pivot, m_HAxis, hp))
				m_HRefVec = hp - pivot;
			else
				m_HEdgeOn = true;
			if (m_HEdgeOn)
			{
				// direction along the ring at the grabbed point on the screen
				float sx, sy;
				DZS_PlaceMath.ToScreen(pivot, sx, sy);
				vector rad = Vector(mx - sx, 0, my - sy);
				m_HTanX = -rad[2];
				m_HTanY = rad[0];
				float tl = Math.Sqrt(m_HTanX * m_HTanX + m_HTanY * m_HTanY);
				if (tl < 0.001)
				{
					m_HTanX = 1;
					m_HTanY = 0;
				}
				else
				{
					m_HTanX = m_HTanX / tl;
					m_HTanY = m_HTanY / tl;
				}
			}
		}
		else
		{
			m_GestureKind = "scale";
			float px, py;
			DZS_PlaceMath.ToScreen(pivot, px, py);
			m_HStartDist = Math.Max(1, Math.Sqrt((mx - px) * (mx - px) + (my - py) * (my - py)));
			m_HFactor = 1;
		}
		DZS_Log.Info("placement: handle " + m_HandleHeld + " of " + m_Tool + " held");
	}

	// Parameter along an axis line (through 'base') of the point nearest to the pointer ray.
	protected float AxisParam(vector from, vector dir, vector base, vector axis)
	{
		vector w = from - base;
		float a = vector.Dot(axis, axis);
		float b = vector.Dot(axis, dir);
		float c = vector.Dot(dir, dir);
		float d = vector.Dot(axis, w);
		float e = vector.Dot(dir, w);
		float den = a * c - b * b;
		if (Math.AbsFloat(den) < 0.000001)
			return 0;
		return (d * c - b * e) / den;
	}

	protected void UpdateHandle(DZS_Instance inst, int mx, int my)
	{
		bool fine = DZS_UiRegistry.ModDown("Shift");
		bool snap = DZS_UiRegistry.ModDown("Ctrl");
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		vector pivot0 = WorkPivotStart();
		RestoreStart();
		if (m_Tool == "placement.tool.move")
		{
			float raw = AxisParam(from, dir, pivot0 + m_HAxis * m_HAccum, m_HAxis) + m_HAccum;
			float step = raw - m_HLastRaw;
			m_HLastRaw = raw;
			if (fine)
				step *= 0.1;
			m_HAccum += step;
			float dist = m_HAccum;
			if (snap)
				dist = Math.Round(dist / 0.5) * 0.5;
			vector mv = m_HAxis * dist;
			foreach (DZS_PlaceObj o : m_Work)
				o.m_Pos = o.m_Pos + mv;
			m_Delta = mv;
		}
		else if (m_Tool == "placement.tool.rotate")
		{
			float d = 0;
			if (m_HEdgeOn)
			{
				d = ((mx - m_HLastX) * m_HTanX + (my - m_HLastY) * m_HTanY) * 0.5 / DZS_Scale.Factor();
			}
			else
			{
				vector hp;
				if (DZS_PlaceMath.RayPlane(from, dir, pivot0, m_HAxis, hp))
				{
					vector v = hp - pivot0;
					float cr = vector.Dot(m_HAxis, m_HRefVec * v);
					float dt2 = vector.Dot(m_HRefVec, v);
					d = Math.Atan2(cr, dt2) * Math.RAD2DEG;
					m_HRefVec = v;
				}
			}
			m_HLastX = mx;
			m_HLastY = my;
			if (fine)
				d *= 0.1;
			m_HAccum += d;
			float ang = m_HAccum;
			if (snap)
				ang = Math.Round(ang / 15) * 15;
			foreach (DZS_PlaceObj ro : m_Work)
				ro.RotateAround(pivot0, m_HAxis, ang);
			m_RotAngle = ang;
		}
		else
		{
			float px, py;
			DZS_PlaceMath.ToScreen(pivot0, px, py);
			float dist2 = Math.Max(1, Math.Sqrt((mx - px) * (mx - px) + (my - py) * (my - py)));
			float raw2 = dist2 / m_HStartDist;
			float f = raw2;
			if (fine)
				f = 1 + (raw2 - 1) * 0.1;
			DZS_PlaceObj lead = LeadOfList(m_Start);
			if (snap && lead)
			{
				float target = Math.Round((lead.m_Scale * f - lead.m_Scale) / 0.25) * 0.25 + lead.m_Scale;
				f = target / lead.m_Scale;
			}
			f = ClampFactor(f);
			foreach (DZS_PlaceObj so : m_Work)
				so.ScaleAround(pivot0, f);
			m_HFactor = f;
		}
		ShowWork();
	}

	protected DZS_PlaceObj LeadOfList(array<ref DZS_PlaceObj> list)
	{
		return LeadOf(list);
	}

	protected vector WorkPivotStart()
	{
		if (m_PivotSet)
			return m_Pivot;
		vector mn, mx;
		BoundsOf(m_Start, mn, mx);
		return (mn + mx) * 0.5;
	}

	protected void RestoreStart()
	{
		for (int i = 0; i < m_Work.Count(); i++)
		{
			DZS_PlaceObj s = m_Start[i];
			DZS_PlaceObj o = m_Work[i];
			o.m_Pos = s.m_Pos;
			o.m_Ypr = s.m_Ypr;
			o.m_Scale = s.m_Scale;
			o.m_R = s.m_R;
			o.m_U = s.m_U;
			o.m_F = s.m_F;
		}
	}

	// Factor limited so that every size stays within 0.05–20.
	protected float ClampFactor(float f)
	{
		float lo = 0;
		float hi = 1000000000.0;
		foreach (DZS_PlaceObj s : m_Start)
		{
			if (s.m_Scale <= 0)
				continue;
			lo = Math.Max(lo, DZS_Obj.SCALE_MIN / s.m_Scale);
			hi = Math.Min(hi, DZS_Obj.SCALE_MAX / s.m_Scale);
		}
		return Math.Clamp(f, lo, hi);
	}

	// x: number of objects with a size outside 0.05–20 in the working set.
	protected vector OutOfRangeScale()
	{
		int n = 0;
		foreach (DZS_PlaceObj s : m_Start)
		{
			if (s.m_Scale < DZS_Obj.SCALE_MIN || s.m_Scale > DZS_Obj.SCALE_MAX)
				n++;
		}
		return Vector(n, 0, 0);
	}

	// ---- pivot ----

	protected void SetPivotAtPointer()
	{
		array<ref DZS_RayHit> hits = DZS_Rays.Ray(GetGame().GetCurrentCameraPosition(), PointerDir(), ViewDist(), new array<Object>());
		if (hits.Count() == 0)
		{
			m_PivotSet = false;
			DZS_Log.Info("placement: pivot reset");
			return;
		}
		m_PivotSet = true;
		m_Pivot = hits[0].m_Pos;
		DZS_Log.Info("placement: pivot at " + m_Pivot.ToString());
	}

	// ---- keys ----

	protected void UpdateKeys(DZS_Instance inst, float dt)
	{
		DZS_UiRegistry ui = m_Kernel.GetUi();
		bool blocked = m_Kernel.GetScreen().IsSceneKeyboardBlocked() || m_Hand != "" || m_Sel.Count() == 0 || m_Gesture != G_NONE;
		vector mv = "0 0 0";
		float yaw = 0;
		float roll = 0;
		float pitch = 0;
		float grow = 0;
		if (!blocked)
		{
			if (ui.IsHeld("placement.key.fwd")) mv[2] = mv[2] + 1;
			if (ui.IsHeld("placement.key.back")) mv[2] = mv[2] - 1;
			if (ui.IsHeld("placement.key.right")) mv[0] = mv[0] + 1;
			if (ui.IsHeld("placement.key.left")) mv[0] = mv[0] - 1;
			if (ui.IsHeld("placement.key.up")) mv[1] = mv[1] + 1;
			if (ui.IsHeld("placement.key.down")) mv[1] = mv[1] - 1;
			if (ui.IsHeld("placement.key.yawR")) yaw += 1;
			if (ui.IsHeld("placement.key.yawL")) yaw -= 1;
			if (ui.IsHeld("placement.key.rollF")) roll += 1;
			if (ui.IsHeld("placement.key.rollB")) roll -= 1;
			if (ui.IsHeld("placement.key.pitchU")) pitch += 1;
			if (ui.IsHeld("placement.key.pitchD")) pitch -= 1;
			if (ui.IsHeld("placement.key.grow")) grow += 1;
			if (ui.IsHeld("placement.key.shrink")) grow -= 1;
		}
		bool any = mv.Length() > 0 || yaw != 0 || roll != 0 || pitch != 0 || grow != 0;
		bool anyDown = AnyKeyGestureKey();
		if (m_KeyNeedRelease)
		{
			if (!anyDown)
				m_KeyNeedRelease = false;
			return;
		}
		if (!m_KeyGesture)
		{
			if (!any)
				return;
			Snapshot(inst);
			if (m_Work.Count() == 0)
				return;
			vector bad = OutOfRangeScale();
			if (grow != 0 && bad[0] > 0)
			{
				int nBad = Math.Round(bad[0]);
				string n = nBad.ToString();
				Msg("placement.scale", "warning", DZS_Texts.Fmt("placement.scaleOut", n));
				m_Work.Clear();
				m_Start.Clear();
				m_KeyNeedRelease = true;
				return;
			}
			m_KeyGesture = true;
			m_KeyMove = "0 0 0";
			m_KeyYaw = 0;
			m_KeyRoll = 0;
			m_KeyPitch = 0;
			m_KeyFactor = 1;
			m_Kernel.SetGesture("placement", true);
			DZS_Log.Info("placement: key gesture start");
		}
		if (!anyDown)
		{
			m_KeyGesture = false;
			if (m_KeyFactor != 1)
				m_GestureKind = "scale";
			else if (m_KeyYaw != 0 || m_KeyRoll != 0 || m_KeyPitch != 0)
				m_GestureKind = "rotate";
			else
				m_GestureKind = "move";
			DZS_Log.Info("placement: key gesture end, move " + m_KeyMove.ToString() + " yaw " + m_KeyYaw + " roll " + m_KeyRoll + " pitch " + m_KeyPitch + " factor " + m_KeyFactor);
			CommitWork(inst);
			return;
		}
		bool fast = ui.IsHeld("camera.faster");
		bool slow = ui.IsHeld("camera.slower");
		float mul = 1;
		if (fast && !slow)
			mul = 5;
		else if (slow && !fast)
			mul = 0.2;
		DZS_PlaceObj lead = LeadOfList(m_Start);
		if (!lead)
			return;
		float speed = m_Kernel.GetSettings().GetSettingNumber("placement.keySpeed");
		if (speed <= 0)
			speed = 2;
		m_KeyMove = m_KeyMove + mv * (speed * mul * dt);
		m_KeyYaw += yaw * 45 * mul * dt;
		m_KeyRoll += roll * 45 * mul * dt;
		m_KeyPitch += pitch * 45 * mul * dt;
		if (grow != 0)
		{
			float rate = 2;
			if (fast && !slow)
				rate = 5;
			else if (slow && !fast)
				rate = 1.15;
			float want = m_KeyFactor * Math.Pow(rate, grow * dt);
			m_KeyFactor = ClampFactor(want);
		}
		RestoreStart();
		vector pivot = WorkPivotStart();
		vector world = lead.m_R * m_KeyMove[0] + lead.m_U * m_KeyMove[1] + lead.m_F * m_KeyMove[2];
		foreach (DZS_PlaceObj o : m_Work)
		{
			if (m_KeyFactor != 1)
				o.ScaleAround(pivot, m_KeyFactor);
			if (m_KeyYaw != 0)
				o.RotateAround(pivot, "0 1 0", DZS_PlaceMath.YawRot(m_KeyYaw));
			if (m_KeyRoll != 0)
				o.RotateAround(pivot, lead.m_F, m_KeyRoll);
			if (m_KeyPitch != 0)
				o.RotateAround(pivot, lead.m_R, -m_KeyPitch);
			o.m_Pos = o.m_Pos + world;
		}
		ShowWork();
	}

	protected bool AnyKeyGestureKey()
	{
		array<string> keys = {"UP", "DOWN", "LEFT", "RIGHT", "PRIOR", "NEXT", "PERIOD", "COMMA", "ADD", "SUBTRACT"};
		foreach (string k : keys)
		{
			if (DZS_UiRegistry.KeyDown(k))
				return true;
		}
		return false;
	}

	// ---- Esc ----

	bool EscActive(string what)
	{
		if (what == "gesture")
			return m_Gesture == G_DRAG || m_Gesture == G_HANDLE || m_Gesture == G_LASSO || m_Gesture == G_CARRY || m_KeyGesture;
		if (what == "pick")
			return m_PickFor != "" || m_Swapping;
		return m_Sel.Count() > 0 && m_Hand == "";
	}

	void Esc(string what)
	{
		if (what == "gesture")
		{
			if (m_Gesture == G_LASSO)
				m_Kernel.SetGesture("placement", false);
			CancelGesture();
			return;
		}
		if (what == "pick")
		{
			EndPick();
			return;
		}
		ClearSelection();
	}

	protected void EndPick()
	{
		m_PickFor = "";
		if (m_Swapping)
		{
			m_Swapping = false;
			m_Kernel.GetScreen().HideBanner("placement.swap");
		}
	}

	// ---- actions ----

	bool ActionEnabled(string id, out string de, out string en)
	{
		if (!m_Ready)
			return false;
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return false;
		string blocked = m_Kernel.GetScreen().SceneBlockedReason();
		if (blocked != "" && id != "placement.copy")
		{
			de = blocked;
			en = blocked;
			return false;
		}
		array<string> selOnly = {"placement.cut", "placement.duplicate", "placement.delete", "placement.selectSame", "placement.deselect", "placement.lock", "placement.hide", "placement.unlock", "placement.unhide", "placement.straighten", "placement.surface", "placement.align", "placement.alignHeight", "placement.alignYaw", "placement.randomYaw", "placement.swap", "placement.drop", "placement.focus", "placement.position"};
		if (id == "placement.copyText" && m_Sel.Count() == 0)
		{
			de = DZS_Texts.Get("placement.copyText.none");
			en = "Nothing selected — without a selection Copy copies the camera position";
			return false;
		}
		if (id == "placement.carry")
		{
			if (m_Sel.Count() == 0)
			{
				de = DZS_Texts.Pick("Erst Objekte auswählen", "Select objects first");
				en = "Select objects first";
				return false;
			}
			if (m_Hand != "" || m_Gesture != G_NONE)
				return false;
			DZS_Instance cinst = m_Kernel.GetShown();
			if (cinst && CountLocked(cinst) >= m_Sel.Count())
			{
				de = DZS_Texts.Get("placement.carry.locked");
				en = "Locked";
				return false;
			}
		}
		if (id == "placement.pipetteObj" && m_MenuObjId != "" && inst.Has(m_MenuObjId))
		{
			string pn;
			inst.GetField(m_MenuObjId, DZS_Obj.NAME, pn);
			DZS_CatalogApi pcat = DZS_CatalogApi.Get();
			if (!pcat || !pcat.InCatalog(pn))
			{
				de = DZS_Texts.Fmt("placement.notInCatalog", Shown(pn));
				en = de;
				return false;
			}
		}
		bool needSel = selOnly.Find(id) >= 0;
		if (needSel && m_Sel.Count() == 0)
		{
			de = DZS_Texts.Pick("Erst Objekte auswählen", "Select objects first");
			en = "Select objects first";
			return false;
		}
		if (id == "placement.straighten")
		{
			DZS_Settings st = m_Kernel.GetSettings();
			if (!st.GetBool("placement.straightenPitch", true) && !st.GetBool("placement.straightenRoll", true) && !st.GetBool("placement.straightenYaw", false))
			{
				de = DZS_Texts.Get("placement.noAxis");
				en = "No axis is chosen in the settings";
				return false;
			}
		}
		if (id == "placement.paste" || id == "placement.pasteOrigin")
		{
			string clip;
			GetGame().CopyFromClipboard(clip);
			if (clip.IndexOf("{") < 0 && clip.IndexOf("[") < 0)
			{
				de = DZS_Texts.Get("placement.nothingInClip");
				en = "Nothing to paste";
				return false;
			}
		}
		return true;
	}

	bool ActionChecked(string id)
	{
		if (id == "placement.followSlope")
			return FollowSlope();
		if (id == "placement.pipette")
			return m_PickFor == "placement.pipette";
		return false;
	}

	string ActionLabel(string id)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (id == "placement.copy")
		{
			if (m_Sel.Count() > 0)
				return Txt("Kopieren", "Copy");
			return Txt("Position kopieren", "Copy position");
		}
		if (id == "placement.lock" && inst)
		{
			if (AllFlag(inst, true) && m_Sel.Count() > 0)
				return Txt("Entsperren", "Unlock");
			return Txt("Sperren", "Lock");
		}
		if (id == "placement.hide" && inst)
		{
			if (AllFlag(inst, false) && m_Sel.Count() > 0)
				return Txt("Einblenden", "Show");
			return Txt("Ausblenden", "Hide");
		}
		return "";
	}

	void AddForeignBarButton(string actionId, string icon, string labelDe, string labelEn, int order, bool singleOnly)
	{
		foreach (DZS_BarEntry known : m_ForeignBar)
		{
			if (known.m_Action == actionId)
				return;
		}
		DZS_BarEntry e = new DZS_BarEntry();
		e.m_Action = actionId;
		e.m_Icon = icon;
		e.m_De = labelDe;
		e.m_En = labelEn;
		e.m_Order = order;
		e.m_SingleOnly = singleOnly;
		m_ForeignBar.Insert(e);
		if (m_Bar)
			m_Bar.AddButton(actionId, icon, labelDe, order, singleOnly, labelEn);
	}

	int SelectionCount() { return m_Sel.Count(); }

	bool BarButtonVisible(string id)
	{
		if (id == "placement.position")
			return m_Sel.Count() > 1;
		return true;
	}

	string BarButtonLabel(string id)
	{
		if (m_Bar && m_Bar.ForeignLabel(id) != "")
			return m_Bar.ForeignLabel(id);
		if (id == "placement.lock" || id == "placement.hide")
			return ActionLabel(id);
		if (id == "placement.straighten")
			return Txt("Gerade stellen", "Straighten");
		if (id == "placement.surface")
			return Txt("An Oberfläche", "Settle on ground");
		if (id == "placement.swap")
			return Txt("Austauschen", "Replace");
		if (id == "placement.duplicate")
			return Txt("Duplizieren", "Duplicate");
		if (id == "placement.align")
			return Txt("Ausrichten", "Align");
		if (id == "catalog.saveTemplate")
			return Txt("Als Vorlage", "As template");
		if (id == "placement.position")
			return Txt("Position …", "Position …");
		return id;
	}

	void OnBarButton(string id)
	{
		if (id == "placement.align")
		{
			OpenAlignMenu();
			return;
		}
		m_Kernel.GetUi().Trigger(id);
	}

	protected bool AllFlag(DZS_Instance inst, bool locked)
	{
		foreach (string id : m_Sel)
		{
			if (!inst.Has(id))
				continue;
			if (locked && !inst.IsLocked(id))
				return false;
			if (!locked && !inst.IsHidden(id))
				return false;
		}
		return true;
	}

	void OnAction(string id)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return;
		if (id.IndexOf("placement.tool.") == 0)
		{
			m_Tool = id;
			m_Kernel.GetUi().SetActiveTool(id);
			DZS_Log.Info("placement: tool " + id);
			return;
		}
		if (id == "placement.boxKind")
		{
			if (m_Kernel.GetUi().m_FromArrow)
				OpenBoxKindMenu();
			else if (BoxKindRect())
				SetBoxKind("lasso");
			else
				SetBoxKind("rect");
			return;
		}
		if (id == "placement.toolCycle")
		{
			array<string> tools = {"placement.tool.move", "placement.tool.rotate", "placement.tool.scale"};
			int ti = tools.Find(m_Tool);
			string next = tools[(ti + 1) % 3];
			OnAction(next);
			return;
		}
		if (id == "placement.axis")
		{
			m_Axis = (m_Axis + 1) % 3;
			DZS_Log.Info("placement: axis " + m_Axis);
			return;
		}
		if (id == "placement.tab")
		{
			DZS_Log.Info("placement: tab hand '" + m_Hand + "' gesture " + m_Gesture + " surfaces " + m_SurfCount);
			if (m_Hand != "" || (m_Gesture == G_DRAG && m_DragMode == DM_GROUND))
			{
				if (m_SurfCount > 1)
				{
					int lvl = m_SurfLevel;
					if (lvl > m_SurfCount - 1)
						lvl = m_SurfCount - 1;
					m_SurfLevel = (lvl + 1) % m_SurfCount;
					DZS_Log.Info("placement: surface " + (m_SurfLevel + 1) + "/" + m_SurfCount);
				}
				return;
			}
			if (m_Gesture == G_DRAG || m_Kernel.GetSceneTool() != "")
				return;
			if (m_HoverList.Count() > 1)
			{
				m_HoverIdx = (m_HoverIdx + 1) % m_HoverList.Count();
				DZS_Log.Info("placement: step through " + (m_HoverIdx + 1) + "/" + m_HoverList.Count());
			}
			return;
		}
		if (id == "placement.followSlope")
		{
			bool slopeNext = !FollowSlope();
			DZS_Json slopeValue = DZS_Json.Boolean(slopeNext);
			m_Kernel.GetSettings().Set("placement.followSlope", slopeValue);
			DZS_Log.Info("placement: follow slope " + slopeNext + " -> " + FollowSlope());
			return;
		}
		if (id == "placement.copy")
		{
			if (m_Sel.Count() == 0)
				m_Kernel.GetUi().Trigger("camera.copyPosition");
			else
				CopySelection(inst, false);
			return;
		}
		if (id == "placement.copyText")
		{
			if (m_Sel.Count() > 0)
				CopyAsText(inst, SelectionInGroupOrder(inst));
			return;
		}
		if (id == "placement.cut")
		{
			CopySelection(inst, true);
			return;
		}
		if (id == "placement.paste")
		{
			Paste(inst, false);
			return;
		}
		if (id == "placement.pasteOrigin")
		{
			Paste(inst, true);
			return;
		}
		if (id == "placement.delete")
		{
			DeleteSelection(inst, "");
			return;
		}
		if (id == "placement.duplicate")
		{
			Duplicate(inst);
			return;
		}
		if (id == "placement.lock" || id == "placement.hide")
		{
			ToggleFlags(inst, id);
			return;
		}
		if (id == "placement.unlock" || id == "placement.unhide")
		{
			ToggleFlags(inst, id, 0);
			return;
		}
		if (id == "placement.carry")
		{
			StartCarry(inst);
			return;
		}
		if (id == "placement.selectAll")
		{
			array<string> all = AllSelectable(inst);
			string lead = m_Lead;
			if (all.Find(lead) < 0 && all.Count() > 0)
				lead = all[0];
			SetSelection(all, lead);
			return;
		}
		if (id == "placement.invert")
		{
			array<string> all2 = AllSelectable(inst);
			array<string> inv = new array<string>();
			foreach (string a : all2)
			{
				if (m_Sel.Find(a) < 0)
					inv.Insert(a);
			}
			string lead2 = m_Lead;
			if (inv.Find(lead2) < 0 && inv.Count() > 0)
				lead2 = inv[0];
			SetSelection(inv, lead2);
			return;
		}
		if (id == "placement.selectSame")
		{
			SelectSame(inst);
			return;
		}
		if (id == "placement.deselect")
		{
			ClearSelection();
			return;
		}
		if (id == "placement.drop")
		{
			DropToGround(inst);
			return;
		}
		if (id == "placement.straighten")
		{
			Straighten(inst);
			return;
		}
		if (id == "placement.surface")
		{
			ToSurface(inst);
			return;
		}
		if (id == "placement.randomYaw")
		{
			RandomYaw(inst);
			return;
		}
		if (id == "placement.alignHeight" || id == "placement.alignYaw")
		{
			m_PickFor = id;
			return;
		}
		if (id == "placement.pipetteObj")
		{
			if (m_MenuObjId != "" && inst.Has(m_MenuObjId))
				PipettePlaced(inst, m_MenuObjId, false);
			return;
		}
		if (id == "placement.pipette")
		{
			if (m_PickFor == id)
			{
				EndPick();
				return;
			}
			DZS_CatalogApi pc = DZS_CatalogApi.Get();
			if (pc && m_Hand != "")
				pc.DropHand();
			m_PickFor = id;
			DZS_Log.Info("placement: pipette tool on");
			return;
		}
		if (id == "placement.align")
		{
			OpenAlignMenu();
			return;
		}
		if (id == "placement.swap")
		{
			m_Swapping = true;
			m_Kernel.GetScreen().ShowBanner("placement.swap", 50, "info", DZS_Texts.Get("placement.swapBanner"));
			return;
		}
		if (id == "placement.focus")
		{
			FocusSelection(inst);
			return;
		}
		if (id == "placement.dupes")
		{
			FindDuplicates(inst);
			return;
		}
		if (id == "placement.position")
		{
			OpenPositionDialog(inst);
			return;
		}
	}

	protected ref array<ref DZS_PlaceObj> m_PosSeen;

	protected void OpenPositionDialog(DZS_Instance inst)
	{
		m_PosSeen = FreeSelection(inst);
		if (m_PosSeen.Count() == 0)
			return;
		float lo = 0;
		float hi = 1000000000.0;
		bool off = false;
		foreach (DZS_PlaceObj o : m_PosSeen)
		{
			if (o.m_Scale < DZS_Obj.SCALE_MIN || o.m_Scale > DZS_Obj.SCALE_MAX)
				off = true;
			else
			{
				lo = Math.Max(lo, DZS_Obj.SCALE_MIN / o.m_Scale);
				hi = Math.Min(hi, DZS_Obj.SCALE_MAX / o.m_Scale);
			}
		}
		DZS_PositionDialog d = new DZS_PositionDialog(this, PivotOf(m_PosSeen), lo, hi, off);
		d.Open();
	}

	// One step with the values seen when the dialog opened as precondition (they are the old values).
	void ApplyPosition(vector target, float turn, float factor)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !m_PosSeen)
			return;
		array<ref DZS_PlaceObj> list = m_PosSeen;
		vector pivot = PivotOf(list);
		vector shift = target - pivot;
		foreach (DZS_PlaceObj o : list)
		{
			if (factor != 1)
				o.ScaleAround(pivot, factor);
			if (turn != 0)
				o.RotateAround(pivot, "0 1 0", DZS_PlaceMath.YawRot(turn));
			o.m_Pos = o.m_Pos + shift;
		}
		if (m_PivotSet)
			m_Pivot = m_Pivot + shift;
		SubmitObjs(inst, list, Txt("Position der Auswahl", "Position of the selection"), "Position of the selection");
		m_PosSeen = null;
	}

	// ---- Clean up duplicates ----

	protected ref array<string> m_Surplus;
	protected ref array<ref array<string>> m_Twins;
	static const float DUPE_SHARE = 0.05;

	// Angle of the rotation that turns one orientation into the other (degrees).
	static float TurnBetween(DZS_PlaceObj a, DZS_PlaceObj b)
	{
		float tr = vector.Dot(a.m_R, b.m_R) + vector.Dot(a.m_U, b.m_U) + vector.Dot(a.m_F, b.m_F);
		float c = Math.Clamp((tr - 1) * 0.5, -1, 1);
		return Math.Acos(c) * Math.RAD2DEG;
	}

	protected void FindDuplicates(DZS_Instance inst)
	{
		array<string> pool = new array<string>();
		if (m_Sel.Count() > 0)
			pool = SelectionInGroupOrder(inst);
		else
		{
			array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
			foreach (string g : groups)
			{
				if (inst.GetKind(g) != DZS_KIND_GROUP)
					continue;
				array<string> ids = inst.GetChildren(g);
				foreach (string id : ids)
					pool.Insert(id);
			}
		}
		// tolerance: 5 % of the space diagonal of the smaller box; a grid per name with the largest tolerance of
		// that name as cell keeps the search linear
		DZS_SceneApi scene = DZS_SceneApi.Get();
		map<string, float> diagOf = new map<string, float>();
		map<string, float> cellOf = new map<string, float>();
		array<ref DZS_PlaceObj> objs = new array<ref DZS_PlaceObj>();
		array<float> tols = new array<float>();
		foreach (string pid : pool)
		{
			DZS_PlaceObj po = DZS_PlaceObj.FromState(inst, pid);
			objs.Insert(po);
			if (!diagOf.Contains(po.m_Name))
			{
				vector bmin = "-0.25 0 -0.25";
				vector bmax = "0.25 0.5 0.25";
				if (scene)
					scene.GetBox(po.m_Name, bmin, bmax);
				diagOf.Set(po.m_Name, vector.Distance(bmin, bmax));
			}
			float tol = DUPE_SHARE * diagOf.Get(po.m_Name) * Math.AbsFloat(po.m_Scale);
			tols.Insert(tol);
			if (!cellOf.Contains(po.m_Name) || cellOf.Get(po.m_Name) < tol)
				cellOf.Set(po.m_Name, tol);
		}
		map<string, ref array<int>> grid = new map<string, ref array<int>>();
		m_Surplus = new array<string>();
		m_Twins = new array<ref array<string>>();
		map<int, int> twinOf = new map<int, int>();
		array<string> labels = new array<string>();
		for (int i = 0; i < objs.Count(); i++)
		{
			DZS_PlaceObj o = objs[i];
			float cell = Math.Max(cellOf.Get(o.m_Name), 0.001);
			int cx = Math.Floor(o.m_Pos[0] / cell);
			int cy = Math.Floor(o.m_Pos[1] / cell);
			int cz = Math.Floor(o.m_Pos[2] / cell);
			bool dup = false;
			int keptIdx = -1;
			if (!o.m_Locked)
			{
				for (int dx = -1; dx <= 1 && !dup; dx++)
				{
					for (int dy = -1; dy <= 1 && !dup; dy++)
					{
						for (int dz = -1; dz <= 1 && !dup; dz++)
						{
							string nk = o.m_Name + "|" + (cx + dx) + "|" + (cy + dy) + "|" + (cz + dz);
							array<int> near = grid.Get(nk);
							if (!near)
								continue;
							foreach (int ki : near)
							{
								DZS_PlaceObj k = objs[ki];
								float lim = Math.Min(tols[ki], tols[i]);
								if (vector.Distance(k.m_Pos, o.m_Pos) <= lim && TurnBetween(k, o) <= 1)
								{
									dup = true;
									keptIdx = ki;
									break;
								}
							}
						}
					}
				}
			}
			if (dup)
			{
				m_Surplus.Insert(pool[i]);
				int tw;
				if (!twinOf.Find(keptIdx, tw))
				{
					tw = m_Twins.Count();
					twinOf.Set(keptIdx, tw);
					array<string> twins = new array<string>();
					twins.Insert(pool[keptIdx]);
					m_Twins.Insert(twins);
				}
				m_Twins[tw].Insert(pool[i]);
				continue;
			}
			string key = o.m_Name + "|" + cx + "|" + cy + "|" + cz;
			array<int> cellList = grid.Get(key);
			if (!cellList)
			{
				cellList = new array<int>();
				grid.Set(key, cellList);
			}
			cellList.Insert(i);
		}
		int n = m_Surplus.Count();
		DZS_Log.Info("placement: duplicates " + n + " of " + pool.Count());
		if (n == 0)
		{
			Msg("placement.dupes", "info", Txt("Keine doppelten Objekte", "No duplicate objects"));
			return;
		}
		string title;
		if (n == 1)
			title = Txt("1 doppeltes Objekt gefunden", "1 duplicate object found");
		else
			title = n.ToString() + Txt(" doppelte Objekte gefunden", " duplicate objects found");
		foreach (array<string> twinSet : m_Twins)
		{
			string cls;
			inst.GetField(twinSet[0], DZS_Obj.NAME, cls);
			int twinN = twinSet.Count();
			labels.Insert(cls + " · " + twinN.ToString() + "×");
		}
		DZS_Log.Info("placement: duplicate sets " + m_Twins.Count());
		DZS_DupesDialog c = new DZS_DupesDialog(Txt("Doppelte aufräumen", "Clean up duplicates"), title, labels);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("delete", Txt("Doppelte entfernen", "Remove duplicates"), "danger");
		c.m_OnResult.Insert(OnDupesResult);
		c.m_OnShow.Insert(OnDupesShow);
		c.Open();
	}

	void OnDupesShow(int row)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || !m_Twins || row < 0 || row >= m_Twins.Count())
			return;
		array<string> ids = new array<string>();
		foreach (string id : m_Twins[row])
		{
			if (inst.Has(id))
				ids.Insert(id);
		}
		if (ids.Count() == 0)
			return;
		SetSelection(ids, ids[0]);
		FocusSelection(inst);
		DZS_Log.Info("placement: duplicates shown row " + row + ", " + ids.Count() + " selected");
	}

	void OnDupesResult(DZS_Dialog d, string tag)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		if (tag != "delete" || !inst || !m_Surplus)
			return;
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		foreach (string id : m_Surplus)
		{
			if (inst.Has(id))
				b.Delete(id);
		}
		b.Commit(Txt("Doppelte aufräumen", "Clean up duplicates"), "Clean up duplicates");
		DZS_Log.Info("placement: surplus deleted " + m_Surplus.Count());
		m_Surplus = null;
	}

	override void OnPluginEvent(string topic, Managed payload)
	{
		if (topic == "camera.rightClick")
		{
			if (m_Gesture == G_CARRY)
			{
				CancelGesture();
				m_RPressScene = false;
				m_RPressMapObj = null;
				return;
			}
			DZS_Instance ri = m_Kernel.GetShown();
			if (ri && m_RPressScene && m_RPressHover != "" && m_Hand == "" && m_Gesture == G_NONE)
				OpenObjectMenu(ri, m_RPressHover);
			else if (ri && m_RPressScene && m_RPressMapObj && m_Hand == "" && m_Gesture == G_NONE)
				OpenMapObjectMenu(m_RPressMapObj);
			m_RPressScene = false;
			m_RPressMapObj = null;
			return;
		}
		if (topic == "selection.contextRequest")
		{
			DZS_Instance ci = m_Kernel.GetShown();
			if (ci && m_Lead != "" && m_Hand == "")
				OpenObjectMenu(ci, m_Lead);
			return;
		}
		if (topic == "core.stepObjects")
		{
			DZS_StepObjects so = DZS_StepObjects.Cast(payload);
			if (so && so.m_Inst == m_Kernel.GetShown())
				SelectAfterStep(so);
			return;
		}
		if (topic == "camera.focusRequest")
		{
			DZS_Instance inst = m_Kernel.GetShown();
			if (inst)
				FocusSelection(inst);
		}
	}

	protected void FocusSelection(DZS_Instance inst)
	{
		array<ref DZS_PlaceObj> list = new array<ref DZS_PlaceObj>();
		foreach (string id : m_Sel)
		{
			DZS_PlaceObj t2854 = DZS_PlaceObj.FromState(inst, id);
			list.Insert(t2854);
		}
		if (list.Count() == 0)
			return;
		vector mn, mx;
		BoundsOf(list, mn, mx);
		DZS_CameraPlugin cam = Cam();
		if (cam)
			cam.FocusOn(mn, mx, true);
	}

	protected void OpenAlignMenu()
	{
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		array<string> ids = {"placement.drop", "placement.alignHeight", "placement.alignYaw", "placement.randomYaw"};
		DZS_UiRegistry ui = m_Kernel.GetUi();
		foreach (string aid : ids)
		{
			DZS_MenuItem it = new DZS_MenuItem();
			it.m_ActionId = aid;
			it.m_Text = ui.GetActionLabel(aid);
			it.m_Binding = ui.FirstBinding(aid);
			items.Insert(it);
		}
		DZS_Popup pop = m_Kernel.GetScreen().OpenContextMenu(items);
		pop.m_OnChoose.Insert(OnMenuChoose);
	}

	void OnMenuChoose(DZS_Popup p, DZS_MenuItem it)
	{
		m_Kernel.GetScreen().CloseMenu();
		m_Kernel.GetUi().Trigger(it.m_ActionId);
	}

	// Context menu of a map object: "Take one into the hand" (eyedropper), then what other plugins add (loot).
	protected void OpenMapObjectMenu(Object o)
	{
		m_MenuMapObj = o;
		DZS_MenuItem it = new DZS_MenuItem();
		it.m_Tag = "placement.pipette";
		it.m_Text = DZS_Texts.Get("placement.pipette.menu");
		if (CatalogNameOf(o) == "")
		{
			it.m_Enabled = false;
			it.m_Reason = DZS_Texts.Fmt("placement.notSpawnable", MapObjectShown(o));
		}
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		items.Insert(it);
		DZS_MapObjectMenu.Fill(o, items);
		DZS_Popup pop = m_Kernel.GetScreen().OpenContextMenu(items);
		pop.m_OnChoose.Insert(OnMapObjectMenuChoose);
		DZS_Log.Info("placement: map object menu " + MapObjectShown(o) + ", " + items.Count() + " entries, pipette " + it.m_Enabled);
	}

	void OnMapObjectMenuChoose(DZS_Popup p, DZS_MenuItem it)
	{
		m_Kernel.GetScreen().CloseMenu();
		Object o = m_MenuMapObj;
		m_MenuMapObj = null;
		if (!o)
			return;
		if (it.m_Tag == "placement.pipette")
		{
			if (it.m_Enabled)
				Pipette(o, false);
			return;
		}
		DZS_MapObjectMenu.Choose(o, it);
	}

	protected string MapObjectShown(Object o)
	{
		string shown = o.GetType();
		if (shown == "")
			shown = o.GetShapeName();
		return shown;
	}

	// Context menu of an object.
	protected void OpenObjectMenu(DZS_Instance inst, string target)
	{
		if (target == "")
			return;
		if (m_Sel.Find(target) < 0)
		{
			array<string> one = {target};
			SetSelection(one, target);
		}
		DZS_UiRegistry ui = m_Kernel.GetUi();
		m_MenuObjId = target;
		array<string> ids = {"placement.cut", "placement.copy", "placement.copyText", "placement.paste", "placement.duplicate", "placement.delete", "|", "placement.pipetteObj", "placement.carry", "|", "placement.straighten", "placement.surface", "placement.drop", "placement.alignHeight", "placement.alignYaw", "placement.randomYaw", "|", "placement.swap", "|", "placement.lock", "placement.hide", "|", "placement.selectSame", "camera.focus", "|", "catalog.saveTemplate", "properties.details"};
		array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
		bool sep = false;
		foreach (string aid : ids)
		{
			if (aid == "|")
			{
				sep = true;
				continue;
			}
			if (!ui.GetAction(aid))
				continue;
			if (aid == "properties.details" && m_Sel.Count() != 1)
				continue;
			DZS_MenuItem it = new DZS_MenuItem();
			it.m_ActionId = aid;
			it.m_Text = ui.GetActionLabel(aid);
			it.m_Binding = ui.FirstBinding(aid);
			string rd, re;
			it.m_Enabled = ui.IsActionEnabled(aid, rd, re);
			it.m_Reason = DZS_Texts.Pick(rd, re);
			it.m_SepBefore = sep;
			sep = false;
			items.Insert(it);
		}
		DZS_Popup pop = m_Kernel.GetScreen().OpenContextMenu(items);
		pop.m_OnChoose.Insert(OnMenuChoose);
		DZS_Log.Info("placement: context menu, " + items.Count() + " entries");
	}

	// ---- operations on the selection ----

	protected int CountLocked(DZS_Instance inst)
	{
		int n = 0;
		foreach (string id : m_Sel)
		{
			if (inst.IsLocked(id) || inst.IsLocked(inst.GetParent(id)))
				n++;
		}
		return n;
	}

	protected void LockedMessage(int locked)
	{
		if (locked == 1)
			Msg("placement.locked", "info", DZS_Texts.Get("placement.lockedOne"));
		else if (locked > 1)
			Msg("placement.locked", "info", DZS_Texts.Fmt("placement.lockedMany", locked.ToString()));
	}

	protected void DeleteSelection(DZS_Instance inst, string nameDe)
	{
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		int n = 0;
		foreach (string id : m_Sel)
		{
			if (inst.IsLocked(id) || inst.IsLocked(inst.GetParent(id)))
				continue;
			b.Delete(id);
			n++;
		}
		int locked = CountLocked(inst);
		if (n > 0)
		{
			string de = nameDe;
			string en = "Cut";
			if (de == "")
			{
				de = Txt("Löschen", "Delete");
				en = "Delete";
			}
			b.Commit(de, en);
		}
		DZS_Log.Info("placement: deleted " + n + ", locked kept " + locked);
		if (nameDe == "")
			LockedMessage(locked);
	}

	protected void Duplicate(DZS_Instance inst)
	{
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		array<string> made = new array<string>();
		string newLead = "";
		array<string> order = SelectionInGroupOrder(inst);
		foreach (string id : order)
		{
			string g = inst.GetParent(id);
			if (inst.IsLocked(g) || inst.IsHidden(g))
			{
				Msg("placement.group", "warning", DZS_Texts.Fmt("placement.groupLocked", GroupName(inst, g)));
				return;
			}
			DZS_ElementData d = inst.GetElementCopy(id);
			array<ref DZS_Companion> comps = new array<ref DZS_Companion>();
			for (int i = 0; i < d.m_Companions.Count(); i++)
				comps.Insert(d.m_Companions.GetElement(i));
			string nid = b.Create(DZS_KIND_OBJECT, g, id, d.m_Fields, false, false, comps);
			made.Insert(nid);
			if (id == m_Lead)
				newLead = nid;
		}
		if (made.Count() == 0)
			return;
		DZS_SubmitResult dr = b.Commit(Txt("Duplizieren", "Duplicate"), "Duplicate");
		if (!dr || !dr.Ok())
			return;
		SetSelection(made, newLead);
		DZS_Log.Info("placement: duplicated " + made.Count());
	}

	protected array<string> SelectionInGroupOrder(DZS_Instance inst)
	{
		array<string> order = new array<string>();
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			array<string> ids = inst.GetChildren(g);
			foreach (string id : ids)
			{
				if (m_Sel.Find(id) >= 0)
					order.Insert(id);
			}
		}
		return order;
	}

	// force: -1 toggles, 0 clears the flag on all (Ctrl+L / Ctrl+H).
	protected void ToggleFlags(DZS_Instance inst, string id, int force = -1)
	{
		bool locking = id == "placement.lock" || id == "placement.unlock";
		bool target = !AllFlag(inst, locking);
		if (force == 0)
			target = false;
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		int n = 0;
		foreach (string sid : m_Sel)
		{
			if (locking && inst.IsLocked(sid) != target)
			{
				b.SetLocked(sid, target);
				n++;
			}
			else if (!locking && inst.IsHidden(sid) != target)
			{
				b.SetHidden(sid, target);
				n++;
			}
		}
		if (n == 0)
			return;
		string de;
		string en;
		if (locking && target)
		{
			de = Txt("Sperren", "Lock");
			en = "Lock";
		}
		else if (locking)
		{
			de = Txt("Entsperren", "Unlock");
			en = "Unlock";
		}
		else if (target)
		{
			de = Txt("Ausblenden", "Hide");
			en = "Hide";
		}
		else
		{
			de = Txt("Einblenden", "Show");
			en = "Show";
		}
		b.Commit(de, en);
		DZS_Log.Info("placement: " + en + " " + n + " object(s)");
	}

	protected void SelectSame(DZS_Instance inst)
	{
		map<string, bool> names = new map<string, bool>();
		foreach (string id : m_Sel)
		{
			string nm;
			inst.GetField(id, DZS_Obj.NAME, nm);
			names.Set(nm, true);
		}
		array<string> all = AllSelectable(inst);
		array<string> add = new array<string>();
		foreach (string a : all)
		{
			string an;
			inst.GetField(a, DZS_Obj.NAME, an);
			if (names.Contains(an))
				add.Insert(a);
		}
		AddToSelection(add, m_Lead);
		int total = m_Sel.Count();
		if (names.Count() == 1)
		{
			string kindName = Shown(names.GetKey(0));
			Msg("placement.same", "info", DZS_Texts.Fmt("placement.sameN", total.ToString(), kindName));
		}
		else
		{
			int kindCount = names.Count();
			string kinds = kindCount.ToString();
			Msg("placement.same", "info", DZS_Texts.Fmt("placement.sameKinds", total.ToString(), kinds));
		}
	}

	// Applies a working set as one operation (positions, angles, sizes).
	protected void SubmitObjs(DZS_Instance inst, array<ref DZS_PlaceObj> objs, string de, string en)
	{
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		foreach (DZS_PlaceObj o : objs)
		{
			b.SetField(o.m_Id, DZS_Obj.POS, DZS_Obj.Vec(o.m_Pos[0], o.m_Pos[1], o.m_Pos[2]));
			b.SetField(o.m_Id, DZS_Obj.YPR, DZS_Obj.Vec(o.m_Ypr[0], o.m_Ypr[1], o.m_Ypr[2]));
			b.SetField(o.m_Id, DZS_Obj.SCALE, DZS_Obj.Num(o.m_Scale));
		}
		DZS_SubmitResult r = b.Commit(de, en);
		DZS_Log.Info("placement: " + en + " " + objs.Count() + " object(s) ok " + r.Ok());
	}

	protected array<Object> GameObjectsOf(array<ref DZS_PlaceObj> objs)
	{
		array<Object> ex = new array<Object>();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		foreach (DZS_PlaceObj o : objs)
		{
			Object go = scene.GetObject(o.m_Id);
			if (go)
				ex.Insert(go);
		}
		if (m_Preview)
			ex.Insert(m_Preview);
		return ex;
	}

	// Drop: down without tilting until the first bottom touches the ground.
	protected void DropToGround(DZS_Instance inst)
	{
		array<ref DZS_PlaceObj> list = FreeSelection(inst);
		if (list.Count() == 0)
			return;
		array<Object> ex = GameObjectsOf(list);
		float best = 1000000000.0;
		array<vector> c = new array<vector>();
		foreach (DZS_PlaceObj o : list)
		{
			o.Corners(c);
			float lowY = 1000000000.0;
			foreach (vector p : c)
				lowY = Math.Min(lowY, p[1]);
			array<vector> probes = new array<vector>();
			foreach (vector q : c)
				probes.Insert(Vector(q[0], lowY, q[2]));
			probes.Insert(Vector(o.Center()[0], lowY, o.Center()[2]));
			foreach (vector pr : probes)
			{
				vector below, n;
				if (DZS_Rays.Below(pr, 0.01, ex, below, n))
					best = Math.Min(best, pr[1] - below[1]);
			}
		}
		if (best > 1e8)
		{
			Msg("placement.drop", "warning", DZS_Texts.Get("placement.noGround"));
			return;
		}
		if (best <= 0.001)
		{
			Msg("placement.drop", "info", DZS_Texts.Get("placement.alreadyDown"));
			return;
		}
		foreach (DZS_PlaceObj m : list)
			m.m_Pos = m.m_Pos - Vector(0, best, 0);
		SubmitObjs(inst, list, Txt("Fallen lassen", "Drop"), "Drop");
	}

	protected void Straighten(DZS_Instance inst)
	{
		array<ref DZS_PlaceObj> list = FreeSelection(inst);
		DZS_PlaceObj lead = LeadOf(list);
		if (!lead)
			return;
		DZS_Settings st = m_Kernel.GetSettings();
		bool doPitch = st.GetBool("placement.straightenPitch", true);
		bool doRoll = st.GetBool("placement.straightenRoll", true);
		bool doYaw = st.GetBool("placement.straightenYaw", false);
		vector target = lead.m_Ypr;
		if (doYaw)
			target[0] = 0;
		if (doPitch)
			target[1] = 0;
		if (doRoll)
			target[2] = 0;
		vector tr, tu, tf;
		DZS_PlaceMath.Axes(target, tr, tu, tf);
		// rotation that takes the lead's axes onto the target axes, applied to all around the middle
		vector centre;
		if (list.Count() == 1)
			centre = lead.Center();
		else
		{
			vector mn, mx;
			BoundsOf(list, mn, mx);
			centre = (mn + mx) * 0.5;
		}
		vector lr = lead.m_R;
		vector lu = lead.m_U;
		vector lf = lead.m_F;
		foreach (DZS_PlaceObj o : list)
		{
			vector rel = o.m_Pos - centre;
			o.m_Pos = centre + MapBasis(rel, lr, lu, lf, tr, tu, tf);
			o.m_R = MapBasis(o.m_R, lr, lu, lf, tr, tu, tf);
			o.m_U = MapBasis(o.m_U, lr, lu, lf, tr, tu, tf);
			o.m_F = MapBasis(o.m_F, lr, lu, lf, tr, tu, tf);
			o.m_Ypr = DZS_PlaceMath.Angles(o.m_R, o.m_U, o.m_F);
		}
		lead.m_Ypr = target;
		SubmitObjs(inst, list, Txt("Gerade stellen", "Straighten"), "Straighten");
	}

	// The rotation that maps basis (a1 a2 a3) onto (b1 b2 b3), applied to v.
	static vector MapBasis(vector v, vector a1, vector a2, vector a3, vector b1, vector b2, vector b3)
	{
		return b1 * vector.Dot(v, a1) + b2 * vector.Dot(v, a2) + b3 * vector.Dot(v, a3);
	}

	// Settle on ground: tilt as a whole so that the lead's up axis stands on the plane through the ground below
	// the four lower corners.
	protected void ToSurface(DZS_Instance inst)
	{
		array<ref DZS_PlaceObj> list = FreeSelection(inst);
		DZS_PlaceObj lead = LeadOf(list);
		if (!lead)
			return;
		array<Object> ex = GameObjectsOf(list);
		array<vector> pts = new array<vector>();
		vector centre;
		if (list.Count() == 1)
		{
			array<vector> c = new array<vector>();
			lead.Corners(c);
			pts.Insert(c[0]);
			pts.Insert(c[1]);
			pts.Insert(c[4]);
			pts.Insert(c[5]);
			centre = lead.Center();
		}
		else
		{
			vector mn, mx;
			BoundsOf(list, mn, mx);
			pts.Insert(Vector(mn[0], mn[1], mn[2]));
			pts.Insert(Vector(mx[0], mn[1], mn[2]));
			pts.Insert(Vector(mn[0], mn[1], mx[2]));
			pts.Insert(Vector(mx[0], mn[1], mx[2]));
			centre = (mn + mx) * 0.5;
		}
		array<vector> ground = new array<vector>();
		foreach (vector p : pts)
		{
			float off;
			if (!MeasureFoot(p, ex, off))
			{
				Msg("placement.surface", "warning", DZS_Texts.Get("placement.noGround"));
				return;
			}
			ground.Insert(Vector(p[0], p[1] - off, p[2]));
		}
		vector normal = (ground[1] - ground[0]) * (ground[2] - ground[0]) + (ground[2] - ground[3]) * (ground[1] - ground[3]);
		if (normal[1] < 0)
			normal = -normal;
		if (normal.Length() < 0.0001)
			return;
		normal = normal.Normalized();
		float yaw0 = lead.m_Ypr[0];
		vector axis = lead.m_U * normal;
		float s = axis.Length();
		float ang = Math.Atan2(s, vector.Dot(lead.m_U, normal)) * Math.RAD2DEG;
		if (s > 0.00001)
		{
			axis = axis * (1.0 / s);
			foreach (DZS_PlaceObj o : list)
				o.RotateAround(centre, axis, ang);
		}
		// the shortest tilt turns the heading a little; a turn about the new up axis brings the lead's yaw back
		float yr = yaw0 * Math.DEG2RAD;
		vector h = Vector(Math.Sin(yr), 0, Math.Cos(yr));
		vector q = Vector(Math.Cos(yr), 0, -Math.Sin(yr));
		vector f1 = lead.m_F;
		vector nf = normal * f1;
		float phi = Math.Atan2(-vector.Dot(f1, q), vector.Dot(nf, q));
		vector f2 = f1 * Math.Cos(phi) + nf * Math.Sin(phi);
		if (vector.Dot(f2, h) < 0)
			phi += Math.PI;
		float phiDeg = DZS_PlaceMath.Norm180(phi * Math.RAD2DEG);
		if (Math.AbsFloat(phiDeg) > 0.0001)
		{
			foreach (DZS_PlaceObj o2 : list)
				o2.RotateAround(centre, normal, phiDeg);
		}
		float footOff;
		if (!MeasureFoot(lead.Foot(), ex, footOff))
		{
			Msg("placement.surface", "warning", DZS_Texts.Get("placement.noGround"));
			return;
		}
		foreach (DZS_PlaceObj o3 : list)
			o3.m_Pos = o3.m_Pos - Vector(0, footOff, 0);
		DZS_Log.Info("placement: settle normal " + normal.ToString() + " tilt " + ang + " turn " + phiDeg + " lift " + (-footOff) + " lead ypr " + lead.m_Ypr.ToString() + " foot " + lead.Foot().ToString());
		SubmitObjs(inst, list, Txt("An Oberfläche", "Settle on ground"), "Settle on ground");
	}

	protected void RandomYaw(DZS_Instance inst)
	{
		array<ref DZS_PlaceObj> list = FreeSelection(inst);
		foreach (DZS_PlaceObj o : list)
		{
			vector c = o.Center();
			float d = Math.RandomFloatInclusive(0, 360) - o.m_Ypr[0];
			o.RotateAround(c, "0 1 0", DZS_PlaceMath.YawRot(d));
		}
		if (list.Count() > 0)
			SubmitObjs(inst, list, Txt("Zufällig drehen", "Random yaw"), "Random yaw");
	}

	protected void PickTarget(DZS_Instance inst)
	{
		if (m_PickFor == "placement.pipette")
		{
			if (TakeOverUnderPointer(inst, false))
				m_PickFor = "";
			return;
		}
		DZS_SceneApi scene = DZS_SceneApi.Get();
		array<ref DZS_RayHit> hits = PickRay(Excluded());
		if (hits.Count() == 0)
			return;
		Object o = hits[0].m_Obj;
		string tid = scene.IdOf(o);
		if (tid != "" && m_Sel.Find(tid) >= 0)
			return;
		DZS_PlaceObj target;
		if (tid != "")
			target = DZS_PlaceObj.FromState(inst, tid);
		else
		{
			string nm = o.GetType();
			target = NewObj(nm, o.GetPosition(), o.GetOrientation(), o.GetScale());
		}
		string what = m_PickFor;
		m_PickFor = "";
		array<ref DZS_PlaceObj> list = FreeSelection(inst);
		DZS_PlaceObj lead = LeadOf(list);
		if (!lead)
			return;
		if (what == "placement.alignHeight")
		{
			float dy = target.Foot()[1] - lead.Foot()[1];
			foreach (DZS_PlaceObj m : list)
				m.m_Pos = m.m_Pos + Vector(0, dy, 0);
			SubmitObjs(inst, list, Txt("Auf Höhe von", "Match height"), "Match height");
		}
		else
		{
			float d = DZS_PlaceMath.Norm180(target.m_Ypr[0] - lead.m_Ypr[0]);
			vector pivot = PivotOf(list);
			foreach (DZS_PlaceObj r : list)
				r.RotateAround(pivot, "0 1 0", DZS_PlaceMath.YawRot(d));
			SubmitObjs(inst, list, Txt("Gleich ausrichten", "Match direction"), "Match direction");
		}
	}

	void SwapTo(string name)
	{
		DZS_Instance inst = m_Kernel.GetShown();
		EndPick();
		if (!inst)
			return;
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		int n = 0;
		foreach (string id : m_Sel)
		{
			if (inst.IsLocked(id) || inst.IsLocked(inst.GetParent(id)))
				continue;
			b.SetField(id, DZS_Obj.NAME, name);
			n++;
		}
		if (n > 0)
			b.Commit(Txt("Austauschen", "Replace"), "Replace");
		DZS_Log.Info("placement: replaced " + n + " by " + name);
	}

	// ---- clipboard ----

	protected void CopySelection(DZS_Instance inst, bool cut)
	{
		array<string> order = SelectionInGroupOrder(inst);
		DZS_Format fmt = m_Kernel.GetFormat("spawner");
		if (!fmt || order.Count() == 0)
			return;
		DZS_Instance scratch = m_Kernel.NewLoadInstance();
		DZS_OpBuilder b = scratch.BeginOperation("core:load", 1);
		b.Create(DZS_KIND_PROJECT, "", "", null, false, false, null, DZS_PROJECT_ID);
		string g = b.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, null);
		foreach (string id : order)
		{
			DZS_ElementData d = inst.GetElementCopy(id);
			array<ref DZS_Companion> comps = new array<ref DZS_Companion>();
			for (int i = 0; i < d.m_Companions.Count(); i++)
				comps.Insert(d.m_Companions.GetElement(i));
			b.Create(DZS_KIND_OBJECT, g, DZS_END, d.m_Fields, false, false, comps);
		}
		string err;
		if (!scratch.LoadBuilt(b, err))
		{
			DZS_Log.Error("placement: copy failed: " + err);
			return;
		}
		DZS_WriteResult wr = fmt.Write(scratch, g);
		string text = DZS_SpawnerFormat.Join(wr.m_Chunks);
		GetGame().CopyToClipboard(text);
		int total = order.Count();
		DZS_Log.Info("placement: copied " + total + " object(s), " + text.Length() + " bytes");
		if (!cut)
		{
			if (total == 1)
				Msg("placement.copy", "success", DZS_Texts.Get("placement.copied1"));
			else
				Msg("placement.copy", "success", DZS_Texts.Fmt("placement.copied", total.ToString()));
			return;
		}
		int locked = CountLocked(inst);
		DeleteSelection(inst, Txt("Ausschneiden", "Cut"));
		int cutCount = total - locked;
		string m = DZS_Texts.Fmt("placement.cutN", cutCount.ToString());
		if (locked == 1)
			m += " · " + DZS_Texts.Get("placement.lockedOne");
		else if (locked > 1)
			m += " · " + DZS_Texts.Fmt("placement.lockedMany", locked.ToString());
		Msg("placement.copy", "success", m);
	}

	protected void Paste(DZS_Instance inst, bool atOrigin)
	{
		string clip;
		GetGame().CopyFromClipboard(clip);
		string t = clip.Trim();
		if (t.IndexOf("[") == 0)
			t = "{\"Objects\": " + t + "}";
		DZS_Format fmt = m_Kernel.GetFormat("spawner");
		DZS_ReadResult rr = null;
		if (fmt && t.IndexOf("{") == 0)
		{
			array<string> chunks = new array<string>();
			int at = 0;
			while (at < t.Length())
			{
				int len = Math.Min(900, t.Length() - at);
				chunks.Insert(t.Substring(at, len));
				at += len;
			}
			rr = fmt.Read(chunks);
		}
		if (!rr || !rr.m_Readable || rr.m_Objects.Count() == 0)
		{
			Msg("placement.paste", "warning", DZS_Texts.Get("placement.nothingToPaste"));
			DZS_Log.Info("placement: nothing to paste");
			return;
		}
		if (!ActiveGroupOpen(inst))
			return;
		array<ref DZS_PlaceObj> objs = new array<ref DZS_PlaceObj>();
		array<ref DZS_ElementData> datas = new array<ref DZS_ElementData>();
		foreach (DZS_ReadObject ro : rr.m_Objects)
		{
			DZS_ElementData d = ro.m_Data;
			string name, pos, ypr, scale;
			d.m_Fields.Find(DZS_Obj.NAME, name);
			d.m_Fields.Find(DZS_Obj.POS, pos);
			d.m_Fields.Find(DZS_Obj.YPR, ypr);
			d.m_Fields.Find(DZS_Obj.SCALE, scale);
			float s = 1;
			if (scale != "")
				DZS_F32.FromText(scale, s);
			DZS_PlaceObj o = NewObj(name, DZS_Obj.QuickVec(pos), DZS_Obj.QuickVec(ypr), s);
			objs.Insert(o);
			datas.Insert(d);
		}
		InsertSet(inst, objs, datas, null, !atOrigin, Txt("Einfügen", "Paste"));
	}

	// Creates a set of objects at the end of the active group; at the pointer: the middle of the common box
	// under the pointer, its bottom on the surface there (screen centre, else 10 m ahead).
	protected bool InsertSet(DZS_Instance inst, array<ref DZS_PlaceObj> objs, array<ref DZS_ElementData> datas, array<ref map<string, string>> extra, bool atPointer, string nameDe)
	{
		if (!ActiveGroupOpen(inst))
			return false;
		if (atPointer)
		{
			vector mn, mx;
			BoundsOf(objs, mn, mx);
			vector bottom = Vector((mn[0] + mx[0]) * 0.5, mn[1], (mn[2] + mx[2]) * 0.5);
			vector from = GetGame().GetCurrentCameraPosition();
			vector dir = PointerDir();
			if (!PointerInScene())
				dir = GetGame().GetCurrentCameraDirection();
			array<ref DZS_RayHit> hits = DZS_Rays.Ray(from, dir, ViewDist(), Excluded());
			if (hits.Count() == 0 && PointerInScene())
			{
				dir = GetGame().GetCurrentCameraDirection();
				hits = DZS_Rays.Ray(from, dir, ViewDist(), Excluded());
			}
			if (hits.Count() == 0)
			{
				Msg("placement.noGround", "warning", DZS_Texts.Get("placement.noGroundHere"));
				DZS_Log.Info("placement: no ground, nothing inserted");
				return false;
			}
			vector target = hits[0].m_Pos;
			vector shift = target - bottom;
			foreach (DZS_PlaceObj o : objs)
				o.m_Pos = o.m_Pos + shift;
		}
		DZS_OpBuilder b = inst.BeginOperation("placement:edit", 1);
		string group = DZS_ActiveGroup.ForNewObjects(inst, b);
		array<string> made = new array<string>();
		for (int i = 0; i < objs.Count(); i++)
		{
			DZS_PlaceObj p = objs[i];
			map<string, string> f = Fields(p.m_Name, p.m_Pos, p.m_Ypr, p.m_Scale);
			array<ref DZS_Companion> comps = null;
			if (datas)
			{
				DZS_ElementData d = datas[i];
				for (int k = 0; k < d.m_Fields.Count(); k++)
				{
					string key = d.m_Fields.GetKey(k);
					if (key != DZS_Obj.POS && key != DZS_Obj.YPR && key != DZS_Obj.SCALE && key != DZS_Obj.NAME)
						f.Set(key, d.m_Fields.GetElement(k));
				}
				if (!atPointer)
				{
					f.Set(DZS_Obj.POS, d.m_Fields.Get(DZS_Obj.POS));
					f.Set(DZS_Obj.YPR, d.m_Fields.Get(DZS_Obj.YPR));
				}
				comps = new array<ref DZS_Companion>();
				for (int c = 0; c < d.m_Companions.Count(); c++)
					comps.Insert(d.m_Companions.GetElement(c));
			}
			if (extra)
			{
				map<string, string> ef = extra[i];
				for (int e = 0; e < ef.Count(); e++)
					f.Set(ef.GetKey(e), ef.GetElement(e));
			}
			made.Insert(b.Create(DZS_KIND_OBJECT, group, DZS_END, f, false, false, comps));
		}
		DZS_SubmitResult r = b.Commit(nameDe, nameDe);
		if (!r.Ok())
			return false;
		string lead = "";
		if (made.Count() > 0)
			lead = made[0];
		SetSelection(made, lead);
		DZS_Log.Info("placement: inserted " + made.Count() + " object(s), at pointer " + atPointer);
		return true;
	}

	// One line per object; the values of the objects, not of an open dialog.
	void CopyAsText(DZS_Instance inst, array<string> order)
	{
		string text = "";
		foreach (int i, string id : order)
		{
			DZS_PlaceObj o = DZS_PlaceObj.FromState(inst, id);
			if (i > 0)
				text += "\n";
			string q0 = DZS_Num.Fixed(o.m_Pos[0], 3, true);
			string q1 = DZS_Num.Fixed(o.m_Pos[1], 3, true);
			string q2 = DZS_Num.Fixed(o.m_Pos[2], 3, true);
			text += o.m_Name + " " + q0 + " " + q1 + " " + q2;
			string a0 = DZS_Num.Fixed(DZS_PlaceMath.Norm180(o.m_Ypr[0]), 2, true);
			string a1 = DZS_Num.Fixed(DZS_PlaceMath.Norm180(o.m_Ypr[1]), 2, true);
			string a2 = DZS_Num.Fixed(DZS_PlaceMath.Norm180(o.m_Ypr[2]), 2, true);
			text += " " + a0 + " " + a1 + " " + a2;
			text += " " + DZS_Num.Fixed(o.m_Scale, 3, true);
		}
		GetGame().CopyToClipboard(text);
		int n = order.Count();
		if (n == 1)
			Msg("placement.copyText", "success", DZS_Texts.Get("placement.textCopied1"));
		else
			Msg("placement.copyText", "success", DZS_Texts.Fmt("placement.textCopied", n.ToString()));
		DZS_Log.Info("placement: copied as text '" + text + "'");
	}

	// ---- drawing ----

	// Objects others hold: at most 20 frames, the nearest to the camera; dots beyond;
	// lapsed ones only as hollow dots.
	protected ref array<string> m_FIds = new array<string>();
	protected ref array<int> m_FCols = new array<int>();
	protected ref array<bool> m_FLapsed = new array<bool>();
	protected ref array<vector> m_FDots = new array<vector>();
	protected ref array<int> m_FDotCols = new array<int>();
	protected ref array<bool> m_FDotRing = new array<bool>();
	protected ref array<float> m_FDotDist = new array<float>();
	static const int FOREIGN_DOTS_MAX = 2000;

	protected void ForeignFrames(DZS_Instance inst, array<vector> c, float w)
	{
		m_FDots.Clear();
		m_FDotCols.Clear();
		m_FDotRing.Clear();
		m_FDotDist.Clear();
		DZS_SelectGuard guard = DZS_SelectGuard.Get();
		if (!guard)
			return;
		m_FIds.Clear();
		m_FCols.Clear();
		m_FLapsed.Clear();
		guard.Held(m_FIds, m_FCols, m_FLapsed);
		if (m_FIds.Count() == 0)
			return;
		vector cam = GetGame().GetCurrentCameraPosition();
		array<int> near = new array<int>();
		array<float> nearD = new array<float>();
		for (int i = 0; i < m_FIds.Count(); i++)
		{
			string fid = m_FIds[i];
			if (!inst.Has(fid))
				continue;
			if (m_FLapsed[i])
			{
				AddForeignDot(inst, fid, m_FCols[i], true);
				continue;
			}
			float d = vector.Distance(cam, CenterOf(inst, fid));
			if (near.Count() == 20 && d >= nearD[19])
			{
				AddForeignDot(inst, fid, m_FCols[i], false);
				continue;
			}
			int at = 0;
			while (at < nearD.Count() && nearD[at] <= d)
				at++;
			near.InsertAt(i, at);
			nearD.InsertAt(d, at);
			if (near.Count() > 20)
			{
				int far = near[20];
				near.RemoveOrdered(20);
				nearD.RemoveOrdered(20);
				AddForeignDot(inst, m_FIds[far], m_FCols[far], false);
			}
		}
		foreach (int idx : near)
		{
			string oid = m_FIds[idx];
			CornersOf(inst, oid, c);
			DZS_WorldLines.Frame("o:" + oid, c, w, m_FCols[idx], 0, DZS_WorldLines.PRIO_NEW);
		}
		CapForeignDots();
	}

	protected void AddForeignDot(DZS_Instance inst, string oid, int col, bool ring)
	{
		vector at = CenterOf(inst, oid);
		m_FDots.Insert(at);
		m_FDotCols.Insert(col);
		m_FDotRing.Insert(ring);
		m_FDotDist.Insert(vector.Distance(GetGame().GetCurrentCameraPosition(), at));
	}

	// At most 2000 dots of foreign locks, the nearest to the camera: the limit
	// distance comes from 64 distance buckets, so no sorting of all dots per frame.
	protected int m_FDotsLogged;

	protected void CapForeignDots()
	{
		int n = m_FDots.Count();
		if (n <= FOREIGN_DOTS_MAX)
			return;
		float maxD = 0;
		foreach (float d0 : m_FDotDist)
			maxD = Math.Max(maxD, d0);
		float width = Math.Max(maxD / 64.0, 0.001);
		array<int> counts = new array<int>();
		for (int b = 0; b < 65; b++)
			counts.Insert(0);
		foreach (float d1 : m_FDotDist)
		{
			int bi = Math.Floor(d1 / width);
			bi = Math.Min(bi, 64);
			counts[bi] = counts[bi] + 1;
		}
		int sum = 0;
		int edge = 0;
		while (edge < 65 && sum + counts[edge] <= FOREIGN_DOTS_MAX)
		{
			sum += counts[edge];
			edge++;
		}
		int room = FOREIGN_DOTS_MAX - sum;
		// the edge bucket is split again into 64 finer steps while they stay above float precision, so the nearest fill the room
		float cutLo = edge * width;
		float bw = width;
		for (int lvl = 0; lvl < 4 && room > 0; lvl++)
		{
			float sub = bw / 64.0;
			if (sub < 0.0001 * Math.Max(1, cutLo))
				break;
			array<int> sc = new array<int>();
			for (int sb = 0; sb < 64; sb++)
				sc.Insert(0);
			foreach (float d2 : m_FDotDist)
			{
				if (d2 < cutLo || d2 >= cutLo + bw)
					continue;
				int k = Math.Floor((d2 - cutLo) / sub);
				k = Math.Min(Math.Max(k, 0), 63);
				sc[k] = sc[k] + 1;
			}
			int se = 0;
			int ss = 0;
			while (se < 64 && ss + sc[se] <= room)
			{
				ss += sc[se];
				se++;
			}
			room -= ss;
			cutLo += se * sub;
			bw = sub;
		}
		array<vector> dots = new array<vector>();
		array<int> cols = new array<int>();
		array<bool> rings = new array<bool>();
		array<float> dists = new array<float>();
		float keptMax = 0;
		float dropMin = 1e9;
		for (int i = 0; i < n; i++)
		{
			float d = m_FDotDist[i];
			bool keep = d < cutLo;
			if (!keep && d < cutLo + bw && room > 0)
			{
				keep = true;
				room--;
			}
			if (!keep)
			{
				dropMin = Math.Min(dropMin, d);
				continue;
			}
			keptMax = Math.Max(keptMax, d);
			dots.Insert(m_FDots[i]);
			cols.Insert(m_FDotCols[i]);
			rings.Insert(m_FDotRing[i]);
			dists.Insert(d);
		}
		m_FDots = dots;
		m_FDotCols = cols;
		m_FDotRing = rings;
		m_FDotDist = dists;
		if (n != m_FDotsLogged)
		{
			m_FDotsLogged = n;
			string fdl = "placement: foreign dots " + dots.Count() + " of " + n + ", farthest shown " + keptMax + " m, nearest left out " + dropMin + " m";
			fdl += ", cut " + cutLo + " m";
			DZS_Log.Info(fdl);
		}
	}

	protected void Draw(DZS_Instance inst)
	{
		m_DrawT0 = TickCount(0);
		DZS_Screen scr = m_Kernel.GetScreen();
		bool hidden = scr.IsUiHidden() || scr.HasKeyboardOwner("map");
		bool world = DepthFrames();
		string dlog;
		if (GetCLIParam("dzsDepthLog", dlog) && m_Time - m_DepthLogAt > 2)
		{
			m_DepthLogAt = m_Time;
			DZS_Log.Info("placement: depth rays " + DZS_WorldLines.RaysLastFrame() + " fps " + (1 / Math.Max(0.001, m_LastDt)));
		}
		CanvasWidget cv = m_Canvas.m_Canvas;
		m_Canvas.Begin();
		m_Canvas.Show(!hidden);
		if (hidden)
		{
			m_Bar.Hide();
			m_Kernel.GetScreen().SetTopReserve(0);
			return;
		}
		int accent = DZS_Color.Get("accent");
		int accentHalf = (accent & 0x00FFFFFF) | (128 << 24);
		int accentFaint = (accent & 0x00FFFFFF) | (110 << 24);
		float w1 = Math.Max(1, DZS_Scale.Px(1));
		float w2 = Math.Max(2, DZS_Scale.Px(2));
		float w4 = Math.Max(4, DZS_Scale.Px(4));
		array<vector> c = new array<vector>();
		int big = BIG_SELECTION;
		if (world)
		{
			// selection: own frames up to 20 objects, from 21 one frame around all and a dot per object
			if (m_Sel.Count() <= big)
			{
				foreach (string id : m_Sel)
				{
					if (m_Kernel.IsFrameTaken(id))
						continue;
					CornersOf(inst, id, c);
					float w = w2;
					if (id == m_Lead)
						w = w4;
					bool dashed = inst.IsHidden(id) || inst.IsHidden(inst.GetParent(id));
					DZS_WorldLines.Frame("s:" + id, c, w, accent, DashOf(dashed), DZS_WorldLines.PRIO_NEW);
				}
			}
			else
			{
				BigSelection(inst);
				DZS_WorldLines.Frame("s:big", m_BigBox, w4, accent, 0, DZS_WorldLines.PRIO_NOW);
			}
			string hov = m_Hover;
			if (hov != "" && m_Gesture == G_NONE && m_Hand == "")
			{
				CornersOf(inst, hov, c);
				int hovColor = accentHalf;
				if (DZS_SelectGuard.IsBlocked(hov))
					hovColor = DZS_SelectGuard.Get().HolderColor(hov);
				DZS_WorldLines.Frame("h:" + hov, c, w1, hovColor, 0, DZS_WorldLines.PRIO_NOW);
			}
			ForeignFrames(inst, c, w2);
			if (m_LassoHits.Count() <= big)
			{
				foreach (string lid : m_LassoHits)
				{
					CornersOf(inst, lid, c);
					DZS_WorldLines.Frame("l:" + lid, c, w1, accentHalf, 0, DZS_WorldLines.PRIO_NEW);
				}
			}
			DZS_WorldLines.Flush(cv);
			if (m_Sel.Count() > big)
			{
				float d6 = DZS_Scale.Px(6);
				float d10 = DZS_Scale.Px(10);
				for (int di = 0; di < m_BigCenters.Count() && di < m_Sel.Count(); di++)
				{
					float dd = d6;
					if (m_Sel[di] == m_Lead)
						dd = d10;
					DZS_WorldLines.Dot(cv, m_BigCenters[di], dd, accent, m_BigHidden[di]);
				}
			}
			float dfo = DZS_Scale.Px(6);
			for (int fdi = 0; fdi < m_FDots.Count(); fdi++)
				DZS_WorldLines.Dot(cv, m_FDots[fdi], dfo, m_FDotCols[fdi], m_FDotRing[fdi]);
			if (m_LassoHits.Count() > big)
			{
				float dl = DZS_Scale.Px(6);
				foreach (string ldid : m_LassoHits)
					DZS_WorldLines.Dot(cv, CenterOf(inst, ldid), dl, accentHalf, false);
			}
		}
		else
		{
			foreach (string oid : m_Sel)
			{
				if (m_Kernel.IsFrameTaken(oid))
					continue;
				CornersOf(inst, oid, c);
				float ow = w2;
				if (oid == m_Lead)
					ow = w4;
				bool odashed = inst.IsHidden(oid) || inst.IsHidden(inst.GetParent(oid));
				m_Canvas.Box(c, ow, accent, odashed);
			}
			string ohov = m_Hover;
			if (ohov != "" && m_Gesture == G_NONE && m_Hand == "")
			{
				CornersOf(inst, ohov, c);
				m_Canvas.Box(c, w1, accentHalf, false);
			}
			foreach (string olid : m_LassoHits)
			{
				CornersOf(inst, olid, c);
				m_Canvas.Box(c, w1, accentHalf, false);
			}
		}
		// drag-out area: tinted fill, outline in the accent colour
		if (m_Gesture == G_LASSO && m_LassoPoly.Count() >= 4)
		{
			FillPolygon(m_LassoPoly, DZS_Color.Get("accentTint"));
			int n = m_LassoPoly.Count() / 2;
			for (int i = 0; i < n; i++)
			{
				int j = (i + 1) % n;
				m_Canvas.Line(m_LassoPoly[2 * i], m_LassoPoly[2 * i + 1], m_LassoPoly[2 * j], m_LassoPoly[2 * j + 1], w1, accent);
			}
		}
		// ring at the preview
		if (m_HandObj && m_HandVisible && !m_HandNoGround)
		{
			int axisColor = DZS_Color.Get("axisY");
			vector ra = m_HandObj.m_R;
			vector rb = m_HandObj.m_F;
			if (m_Axis == 1)
			{
				axisColor = DZS_Color.Get("axisX");
				ra = m_HandObj.m_U;
				rb = m_HandObj.m_F;
			}
			else if (m_Axis == 2)
			{
				axisColor = DZS_Color.Get("axisZ");
				ra = m_HandObj.m_R;
				rb = m_HandObj.m_U;
			}
			float rad = Math.Max(0.3, vector.Distance(m_HandObj.m_BMin, m_HandObj.m_BMax) * 0.5);
			m_Canvas.Circle(m_HandObj.Center(), ra, rb, rad, w1, axisColor);
		}
		// pivot cross
		if (m_PivotSet && m_Sel.Count() > 0)
		{
			float px, py;
			if (DZS_PlaceMath.ToScreen(m_Pivot, px, py))
			{
				float cs = DZS_Scale.Px(8);
				m_Canvas.Line(px - cs, py, px + cs, py, w2, accent);
				m_Canvas.Line(px, py - cs, px, py + cs, w2, accent);
			}
		}
		int th0 = TickCount(0);
		DrawHandles(inst);
		int th1 = TickCount(0);
		UpdateBar(inst);
		int th2 = TickCount(0);
		if (DZS_Kernel.s_Prof)
		{
			ProfAdd("place.handles", th1 - th0);
			ProfAdd("place.bar", th2 - th1);
			ProfAdd("place.drawStart", th0 - m_DrawT0);
		}
	}

	protected int m_DrawT0;

	protected static void ProfAdd(string key, int ticks)
	{
		int had = 0;
		DZS_Kernel.s_ProfTicks.Find(key, had);
		DZS_Kernel.s_ProfTicks.Set(key, had + ticks);
	}

	// Box around a large selection, the centres and hidden flags for its dots and the middle; rebuilt only after a
	// change of the state or the selection, every frame during a gesture.
	protected void BigSelection(DZS_Instance inst)
	{
		bool live = m_Gesture != G_NONE || m_KeyGesture;
		int n = m_Sel.Count();
		string sg = m_GeomVer.ToString();
		string sn = n.ToString();
		string sig = sg + "|" + sn + "|" + m_Lead;
		if (n > 0)
			sig += "|" + m_Sel[0] + "|" + m_Sel[n - 1];
		if (!live && sig == m_BigSig)
			return;
		m_BigSig = sig;
		array<vector> all = new array<vector>();
		array<vector> c = new array<vector>();
		m_BigCenters.Clear();
		m_BigHidden.Clear();
		vector sum = "0 0 0";
		foreach (string id : m_Sel)
		{
			CornersOf(inst, id, c);
			foreach (vector bc : c)
				all.Insert(bc);
			vector centre = CenterOf(inst, id);
			m_BigCenters.Insert(centre);
			sum = sum + centre;
			bool hid = inst.IsHidden(id) || inst.IsHidden(inst.GetParent(id));
			m_BigHidden.Insert(hid);
		}
		if (n > 0)
			m_BigMid = sum * (1.0 / n);
		DZS_WorldLines.Around(all, m_BigBox);
	}

	string SelectionSignature()
	{
		int n = m_Sel.Count();
		string sig = n.ToString() + "|" + m_Lead;
		if (n > 0)
			sig += "|" + m_Sel[0] + "|" + m_Sel[n - 1];
		return sig;
	}

	// Lookup of the selection for other plugins (group list, properties) without searching the list each time;
	// rebuilt once per frame or when the count changes.
	bool IsSelectedFast(string id)
	{
		int frame = DZS_WorldLines.s_Frame;
		if (frame != m_SelSetFrame || m_Sel.Count() != m_SelSetCount)
		{
			m_SelSet.Clear();
			foreach (string sid : m_Sel)
				m_SelSet.Set(sid, true);
			m_SelSetFrame = frame;
			m_SelSetCount = m_Sel.Count();
		}
		return m_SelSet.Contains(id);
	}

	// Corners and centre of an object: the working copy during a gesture, else from the state (cached until the next
	// operation, so a large selection costs no field parsing per frame).
	protected void CornersOf(DZS_Instance inst, string id, array<vector> c)
	{
		DZS_PlaceObj o = WorkOf(id);
		if (o)
		{
			o.FrameCorners(c);
			return;
		}
		DZS_PlaceObj cached = CachedObj(inst, id);
		if (!cached)
		{
			c.Clear();
			for (int i = 0; i < 8; i++)
				c.Insert("0 0 0");
			return;
		}
		array<vector> k = m_CornerCache.Get(id);
		c.Copy(k);
	}

	protected vector CenterOf(DZS_Instance inst, string id)
	{
		DZS_PlaceObj o = WorkOf(id);
		if (o)
			return o.Center();
		DZS_PlaceObj cached = CachedObj(inst, id);
		if (!cached)
			return "0 0 0";
		return m_CenterCache.Get(id);
	}

	protected int m_GeomCacheView = -1;

	protected DZS_PlaceObj CachedObj(DZS_Instance inst, string id)
	{
		if (m_GeomCacheVer != m_GeomVer || m_GeomCacheInst != inst || m_GeomCacheView != DZS_ViewPose.Stamp())
		{
			m_GeomCacheView = DZS_ViewPose.Stamp();
			m_ObjCache.Clear();
			m_CornerCache.Clear();
			m_CenterCache.Clear();
			m_GeomCacheVer = m_GeomVer;
			m_GeomCacheInst = inst;
		}
		DZS_PlaceObj o = m_ObjCache.Get(id);
		if (o)
			return o;
		if (!inst.Has(id))
			return null;
		o = DZS_PlaceObj.FromStateQuick(inst, id);
		o.UseViewPose();
		m_ObjCache.Set(id, o);
		array<vector> k = new array<vector>();
		o.FrameCorners(k);
		m_CornerCache.Set(id, k);
		m_CenterCache.Set(id, o.Center());
		return o;
	}

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		m_GeomVer++;
	}

	// Frames with depth checked by rays (fallback C); -dzsDepthFrames=0 draws the earlier frame for comparison.
	protected bool DepthFrames()
	{
		string v;
		return !GetCLIParam("dzsDepthFrames", v) || v != "0";
	}

	protected float DashOf(bool dashed)
	{
		if (dashed)
			return DZS_Scale.Px(6);
		return 0;
	}

	protected void DrawHandles(DZS_Instance inst)
	{
		if (!HandlesShown())
			return;
		array<ref DZS_PlaceObj> list = m_Work;
		if (m_Gesture != G_HANDLE && m_Gesture != G_DRAG && !m_KeyGesture)
			list = FreeSelection(inst);
		DZS_PlaceObj lead = LeadOf(list);
		if (!lead)
			return;
		vector pivot = PivotOf(list);
		float px, py;
		if (!DZS_PlaceMath.ToScreen(pivot, px, py))
			return;
		array<vector> axes = new array<vector>();
		HandleAxes(lead, axes);
		array<string> colors = {"axisX", "axisY", "axisZ"};
		float len = WorldLen(pivot, DZS_Scale.Px(HANDLE_PX));
		float thin = Math.Max(3, DZS_Scale.Px(3));
		if (m_Tool == "placement.tool.scale")
		{
			float off = DZS_Scale.Px(72) * 0.70710678;
			float size = DZS_Scale.Px(18);
			int sc = DZS_Color.Get("text");
			if (m_HandleHover == 3 || m_HandleHeld == 3)
				size = DZS_Scale.Px(24);
			m_Canvas.Line(px, py, px + off, py - off, Math.Max(1, DZS_Scale.Px(1)), sc);
			m_Canvas.Square(px + off, py - off, size, sc);
			return;
		}
		int held = -1;
		if (m_Gesture == G_HANDLE)
			held = m_HandleHeld;
		for (int i = 0; i < 3; i++)
		{
			if (held >= 0 && held != i)
				continue;
			int col = DZS_Color.Get(colors[i]);
			float w = thin;
			if (m_HandleHover == i || held == i)
				w = thin * 1.7;
			if (m_Tool == "placement.tool.move")
			{
				float ex, ey;
				if (!ArrowTip(pivot, axes[i] * len, px, py, ex, ey))
					continue;
				m_Canvas.Line(px, py, ex, ey, w, col);
				float dx = ex - px;
				float dy = ey - py;
				float dl = Math.Sqrt(dx * dx + dy * dy);
				dx /= dl;
				dy /= dl;
				float hs = DZS_Scale.Px(14);
				m_Canvas.Line(ex, ey, ex - dx * hs - dy * hs * 0.5, ey - dy * hs + dx * hs * 0.5, w, col);
				m_Canvas.Line(ex, ey, ex - dx * hs + dy * hs * 0.5, ey - dy * hs - dx * hs * 0.5, w, col);
			}
			else
			{
				vector a, b;
				RingPlane(axes, i, a, b);
				float lx, ly;
				RingPoint(pivot, a, b, len * 0.75, 0, px, py, lx, ly);
				for (int k = 1; k <= 48; k++)
				{
					float qx, qy;
					RingPoint(pivot, a, b, len * 0.75, k, px, py, qx, qy);
					m_Canvas.Line(lx, ly, qx, qy, w, col);
					lx = qx;
					ly = qy;
				}
			}
		}
	}

	protected void UpdateBar(DZS_Instance inst)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		bool blocked = scr.HasDialog() || scr.HasKeyboardOwner("map") || scr.HasKeyboardOwner("fullsearch") || GetGame().GetUIManager().GetMenu() != null;
		if (m_Sel.Count() == 0 || m_Hand != "" || blocked || scr.IsUiHidden() || m_Kernel.GetSceneTool() != "")
		{
			m_Bar.Hide();
			scr.SetTopReserve(0);
			return;
		}
		string label;
		if (m_Sel.Count() == 1)
		{
			string nm;
			inst.GetField(m_Sel[0], DZS_Obj.NAME, nm);
			label = Shown(nm);
		}
		else
		{
			int selCount = m_Sel.Count();
			label = DZS_Texts.Fmt("placement.objects", selCount.ToString());
		}
		int selCountKey = m_Sel.Count();
		string kg = m_GeomVer.ToString();
		string kn = selCountKey.ToString();
		string key = kg + "|" + kn + "|" + m_Lead + "|" + m_Tool;
		if (selCountKey > 0)
			key += "|" + m_Sel[0] + "|" + m_Sel[selCountKey - 1];
		string gateDe, gateEn;
		if (DZS_EditGate.ActionOff("placement.straighten", gateDe, gateEn))
			key += "|gate:" + gateDe;
		key += "|" + DZS_ChangeWait.Reason();
		m_Bar.Show(label, key);
		scr.SetTopReserve(m_Bar.ShownHeight());
	}

	// Pointer hint and context hint.
	protected void UpdateHints(DZS_Instance inst)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		DZS_UiRegistry ui = m_Kernel.GetUi();
		int mx, my;
		PointerPos(mx, my);
		array<string> lines = new array<string>();
		string title = "";
		if (m_Hand != "" && m_HandVisible && m_HandObj)
		{
			string gname = DZS_ActiveGroup.Name(inst);
			title = DZS_Texts.Fmt("placement.hand.title", Shown(m_HandObj.m_Name), gname) + SurfSuffix();
			string axisName = DZS_Texts.Get("placement.axis." + m_Axis);
			string rKey = ui.FirstBinding("placement.axis");
			if (m_HandNoGround)
				lines.Insert(DZS_Texts.Get("placement.noGroundHere"));
			else
				lines.Insert(DZS_Texts.Get("placement.hand.click"));
			lines.Insert(DZS_Texts.Fmt("placement.hand.axis", rKey, axisName));
			string angle = DZS_Num.Fixed(DZS_PlaceMath.Norm180(m_HandYpr[m_Axis]), 1) + "°";
			lines.Insert(DZS_Texts.Fmt("placement.hand.wheel", angle));
			string off = DZS_Num.Fixed(m_HandOffset, 2) + " m";
			if (m_HandOffset >= 0)
				off = "+" + off;
			lines.Insert(DZS_Texts.Fmt("placement.hand.height", off));
			scr.SetHint("placement", 20, DZS_Texts.Fmt("placement.hint.hand", rKey));
			HandWheel();
		}
		else if (m_Hand != "")
			scr.SetHint("placement", 20, "");
		else if (m_Gesture == G_CARRY)
		{
			CarryLines(inst, title, lines);
			scr.SetHint("placement", 20, DZS_Texts.Fmt("placement.hint.carry", m_Kernel.GetUi().FirstBinding("placement.axis")));
		}
		else
		{
			if (m_Sel.Count() > 0)
			{
				string l = ui.FirstBinding("placement.lock");
				string h = ui.FirstBinding("placement.hide");
				string d = ui.FirstBinding("placement.duplicate");
				scr.SetHint("placement", 5, DZS_Texts.Fmt("placement.hint.sel", l, h, d));
			}
			else
				scr.SetHint("placement", 5, "");
			TakeOverHint(inst);
			GestureLines(inst, title, lines);
		}
		if (m_PickFor != "")
		{
			title = DZS_Texts.Get("placement.pick");
			if (m_PickFor == "placement.alignYaw")
				title = DZS_Texts.Get("placement.pickYaw");
			else if (m_PickFor == "placement.pipette")
				title = DZS_Texts.Get("placement.pickPipette");
			lines.Clear();
		}
		if (title == "" && m_Gesture == G_NONE && m_Hand == "" && m_Hover != "" && DZS_SelectGuard.IsBlocked(m_Hover))
		{
			string holder = DZS_SelectGuard.Get().HolderName(m_Hover);
			title = DZS_Texts.Pick("In Bearbeitung bei " + holder, "Being edited by " + holder);
			lines.Clear();
		}
		if (title == "" && m_Gesture == G_NONE && m_Hand == "" && m_HoverList.Count() > 1 && m_Hover != "")
		{
			string hn;
			inst.GetField(m_Hover, DZS_Obj.NAME, hn);
			int hpos = m_HoverIdx + 1;
			int hcount = m_HoverList.Count();
			string th = hpos.ToString();
			string tc = hcount.ToString();
			string ts = Shown(hn);
			title = th + "/" + tc + " · " + ts;
		}
		bool clickHint = m_Kernel.GetSettings().GetBool("core.pointerHints", true);
		if (clickHint && m_Gesture == G_NONE && m_Hand == "" && m_PickFor == "" && m_Hover != "" && m_HandleHover < 0 && !IsSelectedFast(m_Hover) && !DZS_SelectGuard.IsBlocked(m_Hover))
		{
			if (title == "")
			{
				string cn;
				inst.GetField(m_Hover, DZS_Obj.NAME, cn);
				title = Shown(cn);
			}
			lines.InsertAt(DZS_Texts.Get("placement.clickToSelect"), 0);
		}
		if (title == "" || scr.IsUiHidden() || !PointerInScene())
		{
			m_PHint.Hide();
			return;
		}
		m_PHint.Show(title, lines, mx, my, true);
	}

	// With Alt over a map object: whether one can be taken into the hand.
	protected void TakeOverHint(DZS_Instance inst)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		string text = "";
		bool alt = DZS_UiRegistry.ModDown("Alt");
		bool ctrl = DZS_UiRegistry.ModDown("Ctrl");
		bool tool = m_PickFor == "placement.pipette";
		if ((alt || ctrl || tool) && m_Hand == "" && m_Kernel.GetSceneTool() == "" && m_Gesture == G_NONE && PointerInScene() && !SceneMouseBlocked())
		{
			array<ref DZS_RayHit> hits = PickRay(Excluded());
			DZS_SceneApi scene = DZS_SceneApi.Get();
			string placedId = "";
			if (hits.Count() > 0)
				placedId = scene.IdOf(hits[0].m_Obj);
			if (placedId != "" && (alt || tool) && !ctrl)
			{
				string pn;
				inst.GetField(placedId, DZS_Obj.NAME, pn);
				DZS_CatalogApi pcat = DZS_CatalogApi.Get();
				if (pcat && pcat.InCatalog(pn) && tool)
					text = DZS_Texts.Fmt("placement.takeOverTool", Shown(pn));
				else if (pcat && pcat.InCatalog(pn))
					text = DZS_Texts.Fmt("placement.takeOver", Shown(pn), "Alt");
				else
					text = DZS_Texts.Fmt("placement.notInCatalog", Shown(pn));
			}
			else if (hits.Count() > 0 && placedId == "" && m_Hover == "")
			{
				Object o = hits[0].m_Obj;
				string name = CatalogNameOf(o);
				if (name != "")
				{
					string held = "Alt";
					if (ctrl && !alt)
						held = "Ctrl";
					if (tool && !alt && !ctrl)
						text = DZS_Texts.Fmt("placement.takeOverTool", Shown(name));
					else
						text = DZS_Texts.Fmt("placement.takeOver", Shown(name), held);
				}
				else
					text = DZS_Texts.Fmt("placement.notSpawnable", MapObjectShown(o));
			}
		}
		scr.SetHint("placement.ctrl", 15, text);
	}

	// Even-odd scanline fill in rows of 2 px.
	protected void FillPolygon(array<float> poly, int color)
	{
		int n = poly.Count() / 2;
		if (n < 3)
			return;
		float y0 = poly[1];
		float y1 = poly[1];
		for (int i = 1; i < n; i++)
		{
			y0 = Math.Min(y0, poly[2 * i + 1]);
			y1 = Math.Max(y1, poly[2 * i + 1]);
		}
		float step = 2;
		array<float> xs = new array<float>();
		for (float y = Math.Floor(y0) + step / 2; y < y1; y += step)
		{
			xs.Clear();
			for (int e = 0; e < n; e++)
			{
				int f = (e + 1) % n;
				float ay = poly[2 * e + 1];
				float by = poly[2 * f + 1];
				if ((ay <= y && by > y) || (by <= y && ay > y))
					xs.Insert(poly[2 * e] + (y - ay) * (poly[2 * f] - poly[2 * e]) / (by - ay));
			}
			xs.Sort();
			for (int k = 0; k + 1 < xs.Count(); k += 2)
				m_Canvas.Line(xs[k], y, xs[k + 1], y, step, color);
		}
	}

	protected void GestureLines(DZS_Instance inst, out string title, array<string> lines)
	{
		if (m_Gesture != G_DRAG && m_Gesture != G_HANDLE && m_Gesture != G_LASSO && !m_KeyGesture)
			return;
		if (m_Gesture == G_LASSO)
		{
			title = DZS_Texts.Get("placement.selectionN");
			int lassoCount = m_LassoHits.Count();
			lines.Insert(DZS_Texts.Fmt("placement.objects", lassoCount.ToString()));
			return;
		}
		string who;
		if (m_Work.Count() == 1)
			who = Shown(m_Work[0].m_Name);
		else
		{
			int workCount = m_Work.Count();
			who = DZS_Texts.Fmt("placement.objects", workCount.ToString());
		}
		DZS_PlaceObj lead = WorkLead();
		DZS_PlaceObj start = LeadOfList(m_Start);
		if (!lead || !start)
			return;
		string kind = m_GestureKind;
		if (m_KeyGesture)
		{
			if (m_KeyFactor != 1)
				kind = "scale";
			else if (m_KeyYaw != 0 || m_KeyRoll != 0 || m_KeyPitch != 0)
				kind = "rotate";
			else
				kind = "move";
		}
		if (kind == "rotate")
		{
			// ring 0 = X (pitch), 1 = Y (yaw), 2 = Z (roll); everything else turns around the vertical
			int axisIdx = 0;
			int valueIdx = 0;
			float ang = DZS_PlaceMath.Norm180(lead.m_Ypr[0] - start.m_Ypr[0]);
			if (m_Gesture == G_HANDLE && m_HandleHeld == 0)
			{
				axisIdx = 1;
				valueIdx = 1;
				ang = m_RotAngle;
			}
			else if (m_Gesture == G_HANDLE && m_HandleHeld == 2)
			{
				axisIdx = 2;
				valueIdx = 2;
				ang = m_RotAngle;
			}
			else if (m_Gesture == G_HANDLE)
				ang = m_RotAngle;
			title = DZS_Texts.Fmt("placement.rotateN", who, DZS_Texts.Get("placement.axis." + axisIdx));
			string a = DZS_Num.Fixed(ang, 1) + "°";
			if (ang >= 0)
				a = "+" + a;
			lines.Insert(a);
			array<string> valueNames = {"Gier ", "Nick ", "Roll "};
			lines.Insert(valueNames[valueIdx] + DZS_Num.Fixed(DZS_PlaceMath.Norm180(lead.m_Ypr[valueIdx]), 1) + "°");
		}
		else if (kind == "scale")
		{
			title = DZS_Texts.Fmt("placement.scaleN", who);
			lines.Insert("×" + DZS_Num.Fixed(lead.m_Scale / start.m_Scale, 2));
			lines.Insert(Txt("Größe ", "Size ") + DZS_Num.Fixed(lead.m_Scale, 2));
		}
		else
		{
			title = DZS_Texts.Fmt("placement.moveN", who);
			if (m_Gesture == G_DRAG)
				title = title + SurfSuffix();
			vector d = lead.m_Pos - start.m_Pos;
			float horiz = Math.Sqrt(d[0] * d[0] + d[2] * d[2]);
			lines.Insert(Txt("waagerecht ", "horizontal ") + DZS_Num.Fixed(horiz, 2) + " m");
			string up = DZS_Num.Fixed(d[1], 2) + " m";
			if (d[1] >= 0)
				up = "+" + up;
			lines.Insert(Txt("Höhe ", "height ") + up);
			if (m_Gesture == G_DRAG && m_Lift != 0)
			{
				string lt = DZS_Num.Fixed(m_Lift, 2) + " m";
				if (m_Lift > 0)
					lt = "+" + lt;
				lines.Insert(Txt("Mausrad ", "wheel ") + lt);
			}
			float leadOff;
			if (MeasureFoot(lead.Foot(), Excluded(), leadOff))
				lines.Insert(Txt("über Grund ", "above ground ") + DZS_Num.Fixed(leadOff, 2) + " m");
		}
		if (m_KeyGesture)
		{
			vector pivot = WorkPivot();
			float sx, sy;
			if (DZS_PlaceMath.ToScreen(pivot, sx, sy))
			{
				m_PHint.Show(title, lines, sx, sy, true);
				title = "";
			}
		}
	}

	// Wheel in the hand: turn around the active axis (Shift fine); with Ctrl the height offset.
	// ---- Offset ----

	protected vector m_CarryRef;
	protected float m_CarryTurn;
	protected float m_CarryOffset;
	protected bool m_CarryShown;

	protected void StartCarry(DZS_Instance inst)
	{
		if (!inst || m_Hand != "" || m_Gesture != G_NONE)
			return;
		Snapshot(inst);
		if (m_Work.Count() == 0)
		{
			Msg("placement.carry", "warning", DZS_Texts.Get("placement.allLocked"));
			return;
		}
		vector mn, mx;
		BoundsOf(m_Start, mn, mx);
		m_CarryRef = Vector((mn[0] + mx[0]) * 0.5, mn[1], (mn[2] + mx[2]) * 0.5);
		m_CarryTurn = 0;
		m_CarryOffset = 0;
		float off;
		if (MeasureFoot(m_CarryRef, m_WorkObjs, off))
			m_CarryOffset = Math.Round(off * 1000) / 1000;
		m_CarryShown = false;
		ResetSurfLevel();
		m_Gesture = G_CARRY;
		m_GestureKind = "move";
		m_GestureLabel = m_Work[0].m_Id;
		m_Kernel.SetGesture("placement", true);
		m_Kernel.ClaimWheel("placement", true);
		DZS_Log.Info("placement: carry start " + m_Work.Count() + " object(s), ref " + m_CarryRef.ToString() + ", offset " + m_CarryOffset);
	}

	protected void CarryWheel()
	{
		int wheel = GetMouseState(MouseState.WHEEL);
		if (wheel == 0 || !PointerInScene() || SceneMouseBlocked())
			return;
		bool fine = DZS_UiRegistry.ModDown("Shift");
		if (DZS_UiRegistry.ModDown("Ctrl"))
		{
			float step = 0.05;
			if (fine)
				step = 0.01;
			m_CarryOffset = Math.Clamp(Math.Round((m_CarryOffset + wheel * step) * 1000) / 1000, -50, 50);
			DZS_Log.Info("placement: carry height offset " + m_CarryOffset);
			return;
		}
		float deg = 15;
		if (fine)
			deg = 1;
		m_CarryTurn = DZS_PlaceMath.Norm180(m_CarryTurn + wheel * deg);
		DZS_Log.Info("placement: carry turn " + m_CarryTurn + " axis " + m_Axis);
	}

	// The objects themselves are the preview: start poses turned about the reference point (one object: about its
	// own axis of R, more: about the vertical), the reference point on the surface under the pointer plus the offset.
	protected void UpdateCarry(DZS_Instance inst, bool inScene)
	{
		CarryWheel();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		DZS_RayHit hit = null;
		if (inScene)
			hit = PickSurface(GetGame().GetCurrentCameraPosition(), PointerDir(), Excluded());
		m_HandNoGround = inScene && !hit;
		if (!hit)
		{
			if (m_CarryShown)
			{
				foreach (DZS_PlaceObj h : m_Work)
					scene.ClearView(h.m_Id);
				m_CarryShown = false;
			}
			return;
		}
		vector target = hit.m_Pos + Vector(0, m_CarryOffset, 0);
		for (int i = 0; i < m_Work.Count(); i++)
		{
			DZS_PlaceObj o = m_Work[i];
			DZS_PlaceObj st = m_Start[i];
			o.m_Pos = st.m_Pos;
			o.m_Ypr = st.m_Ypr;
			o.m_Scale = st.m_Scale;
			o.UpdateAxes();
			if (m_CarryTurn != 0)
			{
				if (m_Work.Count() == 1 && m_Axis != 0)
				{
					vector keep = o.Foot();
					vector ypr = o.m_Ypr;
					ypr[m_Axis] = DZS_PlaceMath.Norm180(ypr[m_Axis] + m_CarryTurn);
					o.m_Ypr = ypr;
					o.UpdateAxes();
					o.m_Pos = o.OriginForFoot(keep);
				}
				else
					o.RotateAround(m_CarryRef, "0 1 0", DZS_PlaceMath.YawRot(m_CarryTurn));
			}
			o.m_Pos = o.m_Pos + (target - m_CarryRef);
		}
		ShowWork();
		m_CarryShown = true;
	}

	protected void CommitCarry(DZS_Instance inst)
	{
		if (!m_CarryShown)
			return;
		m_Kernel.ClaimWheel("placement", false);
		m_Gesture = G_NONE;
		ResetSurfLevel();
		m_GestureKind = "move";
		CommitWork(inst);
	}

	protected void CarryLines(DZS_Instance inst, out string title, array<string> lines)
	{
		if (m_Work.Count() == 0)
			return;
		string who;
		if (m_Work.Count() == 1)
		{
			string nm;
			inst.GetField(m_Work[0].m_Id, DZS_Obj.NAME, nm);
			who = Shown(nm);
		}
		else
			who = DZS_Texts.Fmt("placement.objects", m_Work.Count().ToString());
		title = DZS_Texts.Fmt("placement.moveN", who) + SurfSuffix();
		if (m_HandNoGround)
			lines.Insert(DZS_Texts.Get("placement.noGroundHere"));
		else
			lines.Insert(DZS_Texts.Get("placement.hand.click"));
		if (m_Work.Count() == 1)
			lines.Insert(DZS_Texts.Fmt("placement.hand.axis", m_Kernel.GetUi().FirstBinding("placement.axis"), DZS_Texts.Get("placement.axis." + m_Axis)));
		lines.Insert(DZS_Texts.Fmt("placement.hand.wheel", DZS_Num.Fixed(m_CarryTurn, 1) + "°"));
		string off = DZS_Num.Fixed(m_CarryOffset, 2) + " m";
		if (m_CarryOffset >= 0)
			off = "+" + off;
		lines.Insert(DZS_Texts.Fmt("placement.hand.height", off));
	}

	protected void HandWheel()
	{
		int wheel = GetMouseState(MouseState.WHEEL);
		if (wheel == 0 || !PointerInScene() || SceneMouseBlocked())
			return;
		bool fine = DZS_UiRegistry.ModDown("Shift");
		if (DZS_UiRegistry.ModDown("Ctrl"))
		{
			float step = 0.05;
			if (fine)
				step = 0.01;
			m_HandOffset = Math.Clamp(Math.Round((m_HandOffset + wheel * step) * 1000) / 1000, -50, 50);
			DZS_Log.Info("placement: height offset " + m_HandOffset);
			return;
		}
		float deg = 15;
		if (fine)
			deg = 1;
		float v = DZS_PlaceMath.Norm180(m_HandYpr[m_Axis] + wheel * deg);
		m_HandYpr[m_Axis] = Math.Round(v * 1000) / 1000;
		DZS_Log.Info("placement: hand ypr " + m_HandYpr.ToString());
	}
}

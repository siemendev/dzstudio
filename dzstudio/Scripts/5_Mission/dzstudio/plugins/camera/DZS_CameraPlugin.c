// The free camera of the editor as a bundled plugin. Other plugins use its public methods
// (GetPosition, SetPose, FlyTo, FocusOn …) through kernel.GetPlugin("camera") and listen to the plugin
// event "camera.changed" (payload DZS_CameraState).
class DZS_CameraState : Managed
{
	vector m_Pos;
	float m_Yaw;
	float m_Pitch;
}

class DZS_CameraActions : DZS_ActionHandler
{
	protected DZS_CameraPlugin m_Cam;

	void DZS_CameraActions(DZS_CameraPlugin cam)
	{
		m_Cam = cam;
	}

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		return m_Cam.IsActionEnabled(actionId);
	}

	override void OnAction(string actionId)
	{
		m_Cam.OnAction(actionId);
	}

	override bool IsActionChecked(string actionId)
	{
		return m_Cam.IsActionChecked(actionId);
	}
}

class DZS_CameraEsc : DZS_EscHandler
{
	protected DZS_CameraPlugin m_Cam;

	void DZS_CameraEsc(DZS_CameraPlugin cam)
	{
		m_Cam = cam;
	}

	override bool IsEscActive() { return m_Cam.IsFreeLook(); }
	override void OnEsc() { m_Cam.SetFreeLook(false); }
}

[DZS_RegisterPlugin(DZS_CameraPlugin)]
class DZS_CameraPlugin : DZS_Plugin
{
	static const int STAGE_MIN = 1;
	static const int STAGE_MAX = 45;
	static const int STAGE_DEFAULT = 21;
	static const float DEG_PER_PX = 0.075;
	// GetMouseState(X/Y) units per screen pixel of cursor movement, measured on the test PC.
	static const float MOUSE_UNITS_PER_PX = 48.5;
	static const float MIN_HEIGHT = 0.5;
	static const float COLLISION_GAP = 0.3;
	static const float MAX_Y = 3000;
	static const float FRAME_Y_MIN = -1000;
	static const float FRAME_Y_MAX = 10000;
	static const float FLY_TIME = 0.4;
	static const float JUMP_DIST = 20;
	static const float JUMP_PITCH = 30;

	protected ref DZS_CameraActions m_Actions;
	protected ref DZS_CameraEsc m_Esc;
	protected Camera m_Cam;
	protected vector m_Pos;
	protected float m_Yaw;
	protected float m_Pitch;
	protected float m_WorldSize;
	protected float m_Time;
	protected bool m_Started;

	protected int m_Stage;
	protected float m_TurnPx;
	protected ref map<string, bool> m_NeedRelease;
	protected bool m_WasBlocked;

	protected int m_Mode;
	protected float m_PendingPx;
	protected float m_PendingDx;
	protected float m_PendingDy;
	protected bool m_RmbWas;
	protected bool m_MmbWas;
	protected vector m_Pivot;
	protected float m_OrbitDist;
	protected bool m_OrbitPivot;

	protected bool m_FreeLook;
	protected bool m_AnarchyLatched;
	protected bool m_AnarchyKey;
	protected bool m_AnarchyMouse;
	protected bool m_AnarchyWas;
	protected string m_AnarchySource = "button";
	protected float m_LastLimitHint = -1000;

	protected bool m_Flying;
	protected float m_FlyT;
	protected vector m_FlyFromPos;
	protected vector m_FlyToPos;
	protected float m_FlyFromYaw;
	protected float m_FlyToYaw;
	protected float m_FlyFromPitch;
	protected float m_FlyToPitch;

	protected ref DZS_CameraState m_LastFired;
	protected bool m_Driving;
	bool m_LogHits;
	protected vector m_PosBeforeFrame;
	protected float m_DriveStart;
	protected vector m_DriveFrom;
	protected int m_DriveFrames;
	protected DZS_CameraLight m_Light;
	protected bool m_LightOn;

	protected ref DZS_StatusText m_SpeedItem;
	protected ref DZS_NumberField m_FieldX;
	protected ref DZS_NumberField m_FieldY;
	protected ref DZS_NumberField m_FieldZ;
	protected ref DZS_CameraGear m_Gear;
	protected ref DZS_CameraMap m_Map;
	protected ref DZS_CameraChar m_Char;

	static const int MODE_NONE = 0;
	static const int MODE_PENDING = 1;
	static const int MODE_ROTATE = 2;
	static const int MODE_ORBIT = 3;

	override string GetName() { return "camera"; }

	override void OnRegister()
	{
		m_NeedRelease = new map<string, bool>();
		m_Actions = new DZS_CameraActions(this);
		m_Esc = new DZS_CameraEsc(this);
		RegisterTexts();

		DZS_Settings st = m_Kernel.GetSettings();
		st.DefineSlider("camera.fov", "camera", "Sichtfeld", "Field of view", "Wie viel die Kamera waagerecht zeigt", "How much the camera shows from side to side", 30, 120, 1, 0, "°", 70);
		st.DefineSlider("camera.viewDistance", "camera", "Klick-Reichweite", "Click range", "Wie weit Klicks in die Szene reichen", "How far clicks into the scene reach", 200, 5000, 100, 0, " m", 1500);
		st.DefineToggle("camera.groundProtection", "camera", "Nicht unter das Gelände", "Not below the ground", "Die Kamera bleibt über dem Boden", "The camera stays above the ground", true);
		st.DefineToggle("camera.collision", "camera", "Kollision", "Collision", "Die Kamera stoppt vor Wänden und Objekten", "The camera stops in front of walls and objects", true);
		st.DefineToggle("camera.invertPitch", "camera", "Neigung umkehren", "Invert tilt", "Maus nach oben neigt die Kamera nach unten", "Mouse up tilts the camera down", false);
		st.DefineSlider("camera.mouseSensitivity", "camera", "Mausempfindlichkeit", "Mouse sensitivity", "Wie weit die Kamera je Mausbewegung dreht", "How far the camera turns per mouse movement", 25, 200, 5, 0, " %", 100);
		st.DefineToggle("camera.playerInvulnerable", "camera", "Spielfigur unverwundbar", "Character invulnerable", "Die Spielfigur nimmt keinen Schaden", "The character takes no damage", true);
		st.Define("camera.speedStage", DZS_Json.Int(STAGE_DEFAULT));

		DZS_UiRegistry ui = m_Kernel.GetUi();
		Hold(ui, "camera.forward", "Vorwärts fahren", "Drive forward", "W");
		Hold(ui, "camera.back", "Rückwärts fahren", "Drive backward", "S");
		Hold(ui, "camera.left", "Links fahren", "Drive left", "A");
		Hold(ui, "camera.right", "Rechts fahren", "Drive right", "D");
		Hold(ui, "camera.up", "Hoch", "Up", "E");
		Hold(ui, "camera.down", "Runter", "Down", "Q");
		array<string> drives = {"camera.forward", "camera.back", "camera.left", "camera.right", "camera.up", "camera.down"};
		foreach (string did : drives)
			ui.SetNoCtrl(did);
		Hold(ui, "camera.faster", "Schneller", "Faster", "LSHIFT");
		Hold(ui, "camera.slower", "Langsamer", "Slower", "LMENU");
		ui.SetTempo("camera.faster");
		ui.SetTempo("camera.slower");
		Hold(ui, "camera.rotate", "Drehen und neigen", "Turn and tilt", "MouseRight");
		Hold(ui, "camera.orbit", "Um die Auswahl kreisen", "Orbit the selection", "Alt+MouseRight");
		Hold(ui, "camera.speed", "Tempo hoch / runter", "Speed up / down", "Wheel");
		ui.SetGesture("camera.rotate");
		ui.SetGesture("camera.orbit");
		ui.SetGesture("camera.speed");
		array<string> anarchyKeys = {"GRAVE", "Mouse4"};
		ui.RegisterAction("camera", "camera.anarchy", "Anarchy-Modus", "Anarchy mode", anarchyKeys, m_Actions);
		ui.SetHold("camera.anarchy");
		Press(ui, "camera.freeLook", "Freie Sicht", "Free look", "SPACE");
		Press(ui, "camera.jump", "Sprung zum Mauszeiger", "Jump to pointer", "MouseMiddle");
		ui.SetWhen("camera.jump", "scene,map");
		Press(ui, "camera.focus", "Fokus auf Auswahl", "Focus selection", "F");
		ui.SetWhen("camera.focus", "selection");
		Press(ui, "camera.player", "Spielfigur", "Character", "HOME");
		ui.SetWhen("camera.player", "scene,character");
		Press(ui, "camera.playerAtCursor", "Spielfigur zum Mauszeiger", "Character to pointer", "T");
		ui.SetWhen("camera.playerAtCursor", "scene");
		Press(ui, "camera.invulnerable", "Unverwundbar umschalten (nur in der Spielfigur)", "Toggle invulnerable (character only)", "INSERT");
		ui.SetWhen("camera.invulnerable", "character");
		Press(ui, "camera.light", "Kameralicht", "Camera light", "Shift+L");
		Press(ui, "camera.hideUi", "Oberfläche aus/an", "Interface off/on", "U");
		ui.SetWhen("camera.hideUi", "scene,map");
		Press(ui, "camera.map", "Karte", "Map", "M");
		ui.SetWhen("camera.map", "scene,map");
		ui.AddMenuEntry("view", "camera.map", 160);
		Press(ui, "camera.copyPosition", "Position kopieren", "Copy position", "");
		ui.SetListed("camera.copyPosition", false);
		ui.SetWhen("camera.copyPosition", "scene,map");
		Press(ui, "camera.collision", "Kollision", "Collision", "");
		Press(ui, "camera.anarchyLatch", "Anarchy", "Anarchy", "");
		ui.SetListed("camera.collision", false);
		ui.SetWhen("camera.collision", "scene,map,fullsearch");
		ui.SetListed("camera.anarchyLatch", false);
		ui.AddMenuEntry("view", "camera.hideUi", 210);
		ui.AddMenuEntry("view", "camera.collision", 310, true);
		ui.AddMenuEntry("view", "camera.light", 320, true);
		ui.RegisterTool("camera", "camera.anarchyLatch", "Anarchy", "Anarchy", "switches", "zap", 10);
		ui.RegisterTool("camera", "camera.light", "Kameralicht", "Camera light", "switches", "flashlight", 20);
	}

	protected void RegisterTexts()
	{
		DZS_Texts.Add("camera.speed", "%1 m/s", "%1 m/s");
		DZS_Texts.Add("camera.speed.fast", "%1 m/s ×5", "%1 m/s ×5");
		DZS_Texts.Add("camera.speed.slow", "%1 m/s ×1/5", "%1 m/s ×1/5");
		DZS_Texts.Add("camera.gear.min", "min", "min");
		DZS_Texts.Add("camera.gear.max", "max", "max");
		DZS_Texts.Add("camera.freeLook.banner", "Freie Sicht — [%1] zum Beenden", "Free look — [%1] to exit");
		DZS_Texts.Add("camera.anarchy.banner", "ANARCHY — Boden, Kollision und Kartengrenzen aus · %1", "ANARCHY — ground, collision and map borders off · %1");
		DZS_Texts.Add("camera.anarchy.endKey", "[%1] loslassen", "release [%1]");
		DZS_Texts.Add("camera.anarchy.endButton", "Knopf zum Beenden", "button to end");
		DZS_Texts.Add("camera.anarchy.hint", "Anarchy-Modus: [%1] oder [%2] halten, um weiterzufahren", "Anarchy mode: hold [%1] or [%2] to keep going");
		DZS_Texts.Add("camera.anarchy.hint1", "Anarchy-Modus: [%1] halten, um weiterzufahren", "Anarchy mode: hold [%1] to keep going");
		DZS_Texts.Add("camera.anarchy.hint0", "Anarchy-Modus: Knopf in der oberen Leiste, um weiterzufahren", "Anarchy mode: button in the top bar to keep going");
		DZS_Texts.Add("camera.collision.on", "Kollision an", "Collision on");
		DZS_Texts.Add("camera.collision.off", "Kollision aus", "Collision off");
		DZS_Texts.Add("camera.ground.off", "Bodenschutz aus", "Ground protection off");
		DZS_Texts.Add("camera.copied", "Position kopiert: %1", "Position copied: %1");
		DZS_Texts.Add("camera.map.template", "Vorlagen nur in der Szene setzen", "Place templates in the scene only");
		DZS_Texts.Add("camera.map.help", "Doppelklick oder [%1]: Kamera hierhin · Mausrad zoomen · Ziehen verschieben · [%2] oder [Esc] schließen", "Double-click or [%1]: camera here · wheel zooms · drag pans · [%2] or [Esc] closes");
	}

	protected void Hold(DZS_UiRegistry ui, string id, string de, string en, string key)
	{
		array<string> keys = {key};
		ui.RegisterAction("camera", id, de, en, keys, m_Actions);
		ui.SetHold(id);
		ui.SetGroup(id, "camera");
	}

	protected void Press(DZS_UiRegistry ui, string id, string de, string en, string key)
	{
		array<string> keys = new array<string>();
		if (key != "")
			keys.Insert(key);
		ui.RegisterAction("camera", id, de, en, keys, m_Actions);
		ui.SetGroup(id, "camera");
	}

	override void OnSessionStart(string world)
	{
		if (world == "")
			return;
		m_WorldSize = GetGame().GetWorld().GetWorldSize();
		m_Stage = Math.Clamp(Math.Round(m_Kernel.GetSettings().GetNumber("camera.speedStage", STAGE_DEFAULT)), STAGE_MIN, STAGE_MAX);
		m_Cam = Camera.Cast(GetGame().CreateObject("staticcamera", "0 0 0", true));
		StartPosition(world);
		m_Cam.SetActive(true);
		ApplyFov();
		BuildUi();
		ApplySettingIcons();
		m_Char = new DZS_CameraChar(this, m_Kernel);
		m_Started = true;
		DZS_Log.Info("camera ready, world size " + m_WorldSize + ", stage " + m_Stage + " (" + SpeedText(Speed(), "") + ")");
	}

	override void OnSessionEnd()
	{
		if (!m_Started)
			return;
		if (m_Char)
			m_Char.End();
		m_Char = null;
		SetLight(false);
		if (m_FreeLook)
			SetFreeLook(false);
		m_Kernel.GetSettings().Set("camera.speedStage", DZS_Json.Int(m_Stage));
		if (m_Map)
			m_Map.Destroy();
		m_Map = null;
		if (m_Cam)
			GetGame().ObjectDelete(m_Cam);
		m_Started = false;
	}

	// ---- start position ----

	protected void StartPosition(string world)
	{
		float x = m_WorldSize / 2;
		float z = m_WorldSize / 2;
		float h = 100;
		float yaw = 0;
		float pitch = -30;
		if (world == "ChernarusPlus")
		{
			x = 4600; z = 9300; h = 150; yaw = 0; pitch = -25;
		}
		else if (world == "Enoch")
		{
			x = 4450; z = 9700; h = 150; yaw = 320; pitch = -20;
		}
		else if (world == "Sakhal")
		{
			x = 7180; z = 6600; h = 150; yaw = 0; pitch = -20;
		}
		else if (world == "Nasdara")
		{
			x = 9900; z = 3300; h = 200; yaw = 0; pitch = -20;
		}
		m_Pos = Vector(x, GetGame().SurfaceY(x, z) + h, z);
		m_Yaw = yaw;
		m_Pitch = pitch;
		Apply();
	}

	// ---- public interface ----

	bool IsStarted() { return m_Started; }
	DZS_Kernel Kernel() { return m_Kernel; }
	DZS_CameraMap GetMap() { return m_Map; }

	void GoToStart()
	{
		StartPosition(m_Kernel.GetWorld());
	}

	vector GetPosition() { return m_Pos; }
	float GetYaw() { return m_Yaw; }
	float GetPitch() { return m_Pitch; }
	bool IsAnarchy() { return m_AnarchyLatched || m_AnarchyKey || m_AnarchyMouse; }
	bool IsFreeLook() { return m_FreeLook; }
	int GetStage() { return m_Stage; }
	float GetWorldSize() { return m_WorldSize; }

	// Base speed of the current stage: 2^(n/4) m/s with n = stage − 9.
	float Speed()
	{
		return Math.Pow(2, (m_Stage - 9) / 4.0);
	}

	float ViewDistance()
	{
		return m_Kernel.GetSettings().GetSettingNumber("camera.viewDistance");
	}

	// Sets the camera; with limits the position goes through the same rules as typed input.
	void SetPose(vector pos, float yaw, float pitch, bool applyLimits = true)
	{
		m_Flying = false;
		if (applyLimits)
			pos = ClampTyped(pos);
		m_Pos = pos;
		m_Yaw = NormYaw(yaw);
		m_Pitch = Math.Clamp(pitch, -89, 89);
		Apply();
	}

	void FlyTo(vector pos, float yaw, float pitch, float seconds = FLY_TIME)
	{
		m_FlyFromPos = m_Pos;
		m_FlyFromYaw = m_Yaw;
		m_FlyFromPitch = m_Pitch;
		m_FlyToPos = pos;
		m_FlyToYaw = m_Yaw + AngleDiff(m_Yaw, yaw);
		m_FlyToPitch = Math.Clamp(pitch, -89, 89);
		m_FlyT = 0;
		if (seconds <= 0)
		{
			SetPose(pos, yaw, pitch, false);
			return;
		}
		m_Flying = true;
	}

	// Looks at a box from the current heading, 30° down, so that the box fills about two thirds of the height.
	void FocusOn(vector boxMin, vector boxMax, bool fly = true)
	{
		vector center = (boxMin + boxMax) * 0.5;
		float radius = vector.Distance(boxMin, boxMax) * 0.5;
		radius = Math.Max(radius, 0.25);
		float vfov = VerticalFov();
		float dist = radius / Math.Tan(vfov * 0.5 * 0.67);
		float pitch = -30;
		vector dir = Dir(m_Yaw, pitch);
		vector pos = center - dir * dist;
		if (fly)
			FlyTo(pos, m_Yaw, pitch);
		else
			SetPose(pos, m_Yaw, pitch, false);
	}

	// ---- geometry ----

	static vector Dir(float yaw, float pitch)
	{
		float y = yaw * Math.DEG2RAD;
		float p = pitch * Math.DEG2RAD;
		return Vector(Math.Sin(y) * Math.Cos(p), Math.Sin(p), Math.Cos(y) * Math.Cos(p));
	}

	static vector RightOf(float yaw)
	{
		float y = yaw * Math.DEG2RAD;
		return Vector(Math.Cos(y), 0, -Math.Sin(y));
	}

	static float NormYaw(float yaw)
	{
		while (yaw < 0)
			yaw += 360;
		while (yaw >= 360)
			yaw -= 360;
		return yaw;
	}

	static float AngleDiff(float from, float to)
	{
		float d = NormYaw(to) - NormYaw(from);
		if (d > 180)
			d -= 360;
		if (d < -180)
			d += 360;
		return d;
	}

	float HorizontalFov()
	{
		return m_Kernel.GetSettings().GetSettingNumber("camera.fov");
	}

	float Aspect()
	{
		int w, h;
		GetScreenSize(w, h);
		return w / Math.Max(1.0, h);
	}

	float VerticalFov()
	{
		float hf = HorizontalFov() * Math.DEG2RAD;
		return 2 * Math.Atan(Math.Tan(hf / 2) / Aspect());
	}

	// Measured in the game: the engine's value p gives tan(vertical/2) = 0.75 · p.
	float EngineFov()
	{
		float hf = HorizontalFov() * Math.DEG2RAD;
		return Math.Tan(hf / 2) / (Aspect() * 0.75);
	}

	protected void ApplyFov()
	{
		if (m_Cam)
			m_Cam.SetFOV(EngineFov());
	}

	protected void Apply()
	{
		if (!m_Cam)
			return;
		m_Cam.SetPosition(m_Pos);
		m_Cam.SetOrientation(Vector(m_Yaw, m_Pitch, 0));
		if (m_Light)
		{
			m_Light.SetPosition(m_Pos);
			m_Light.SetOrientation(Vector(m_Yaw, m_Pitch, 0));
		}
	}

	float GroundY(vector at)
	{
		return GetGame().SurfaceY(at[0], at[2]);
	}

	bool GroundOn()
	{
		return m_Kernel.GetSettings().GetBool("camera.groundProtection", true);
	}

	bool CollisionOn()
	{
		return m_Kernel.GetSettings().GetBool("camera.collision", true);
	}

	protected bool InsideMap(vector at)
	{
		return at[0] >= 0 && at[0] <= m_WorldSize && at[2] >= 0 && at[2] <= m_WorldSize && at[1] <= MAX_Y;
	}

	// Position input and jumps: map borders, minimum height; in anarchy only the technical frame.
	vector ClampTyped(vector want)
	{
		if (IsAnarchy())
			return ClampFrame(want);
		float x = Math.Clamp(want[0], 0, m_WorldSize);
		float z = Math.Clamp(want[2], 0, m_WorldSize);
		float y = Math.Min(want[1], MAX_Y);
		if (GroundOn())
			y = Math.Max(y, GetGame().SurfaceY(x, z) + MIN_HEIGHT);
		else
			y = Math.Max(y, FRAME_Y_MIN);
		return Vector(x, y, z);
	}

	vector ClampFrame(vector want)
	{
		float x = Math.Clamp(want[0], -m_WorldSize, 2 * m_WorldSize);
		float z = Math.Clamp(want[2], -m_WorldSize, 2 * m_WorldSize);
		float y = Math.Clamp(want[1], FRAME_Y_MIN, FRAME_Y_MAX);
		return Vector(x, y, z);
	}

	// Ray against solid objects only (terrain is the ground protection's job; soft bushes do not stop).
	// Returns the hit distance or -1.
	protected float CastObjects(vector from, vector to, out vector normal)
	{
		float total = vector.Distance(from, to);
		if (total < 0.000001)
			return -1;
		vector dir = (to - from) * (1.0 / total);
		vector start = from;
		float done = 0;
		Object skip = null;
		for (int i = 0; i < 6; i++)
		{
			vector contact, contactDir;
			int comp;
			set<Object> hits = new set<Object>();
			if (!DayZPhysics.RaycastRV(start, to, contact, contactDir, comp, hits, null, skip, true, false, ObjIntersectGeom, 0.0))
				return -1;
			if (hits.Count() == 0)
				return -1;
			Object o = hits[0];
			float d = done + vector.Distance(start, contact);
			if (o.IsBush() && o.GetType().Contains("Soft"))
			{
				skip = o;
				done = d + 0.01;
				start = from + dir * done;
				continue;
			}
			normal = contactDir;
			if (m_LogHits)
				DZS_Log.Info("collision with " + o.GetType() + " at " + contact.ToString() + " dist " + d.ToString());
			return d;
		}
		return -1;
	}

	// One movement step under the protections; sets blocked when a protection took a noticeable part.
	protected vector MoveLimited(vector from, vector delta, out bool blocked)
	{
		blocked = false;
		bool anarchy = IsAnarchy();
		vector target = from + delta;
		float len = delta.Length();
		if (len < 0.000001)
			return from;
		vector dirN = delta * (1.0 / len);

		if (!anarchy && CollisionOn())
		{
			vector n;
			float hit = CastObjects(from, target + dirN * COLLISION_GAP, n);
			if (hit >= 0)
			{
				blocked = true;
				float allowed = Math.Max(0, hit - COLLISION_GAP);
				vector reached = from + dirN * Math.Min(allowed, len);
				vector rest = delta - dirN * Math.Min(allowed, len);
				if (n.Length() > 0.001)
				{
					n.Normalize();
					rest = rest - n * vector.Dot(rest, n);
				}
				target = reached;
				float rl = rest.Length();
				if (rl > 0.000001)
				{
					vector rn = rest * (1.0 / rl);
					vector n2;
					float hit2 = CastObjects(reached, reached + rest + rn * COLLISION_GAP, n2);
					if (hit2 < 0)
						target = reached + rest;
					else
						target = reached + rn * Math.Max(0, Math.Min(rl, hit2 - COLLISION_GAP));
				}
			}
		}

		if (anarchy)
			target = ClampFrame(target);
		else
		{
			if (from[0] >= 0 && from[0] <= m_WorldSize)
				target[0] = Math.Clamp(target[0], 0, m_WorldSize);
			if (from[2] >= 0 && from[2] <= m_WorldSize)
				target[2] = Math.Clamp(target[2], 0, m_WorldSize);
			if (from[1] <= MAX_Y)
				target[1] = Math.Min(target[1], MAX_Y);
			if (GroundOn())
			{
				float minFrom = GroundY(from) + MIN_HEIGHT;
				float minTo = GroundY(target) + MIN_HEIGHT;
				if (from[1] >= minFrom - 0.01)
					target[1] = Math.Max(target[1], minTo);
			}
			target = ClampFrame(target);
		}

		vector moved = target - from;
		if (vector.Dot(moved, dirN) < len * 0.1)
			blocked = true;
		else if (!blocked)
			blocked = false;
		return target;
	}

	// ---- actions ----

	bool IsActionEnabled(string id)
	{
		if (!m_Started)
			return false;
		if (id == "camera.invulnerable")
			return m_Char && m_Char.IsIn();
		if (id == "camera.playerAtCursor")
		{
			DZS_Screen ps = m_Kernel.GetScreen();
			return ps.IsCursorVisible() && ps.IsPointerOverScene();
		}
		if (id == "camera.jump" && m_Map && m_Map.IsOpen())
			return true;
		if (id == "camera.freeLook" || id == "camera.jump")
			return !m_Kernel.GetScreen().IsSceneKeyboardBlocked();
		return true;
	}

	void OnAction(string id)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		if (id == "camera.player" && m_Char)
			m_Char.Toggle();
		else if (id == "camera.invulnerable" && m_Char)
			m_Char.ToggleInvulnerable();
		else if (id == "camera.playerAtCursor" && m_Char)
			CharacterToPointer();
		else if (id == "camera.freeLook")
			SetFreeLook(!m_FreeLook);
		else if (id == "camera.jump" && m_Map && m_Map.IsOpen())
			m_Map.MiddleJump();
		else if (id == "camera.jump")
			JumpToPointer();
		else if (id == "camera.map" && m_Map)
			m_Map.Toggle();
		else if (id == "camera.hideUi")
			scr.SetUiHidden(!scr.IsUiHidden());
		else if (id == "camera.copyPosition")
			CopyPosition();
		else if (id == "camera.collision")
		{
			bool next = !CollisionOn();
			DZS_Json value = DZS_Json.Boolean(next);
			m_Kernel.GetSettings().Set("camera.collision", value);
		}
		else if (id == "camera.anarchyLatch")
			m_AnarchyLatched = !m_AnarchyLatched;
		else if (id == "camera.light")
			SetLight(!m_LightOn);
		else if (id == "camera.focus")
			m_Kernel.GetEvents().FirePluginEvent("camera.focusRequest", null);
	}

	bool IsActionChecked(string id)
	{
		if (id == "camera.collision")
			return CollisionOn();
		if (id == "camera.light")
			return m_LightOn;
		if (id == "camera.anarchyLatch")
			return m_AnarchyLatched;
		return false;
	}

	void SetFreeLook(bool on)
	{
		if (on == m_FreeLook)
			return;
		m_FreeLook = on;
		DZS_Screen scr = m_Kernel.GetScreen();
		scr.SetCursorHidden("camera.freeLook", on);
		if (on)
			scr.ShowBanner("camera.freeLook", 1, "info", DZS_Texts.Fmt("camera.freeLook.banner", m_Kernel.GetUi().FirstBinding("camera.freeLook")));
		else
			scr.HideBanner("camera.freeLook");
		DZS_Log.Info("free look " + DZS_Str.Bool(on));
	}

	void SetLight(bool on)
	{
		if (on == m_LightOn)
			return;
		m_LightOn = on;
		if (on)
		{
			m_Light = DZS_CameraLight.Cast(ScriptedLightBase.CreateLight(DZS_CameraLight, m_Pos));
			Apply();
		}
		else if (m_Light)
		{
			m_Light.Destroy();
			m_Light = null;
		}
		DZS_Log.Info("camera light " + DZS_Str.Bool(on));
		m_Kernel.GetEvents().FirePluginEvent("camera.lightChanged", null);
	}

	bool IsLightOn() { return m_LightOn; }

	string PositionText()
	{
		return DZS_Num.Fixed(m_Pos[0], 2, true) + " " + DZS_Num.Fixed(m_Pos[1], 2, true) + " " + DZS_Num.Fixed(m_Pos[2], 2, true);
	}

	protected void CopyPosition()
	{
		string t = PositionText();
		GetGame().CopyToClipboard(t);
		m_Kernel.GetScreen().ShowMessage("camera.copied", "success", DZS_Texts.Fmt("camera.copied", t), 2.0);
		DZS_Log.Info("copied position '" + t + "'");
	}

	// Point of the scene under the pointer (or the screen centre without pointer), within the view distance.
	bool PickScene(out vector hit)
	{
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir;
		if (m_Kernel.GetScreen().IsCursorVisible())
			dir = GetGame().GetPointerDirection();
		else
			dir = Dir(m_Yaw, m_Pitch);
		vector to = from + dir * ViewDistance();
		vector contactDir;
		int comp;
		set<Object> hits = new set<Object>();
		if (DayZPhysics.RaycastRV(from, to, hit, contactDir, comp, hits, null, null, true, false, ObjIntersectView, 0.0))
			return true;
		// long rays of DayZ 1.30 can miss the terrain: the terrain alone, in pieces
		float len = ViewDistance();
		float at = 0;
		while (at < len)
		{
			float step = Math.Min(500, len - at);
			vector a0 = from + dir * at;
			vector a1 = from + dir * (at + step);
			set<Object> none = new set<Object>();
			if (DayZPhysics.RaycastRV(a0, a1, hit, contactDir, comp, none, null, null, false, true, ObjIntersectView, 0.0))
			{
				DZS_Log.Info("pick: terrain at " + vector.Distance(from, hit) + " m in pieces");
				return true;
			}
			at += step;
		}
		return false;
	}

	protected void JumpToPointer()
	{
		if (ModDownLeftAlt())
			return;
		DZS_Screen scr = m_Kernel.GetScreen();
		if (scr.IsCursorVisible() && !scr.IsPointerOverScene())
			return;
		vector hitPos;
		if (!PickScene(hitPos))
		{
			DZS_Log.Info("jump: nothing under the pointer");
			return;
		}
		vector dirH = Vector(Math.Sin(m_Yaw * Math.DEG2RAD), 0, Math.Cos(m_Yaw * Math.DEG2RAD));
		float back = JUMP_DIST * Math.Cos(JUMP_PITCH * Math.DEG2RAD);
		float up = JUMP_DIST * Math.Sin(JUMP_PITCH * Math.DEG2RAD);
		vector c0 = hitPos - dirH * back;
		float y = Math.Max(hitPos[1] + up, GroundY(c0) + 2);
		vector c = ClampTyped(Vector(c0[0], y, c0[2]));
		float dy = hitPos[1] - c[1];
		float dh = Math.Sqrt((hitPos[0] - c[0]) * (hitPos[0] - c[0]) + (hitPos[2] - c[2]) * (hitPos[2] - c[2]));
		float pitch = Math.Atan2(dy, Math.Max(0.01, dh)) * Math.RAD2DEG;
		DZS_Log.Info("jump to " + hitPos.ToString() + " -> " + c.ToString() + " pitch " + pitch);
		FlyTo(c, m_Yaw, pitch);
	}

	protected void CharacterToPointer()
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		if (!scr.IsCursorVisible() || !scr.IsPointerOverScene())
			return;
		vector hit;
		if (!PickScene(hit))
		{
			DZS_Log.Info("character: nothing under the pointer");
			return;
		}
		m_Char.ToPoint(hit);
	}

	// ---- for the character (DZS_CameraChar) ----

	DZS_CameraChar GetCharacter() { return m_Char; }

	void BeforeCharacter()
	{
		if (m_Map && m_Map.IsOpen())
			m_Map.Toggle();
		if (m_FreeLook)
			SetFreeLook(false);
		m_AnarchyLatched = false;
		m_Mode = MODE_NONE;
		m_Flying = false;
	}

	void SetCameraActive(bool on)
	{
		if (!m_Cam)
			return;
		m_Cam.SetActive(on);
		if (on)
		{
			Apply();
			ApplyFov();
		}
	}

	static bool ModDownLeftAlt()
	{
		return KeyState(KeyCode.KC_LMENU) != 0;
	}

	// ---- settings ----

	override void OnSettingChanged(string key)
	{
		if (key == "camera.fov")
			ApplyFov();
		if (key == "camera.collision" && m_Started)
		{
			string k = "camera.collision.off";
			if (CollisionOn())
				k = "camera.collision.on";
			m_Kernel.GetScreen().ShowMessage("camera.collision", "info", DZS_Texts.Get(k), 2.0);
		}
		if (key == "camera.collision" || key == "camera.groundProtection")
			ApplySettingIcons();
	}

	protected void ApplySettingIcons()
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		scr.SetStatusIcon("camera.ground", !GroundOn(), "arrow-down-to-line", DZS_Texts.Get("camera.ground.off"), "warning");
		scr.SetStatusIcon("camera.collision", !CollisionOn(), "ghost", DZS_Texts.Get("camera.collision.off"), "warning");
	}

	// ---- status bar ----

	protected void BuildUi()
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		m_SpeedItem = new DZS_StatusText("camera.speed", DZS_Font.MONO, 0, 120, true);
		scr.AddStatusRight(m_SpeedItem);
		m_FieldX = new DZS_NumberField("camera.x", "X", "axisX", 112);
		m_FieldY = new DZS_NumberField("camera.y", "Y", "axisY", 112);
		m_FieldZ = new DZS_NumberField("camera.z", "Z", "axisZ", 112);
		scr.AddStatusRight(m_FieldX);
		scr.AddStatusRight(m_FieldY);
		scr.AddStatusRight(m_FieldZ);
		m_FieldX.m_OnCommit.Insert(OnFieldCommit);
		m_FieldY.m_OnCommit.Insert(OnFieldCommit);
		m_FieldZ.m_OnCommit.Insert(OnFieldCommit);
		array<DZS_NumberField> group = {m_FieldX, m_FieldY, m_FieldZ};
		scr.SetFocusGroup(group);
		scr.AddEscHandler(m_Esc, 30);
		m_Gear = new DZS_CameraGear(scr.GetRoot());
		m_Map = new DZS_CameraMap(this);
	}

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		if (m_Map)
			m_Map.PointsChanged();
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		if (m_Map)
			m_Map.PointsChanged();
	}

	protected void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		if (!DZS_Num.Parse(typed, v))
		{
			r.m_Ok = false;
			DZS_Log.Info("position field " + f.m_Id + ": '" + typed + "' is not a number");
			return;
		}
		v = Math.Round(v * 100) / 100;
		vector np = m_Pos;
		if (f == m_FieldX)
			np[0] = v;
		else if (f == m_FieldY)
			np[1] = v;
		else
			np[2] = v;
		m_Flying = false;
		m_Pos = ClampTyped(np);
		Apply();
		r.m_Ok = true;
		DZS_Log.Info("position field " + f.m_Id + " = " + typed + " -> " + m_Pos.ToString());
	}

	string SpeedText(float speed, string mark)
	{
		string num;
		if (speed < 10)
			num = DZS_Num.Fixed(speed, 1);
		else
			num = DZS_Num.Fixed(Math.Round(speed), 0);
		if (mark == "fast")
			return DZS_Texts.Fmt("camera.speed.fast", num);
		if (mark == "slow")
			return DZS_Texts.Fmt("camera.speed.slow", num);
		return DZS_Texts.Fmt("camera.speed", num);
	}

	protected void UpdateStatus(bool fast, bool slow)
	{
		float sp = Speed();
		string mark = "";
		if (fast && !slow)
		{
			sp *= 5;
			mark = "fast";
		}
		else if (slow && !fast)
		{
			sp /= 5;
			mark = "slow";
		}
		m_SpeedItem.SetText(SpeedText(sp, mark));
		m_FieldX.ShowValue(DZS_Num.Fixed(m_Pos[0], 2));
		m_FieldY.ShowValue(DZS_Num.Fixed(m_Pos[1], 2));
		m_FieldZ.ShowValue(DZS_Num.Fixed(m_Pos[2], 2));
	}

	// ---- per frame ----

	protected bool HeldArmed(string id, bool blocked)
	{
		bool held = m_Kernel.GetUi().IsHeld(id);
		if (!held)
		{
			m_NeedRelease.Remove(id);
			return false;
		}
		if (blocked)
		{
			m_NeedRelease.Set(id, true);
			return false;
		}
		return !m_NeedRelease.Contains(id);
	}

	override void OnUpdate(float dt)
	{
		if (!m_Started || !m_Cam)
			return;
		m_Time += dt;
		if (m_Char)
		{
			m_Char.Update(dt);
			if (m_Char.IsIn())
			{
				m_AnarchyKey = false;
				m_AnarchyMouse = false;
				UpdateAnarchyDisplay();
				return;
			}
		}
		if (m_Map)
			m_Map.Update(dt);
		DZS_Screen scr = m_Kernel.GetScreen();
		DZS_UiRegistry ui = m_Kernel.GetUi();
		bool kbBlocked = scr.IsSceneKeyboardBlocked();
		bool ctrl = DZS_UiRegistry.ModDown("Ctrl");
		bool dialog = scr.HasKeyboardOwner("dialog") || GetGame().GetUIManager().GetMenu() != null;
		if (dialog && m_FreeLook)
			SetFreeLook(false);

		// anarchy: key held (not while a field, dialog or menu has the keyboard), mouse 4, or latched button
		bool anarchyHeld = !kbBlocked && ui.IsHeld("camera.anarchy");
		bool mouseAnarchy = false;
		if (anarchyHeld)
		{
			array<string> ab = ui.GetBindings("camera.anarchy");
			foreach (string abk : ab)
			{
				string ak = DZS_UiRegistry.KeyOf(abk);
				if ((ak == "Mouse4" || ak == "Mouse5" || ak == "MouseMiddle") && DZS_UiRegistry.KeyDown(ak))
					mouseAnarchy = true;
			}
		}
		bool keyAnarchy = anarchyHeld && !mouseAnarchy;
		m_AnarchyKey = keyAnarchy;
		m_AnarchyMouse = mouseAnarchy;
		UpdateAnarchyDisplay();

		bool driveBlocked = kbBlocked || ctrl || m_Mode == MODE_ORBIT;
		float fwd = 0;
		float side = 0;
		float up = 0;
		if (HeldArmed("camera.forward", driveBlocked)) fwd += 1;
		if (HeldArmed("camera.back", driveBlocked)) fwd -= 1;
		if (HeldArmed("camera.right", driveBlocked)) side += 1;
		if (HeldArmed("camera.left", driveBlocked)) side -= 1;
		if (HeldArmed("camera.up", driveBlocked)) up += 1;
		if (HeldArmed("camera.down", driveBlocked)) up -= 1;
		bool fast = !kbBlocked && ui.IsHeld("camera.faster");
		bool slow = !kbBlocked && ui.IsHeld("camera.slower") && m_Mode != MODE_ORBIT;

		m_PosBeforeFrame = m_Pos;
		UpdateMouse(dt, kbBlocked, dialog);

		if (fwd != 0 || side != 0 || up != 0)
		{
			m_Flying = false;
			float speed = Speed();
			if (fast && !slow)
				speed *= 5;
			else if (slow && !fast)
				speed /= 5;
			vector dir = Dir(m_Yaw, m_Pitch) * fwd + RightOf(m_Yaw) * side + Vector(0, up, 0);
			float dl = dir.Length();
			if (dl > 0.0001)
			{
				dir = dir * (1.0 / dl);
				bool blocked;
				vector before = m_Pos;
				m_Pos = MoveLimited(m_Pos, dir * speed * dt, blocked);
				if (blocked && !IsAnarchy())
				{
					float progress = vector.Dot(m_Pos - before, dir) / Math.Max(dt, 0.0001);
					if (progress < 0.1 * Speed())
						LimitHint();
				}
			}
		}

		bool driving = fwd != 0 || side != 0 || up != 0;
		if (driving && !m_Driving)
		{
			m_DriveStart = m_Time - dt;
			m_DriveFrom = m_PosBeforeFrame;
			m_DriveFrames = 0;
		}
		if (driving)
			m_DriveFrames++;
		if (!driving && m_Driving)
			LogDrive();
		m_Driving = driving;

		UpdateFlight(dt);
		Apply();
		UpdateStatus(fast, slow);
		m_Gear.Update(dt, scr);
		FireChanged();
	}

	protected int m_PressMx;
	protected int m_PressMy;

	protected void UpdateMouse(float dt, bool kbBlocked, bool dialog)
	{
		DZS_Screen scr = m_Kernel.GetScreen();
		bool rmb = DZS_UiRegistry.KeyDown("MouseRight");
		int rawX = GetMouseState(MouseState.X);
		int rawY = GetMouseState(MouseState.Y);
		float scale = DZS_Scale.Factor();
		float dxPx = rawX / MOUSE_UNITS_PER_PX / scale;
		float dyPx = rawY / MOUSE_UNITS_PER_PX / scale;
		bool mouseBlocked = dialog || scr.HasKeyboardOwner("map") || scr.HasKeyboardOwner("menu");

		if (rmb && !m_RmbWas && !mouseBlocked && (m_FreeLook || scr.IsPointerOverScene()))
		{
			m_Mode = MODE_PENDING;
			m_TurnPx = 0;
			m_PendingPx = 0;
			m_PendingDx = 0;
			m_PendingDy = 0;
			m_OrbitPivot = ModDownLeftAlt();
			if (m_OrbitPivot)
				PreparePivot();
			GetMousePos(m_PressMx, m_PressMy);
		}
		if (!rmb && m_RmbWas && m_Mode != MODE_NONE)
		{
			if (m_Mode == MODE_PENDING)
			{
				DZS_Log.Info("right click without turning");
				m_Kernel.GetEvents().FirePluginEvent("camera.rightClick", null);
			}
			else
				DZS_Log.Info("turn end, " + m_TurnPx + " px, yaw " + m_Yaw + " pitch " + m_Pitch);
			m_Mode = MODE_NONE;
			scr.SetCursorHidden("camera.rotate", false);
			m_Kernel.SetGesture("camera.turn", false);
		}
		m_RmbWas = rmb;
		if (mouseBlocked && m_Mode != MODE_NONE)
		{
			m_Mode = MODE_NONE;
			scr.SetCursorHidden("camera.rotate", false);
			m_Kernel.SetGesture("camera.turn", false);
		}

		if (m_Mode == MODE_PENDING)
		{
			m_PendingDx += dxPx;
			m_PendingDy += dyPx;
			m_PendingPx = Math.Sqrt(m_PendingDx * m_PendingDx + m_PendingDy * m_PendingDy);
			int pmx, pmy;
			GetMousePos(pmx, pmy);
			float posPx = Math.Sqrt((pmx - m_PressMx) * (pmx - m_PressMx) + (pmy - m_PressMy) * (pmy - m_PressMy)) / scale;
			if (posPx > m_PendingPx)
				m_PendingPx = posPx;
			if (m_PendingPx >= 4)
			{
				scr.SetCursorHidden("camera.rotate", true);
				m_Flying = false;
				m_Kernel.SetGesture("camera.turn", true);
				if (m_OrbitPivot && m_OrbitDist >= 0.1)
					m_Mode = MODE_ORBIT;
				else
					m_Mode = MODE_ROTATE;
				dxPx = m_PendingDx;
				dyPx = m_PendingDy;
				DZS_Log.Info("turn start mode " + m_Mode);
			}
			else
				return;
		}

		float dYaw = 0;
		float dPitch = 0;
		bool turning = m_Mode == MODE_ROTATE || m_Mode == MODE_ORBIT || (m_FreeLook && !mouseBlocked);
		if (turning && (dxPx != 0 || dyPx != 0))
		{
			m_TurnPx += dxPx;
			float degPx = DEG_PER_PX * Math.Clamp(m_Kernel.GetSettings().GetNumber("camera.mouseSensitivity", 100), 25, 200) / 100.0;
			dYaw = dxPx * degPx;
			dPitch = -dyPx * degPx;
			if (m_Kernel.GetSettings().GetBool("camera.invertPitch", false))
				dPitch = -dPitch;
			m_Flying = false;
			if (m_Mode == MODE_ORBIT)
				Orbit(dYaw, dPitch);
			else
			{
				m_Yaw = NormYaw(m_Yaw + dYaw);
				m_Pitch = Math.Clamp(m_Pitch + dPitch, -89, 89);
			}
		}

		// wheel: speed stage, only over the scene (or without pointer) and never with an open dialog
		// in the hand the wheel turns the object; with Ctrl the camera stands
		int wheel = GetMouseState(MouseState.WHEEL);
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		bool handBusy = cat && cat.GetHand() != "";
		bool toolWheel = m_Kernel.GetSceneTool() != "" && DZS_UiRegistry.ModDown("Alt");
		if (wheel != 0 && !mouseBlocked && !handBusy && !toolWheel && !m_Kernel.IsWheelClaimed() && !DZS_UiRegistry.ModDown("Ctrl") && (!scr.IsCursorVisible() || scr.IsPointerOverScene()))
			Shift(wheel);
	}

	protected void PreparePivot()
	{
		m_OrbitDist = -1;
		vector hit;
		if (!PickScene(hit))
			return;
		m_Pivot = hit;
		m_OrbitDist = vector.Distance(m_Pos, m_Pivot);
	}

	// Moves the camera on a sphere around the pivot and turns the view by the same angles (no roll).
	protected void Orbit(float dYaw, float dPitch)
	{
		float newPitch = Math.Clamp(m_Pitch + dPitch, -89, 89);
		dPitch = newPitch - m_Pitch;
		vector rel = m_Pos - m_Pivot;
		vector afterYaw = RotY(rel, dYaw);
		vector axis = RightOf(m_Yaw + dYaw) * -1;
		vector both = RotAxis(afterYaw, axis, dPitch);
		bool blocked;
		vector cand = m_Pivot + both;
		if (OrbitAllowed(cand))
		{
			m_Pos = cand;
			m_Yaw = NormYaw(m_Yaw + dYaw);
			m_Pitch = newPitch;
			return;
		}
		cand = m_Pivot + afterYaw;
		if (dYaw != 0 && OrbitAllowed(cand))
		{
			m_Pos = cand;
			m_Yaw = NormYaw(m_Yaw + dYaw);
		}
		LimitHint();
	}

	protected bool OrbitAllowed(vector cand)
	{
		if (IsAnarchy())
			return vector.Distance(ClampFrame(cand), cand) < 0.001;
		if (InsideMap(m_Pos) && !InsideMap(cand))
			return false;
		if (GroundOn() && m_Pos[1] >= GroundY(m_Pos) + MIN_HEIGHT - 0.01 && cand[1] < GroundY(cand) + MIN_HEIGHT)
			return false;
		if (CollisionOn())
		{
			vector d = cand - m_Pos;
			float l = d.Length();
			if (l > 0.0001)
			{
				vector n;
				float hit = CastObjects(m_Pos, cand + d * (COLLISION_GAP / l), n);
				if (hit >= 0)
					return false;
			}
		}
		return true;
	}

	static vector RotY(vector v, float deg)
	{
		float a = deg * Math.DEG2RAD;
		float c = Math.Cos(a);
		float s = Math.Sin(a);
		return Vector(v[0] * c + v[2] * s, v[1], -v[0] * s + v[2] * c);
	}

	static vector RotAxis(vector v, vector k, float deg)
	{
		float a = deg * Math.DEG2RAD;
		float c = Math.Cos(a);
		float s = Math.Sin(a);
		return v * c + (k * v) * s + k * (vector.Dot(k, v) * (1 - c));
	}

	protected void Shift(int notches)
	{
		int dirSign = 1;
		if (notches < 0)
			dirSign = -1;
		int n = Math.AbsInt(notches);
		for (int i = 0; i < n; i++)
		{
			int next = m_Stage + dirSign;
			if (next < STAGE_MIN || next > STAGE_MAX)
			{
				m_Gear.Bump(m_Stage, dirSign, SpeedText(Speed(), ""));
				continue;
			}
			m_Stage = next;
			m_Gear.Shift(m_Stage, dirSign, SpeedText(Speed(), ""));
		}
		m_Kernel.GetSettings().Set("camera.speedStage", DZS_Json.Int(m_Stage));
		DZS_Log.Info("speed stage " + m_Stage + " = " + SpeedText(Speed(), ""));
	}

	// The binding of the anarchy action that is held now (for the banner text).
	protected string HeldAnarchyBinding()
	{
		array<string> b = m_Kernel.GetUi().GetBindings("camera.anarchy");
		foreach (string k : b)
		{
			if (DZS_UiRegistry.KeyDown(DZS_UiRegistry.KeyOf(k)))
				return k;
		}
		if (b.Count() > 0)
			return b[0];
		return "GRAVE";
	}

	protected void LimitHint()
	{
		if (m_Time - m_LastLimitHint < 20)
			return;
		m_LastLimitHint = m_Time;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		array<string> b = ui.GetBindings("camera.anarchy");
		string text = DZS_Texts.Get("camera.anarchy.hint0");
		if (b.Count() == 1)
			text = DZS_Texts.Fmt("camera.anarchy.hint1", b[0]);
		else if (b.Count() > 1)
			text = DZS_Texts.Fmt("camera.anarchy.hint", b[0], b[1]);
		m_Kernel.GetScreen().ShowMessage("camera.anarchyHint", "warning", text, 3.0);
		DZS_Log.Info("limit hint at " + m_Pos.ToString());
	}

	protected void UpdateAnarchyDisplay()
	{
		bool on = IsAnarchy();
		DZS_Screen scr = m_Kernel.GetScreen();
		string source = "button";
		if (!m_AnarchyLatched)
		{
			if (m_AnarchyKey)
				source = "key";
			else if (m_AnarchyMouse)
				source = "mouse";
		}
		if (on == m_AnarchyWas && source == m_AnarchySource)
			return;
		m_AnarchyWas = on;
		m_AnarchySource = source;
		scr.ShowSceneFrame(on, "warning");
		if (!on)
		{
			scr.HideBanner("camera.anarchy");
			DZS_Log.Info("anarchy off at " + m_Pos.ToString());
			return;
		}
		scr.HideMessage("camera.anarchyHint");
		string end;
		if (source == "key" || source == "mouse")
			end = DZS_Texts.Fmt("camera.anarchy.endKey", HeldAnarchyBinding());
		else
			end = DZS_Texts.Get("camera.anarchy.endButton");
		scr.HideBanner("camera.anarchy");
		scr.ShowBanner("camera.anarchy", 0, "warning", DZS_Texts.Fmt("camera.anarchy.banner", end));
		DZS_Log.Info("anarchy on (" + source + ")");
	}

	// Drive log: time with a drive key down, distance and height change, for the measurements in the game.
	protected void LogDrive()
	{
		float held = m_Time - m_DriveStart;
		vector d = m_Pos - m_DriveFrom;
		float horiz = Math.Sqrt(d[0] * d[0] + d[2] * d[2]);
		float dist = d.Length();
		string t = "drive held " + held.ToString() + " s, frames " + m_DriveFrames.ToString() + ", dist " + dist.ToString();
		t += ", horiz " + horiz.ToString() + ", dy " + d[1].ToString() + ", per s " + (dist / Math.Max(held, 0.001)).ToString();
		DZS_Log.Info(t);
	}

	protected void UpdateFlight(float dt)
	{
		if (!m_Flying)
			return;
		m_FlyT += dt;
		float t = Math.Clamp(m_FlyT / FLY_TIME, 0, 1);
		float e = t * t * (3 - 2 * t);
		m_Pos = m_FlyFromPos + (m_FlyToPos - m_FlyFromPos) * e;
		m_Yaw = NormYaw(m_FlyFromYaw + (m_FlyToYaw - m_FlyFromYaw) * e);
		m_Pitch = m_FlyFromPitch + (m_FlyToPitch - m_FlyFromPitch) * e;
		if (t >= 1)
		{
			m_Flying = false;
			DZS_Log.Info("flight done after " + m_FlyT.ToString() + " s at " + m_Pos.ToString() + " pitch " + m_Pitch);
		}
	}

	protected void FireChanged()
	{
		if (m_LastFired && vector.Distance(m_LastFired.m_Pos, m_Pos) < 0.0001 && m_LastFired.m_Yaw == m_Yaw && m_LastFired.m_Pitch == m_Pitch)
			return;
		DZS_CameraState s = new DZS_CameraState();
		s.m_Pos = m_Pos;
		s.m_Yaw = m_Yaw;
		s.m_Pitch = m_Pitch;
		m_LastFired = s;
		m_Kernel.GetEvents().FirePluginEvent("camera.changed", s);
	}
}

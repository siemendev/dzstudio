// Properties: a bundled plugin on the public interface only. It fills the properties part of the
// project bar, adds "Physik" and "Details …" to the object toolbar and the context menu, and submits every change as
// an operation "properties:edit".
class DZS_PropService : DZS_PropertiesApi
{
	DZS_PropertiesPlugin m_P;

	override void AddRow(DZS_PropertyRow row) { m_P.AddRow(row); }
}

class DZS_PropActions : DZS_ActionHandler
{
	DZS_PropertiesPlugin m_P;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn) { return m_P.ActionEnabled(actionId, reasonDe, reasonEn); }
	override void OnAction(string actionId) { m_P.OnAction(actionId); }
	override bool IsActionChecked(string actionId) { return actionId == "properties.physics" && m_P.PhysicsRunning(); }
}

class DZS_PropEsc : DZS_EscHandler
{
	DZS_PropertiesPlugin m_P;

	override bool IsEscActive() { return m_P.EscActive(); }
	override void OnEsc() { m_P.Esc(); }
}

class DZS_PhysBody : Managed
{
	string m_Id;
	vector m_StartPos;
	vector m_StartYpr;
	vector m_StartAxes[4];
	float m_Calm;
	float m_MaxSpeed;
}

[DZS_RegisterPlugin(DZS_PropertiesPlugin)]
class DZS_PropertiesPlugin : DZS_Plugin
{
	// field index: 0–2 X/Y/Z, 3–5 yaw/pitch/roll, 6 size, 7 height offset
	static const int F_SIZE = 6;
	static const int F_HEIGHT = 7;

	ref array<ref DZS_PropertyRow> m_Rows;
	protected ref DZS_PropService m_Service;
	protected ref DZS_PropActions m_Actions;
	protected ref DZS_PropEsc m_Esc;
	ref DZS_PropPanel m_Panel;
	protected bool m_World;

	// physics tool
	protected ref array<ref DZS_PhysBody> m_Bodies;
	protected float m_PhysTime;
	protected bool m_Physics;

	override string GetName() { return "properties"; }

	override void OnRegister()
	{
		m_Rows = new array<ref DZS_PropertyRow>();
		m_Bodies = new array<ref DZS_PhysBody>();
		m_Service = new DZS_PropService();
		m_Service.m_P = this;
		m_Kernel.SetService("properties", m_Service);
		m_Actions = new DZS_PropActions();
		m_Actions.m_P = this;
		m_Kernel.GetTypes().RegisterOperationType("properties", "properties:edit", 1);
		RegisterTexts();
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("properties", "Eigenschaften", "Properties");
		array<string> keys = {"Ctrl+T"};
		ui.RegisterAction("properties", "properties.details", "Details …", "Details …", keys, m_Actions);
		ui.SetWhen("properties.details", "selection");
		ui.SetGroup("properties.details", "properties");
		ui.RegisterAction("properties", "properties.physics", "Physik", "Physics", null, m_Actions);
		ui.SetListed("properties.physics", false);
		ui.SetChangesSelection("properties.physics");
		m_Panel = new DZS_PropPanel(this);
		ui.RegisterPanel("properties", "properties.panel", "Eigenschaften", "Properties", "right", "sliders-horizontal", m_Panel, 900);
		ui.SetPanelFixedHeight("properties.panel", 280);
		m_Esc = new DZS_PropEsc();
		m_Esc.m_P = this;
		m_Kernel.GetScreen().AddEscHandler(m_Esc, 38);
	}

	protected void RegisterTexts()
	{
		DZS_Texts.Add("properties.locked", "Gesperrt — %1 zum Entsperren", "Locked — %1 to unlock");
		DZS_Texts.Add("properties.copied", "Klassenname kopiert", "Class name copied");
		DZS_Texts.Add("properties.noPhysics", "Keine Objekte mit Physik ausgewählt", "No objects with physics selected");
		DZS_Texts.Add("properties.oneOnly", "Nur bei genau einem ausgewählten Objekt", "Only with exactly one selected object");
		DZS_Texts.Add("properties.handFull", "Erst das Objekt in der Hand absetzen", "Put down the object in the hand first");
		DZS_Texts.Add("properties.control", "Enthält Steuerzeichen — nur in der Datei änderbar", "Contains control characters — only editable in the file");
		DZS_Texts.Add("properties.plants", "Bei Pflanzen und Felsen ohne Wirkung", "No effect on plants and rocks");
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		if (!m_World)
			return;
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (sel)
		{
			sel.AddBarButton("properties.physics", "atom", "Physik", "Physics", 30);
			sel.AddBarButton("properties.details", "file-text", "Details …", "Details …", 80, true);
		}
	}

	override void OnSessionEnd()
	{
		m_Bodies.Clear();
		m_Physics = false;
		m_Kernel.SetGesture("properties.physics", false);
	}

	void AddRow(DZS_PropertyRow row)
	{
		m_Rows.Insert(row);
		if (m_Panel)
			m_Panel.AddPluginRow(row);
	}

	// ---- reading ----

	DZS_Instance Inst() { return m_Kernel.GetShown(); }
	DZS_Kernel Kernel() { return m_Kernel; }

	array<string> SelIds()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel)
			return new array<string>();
		array<string> ids = sel.GetSelection();
		return ids;
	}

	// The one selected object, or null.
	DZS_PropObj Single()
	{
		array<string> ids = SelIds();
		if (ids.Count() != 1)
			return null;
		DZS_PropObj o = DZS_PropObj.Read(Inst(), ids[0]);
		return o;
	}

	bool HandEmpty()
	{
		DZS_CatalogApi cat = DZS_CatalogApi.Get();
		if (!cat)
			return true;
		return cat.GetHand() == "";
	}

	string LockedTip()
	{
		string key = m_Kernel.GetUi().FirstBinding("placement.lock");
		if (key == "")
			key = "L";
		string t = DZS_Texts.Get("properties.locked");
		t.Replace("%1", key);
		return t;
	}

	// ---- the seven number values ----

	static int Decimals(int f)
	{
		if (f >= 3 && f <= 5)
			return 2;
		return 3;
	}

	static float StepOf(int f)
	{
		if (f == F_SIZE)
			return 0.01;
		if (f == F_HEIGHT)
			return 0.1;
		return 1;
	}

	static float Clamp(int f, float v)
	{
		if (f >= 3 && f <= 5)
			return Math.Clamp(v, -180, 180);
		if (f == F_SIZE)
			return Math.Clamp(v, DZS_Obj.SCALE_MIN, DZS_Obj.SCALE_MAX);
		if (f == F_HEIGHT)
			return Math.Clamp(v, -1000, 1000);
		return Math.Clamp(v, -100000, 100000);
	}

	static string FieldName(int f)
	{
		array<string> de = {"Position X", "Position Y", "Position Z", "Gier", "Nick", "Roll", "Größe", "Höhenversatz"};
		array<string> en = {"Position X", "Position Y", "Position Z", "Yaw", "Pitch", "Roll", "Size", "Height offset"};
		return DZS_Texts.Pick(de[f], en[f]);
	}

	static string OpWhat(int f, bool german)
	{
		array<string> de = {"Position", "Position", "Position", "Gier", "Nick", "Roll", "Größe", "Höhenversatz"};
		array<string> en = {"position", "position", "position", "yaw", "pitch", "roll", "size", "height offset"};
		if (german)
			return de[f];
		return en[f];
	}

	// Values of o with field f set to v (height offset: the origin rises by the difference to the measured value).
	DZS_PropObj WithValue(DZS_PropObj o, int f, float v, float measured)
	{
		DZS_PropObj n = o.Copy();
		if (f == F_HEIGHT)
			n.m_Pos = Vector(o.m_Pos[0], o.m_Pos[1] + (v - measured), o.m_Pos[2]);
		else
			n.SetValue(f, v);
		return n;
	}

	bool Measure(string id, out float h)
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel)
			return false;
		return sel.MeasureHeightOffset(id, h);
	}

	// One operation with the new values of o (only fields that change); false when nothing changes.
	bool Submit(DZS_PropObj before, DZS_PropObj after, string nameDe, string nameEn, string mergeKey = "")
	{
		DZS_Instance inst = Inst();
		if (!inst || !inst.Has(after.m_Id))
			return false;
		DZS_OpBuilder b = inst.BeginOperation("properties:edit", 1);
		string id = after.m_Id;
		string pos = DZS_Obj.Vec(after.m_Pos[0], after.m_Pos[1], after.m_Pos[2]);
		string ypr = DZS_Obj.Vec(after.m_Ypr[0], after.m_Ypr[1], after.m_Ypr[2]);
		string scale = DZS_Obj.Num(after.m_Scale);
		string curPos, curYpr, curScale, curPersist, curCustom;
		inst.GetField(id, DZS_Obj.POS, curPos);
		inst.GetField(id, DZS_Obj.YPR, curYpr);
		inst.GetField(id, DZS_Obj.SCALE, curScale);
		inst.GetField(id, DZS_Obj.PERSIST, curPersist);
		inst.GetField(id, DZS_Obj.CUSTOM, curCustom);
		if (VecChanged(before.m_Pos, after.m_Pos, 0.00001) && pos != curPos)
			b.SetField(id, DZS_Obj.POS, pos);
		if (VecChanged(before.m_Ypr, after.m_Ypr, 0.00001) && ypr != curYpr)
			b.SetField(id, DZS_Obj.YPR, ypr);
		if (Math.AbsFloat(before.m_Scale - after.m_Scale) > 0.0000001 && scale != curScale)
			b.SetField(id, DZS_Obj.SCALE, scale);
		if (before.m_Persist != after.m_Persist)
		{
			string pv = "0";
			if (after.m_Persist)
				pv = "1";
			if (pv != curPersist)
				b.SetField(id, DZS_Obj.PERSIST, pv);
		}
		if (before.m_Custom != after.m_Custom && after.m_Custom != curCustom)
			b.SetField(id, DZS_Obj.CUSTOM, after.m_Custom);
		if (b.IsEmpty())
			return false;
		DZS_SubmitResult r = b.Commit(nameDe, nameEn, mergeKey);
		DZS_Log.Info("properties: '" + nameDe + "' ok " + r.Ok() + " pos " + pos + " ypr " + ypr + " scale " + scale + " persist " + after.m_Persist + " custom '" + after.m_Custom + "'");
		return r.Ok();
	}

	static bool VecChanged(vector a, vector b, float eps)
	{
		return Math.AbsFloat(a[0] - b[0]) > eps || Math.AbsFloat(a[1] - b[1]) > eps || Math.AbsFloat(a[2] - b[2]) > eps;
	}

	// A value from the properties part: one operation, named "‹Spielname›: ‹Wert› ändern".
	bool ApplyValue(string id, int f, float v, string mergeKey = "")
	{
		DZS_PropObj o = DZS_PropObj.Read(Inst(), id);
		if (!o || o.m_Locked || m_Physics)
			return false;
		float measured = 0;
		if (f == F_HEIGHT && !Measure(id, measured))
			return false;
		DZS_PropObj n = WithValue(o, f, v, measured);
		string game = DZS_PropText.GameName(o.m_Name);
		string whatDe = OpWhat(f, true);
		string whatEn = OpWhat(f, false);
		return Submit(o, n, game + ": " + whatDe + " ändern", game + ": change " + whatEn, mergeKey);
	}

	// Intermediate value while dragging in a field: the object follows without an operation.
	void Preview(DZS_PropObj o, int f, float v, float measured)
	{
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (!scene)
			return;
		DZS_PropObj n = WithValue(o, f, v, measured);
		scene.SetView(o.m_Id, n.m_Pos, n.m_Ypr, n.m_Scale);
	}

	void ClearPreview(string id)
	{
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (scene)
			scene.ClearView(id);
	}

	bool ApplyCustom(string id, string custom, string whatDe, string whatEn)
	{
		DZS_PropObj o = DZS_PropObj.Read(Inst(), id);
		if (!o || o.m_Locked || o.m_Custom == custom)
			return false;
		DZS_PropObj n = o.Copy();
		n.m_Custom = custom;
		string game = DZS_PropText.GameName(o.m_Name);
		return Submit(o, n, game + ": " + whatDe + " ändern", game + ": change " + whatEn);
	}

	void CopyClassName(string name)
	{
		GetGame().CopyToClipboard(name);
		m_Kernel.GetScreen().ShowMessage("properties.copied", "success", DZS_Texts.Get("properties.copied"), 2.0);
		DZS_Log.Info("properties: class name copied '" + name + "'");
	}

	// ---- actions ----

	bool ActionEnabled(string id, out string reasonDe, out string reasonEn)
	{
		if (id == "properties.details")
		{
			if (SelIds().Count() != 1)
			{
				reasonDe = "Nur bei genau einem ausgewählten Objekt";
				reasonEn = "Only with exactly one selected object";
				return false;
			}
			if (!HandEmpty())
			{
				reasonDe = "Erst das Objekt in der Hand absetzen";
				reasonEn = "Put down the object in the hand first";
				return false;
			}
			return true;
		}
		if (id == "properties.physics")
		{
			if (m_Physics)
				return true;
			if (PhysicsCandidates().Count() == 0)
			{
				reasonDe = "Keine Objekte mit Physik ausgewählt";
				reasonEn = "No objects with physics selected";
				return false;
			}
			return true;
		}
		return true;
	}

	void OnAction(string id)
	{
		if (id == "properties.details")
		{
			OpenDetails();
			return;
		}
		if (id == "properties.physics")
		{
			if (m_Physics)
				EndPhysics(true);
			else
				StartPhysics();
		}
	}

	void OpenDetails()
	{
		string rd, re;
		if (!ActionEnabled("properties.details", rd, re))
			return;
		DZS_PropObj o = Single();
		if (!o)
			return;
		DZS_DetailsDialog d = new DZS_DetailsDialog(this, o.m_Id);
		d.Open();
		DZS_Log.Info("properties: details open " + o.m_Id + " " + o.m_Name);
	}

	// "Austauschen" from the dialog: this object alone, then the catalog picks the new class.
	void StartSwapFor(string id)
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel)
			return;
		array<string> one = {id};
		sel.SetSelection(one, id);
		sel.StartSwap();
	}

	// ---- physics ----

	bool PhysicsRunning() { return m_Physics; }

	// Selected, unlocked objects the game simulates as moving bodies: items (things one can pick up).
	array<string> PhysicsCandidates()
	{
		array<string> r = new array<string>();
		DZS_Instance inst = Inst();
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (!inst || !scene)
			return r;
		array<string> ids = SelIds();
		foreach (string id : ids)
		{
			DZS_PropObj o = DZS_PropObj.Read(inst, id);
			if (!o || o.m_Locked)
				continue;
			Object go = scene.GetObject(id);
			if (go && ItemBase.Cast(go) && !BaseBuildingBase.Cast(go))
				r.Insert(id);
		}
		return r;
	}

	protected void StartPhysics()
	{
		DZS_SceneApi scene = DZS_SceneApi.Get();
		DZS_Instance inst = Inst();
		if (!scene || !inst)
			return;
		m_Bodies.Clear();
		array<string> ids = PhysicsCandidates();
		foreach (string id : ids)
		{
			DZS_PropObj o = DZS_PropObj.Read(inst, id);
			if (!scene.StartPhysics(id))
				continue;
			DZS_PhysBody b = new DZS_PhysBody();
			b.m_Id = id;
			b.m_StartPos = o.m_Pos;
			b.m_StartYpr = o.m_Ypr;
			Object go = scene.GetObject(id);
			go.GetTransform(b.m_StartAxes);
			m_Bodies.Insert(b);
		}
		if (m_Bodies.Count() == 0)
		{
			DZS_Log.Info("properties: physics, no body could be simulated");
			return;
		}
		m_Physics = true;
		m_PhysTime = 0;
		m_Kernel.SetGesture("properties.physics", true);
		DZS_Log.Info("properties: physics on, " + m_Bodies.Count() + " body/bodies");
	}

	override void OnUpdate(float dt)
	{
		if (!m_Physics)
			return;
		m_PhysTime += dt;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		bool allCalm = true;
		foreach (DZS_PhysBody b : m_Bodies)
		{
			Object o = scene.GetObject(b.m_Id);
			if (!o)
				continue;
			float speed = GetVelocity(o).Length();
			if (speed > b.m_MaxSpeed)
				b.m_MaxSpeed = speed;
			if (speed < 0.01)
				b.m_Calm += dt;
			else
				b.m_Calm = 0;
			if (b.m_Calm < 0.5)
				allCalm = false;
		}
		if (allCalm || m_PhysTime >= 10)
		{
			DZS_Log.Info("properties: physics ends by itself after " + m_PhysTime + " s, calm " + allCalm);
			EndPhysics(true);
		}
	}

	// keep: the end positions become one operation; otherwise every object goes back to where it started.
	void EndPhysics(bool keep)
	{
		if (!m_Physics)
			return;
		m_Physics = false;
		m_Kernel.SetGesture("properties.physics", false);
		DZS_SceneApi scene = DZS_SceneApi.Get();
		DZS_Instance inst = Inst();
		array<string> moved = new array<string>();
		array<vector> movedPos = new array<vector>();
		array<vector> movedYpr = new array<vector>();
		foreach (DZS_PhysBody b : m_Bodies)
		{
			Object o = scene.GetObject(b.m_Id);
			if (o && keep)
			{
				vector p = o.GetPosition();
				vector y = o.GetOrientation();
				y = Vector(Norm180(y[0]), Norm180(y[1]), Norm180(y[2]));
				// Waking a resting body makes the game's physics twitch it by millimetres; that is not a move.
				vector m[4];
				o.GetTransform(m);
				float t0 = AxisAngle(m[0], b.m_StartAxes[0]);
				float t1 = AxisAngle(m[1], b.m_StartAxes[1]);
				float t2 = AxisAngle(m[2], b.m_StartAxes[2]);
				float turned = Math.Max(t0, Math.Max(t1, t2));
				float dist = vector.Distance(p, b.m_StartPos);
				bool posMoved = dist >= JITTER_M;
				bool rotMoved = turned >= JITTER_DEG;
				DZS_Log.Info("properties: physics body " + b.m_Id + " max speed " + b.m_MaxSpeed + " moved " + dist + " m, turned " + turned + " deg");
				if (posMoved || rotMoved)
				{
					moved.Insert(b.m_Id);
					movedPos.Insert(p);
					movedYpr.Insert(y);
				}
			}
			scene.EndPhysics(b.m_Id);
		}
		if (!keep || moved.Count() == 0 || !inst)
		{
			foreach (DZS_PhysBody back : m_Bodies)
				scene.ClearView(back.m_Id);
			m_Bodies.Clear();
			DZS_Log.Info("properties: physics off without a step (keep " + keep + ")");
			return;
		}
		DZS_OpBuilder op = inst.BeginOperation("properties:edit", 1);
		for (int i = 0; i < moved.Count(); i++)
		{
			vector mp = movedPos[i];
			vector my = movedYpr[i];
			op.SetField(moved[i], DZS_Obj.POS, DZS_Obj.Vec(mp[0], mp[1], mp[2]));
			op.SetField(moved[i], DZS_Obj.YPR, DZS_Obj.Vec(my[0], my[1], my[2]));
			DZS_Log.Info("properties: physics end " + moved[i] + " pos " + mp.ToString() + " ypr " + my.ToString());
		}
		string de, en;
		if (moved.Count() == 1)
		{
			string name;
			inst.GetField(moved[0], DZS_Obj.NAME, name);
			string game = DZS_PropText.GameName(name);
			de = game + " verschieben";
			en = "Move " + game;
		}
		else
		{
			de = moved.Count().ToString() + " Objekte verschieben";
			en = "Move " + moved.Count().ToString() + " objects";
		}
		DZS_SubmitResult r = op.Commit(de, en);
		foreach (DZS_PhysBody rest : m_Bodies)
		{
			if (moved.Find(rest.m_Id) < 0)
				scene.ClearView(rest.m_Id);
		}
		m_Bodies.Clear();
		DZS_Log.Info("properties: physics step '" + de + "' ok " + r.Ok());
	}

	static const float JITTER_M = 0.005;
	static const float JITTER_DEG = 0.5;

	// Angles near pitch ±90° are ambiguous (yaw and roll trade places), so turning is measured on the axes.
	static float AxisAngle(vector a, vector b)
	{
		float la = a.Length();
		float lb = b.Length();
		if (la < 0.000001 || lb < 0.000001)
			return 0;
		if (vector.Dot(a, b) < 0)
			return 180;
		vector x = Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
		float sn = Math.Clamp(x.Length() / (la * lb), 0, 1);
		return Math.Asin(sn) * Math.RAD2DEG;
	}

	static float Norm180(float a)
	{
		while (a > 180)
			a -= 360;
		while (a <= -180)
			a += 360;
		return a;
	}

	// ---- Esc: a running drag in a field or the physics simulation ----

	bool EscActive()
	{
		return m_Physics || (m_Panel && m_Panel.IsDragging());
	}

	void Esc()
	{
		if (m_Panel && m_Panel.IsDragging())
		{
			m_Panel.CancelDrag();
			return;
		}
		if (m_Physics)
			EndPhysics(false);
	}

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		if (m_Panel)
			m_Panel.MarkDirty();
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		if (m_Panel)
			m_Panel.MarkDirty();
	}

	override void OnPluginEvent(string topic, Managed payload)
	{
		if (topic == "selection.changed" && m_Panel)
			m_Panel.MarkDirty();
	}
}

// Shows the objects of the shown project in the world, driven only by kernel events; hidden objects are not
// drawn. Offers the service "scene" (DZS_SceneApi): which game object stands for which project object, model
// boxes and views during gestures.
class DZS_SceneService : DZS_SceneApi
{
	DZS_ScenePlugin m_P;

	override Object GetObject(string id) { return m_P.m_Objects.Get(id); }
	override string IdOf(Object o)
	{
		if (!o)
			return "";
		string id;
		if (m_P.m_Ids.Find(o, id))
			return id;
		return "";
	}
	override void CollectObjects(array<Object> into)
	{
		for (int i = 0; i < m_P.m_Objects.Count(); i++)
		{
			Object o = m_P.m_Objects.GetElement(i);
			if (o)
				into.Insert(o);
		}
	}
	override bool GetBox(string name, out vector boxMin, out vector boxMax) { return m_P.GetBox(name, boxMin, boxMax); }
	override void SetView(string id, vector pos, vector ypr, float scale) { m_P.SetView(id, pos, ypr, scale); }
	override void ClearView(string id) { m_P.ClearView(id); }
	override bool GetView(string id, out vector pos, out vector ypr, out float scale) { return m_P.GetView(id, pos, ypr, scale); }
	override bool StartPhysics(string id) { return m_P.StartPhysics(id); }
	override void EndPhysics(string id) { m_P.EndPhysics(id); }
	override Object MakeTemp(string name, vector pos, vector ypr, float scale)
	{
		DZS_WorldLines.Moved();
		return DZS_ScenePlugin.Make(name, pos, ypr, scale, false);
	}
	override void HideView(string id) { m_P.HideView(id); }
	override void Adopt(string id, Object o) { m_P.m_Adopt.Set(id, o); }
	override int PendingObjects() { return m_P.Pending(); }
	override void AddBoxHits(vector from, vector dir, float len, array<Object> exclude, array<ref DZS_RayHit> hits) { m_P.AddBoxHits(from, dir, len, exclude, hits); }
}

[DZS_RegisterPlugin(DZS_ScenePlugin)]
class DZS_ScenePlugin : DZS_Plugin
{
	ref map<string, Object> m_Objects;
	ref map<Object, string> m_Ids;
	protected ref map<string, vector> m_BoxMin;
	protected ref map<string, vector> m_BoxMax;
	protected ref DZS_SceneService m_Service;
	protected bool m_World;
	ref map<string, Object> m_Adopt = new map<string, Object>();
	protected ref array<string> m_Queue = new array<string>();
	protected int m_QNext;
	protected DZS_Instance m_QInst;
	protected int m_QFrames;
	protected float m_QSlowest;

	override string GetName() { return "scene"; }

	override void OnRegister()
	{
		m_Objects = new map<string, Object>();
		m_Ids = new map<Object, string>();
		m_BoxMin = new map<string, vector>();
		m_BoxMax = new map<string, vector>();
		m_Service = new DZS_SceneService();
		m_Service.m_P = this;
		m_Kernel.SetService("scene", m_Service);
	}

	override void OnSessionStart(string world)
	{
		string off;
		m_World = world != "" && !GetCLIParam("dzsSceneOff", off);
	}

	override void OnSessionEnd()
	{
		Clear();
	}

	protected void Clear()
	{
		for (int i = 0; i < m_Objects.Count(); i++)
		{
			Object o = m_Objects.GetElement(i);
			if (o)
				GetGame().ObjectDelete(o);
		}
		m_Objects.Clear();
		m_Ids.Clear();
		m_NoCbObjs.Clear();
		DZS_ViewPose.ClearAll();
	}

	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		Clear();
		m_Queue.Clear();
		m_QNext = 0;
		m_QInst = inst;
		m_QFrames = 0;
		m_QSlowest = 0;
		if (!inst || !m_World)
			return;
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			array<string> ids = inst.GetChildren(g);
			foreach (string id : ids)
				m_Queue.Insert(id);
		}
		StepQueue();
	}

	int Pending() { return m_Queue.Count() - m_QNext; }

	override void OnUpdate(float dt)
	{
		StepQueue();
	}

	// The objects of a newly shown project, about 15 ms per frame.
	protected void StepQueue()
	{
		if (m_QNext >= m_Queue.Count())
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst || inst != m_QInst)
		{
			m_Queue.Clear();
			m_QNext = 0;
			return;
		}
		int t0 = TickCount(0);
		while (m_QNext < m_Queue.Count())
		{
			Sync(inst, m_Queue[m_QNext]);
			m_QNext++;
			if (TickCount(0) - t0 >= 150000)
				break;
		}
		float ms = (TickCount(0) - t0) / 10000.0;
		m_QFrames++;
		if (ms > m_QSlowest)
			m_QSlowest = ms;
		if (m_QNext < m_Queue.Count())
			return;
		DZS_Log.Info("scene: " + m_Objects.Count() + " object(s) shown in " + m_QFrames + " frame(s), slowest " + DZS_Num.Fixed(m_QSlowest, 1) + " ms");
		m_Queue.Clear();
		m_QNext = 0;
	}

	override void OnOperationApplied(DZS_Instance inst, DZS_Operation op)
	{
		if (!m_World || inst != m_Kernel.GetShown())
			return;
		map<string, bool> poseOnly = new map<string, bool>();
		for (int p = 0; p < op.CountChanges(); p++)
		{
			DZS_Change pc = op.PeekChange(p);
			string key = pc.GetKey();
			bool pose = pc.GetKind() == DZS_ChangeKind.SET_FIELD && (key == DZS_Obj.POS || key == DZS_Obj.YPR || key == DZS_Obj.SCALE);
			bool had;
			if (!poseOnly.Find(pc.GetId(), had) || had)
				poseOnly.Set(pc.GetId(), pose);
		}
		map<string, bool> done = new map<string, bool>();
		for (int i = 0; i < op.CountChanges(); i++)
		{
			DZS_Change ch = op.PeekChange(i);
			string id = ch.GetId();
			if (done.Contains(id))
				continue;
			done.Set(id, true);
			string kind = inst.GetKind(id);
			if (kind == DZS_KIND_GROUP)
			{
				array<string> ids = inst.GetChildren(id);
				foreach (string cid : ids)
					Sync(inst, cid);
			}
			else if (!poseOnly.Get(id) || !Repose(inst, id))
				Sync(inst, id);
		}
	}

	// A new pose for an object that is already shown: moved in place instead of made again (moving thousands at once).
	protected bool Repose(DZS_Instance inst, string id)
	{
		Object o = m_Objects.Get(id);
		if (!o || inst.GetKind(id) != DZS_KIND_OBJECT)
			return false;
		if (dBodyIsSet(o) && dBodyIsDynamic(o))
			return false;
		string name, pos, ypr, scale;
		inst.GetField(id, DZS_Obj.NAME, name);
		if (DZS_Obj.IsModelPath(name))
			return false;
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		float s = 1;
		if (scale != "")
			s = scale.ToFloat();
		if (s == 0)
			s = 1;
		ForgetView(id);
		DZS_WorldLines.Moved();
		o.SetPosition(DZS_Obj.QuickVec(pos));
		o.SetOrientation(DZS_Obj.QuickVec(ypr));
		if (o.GetScale() != s)
			o.SetScale(s);
		o.Update();
		return true;
	}

	// Model box of a name in its own axes, unscaled; measured once on an object made only for that.
	bool GetBox(string name, out vector boxMin, out vector boxMax)
	{
		string key = name;
		key.ToLower();
		if (m_BoxMin.Contains(key))
		{
			boxMin = m_BoxMin.Get(key);
			boxMax = m_BoxMax.Get(key);
			return true;
		}
		Object o = Make(name, "0 0 0", "0 0 0", 1, false);
		if (!o)
			return false;
		bool ok = MeasureBox(o, boxMin, boxMax);
		GetGame().ObjectDelete(o);
		if (ok && (m_NoCollisionBox || DZS_Obj.IsModelPath(name)))
		{
			MeasureFull(name, boxMin, boxMax, !m_NoCollisionBox);
			m_NoCbKeys.Set(key, true);
		}
		if (!ok)
			return false;
		m_BoxMin.Set(key, boxMin);
		m_BoxMax.Set(key, boxMax);
		return true;
	}

	protected bool m_NoCollisionBox;
	protected ref map<string, bool> m_NoCbKeys = new map<string, bool>();
	protected ref map<Object, string> m_NoCbObjs = new map<Object, string>();

	// Without a collision box the clipping box can be far larger than the model (flag pole: 2.2 m below, bushes,
	// decals: metres high). The box is what rays along all three axes from both sides meet, on a copy high above
	// the camera; an object no ray meets (decals) keeps the clipping box flat at its origin height.
	// keep: the box so far is a collision box; the visible model is added to it (bushes: the collision box holds
	// only the stems).
	protected void MeasureFull(string name, inout vector boxMin, inout vector boxMax, bool keep)
	{
		vector at = GetGame().GetCurrentCameraPosition() + "0 600 0";
		Object o = Make(name, at, "0 0 0", 1, true);
		if (!o)
			return;
		o.Update();
		vector clip[2];
		o.ClippingInfo(clip);
		vector cmin = clip[0];
		vector cmax = clip[1];
		if (cmax[0] - cmin[0] < 0.001)
		{
			cmin = boxMin;
			cmax = boxMax;
		}
		vector lo = "1e9 1e9 1e9";
		vector hi = "-1e9 -1e9 -1e9";
		int hits = 0;
		array<int> kinds = {ObjIntersectView, ObjIntersectGeom, ObjIntersectFire};
		int used = -1;
		for (int k = 0; k < kinds.Count() && hits == 0; k++)
		{
			used = k;
			for (int ax = 0; ax < 3; ax++)
			{
				int a1 = (ax + 1) % 3;
				int a2 = (ax + 2) % 3;
				for (int side = 0; side < 2; side++)
				{
					for (int i = 0; i <= 16; i++)
					{
						for (int j = 0; j <= 16; j++)
						{
							vector l0;
							l0[a1] = cmin[a1] + (cmax[a1] - cmin[a1]) * (0.005 + 0.99 * i / 16.0);
							l0[a2] = cmin[a2] + (cmax[a2] - cmin[a2]) * (0.005 + 0.99 * j / 16.0);
							vector l1 = l0;
							if (side == 0)
							{
								l0[ax] = cmin[ax] - 1;
								l1[ax] = cmax[ax] + 1;
							}
							else
							{
								l0[ax] = cmax[ax] + 1;
								l1[ax] = cmin[ax] - 1;
							}
							vector cp, cd;
							int comp;
							set<Object> res = new set<Object>();
							if (!DayZPhysics.RaycastRV(o.ModelToWorld(l0), o.ModelToWorld(l1), cp, cd, comp, res, null, null, false, false, kinds[k], 0))
								continue;
							bool mine = false;
							foreach (Object r : res)
							{
								if (r == o || (r && r.GetType() == ""))
									mine = true;
							}
							if (!mine)
								continue;
							vector l = o.WorldToModel(cp);
							float v = l[ax];
							hits++;
							if (v < lo[ax])
								lo[ax] = v;
							if (v > hi[ax])
								hi[ax] = v;
						}
					}
				}
			}
		}
		GetGame().ObjectDelete(o);
		string lname = name;
		lname.ToLower();
		if (hits == 0)
		{
			if (keep)
			{
				DZS_Log.Info("scene: box of " + name + ", no ray meets the model: collision box kept " + boxMin + " " + boxMax);
				return;
			}
			boxMin = cmin;
			boxMax = cmax;
			if (lname.Contains("decal"))
			{
				float y0 = 0;
				if (cmin[1] > 0 || cmax[1] < 0)
					y0 = cmin[1];
				boxMin[1] = y0 - 0.01;
				boxMax[1] = y0 + 0.01;
			}
			string nmsg = "scene: box of " + name + " without collision box, no ray meets it: clipping " + cmin + " " + cmax;
			nmsg += " -> " + boxMin + " " + boxMax;
			DZS_Log.Info(nmsg);
			return;
		}
		vector was0 = boxMin;
		vector was1 = boxMax;
		for (int c = 0; c < 3; c++)
		{
			if (lo[c] > hi[c])
			{
				if (!keep)
				{
					boxMin[c] = cmin[c];
					boxMax[c] = cmax[c];
				}
				continue;
			}
			float l2 = lo[c];
			float h2 = hi[c];
			if (h2 - l2 < 0.02)
			{
				float mid = (l2 + h2) * 0.5;
				l2 = mid - 0.01;
				h2 = mid + 0.01;
			}
			if (keep)
			{
				l2 = Math.Min(l2, was0[c]);
				h2 = Math.Max(h2, was1[c]);
			}
			boxMin[c] = l2;
			boxMax[c] = h2;
		}
		string msg = "scene: box of " + name + ", visible model, " + hits + " ray hits (" + used + "), keep " + keep;
		msg += ": clipping " + cmin + " " + cmax;
		msg += ", before " + was0 + " " + was1;
		msg += " -> " + boxMin + " " + boxMax;
		DZS_Log.Info(msg);
	}

	bool IsNoCollisionBox(string name)
	{
		string key = name;
		key.ToLower();
		return m_NoCbKeys.Contains(key);
	}

	// Rays the game answers without a hit for objects lacking a collision box meet their frame instead:
	// the box padded like the frame, in the object's axes.
	void AddBoxHits(vector from, vector dir, float len, array<Object> exclude, array<ref DZS_RayHit> hits)
	{
		if (m_NoCbObjs.Count() == 0)
			return;
		array<Object> gone = new array<Object>();
		for (int i = 0; i < m_NoCbObjs.Count(); i++)
		{
			Object o = m_NoCbObjs.GetKey(i);
			if (!o || !m_Ids.Contains(o))
			{
				gone.Insert(o);
				continue;
			}
			string key = m_NoCbObjs.GetElement(i);
			if (exclude && exclude.Find(o) >= 0)
				continue;
			vector bmin = m_BoxMin.Get(key);
			vector bmax = m_BoxMax.Get(key);
			float s = Math.Max(o.GetScale(), 0.0001);
			vector pos = o.GetPosition();
			float fx = Math.Max(Math.AbsFloat(bmin[0]), Math.AbsFloat(bmax[0]));
			float fy = Math.Max(Math.AbsFloat(bmin[1]), Math.AbsFloat(bmax[1]));
			float fz = Math.Max(Math.AbsFloat(bmin[2]), Math.AbsFloat(bmax[2]));
			float rad = Math.Sqrt(fx * fx + fy * fy + fz * fz) * 1.1 * s + 0.2;
			vector toC = pos - from;
			float along = vector.Dot(toC, dir);
			if (along < -rad || along > len + rad)
				continue;
			if ((toC - dir * along).Length() > rad)
				continue;
			vector r, u, f;
			DZS_PlaceMath.Axes(o.GetOrientation(), r, u, f);
			vector d = from - pos;
			vector lf = Vector(vector.Dot(d, r) / s, vector.Dot(d, u) / s, vector.Dot(d, f) / s);
			vector ld = Vector(vector.Dot(dir, r) / s, vector.Dot(dir, u) / s, vector.Dot(dir, f) / s);
			float t0 = 0;
			float t1 = len;
			bool miss = false;
			for (int a = 0; a < 3 && !miss; a++)
			{
				float pad = Math.Max((bmax[a] - bmin[a]) * 0.04, 0.05 / s);
				float lo = bmin[a] - pad;
				float hi = bmax[a] + pad;
				if (Math.AbsFloat(ld[a]) < 0.0000001)
				{
					if (lf[a] < lo || lf[a] > hi)
						miss = true;
					continue;
				}
				float ta = (lo - lf[a]) / ld[a];
				float tb = (hi - lf[a]) / ld[a];
				if (ta > tb)
				{
					float tt = ta;
					ta = tb;
					tb = tt;
				}
				if (ta > t0)
					t0 = ta;
				if (tb < t1)
					t1 = tb;
				if (t0 > t1)
					miss = true;
			}
			if (miss)
				continue;
			bool had = false;
			foreach (DZS_RayHit h : hits)
			{
				if (h.m_Obj == o)
				{
					had = true;
					if (t0 < h.m_Dist)
					{
						h.m_Dist = t0;
						h.m_Pos = from + dir * t0;
					}
				}
			}
			if (had)
				continue;
			DZS_RayHit nh = new DZS_RayHit();
			nh.m_Obj = o;
			nh.m_Pos = from + dir * t0;
			nh.m_Normal = -dir;
			nh.m_Dist = t0;
			hits.Insert(nh);
		}
		foreach (Object g : gone)
			m_NoCbObjs.Remove(g);
	}

	protected bool MeasureBox(Object o, out vector boxMin, out vector boxMax)
	{
		m_NoCollisionBox = false;
		vector mm[2];
		if (o.GetCollisionBox(mm) && (mm[1][0] - mm[0][0]) > 0.001)
		{
			boxMin = mm[0];
			boxMax = mm[1];
			return true;
		}
		m_NoCollisionBox = true;
		vector cm[2];
		o.ClippingInfo(cm);
		if ((cm[1][0] - cm[0][0]) > 0.001)
		{
			boxMin = cm[0];
			boxMax = cm[1];
			return true;
		}
		boxMin = "-0.25 0 -0.25";
		boxMax = "0.25 0.5 0.25";
		return true;
	}

	// A local game object for a name; model paths through the file values of a server.
	static Object Make(string name, vector p, vector ypr, float s, bool physics, string custom = "")
	{
		Object o;
		if (DZS_Obj.IsModelPath(name))
		{
			vector rad = ypr * Math.DEG2RAD;
			vector fp;
			if (DZS_SpawnerP3D.ToFile(name, p, rad, s, fp))
				o = GetGame().CreateStaticObjectUsingP3D(name, fp, rad, s, true);
			return o;
		}
		if (DZS_ClassDir.Find(name) == "")
			return null;
		int flags = ECE_LOCAL | ECE_KEEPHEIGHT | ECE_NOLIFETIME;
		if (physics)
			flags = flags | ECE_CREATEPHYSICS;
		o = GetGame().CreateObjectEx(name, p, flags, RF_IGNORE);
		if (o)
		{
			o.SetPosition(p);
			o.SetOrientation(ypr);
			if (s != 1)
				o.SetScale(s);
			AsServerBuildsIt(o, s, custom);
		}
		return o;
	}

	// Flag poles: built up and the flag of customString hoisted the way the game does it when it reads a spawner
	// file (ObjectSpawnerHandler.SpawnObject). Building drops a construction kit beside the pole; it goes again.
	static void AsServerBuildsIt(Object o, float s, string custom)
	{
		StaticFlagPole pole = StaticFlagPole.Cast(o);
		if (!pole || !pole.GetConstruction())
			return;
		array<Object> before = new array<Object>();
		GetGame().GetObjectsAtPosition(o.GetPosition(), 3, before, null);
		ITEM_SpawnerObject item = new ITEM_SpawnerObject();
		item.name = o.GetType();
		item.scale = s;
		item.customString = custom;
		pole.OnSpawnByObjectSpawner(item);
		array<Object> after = new array<Object>();
		GetGame().GetObjectsAtPosition(o.GetPosition(), 3, after, null);
		foreach (Object a : after)
		{
			if (a != o && before.Find(a) < 0 && a.IsKindOf("TerritoryFlagKit"))
				GetGame().ObjectDelete(a);
		}
		int att = 0;
		if (pole.GetInventory())
			att = pole.GetInventory().AttachmentCount();
		DZS_Log.Info("scene: flag pole built, customString '" + custom + "', attachments " + att);
	}

	bool GetView(string id, out vector pos, out vector ypr, out float scale)
	{
		return DZS_ViewPose.Get(id, pos, ypr, scale);
	}

	protected void ForgetView(string id)
	{
		DZS_ViewPose.Clear(id);
	}

	// The drawn object becomes a body the game simulates; only items (pickable things) take part.
	bool StartPhysics(string id)
	{
		Object o = m_Objects.Get(id);
		if (!o || !ItemBase.Cast(o) || BaseBuildingBase.Cast(o))
			return false;
		o.CreateDynamicPhysics(PhxInteractionLayers.DYNAMICITEM);
		if (!dBodyIsSet(o))
			return false;
		dBodyDynamic(o, true);
		dBodyEnableGravity(o, true);
		dBodyActive(o, ActiveState.ACTIVE);
		return true;
	}

	void EndPhysics(string id)
	{
		Object o = m_Objects.Get(id);
		if (!o || !dBodyIsSet(o))
			return;
		dBodyActive(o, ActiveState.INACTIVE);
		dBodyDynamic(o, false);
	}

	void SetView(string id, vector pos, vector ypr, float scale)
	{
		DZS_WorldLines.Moved();
		DZS_ViewPose.Set(id, pos, ypr, scale);
		Object o = m_Objects.Get(id);
		if (!o)
			return;
		DZS_Instance inst = m_Kernel.GetShown();
		string name;
		if (inst)
			inst.GetField(id, DZS_Obj.NAME, name);
		if (DZS_Obj.IsModelPath(name))
		{
			m_Ids.Remove(o);
			GetGame().ObjectDelete(o);
			o = Make(name, pos, ypr, scale, true);
			m_Objects.Set(id, o);
			if (o)
			{
				m_Ids.Set(o, id);
				string nk = name;
				nk.ToLower();
				if (m_NoCbKeys.Contains(nk))
					m_NoCbObjs.Set(o, nk);
			}
			return;
		}
		vector mat[4];
		Math3D.YawPitchRollMatrix(ypr, mat);
		mat[0] = mat[0] * scale;
		mat[1] = mat[1] * scale;
		mat[2] = mat[2] * scale;
		mat[3] = pos;
		o.SetTransform(mat);
		if (o.GetScale() != scale)
			o.SetScale(scale);
		o.Update();
	}

	void HideView(string id)
	{
		DZS_WorldLines.Moved();
		Object o = m_Objects.Get(id);
		if (!o)
			return;
		m_Ids.Remove(o);
		m_Objects.Remove(id);
		GetGame().ObjectDelete(o);
	}

	void ClearView(string id)
	{
		DZS_WorldLines.Moved();
		DZS_Instance inst = m_Kernel.GetShown();
		if (inst)
			Sync(inst, id);
	}

	protected void Sync(DZS_Instance inst, string id)
	{
		ForgetView(id);
		Object old = m_Objects.Get(id);
		if (old)
		{
			m_Ids.Remove(old);
			GetGame().ObjectDelete(old);
		}
		m_Objects.Remove(id);
		if (!inst.Has(id) || inst.GetKind(id) != DZS_KIND_OBJECT)
			return;
		string g = inst.GetParent(id);
		if (inst.IsHidden(id) || inst.IsHidden(g))
			return;
		string name, pos, ypr, scale, custom;
		inst.GetField(id, DZS_Obj.CUSTOM, custom);
		inst.GetField(id, DZS_Obj.NAME, name);
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		if (name == "")
			return;
		float s = 1;
		if (scale != "")
			s = scale.ToFloat();
		if (s == 0)
			s = 1;
		Object o;
		if (m_Adopt.Find(id, o) && o)
			m_Adopt.Remove(id);
		else
			o = Make(name, DZS_Obj.QuickVec(pos), DZS_Obj.QuickVec(ypr), s, true, custom);
		if (o)
		{
			m_Objects.Set(id, o);
			m_Ids.Set(o, id);
			string key = name;
			key.ToLower();
			if (!m_BoxMin.Contains(key) && s == 1)
			{
				vector bmin, bmax;
				if (MeasureBox(o, bmin, bmax))
				{
					if (m_NoCollisionBox || DZS_Obj.IsModelPath(name))
					{
						MeasureFull(name, bmin, bmax, !m_NoCollisionBox);
						m_NoCbKeys.Set(key, true);
					}
					m_BoxMin.Set(key, bmin);
					m_BoxMax.Set(key, bmax);
				}
			}
			if (m_NoCbKeys.Contains(key))
				m_NoCbObjs.Set(o, key);
		}
		else
			DZS_Log.Warn("scene: cannot show " + name);
	}
}

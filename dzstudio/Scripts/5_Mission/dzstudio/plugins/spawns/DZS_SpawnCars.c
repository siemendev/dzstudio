// The cars of the Spawns layer: at the spawn points within about a kilometre of the camera (the nearest 60) a real,
// complete vehicle of the point's event — with the parts the base's cfgspawnabletypes.xml puts on it — frozen: no
// physics body, no simulation, no damage (with physics they roll, collide and sink). Points on the same spot get one
// car (a stack). Made in slices of a few ms per frame. Rays of the editor's tools pass through them (DZS_Scenery);
// they never belong to the project.
// A car no longer wanted is parked deep under the map and taken again for the next point of its class: placing a
// parked car costs a fraction of making one, so flying over the map makes only as many cars as are in view at once.
class DZS_SpawnCar : Managed
{
	// stack, class and place: a car whose key changes is placed anew
	string m_Key;
	string m_Stack;
	string m_Cls;
	vector m_Pos;
	float m_A;
	EntityAI m_Obj;
	vector m_Min;
	vector m_Max;
}

class DZS_SpawnCars : Managed
{
	static const float RANGE_IN = 1000;
	static const float RANGE_OUT = 1100;
	static const int MAX = 60;
	static const int POOL_MAX = 60;
	static const float BUDGET_MS = 4;
	static const vector PARK = "0 -3000 0";

	ref map<string, ref DZS_SpawnCar> m_Built = new map<string, ref DZS_SpawnCar>();
	protected ref array<ref DZS_SpawnCar> m_Todo = new array<ref DZS_SpawnCar>();
	// parked cars by class, and all of them oldest first
	protected ref map<string, ref array<EntityAI>> m_Pool = new map<string, ref array<EntityAI>>();
	protected ref array<EntityAI> m_Parked = new array<EntityAI>();
	protected ref map<string, vector> m_ClsMin = new map<string, vector>();
	protected ref map<string, vector> m_ClsMax = new map<string, vector>();
	protected ref map<string, bool> m_Failed = new map<string, bool>();
	// numbers for the log: cars placed and made, their time, the slowest slice
	protected int m_Placed;
	protected float m_PlaceMs;
	protected float m_PlaceSlowest;
	protected float m_StepSlowest;
	protected int m_StepFrames;
	protected bool m_Busy;
	protected int m_Made;
	protected int m_Deleted;

	bool Failed(string cls) { return m_Failed.Contains(cls); }

	DZS_SpawnCar Of(string stack) { return m_Built.Get(stack); }

	int Count() { return m_Built.Count(); }

	int Waiting() { return m_Todo.Count(); }

	string Numbers()
	{
		return m_Built.Count().ToString() + " shown, " + m_Parked.Count() + " parked, " + m_Made + " made, " + m_Deleted + " deleted";
	}

	// The cars wanted now, nearest first: what stands and is not wanted is parked, the rest comes in the next slices.
	void Plan(array<ref DZS_SpawnCar> want)
	{
		map<string, string> keys = new map<string, string>();
		foreach (DZS_SpawnCar w : want)
			keys.Set(w.m_Stack, w.m_Key);
		array<string> gone = new array<string>();
		foreach (string sk, DZS_SpawnCar c : m_Built)
		{
			if (keys.Get(sk) != c.m_Key)
				gone.Insert(sk);
		}
		foreach (string g : gone)
		{
			DZS_SpawnCar old = m_Built.Get(g);
			Park(old);
			m_Built.Remove(g);
		}
		m_Todo.Clear();
		foreach (DZS_SpawnCar n : want)
		{
			if (!m_Built.Contains(n.m_Stack) && !m_Failed.Contains(n.m_Cls))
				m_Todo.Insert(n);
		}
	}

	protected void Park(DZS_SpawnCar c)
	{
		if (!c.m_Obj)
			return;
		DZS_Scenery.Remove(c.m_Obj);
		c.m_Obj.SetPosition(PARK);
		array<EntityAI> list = m_Pool.Get(c.m_Cls);
		if (!list)
		{
			list = new array<EntityAI>();
			m_Pool.Set(c.m_Cls, list);
		}
		list.Insert(c.m_Obj);
		m_Parked.Insert(c.m_Obj);
		while (m_Parked.Count() > POOL_MAX)
		{
			EntityAI oldest = m_Parked[0];
			m_Parked.Remove(0);
			foreach (string cls, array<EntityAI> l : m_Pool)
			{
				int at = l.Find(oldest);
				if (at >= 0)
					l.Remove(at);
			}
			DeleteCar(oldest);
		}
	}

	// One slice: new cars, at least one per frame while any wait.
	void Step(DZS_SpawnsBase base)
	{
		if (m_Todo.Count() == 0)
		{
			if (m_Busy)
				Report();
			return;
		}
		m_Busy = true;
		int t0 = TickCount(0);
		int done = 0;
		while (m_Todo.Count() > 0 && (done == 0 || Ms(t0) < BUDGET_MS))
		{
			DZS_SpawnCar c = m_Todo[0];
			m_Todo.Remove(0);
			int t1 = TickCount(0);
			if (Place(c, base))
			{
				m_Built.Set(c.m_Stack, c);
				float ms = Ms(t1);
				m_Placed++;
				m_PlaceMs += ms;
				m_PlaceSlowest = Math.Max(m_PlaceSlowest, ms);
			}
			done++;
		}
		m_StepFrames++;
		m_StepSlowest = Math.Max(m_StepSlowest, Ms(t0));
	}

	protected void DeleteCar(EntityAI car)
	{
		if (!car)
			return;
		GetGame().ObjectDelete(car);
		m_Deleted++;
	}

	protected float Ms(int t0)
	{
		return (TickCount(0) - t0) / 10000.0;
	}

	protected void Report()
	{
		m_Busy = false;
		if (m_Placed > 0)
		{
			string avg = (m_PlaceMs / m_Placed).ToString();
			string line = "spawns: cars " + m_Placed + " placed in " + m_StepFrames + " frames, " + avg + " ms per car (slowest " + m_PlaceSlowest + ")";
			line += ", slowest slice " + m_StepSlowest + " ms; " + Numbers();
			DZS_Log.Info(line);
		}
		m_Placed = 0;
		m_PlaceMs = 0;
		m_PlaceSlowest = 0;
		m_StepSlowest = 0;
		m_StepFrames = 0;
	}

	// A parked car of the class when there is one, else a new one.
	protected bool Place(DZS_SpawnCar c, DZS_SpawnsBase base)
	{
		array<EntityAI> list = m_Pool.Get(c.m_Cls);
		if (list && list.Count() > 0)
		{
			EntityAI car = list[list.Count() - 1];
			list.Remove(list.Count() - 1);
			int at = m_Parked.Find(car);
			if (at >= 0)
				m_Parked.Remove(at);
			if (car)
			{
				car.SetPosition(c.m_Pos);
				car.SetOrientation(Vector(c.m_A, 0, 0));
				c.m_Obj = car;
				c.m_Min = m_ClsMin.Get(c.m_Cls);
				c.m_Max = m_ClsMax.Get(c.m_Cls);
				DZS_Scenery.Add(car);
				return true;
			}
		}
		return Make(c, base);
	}

	protected bool Make(DZS_SpawnCar c, DZS_SpawnsBase base)
	{
		int t0 = TickCount(0);
		Object o = GetGame().CreateObjectEx(c.m_Cls, c.m_Pos, ECE_NOLIFETIME | ECE_KEEPHEIGHT | ECE_LOCAL, RF_IGNORE);
		EntityAI car = EntityAI.Cast(o);
		if (!car)
		{
			if (o)
				GetGame().ObjectDelete(o);
			m_Failed.Set(c.m_Cls, true);
			DZS_Log.Warn("spawns: no car of class " + c.m_Cls);
			return false;
		}
		m_Made++;
		car.SetPosition(c.m_Pos);
		car.SetOrientation(Vector(c.m_A, 0, 0));
		float body = Ms(t0);
		array<string> parts = null;
		if (base)
			parts = base.Parts(c.m_Cls);
		if (parts)
		{
			foreach (string part : parts)
				car.GetInventory().CreateAttachment(part);
		}
		else
			car.OnDebugSpawn();
		vector mm[2];
		if (!car.GetCollisionBox(mm) || mm[1][0] - mm[0][0] < 0.01)
			car.ClippingInfo(mm);
		c.m_Min = mm[0];
		c.m_Max = mm[1];
		m_ClsMin.Set(c.m_Cls, mm[0]);
		m_ClsMax.Set(c.m_Cls, mm[1]);
		if (dBodyIsSet(car))
			dBodyDestroy(car);
		car.DisableSimulation(true);
		car.SetAllowDamage(false);
		c.m_Obj = car;
		DZS_Scenery.Add(car);
		float all = Ms(t0);
		if (all > 5)
			DZS_Log.Info("spawns: slow car " + c.m_Cls + " " + all + " ms (body " + body + ")");
		return true;
	}

	// The car whose box the ray meets first, with the distance along the ray; "" for none.
	string Under(vector from, vector dir, out float dist)
	{
		string best = "";
		dist = 1e9;
		foreach (string sk, DZS_SpawnCar c : m_Built)
		{
			if (!c.m_Obj)
				continue;
			float t;
			if (RayBox(from, dir, c, t) && t < dist)
			{
				dist = t;
				best = sk;
			}
		}
		return best;
	}

	protected static bool RayBox(vector from, vector dir, DZS_SpawnCar c, out float t)
	{
		vector lf = c.m_Obj.WorldToModel(from);
		vector ld = c.m_Obj.WorldToModel(from + dir) - lf;
		float t0 = 0;
		float t1 = 1e9;
		for (int k = 0; k < 3; k++)
		{
			if (Math.AbsFloat(ld[k]) < 0.000001)
			{
				if (lf[k] < c.m_Min[k] || lf[k] > c.m_Max[k])
					return false;
				continue;
			}
			float a = (c.m_Min[k] - lf[k]) / ld[k];
			float b = (c.m_Max[k] - lf[k]) / ld[k];
			if (a > b)
			{
				float sw = a;
				a = b;
				b = sw;
			}
			t0 = Math.Max(t0, a);
			t1 = Math.Min(t1, b);
			if (t0 > t1)
				return false;
		}
		t = t0;
		return true;
	}

	// The eight corners of a car's box in the world (bit 0 = x, 1 = y, 2 = z).
	static void Corners(DZS_SpawnCar c, array<vector> into)
	{
		into.Clear();
		for (int i = 0; i < 8; i++)
		{
			float x = c.m_Min[0];
			float y = c.m_Min[1];
			float z = c.m_Min[2];
			if ((i & 1) != 0)
				x = c.m_Max[0];
			if ((i & 2) != 0)
				y = c.m_Max[1];
			if ((i & 4) != 0)
				z = c.m_Max[2];
			into.Insert(c.m_Obj.ModelToWorld(Vector(x, y, z)));
		}
	}

	// Moves a car while its point is dragged (the plan places it anew afterwards).
	void Carry(string stack, vector pos)
	{
		DZS_SpawnCar c = m_Built.Get(stack);
		if (c && c.m_Obj)
			c.m_Obj.SetPosition(pos);
	}

	// Every standing car parked (another project, no project shown).
	void ParkAll()
	{
		foreach (string sk, DZS_SpawnCar c : m_Built)
			Park(c);
		m_Built.Clear();
		m_Todo.Clear();
	}

	// All gone (the session ends).
	void Clear()
	{
		ParkAll();
		foreach (EntityAI car : m_Parked)
			DeleteCar(car);
		m_Parked.Clear();
		m_Pool.Clear();
	}
}

// Working copy of one object during a gesture: values as the project holds them plus its model box.
class DZS_PlaceObj : Managed
{
	string m_Id;
	string m_Name;
	vector m_Pos;
	vector m_Ypr;
	float m_Scale = 1;
	vector m_BMin;
	vector m_BMax;
	vector m_R;
	vector m_U;
	vector m_F;
	bool m_Locked;

	void UpdateAxes()
	{
		vector r, u, f;
		DZS_PlaceMath.Axes(m_Ypr, r, u, f);
		m_R = r;
		m_U = u;
		m_F = f;
	}

	// A point given in the object's own (unscaled) model coordinates, in the world.
	vector Local(vector l)
	{
		return m_Pos + m_R * (l[0] * m_Scale) + m_U * (l[1] * m_Scale) + m_F * (l[2] * m_Scale);
	}

	vector Center()
	{
		return Local((m_BMin + m_BMax) * 0.5);
	}

	vector FootLocal()
	{
		return Vector((m_BMin[0] + m_BMax[0]) * 0.5, m_BMin[1], (m_BMin[2] + m_BMax[2]) * 0.5);
	}

	vector Foot()
	{
		return Local(FootLocal());
	}

	// Origin that puts the foot point at 'foot' with the current axes and size.
	vector OriginForFoot(vector foot)
	{
		return foot - (Local(FootLocal()) - m_Pos);
	}

	void Corners(array<vector> c)
	{
		CornersPadded(c, 0, 0);
	}

	// Corners of the frame: each side 4 % of its box edge further out, at least 0.05 m.
	void FrameCorners(array<vector> c)
	{
		CornersPadded(c, 0.04, 0.05);
	}

	protected void CornersPadded(array<vector> c, float share, float minWorld)
	{
		c.Clear();
		float s = Math.Max(m_Scale, 0.0001);
		vector lo = m_BMin;
		vector hi = m_BMax;
		for (int a = 0; a < 3; a++)
		{
			float pad = Math.Max((hi[a] - lo[a]) * share, minWorld / s);
			lo[a] = lo[a] - pad;
			hi[a] = hi[a] + pad;
		}
		for (int i = 0; i < 8; i++)
		{
			float x = lo[0];
			float y = lo[1];
			float z = lo[2];
			if (i & 1)
				x = hi[0];
			if (i & 2)
				y = hi[1];
			if (i & 4)
				z = hi[2];
			c.Insert(Local(Vector(x, y, z)));
		}
	}

	DZS_PlaceObj Copy()
	{
		DZS_PlaceObj o = new DZS_PlaceObj();
		o.m_Id = m_Id;
		o.m_Name = m_Name;
		o.m_Pos = m_Pos;
		o.m_Ypr = m_Ypr;
		o.m_Scale = m_Scale;
		o.m_BMin = m_BMin;
		o.m_BMax = m_BMax;
		o.m_R = m_R;
		o.m_U = m_U;
		o.m_F = m_F;
		o.m_Locked = m_Locked;
		return o;
	}

	// Rotates around an axis through a pivot (as part of a whole).
	void RotateAround(vector pivot, vector axis, float deg)
	{
		m_Pos = pivot + DZS_PlaceMath.Rot(m_Pos - pivot, axis, deg);
		m_R = DZS_PlaceMath.Rot(m_R, axis, deg);
		m_U = DZS_PlaceMath.Rot(m_U, axis, deg);
		m_F = DZS_PlaceMath.Rot(m_F, axis, deg);
		m_Ypr = DZS_PlaceMath.Angles(m_R, m_U, m_F);
	}

	void ScaleAround(vector pivot, float f)
	{
		m_Pos = pivot + (m_Pos - pivot) * f;
		m_Scale = m_Scale * f;
	}

	// For display only (frames, dots): values read quickly, not exact in the last bit; no exact float32 parsing.
	static DZS_PlaceObj FromStateQuick(DZS_Instance inst, string id)
	{
		DZS_PlaceObj o = new DZS_PlaceObj();
		o.m_Id = id;
		string name, pos, ypr, scale;
		inst.GetField(id, DZS_Obj.NAME, name);
		o.m_Name = name;
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		o.m_Pos = DZS_Obj.QuickVec(pos);
		o.m_Ypr = DZS_Obj.QuickVec(ypr);
		if (scale != "")
			o.m_Scale = scale.ToFloat();
		string g = inst.GetParent(id);
		bool lockedSelf = inst.IsLocked(id);
		bool lockedGroup = inst.IsLocked(g);
		o.m_Locked = lockedSelf || lockedGroup;
		vector bmin = "-0.25 0 -0.25";
		vector bmax = "0.25 0.5 0.25";
		DZS_SceneApi scene = DZS_SceneApi.Get();
		if (scene)
			scene.GetBox(o.m_Name, bmin, bmax);
		o.m_BMin = bmin;
		o.m_BMax = bmax;
		o.UpdateAxes();
		return o;
	}

	// For display only: the pose a plugin set in the kernel (display pose) instead of the state's.
	void UseViewPose()
	{
		vector p, y;
		float sc;
		if (!DZS_ViewPose.Get(m_Id, p, y, sc))
			return;
		m_Pos = p;
		m_Ypr = y;
		m_Scale = sc;
		UpdateAxes();
	}

	static DZS_PlaceObj FromState(DZS_Instance inst, string id)
	{
		DZS_PlaceObj o = new DZS_PlaceObj();
		o.m_Id = id;
		string name, pos, ypr, scale;
		inst.GetField(id, DZS_Obj.NAME, name);
		o.m_Name = name;
		inst.GetField(id, DZS_Obj.POS, pos);
		inst.GetField(id, DZS_Obj.YPR, ypr);
		inst.GetField(id, DZS_Obj.SCALE, scale);
		float a, b, c;
		if (DZS_Obj.ParseVec(pos, a, b, c))
			o.m_Pos = Vector(a, b, c);
		if (DZS_Obj.ParseVec(ypr, a, b, c))
			o.m_Ypr = Vector(a, b, c);
		float s;
		if (scale != "" && DZS_F32.FromText(scale, s))
			o.m_Scale = s;
		string g = inst.GetParent(id);
		bool lockedSelf = inst.IsLocked(id);
		bool lockedGroup = inst.IsLocked(g);
		o.m_Locked = lockedSelf || lockedGroup;
		DZS_SceneApi scene = DZS_SceneApi.Get();
		vector bmin = "-0.25 0 -0.25";
		vector bmax = "0.25 0.5 0.25";
		if (scene)
			scene.GetBox(o.m_Name, bmin, bmax);
		o.m_BMin = bmin;
		o.m_BMax = bmax;
		o.UpdateAxes();
		return o;
	}
}

// One hit of a ray: object (null = terrain), point, surface normal and distance from the start.
class DZS_RayHit : Managed
{
	Object m_Obj;
	vector m_Pos;
	vector m_Normal;
	float m_Dist;
}

// Objects shown as scenery only (the Spawns layer's cars): rays of the editor's tools pass through them. A plugin
// that draws them says when one is the front thing under the pointer, so the Objects layer shows no hover behind it.
class DZS_Scenery
{
	protected static ref array<Object> s_Objs;
	static bool s_PointerOn;

	static void Add(Object o)
	{
		if (!s_Objs)
			s_Objs = new array<Object>();
		if (o && s_Objs.Find(o) < 0)
			s_Objs.Insert(o);
	}

	static void Remove(Object o)
	{
		if (!s_Objs)
			return;
		int at = s_Objs.Find(o);
		if (at >= 0)
			s_Objs.Remove(at);
	}

	// The object or the vehicle it is part of (a wheel, a door).
	static bool Has(Object o)
	{
		if (!o || !s_Objs || s_Objs.Count() == 0)
			return false;
		if (s_Objs.Find(o) >= 0)
			return true;
		EntityAI e = EntityAI.Cast(o);
		if (!e)
			return false;
		EntityAI root = e.GetHierarchyRoot();
		return root && root != e && s_Objs.Find(root) >= 0;
	}
}

class DZS_Rays
{
	// Everything a ray meets within len, nearest first. One cast reports only its nearest contact, so the ray
	// is cast again from just behind each hit without that object.
	// RaycastRVProxy corrupted the heap of DayZ 1.30 while a world was loading; this path does not use it.
	static array<ref DZS_RayHit> Ray(vector from, vector dir, float len, array<Object> exclude, bool withTerrain = true)
	{
		array<ref DZS_RayHit> hits = new array<ref DZS_RayHit>();
		vector start = from;
		float done = 0;
		Object ignore = null;
		for (int n = 0; n < 12 && done < len; n++)
		{
			vector to = from + dir * len;
			vector cp, cd;
			int comp;
			set<Object> res = new set<Object>();
			if (!DayZPhysics.RaycastRV(start, to, cp, cd, comp, res, null, ignore, true, false, ObjIntersectView, 0))
				break;
			Object raw = null;
			if (res.Count() > 0)
				raw = res[0];
			Object o = Owner(raw, cp);
			float d = vector.Distance(from, cp);
			if (!o && d - done < 0.002 && n == 0 && cp[1] < GetGame().SurfaceY(cp[0], cp[2]) - 0.001)
			{
				// started under the terrain: the contact is the start itself
				start = cp + dir * 0.01;
				done = d;
				continue;
			}
			if (!o)
			{
				if (withTerrain)
				{
					DZS_RayHit t = new DZS_RayHit();
					t.m_Pos = cp;
					t.m_Normal = GetGame().SurfaceGetNormal(cp[0], cp[2]);
					t.m_Dist = d;
					hits.Insert(t);
				}
				break;
			}
			if (exclude.Find(o) < 0 && !DZS_Scenery.Has(o))
			{
				DZS_RayHit h = new DZS_RayHit();
				h.m_Obj = o;
				h.m_Pos = cp;
				h.m_Normal = NormalAt(cp, dir, ignore);
				h.m_Dist = d;
				hits.Insert(h);
			}
			ignore = raw;
			start = cp + dir * 0.01;
			done = d;
		}
		if (!withTerrain || hits.Count() == 0 || hits[hits.Count() - 1].m_Obj)
		{
			vector tp, td;
			int tcomp;
			set<Object> none = new set<Object>();
			if (withTerrain && DayZPhysics.RaycastRV(from, from + dir * len, tp, td, tcomp, none, null, null, false, true, ObjIntersectView, 0))
			{
				DZS_RayHit g = new DZS_RayHit();
				g.m_Pos = tp;
				g.m_Normal = GetGame().SurfaceGetNormal(tp[0], tp[2]);
				g.m_Dist = vector.Distance(from, tp);
				hits.Insert(g);
			}
		}
		Sort(hits);
		return hits;
	}

	// A ray can meet a proxy part of a building (tent canvas: class Object, no type, position in the owner's model
	// space); the owner is the object around the point whose box holds it, the smallest if several do.
	static Object Owner(Object o, vector p)
	{
		if (!o || o.GetType() != "" || vector.Distance(o.GetPosition(), p) < 50)
			return o;
		array<Object> near = new array<Object>();
		GetGame().GetObjectsAtPosition3D(p, 40, near, null);
		Object best = o;
		float bestVol = 1e30;
		foreach (Object c : near)
		{
			if (!c || c == o || c.GetType() == "")
				continue;
			vector mm[2];
			c.ClippingInfo(mm);
			vector l = c.WorldToModel(p);
			bool inside = true;
			for (int k = 0; k < 3; k++)
			{
				if (l[k] < mm[0][k] - 0.2 || l[k] > mm[1][k] + 0.2)
					inside = false;
			}
			if (!inside)
				continue;
			float vol = (mm[1][0] - mm[0][0]) * (mm[1][1] - mm[0][1]) * (mm[1][2] - mm[0][2]);
			if (vol < bestVol)
			{
				bestVol = vol;
				best = c;
			}
		}
		return best;
	}

	// The game reports no surface normal (contactDir is the ray's direction): two parallel rays 3 cm beside the hit
	// give the plane; facing the ray's origin. Straight up if a neighbour misses.
	static vector NormalAt(vector p, vector dir, Object ignore)
	{
		vector u = dir * "0 1 0";
		if (u.Length() < 0.01)
			u = dir * "1 0 0";
		u.Normalize();
		vector v = u * dir;
		v.Normalize();
		vector p1, p2;
		if (!Probe(p + u * 0.03, dir, ignore, p1) || !Probe(p + v * 0.03, dir, ignore, p2))
			return "0 1 0";
		vector n = (p1 - p) * (p2 - p);
		if (n.Length() < 0.000001)
			return "0 1 0";
		n.Normalize();
		if (vector.Dot(n, dir) > 0)
			n = -n;
		return n;
	}

	protected static bool Probe(vector p, vector dir, Object ignore, out vector hit)
	{
		vector cd;
		int comp;
		set<Object> res = new set<Object>();
		return DayZPhysics.RaycastRV(p - dir * 0.3, p + dir * 0.3, hit, cd, comp, res, null, ignore, true, false, ObjIntersectView, 0);
	}

	static void Sort(array<ref DZS_RayHit> hits)
	{
		for (int i = 1; i < hits.Count(); i++)
		{
			int j = i;
			while (j > 0 && hits[j - 1].m_Dist > hits[j].m_Dist)
			{
				hits.SwapItems(j, j - 1);
				j--;
			}
		}
	}

	// First surface (terrain or object) straight below a point, searched from 'above' m over it to 1000 m
	// under it; false if none.
	static bool Below(vector p, float above, array<Object> exclude, out vector hit, out vector normal)
	{
		vector from = p + Vector(0, above, 0);
		array<ref DZS_RayHit> hs = DZS_Rays.Ray(from, "0 -1 0", above + 1000, exclude);
		if (hs.Count() == 0)
			return false;
		hit = hs[0].m_Pos;
		normal = hs[0].m_Normal;
		return true;
	}
}

// Frames with depth. The game draws no lines into the world, so edges are drawn on a
// canvas over the scene and their depth is checked with rays from the camera. Covered pieces are left out (hidden
// objects: the free pieces dashed). Depth is checked only after something moved: camera, objects or the scene. Each
// frame sends at most BUDGET rays; the rest waits for the next frames. Results are kept per edge as intervals of the
// edge's own parameter, so their borders stay on the edge and move with it while a recheck is pending.
// Callers register frames with Frame(), then Flush() checks and draws them; the kernel calls Begin once per frame.
// Canvas lines cut to the scene area, so nothing is drawn over the bars.
class DZS_Clip
{
	static float s_X;
	static float s_Y;
	static float s_W = 100000;
	static float s_H = 100000;

	static void Set(float x, float y, float w, float h)
	{
		s_X = x;
		s_Y = y;
		s_W = w;
		s_H = h;
	}

	static bool Inside(float x, float y)
	{
		return x >= s_X && y >= s_Y && x <= s_X + s_W && y <= s_Y + s_H;
	}

	static void Line(CanvasWidget cv, float x1, float y1, float x2, float y2, float width, int color)
	{
		float dx = x2 - x1;
		float dy = y2 - y1;
		float t0 = 0;
		float t1 = 1;
		float xmax = s_X + s_W;
		float ymax = s_Y + s_H;
		if (!Edge(-dx, x1 - s_X, t0, t1))
			return;
		if (!Edge(dx, xmax - x1, t0, t1))
			return;
		if (!Edge(-dy, y1 - s_Y, t0, t1))
			return;
		if (!Edge(dy, ymax - y1, t0, t1))
			return;
		cv.DrawLine(x1 + dx * t0, y1 + dy * t0, x1 + dx * t1, y1 + dy * t1, width, color);
	}

	protected static bool Edge(float p, float q, inout float t0, inout float t1)
	{
		if (p == 0)
			return q >= 0;
		float r = q / p;
		if (p < 0)
		{
			if (r > t1)
				return false;
			if (r > t0)
				t0 = r;
		}
		else
		{
			if (r < t0)
				return false;
			if (r < t1)
				t1 = r;
		}
		return true;
	}
}

class DZS_EdgeCache : Managed
{
	ref array<float> m_T = new array<float>();
	ref array<bool> m_Vis = new array<bool>();
	bool m_Checked;
	int m_CamGen = -1;
	int m_SceneGen = -1;
	int m_GeomGen = -1;
	int m_LastFrame = -1;
}

class DZS_FrameCache : Managed
{
	string m_Key;
	ref array<vector> m_C = new array<vector>();
	ref array<ref DZS_EdgeCache> m_Edges = new array<ref DZS_EdgeCache>();
	int m_GeomGen;
	int m_Prio;
	float m_Width;
	int m_Color;
	float m_Dash;
	int m_UsedFrame = -1;
	// line sets: corner indices of each edge (null = the twelve edges of a box)
	ref array<int> m_A;
	ref array<int> m_B;
	// colour of covered and not yet checked pieces (0 = left out)
	int m_Cover;

	void DZS_FrameCache(int corners = 8, int edges = 12)
	{
		for (int i = 0; i < edges; i++)
			m_Edges.Insert(new DZS_EdgeCache());
		for (int k = 0; k < corners; k++)
			m_C.Insert("0 0 0");
	}

	int EA(int edge)
	{
		if (m_A)
			return m_A[edge];
		return DZS_WorldLines.s_EA[edge];
	}

	int EB(int edge)
	{
		if (m_B)
			return m_B[edge];
		return DZS_WorldLines.s_EB[edge];
	}
}

class DZS_WorldLines
{
	static vector s_Cam;
	static vector s_Fwd;
	static vector s_LastCam;
	static vector s_LastFwd;
	static int s_Rays;
	static int s_RaysLast;
	static int s_Frame;
	static int s_CamGen;
	static int s_SceneGen;
	static int s_MovedFrame = -1;
	static float s_Spacing = 8;
	static const float NEAR = 0.12;
	static const float TOL = 0.02;
	// Screen pixels between the first depth samples along an edge, and the precision at a cover's edge.
	static const float SAMPLE_PX = 8;
	static const float EXACT_PX = 2;
	static const int MAX_SAMPLES = 40;
	// Rays per frame for depth checks (ray budget).
	static const int BUDGET = 1500;
	// Frames to complete a full recheck of everything shown.
	static const int CATCH_UP = 4;
	// Priorities: needed in the frame it appears (hover, frame around a large selection), new, rechecked.
	static const int PRIO_NOW = 0;
	static const int PRIO_NEW = 1;

	static ref map<string, ref DZS_FrameCache> s_Cache;
	static ref array<DZS_FrameCache> s_Pending;
	static ref array<int> s_EA;
	static ref array<int> s_EB;

	static void Begin()
	{
		if (!s_Cache)
		{
			s_Cache = new map<string, ref DZS_FrameCache>();
			s_Pending = new array<DZS_FrameCache>();
			s_EA = new array<int>();
			s_EB = new array<int>();
			for (int i = 0; i < 8; i++)
			{
				for (int bit = 1; bit <= 4; bit *= 2)
				{
					if ((i & bit) == 0)
					{
						s_EA.Insert(i);
						s_EB.Insert(i | bit);
					}
				}
			}
		}
		s_Frame++;
		s_Cam = GetGame().GetCurrentCameraPosition();
		s_Fwd = GetGame().GetCurrentCameraDirection();
		if (vector.DistanceSq(s_Cam, s_LastCam) > 0.000001 || vector.DistanceSq(s_Fwd, s_LastFwd) > 0.0000001)
			s_CamGen++;
		s_LastCam = s_Cam;
		s_LastFwd = s_Fwd;
		s_RaysLast = s_Rays;
		s_Rays = 0;
		if (s_Frame % 60 == 0)
			Purge();
	}

	// Something in the scene moved, appeared or vanished: every frame is checked again.
	static void Moved()
	{
		if (s_MovedFrame == s_Frame)
			return;
		s_MovedFrame = s_Frame;
		s_SceneGen++;
	}

	static int RaysLastFrame()
	{
		return s_RaysLast;
	}

	static float Depth(vector p)
	{
		return vector.Dot(p - s_Cam, s_Fwd);
	}

	// True if nothing lies between the camera and the point (2 cm in front of it counts as free).
	// Objects the depth check looks through (test items at loot points).
	static ref array<Object> s_Pass;

	static void SetPass(array<Object> objs)
	{
		s_Pass = new array<Object>();
		foreach (Object o : objs)
			s_Pass.Insert(o);
		Moved();
	}

	static bool Free(vector p)
	{
		vector d = p - s_Cam;
		float len = d.Length();
		if (len <= TOL)
			return true;
		vector to = s_Cam + d * ((len - TOL) / len);
		vector cp, cd;
		int comp;
		set<Object> res = new set<Object>();
		vector start = s_Cam;
		Object ign = null;
		for (int pass = 0; pass < 4; pass++)
		{
			res.Clear();
			s_Rays++;
			if (!DayZPhysics.RaycastRV(start, to, cp, cd, comp, res, null, ign, false, false, ObjIntersectView, 0))
				return true;
			if (res.Count() == 0 || !s_Pass || s_Pass.Find(res.Get(0)) < 0)
				break;
			ign = res.Get(0);
			start = cp;
		}
		if (res.Count() > 0 || cp[1] < GetGame().SurfaceY(cp[0], cp[2]) + 0.03)
			return false;
		// a hit without an object above the terrain is the water surface (sea, pond): it covers nothing.
		// Below it RaycastRV reports every start point as a hit, so the rest of the way
		// is checked with the physics ray, which passes through water.
		vector hp, hn;
		float hf;
		s_Rays++;
		int layers = PhxInteractionLayers.BUILDING | PhxInteractionLayers.DOOR | PhxInteractionLayers.VEHICLE | PhxInteractionLayers.ROADWAY | PhxInteractionLayers.TERRAIN | PhxInteractionLayers.FENCE | PhxInteractionLayers.DYNAMICITEM;
		return !DayZPhysics.RayCastBullet(cp, to, layers, null, null, hp, hn, hf);
	}

	// Registers a box frame (eight corners, bit 0 = x, 1 = y, 2 = z) for this frame. dashPx > 0: hidden object.
	static void Frame(string key, array<vector> c, float width, int color, float dashPx = 0, int prio = 1)
	{
		if (!s_Cache)
			Begin();
		DZS_FrameCache f = s_Cache.Get(key);
		if (!f)
		{
			f = new DZS_FrameCache();
			f.m_Key = key;
			s_Cache.Set(key, f);
			for (int k = 0; k < 8; k++)
				f.m_C[k] = c[k];
		}
		else
		{
			bool same = true;
			for (int i = 0; i < 8; i++)
			{
				if (vector.DistanceSq(f.m_C[i], c[i]) > 0.000001)
				{
					same = false;
					f.m_C[i] = c[i];
				}
			}
			if (!same)
				f.m_GeomGen++;
		}
		f.m_Width = width;
		f.m_Color = color;
		f.m_Dash = dashPx;
		f.m_Prio = prio;
		if (f.m_UsedFrame != s_Frame)
		{
			f.m_UsedFrame = s_Frame;
			s_Pending.Insert(f);
		}
	}

	// Registers a set of lines (pairs of indices into pts) for this frame. Covered pieces, and pieces not checked
	// yet, are drawn in 'cover' (loot points: never left out). The number of points and lines of a key stays fixed.
	static void Lines(string key, array<vector> pts, array<int> pairs, float width, int color, int cover, int prio = 1)
	{
		if (!s_Cache)
			Begin();
		int ne = pairs.Count() / 2;
		DZS_FrameCache f = s_Cache.Get(key);
		if (f && (f.m_C.Count() != pts.Count() || f.m_Edges.Count() != ne))
		{
			s_Cache.Remove(key);
			f = null;
		}
		if (!f)
		{
			f = new DZS_FrameCache(pts.Count(), ne);
			f.m_Key = key;
			f.m_A = new array<int>();
			f.m_B = new array<int>();
			for (int e = 0; e < ne; e++)
			{
				f.m_A.Insert(pairs[2 * e]);
				f.m_B.Insert(pairs[2 * e + 1]);
			}
			s_Cache.Set(key, f);
			for (int k = 0; k < pts.Count(); k++)
				f.m_C[k] = pts[k];
		}
		else
		{
			bool same = true;
			for (int i = 0; i < pts.Count(); i++)
			{
				if (vector.DistanceSq(f.m_C[i], pts[i]) > 0.000001)
				{
					same = false;
					f.m_C[i] = pts[i];
				}
			}
			if (!same)
				f.m_GeomGen++;
		}
		f.m_Width = width;
		f.m_Color = color;
		f.m_Cover = cover;
		f.m_Dash = 0;
		f.m_Prio = prio;
		if (f.m_UsedFrame != s_Frame)
		{
			f.m_UsedFrame = s_Frame;
			s_Pending.Insert(f);
		}
	}

	// Checks what the budget allows (new frames first, then the oldest results) and draws the frames registered
	// since the last flush.
	static void Flush(CanvasWidget cv)
	{
		if (!s_Cache)
			return;
		Check();
		foreach (DZS_FrameCache f : s_Pending)
		{
			if (f)
				DrawFrame(cv, f);
		}
		s_Pending.Clear();
	}

	// Frames registered but not to be drawn this time (e.g. the overlay is hidden) are dropped from the list.
	static void Discard()
	{
		if (s_Pending)
			s_Pending.Clear();
	}

	static void Purge()
	{
		array<string> old = new array<string>();
		for (int i = 0; i < s_Cache.Count(); i++)
		{
			DZS_FrameCache f = s_Cache.GetElement(i);
			if (s_Frame - f.m_UsedFrame > 30)
				old.Insert(s_Cache.GetKey(i));
		}
		foreach (string k : old)
			s_Cache.Remove(k);
	}

	protected static bool Stale(DZS_FrameCache f, DZS_EdgeCache e)
	{
		return !e.m_Checked || e.m_CamGen != s_CamGen || e.m_SceneGen != s_SceneGen || e.m_GeomGen != f.m_GeomGen;
	}

	protected static void Check()
	{
		// spacing of the first samples: wider when everything shown would not fit into CATCH_UP frames at 8 px
		float need = 0;
		foreach (DZS_FrameCache f0 : s_Pending)
		{
			if (!f0)
				continue;
			for (int e0 = 0; e0 < f0.m_Edges.Count(); e0++)
			{
				if (Stale(f0, f0.m_Edges[e0]))
					need += ScreenSamples(f0, e0, SAMPLE_PX);
			}
		}
		float room = BUDGET * CATCH_UP * 0.6;
		s_Spacing = SAMPLE_PX;
		if (need > room)
			s_Spacing = SAMPLE_PX * need / room;
		// edges never checked: frames needed now (hover, frame around a large selection) at once, others while the
		// budget lasts
		foreach (DZS_FrameCache f1 : s_Pending)
		{
			if (!f1 || f1.m_Prio != PRIO_NOW)
				continue;
			for (int e1 = 0; e1 < f1.m_Edges.Count(); e1++)
			{
				if (!f1.m_Edges[e1].m_Checked)
					CheckEdge(f1, e1);
			}
		}
		foreach (DZS_FrameCache f2 : s_Pending)
		{
			if (!f2)
				continue;
			for (int e2 = 0; e2 < f2.m_Edges.Count(); e2++)
			{
				if (s_Rays >= BUDGET)
					return;
				if (!f2.m_Edges[e2].m_Checked)
					CheckEdge(f2, e2);
			}
		}
		// rechecks after movement, oldest first, a quarter of them per frame (all within CATCH_UP frames)
		array<int> order = new array<int>();
		array<int> of = new array<int>();
		array<int> oe = new array<int>();
		for (int fi = 0; fi < s_Pending.Count(); fi++)
		{
			DZS_FrameCache f3 = s_Pending[fi];
			if (!f3)
				continue;
			for (int e3 = 0; e3 < f3.m_Edges.Count(); e3++)
			{
				DZS_EdgeCache ec = f3.m_Edges[e3];
				if (!Stale(f3, ec) || of.Count() >= 65535)
					continue;
				int age = Math.Min(s_Frame - ec.m_LastFrame, 30000);
				order.Insert((30000 - age) * 65536 + of.Count());
				of.Insert(fi);
				oe.Insert(e3);
			}
		}
		order.Sort();
		float staleCount = order.Count();
		int allowed = Math.Ceil(staleCount / CATCH_UP);
		if (order.Count() > 0 && allowed < 1)
			allowed = 1;
		int done = 0;
		foreach (int code : order)
		{
			if (s_Rays >= BUDGET || done >= allowed)
				return;
			int low = code % 65536;
			CheckEdge(s_Pending[of[low]], oe[low]);
			done++;
		}
	}

	// The part of an edge in front of the camera as parameters t0..t1; false if all of it lies behind.
	protected static bool Clip(vector a, vector b, out float t0, out float t1)
	{
		float da = Depth(a);
		float db = Depth(b);
		t0 = 0;
		t1 = 1;
		if (da <= NEAR && db <= NEAR)
			return false;
		if (da < NEAR)
			t0 = (NEAR - da) / (db - da);
		else if (db < NEAR)
			t1 = (da - NEAR) / (da - db);
		return true;
	}

	protected static float ScreenSamples(DZS_FrameCache f, int edge, float spacing)
	{
		vector a = f.m_C[f.EA(edge)];
		vector b = f.m_C[f.EB(edge)];
		float t0, t1;
		if (!Clip(a, b, t0, t1))
			return 0;
		vector pa = a + (b - a) * t0;
		vector pb = a + (b - a) * t1;
		vector sa = GetGame().GetScreenPos(pa);
		vector sb = GetGame().GetScreenPos(pb);
		float lx = sb[0] - sa[0];
		float ly = sb[1] - sa[1];
		float len = Math.Sqrt(lx * lx + ly * ly);
		return Math.Clamp(Math.Ceil(len / spacing), 1, MAX_SAMPLES) + 1;
	}

	// World parameter of a screen-linear fraction between t0 and t1 (1/depth is linear on screen).
	protected static float ParamAt(vector a, vector b, float t0, float t1, float f)
	{
		vector pa = a + (b - a) * t0;
		vector pb = a + (b - a) * t1;
		float da = Depth(pa);
		float db = Depth(pb);
		float u = f;
		if (Math.AbsFloat(db - da) > 0.0001)
		{
			float z = 1 / ((1 - f) / da + f / db);
			u = (z - da) / (db - da);
		}
		return t0 + (t1 - t0) * u;
	}

	protected static void CheckEdge(DZS_FrameCache f, int edge)
	{
		DZS_EdgeCache e = f.m_Edges[edge];
		e.m_Checked = true;
		e.m_CamGen = s_CamGen;
		e.m_SceneGen = s_SceneGen;
		e.m_GeomGen = f.m_GeomGen;
		e.m_LastFrame = s_Frame;
		e.m_T.Clear();
		e.m_Vis.Clear();
		vector a = f.m_C[f.EA(edge)];
		vector b = f.m_C[f.EB(edge)];
		float t0, t1;
		if (!Clip(a, b, t0, t1))
			return;
		vector pa = a + (b - a) * t0;
		vector pb = a + (b - a) * t1;
		vector sa = GetGame().GetScreenPos(pa);
		vector sb = GetGame().GetScreenPos(pb);
		float lx = sb[0] - sa[0];
		float ly = sb[1] - sa[1];
		float len = Math.Sqrt(lx * lx + ly * ly);
		int n = Math.Clamp(Math.Ceil(len / s_Spacing), 1, MAX_SAMPLES);
		float exact = DZS_Scale.Px(EXACT_PX);
		float fPrev = 0;
		vector p0 = a + (b - a) * t0;
		bool vPrev = Free(p0);
		e.m_T.Insert(t0);
		e.m_Vis.Insert(vPrev);
		for (int i = 1; i <= n; i++)
		{
			float fr = i / n;
			float ti = ParamAt(a, b, t0, t1, fr);
			vector pi = a + (b - a) * ti;
			bool v = Free(pi);
			if (v != vPrev)
			{
				float lo = fPrev;
				float hi = fr;
				while ((hi - lo) * len > exact)
				{
					float mid = (lo + hi) * 0.5;
					float tm = ParamAt(a, b, t0, t1, mid);
					vector pm = a + (b - a) * tm;
					if (Free(pm) == vPrev)
						lo = mid;
					else
						hi = mid;
				}
				float cut = ParamAt(a, b, t0, t1, (lo + hi) * 0.5);
				e.m_T.Insert(cut);
				e.m_Vis.Insert(v);
				vPrev = v;
			}
			fPrev = fr;
		}
		e.m_T.Insert(t1);
		e.m_Vis.Insert(vPrev);
	}

	protected static void DrawFrame(CanvasWidget cv, DZS_FrameCache f)
	{
		for (int edge = 0; edge < f.m_Edges.Count(); edge++)
		{
			DZS_EdgeCache e = f.m_Edges[edge];
			vector a = f.m_C[f.EA(edge)];
			vector b = f.m_C[f.EB(edge)];
			float c0, c1;
			if (!Clip(a, b, c0, c1))
				continue;
			if (!e.m_Checked || e.m_T.Count() < 2)
			{
				if (f.m_Cover != 0)
				{
					vector ua = GetGame().GetScreenPos(a + (b - a) * c0);
					vector ub = GetGame().GetScreenPos(a + (b - a) * c1);
					DZS_Clip.Line(cv, ua[0], ua[1], ub[0], ub[1], f.m_Width, f.m_Cover);
				}
				continue;
			}
			// dash pattern from the end nearer to the camera
			float nt = 0;
			if (Depth(b) < Depth(a))
				nt = 1;
			nt = Math.Clamp(nt, c0, c1);
			vector pn = a + (b - a) * nt;
			vector sn = GetGame().GetScreenPos(pn);
			int last = e.m_T.Count() - 1;
			for (int k = 0; k < last; k++)
			{
				if (!e.m_Vis[k] && f.m_Cover == 0)
					continue;
				float u0 = Math.Max(e.m_T[k], c0);
				float u1 = Math.Min(e.m_T[k + 1], c1);
				if (u1 <= u0)
					continue;
				vector p = a + (b - a) * u0;
				vector q = a + (b - a) * u1;
				vector sp = GetGame().GetScreenPos(p);
				vector sq = GetGame().GetScreenPos(q);
				if (f.m_Dash > 0)
				{
					float dp = Math.Sqrt((sp[0] - sn[0]) * (sp[0] - sn[0]) + (sp[1] - sn[1]) * (sp[1] - sn[1]));
					float dq = Math.Sqrt((sq[0] - sn[0]) * (sq[0] - sn[0]) + (sq[1] - sn[1]) * (sq[1] - sn[1]));
					if (dq < dp)
						Dashed(cv, sq[0], sq[1], sp[0], sp[1], f.m_Width, f.m_Color, f.m_Dash, dq);
					else
						Dashed(cv, sp[0], sp[1], sq[0], sq[1], f.m_Width, f.m_Color, f.m_Dash, dp);
				}
				else if (e.m_Vis[k])
					DZS_Clip.Line(cv, sp[0], sp[1], sq[0], sq[1], f.m_Width, f.m_Color);
				else
					DZS_Clip.Line(cv, sp[0], sp[1], sq[0], sq[1], f.m_Width, f.m_Cover);
			}
		}
	}

	// Dashes and gaps of 'dash' pixels, phase counted from the start of the whole edge so runs join up.
	static void Dashed(CanvasWidget cv, float x1, float y1, float x2, float y2, float width, int color, float dash, float phase)
	{
		float dx = x2 - x1;
		float dy = y2 - y1;
		float len = Math.Sqrt(dx * dx + dy * dy);
		if (len < 0.5)
			return;
		float period = 2 * dash;
		float t = -Math.ModFloat(phase, period);
		while (t < len)
		{
			float s = Math.Max(t, 0);
			float e = Math.Min(t + dash, len);
			if (e > s)
				DZS_Clip.Line(cv, x1 + dx * s / len, y1 + dy * s / len, x1 + dx * e / len, y1 + dy * e / len, width, color);
			t += period;
		}
	}

	// A filled dot (or a ring 2 px wide) of the given diameter at a world point; nothing behind the camera.
	static int s_NoDots = -1;

	static void Dot(CanvasWidget cv, vector p, float diameter, int color, bool ring)
	{
		if (s_NoDots < 0)
		{
			string nd;
			s_NoDots = GetCLIParam("dzsNoDots", nd);
		}
		if (s_NoDots == 1)
			return;
		if (Depth(p) <= NEAR)
			return;
		vector s = GetGame().GetScreenPos(p);
		float r = diameter * 0.5;
		if (ring)
		{
			float w = Math.Max(1, DZS_Scale.Px(2));
			float rr = r - w * 0.5;
			int segs = 12;
			for (int i = 0; i < segs; i++)
			{
				float a0 = Math.PI2 * i / segs;
				float a1 = Math.PI2 * (i + 1) / segs;
				DZS_Clip.Line(cv, s[0] + Math.Cos(a0) * rr, s[1] + Math.Sin(a0) * rr, s[0] + Math.Cos(a1) * rr, s[1] + Math.Sin(a1) * rr, w, color);
			}
			return;
		}
		int strips = Math.Clamp(Math.Ceil(diameter / 6), 2, 4);
		float h = diameter / strips;
		for (int k = 0; k < strips; k++)
		{
			float yc = -r + h * (k + 0.5);
			float half = Math.Sqrt(Math.Max(0, r * r - yc * yc));
			if (half < 0.5)
				half = 0.5;
			DZS_Clip.Line(cv, s[0] - half, s[1] + yc, s[0] + half, s[1] + yc, h + 0.4, color);
		}
	}

	// Corners of the north/east/up box around all given corner sets.
	static void Around(array<vector> all, array<vector> into)
	{
		into.Clear();
		if (all.Count() == 0)
			return;
		vector lo = all[0];
		vector hi = all[0];
		foreach (vector v : all)
		{
			for (int k = 0; k < 3; k++)
			{
				if (v[k] < lo[k])
					lo[k] = v[k];
				if (v[k] > hi[k])
					hi[k] = v[k];
			}
		}
		for (int i = 0; i < 8; i++)
		{
			float x = lo[0];
			float y = lo[1];
			float z = lo[2];
			if ((i & 1) != 0)
				x = hi[0];
			if ((i & 2) != 0)
				y = hi[1];
			if ((i & 4) != 0)
				z = hi[2];
			into.Insert(Vector(x, y, z));
		}
	}
}

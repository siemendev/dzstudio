// Geometry of placing: object axes from yaw/pitch/roll and back, rotation of
// whole selections, rays sorted by distance, screen projection with clipping at the camera.
class DZS_PlaceMath
{
	// Signs between the game's angles and the internal ones (yaw clockwise from north, pitch nose up, roll):
	// measured once per session on a real object (Calibrate).
	static float s_SY = 1;
	static float s_SP = 1;
	static float s_SR = 1;

	// right, up, forward of an object with the game's yaw/pitch/roll (degrees).
	static void Axes(vector ypr, out vector right, out vector up, out vector fwd)
	{
		float y = ypr[0] * s_SY * Math.DEG2RAD;
		float p = ypr[1] * s_SP * Math.DEG2RAD;
		float r = ypr[2] * s_SR * Math.DEG2RAD;
		float cy = Math.Cos(y);
		float sy = Math.Sin(y);
		float cp = Math.Cos(p);
		float sp = Math.Sin(p);
		float cr = Math.Cos(r);
		float sr = Math.Sin(r);
		fwd = Vector(cp * sy, sp, cp * cy);
		right = Vector(cr * cy - sr * sp * sy, sr * cp, -cr * sy - sr * sp * cy);
		up = Vector(-sr * cy - cr * sp * sy, cr * cp, sr * sy - cr * sp * cy);
	}

	// The game's yaw/pitch/roll (degrees, −180…180) of three axes.
	static vector Angles(vector right, vector up, vector fwd)
	{
		float sp = Math.Clamp(fwd[1], -1, 1);
		float p = Math.Asin(sp);
		float y;
		float r;
		if (Math.AbsFloat(sp) > 0.99999)
		{
			r = 0;
			y = Math.Atan2(-right[2], right[0]);
		}
		else
		{
			y = Math.Atan2(fwd[0], fwd[2]);
			r = Math.Atan2(right[1], up[1]);
		}
		vector a = Vector(y * Math.RAD2DEG * s_SY, p * Math.RAD2DEG * s_SP, r * Math.RAD2DEG * s_SR);
		return Vector(Norm180(a[0]), Norm180(a[1]), Norm180(a[2]));
	}

	static float Norm180(float a)
	{
		while (a > 180)
			a -= 360;
		while (a <= -180)
			a += 360;
		if (Math.AbsFloat(a) < 0.0000001)
			a = 0;
		return a;
	}

	// Rodrigues rotation of v around the unit axis k by deg (right hand).
	static vector Rot(vector v, vector k, float deg)
	{
		float a = deg * Math.DEG2RAD;
		float c = Math.Cos(a);
		float s = Math.Sin(a);
		return v * c + (k * v) * s + k * (vector.Dot(k, v) * (1 - c));
	}

	// Internal angle for a change of the game's yaw by deg around the vertical.
	static float YawRot(float deg)
	{
		return deg * s_SY;
	}

	// Finds the signs that make Axes() agree with the game on a real object; logs the result.
	static void Calibrate()
	{
		Object o = GetGame().CreateObjectEx("Barrel_Blue", "100 0 100", ECE_LOCAL | ECE_NOLIFETIME, RF_IGNORE);
		if (!o)
		{
			DZS_Log.Warn("placement: calibration object missing");
			return;
		}
		vector test = "37 21 -13";
		o.SetOrientation(test);
		vector gf = o.GetDirection();
		vector gu = o.GetDirectionUp();
		float best = 1e9;
		float by = 1;
		float bp = 1;
		float br = 1;
		for (int i = 0; i < 8; i++)
		{
			s_SY = 1;
			s_SP = 1;
			s_SR = 1;
			if (i & 1)
				s_SY = -1;
			if (i & 2)
				s_SP = -1;
			if (i & 4)
				s_SR = -1;
			vector r, u, f;
			Axes(test, r, u, f);
			float err = vector.Distance(f, gf) + vector.Distance(u, gu);
			if (err < best)
			{
				best = err;
				by = s_SY;
				bp = s_SP;
				br = s_SR;
			}
		}
		s_SY = by;
		s_SP = bp;
		s_SR = br;
		vector rr, ru, rf;
		Axes(test, rr, ru, rf);
		vector back = Angles(rr, ru, rf);
		DZS_Log.Info("placement: angle signs " + s_SY + " " + s_SP + " " + s_SR + ", error " + best + ", game fwd " + gf.ToString() + " up " + gu.ToString() + ", back " + back.ToString());
		GetGame().ObjectDelete(o);
	}

	// ---- screen ----

	// Pinhole projection of the current camera. GetScreenPos gives wrong places for points far outside the
	// picture (frames and handles bent when the camera came close); it is used only for three points near the
	// middle to read the centre and the focal lengths whenever the camera or the screen changes.
	protected static vector s_VCam;
	protected static vector s_VFwd;
	protected static vector s_VRight = "1 0 0";
	protected static vector s_VUp = "0 1 0";
	protected static float s_VCx;
	protected static float s_VCy;
	protected static float s_VFx = 1;
	protected static float s_VFy = 1;
	protected static int s_VW = -1;
	protected static int s_VH = -1;

	protected static void UpdateView()
	{
		vector cam = GetGame().GetCurrentCameraPosition();
		vector fwd = GetGame().GetCurrentCameraDirection();
		int sw, sh;
		GetScreenSize(sw, sh);
		if (sw == s_VW && sh == s_VH && vector.DistanceSq(cam, s_VCam) < 0.0000000001 && vector.DistanceSq(fwd, s_VFwd) < 0.0000000001)
			return;
		s_VCam = cam;
		s_VFwd = fwd;
		s_VW = sw;
		s_VH = sh;
		float hl = Math.Sqrt(fwd[0] * fwd[0] + fwd[2] * fwd[2]);
		if (hl > 0.0001)
			s_VRight = Vector(fwd[2] / hl, 0, -fwd[0] / hl);
		vector r = s_VRight;
		s_VUp = Vector(fwd[1] * r[2] - fwd[2] * r[1], fwd[2] * r[0] - fwd[0] * r[2], fwd[0] * r[1] - fwd[1] * r[0]);
		vector mid = cam + fwd * 10;
		vector sc = GetGame().GetScreenPos(mid);
		vector sx = GetGame().GetScreenPos(mid + s_VRight);
		vector sy = GetGame().GetScreenPos(mid + s_VUp);
		s_VCx = sc[0];
		s_VCy = sc[1];
		s_VFx = (sx[0] - sc[0]) * 10;
		s_VFy = (sc[1] - sy[1]) * 10;
	}

	// Screen pixels (x, y) and depth along the view (z) of a world point; x and y are meaningful only in front.
	static vector Project(vector w)
	{
		UpdateView();
		vector rel = w - s_VCam;
		float z = vector.Dot(rel, s_VFwd);
		float zz = Math.Max(z, 0.0001);
		float x = s_VCx + s_VFx * vector.Dot(rel, s_VRight) / zz;
		float y = s_VCy - s_VFy * vector.Dot(rel, s_VUp) / zz;
		return Vector(x, y, z);
	}

	// Screen pixels of a world point; false if it lies behind the camera.
	static bool ToScreen(vector w, out float x, out float y)
	{
		vector s = Project(w);
		if (s[2] < 0.05)
			return false;
		x = s[0];
		y = s[1];
		return true;
	}

	// Screen offset of a world offset 'd' at the point 'at', without the perspective change along 'd'
	// (handles keep their shape however close the camera comes).
	static void Flat(vector at, vector d, out float dx, out float dy)
	{
		UpdateView();
		float z = Math.Max(vector.Dot(at - s_VCam, s_VFwd), 0.05);
		dx = s_VFx * vector.Dot(d, s_VRight) / z;
		dy = -s_VFy * vector.Dot(d, s_VUp) / z;
	}

	// A segment clipped at the plane 0.1 m in front of the camera; false if nothing is left.
	static bool Segment(vector a, vector b, out float x1, out float y1, out float x2, out float y2)
	{
		vector cam = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetCurrentCameraDirection();
		float da = vector.Dot(a - cam, dir) - 0.1;
		float db = vector.Dot(b - cam, dir) - 0.1;
		if (da <= 0 && db <= 0)
			return false;
		if (da <= 0)
			a = a + (b - a) * (da / (da - db) + 0.0001);
		else if (db <= 0)
			b = b + (a - b) * (db / (db - da) + 0.0001);
		vector sa = Project(a);
		vector sb = Project(b);
		x1 = sa[0];
		y1 = sa[1];
		x2 = sb[0];
		y2 = sb[1];
		return true;
	}

	// Distance of a point from a screen segment.
	static float DistToSegment(float px, float py, float ax, float ay, float bx, float by)
	{
		float vx = bx - ax;
		float vy = by - ay;
		float len2 = vx * vx + vy * vy;
		float t = 0;
		if (len2 > 0.0001)
			t = Math.Clamp(((px - ax) * vx + (py - ay) * vy) / len2, 0, 1);
		float qx = ax + vx * t - px;
		float qy = ay + vy * t - py;
		return Math.Sqrt(qx * qx + qy * qy);
	}

	// Even-odd rule for a closed polygon of screen points (x0 y0 x1 y1 …).
	static bool InPolygon(float px, float py, array<float> poly)
	{
		int n = poly.Count() / 2;
		bool inside = false;
		int j = n - 1;
		for (int i = 0; i < n; i++)
		{
			float xi = poly[2 * i];
			float yi = poly[2 * i + 1];
			float xj = poly[2 * j];
			float yj = poly[2 * j + 1];
			if ((yi > py) != (yj > py))
			{
				float xc = xi + (py - yi) * (xj - xi) / (yj - yi);
				if (px < xc)
					inside = !inside;
			}
			j = i;
		}
		return inside;
	}

	// Point on the ray from 'from' along 'dir' that lies on a plane; false if parallel or behind.
	static bool RayPlane(vector from, vector dir, vector planePoint, vector normal, out vector hit)
	{
		float d = vector.Dot(dir, normal);
		if (Math.AbsFloat(d) < 0.00001)
			return false;
		float t = vector.Dot(planePoint - from, normal) / d;
		if (t < 0)
			return false;
		hit = from + dir * t;
		return true;
	}
}

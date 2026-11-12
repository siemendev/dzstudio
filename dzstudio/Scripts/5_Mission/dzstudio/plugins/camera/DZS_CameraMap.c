// Map: the game's map widget over
// exactly the scene area, with the objects of the shown project as points, the marks of plugins and the camera
// mark on a canvas above it. The widget takes no input; zoom and drag are done here so the point under the pointer
// stays put. DayZ 1.30 draws a map widget only as a top-level widget made after the world has loaded, so it is made
// on the first opening and lies under the editor's root.
class DZS_CameraMapHit : DZS_HitListener
{
	DZS_CameraMap m_M;

	override bool OnHitWheel(DZS_Hit h, int delta) { return m_M.Wheel(delta); }
}

class DZS_CameraMapEsc : DZS_EscHandler
{
	DZS_CameraMap m_M;

	override bool IsEscActive() { return m_M.IsOpen(); }
	override void OnEsc() { m_M.Close("esc"); }
}

class DZS_CameraMap : Managed
{
	static const float DOUBLE_S = 0.45;
	static const float DOUBLE_PX = 6;
	static const float DRAG_PX = 4;
	static const float ZOOM_STEP = 1.25;
	static const float MIN_MPP = 0.25;
	// the widget shows no more than at scale 1 (DayZ 1.30)
	static const float MAX_SCALE = 1;
	static const float CAM_R = 11;
	static const float POINT_D = 6;

	protected DZS_CameraPlugin m_Cam;
	protected ref DZS_Hit m_Root;
	protected ref DZS_CameraMapHit m_Listener;
	protected ref DZS_CameraMapEsc m_Esc;
	protected MapWidget m_Map;
	protected CanvasWidget m_Canvas;
	protected ref DZS_PointerHint m_Hint;
	protected ref array<ref DZS_Hit> m_MarkHits;
	protected bool m_Open;
	protected float m_RX;
	protected float m_RY;
	protected float m_RW;
	protected float m_RH;
	// metres per screen pixel for a widget scale of 1, measured when the map opens
	protected float m_MppPerScale;
	protected float m_Scale = -1;
	protected ref array<vector> m_Points;
	protected ref array<string> m_PointIds = new array<string>();
	protected bool m_PointsDirty = true;
	protected int m_MarkStamp = -1;
	protected ref array<DZS_MapMark> m_Marks;
	protected ref array<DZS_MapMarkOwner> m_Owners;
	protected string m_DrawnSig;
	protected float m_T;
	protected bool m_LmbWas;
	protected bool m_Down;
	protected bool m_Dragging;
	protected int m_DownX;
	protected int m_DownY;
	protected vector m_DragWorld;
	protected float m_LastClickT = -10;
	// the map widget applies a new position only in the next frame: centring is corrected over a few frames
	protected int m_CenterFrames;
	protected vector m_CenterWant;
	protected float m_CenterErr;
	protected int m_LastClickX;
	protected int m_LastClickY;

	void DZS_CameraMap(DZS_CameraPlugin cam)
	{
		m_Cam = cam;
		DZS_Screen scr = cam.Kernel().GetScreen();
		m_Listener = new DZS_CameraMapHit();
		m_Listener.m_M = this;
		m_Root = new DZS_Hit(scr.GetMapLayer(), m_Listener, "camera.map");
		m_Root.m_Surface = true;
		m_Root.m_FocusKind = "";
		m_Root.m_W.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Canvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", m_Root.m_W));
		m_Hint = new DZS_PointerHint(scr.GetTipLayer());
		m_MarkHits = new array<ref DZS_Hit>();
		m_Points = new array<vector>();
		m_Marks = new array<DZS_MapMark>();
		m_Owners = new array<DZS_MapMarkOwner>();
		m_Esc = new DZS_CameraMapEsc();
		m_Esc.m_M = this;
		scr.AddEscHandler(m_Esc, 20);
		m_Root.m_W.Show(false);
	}

	void Destroy()
	{
		if (m_Open)
			Close("end");
		foreach (DZS_Hit h : m_MarkHits)
			h.Destroy();
		m_MarkHits.Clear();
		m_Root.Destroy();
		if (m_Map)
			m_Map.Unlink();
	}

	bool IsOpen() { return m_Open; }

	void PointsChanged() { m_PointsDirty = true; }

	void Toggle()
	{
		if (m_Open)
			Close("key");
		else
			Open();
	}

	void Open()
	{
		OpenAt(m_Cam.GetPosition());
	}

	// Opens the map centred on the world point p (the camera for M, a clicked point of the minimap).
	void OpenAt(vector p)
	{
		if (m_Open)
			return;
		DZS_Screen scr = m_Cam.Kernel().GetScreen();
		if (m_Cam.IsFreeLook())
			m_Cam.SetFreeLook(false);
		if (!m_Map)
			m_Map = MapWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_map.layout", null));
		m_Open = true;
		m_Root.m_W.Show(true);
		m_Map.Show(true);
		scr.SetKeyboardOwner("map", true);
		scr.SetHint("camera.map", 25, Help());
		PlaceRect();
		if (m_MppPerScale <= 0)
		{
			Measure();
			m_Scale = ClampScale(1.5 / Math.Max(0.000001, m_MppPerScale));
		}
		m_Map.SetScale(m_Scale);
		CenterOn(p);
		m_DrawnSig = "";
		m_Down = false;
		m_Dragging = false;
		m_LmbWas = (GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) != 0;
		Draw();
		vector cs = CamScreen();
		DZS_Log.Info("map open: scale " + m_Map.GetScale() + ", " + Mpp().ToString() + " m/px, rect " + m_RX + " " + m_RY + " " + m_RW + " " + m_RH + ", camera mark at " + cs[0] + " " + cs[1]);
	}

	void Close(string why)
	{
		if (!m_Open)
			return;
		m_Open = false;
		m_Scale = m_Map.GetScale();
		m_Root.m_W.Show(false);
		m_Map.Show(false);
		foreach (DZS_Hit h : m_MarkHits)
			h.m_W.Show(false);
		DZS_Screen scr = m_Cam.Kernel().GetScreen();
		scr.SetKeyboardOwner("map", false);
		scr.SetHint("camera.map", 25, "");
		m_Hint.Hide();
		DZS_Log.Info("map closed (" + why + ")");
	}

	protected string Help()
	{
		DZS_UiRegistry ui = m_Cam.Kernel().GetUi();
		return DZS_Texts.Fmt("camera.map.help", ui.FirstBinding("camera.jump"), ui.FirstBinding("camera.map"));
	}

	protected void PlaceRect()
	{
		DZS_Screen scr = m_Cam.Kernel().GetScreen();
		scr.GetSceneRect(m_RX, m_RY, m_RW, m_RH);
		m_Root.Place(m_RX, m_RY, m_RW, m_RH);
		DZS_W.Place(m_Map, m_RX, m_RY, m_RW, m_RH);
		DZS_W.Place(m_Canvas, 0, 0, m_RW, m_RH);
	}

	// ---- geometry ----

	protected void Measure()
	{
		float cx = m_RX + m_RW / 2;
		float cy = m_RY + m_RH / 2;
		vector a = m_Map.ScreenToMap(Vector(cx, cy, 0));
		vector b = m_Map.ScreenToMap(Vector(cx + 100, cy, 0));
		float mpp = Math.AbsFloat(b[0] - a[0]) / 100;
		float s = m_Map.GetScale();
		if (s > 0 && mpp > 0)
			m_MppPerScale = mpp / s;
	}

	float Mpp()
	{
		float cx = m_RX + m_RW / 2;
		float cy = m_RY + m_RH / 2;
		vector a = m_Map.ScreenToMap(Vector(cx, cy, 0));
		vector b = m_Map.ScreenToMap(Vector(cx + 100, cy, 0));
		return Math.AbsFloat(b[0] - a[0]) / 100;
	}

	protected float ClampScale(float s)
	{
		if (m_MppPerScale <= 0)
			return s;
		float lo = MIN_MPP / m_MppPerScale;
		return Math.Clamp(s, lo, Math.Max(lo, MAX_SCALE));
	}

	// The world point p in the middle of the map area.
	// The widget keeps the whole map inside its area; when that stops the point from reaching the middle, the map
	// zooms in a step and tries again.
	protected int m_CenterStuck;

	protected void CenterOn(vector p)
	{
		m_Map.SetMapPos(Vector(p[0], 0, p[2]));
		m_CenterWant = p;
		m_CenterFrames = 30;
		m_CenterStuck = 0;
	}

	protected void CenterStep()
	{
		vector c = m_Map.ScreenToMap(Vector(m_RX + m_RW / 2, m_RY + m_RH / 2, 0));
		vector mp = m_Map.GetMapPos();
		float ex = m_CenterWant[0] - c[0];
		float ez = m_CenterWant[2] - c[2];
		float tol = Mpp();
		if (Math.AbsFloat(ex) <= tol && Math.AbsFloat(ez) <= tol)
		{
			DZS_Log.Info("map centred: error " + ex + " " + ez + " m, scale " + m_Map.GetScale());
			m_CenterFrames = 0;
			return;
		}
		m_CenterFrames--;
		float err = Math.AbsFloat(ex) + Math.AbsFloat(ez);
		// the widget takes a new position a frame or two late: only an error that stays for three frames is the edge
		if (m_CenterFrames < 29 && err > m_CenterErr * 0.5)
			m_CenterStuck++;
		else
			m_CenterStuck = 0;
		if (m_CenterStuck >= 3)
		{
			m_CenterStuck = 0;
			m_Map.SetScale(m_Map.GetScale() * 0.8);
			DZS_Log.Info("map centre: error " + ex + " " + ez + " m at the edge of the map, zoom in to " + m_Map.GetScale());
		}
		m_CenterErr = err;
		m_Map.SetMapPos(Vector(mp[0] + ex, 0, mp[2] + ez));
	}

	// Keeps the world point w under the screen point (sx, sy).
	protected void Pin(vector w, float sx, float sy)
	{
		vector now = m_Map.ScreenToMap(Vector(sx, sy, 0));
		vector mp = m_Map.GetMapPos();
		m_Map.SetMapPos(Vector(mp[0] + w[0] - now[0], 0, mp[2] + w[2] - now[2]));
	}

	vector CamScreen()
	{
		return m_Map.MapToScreen(m_Cam.GetPosition());
	}

	vector WorldAt(float sx, float sy)
	{
		return m_Map.ScreenToMap(Vector(sx, sy, 0));
	}

	bool PointerOver()
	{
		if (!m_Open)
			return false;
		DZS_Screen scr = m_Cam.Kernel().GetScreen();
		if (scr.HasDialog() || scr.IsMenuOpen() || !scr.IsCursorVisible())
			return false;
		Widget under = GetWidgetUnderCursor();
		return under && DZS_W.IsInside(under, m_Root.m_W);
	}

	// ---- input ----

	bool Wheel(int delta)
	{
		if (!m_Open || delta == 0)
			return true;
		int mx, my;
		GetMousePos(mx, my);
		vector w = WorldAt(mx, my);
		float s = m_Map.GetScale();
		float ns = ClampScale(s * Math.Pow(ZOOM_STEP, -delta));
		m_Map.SetScale(ns);
		Pin(w, mx, my);
		vector back = m_Map.MapToScreen(w);
		DZS_Log.Info("map zoom " + delta + ": scale " + s + " -> " + ns + ", " + Mpp().ToString() + " m/px, point under pointer moved " + (back[0] - mx).ToString() + " " + (back[1] - my).ToString() + " px");
		return true;
	}

	protected void JumpTo(vector w, string how)
	{
		vector cam = m_Cam.GetPosition();
		float h = Math.Max(2, cam[1] - GetGame().SurfaceY(cam[0], cam[2]));
		float gy = GetGame().SurfaceY(w[0], w[2]);
		vector dest = Vector(w[0], gy + h, w[2]);
		m_Cam.SetPose(dest, m_Cam.GetYaw(), m_Cam.GetPitch(), true);
		vector p = m_Cam.GetPosition();
		float agl = p[1] - GetGame().SurfaceY(p[0], p[2]);
		DZS_Log.Info("map jump (" + how + ") to " + w[0] + " " + w[2] + " -> camera " + p.ToString() + " height " + agl + " (before " + h + "), visible width " + (Mpp() * m_RW).ToString() + " m");
		Close("jump");
	}

	protected void Pointer()
	{
		bool lmb = (GetMouseState(MouseState.LEFT) & MB_PRESSED_MASK) != 0;
		bool down = lmb && !m_LmbWas;
		bool up = !lmb && m_LmbWas;
		m_LmbWas = lmb;
		int mx, my;
		GetMousePos(mx, my);
		DZS_Screen scr = m_Cam.Kernel().GetScreen();
		if (down && PointerOver() && !scr.IsMouseSwallowed())
		{
			m_Down = true;
			m_Dragging = false;
			m_DownX = mx;
			m_DownY = my;
			m_DragWorld = WorldAt(mx, my);
		}
		if (m_Down && lmb)
		{
			float dx = mx - m_DownX;
			float dy = my - m_DownY;
			if (!m_Dragging && Math.Sqrt(dx * dx + dy * dy) >= DZS_Scale.Px(DRAG_PX))
				m_Dragging = true;
			if (m_Dragging)
				Pin(m_DragWorld, mx, my);
		}
		if (up && m_Down)
		{
			m_Down = false;
			if (m_Dragging)
			{
				m_Dragging = false;
				m_LastClickT = -10;
				DZS_Log.Info("map dragged");
				return;
			}
			DZS_SelectionApi sel = DZS_SelectionApi.Get();
			if (sel && sel.HasHand() && !sel.HandIsTemplate() && m_T - m_LastClickT > DOUBLE_S)
			{
				vector gw = WorldAt(mx, my);
				sel.PlaceHandAt(gw[0], gw[2]);
				m_DrawnSig = "";
			}
			float ddx = mx - m_LastClickX;
			float ddy = my - m_LastClickY;
			if (m_T - m_LastClickT <= DOUBLE_S && Math.Sqrt(ddx * ddx + ddy * ddy) <= DZS_Scale.Px(DOUBLE_PX))
			{
				m_LastClickT = -10;
				DoubleClick(mx, my);
				return;
			}
			m_LastClickT = m_T;
			m_LastClickX = mx;
			m_LastClickY = my;
		}
	}

	// Middle mouse button on the open map: like a double click there.
	void MiddleJump()
	{
		if (!m_Open || !PointerOver())
			return;
		int mx, my;
		GetMousePos(mx, my);
		DZS_Log.Info("map: middle button");
		DoubleClick(mx, my);
	}

	protected void DoubleClick(int mx, int my)
	{
		int top = MarkAt(mx, my);
		if (top >= 0)
		{
			DZS_MapMark m = m_Marks[top];
			DZS_Log.Info("map: double click on mark " + m.m_Id);
			DZS_MapMarkOwner o = m_Owners[top];
			Close("mark");
			if (o)
				o.OnMapMarkAction(m.m_Id);
			return;
		}
		JumpTo(WorldAt(mx, my), "double click");
	}

	// The topmost mark whose area and 3 px around it contain the screen point, or −1.
	protected int MarkAt(float sx, float sy)
	{
		for (int i = m_Marks.Count() - 1; i >= 0; i--)
		{
			DZS_MapMark m = m_Marks[i];
			vector s = m_Map.MapToScreen(Vector(m.m_X, 0, m.m_Z));
			float r = DZS_Scale.Px(m.m_Size) / 2 + DZS_Scale.Px(1) + DZS_Scale.Px(3);
			float dx = sx - s[0];
			float dy = sy - s[1];
			if (dx * dx + dy * dy <= r * r)
				return i;
		}
		return -1;
	}

	// ---- frame ----

	void Update(float dt)
	{
		m_T += dt;
		if (!m_Open)
			return;
		DZS_Screen scr = m_Cam.Kernel().GetScreen();
		if (scr.IsUiHidden())
		{
			Close("ui hidden");
			return;
		}
		float x, y, w, h;
		scr.GetSceneRect(x, y, w, h);
		if (x != m_RX || y != m_RY || w != m_RW || h != m_RH)
		{
			vector keep = WorldAt(m_RX + m_RW / 2, m_RY + m_RH / 2);
			PlaceRect();
			CenterOn(keep);
		}
		if (m_CenterFrames > 0)
			CenterStep();
		Pointer();
		if (m_PointsDirty)
			CollectPoints();
		if (m_MarkStamp != DZS_MapMarks.Stamp())
		{
			m_MarkStamp = DZS_MapMarks.Stamp();
			m_Marks.Clear();
			m_Owners.Clear();
			DZS_MapMarks.All(m_Marks, m_Owners);
			m_DrawnSig = "";
		}
		Draw();
		TemplateHint();
	}

	protected void CollectPoints()
	{
		m_PointsDirty = false;
		m_Points.Clear();
		m_PointIds.Clear();
		m_DrawnSig = "";
		DZS_Instance inst = m_Cam.Kernel().GetShown();
		if (!inst)
			return;
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			if (inst.IsHidden(g))
				continue;
			array<string> ids = inst.GetChildren(g);
			foreach (string id : ids)
			{
				if (inst.IsHidden(id))
					continue;
				string pos;
				if (inst.GetField(id, DZS_Obj.POS, pos))
				{
					m_Points.Insert(DZS_Obj.QuickVec(pos));
					m_PointIds.Insert(id);
				}
			}
		}
	}

	protected void Draw()
	{
		vector cam = m_Cam.GetPosition();
		vector mp = m_Map.GetMapPos();
		int hmx, hmy;
		GetMousePos(hmx, hmy);
		string sig = hmx.ToString() + "|" + hmy + "|" + m_Map.GetScale().ToString() + "|" + mp.ToString() + "|" + cam.ToString() + "|" + m_Cam.GetYaw().ToString() + "|" + m_RW + "|" + m_RH + "|" + DZS_ViewPose.Stamp();
		if (sig == m_DrawnSig)
			return;
		m_DrawnSig = sig;
		m_Canvas.Clear();
		DZS_Clip.Set(0, 0, m_RW, m_RH);
		DrawPoints();
		DrawMarks();
		DrawCamera();
		DrawHandDot();
		DZS_Clip.Set(0, 0, 100000, 100000);
	}

	protected void DrawPoints()
	{
		float d = DZS_Scale.Px(POINT_D);
		float o = d + Math.Max(2, DZS_Scale.Px(2));
		int fill = DZS_Color.Get("accent");
		int edge = DZS_Color.Get("well");
		vector vp, vy;
		float vs;
		for (int i = 0; i < m_Points.Count(); i++)
		{
			vector p = m_Points[i];
			if (DZS_ViewPose.Get(m_PointIds[i], vp, vy, vs))
				p = vp;
			vector s = m_Map.MapToScreen(p);
			float x = s[0] - m_RX;
			float y = s[1] - m_RY;
			if (x < -o || y < -o || x > m_RW + o || y > m_RH + o)
				continue;
			m_Canvas.DrawLine(x - o / 2, y, x + o / 2, y, o, edge);
			m_Canvas.DrawLine(x - d / 2, y, x + d / 2, y, d, fill);
		}
	}

	protected void DrawMarks()
	{
		int plate = DZS_Color.Get("plate");
		float edge = Math.Max(1, DZS_Scale.Px(1));
		int used = 0;
		for (int i = 0; i < m_Marks.Count(); i++)
		{
			DZS_MapMark m = m_Marks[i];
			vector s = m_Map.MapToScreen(Vector(m.m_X, 0, m.m_Z));
			float x = s[0] - m_RX;
			float y = s[1] - m_RY;
			float d = DZS_Scale.Px(m.m_Size);
			if (m.m_Diamond)
			{
				d *= 1.3;
				if (m.m_Ring)
				{
					DiamondEdge(x, y, d + 2 * edge, Math.Max(2, DZS_Scale.Px(2)) + 2 * edge, plate);
					DiamondEdge(x, y, d, Math.Max(2, DZS_Scale.Px(2)), m.m_Color);
				}
				else
				{
					Diamond(x, y, d + 3 * edge, plate);
					Diamond(x, y, d, m.m_Color);
				}
			}
			else if (m.m_Ring)
			{
				Ring(x, y, d + edge, Math.Max(2, DZS_Scale.Px(2)) + 2 * edge, plate);
				Ring(x, y, d, Math.Max(2, DZS_Scale.Px(2)), m.m_Color);
			}
			else
			{
				Disc(x, y, d + 2 * edge, plate);
				Disc(x, y, d, m.m_Color);
			}
			float r = d / 2 + edge + DZS_Scale.Px(3);
			if (x < -r || y < -r || x > m_RW + r || y > m_RH + r)
				continue;
			if (used >= m_MarkHits.Count())
			{
				DZS_Hit nh = new DZS_Hit(m_Root.m_W, null, "camera.map.mark");
				nh.m_FocusKind = "";
				m_MarkHits.Insert(nh);
			}
			DZS_Hit hit = m_MarkHits[used];
			hit.Place(x - r, y - r, 2 * r, 2 * r);
			hit.SetTip(m.m_Tip);
			hit.m_W.Show(true);
			used++;
		}
		for (int k = used; k < m_MarkHits.Count(); k++)
			m_MarkHits[k].m_W.Show(false);
	}

	// Template in the hand: no dot, a pointer hint instead.
	protected void TemplateHint()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel || !sel.HasHand() || !sel.HandIsTemplate() || !PointerOver())
		{
			m_Hint.Hide();
			return;
		}
		int mx, my;
		GetMousePos(mx, my);
		m_Hint.Show(DZS_Texts.Get("camera.map.template"), new array<string>(), mx, my, true);
	}

	// Object in the hand: a small accent dot at the pointer instead of a 3D preview.
	protected void DrawHandDot()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel || !sel.HasHand() || sel.HandIsTemplate() || !PointerOver())
			return;
		int mx, my;
		GetMousePos(mx, my);
		float d = DZS_Scale.Px(8);
		Disc(mx - m_RX, my - m_RY, d + 2 * Math.Max(1, DZS_Scale.Px(1)), DZS_Color.Get("well"));
		Disc(mx - m_RX, my - m_RY, d, DZS_Color.Get("accent"));
	}

	// Camera mark: dark disc in a white ring with a white centre and a dark nose in the viewing direction (stands out
	// on the light map as on water and forest); outside the shown area the same mark sits at the edge, its nose pointing
	// towards the camera.
	protected void DrawCamera()
	{
		vector s = CamScreen();
		float x = s[0] - m_RX;
		float y = s[1] - m_RY;
		float yaw = m_Cam.GetYaw() * Math.DEG2RAD;
		float dx = Math.Sin(yaw);
		float dy = -Math.Cos(yaw);
		float r = DZS_Scale.Px(CAM_R);
		if (x >= 0 && y >= 0 && x <= m_RW && y <= m_RH)
		{
			CameraMark(x, y, dx, dy);
			return;
		}
		float margin = r * 2.6;
		float cx = m_RW / 2;
		float cy = m_RH / 2;
		float vx = x - cx;
		float vy = y - cy;
		float tx = 1000000;
		float ty = 1000000;
		if (Math.AbsFloat(vx) > 0.001)
			tx = (cx - margin) / Math.AbsFloat(vx);
		if (Math.AbsFloat(vy) > 0.001)
			ty = (cy - margin) / Math.AbsFloat(vy);
		float t = Math.Min(tx, ty);
		float len = Math.Sqrt(vx * vx + vy * vy);
		CameraMark(cx + vx * t, cy + vy * t, vx / len, vy / len);
	}

	protected void CameraMark(float x, float y, float dx, float dy)
	{
		float r = DZS_Scale.Px(CAM_R);
		float e = Math.Max(2, DZS_Scale.Px(2));
		int white = ARGB(255, 255, 255, 255);
		int dark = ARGB(255, 0x09, 0x09, 0x0B);
		Triangle(x, y, dx, dy, r * 0.3, r * 2.2 + e * 2, r * 0.9 + e * 1.4, white);
		Triangle(x, y, dx, dy, r * 0.3, r * 2.2, r * 0.9, dark);
		Disc(x, y, 2 * (r + e), white);
		Disc(x, y, 2 * r, dark);
		Disc(x, y, r * 0.7, white);
	}

	protected void Triangle(float x, float y, float dx, float dy, float from, float to, float half, int color)
	{
		float px = -dy;
		float py = dx;
		float len = to - from;
		int strips = Math.Max(4, Math.Ceil(len / 2));
		float step = len / strips;
		for (int i = 0; i < strips; i++)
		{
			float a = from + step * (i + 0.5);
			float hw = half * (1 - (a - from) / len);
			float cx = x + dx * a;
			float cy = y + dy * a;
			m_Canvas.DrawLine(cx - px * hw, cy - py * hw, cx + px * hw, cy + py * hw, step + 0.6, color);
		}
	}

	protected void Disc(float x, float y, float d, int color)
	{
		float r = d / 2;
		int strips = Math.Clamp(Math.Ceil(d / 3), 2, 16);
		float h = d / strips;
		for (int k = 0; k < strips; k++)
		{
			float yc = -r + h * (k + 0.5);
			float half = Math.Sqrt(Math.Max(0, r * r - yc * yc));
			if (half < 0.5)
				half = 0.5;
			DZS_Clip.Line(m_Canvas, x - half, y + yc, x + half, y + yc, h + 0.4, color);
		}
	}

	protected void Diamond(float x, float y, float d, int color)
	{
		float r = d / 2;
		int strips = Math.Clamp(Math.Ceil(d / 2), 2, 24);
		float h = d / strips;
		for (int k = 0; k < strips; k++)
		{
			float yc = -r + h * (k + 0.5);
			float half = Math.Max(0.5, r - Math.AbsFloat(yc));
			DZS_Clip.Line(m_Canvas, x - half, y + yc, x + half, y + yc, h + 0.4, color);
		}
	}

	protected void DiamondEdge(float x, float y, float d, float w, int color)
	{
		float r = d / 2 - w / 2;
		DZS_Clip.Line(m_Canvas, x, y - r, x + r, y, w, color);
		DZS_Clip.Line(m_Canvas, x + r, y, x, y + r, w, color);
		DZS_Clip.Line(m_Canvas, x, y + r, x - r, y, w, color);
		DZS_Clip.Line(m_Canvas, x - r, y, x, y - r, w, color);
	}

	protected void Ring(float x, float y, float d, float w, int color)
	{
		float rr = d / 2 - w / 2;
		int segs = 20;
		for (int i = 0; i < segs; i++)
		{
			float a0 = Math.PI2 * i / segs;
			float a1 = Math.PI2 * (i + 1) / segs;
			DZS_Clip.Line(m_Canvas, x + Math.Cos(a0) * rr, y + Math.Sin(a0) * rr, x + Math.Cos(a1) * rr, y + Math.Sin(a1) * rr, w, color);
		}
	}

	// For tests: screen data of the map.
	string Describe()
	{
		if (!m_Map)
			return "map never opened";
		vector s = CamScreen();
		vector c = WorldAt(m_RX + m_RW / 2, m_RY + m_RH / 2);
		string r = "map open " + m_Open + " scale " + m_Map.GetScale() + " mpp " + Mpp() + " rect " + m_RX + " " + m_RY + " " + m_RW + " " + m_RH;
		r += " centre world " + c[0] + " " + c[2] + " camera mark " + s[0] + " " + s[1] + " points " + m_Points.Count() + " marks " + m_Marks.Count();
		foreach (DZS_MapMark m : m_Marks)
		{
			vector ms = m_Map.MapToScreen(Vector(m.m_X, 0, m.m_Z));
			r += " [" + m.m_Id + " " + ms[0] + " " + ms[1] + "]";
		}
		return r;
	}

	vector ScreenOf(float x, float z)
	{
		if (!m_Map)
			return "0 0 0";
		return m_Map.MapToScreen(Vector(x, 0, z));
	}
}

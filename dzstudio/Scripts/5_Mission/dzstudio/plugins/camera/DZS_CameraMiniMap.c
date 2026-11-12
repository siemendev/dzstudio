// Minimap: a small map in the bottom-left corner of the scene area, in every layer. It shows the camera with its
// field of view and the marks of the active layer (Objects: the project's objects, the selection larger; Spawns: every
// spawn point; Effects and Territories: their areas as circles). The map moves in steps: it centres on the camera
// again only when the camera has left the middle half, so most frames redraw nothing but the camera mark. The wheel
// zooms, a click opens the big map (M) centred on the clicked point. View › Minimap (and the button on it) hides it
// down to a small tab; state and zoom are kept in the settings.
// Like the big map, the game's map widget is a top-level widget made after the world has loaded, under the editor's
// root: the frame, canvases and buttons live in the map layer above it.
class DZS_MiniMapHit : DZS_HitListener
{
	DZS_CameraMiniMap m_M;

	override void OnHitClick(DZS_Hit h, int button) { m_M.Clicked(button); }
	override bool OnHitWheel(DZS_Hit h, int delta) { return m_M.Wheel(delta); }
	override void OnHitState(DZS_Hit h) { m_M.HoverChanged(); }
}

class DZS_CameraMiniMap : Managed
{
	static const float SIZE = 200;
	static const float MARGIN = 12;
	static const float MIN_SIZE = 96;
	static const int SPAN_DEFAULT = 800;
	static const float SPAN_MIN = 100;
	static const float ZOOM_STEP = 1.25;
	// marks redraw at most this often while only their content changes (a dragged object); a new map view redraws at once
	static const float MARKS_EVERY = 0.1;
	// the camera may move this share of the size away from the requested centre before the map centres again
	static const float RECENTRE = 0.25;
	static const string SETTING_ON = "camera.minimap";
	static const string SETTING_SPAN = "camera.minimapSpan";

	protected DZS_CameraPlugin m_Cam;
	protected Widget m_Frame;
	protected ref DZS_Border m_Border;
	protected ref DZS_Hit m_Hit;
	protected ref DZS_MiniMapHit m_Listener;
	protected CanvasWidget m_MarkCanvas;
	protected CanvasWidget m_CamCanvas;
	protected Widget m_ButtonPlate;
	protected ref DZS_Button m_Collapse;
	protected Widget m_Tab;
	protected Widget m_TabPlate;
	protected ref DZS_Border m_TabBorder;
	protected ref DZS_Button m_Expand;
	protected MapWidget m_Map;
	protected bool m_Shown;
	protected bool m_TabShown;
	protected float m_X;
	protected float m_Y;
	protected float m_S;
	protected float m_T;
	// view: world point at the centre of the frame, pixels per metre (z grows upwards, so m_PZ < 0)
	protected bool m_ViewOk;
	protected float m_OX;
	protected float m_OZ;
	protected float m_PX;
	protected float m_PZ;
	protected float m_MppPerScale;
	protected int m_Settle;
	protected vector m_Want;
	protected bool m_HasWant;
	protected float m_Span;
	protected string m_ViewSig;
	protected string m_MarksSig;
	protected string m_CamSig;
	protected float m_LastMarks = -10;
	// marks of the Objects layer
	protected ref array<vector> m_Points = new array<vector>();
	protected ref array<string> m_PointIds = new array<string>();
	protected bool m_PointsDirty = true;
	protected int m_PointsGen;
	// marks of another layer
	protected ref array<ref DZS_MapMark> m_LayerMarks = new array<ref DZS_MapMark>();
	protected string m_LayerSig = "-";
	protected string m_LayerId = "-";
	// cost: TickCount ticks (0.1 µs) of Update, worst frame and the marks redraws
	protected int m_StatFrames;
	protected int m_StatTicks;
	protected int m_StatWorst;
	protected int m_StatMarks;
	protected int m_StatMarksWorst;
	protected int m_StatMarksLines;
	protected int m_Lines;

	void DZS_CameraMiniMap(DZS_CameraPlugin cam)
	{
		m_Cam = cam;
		DZS_Screen scr = cam.Kernel().GetScreen();
		m_Listener = new DZS_MiniMapHit();
		m_Listener.m_M = this;
		m_Hit = new DZS_Hit(scr.GetMapLayer(), m_Listener, "camera.minimap");
		m_Hit.m_FocusKind = "";
		m_Frame = m_Hit.m_W;
		m_Frame.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_MarkCanvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", m_Frame));
		m_CamCanvas = CanvasWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_canvas.layout", m_Frame));
		m_Border = new DZS_Border(m_Frame, DZS_Color.Get("lineStrong"));
		m_ButtonPlate = DZS_W.Panel(m_Frame, DZS_Color.Get("plate"));
		m_Collapse = new DZS_Button(m_Frame, "ghost", "", "minus", 22);
		m_Collapse.SetTipAction(DZS_Texts.Get("camera.minimap.hide"), "camera.minimap");
		m_Collapse.m_OnClick.Insert(OnCollapse);
		m_Tab = DZS_W.Frame(scr.GetMapLayer());
		m_TabPlate = DZS_W.Panel(m_Tab, DZS_Color.Get("plate"));
		m_TabBorder = new DZS_Border(m_Tab, DZS_Color.Get("lineStrong"));
		m_Expand = new DZS_Button(m_Tab, "ghost", DZS_Texts.Get("camera.minimap.tab"), "maximize-2", 28);
		m_Expand.SetTipAction(DZS_Texts.Get("camera.minimap.show"), "camera.minimap");
		m_Expand.m_OnClick.Insert(OnExpand);
		m_Hit.SetTip(DZS_Texts.Get("camera.minimap.tip"));
		m_Span = cam.Kernel().GetSettings().GetNumber(SETTING_SPAN, SPAN_DEFAULT);
		m_Frame.Show(false);
		m_Tab.Show(false);
	}

	void Destroy()
	{
		m_Collapse.Destroy();
		m_Expand.Destroy();
		m_Hit.Destroy();
		if (m_Tab)
			m_Tab.Unlink();
		if (m_Map)
			m_Map.Unlink();
		m_Map = null;
	}

	bool IsOn()
	{
		return m_Cam.Kernel().GetSettings().GetBool(SETTING_ON, true);
	}

	bool IsShown() { return m_Shown; }

	void SetOn(bool on)
	{
		m_Cam.Kernel().GetSettings().Set(SETTING_ON, DZS_Json.Boolean(on));
		DZS_Log.Info("minimap " + DZS_Str.Bool(on));
	}

	protected void OnCollapse(DZS_Button b) { SetOn(false); }
	protected void OnExpand(DZS_Button b) { SetOn(true); }

	void PointsChanged() { m_PointsDirty = true; }

	void HoverChanged()
	{
		string c = "lineStrong";
		if (m_Hit.m_Hover)
			c = "capBorder";
		m_Border.SetColor(DZS_Color.Get(c));
	}

	// ---- input ----

	void Clicked(int button)
	{
		if (button != MouseState.LEFT || !m_ViewOk)
			return;
		int mx, my;
		GetMousePos(mx, my);
		vector w = WorldOf(mx - m_X, my - m_Y);
		DZS_CameraMap big = m_Cam.GetMap();
		DZS_Log.Info("minimap click at " + w[0] + " " + w[2]);
		if (big)
			big.OpenAt(w);
	}

	bool Wheel(int delta)
	{
		if (delta == 0)
			return true;
		float was = m_Span;
		m_Span = ClampSpan(m_Span * Math.Pow(ZOOM_STEP, -delta));
		SaveSpan();
		m_HasWant = false;
		DZS_Log.Info("minimap zoom " + delta + ": " + Math.Round(was) + " -> " + Math.Round(m_Span) + " m");
		return true;
	}

	protected float ClampSpan(float span)
	{
		float hi = 30000;
		if (m_MppPerScale > 0 && m_S > 0)
			hi = m_S * m_MppPerScale * DZS_CameraMap.MAX_SCALE;
		return Math.Clamp(span, SPAN_MIN, Math.Max(SPAN_MIN, hi));
	}

	protected void SaveSpan()
	{
		int sp = Math.Round(m_Span);
		m_Cam.Kernel().GetSettings().Set(SETTING_SPAN, DZS_Json.Int(sp));
	}

	// ---- frame ----

	void Update(float dt)
	{
		int t0 = TickCount(0);
		m_T += dt;
		bool want = Wanted();
		bool on = IsOn();
		ShowMap(want && on);
		ShowTab(want && !on);
		if (m_Shown)
			Frame();
		int t1 = TickCount(0);
		if (m_Shown)
		{
			m_StatFrames++;
			m_StatTicks += t1 - t0;
			m_StatWorst = Math.Max(m_StatWorst, t1 - t0);
		}
	}

	// Whether the place for the minimap is there: a world, the scene with its bars, no big map, no character.
	protected bool Wanted()
	{
		if (!m_Cam.IsStarted())
			return false;
		DZS_Screen scr = m_Cam.Kernel().GetScreen();
		if (scr.IsHome() || scr.IsUiHidden() || scr.IsGameControl())
			return false;
		if (!m_Cam.Kernel().GetShown())
			return false;
		DZS_CameraMap big = m_Cam.GetMap();
		if (big && big.IsOpen())
			return false;
		DZS_CameraChar ch = m_Cam.GetCharacter();
		if (ch && ch.IsIn())
			return false;
		float sx, sy, sw, sh;
		scr.GetSceneRect(sx, sy, sw, sh);
		float margin = DZS_Scale.Px(MARGIN);
		float s = DZS_Scale.Px(SIZE);
		s = Math.Min(s, Math.Min(sw * 0.3, sh * 0.4));
		s = Math.Round(s);
		if (s < DZS_Scale.Px(MIN_SIZE))
			return false;
		float x = Math.Round(sx + margin);
		float y = Math.Round(sy + sh - margin - s);
		if (x != m_X || y != m_Y || s != m_S)
		{
			m_X = x;
			m_Y = y;
			m_S = s;
			Layout();
		}
		return true;
	}

	protected void Layout()
	{
		m_Hit.Place(m_X, m_Y, m_S, m_S);
		DZS_W.Place(m_MarkCanvas, 0, 0, m_S, m_S);
		DZS_W.Place(m_CamCanvas, 0, 0, m_S, m_S);
		m_Border.Place(0, 0, m_S, m_S);
		float b = m_Collapse.GetHeight();
		float pad = DZS_Scale.Px(4);
		DZS_W.Place(m_ButtonPlate, m_S - b - pad, pad, b, b);
		m_Collapse.Place(m_S - b - pad, pad);
		float tw = m_Expand.GetWidth();
		float th = m_Expand.GetHeight();
		DZS_W.Place(m_Tab, m_X, m_Y + m_S - th, tw, th);
		DZS_W.Place(m_TabPlate, 0, 0, tw, th);
		m_TabBorder.Place(0, 0, tw, th);
		m_Expand.Place(0, 0);
		if (m_Map)
			DZS_W.Place(m_Map, m_X + 1, m_Y + 1, m_S - 2, m_S - 2);
		m_HasWant = false;
		m_ViewSig = "";
		m_CamSig = "";
	}

	protected void ShowMap(bool show)
	{
		if (show && !m_Map)
		{
			m_Map = MapWidget.Cast(DZS_W.Make("dzstudio/gui/layouts/dzs_map.layout", null));
			DZS_W.Place(m_Map, m_X + 1, m_Y + 1, m_S - 2, m_S - 2);
			m_HasWant = false;
			DZS_Log.Info("minimap made: " + m_S + " px at " + m_X + " " + m_Y);
		}
		if (show == m_Shown)
			return;
		m_Shown = show;
		m_Frame.Show(show);
		if (m_Map)
			m_Map.Show(show);
		m_HasWant = false;
		m_ViewSig = "";
		m_CamSig = "";
		m_MarksSig = "";
	}

	protected void ShowTab(bool show)
	{
		if (show == m_TabShown)
			return;
		m_TabShown = show;
		m_Tab.Show(show);
	}

	protected void Frame()
	{
		ReadView();
		if (!m_ViewOk)
			return;
		// zoom: the widget's scale for the wanted span, from the measured metres per pixel; the widget takes a new scale
		// a frame or two late, so measure and rescale only once it has settled; a position set with the new scale may
		// land with the old one, so centre again once it has settled
		bool rescale = false;
		if (m_Settle > 0)
		{
			m_Settle--;
			if (m_Settle == 0)
				m_HasWant = false;
		}
		else
		{
			float mpp = 1 / Math.Max(0.000001, m_PX);
			float scale = m_Map.GetScale();
			if (scale > 0)
				m_MppPerScale = mpp / scale;
			float wantScale = Math.Min(DZS_CameraMap.MAX_SCALE, ClampSpan(m_Span) / m_S / Math.Max(0.000001, m_MppPerScale));
			rescale = Math.AbsFloat(wantScale - scale) > wantScale * 0.01;
			if (rescale)
			{
				m_Map.SetScale(wantScale);
				m_Settle = 3;
			}
		}
		vector cam = m_Cam.GetPosition();
		bool recentre = rescale || !m_HasWant;
		if (!recentre)
		{
			float dx = (cam[0] - m_Want[0]) * m_PX;
			float dz = (cam[2] - m_Want[2]) * m_PX;
			float lim = m_S * RECENTRE;
			recentre = Math.AbsFloat(dx) > lim || Math.AbsFloat(dz) > lim;
		}
		if (recentre)
		{
			m_Want = Vector(cam[0], 0, cam[2]);
			m_HasWant = true;
			m_Map.SetMapPos(m_Want);
		}
		DrawMarks();
		DrawCamera();
	}

	// The map's view from three points: the centre and 100 px to the right and down.
	protected void ReadView()
	{
		float cx = m_X + m_S / 2;
		float cy = m_Y + m_S / 2;
		vector o = m_Map.ScreenToMap(Vector(cx, cy, 0));
		vector a = m_Map.ScreenToMap(Vector(cx + 100, cy, 0));
		vector b = m_Map.ScreenToMap(Vector(cx, cy + 100, 0));
		float ax = a[0] - o[0];
		float bz = b[2] - o[2];
		m_ViewOk = Math.AbsFloat(ax) > 0.0001 && Math.AbsFloat(bz) > 0.0001;
		if (!m_ViewOk)
			return;
		m_OX = o[0];
		m_OZ = o[2];
		m_PX = 100 / ax;
		m_PZ = 100 / bz;
	}

	protected float LocalX(float wx) { return m_S / 2 + (wx - m_OX) * m_PX; }
	protected float LocalY(float wz) { return m_S / 2 + (wz - m_OZ) * m_PZ; }

	protected vector WorldOf(float lx, float ly)
	{
		float wx = m_OX + (lx - m_S / 2) / m_PX;
		float wz = m_OZ + (ly - m_S / 2) / m_PZ;
		return Vector(wx, GetGame().SurfaceY(wx, wz), wz);
	}

	// ---- marks ----

	protected void DrawMarks()
	{
		string view = Math.Round(m_OX * 10).ToString() + "|" + Math.Round(m_OZ * 10).ToString() + "|" + Math.Round(m_PX * 100000).ToString() + "|" + m_S;
		string content = ContentSig();
		bool viewChanged = view != m_ViewSig;
		bool contentChanged = content != m_MarksSig;
		if (!viewChanged && !contentChanged)
			return;
		if (!viewChanged && m_T - m_LastMarks < MARKS_EVERY)
			return;
		m_ViewSig = view;
		m_MarksSig = content;
		m_LastMarks = m_T;
		int t0 = TickCount(0);
		m_Lines = 0;
		float cx = DZS_Clip.s_X;
		float cy = DZS_Clip.s_Y;
		float cw = DZS_Clip.s_W;
		float ch = DZS_Clip.s_H;
		DZS_Clip.Set(1, 1, m_S - 2, m_S - 2);
		m_MarkCanvas.Clear();
		if (m_LayerId == "")
			DrawObjects();
		else
			DrawLayerMarks();
		DZS_Clip.Set(cx, cy, cw, ch);
		int t = TickCount(0) - t0;
		m_StatMarks++;
		m_StatMarksWorst = Math.Max(m_StatMarksWorst, t);
		m_StatMarksLines = m_Lines;
	}

	// Changes whenever what the active layer shows changes.
	protected string ContentSig()
	{
		DZS_Layer l = DZS_Layers.Current();
		string id = "";
		if (l)
			id = l.Id();
		if (id != m_LayerId)
		{
			m_LayerId = id;
			m_LayerSig = "-";
		}
		if (id == "")
		{
			if (m_PointsDirty)
				CollectPoints();
			string sel = "";
			DZS_SelectionApi sa = DZS_SelectionApi.Get();
			if (sa)
				sel = sa.Signature();
			return "objects|" + m_PointsGen + "|" + sel + "|" + DZS_ViewPose.Stamp();
		}
		string sig = l.MiniMapSig();
		if (sig != m_LayerSig)
		{
			m_LayerSig = sig;
			m_LayerMarks.Clear();
			l.MiniMapMarks(m_LayerMarks);
		}
		return id + "|" + sig;
	}

	protected void CollectPoints()
	{
		m_PointsDirty = false;
		m_PointsGen++;
		m_Points.Clear();
		m_PointIds.Clear();
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

	// The project's objects as points; the selected ones larger with a light edge, on top.
	protected void DrawObjects()
	{
		DZS_SelectionApi sa = DZS_SelectionApi.Get();
		float d = Math.Max(3, DZS_Scale.Px(4));
		float e = d + Math.Max(2, DZS_Scale.Px(2));
		float ds = Math.Max(5, DZS_Scale.Px(7));
		float es = ds + Math.Max(2, DZS_Scale.Px(3));
		int fill = DZS_Color.Get("accent");
		int edge = DZS_Color.Get("well");
		int light = ARGB(255, 255, 255, 255);
		array<float> selX = new array<float>();
		array<float> selY = new array<float>();
		vector vp, vy;
		float vs;
		for (int i = 0; i < m_Points.Count(); i++)
		{
			vector p = m_Points[i];
			string id = m_PointIds[i];
			if (DZS_ViewPose.Get(id, vp, vy, vs))
				p = vp;
			float x = LocalX(p[0]);
			float y = LocalY(p[2]);
			if (x < -es || y < -es || x > m_S + es || y > m_S + es)
				continue;
			if (sa && sa.IsSelected(id))
			{
				selX.Insert(x);
				selY.Insert(y);
				continue;
			}
			Square(x, y, e, edge);
			Square(x, y, d, fill);
		}
		for (int k = 0; k < selX.Count(); k++)
		{
			Square(selX[k], selY[k], es, light);
			Square(selX[k], selY[k], ds, fill);
		}
	}

	// Marks of a layer: points (spawns) or circles of their size in metres (effect areas, zones).
	protected void DrawLayerMarks()
	{
		int dark = DZS_Color.Get("well");
		int light = ARGB(255, 255, 255, 255);
		float px1 = DZS_Scale.Px(1);
		float edge = Math.Max(1, px1);
		// the visible marks in three piles, drawn in order: plain, larger (a selected spawn event), the selection
		array<DZS_MapMark> plain = new array<DZS_MapMark>();
		array<DZS_MapMark> big = new array<DZS_MapMark>();
		array<DZS_MapMark> sel = new array<DZS_MapMark>();
		foreach (DZS_MapMark m : m_LayerMarks)
		{
			float x = LocalX(m.m_X);
			float y = LocalY(m.m_Z);
			float reach = Math.Max(m.m_RadiusM * m_PX, m.m_Size * px1) + 4;
			if (x < -reach || y < -reach || x > m_S + reach || y > m_S + reach)
				continue;
			if (m.m_Ring)
				sel.Insert(m);
			else if (m.m_Size >= 10)
				big.Insert(m);
			else
				plain.Insert(m);
		}
		foreach (DZS_MapMark a : plain)
			DrawLayerMark(a, Math.Max(1, px1 * 2.5), dark, edge);
		foreach (DZS_MapMark b : big)
			DrawLayerMark(b, Math.Max(1, px1 * 2.5), light, edge);
		foreach (DZS_MapMark c : sel)
			DrawLayerMark(c, Math.Max(3, px1 * 3.5), light, Math.Max(2, px1 * 2));
	}

	// A point, or a circle of its radius once that is larger than the point, on an edge of edgeColor.
	protected void DrawLayerMark(DZS_MapMark m, float lineW, int edgeColor, float edge)
	{
		float x = LocalX(m.m_X);
		float y = LocalY(m.m_Z);
		float r = m.m_RadiusM * m_PX;
		float dot = Math.Max(3, DZS_Scale.Px(m.m_Size * 0.6));
		if (m.m_RadiusM > 0 && r >= dot)
		{
			Ring(x, y, r, lineW + 2 * edge, edgeColor);
			Ring(x, y, r, lineW, m.m_Color);
			return;
		}
		Disc(x, y, dot + 2 * edge, edgeColor);
		Disc(x, y, dot, m.m_Color);
	}

	// ---- camera ----

	// Field of view as a violet wedge (the map is mostly light paper), the camera as a dark dot in a white ring; redrawn
	// only when it moves or turns.
	protected void DrawCamera()
	{
		vector cam = m_Cam.GetPosition();
		float x = LocalX(cam[0]);
		float y = LocalY(cam[2]);
		float yaw = m_Cam.GetYaw();
		float fov = m_Cam.HorizontalFov();
		string sig = Math.Round(x * 4).ToString() + "|" + Math.Round(y * 4).ToString() + "|" + Math.Round(yaw * 2).ToString() + "|" + Math.Round(fov);
		if (sig == m_CamSig)
			return;
		m_CamSig = sig;
		float cx = DZS_Clip.s_X;
		float cy = DZS_Clip.s_Y;
		float cw = DZS_Clip.s_W;
		float ch = DZS_Clip.s_H;
		DZS_Clip.Set(1, 1, m_S - 2, m_S - 2);
		m_CamCanvas.Clear();
		float rad = yaw * Math.DEG2RAD;
		float dx = Math.Sin(rad);
		float dy = -Math.Cos(rad);
		float len = m_S * 0.22;
		float half = Math.Tan(Math.Clamp(fov, 10, 170) * Math.DEG2RAD / 2);
		Wedge(x, y, dx, dy, len, half, ARGB(80, 0x8B, 0x5C, 0xF6));
		float px = -dy;
		float py = dx;
		int edgeCol = ARGB(255, 0x8B, 0x5C, 0xF6);
		float ex = len * half / Math.Sqrt(1 + half * half);
		float ez = len / Math.Sqrt(1 + half * half);
		float lw = Math.Max(1, DZS_Scale.Px(1.5));
		DZS_Clip.Line(m_CamCanvas, x, y, x + dx * ez + px * ex, y + dy * ez + py * ex, lw, edgeCol);
		DZS_Clip.Line(m_CamCanvas, x, y, x + dx * ez - px * ex, y + dy * ez - py * ex, lw, edgeCol);
		float r = Math.Max(4, DZS_Scale.Px(5));
		float e = Math.Max(2, DZS_Scale.Px(2));
		DiscOn(m_CamCanvas, x, y, 2 * (r + e), ARGB(255, 255, 255, 255));
		DiscOn(m_CamCanvas, x, y, 2 * r, ARGB(255, 0x09, 0x09, 0x0B));
		DiscOn(m_CamCanvas, x, y, r * 0.8, ARGB(255, 255, 255, 255));
		DZS_Clip.Set(cx, cy, cw, ch);
	}

	// A filled wedge from (x, y) along (dx, dy): strips across the direction, each as wide as the wedge there, so the
	// fill has one even alpha. Its far edge is the arc of radius len.
	protected void Wedge(float x, float y, float dx, float dy, float len, float half, int color)
	{
		float px = -dy;
		float py = dx;
		float axis = len / Math.Sqrt(1 + half * half);
		int strips = Math.Max(6, Math.Ceil(len / 2));
		float step = len / strips;
		for (int i = 0; i < strips; i++)
		{
			float a = step * (i + 0.5);
			float hw = a * half;
			if (a > axis)
				hw = Math.Sqrt(Math.Max(0, len * len - a * a));
			if (hw < 0.5)
				continue;
			float cx = x + dx * a;
			float cy = y + dy * a;
			DZS_Clip.Line(m_CamCanvas, cx - px * hw, cy - py * hw, cx + px * hw, cy + py * hw, step + 0.4, color);
		}
	}

	// ---- drawing ----

	protected void Square(float x, float y, float d, int color)
	{
		DZS_Clip.Line(m_MarkCanvas, x - d / 2, y, x + d / 2, y, d, color);
		m_Lines++;
	}

	protected void Disc(float x, float y, float d, int color)
	{
		DiscOn(m_MarkCanvas, x, y, d, color);
	}

	protected void DiscOn(CanvasWidget cv, float x, float y, float d, int color)
	{
		float r = d / 2;
		int strips = Math.Clamp(Math.Ceil(d / 3), 2, 8);
		float h = d / strips;
		for (int k = 0; k < strips; k++)
		{
			float yc = -r + h * (k + 0.5);
			float half = Math.Sqrt(Math.Max(0, r * r - yc * yc));
			if (half < 0.5)
				half = 0.5;
			DZS_Clip.Line(cv, x - half, y + yc, x + half, y + yc, h + 0.4, color);
			m_Lines++;
		}
	}

	// A circle outline of radius r px; fewer segments for small circles.
	protected void Ring(float x, float y, float r, float w, int color)
	{
		int segs = Math.Clamp(Math.Round(r / 2), 8, 40);
		float px = x + r;
		float py = y;
		for (int i = 1; i <= segs; i++)
		{
			float a = Math.PI2 * i / segs;
			float nx = x + Math.Cos(a) * r;
			float ny = y + Math.Sin(a) * r;
			DZS_Clip.Line(m_MarkCanvas, px, py, nx, ny, w, color);
			px = nx;
			py = ny;
		}
		m_Lines += segs;
	}

	// ---- for tests ----

	string Describe()
	{
		string r = "minimap on " + DZS_Str.Bool(IsOn()) + " shown " + DZS_Str.Bool(m_Shown) + " tab " + DZS_Str.Bool(m_TabShown);
		r += " rect " + m_X + " " + m_Y + " " + m_S + " span " + Math.Round(m_Span) + " m";
		if (m_ViewOk)
		{
			vector cam = m_Cam.GetPosition();
			r += " centre " + Math.Round(m_OX) + " " + Math.Round(m_OZ) + " mpp " + (1 / m_PX) + " camera at " + Math.Round(LocalX(cam[0])) + " " + Math.Round(LocalY(cam[2]));
		}
		r += " layer '" + m_LayerId + "' points " + m_Points.Count() + " layer marks " + m_LayerMarks.Count();
		return r;
	}

	string Stats()
	{
		float avg = m_StatTicks / Math.Max(1, m_StatFrames) / 10000.0;
		string r = "minimap stats: frames " + m_StatFrames + " avg " + avg + " ms worst " + (m_StatWorst / 10000.0) + " ms";
		r += " marks redraws " + m_StatMarks + " worst " + (m_StatMarksWorst / 10000.0) + " ms last lines " + m_StatMarksLines;
		return r;
	}

	void ResetStats()
	{
		m_StatFrames = 0;
		m_StatTicks = 0;
		m_StatWorst = 0;
		m_StatMarks = 0;
		m_StatMarksWorst = 0;
	}

	// Screen point of a world point on the minimap (tests click there).
	vector ScreenOf(float wx, float wz)
	{
		return Vector(m_X + LocalX(wx), m_Y + LocalY(wz), 0);
	}

	void SetSpan(float span)
	{
		m_Span = ClampSpan(span);
		SaveSpan();
		m_HasWant = false;
	}
}

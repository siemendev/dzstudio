// Speed stage display at the pointer, like the gear display of a race car.
class DZS_CameraGear : Managed
{
	static const float ANIM = 0.15;
	static const float HOLD = 1.0;
	static const float FADE = 0.2;

	protected Widget m_Frame;
	protected Widget m_Bg;
	protected ref DZS_Border m_Border;
	protected Widget m_Clip;
	protected TextWidget m_Old;
	protected TextWidget m_New;
	protected TextWidget m_Speed;
	protected float m_Age = 100;
	protected float m_AnimT = 100;
	protected int m_Dir;
	protected bool m_Bump;
	protected string m_NewText;

	void DZS_CameraGear(Widget root)
	{
		m_Frame = DZS_W.Frame(root);
		m_Bg = DZS_W.Panel(m_Frame, DZS_Color.Get("plate"));
		m_Border = new DZS_Border(m_Frame, DZS_Color.Get("lineStrong"));
		m_Clip = DZS_W.Frame(m_Frame);
		m_Clip.SetFlags(WidgetFlags.CLIPCHILDREN);
		m_Old = DZS_W.Text(m_Clip, DZS_Font.DISPLAY, 26, DZS_Color.Get("text"), true);
		m_New = DZS_W.Text(m_Clip, DZS_Font.DISPLAY, 26, DZS_Color.Get("text"), true);
		m_Speed = DZS_W.Text(m_Frame, DZS_Font.LABEL, 10, DZS_Color.Get("textMuted"), true);
		m_Frame.Show(false);
	}

	bool IsVisible() { return m_Age < HOLD + FADE; }

	void Shift(int stage, int dir, string speedText)
	{
		string prev = m_NewText;
		if (!IsVisible())
			prev = "";
		m_Old.SetText(prev);
		m_NewText = stage.ToString();
		m_New.SetText(m_NewText);
		m_Speed.SetText(speedText);
		m_Dir = dir;
		m_Bump = false;
		m_Age = 0;
		m_AnimT = 0;
		DZS_Log.Info("gear shows " + stage + " / " + speedText);
	}

	// At the ends: a short sideways wiggle and "min"/"max" instead of the slide.
	void Bump(int stage, int dir, string speedText)
	{
		m_Old.SetText("");
		m_NewText = stage.ToString();
		m_New.SetText(m_NewText);
		string end = DZS_Texts.Get("camera.gear.min");
		if (dir > 0)
			end = DZS_Texts.Get("camera.gear.max");
		m_Speed.SetText(end);
		m_Bump = true;
		m_Age = 0;
		m_AnimT = 0;
		DZS_Log.Info("gear bump " + end);
	}

	// Slide with a little overshoot that settles (ease out back), always exactly done after ANIM.
	protected static float EaseOutBack(float t)
	{
		float c1 = 1.70158;
		float c3 = c1 + 1;
		float u = t - 1;
		return 1 + c3 * u * u * u + c1 * u * u;
	}

	void Update(float dt, DZS_Screen scr)
	{
		m_Age += dt;
		m_AnimT += dt;
		if (!IsVisible())
		{
			m_Frame.Show(false);
			return;
		}
		m_Frame.Show(true);
		float w = DZS_Scale.Px(64);
		float h = DZS_Scale.Px(58);
		float numH = DZS_Scale.Px(34);
		int sw, sh;
		GetScreenSize(sw, sh);
		int mx, my;
		if (scr.IsCursorVisible())
			GetMousePos(mx, my);
		else
		{
			mx = sw / 2;
			my = sh / 2;
		}
		float off = DZS_Scale.Px(24);
		float x = Math.Clamp(mx + off, 0, sw - w);
		float y = Math.Clamp(my + off, 0, sh - h);
		float t = Math.Clamp(m_AnimT / ANIM, 0, 1);
		float wiggle = 0;
		float slide = 0;
		if (m_Bump)
		{
			if (t < 1)
				wiggle = Math.Sin(t * Math.PI * 4) * DZS_Scale.Px(5) * (1 - t);
		}
		else
			slide = 1 - EaseOutBack(t);
		DZS_W.Place(m_Frame, x + wiggle, y, w, h);
		DZS_W.Place(m_Bg, 0, 0, w, h);
		m_Border.Place(0, 0, w, h);
		DZS_W.Place(m_Clip, 0, DZS_Scale.Px(4), w, numH);
		// shifting up: the old number leaves upwards and the new one comes in from below
		float travel = numH * m_Dir;
		DZS_W.Place(m_New, 0, slide * travel, w, numH);
		DZS_W.Place(m_Old, 0, slide * travel - travel, w, numH);
		m_Old.Show(!m_Bump && t < 1);
		DZS_W.Place(m_Speed, 0, DZS_Scale.Px(4) + numH, w, h - numH - DZS_Scale.Px(8));
		float a = 1;
		if (m_Age > HOLD)
			a = 1 - (m_Age - HOLD) / FADE;
		m_Frame.SetAlpha(Math.Clamp(a, 0, 1));
		if (m_Age < 0.05)
			DZS_Log.Info("gear at " + x.ToString() + "," + y.ToString() + " size " + w.ToString() + "x" + h.ToString() + " alpha " + a.ToString());
	}
}

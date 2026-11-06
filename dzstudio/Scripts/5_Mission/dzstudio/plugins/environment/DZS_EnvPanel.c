// One value row: name, slider and number field.
class DZS_EnvRow : Managed
{
	string m_Key;
	string m_Unit;
	TextWidget m_Name;
	ref DZS_Slider m_Slider;
	ref DZS_NumberField m_Field;
	DZS_EnvPanel m_Panel;

	void DZS_EnvRow(DZS_EnvPanel panel, Widget parent, string key, string label, float min, float max, string unit)
	{
		m_Panel = panel;
		m_Key = key;
		m_Unit = unit;
		m_Name = DZS_W.Text(parent, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_Name.SetText(label);
		m_Slider = new DZS_Slider(parent, min, max);
		m_Slider.m_OnChange.Insert(OnSlide);
		m_Field = new DZS_NumberField("env." + key, "", "textMuted", 56);
		m_Field.Build(parent);
		m_Field.m_OnCommit.Insert(OnCommit);
		m_Field.m_OnWheel.Insert(OnWheel);
	}

	void OnSlide(DZS_Slider s, float v)
	{
		m_Panel.m_P.SetValue(m_Key, Math.Round(v));
	}

	void OnCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		float v;
		r.m_Ok = DZS_Num.Parse(typed, v);
		if (r.m_Ok)
			m_Panel.m_P.SetValue(m_Key, Math.Round(v));
	}

	void OnWheel(DZS_NumberField f, int steps, bool fine)
	{
		m_Panel.m_P.SetValue(m_Key, m_Panel.m_P.GetValue(m_Key) + steps);
		f.ForceValue(m_Panel.m_P.GetValue(m_Key).ToString());
	}

	// Name above, slider and field below (the long names of the ground fog do not fit beside the slider).
	float Place(float x, float y, float width)
	{
		float nameH = DZS_Scale.Px(18);
		float rowH = DZS_Scale.Px(28);
		DZS_W.Place(m_Name, x, y, width, nameH);
		if (!m_Slider.IsDragging())
			m_Slider.SetValue(m_Panel.m_P.GetValue(m_Key));
		m_Slider.Place(x, y + nameH + (rowH - DZS_Slider.Height()) / 2);
		m_Field.ShowValue(m_Panel.m_P.GetValue(m_Key).ToString());
		m_Field.Place(x + width - m_Field.GetWidth(), y + nameH, rowH);
		return nameH + rowH + DZS_Scale.Px(4);
	}

	void Show(bool s)
	{
		m_Name.Show(s);
		m_Slider.m_Hit.m_W.Show(s);
		m_Field.Show(s);
	}

	void Update(float dt)
	{
		m_Slider.Update();
		m_Field.Update(dt);
	}
}

class DZS_EnvPanel : DZS_PanelContent
{
	DZS_EnvPlugin m_P;
	protected Widget m_Root;
	protected ref DZS_Button m_Reset;
	protected ref DZS_Button m_Hide;
	protected TextWidget m_TimeHead;
	protected TextWidget m_WeatherHead;
	protected TextWidget m_LightHead;
	protected TextWidget m_Clock;
	protected ref DZS_Slider m_TimeSlider;
	protected ref DZS_NumberField m_Hour;
	protected ref DZS_NumberField m_Min;
	protected TextWidget m_HourLabel;
	protected TextWidget m_MinLabel;
	protected ref array<ref DZS_Button> m_TimeButtons = new array<ref DZS_Button>();
	protected ref array<ref DZS_Chip> m_Months = new array<ref DZS_Chip>();
	protected TextWidget m_DayLabel;
	protected ref DZS_NumberField m_Day;
	protected ref array<ref DZS_EnvRow> m_Weather = new array<ref DZS_EnvRow>();
	protected ref array<ref DZS_Button> m_Presets = new array<ref DZS_Button>();
	protected TextWidget m_NoSnow;
	protected ref DZS_Button m_Advanced;
	protected bool m_AdvancedOpen;
	protected ref array<ref DZS_EnvRow> m_Ground = new array<ref DZS_EnvRow>();
	protected ref DZS_Chip m_Normal;
	protected ref DZS_Chip m_Dark;

	void DZS_EnvPanel(DZS_EnvPlugin p)
	{
		m_P = p;
	}

	override void BuildHead(Widget head)
	{
		m_Reset = new DZS_Button(head, "ghost", DZS_Texts.Get("env.reset"), "", 24);
		m_Reset.m_Hit.m_FocusKind = "";
		m_Reset.m_OnClick.Insert(OnReset);
		m_Hide = new DZS_Button(head, "ghost", "", "x", 24);
		m_Hide.m_Hit.m_FocusKind = "";
		m_Hide.SetTip(DZS_Texts.Get("env.hide"));
		m_Hide.m_OnClick.Insert(OnHide);
	}

	override float LayoutHead(float right, float h)
	{
		bool start = m_P.AtStart();
		if (start)
			m_Reset.SetEnabled(false, DZS_Texts.Get("env.reset.tip"));
		else
			m_Reset.SetEnabled(true);
		float x = right - m_Hide.GetWidth();
		m_Hide.Place(x, (h - m_Hide.GetHeight()) / 2);
		x -= m_Reset.GetWidth() + DZS_Scale.Px(4);
		m_Reset.Place(x, (h - m_Reset.GetHeight()) / 2);
		return right - x;
	}

	protected TextWidget GroupTitle(string key)
	{
		TextWidget t = DZS_W.Text(m_Root, DZS_Font.MONO, 11, DZS_Color.Get("textQuiet"));
		t.SetText(DZS_Texts.Upper(DZS_Texts.Get(key)));
		return t;
	}

	override void Build(Widget parent)
	{
		m_Root = DZS_W.Frame(parent);
		m_TimeHead = GroupTitle("env.time");
		m_WeatherHead = GroupTitle("env.weather");
		m_LightHead = GroupTitle("env.light");
		m_Clock = DZS_W.Text(m_Root, DZS_Font.MONO, 12, DZS_Color.Get("text"));
		m_TimeSlider = new DZS_Slider(m_Root, 0, 1439);
		m_TimeSlider.m_OnChange.Insert(OnTimeSlide);
		m_HourLabel = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_HourLabel.SetText(DZS_Texts.Get("env.hour"));
		m_MinLabel = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_MinLabel.SetText(DZS_Texts.Get("env.minute"));
		m_Hour = new DZS_NumberField("env.hour", "", "textMuted", 44);
		m_Hour.Build(m_Root);
		m_Hour.m_OnCommit.Insert(OnHourCommit);
		m_Hour.m_OnWheel.Insert(OnHourWheel);
		m_Min = new DZS_NumberField("env.min", "", "textMuted", 44);
		m_Min.Build(m_Root);
		m_Min.m_OnCommit.Insert(OnMinCommit);
		m_Min.m_OnWheel.Insert(OnMinWheel);
		array<string> tb = {"env.morning", "env.noon", "env.evening", "env.night"};
		foreach (string k : tb)
		{
			DZS_Button b = new DZS_Button(m_Root, "side", DZS_Texts.Get(k), "", 24);
			b.m_Tag = k;
			b.m_Hit.m_FocusKind = "";
			b.m_OnClick.Insert(OnTimeButton);
			m_TimeButtons.Insert(b);
		}
		for (int i = 1; i <= 12; i++)
		{
			DZS_Chip c = new DZS_Chip(m_Root, i.ToString(), DZS_Texts.Get("env.month." + i));
			c.m_OnClick.Insert(OnMonth);
			m_Months.Insert(c);
		}
		m_DayLabel = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textMuted"));
		m_DayLabel.SetText(DZS_Texts.Get("env.day"));
		m_Day = new DZS_NumberField("env.day", "", "textMuted", 44);
		m_Day.Build(m_Root);
		m_Day.m_OnCommit.Insert(OnDayCommit);
		m_Day.m_OnWheel.Insert(OnDayWheel);
		m_Weather.Insert(new DZS_EnvRow(this, m_Root, "overcast", DZS_Texts.Get("env.overcast"), 0, 100, "%"));
		m_Weather.Insert(new DZS_EnvRow(this, m_Root, "rain", DZS_Texts.Get("env.rain"), 0, 100, "%"));
		m_Weather.Insert(new DZS_EnvRow(this, m_Root, "fog", DZS_Texts.Get("env.fog"), 0, 100, "%"));
		m_Weather.Insert(new DZS_EnvRow(this, m_Root, "snow", DZS_Texts.Get("env.snow"), 0, 100, "%"));
		m_Weather.Insert(new DZS_EnvRow(this, m_Root, "flake", DZS_Texts.Get("env.flake"), 0, 100, "%"));
		array<string> pk = {"clear", "cloudy", "rain", "fog", "snow"};
		array<string> pl = {"env.clear", "env.cloudy", "env.rain", "env.fog", "env.snow"};
		for (int j = 0; j < 5; j++)
		{
			DZS_Button pb = new DZS_Button(m_Root, "side", DZS_Texts.Get(pl[j]), "", 24);
			pb.m_Tag = pk[j];
			pb.m_Hit.m_FocusKind = "";
			pb.m_OnClick.Insert(OnPreset);
			m_Presets.Insert(pb);
		}
		m_NoSnow = DZS_W.Text(m_Root, DZS_Font.READ, 13, DZS_Color.Get("textQuiet"));
		m_NoSnow.SetText(DZS_Texts.Get("env.noSnow"));
		m_Advanced = new DZS_Button(m_Root, "ghost", DZS_Texts.Get("env.advanced"), "chevron-down", 24);
		m_Advanced.m_Hit.m_FocusKind = "";
		m_Advanced.m_OnClick.Insert(OnAdvanced);
		m_Ground.Insert(new DZS_EnvRow(this, m_Root, "volDistance", DZS_Texts.Get("env.volDistance"), 0, 100, "%"));
		m_Ground.Insert(new DZS_EnvRow(this, m_Root, "volHeight", DZS_Texts.Get("env.volHeight"), 0, 100, "%"));
		m_Ground.Insert(new DZS_EnvRow(this, m_Root, "volBias", DZS_Texts.Get("env.volBias"), DZS_EnvPlugin.BIAS_MIN, DZS_EnvPlugin.BIAS_MAX, "m"));
		m_Normal = new DZS_Chip(m_Root, "normal", DZS_Texts.Get("env.normal"));
		m_Normal.m_OnClick.Insert(OnNights);
		m_Dark = new DZS_Chip(m_Root, "dark", DZS_Texts.Get("env.dark"));
		m_Dark.m_OnClick.Insert(OnNights);
	}

	// Shown again: "Erweitert" starts closed only the first time (it stays as it was until the end).
	void OnShown() {}

	override float Layout(float width)
	{
		float pad = DZS_Scale.Px(12);
		float inner = width - 2 * pad;
		float y = DZS_Scale.Px(8);
		float lineH = DZS_Scale.Px(22);
		DZS_W.Place(m_Root, 0, 0, width, DZS_Scale.Px(1200));
		DZS_W.Place(m_TimeHead, pad, y, inner, lineH);
		y += lineH;
		float rowH = DZS_Scale.Px(30);
		m_Clock.SetText(m_P.Clock());
		DZS_W.Place(m_Clock, pad, y, DZS_Scale.Px(48), rowH);
		if (!m_TimeSlider.IsDragging())
			m_TimeSlider.SetValue(m_P.m_Minute);
		m_TimeSlider.Place(pad + DZS_Scale.Px(50), y + (rowH - DZS_Slider.Height()) / 2);
		y += rowH;
		float fx = pad;
		DZS_W.Place(m_HourLabel, fx, y, DZS_Scale.Px(30), rowH);
		fx += DZS_Scale.Px(30);
		m_Hour.ShowValue((m_P.m_Minute / 60).ToString());
		m_Hour.Place(fx, y, rowH);
		fx += m_Hour.GetWidth() + DZS_Scale.Px(16);
		DZS_W.Place(m_MinLabel, fx, y, DZS_Scale.Px(30), rowH);
		fx += DZS_Scale.Px(30);
		m_Min.ShowValue((m_P.m_Minute % 60).ToString());
		m_Min.Place(fx, y, rowH);
		y += rowH + DZS_Scale.Px(4);
		float bx = pad;
		foreach (DZS_Button b : m_TimeButtons)
		{
			b.Place(bx, y);
			bx += b.GetWidth() + DZS_Scale.Px(4);
		}
		y += DZS_Scale.Px(28);
		float cw = (inner - 5 * DZS_Scale.Px(4)) / 6;
		for (int i = 0; i < 12; i++)
		{
			int col = i % 6;
			int row = i / 6;
			m_Months[i].SetOn(m_P.m_Month == i + 1);
			m_Months[i].Place(pad + col * (cw + DZS_Scale.Px(4)), y + row * (DZS_Chip.Height() + DZS_Scale.Px(4)), cw);
		}
		y += 2 * (DZS_Chip.Height() + DZS_Scale.Px(4));
		DZS_W.Place(m_DayLabel, pad, y, DZS_Scale.Px(30), rowH);
		m_Day.ShowValue(m_P.m_Day.ToString());
		m_Day.Place(pad + DZS_Scale.Px(30), y, rowH);
		y += rowH + DZS_Scale.Px(8);
		DZS_W.Place(m_WeatherHead, pad, y, inner, lineH);
		y += lineH;
		foreach (DZS_EnvRow r : m_Weather)
			y += r.Place(pad, y, inner);
		bx = pad;
		foreach (DZS_Button p : m_Presets)
		{
			if (bx + p.GetWidth() > pad + inner)
			{
				bx = pad;
				y += DZS_Scale.Px(28);
			}
			p.Place(bx, y);
			bx += p.GetWidth() + DZS_Scale.Px(4);
		}
		y += DZS_Scale.Px(28);
		bool noSnow = !m_P.MapShowsSnow();
		m_NoSnow.Show(noSnow);
		if (noSnow)
		{
			DZS_W.Place(m_NoSnow, pad, y, inner, lineH);
			y += lineH;
		}
		if (m_AdvancedOpen)
			m_Advanced.SetIcon("chevron-up");
		else
			m_Advanced.SetIcon("chevron-down");
		m_Advanced.Place(pad, y);
		y += DZS_Scale.Px(28);
		foreach (DZS_EnvRow g : m_Ground)
		{
			g.Show(m_AdvancedOpen);
			if (m_AdvancedOpen)
				y += g.Place(pad, y, inner);
		}
		y += DZS_Scale.Px(6);
		DZS_W.Place(m_LightHead, pad, y, inner, lineH);
		y += lineH;
		float half = (inner - DZS_Scale.Px(6)) / 2;
		m_Normal.SetOn(!m_P.m_Dark);
		m_Dark.SetOn(m_P.m_Dark);
		m_Normal.Place(pad, y, half);
		m_Dark.Place(pad + half + DZS_Scale.Px(6), y, half);
		y += DZS_Chip.Height() + DZS_Scale.Px(12);
		return y;
	}

	override void Update(float dt)
	{
		m_TimeSlider.Update();
		m_Hour.Update(dt);
		m_Min.Update(dt);
		m_Day.Update(dt);
		foreach (DZS_EnvRow r : m_Weather)
			r.Update(dt);
		foreach (DZS_EnvRow g : m_Ground)
			g.Update(dt);
	}

	void OnReset(DZS_Button b)
	{
		m_P.ResetValues();
	}

	void OnHide(DZS_Button b)
	{
		m_P.SetShown(false);
	}

	void OnTimeSlide(DZS_Slider s, float v)
	{
		m_P.SetMinute(Math.Round(v));
	}

	void OnTimeButton(DZS_Button b)
	{
		if (b.m_Tag == "env.morning")
			m_P.SetMinute(7 * 60);
		else if (b.m_Tag == "env.noon")
			m_P.SetMinute(12 * 60);
		else if (b.m_Tag == "env.evening")
			m_P.SetMinute(19 * 60);
		else
			m_P.SetMinute(0);
	}

	protected bool Int(string typed, out int v)
	{
		float f;
		if (!DZS_Num.Parse(typed, f))
			return false;
		v = Math.Round(f);
		return true;
	}

	void OnHourCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		int v;
		r.m_Ok = Int(typed, v);
		if (r.m_Ok)
			m_P.SetHour(v);
	}

	void OnMinCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		int v;
		r.m_Ok = Int(typed, v);
		if (r.m_Ok)
			m_P.SetMinuteOfHour(v);
	}

	void OnDayCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		int v;
		r.m_Ok = Int(typed, v);
		if (r.m_Ok)
			m_P.SetDay(v);
	}

	// Wheel in a field with focus: one hour or minute per notch, no carry into the other field.
	void OnHourWheel(DZS_NumberField f, int steps, bool fine)
	{
		m_P.SetHour(m_P.m_Minute / 60 + steps);
		f.ForceValue((m_P.m_Minute / 60).ToString());
	}

	void OnMinWheel(DZS_NumberField f, int steps, bool fine)
	{
		m_P.SetMinuteOfHour(m_P.m_Minute % 60 + steps);
		f.ForceValue((m_P.m_Minute % 60).ToString());
	}

	void OnDayWheel(DZS_NumberField f, int steps, bool fine)
	{
		m_P.SetDay(m_P.m_Day + steps);
		f.ForceValue(m_P.m_Day.ToString());
	}

	void OnMonth(DZS_Chip c)
	{
		m_P.SetMonth(c.m_Key.ToInt());
	}

	void OnPreset(DZS_Button b)
	{
		m_P.Preset(b.m_Tag);
	}

	void OnAdvanced(DZS_Button b)
	{
		m_AdvancedOpen = !m_AdvancedOpen;
	}

	void OnNights(DZS_Chip c)
	{
		m_P.SetDark(c.m_Key == "dark");
	}
}

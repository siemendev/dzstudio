// Weather and time: time, date, weather, ground fog and nights for the one who sets them. All
// values are view: no operation, no undo step, no file, not in the settings; every project starts with the start values.
class DZS_EnvActions : DZS_ActionHandler
{
	DZS_EnvPlugin m_P;

	override bool IsActionChecked(string actionId) { return actionId == "environment.toggle" && m_P.IsShown(); }

	override void OnAction(string actionId)
	{
		if (actionId == "environment.toggle")
			m_P.SetShown(!m_P.IsShown());
	}
}

[DZS_RegisterPlugin(DZS_EnvPlugin)]
class DZS_EnvPlugin : DZS_Plugin
{
	static const string PANEL = "environment.panel";
	// The game's fog height is metres above sea level, not above the ground (proved in the game):
	// −200…+200 m would not reach the ground of the maps (Chernarus up to about 700 m).
	static const int BIAS_MIN = -200;
	static const int BIAS_MAX = 1000;
	// The year the editor gives the game; it has no 29 February and appears nowhere in the surface.
	static const int YEAR = 2023;

	// values
	int m_Minute = 720;
	int m_Month = 6;
	int m_Day = 15;
	int m_Overcast;
	int m_Rain;
	int m_Fog;
	int m_Snow;
	int m_Flake = 50;
	int m_VolDistance;
	int m_VolHeight;
	int m_VolBias;
	bool m_Dark;

	protected ref DZS_EnvActions m_Actions;
	protected ref DZS_EnvPanel m_Panel;
	protected bool m_World;
	protected string m_WorldName;
	protected string m_LightingNow;
	protected float m_Time;
	protected float m_CheckAt;
	protected float m_LogAt;
	protected DZS_Instance m_For;

	override string GetName() { return "environment"; }

	override void OnRegister()
	{
		RegisterTexts();
		m_Actions = new DZS_EnvActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("environment", "Wetter und Zeit", "Weather and time");
		ui.RegisterAction("environment", "environment.toggle", "Wetter und Zeit an/aus", "Weather and time on/off", null, m_Actions);
		ui.SetWhen("environment.toggle", "scene");
		ui.SetGroup("environment.toggle", "environment");
		ui.RegisterTool("environment", "environment.toggle", "Wetter und Zeit", "Weather and time", "switches", "cloud-clock", 50);
		ui.AddMenuEntry("view", "environment.toggle", 350, true);
		m_Panel = new DZS_EnvPanel(this);
		ui.RegisterPanel("environment", PANEL, "Wetter und Zeit", "Weather and time", "right", "cloud-clock", m_Panel, 600);
		ui.SetPanelShown(PANEL, false);
	}

	protected void RegisterTexts()
	{
		DZS_Texts.Add("env.reset", "Auf Start", "To start");
		DZS_Texts.Add("env.reset.tip", "Alles steht auf den Startwerten", "Everything is at the start values");
		DZS_Texts.Add("env.hide", "Ausblenden", "Hide");
		DZS_Texts.Add("env.time", "// Zeit", "// Time");
		DZS_Texts.Add("env.weather", "// Wetter", "// Weather");
		DZS_Texts.Add("env.light", "// Licht", "// Light");
		DZS_Texts.Add("env.hour", "Std", "Hr");
		DZS_Texts.Add("env.minute", "Min", "Min");
		DZS_Texts.Add("env.morning", "Morgen", "Morning");
		DZS_Texts.Add("env.noon", "Mittag", "Noon");
		DZS_Texts.Add("env.evening", "Abend", "Evening");
		DZS_Texts.Add("env.night", "Nacht", "Night");
		DZS_Texts.Add("env.day", "Tag", "Day");
		DZS_Texts.Add("env.overcast", "Bewölkung", "Clouds");
		DZS_Texts.Add("env.rain", "Regen", "Rain");
		DZS_Texts.Add("env.fog", "Nebel", "Fog");
		DZS_Texts.Add("env.snow", "Schnee", "Snow");
		DZS_Texts.Add("env.flake", "Flockengröße", "Flake size");
		DZS_Texts.Add("env.clear", "Klar", "Clear");
		DZS_Texts.Add("env.cloudy", "Bewölkt", "Cloudy");
		DZS_Texts.Add("env.noSnow", "Auf dieser Karte fällt kein Schnee", "No snow falls on this map");
		DZS_Texts.Add("env.advanced", "Erweitert", "Advanced");
		DZS_Texts.Add("env.volDistance", "Dichte in der Ferne", "Density far away");
		DZS_Texts.Add("env.volHeight", "Dichte nach Höhe", "Density by height");
		DZS_Texts.Add("env.volBias", "Höhe der Nebeldecke", "Height of the fog");
		DZS_Texts.Add("env.normal", "Normal", "Normal");
		DZS_Texts.Add("env.dark", "Dunkel", "Dark");
		array<string> de = {"Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};
		array<string> en = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		for (int i = 0; i < 12; i++)
			DZS_Texts.Add("env.month." + (i + 1), de[i], en[i]);
	}

	override void OnSessionStart(string world)
	{
		m_World = world != "";
		m_WorldName = world;
		m_WorldName.ToLower();
	}

	bool IsShown() { return m_Kernel.GetUi().IsPanelShown(PANEL); }

	void SetShown(bool on)
	{
		m_Kernel.GetUi().SetPanelShown(PANEL, on);
		if (on)
			m_Panel.OnShown();
	}

	// Every newly shown project starts with the start values (New project, Open, Import as a new project);
	// "Save as" keeps the instance and the values.
	override void OnShownInstanceChanged(DZS_Instance inst)
	{
		if (!inst || inst == m_For)
			return;
		m_For = inst;
		ResetValues();
		DZS_Log.Info("environment: start values for the opened project");
	}

	void ResetValues()
	{
		m_Minute = 720;
		m_Month = 6;
		m_Day = 15;
		m_Overcast = 0;
		m_Rain = 0;
		m_Fog = 0;
		m_Snow = 0;
		m_Flake = 50;
		m_VolDistance = 0;
		m_VolHeight = 0;
		m_VolBias = 0;
		m_Dark = false;
		Apply("start");
	}

	bool AtStart()
	{
		return m_Minute == 720 && m_Month == 6 && m_Day == 15 && m_Overcast == 0 && m_Rain == 0 && m_Fog == 0 && m_Snow == 0 && m_Flake == 50 && m_VolDistance == 0 && m_VolHeight == 0 && m_VolBias == 0 && !m_Dark;
	}

	static int DaysIn(int month)
	{
		if (month == 2)
			return 28;
		if (month == 4 || month == 6 || month == 9 || month == 11)
			return 30;
		return 31;
	}

	// ---- setters (all clamp to their ranges) ----

	void SetMinute(int m)
	{
		m_Minute = Math.Clamp(m, 0, 1439);
		Apply("time");
	}

	void SetHour(int h)
	{
		SetMinute(Math.Clamp(h, 0, 23) * 60 + m_Minute % 60);
	}

	void SetMinuteOfHour(int m)
	{
		SetMinute((m_Minute / 60) * 60 + Math.Clamp(m, 0, 59));
	}

	void SetMonth(int month)
	{
		m_Month = Math.Clamp(month, 1, 12);
		if (m_Day > DaysIn(m_Month))
			m_Day = DaysIn(m_Month);
		Apply("date");
	}

	void SetDay(int day)
	{
		m_Day = Math.Clamp(day, 1, DaysIn(m_Month));
		Apply("date");
	}

	void SetValue(string key, int v)
	{
		if (key == "overcast")
			m_Overcast = Math.Clamp(v, 0, 100);
		else if (key == "rain")
			m_Rain = Math.Clamp(v, 0, 100);
		else if (key == "fog")
			m_Fog = Math.Clamp(v, 0, 100);
		else if (key == "snow")
			m_Snow = Math.Clamp(v, 0, 100);
		else if (key == "flake")
			m_Flake = Math.Clamp(v, 0, 100);
		else if (key == "volDistance")
			m_VolDistance = Math.Clamp(v, 0, 100);
		else if (key == "volHeight")
			m_VolHeight = Math.Clamp(v, 0, 100);
		else if (key == "volBias")
			m_VolBias = Math.Clamp(v, BIAS_MIN, BIAS_MAX);
		Apply(key);
	}

	int GetValue(string key)
	{
		if (key == "overcast")
			return m_Overcast;
		if (key == "rain")
			return m_Rain;
		if (key == "fog")
			return m_Fog;
		if (key == "snow")
			return m_Snow;
		if (key == "flake")
			return m_Flake;
		if (key == "volDistance")
			return m_VolDistance;
		if (key == "volHeight")
			return m_VolHeight;
		if (key == "volBias")
			return m_VolBias;
		return 0;
	}

	// The five weather knobs; ground fog, time, date and nights stay.
	void Preset(string name)
	{
		if (name == "clear")
			SetWeather(0, 0, 0, 0, -1);
		else if (name == "cloudy")
			SetWeather(70, 0, 0, 0, -1);
		else if (name == "rain")
			SetWeather(90, 80, 10, 0, -1);
		else if (name == "fog")
			SetWeather(30, 0, 70, 0, -1);
		else if (name == "snow")
			SetWeather(80, 0, 10, 70, 50);
	}

	protected void SetWeather(int o, int r, int f, int s, int flake)
	{
		m_Overcast = o;
		m_Rain = r;
		m_Fog = f;
		m_Snow = s;
		if (flake >= 0)
			m_Flake = flake;
		Apply("preset");
	}

	void SetDark(bool dark)
	{
		m_Dark = dark;
		Apply("nights");
	}

	// Whether snow falls on the loaded map: the game's own snowfall value tells (asked for, still 0 after 2 s = no
	// snow here).
	protected bool m_SnowRefused;
	protected float m_SnowAskedAt = -1;

	bool MapShowsSnow()
	{
		return !m_SnowRefused;
	}

	// ---- into the game ----

	void Apply(string why)
	{
		if (!m_World)
			return;
		World w = GetGame().GetWorld();
		if (w)
			w.SetDate(YEAR, m_Month, m_Day, m_Minute / 60, m_Minute % 60);
		Weather we = GetGame().GetWeather();
		if (we)
		{
			we.SetWeatherUpdateFreeze(true);
			we.SetRainThresholds(0, 1, 0);
			we.SetSnowfallThresholds(0, 1, 0);
			we.GetRain().SetLimits(0, 1);
			we.GetSnowfall().SetLimits(0, 1);
			we.GetOvercast().SetLimits(0, 1);
			we.GetFog().SetLimits(0, 1);
			we.GetOvercast().Set(m_Overcast / 100.0, 0, 1000000);
			we.GetRain().Set(m_Rain / 100.0, 0, 1000000);
			we.GetFog().Set(m_Fog / 100.0, 0, 1000000);
			we.GetSnowfall().Set(m_Snow / 100.0, 0, 1000000);
			we.SetSnowflakeScale(Math.Pow(2, (m_Flake - 50) / 50.0));
			we.SetDynVolFogDistanceDensity(m_VolDistance / 100.0, 0);
			we.SetDynVolFogHeightDensity(m_VolHeight / 100.0, 0);
			we.SetDynVolFogHeightBias(m_VolBias, 0);
			if (why.IndexOf("vol") == 0 || why == "start")
				DZS_Log.Info("environment: volumetric fog enabled " + DZS_Str.Bool(we.IsDynVolFogEnabled()) + " distance " + we.GetDynVolFogDistanceDensity() + " height " + we.GetDynVolFogHeightDensity() + " bias " + we.GetDynVolFogHeightBias());
		}
		string lighting = LightingFile();
		if (w && lighting != m_LightingNow)
		{
			w.LoadNewLightingCfg(lighting);
			m_LightingNow = lighting;
		}
		string vals = Clock() + " " + m_Day + "." + m_Month + ". clouds " + m_Overcast + " rain " + m_Rain + " fog " + m_Fog;
		vals = vals + " snow " + m_Snow + " flake " + m_Flake + " ground fog " + m_VolDistance + "/" + m_VolHeight + "/" + m_VolBias;
		DZS_Log.Info("environment: " + why + " " + vals + " dark " + DZS_Str.Bool(m_Dark));
	}

	protected string LightingFile()
	{
		if (m_Dark)
			return "dz\\data\\lighting\\lighting_darknight.txt";
		if (m_WorldName == "sakhal")
			return "DZ\\worlds\\sakhal\\data\\lighting\\lighting_sakhal.txt";
		if (m_WorldName == "nasdara")
			return "DZ\\worlds\\nasdara\\data\\lighting\\lighting_nasdara.txt";
		return "dz\\data\\lighting\\lighting_default.txt";
	}

	string Clock()
	{
		int h = m_Minute / 60;
		int m = m_Minute % 60;
		string hs = h.ToString();
		if (h < 10)
			hs = "0" + hs;
		string ms = m.ToString();
		if (m < 10)
			ms = "0" + ms;
		return hs + ":" + ms;
	}

	// The time stands still: the clock and the weather are put back
	// whenever the game has moved them.
	override void OnUpdate(float dt)
	{
		m_Time += dt;
		if (!m_World || !m_Kernel.GetShown() || m_Kernel.GetScreen().IsHome())
			return;
		if (m_Time < m_CheckAt)
			return;
		m_CheckAt = m_Time + 0.5;
		int y, mo, d, h, mi;
		GetGame().GetWorld().GetDate(y, mo, d, h, mi);
		Weather we = GetGame().GetWeather();
		bool drift = mo != m_Month || d != m_Day || h * 60 + mi != m_Minute;
		if (we && m_Snow > 0 && we.GetSnowfall().GetActual() < 0.001)
		{
			if (m_SnowAskedAt < 0)
				m_SnowAskedAt = m_Time;
			else if (m_Time - m_SnowAskedAt > 2 && !m_SnowRefused)
			{
				m_SnowRefused = true;
				DZS_Log.Info("environment: no snowfall on " + m_WorldName);
			}
		}
		else if (we && we.GetSnowfall().GetActual() > 0.001)
			m_SnowAskedAt = -1;
		if (we)
		{
			drift = drift || Math.AbsFloat(we.GetOvercast().GetActual() - m_Overcast / 100.0) > 0.005;
			drift = drift || Math.AbsFloat(we.GetRain().GetActual() - m_Rain / 100.0) > 0.005;
			drift = drift || Math.AbsFloat(we.GetFog().GetActual() - m_Fog / 100.0) > 0.005;
			if (MapShowsSnow())
				drift = drift || Math.AbsFloat(we.GetSnowfall().GetActual() - m_Snow / 100.0) > 0.005;
		}
		if (m_Time - m_LogAt > 60 || drift)
		{
			m_LogAt = m_Time;
			string game = "game " + h + ":" + mi + " " + d + "." + mo + ".";
			if (we)
			{
				game = game + " clouds " + we.GetOvercast().GetActual() + " rain " + we.GetRain().GetActual();
				game = game + " fog " + we.GetFog().GetActual() + " snow " + we.GetSnowfall().GetActual();
			}
			DZS_Log.Info("environment: check " + game + " drift " + DZS_Str.Bool(drift));
		}
		if (drift)
			Apply("hold");
	}
}

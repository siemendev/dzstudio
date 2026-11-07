// Language table, named colours, fonts, scale and number text: the shared look of every surface.
class DZS_Texts
{
	protected static ref map<string, ref array<string>> s_Table;
	protected static int s_German = -1;

	static bool IsGerman()
	{
		if (s_German < 0)
		{
			s_German = 0;
			GameOptions opts = new GameOptions();
			ListOptionsAccess lang = ListOptionsAccess.Cast(opts.GetOptionByType(OptionAccessType.AT_OPTIONS_LANGUAGE));
			if (lang)
			{
				string t;
				lang.GetItemText(lang.GetIndex(), t);
				t.ToLower();
				if (t.Contains("_de") || t.Contains("deutsch") || t.Contains("german"))
					s_German = 1;
			}
		}
		return s_German == 1;
	}

	static void Add(string key, string de, string en)
	{
		if (!s_Table)
			s_Table = new map<string, ref array<string>>();
		array<string> pair = {de, en};
		s_Table.Set(key, pair);
	}

	// Missing keys come back as the key itself, so a gap is visible instead of silently empty.
	static string Get(string key)
	{
		if (!s_Table || !s_Table.Contains(key))
			return key;
		array<string> pair = s_Table.Get(key);
		if (IsGerman())
			return pair[0];
		return pair[1];
	}

	// Replaces %1, %2 … in a table text.
	static string Fmt(string key, string a1 = "", string a2 = "", string a3 = "")
	{
		string s = Get(key);
		s.Replace("%1", a1);
		s.Replace("%2", a2);
		s.Replace("%3", a3);
		return s;
	}

	// A table text in one language (operation names carry both).
	static string FmtIn(string key, bool german, string a1 = "", string a2 = "")
	{
		string s = key;
		if (s_Table && s_Table.Contains(key))
		{
			array<string> pair = s_Table.Get(key);
			if (german)
				s = pair[0] + "";
			else
				s = pair[1] + "";
		}
		s.Replace("%1", a1);
		s.Replace("%2", a2);
		return s;
	}

	// The bundled reading font draws no ASCII apostrophe in DayZ 1.30; names from the game show the typographic one.
	static string Shown(string s)
	{
		string r = s;
		r.Replace("'", "’");
		return r;
	}

	// Capitals for labels, including the German umlauts (the game's ToUpper knows ASCII only).
	static string Upper(string s)
	{
		string r = s;
		r.ToUpper();
		r.Replace("ä", "Ä");
		r.Replace("ö", "Ö");
		r.Replace("ü", "Ü");
		return r;
	}

	// Returns a fresh string: returning the parameter itself let several Pick calls in one argument list
	// all end up with the text of the last call (seen in DayZ 1.30).
	static string Pick(string de, string en)
	{
		string r;
		if (IsGerman())
			r = de + "";
		else
			r = en + "";
		return r;
	}
}

class DZS_Color
{
	protected static ref map<string, int> s_Colors;

	protected static void Init()
	{
		s_Colors = new map<string, int>();
		s_Colors.Set("surface", ARGB(DarkAlpha(245), 0x18, 0x18, 0x1B));
		s_Colors.Set("bar", ARGB(DarkAlpha(190), 0x18, 0x18, 0x1B));
		s_Colors.Set("surfaceRaised", ARGB(255, 0x27, 0x27, 0x2A));
		s_Colors.Set("surfacePressed", ARGB(255, 0x3F, 0x3F, 0x46));
		s_Colors.Set("well", ARGB(255, 0x09, 0x09, 0x0B));
		s_Colors.Set("plate", ARGB(DarkAlpha(235), 0x09, 0x09, 0x0B));
		s_Colors.Set("dim", ARGB(DarkAlpha(153), 0, 0, 0));
		s_Colors.Set("line", ARGB(255, 0x27, 0x27, 0x2A));
		s_Colors.Set("lineStrong", ARGB(255, 0x3F, 0x3F, 0x46));
		s_Colors.Set("capBorder", ARGB(255, 0x52, 0x52, 0x5B));
		s_Colors.Set("blockEmpty", ARGB(255, 0x27, 0x27, 0x2A));
		s_Colors.Set("text", ARGB(255, 0xE4, 0xE4, 0xE7));
		s_Colors.Set("textMuted", ARGB(255, 0xA1, 0xA1, 0xAA));
		s_Colors.Set("textQuiet", ARGB(255, 0x71, 0x71, 0x7A));
		s_Colors.Set("textOff", ARGB(255, 0x52, 0x52, 0x5B));
		s_Colors.Set("accent", ARGB(255, 0xA7, 0x8B, 0xFA));
		s_Colors.Set("accentText", ARGB(255, 0xC4, 0xB5, 0xFD));
		s_Colors.Set("accentTint", ARGB(TintAlpha(0x8B5CF6, 38, 0x18181B), 0x8B, 0x5C, 0xF6));
		s_Colors.Set("success", ARGB(255, 0x34, 0xD3, 0x99));
		s_Colors.Set("warning", ARGB(255, 0xFB, 0xBF, 0x24));
		s_Colors.Set("danger", ARGB(255, 0xF8, 0x71, 0x71));
		s_Colors.Set("info", ARGB(255, 0x38, 0xBD, 0xF8));
		s_Colors.Set("axisX", ARGB(255, 0xEF, 0x44, 0x44));
		s_Colors.Set("axisY", ARGB(255, 0x22, 0xC5, 0x5E));
		s_Colors.Set("axisZ", ARGB(255, 0x3B, 0x82, 0xF6));
		s_Colors.Set("imageBg", ARGB(255, 0xD4, 0xD4, 0xD8));
	}

	static int Get(string name)
	{
		if (!s_Colors)
			Init();
		int c;
		if (!s_Colors.Find(name, c))
		{
			DZS_Log.Warn("unknown colour '" + name + "'");
			return ARGB(255, 255, 0, 255);
		}
		return c;
	}

	// Status tints: the status colour at 15 % (fill) or 40 % (border), seen over the colour named by over.
	static int WithAlpha(string name, int alpha, string over = "surface")
	{
		int c = Get(name);
		if (name == "surface" || name == "plate" || name == "dim")
			alpha = DarkAlpha(alpha);
		else
			alpha = TintAlpha(c & 0x00FFFFFF, alpha, Get(over) & 0x00FFFFFF);
		return (c & 0x00FFFFFF) | (alpha << 24);
	}

	// A coloured tint over a dark background looks far stronger in linear light (15 % accent on #18181B looks
	// like 40 %). Returns the alpha whose linear blend over bg comes closest to the sRGB blend at the given alpha.
	static int TintAlpha(int rgb, int alpha, int bg)
	{
		if (!s_Tints)
			s_Tints = new map<string, int>();
		string key = rgb.ToString() + ":" + alpha.ToString() + ":" + bg.ToString();
		int found;
		if (s_Tints.Find(key, found))
			return found;
		float a = alpha / 255.0;
		float lo = 0;
		float hi = 1;
		for (int it = 0; it < 24; it++)
		{
			float mid = (lo + hi) * 0.5;
			float got = 0;
			float want = 0;
			for (int ch = 0; ch < 3; ch++)
			{
				int sh = ch * 8;
				int ci = (rgb >> sh) & 255;
				int bi = (bg >> sh) & 255;
				float cs = ci / 255.0;
				float bs = bi / 255.0;
				float wgt = Math.AbsFloat(cs - bs);
				got += wgt * Math.Pow(mid * Math.Pow(cs, 2.2) + (1 - mid) * Math.Pow(bs, 2.2), 1 / 2.2);
				want += wgt * (a * cs + (1 - a) * bs);
			}
			if (got < want)
				lo = mid;
			else
				hi = mid;
		}
		int r = Math.Clamp(Math.Round(255 * (lo + hi) * 0.5), 0, 255);
		s_Tints.Set(key, r);
		return r;
	}

	protected static ref map<string, int> s_Tints;

	// DayZ blends widgets in linear light: black at alpha a lets (1 - a) of the light through, which looks like
	// (1 - a)^(1/2.2). For a (near) black cover the alpha that looks like the given opacity is 1 - (1 - a)^2.2.
	static int DarkAlpha(int alpha)
	{
		float rest = 1 - alpha / 255.0;
		return Math.Clamp(Math.Round(255 * (1 - Math.Pow(rest, 2.2))), 0, 255);
	}
}

// Font roles, bundled as bitmap fonts (05_FontData.c). The game draws a bitmap font
// in the largest size whose line is lower than the widget, so a text widget gets exactly line + 1 px.
class DZS_Font
{
	static const string DISPLAY = "display";
	static const string LABEL = "label";
	static const string LABELBOLD = "labelbold";
	static const string MONO = "mono";
	static const string MENU = "menu";
	static const string READ = "read";
	static const string READBOLD = "readbold";

	protected static ref map<string, ref array<int>> s_Sizes;
	protected static ref map<string, ref array<int>> s_Lines;
	protected static ref map<string, ref array<int>> s_Ascents;

	// One size per role: every extra size is another atlas in the game's font store (see build_assets.py).
	static float Canon(string role)
	{
		if (role == DISPLAY)
			return 24;
		if (role == LABEL)
			return 11;
		if (role == LABELBOLD)
			return 10;
		if (role == MONO)
			return 12;
		if (role == MENU)
			return 14;
		return 13;
	}

	protected static int Index(string role, float refSize)
	{
		refSize = Canon(role);
		if (!s_Sizes)
		{
			s_Sizes = new map<string, ref array<int>>();
			s_Lines = new map<string, ref array<int>>();
			s_Ascents = new map<string, ref array<int>>();
			DZS_FontData.Fill(s_Sizes, s_Lines, s_Ascents);
		}
		array<int> sizes = s_Sizes.Get(role);
		if (!sizes)
			return -1;
		int px = DZS_Scale.FontPx(refSize);
		int best = 0;
		for (int i = 0; i < sizes.Count(); i++)
		{
			if (sizes[i] <= px)
				best = i;
		}
		return best;
	}

	// Widget height that makes the game pick the size for refSize at the current scale.
	static int WidgetHeight(string role, float refSize)
	{
		int i = Index(role, refSize);
		if (i < 0)
			return DZS_Scale.FontPx(refSize);
		return s_Lines.Get(role)[i] + 1;
	}

	static int PixelSize(string role, float refSize)
	{
		int i = Index(role, refSize);
		if (i < 0)
			return DZS_Scale.FontPx(refSize);
		return s_Sizes.Get(role)[i];
	}
}

// Reference pixels (1920 × 1080) to screen pixels.
class DZS_Scale
{
	protected static float s_Forced = -1;

	// -dzsScale=F (tests): a window of 2560 × 1440 with F = 1.5 lays out exactly like 1280 × 720 (factor 0.75), at twice
	// the pixels.
	static float Factor()
	{
		if (s_Forced < 0)
		{
			s_Forced = 0;
			string arg;
			if (GetCLIParam("dzsScale", arg) && arg.ToFloat() > 0)
				s_Forced = arg.ToFloat();
		}
		if (s_Forced > 0)
			return s_Forced;
		int w, h;
		GetScreenSize(w, h);
		float f = h / 1080.0;
		return Math.Clamp(f, 0.75, 2.0);
	}

	static int Px(float refPx)
	{
		return Math.Round(refPx * Factor());
	}

	// Font sizes never below 8 screen pixels.
	static int FontPx(float refPx)
	{
		return Math.Max(8, Math.Round(refPx * Factor()));
	}
}

class DZS_Num
{
	// Fixed decimals, no thousands separator; comma in German, point in English (or forced point for clipboards).
	static string Fixed(float v, int decimals, bool forcePoint = false)
	{
		string sep = ".";
		if (!forcePoint && DZS_Texts.IsGerman())
			sep = ",";
		int mul = 1;
		for (int i = 0; i < decimals; i++)
			mul *= 10;
		bool neg = v < 0;
		float a = Math.AbsFloat(v);
		int whole = Math.Floor(a);
		float fracF = (a - whole) * mul;
		int frac = Math.Round(fracF);
		if (frac >= mul)
		{
			whole += 1;
			frac -= mul;
		}
		string s = whole.ToString();
		if (decimals > 0)
		{
			string f = frac.ToString();
			while (f.Length() < decimals)
				f = "0" + f;
			s += sep + f;
		}
		if (neg && (whole != 0 || frac != 0))
			s = "-" + s;
		return s;
	}

	// Whole numbers in messages: from 1000 in groups of three with a no-break space.
	static string Count(int n)
	{
		string digits = Math.AbsInt(n).ToString();
		string r = "";
		int len = digits.Length();
		for (int i = 0; i < len; i++)
		{
			if (i > 0 && (len - i) % 3 == 0 && len > 3)
				r += " ";
			r += digits.Get(i);
		}
		if (n < 0)
			r = "-" + r;
		return r;
	}

	// A number or a calculation (+ - * / and brackets), comma or point as decimal separator.
	static bool Parse(string text, out float result)
	{
		DZS_Calc c = new DZS_Calc(text);
		return c.Run(result);
	}
}

class DZS_Calc : Managed
{
	protected string m_S;
	protected int m_P;
	protected bool m_Bad;

	void DZS_Calc(string s)
	{
		string t = s;
		t.Replace(",", ".");
		m_S = t;
		m_P = 0;
	}

	bool Run(out float result)
	{
		SkipWs();
		if (m_P >= m_S.Length())
			return false;
		float v = Expr();
		SkipWs();
		if (m_Bad || m_P != m_S.Length())
			return false;
		if (v != v || v > 1e30 || v < -1e30)
			return false;
		result = v;
		return true;
	}

	protected void SkipWs()
	{
		while (m_P < m_S.Length() && m_S.Get(m_P) == " ")
			m_P++;
	}

	protected string Peek()
	{
		SkipWs();
		if (m_P >= m_S.Length())
			return "";
		return m_S.Get(m_P);
	}

	protected float Expr()
	{
		float v = Term();
		while (!m_Bad)
		{
			string c = Peek();
			if (c == "+")
			{
				m_P++;
				v += Term();
			}
			else if (c == "-")
			{
				m_P++;
				v -= Term();
			}
			else
				break;
		}
		return v;
	}

	protected float Term()
	{
		float v = Factor();
		while (!m_Bad)
		{
			string c = Peek();
			if (c == "*")
			{
				m_P++;
				v *= Factor();
			}
			else if (c == "/")
			{
				m_P++;
				float d = Factor();
				if (d == 0)
				{
					m_Bad = true;
					return 0;
				}
				v /= d;
			}
			else
				break;
		}
		return v;
	}

	protected float Factor()
	{
		string c = Peek();
		if (c == "-")
		{
			m_P++;
			return -Factor();
		}
		if (c == "(")
		{
			m_P++;
			float v = Expr();
			if (Peek() != ")")
			{
				m_Bad = true;
				return 0;
			}
			m_P++;
			return v;
		}
		int start = m_P;
		bool dot = false;
		int digits = 0;
		while (m_P < m_S.Length())
		{
			string ch = m_S.Get(m_P);
			if (ch == ".")
			{
				if (dot)
					break;
				dot = true;
			}
			else if (ch.ToAscii() >= 48 && ch.ToAscii() <= 57)
				digits++;
			else
				break;
			m_P++;
		}
		if (digits == 0)
		{
			m_Bad = true;
			return 0;
		}
		return m_S.Substring(start, m_P - start).ToFloat();
	}
}

// Key names: bindings are stored by position (game key code without "KC_"), shown as the label of the
// standard keyboard of the UI language.
class DZS_KeyNames
{
	protected static ref map<string, ref array<string>> s_Names;

	protected static void Name(string key, string de, string en)
	{
		array<string> pair = {de, en};
		s_Names.Set(key, pair);
	}

	static string Display(string keyName)
	{
		if (!s_Names)
		{
			s_Names = new map<string, ref array<string>>();
			Name("Ctrl", "Strg", "Ctrl");
			Name("Shift", "Umschalt", "Shift");
			Name("Alt", "Alt", "Alt");
			Name("LSHIFT", "Umschalt links", "Left Shift");
			Name("RSHIFT", "Umschalt rechts", "Right Shift");
			Name("LCONTROL", "Strg links", "Left Ctrl");
			Name("RCONTROL", "Strg rechts", "Right Ctrl");
			Name("LMENU", "Alt links", "Left Alt");
			Name("RMENU", "Alt Gr", "Right Alt");
			Name("SPACE", "Leertaste", "Space");
			Name("ESCAPE", "Esc", "Esc");
			Name("RETURN", "Enter", "Enter");
			Name("NUMPADENTER", "Ziffernblock Enter", "Numpad Enter");
			Name("BACK", "Rück", "Backspace");
			Name("TAB", "Tab", "Tab");
			Name("DELETE", "Entf", "Del");
			Name("INSERT", "Einfg", "Ins");
			Name("HOME", "Pos1", "Home");
			Name("END", "Ende", "End");
			Name("PRIOR", "Bild↑", "PgUp");
			Name("NEXT", "Bild↓", "PgDn");
			Name("UP", "↑", "↑");
			Name("DOWN", "↓", "↓");
			Name("LEFT", "←", "←");
			Name("RIGHT", "→", "→");
			Name("CAPITAL", "Feststell", "Caps Lock");
			Name("NUMLOCK", "Num", "Num Lock");
			Name("SCROLL", "Rollen", "Scroll Lock");
			Name("SYSRQ", "Druck", "Print");
			Name("PAUSE", "Pause", "Pause");
			Name("LWIN", "Windows links", "Left Windows");
			Name("RWIN", "Windows rechts", "Right Windows");
			Name("APPS", "Menü", "Menu");
			Name("ADD", "Ziffernblock +", "Numpad +");
			Name("SUBTRACT", "Ziffernblock −", "Numpad −");
			Name("MULTIPLY", "Ziffernblock *", "Numpad *");
			Name("DIVIDE", "Ziffernblock /", "Numpad /");
			Name("DECIMAL", "Ziffernblock ,", "Numpad .");
			for (int n = 0; n <= 9; n++)
				Name("NUMPAD" + n, "Ziffernblock " + n, "Numpad " + n);
			Name("GRAVE", "^", "`");
			Name("MINUS", "ß", "-");
			Name("EQUALS", "´", "=");
			Name("LBRACKET", "Ü", "[");
			Name("RBRACKET", "+", "]");
			Name("SEMICOLON", "Ö", ";");
			Name("APOSTROPHE", "Ä", "'");
			Name("BACKSLASH", "#", "\\");
			Name("COMMA", "Komma", ",");
			Name("PERIOD", "Punkt", ".");
			Name("SLASH", "-", "/");
			Name("OEM_102", "<", "\\");
			Name("MouseRight", "Rechte Maustaste", "Right mouse");
			Name("MouseMiddle", "Mittlere Maustaste", "Middle mouse");
			Name("MouseLeft", "Linke Maustaste", "Left mouse");
			Name("Mouse4", "Maustaste 4", "Mouse 4");
			Name("Mouse5", "Maustaste 5", "Mouse 5");
			Name("Wheel", "Mausrad", "Wheel");
			Name("Y", "Z", "Y");
			Name("Z", "Y", "Z");
		}
		array<string> pair = s_Names.Get(keyName);
		if (!pair)
			return keyName;
		if (DZS_Texts.IsGerman())
			return pair[0];
		return pair[1];
	}

	// "Ctrl+Shift+Y" → ["Strg", "Umschalt", "Z"]
	static array<string> Caps(string binding)
	{
		array<string> parts = new array<string>();
		binding.Split("+", parts);
		array<string> r = new array<string>();
		foreach (string p : parts)
			r.Insert(Display(p));
		return r;
	}
}

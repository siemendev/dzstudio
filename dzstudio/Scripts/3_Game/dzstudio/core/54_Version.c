// The one version of dzStudio (MAJOR.MINOR.PATCH, each part 0–999 without leading zeros), what servers say about it,
// the dialog for a version a server no longer accepts and the "new version" message.
class DZS_OutdatedServer : Managed
{
	string m_Name;
	string m_Min;
	string m_Url;
}

class DZS_NewVersionListener : DZS_MessageListener
{
	string m_Url;

	override void OnMessageButton(string kind)
	{
		if (m_Url == "")
			return;
		GetGame().OpenURL(m_Url);
		DZS_Log.Info("version: workshop page opened from the message");
	}
}

class DZS_Version
{
	// per game start: servers that refused this version (key → what they said), servers whose dialog came on its own
	protected static ref map<string, ref DZS_OutdatedServer> s_Blocked;
	protected static ref map<string, bool> s_AutoShown;
	protected static bool s_NewShown;
	protected static bool s_ConfigChecked;
	protected static ref DZS_NewVersionListener s_NewListener;

	// -dzsFakeVersion=0.3.9 (developer): act as another version towards servers and on screen
	static string Get()
	{
		string v;
		if (GetCLIParam("dzsFakeVersion", v) && v != "")
			return v;
		return DZS_VERSION;
	}

	static string Label()
	{
		return "dzStudio " + Get();
	}

	static bool Valid(string v)
	{
		array<string> parts = new array<string>();
		v.Split(".", parts);
		if (parts.Count() != 3 || v.Length() > 11)
			return false;
		foreach (string p : parts)
		{
			int n = p.Length();
			if (n < 1 || n > 3)
				return false;
			for (int i = 0; i < n; i++)
			{
				int c = p.Get(i).ToAscii();
				if (c < 48 || c > 57)
					return false;
			}
			if (n > 1 && p.Get(0) == "0")
				return false;
		}
		return true;
	}

	protected static int Part(string v, int i)
	{
		array<string> parts = new array<string>();
		v.Split(".", parts);
		if (i >= parts.Count())
			return 0;
		return parts[i].ToInt();
	}

	// Part by part as numbers: 0.10.0 is newer than 0.9.3.
	static int Compare(string a, string b)
	{
		for (int i = 0; i < 3; i++)
		{
			int x = Part(a, i);
			int y = Part(b, i);
			if (x != y)
			{
				if (x < y)
					return -1;
				return 1;
			}
		}
		return 0;
	}

	// Is the own version below this minimum? An invalid minimum demands nothing.
	static bool Below(string min)
	{
		if (!Valid(min))
			return false;
		if (!Valid(Get()))
			return true;
		return Compare(Get(), min) < 0;
	}

	static void CheckConfig()
	{
		if (s_ConfigChecked)
			return;
		s_ConfigChecked = true;
		string c;
		GetGame().ConfigGetText("CfgMods dzstudio version", c);
		if (c != DZS_VERSION)
			DZS_Log.Error("version: config.cpp says \"" + c + "\", DZS_VERSION is \"" + DZS_VERSION + "\"");
		else
			DZS_Log.Info("version " + DZS_VERSION + " (config.cpp the same), running as " + Get());
	}

	// The server's workshop address when it is https:// and at most 1000 bytes, else the built-in one.
	static string Url(string fromServer)
	{
		if (fromServer.IndexOf("https://") == 0 && fromServer.Length() <= 1000)
			return fromServer;
		return DZS_WORKSHOP_URL;
	}

	static bool IsBlocked(string key)
	{
		return s_Blocked && s_Blocked.Contains(key);
	}

	static bool AnyBlocked()
	{
		return s_Blocked && s_Blocked.Count() > 0;
	}

	// A server refused this version (or demands more in its info): no more requests to it until the game restarts.
	// The dialog comes on its own once per server and game start; userAction shows it every time.
	static void Outdated(string key, string serverName, string min, string url, bool userAction)
	{
		DZS_OutdatedServer o = MarkOutdated(key, serverName, min, url);
		if (!s_AutoShown)
			s_AutoShown = new map<string, bool>();
		if (!userAction && s_AutoShown.Contains(key))
			return;
		s_AutoShown.Set(key, true);
		OpenDialog(o);
	}

	// Blocked without a dialog (a minimum in the server's info); the dialog comes when something needs the server.
	static DZS_OutdatedServer MarkOutdated(string key, string serverName, string min, string url)
	{
		if (!s_Blocked)
			s_Blocked = new map<string, ref DZS_OutdatedServer>();
		DZS_OutdatedServer o = new DZS_OutdatedServer();
		o.m_Name = serverName;
		o.m_Min = min;
		o.m_Url = url;
		if (!s_Blocked.Contains(key))
			DZS_Log.Info("version: " + serverName + " (" + key + ") needs " + min + ", own " + Get() + " — no more requests to it");
		s_Blocked.Set(key, o);
		return o;
	}

	// Something that needs a blocked server: the dialog again instead of a request. False when it is not blocked.
	static bool ShowIfBlocked(string key)
	{
		if (!IsBlocked(key))
			return false;
		OpenDialog(s_Blocked.Get(key));
		return true;
	}

	protected static void OpenDialog(DZS_OutdatedServer o)
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return;
		k.GetScreen().HideMessage("version.new");
		DZS_OutdatedDialog d = new DZS_OutdatedDialog(o.m_Name, Get(), o.m_Min, Url(o.m_Url));
		d.Open();
	}

	// A server or the dzbot plugin recommends a newer version: one message per game start, from whoever says it first.
	static void Recommended(string v, string url)
	{
		if (s_NewShown || !Valid(v) || Compare(v, Get()) <= 0)
			return;
		if (AnyBlocked())
			return;
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return;
		s_NewShown = true;
		string u = Url(url);
		string text = DZS_Texts.Pick("Neue Version " + v + " verfügbar", "New version " + v + " available");
		if (u == "")
		{
			k.GetScreen().ShowMessage("version.new", "info", text, 4);
			return;
		}
		s_NewListener = new DZS_NewVersionListener();
		s_NewListener.m_Url = u;
		k.GetScreen().ShowMessageWithButton("version.new", "info", text, 8, DZS_Texts.Pick("Zur Workshop-Seite", "Go to Workshop page"), s_NewListener);
	}
}

class DZS_OutdatedDialog : DZS_Dialog
{
	protected string m_Server;
	protected string m_Yours;
	protected string m_Min;
	protected string m_Url;
	protected ref array<TextWidget> m_Lines = new array<TextWidget>();
	protected ref array<float> m_Gaps = new array<float>();
	protected TextWidget m_Opened;

	void DZS_OutdatedDialog(string server, string yours, string min, string url)
	{
		m_Server = server;
		m_Yours = yours;
		m_Min = min;
		m_Url = url;
		Setup(SMALL, server, DZS_Texts.Pick("dzStudio ist veraltet", "dzStudio is out of date"), false);
		string closeLabel = DZS_Texts.Pick("Schließen", "Close");
		if (m_Url != "")
		{
			AddFootButton("copy", DZS_Texts.Pick("Link kopieren", "Copy link"), "side");
			AddFootButton("close", closeLabel, "side");
			AddFootButton("open", DZS_Texts.Pick("Zur Workshop-Seite", "Go to Workshop page"), "main");
		}
		else
			AddFootButton("close", closeLabel, "main");
	}

	override void BuildContent(Widget inner)
	{
		string y = m_Yours;
		if (y == "")
			y = "?";
		string main = DZS_Texts.Pick("Deine Version " + y + ", nötig ist " + m_Min + " oder neuer. Bitte aktualisiere dzStudio.", "Your version " + y + ", required is " + m_Min + " or newer. Please update dzStudio.");
		string quiet = DZS_Texts.Pick("Bis zum Neustart spricht dzStudio nicht mehr mit " + m_Server + ". Lokal arbeitest du normal weiter.", "Until you restart, dzStudio won’t talk to " + m_Server + " any more. You can keep working locally.");
		Wrap(inner, main, "text", 0);
		Wrap(inner, quiet, "textMuted", DZS_Scale.Px(10));
		m_Opened = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get("text"));
		m_Opened.SetText(DZS_W.Literal(DZS_Texts.Pick("Im Browser geöffnet — mit Alt+Tab wechseln", "Opened in the browser — switch with Alt+Tab")));
		m_Opened.Show(false);
		AddFootFocus();
	}

	protected void Wrap(Widget inner, string text, string color, float gapBefore)
	{
		float room = DZS_Scale.Px(520);
		TextWidget probe = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get(color));
		array<string> words = new array<string>();
		text.Split(" ", words);
		string line = "";
		float gap = gapBefore;
		foreach (string w : words)
		{
			string tryLine = w;
			if (line != "")
				tryLine = line + " " + w;
			probe.SetText(DZS_W.Literal(tryLine));
			if (line != "" && DZS_W.TextWidth(probe) > room)
			{
				AddLine(inner, line, color, gap);
				gap = 0;
				line = w;
			}
			else
				line = tryLine;
		}
		if (line != "")
			AddLine(inner, line, color, gap);
		probe.Unlink();
	}

	protected void AddLine(Widget inner, string text, string color, float gap)
	{
		TextWidget t = DZS_W.Text(inner, DZS_Font.READ, 14, DZS_Color.Get(color));
		t.SetText(DZS_W.Literal(text));
		m_Lines.Insert(t);
		m_Gaps.Insert(gap);
	}

	override float BoxWidth()
	{
		float w = DZS_Scale.Px(420);
		foreach (TextWidget t : m_Lines)
			w = Math.Max(w, DZS_W.TextWidth(t) + DZS_Scale.Px(56));
		w = Math.Max(w, DZS_W.TextWidth(m_Title) + DZS_Scale.Px(96));
		return Math.Min(w, DZS_Scale.Px(640));
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		for (int i = 0; i < m_Lines.Count(); i++)
		{
			y += m_Gaps[i];
			DZS_W.Place(m_Lines[i], 0, y, width, DZS_Scale.Px(24));
			y += DZS_Scale.Px(22);
		}
		if (m_Opened && m_Opened.IsVisible())
		{
			y += DZS_Scale.Px(10);
			DZS_W.Place(m_Opened, 0, y, width, DZS_Scale.Px(24));
			y += DZS_Scale.Px(22);
		}
		return y + DZS_Scale.Px(2);
	}

	override void OnFoot(string tag)
	{
		if (tag == "open")
		{
			GetGame().OpenURL(m_Url);
			m_Opened.Show(true);
			Layout();
			DZS_Log.Info("version: workshop page opened from the dialog: " + m_Url);
			return;
		}
		if (tag == "copy")
		{
			GetGame().CopyToClipboard(m_Url);
			DZS_Kernel.Get().GetScreen().ShowMessage("version.copied", "success", DZS_Texts.Pick("Kopiert", "Copied"), 2);
			DZS_Log.Info("version: link copied: " + m_Url);
			return;
		}
		super.OnFoot(tag);
	}

	override void OnClosed()
	{
		DZS_Log.Info("version: dialog closed");
	}
}

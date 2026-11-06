// Requests to dzbot as the game allows them: GET only for the public catalogue,
// everything else POST with a frame of sections; every answer starts with "dzstudio <version>" and ends with
// "end - - 0". An answer without the first line or with a section shorter than announced is not taken.
class DZS_DzbotSection : Managed
{
	static const int PIECE = 4000;
	string m_Kind;
	string m_Key;
	string m_Rev;
	// outgoing: the whole body; incoming: pieces of at most PIECE bytes (Substring cuts at 8191 bytes)
	string m_Body;
	ref array<string> m_Chunks;
	int m_Len;

	static DZS_DzbotSection Make(string kind, string key, string rev, string body)
	{
		DZS_DzbotSection s = new DZS_DzbotSection();
		s.m_Kind = kind;
		s.m_Key = key;
		s.m_Rev = rev;
		s.m_Chunks = new array<string>();
		s.m_Chunks.Insert(body);
		return s;
	}

	static DZS_DzbotSection FromChunks(string kind, string key, string rev, array<string> chunks)
	{
		DZS_DzbotSection s = new DZS_DzbotSection();
		s.m_Kind = kind;
		s.m_Key = key;
		s.m_Rev = rev;
		s.m_Chunks = chunks;
		return s;
	}
}

class DZS_DzbotReply : Managed
{
	bool m_Framed;
	string m_Fail;
	string m_Version;
	ref DZS_SrcNode m_Json;
	ref array<ref DZS_DzbotSection> m_Sections = new array<ref DZS_DzbotSection>();
	int m_Bytes;
	bool m_TimedOut;

	bool Ok()
	{
		if (!m_Framed || !m_Json)
			return false;
		DZS_SrcNode ok = m_Json.Get("ok");
		return ok && ok.IsBool() && ok.m_Bool;
	}

	string Str(string key)
	{
		if (!m_Json)
			return "";
		DZS_SrcNode n = m_Json.Get(key);
		if (!n)
			return "";
		if (n.IsStr() || n.IsNum())
			return n.m_Str;
		return "";
	}

	int Int(string key)
	{
		string s = Str(key);
		return s.ToInt();
	}

	string Error() { return Str("error"); }

	string Message()
	{
		string m = Str("message");
		if (m != "")
			return m;
		return m_Fail;
	}

	DZS_DzbotSection Find(string kind, string key)
	{
		foreach (DZS_DzbotSection s : m_Sections)
		{
			if (s.m_Kind == kind && s.m_Key == key)
				return s;
		}
		return null;
	}
}

class DZS_DzbotFrame
{
	static const string MAGIC = "dzstudio";
	static const int VERSION = 1;

	// -dzsDzbotExpect=N (developer): expect another major version, to see the "update dzStudio" path
	static int Expected()
	{
		string v;
		if (GetCLIParam("dzsDzbotExpect", v))
			return v.ToInt();
		return VERSION;
	}

	// Request: first line, the json section, the given sections, the end section. Lengths in bytes.
	// The sections after the json one as pieces in frame order, for DZS_Joiner (Build with tail).
	static array<string> TailPieces(array<ref DZS_DzbotSection> parts)
	{
		array<string> r = new array<string>();
		foreach (DZS_DzbotSection p : parts)
		{
			int bytes = DZS_Files.Length(p.m_Chunks);
			string bl = bytes.ToString();
			string head = p.m_Kind + " " + p.m_Key + " " + p.m_Rev + " " + bl + "\n";
			r.Insert(head);
			foreach (string c : p.m_Chunks)
				r.Insert(c);
			r.Insert("\n");
		}
		r.Insert("end - - 0\n");
		return r;
	}

	// Every request names the version of dzStudio as the first field of its json section.
	static string WithVersion(string json)
	{
		string v = "\"clientVersion\":" + Q(DZS_Version.Get());
		if (json == "{}")
			return "{" + v + "}";
		return "{" + v + "," + json.Substring(1, json.Length() - 1);
	}

	static string Build(string json, array<ref DZS_DzbotSection> parts = null, string tail = "")
	{
		json = WithVersion(json);
		string jl = json.Length().ToString();
		string s = MAGIC + " " + VERSION.ToString() + "\n";
		s += "json - - " + jl + "\n" + json + "\n";
		if (tail != "")
			return s + tail;
		if (parts)
		{
			foreach (DZS_DzbotSection p : parts)
			{
				int bytes = DZS_Files.Length(p.m_Chunks);
				string bl = bytes.ToString();
				string head = p.m_Kind + " " + p.m_Key + " " + p.m_Rev + " " + bl + "\n";
				s += head;
				foreach (string c : p.m_Chunks)
					s += c;
				s += "\n";
			}
		}
		s += "end - - 0\n";
		return s;
	}

	static DZS_DzbotReply Parse(string data)
	{
		DZS_DzbotReply r = new DZS_DzbotReply();
		int total = data.Length();
		r.m_Bytes = total;
		int nl = data.IndexOf("\n");
		if (nl < 0)
		{
			r.m_Fail = "no status line";
			return r;
		}
		string first = data.Substring(0, nl);
		string magic = MAGIC + " ";
		if (first.IndexOf(magic) != 0)
		{
			r.m_Fail = "no status line";
			return r;
		}
		// same major version only (answers to section 11): "dzstudio 1" and "dzstudio 1.4" pass, "dzstudio 2" not
		string ver = first.Substring(magic.Length(), first.Length() - magic.Length());
		ver.TrimInPlace();
		string major = ver;
		int dot = ver.IndexOf(".");
		if (dot >= 0)
			major = ver.Substring(0, dot);
		if (major == "" || major.ToInt() != Expected() || major.ToInt().ToString() != major)
		{
			r.m_Fail = "version";
			r.m_Version = ver;
			return r;
		}
		int pos = nl + 1;
		while (true)
		{
			if (pos >= total)
			{
				r.m_Fail = "frame incomplete";
				return r;
			}
			int hl = data.IndexOfFrom(pos, "\n");
			if (hl < 0)
			{
				r.m_Fail = "frame incomplete";
				return r;
			}
			string head = data.Substring(pos, hl - pos);
			array<string> f = new array<string>();
			head.Split(" ", f);
			if (f.Count() != 4)
			{
				r.m_Fail = "Kopfzeile unlesbar: " + head;
				return r;
			}
			int len = f[3].ToInt();
			string lenText = len.ToString();
			if (len < 0 || lenText != f[3])
			{
				r.m_Fail = "length unreadable: " + head;
				return r;
			}
			int start = hl + 1;
			if (start + len > total)
			{
				r.m_Fail = "frame shorter than announced";
				return r;
			}
			if (f[0] == "end")
			{
				r.m_Framed = true;
				return r;
			}
			DZS_DzbotSection sec = new DZS_DzbotSection();
			sec.m_Kind = f[0];
			sec.m_Key = f[1];
			sec.m_Rev = f[2];
			sec.m_Len = len;
			sec.m_Chunks = new array<string>();
			int at = start;
			int end = start + len;
			while (at < end)
			{
				int want = Math.Min(DZS_DzbotSection.PIECE, end - at);
				string piece = data.Substring(at, want);
				if (at + want < end)
				{
					int cut = DZS_Utf8.SafeCut(piece, want);
					if (cut > 0 && cut < want)
					{
						piece = piece.Substring(0, cut);
						want = cut;
					}
				}
				sec.m_Chunks.Insert(piece);
				at += want;
			}
			if (len <= DZS_DzbotSection.PIECE && sec.m_Chunks.Count() == 1)
				sec.m_Body = sec.m_Chunks[0];
			else if (len == 0)
				sec.m_Body = "";
			r.m_Sections.Insert(sec);
			pos = start + len;
			if (pos < total && data.Substring(pos, 1) == "\n")
				pos++;
			if (sec.m_Kind == "json" && !r.m_Json)
			{
				DZS_SrcDoc doc = DZS_SrcJson.Parse(sec.m_Chunks);
				if (!doc || !doc.m_Root)
				{
					r.m_Fail = "JSON unlesbar";
					return r;
				}
				r.m_Json = doc.m_Root;
			}
		}
		return r;
	}

	static string Q(string s)
	{
		return DZS_TextForm.Str(s);
	}
}

// One request in flight. The callback object must live until the game answers.
class DZS_DzbotCall : RestCallback
{
	DZS_DzbotClient m_C;
	string m_Slot;
	string m_Tag;
	string m_Path;
	string m_Body;
	bool m_Get;
	bool m_Started;
	bool m_Dropped;
	float m_T0;
	ref Managed m_Data;

	override void OnSuccess(string data, int dataSize)
	{
		if (m_C)
			m_C.Answer(this, data, dataSize, "");
	}

	override void OnError(int errorCode)
	{
		if (m_C)
			m_C.Answer(this, "", 0, "error " + errorCode);
	}

	override void OnTimeout()
	{
		if (m_C)
			m_C.Answer(this, "", 0, "timeout");
	}
}

class DZS_DzbotListener : Managed
{
	void OnDzbot(DZS_DzbotCall call, DZS_DzbotReply reply) {}
}

// Slots: one for loading/saving/sign-in, one for polling, two for the catalogue; never more than 4
// open. While a save is open no catalogue request starts.
class DZS_DzbotClient : Managed
{
	static const string DEFAULT_URL = "https://api.dzbot.de";

	string m_Base;
	ref DZS_DzbotListener m_Listener;
	ref array<ref DZS_DzbotCall> m_Queue = new array<ref DZS_DzbotCall>();
	ref array<ref DZS_DzbotCall> m_Open = new array<ref DZS_DzbotCall>();
	bool m_SaveOpen;
	bool m_Blocked;
	int m_MaxOpenSeen;
	protected RestContext m_Ctx;

	void DZS_DzbotClient(string base, DZS_DzbotListener l)
	{
		m_Base = base;
		m_Listener = l;
	}

	protected RestContext Ctx()
	{
		if (m_Ctx)
			return m_Ctx;
		RestApi api = GetRestApi();
		if (!api)
			api = CreateRestApi();
		if (!api)
			return null;
		api.SetOption(ERestOption.ERESTOPTION_READOPERATION, 10);
		api.SetOption(ERestOption.ERESTOPTION_CONNECTION, 8);
		m_Ctx = api.GetRestContext(m_Base);
		return m_Ctx;
	}

	static int SlotLimit(string slot)
	{
		if (slot == "cat")
			return 2;
		return 1;
	}

	DZS_DzbotCall Post(string slot, string tag, string path, string body, Managed data = null)
	{
		DZS_DzbotCall c = new DZS_DzbotCall();
		c.m_C = this;
		c.m_Slot = slot;
		c.m_Tag = tag;
		c.m_Path = path;
		c.m_Body = body;
		c.m_Data = data;
		Enqueue(c);
		return c;
	}

	DZS_DzbotCall Get(string slot, string tag, string path, Managed data = null)
	{
		string sep = "?";
		if (path.IndexOf("?") >= 0)
			sep = "&";
		path += sep + "clientVersion=" + DZS_Version.Get();
		DZS_DzbotCall c = new DZS_DzbotCall();
		c.m_C = this;
		c.m_Slot = slot;
		c.m_Tag = tag;
		c.m_Path = path;
		c.m_Get = true;
		c.m_Data = data;
		Enqueue(c);
		return c;
	}

	protected void Enqueue(DZS_DzbotCall c)
	{
		if (m_Blocked)
		{
			DZS_Log.Info("dzbot: " + c.m_Tag + " not sent, dzStudio is out of date for this server");
			return;
		}
		if (c.m_Slot == "cat")
			m_Queue.Insert(c);
		else
			m_Queue.InsertAt(c, 0);
		Pump();
	}

	int OpenIn(string slot)
	{
		int n = 0;
		foreach (DZS_DzbotCall c : m_Open)
		{
			if (c.m_Slot == slot)
				n++;
		}
		return n;
	}

	int Waiting(string slot)
	{
		int n = 0;
		foreach (DZS_DzbotCall c : m_Queue)
		{
			if (c.m_Slot == slot)
				n++;
		}
		return n;
	}

	// The server refused this version: nothing more is sent until the game restarts.
	void Block()
	{
		m_Blocked = true;
		m_Queue.Clear();
		foreach (DZS_DzbotCall c : m_Open)
			c.m_Dropped = true;
	}

	// Drops waiting and open calls of a slot; their answers are ignored.
	void Drop(string slot, string tag = "")
	{
		for (int i = m_Queue.Count() - 1; i >= 0; i--)
		{
			if (m_Queue[i].m_Slot == slot && (tag == "" || m_Queue[i].m_Tag == tag))
				m_Queue.Remove(i);
		}
		foreach (DZS_DzbotCall c : m_Open)
		{
			if (c.m_Slot == slot && (tag == "" || c.m_Tag == tag))
				c.m_Dropped = true;
		}
	}

	void Pump()
	{
		RestContext ctx = Ctx();
		if (!ctx)
			return;
		int i = 0;
		while (i < m_Queue.Count())
		{
			DZS_DzbotCall c = m_Queue[i];
			if (c.m_Slot == "cat" && m_SaveOpen)
			{
				i++;
				continue;
			}
			if (OpenIn(c.m_Slot) >= SlotLimit(c.m_Slot) || m_Open.Count() >= 4)
			{
				i++;
				continue;
			}
			m_Queue.Remove(i);
			m_Open.Insert(c);
			if (m_Open.Count() > m_MaxOpenSeen)
				m_MaxOpenSeen = m_Open.Count();
			c.m_Started = true;
			c.m_T0 = GetGame().GetTickTime();
			if (c.m_Get)
				ctx.GET(c, c.m_Path);
			else
				ctx.POST(c, c.m_Path, c.m_Body);
			DZS_Log.Info("dzbot: " + c.m_Slot + " " + c.m_Tag + " " + c.m_Path + " sent, " + c.m_Body.Length() + " bytes, open " + m_Open.Count());
		}
	}

	void Answer(DZS_DzbotCall c, string data, int size, string err)
	{
		int at = m_Open.Find(c);
		if (at >= 0)
			m_Open.Remove(at);
		float secs = GetGame().GetTickTime() - c.m_T0;
		DZS_DzbotReply r;
		if (err != "")
		{
			r = new DZS_DzbotReply();
			r.m_Fail = err;
			r.m_TimedOut = err == "timeout";
		}
		else
			r = DZS_DzbotFrame.Parse(data);
		string res = "ok";
		if (!r.m_Framed)
			res = "not framed (" + r.m_Fail + ")";
		else if (!r.Ok())
			res = "error " + r.Error();
		int dl = data.Length();
		DZS_Log.Info("dzbot: " + c.m_Tag + " answered in " + DZS_Num.Fixed(secs, 2) + " s, " + size + " bytes (" + dl + " in the string), " + res);
		DZS_DzbotCall keep = c;
		if (!c.m_Dropped && m_Listener)
			m_Listener.OnDzbot(keep, r);
		Pump();
	}
}

// Base64 into bytes, written to a file in pieces (at most 4 ms per drawn frame in total).
class DZS_DzbotB64
{
	protected static ref map<string, int> s_Val;

	static int Val(string ch)
	{
		if (!s_Val)
		{
			s_Val = new map<string, int>();
			string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			for (int i = 0; i < 64; i++)
			{
				string c = abc.Substring(i, 1);
				s_Val.Set(c, i);
			}
		}
		int v;
		if (s_Val.Find(ch, v))
			return v;
		return -1;
	}
}

// The game has no script function to write raw bytes; FileSerializer writes an int as its 4 bytes (little-endian,
// checked in the game: 0x04030201 → 01 02 03 04). Images are therefore written in words of 4 bytes; a size that is not
// a multiple of 4 cannot be written exactly (DDS 128 × 128 × 32 bit: 65 664 bytes).
class DZS_DzbotImageJob : Managed
{
	string m_Key;
	string m_Path;
	string m_Tmp;
	ref array<string> m_Parts = new array<string>();
	int m_Part;
	int m_Char;
	protected ref FileSerializer m_Fs;
	protected int m_Word;
	protected int m_WordBytes;
	protected int m_Acc;
	protected int m_Bits;
	int m_Written;
	int m_Expect;
	bool m_Failed;

	bool Begin()
	{
		m_Tmp = m_Path + ".tmp";
		m_Fs = new FileSerializer();
		return m_Fs.Open(m_Tmp, FileMode.WRITE);
	}

	protected void PutByte(int b)
	{
		m_Word = m_Word | (b << (m_WordBytes * 8));
		m_WordBytes++;
		m_Written++;
		if (m_WordBytes == 4)
		{
			int w = m_Word;
			m_Fs.Write(w);
			m_Word = 0;
			m_WordBytes = 0;
		}
	}

	// Decodes until the budget (TickCount units) is used; true when done.
	bool Step(int ticks)
	{
		int t0 = TickCount(0);
		while (m_Part < m_Parts.Count())
		{
			string s = m_Parts[m_Part];
			int n = s.Length();
			while (m_Char < n)
			{
				if (TickCount(0) - t0 > ticks)
					return false;
				int stop = Math.Min(m_Char + 256, n);
				while (m_Char < stop)
				{
					string ch = s.Substring(m_Char, 1);
					m_Char++;
					if (ch == "=")
						continue;
					int v = DZS_DzbotB64.Val(ch);
					if (v < 0)
						continue;
					m_Acc = ((m_Acc << 6) | v) & 16777215;
					m_Bits += 6;
					if (m_Bits >= 8)
					{
						m_Bits -= 8;
						PutByte((m_Acc >> m_Bits) & 255);
					}
				}
			}
			m_Part++;
			m_Char = 0;
		}
		if (m_WordBytes != 0)
			m_Failed = true;
		m_Fs.Close();
		m_Fs = null;
		DeleteFile(m_Path);
		if (m_Failed || !CopyFile(m_Tmp, m_Path))
			m_Failed = true;
		DeleteFile(m_Tmp);
		return true;
	}
}

// Joins many pieces into one string over several frames: neighbours pairwise, level by level, so each byte is copied
// about log2(n) times instead of once per piece appended (5 MB in 1340 pieces took 4.3 s appended in one frame).
class DZS_Joiner : Managed
{
	protected ref array<string> m_Cur = new array<string>();
	protected ref array<string> m_Next = new array<string>();
	protected int m_At;

	void DZS_Joiner(array<string> pieces)
	{
		foreach (string p : pieces)
			m_Cur.Insert(p);
		if (m_Cur.Count() == 0)
			m_Cur.Insert("");
	}

	bool Step(int ms)
	{
		int t0 = TickCount(0);
		while (m_Cur.Count() > 1)
		{
			while (m_At < m_Cur.Count())
			{
				string a = m_Cur[m_At];
				if (m_At + 1 < m_Cur.Count())
				{
					string b = m_Cur[m_At + 1];
					string ab = a + b;
					m_Next.Insert(ab);
				}
				else
					m_Next.Insert(a);
				m_At += 2;
				if ((TickCount(0) - t0) / 10000 >= ms)
					return false;
			}
			m_Cur = m_Next;
			m_Next = new array<string>();
			m_At = 0;
		}
		return true;
	}

	string Result()
	{
		return m_Cur[0];
	}
}

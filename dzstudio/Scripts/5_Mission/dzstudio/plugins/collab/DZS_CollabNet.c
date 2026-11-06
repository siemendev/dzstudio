// Transport of the collaboration protocol: at most two requests at a
// time, the one poll and one send channel that sends everything else in order. Every answer starts with
// "dzcollab 1"; without it, or with JSON that does not parse, it counts as "Server not reachable".
class DZS_CollabReply : Managed
{
	bool m_Framed;
	string m_Fail;
	ref DZS_SrcDoc m_Doc;
	DZS_SrcNode m_J;

	bool Ok()
	{
		if (!m_Framed || !m_J)
			return false;
		DZS_SrcNode ok = m_J.Get("ok");
		return ok && ok.IsBool() && ok.m_Bool;
	}

	// Error code of an answer with ok false; "unreachable" for a failed transport.
	string Code()
	{
		if (!m_Framed || !m_J)
			return "unreachable";
		DZS_SrcNode e = m_J.Get("error");
		if (!e || !e.IsObj())
			return "";
		return DZS_CollabJ.S(e, "code");
	}

	string ErrS(string key)
	{
		if (!m_J)
			return "";
		DZS_SrcNode e = m_J.Get("error");
		if (!e)
			return "";
		return DZS_CollabJ.S(e, key);
	}

	string S(string key) { return DZS_CollabJ.S(m_J, key); }
	int I(string key) { return DZS_CollabJ.I(m_J, key); }
	bool B(string key) { return DZS_CollabJ.B(m_J, key); }
	DZS_SrcNode N(string key)
	{
		if (!m_J)
			return null;
		return m_J.Get(key);
	}
}

class DZS_CollabJ
{
	static string S(DZS_SrcNode n, string key)
	{
		if (!n)
			return "";
		DZS_SrcNode v = n.Get(key);
		if (!v)
			return "";
		if (v.IsStr() || v.IsNum())
			return v.m_Str;
		return "";
	}

	static int I(DZS_SrcNode n, string key)
	{
		string s = S(n, key);
		return s.ToInt();
	}

	static float F(DZS_SrcNode n)
	{
		if (!n)
			return 0;
		string s = n.m_Str;
		return s.ToFloat();
	}

	static bool B(DZS_SrcNode n, string key)
	{
		if (!n)
			return false;
		DZS_SrcNode v = n.Get(key);
		return v && v.IsBool() && v.m_Bool;
	}

	static vector V(DZS_SrcNode n, string key)
	{
		vector r = "0 0 0";
		if (!n)
			return r;
		DZS_SrcNode a = n.Get(key);
		if (!a || !a.IsArr() || a.Count() < 3)
			return r;
		r[0] = F(a.m_Items[0]);
		r[1] = F(a.m_Items[1]);
		r[2] = F(a.m_Items[2]);
		return r;
	}

	static string Q(string s)
	{
		return DZS_TextForm.Str(s);
	}

	static string Num(float f)
	{
		return DZS_F32.ToText(f);
	}

	static string Vec(vector v)
	{
		string x = Num(v[0]);
		string y = Num(v[1]);
		string z = Num(v[2]);
		return "[" + x + "," + y + "," + z + "]";
	}

	// A list of texts as JSON, each quoted as tightly as JSON allows.
	static string List(array<string> items)
	{
		string s = "[";
		for (int i = 0; i < items.Count(); i++)
		{
			if (i > 0)
				s += ",";
			string q = Q(items[i]);
			s += q;
		}
		return s + "]";
	}

	// Bytes of the pieces as the server writes them (with quotes, escaped tightly).
	static int PiecesBytes(array<string> pieces)
	{
		int n = 0;
		foreach (string p : pieces)
		{
			string q = Q(p);
			n += q.Length() + 1;
		}
		return n;
	}

	static array<string> Strings(DZS_SrcNode arr)
	{
		array<string> r = new array<string>();
		if (!arr || !arr.IsArr())
			return r;
		foreach (DZS_SrcNode it : arr.m_Items)
			r.Insert(it.m_Str);
		return r;
	}
}

class DZS_CollabCall : RestCallback
{
	DZS_CollabNet m_Net;
	string m_Ch;
	string m_Tag;
	string m_Path;
	string m_Body;
	bool m_Get;
	bool m_Dropped;
	bool m_Answered;
	float m_T0;
	int m_Tries;
	ref Managed m_Data;

	override void OnSuccess(string data, int dataSize)
	{
		if (m_Net)
			m_Net.Answer(this, data, "");
	}

	override void OnError(int errorCode)
	{
		if (m_Net)
			m_Net.Answer(this, "", "error " + errorCode);
	}

	override void OnTimeout()
	{
		if (m_Net)
			m_Net.Answer(this, "", "timeout");
	}
}

class DZS_CollabListener : Managed
{
	void OnCollab(DZS_CollabCall call, DZS_CollabReply reply) {}
}

class DZS_CollabNet : Managed
{
	static const string MAGIC = "dzcollab 1";
	string m_Base;
	ref DZS_CollabListener m_L;
	protected RestContext m_Ctx;
	ref DZS_CollabCall m_PollCall;
	ref DZS_CollabCall m_SendCall;
	ref array<ref DZS_CollabCall> m_Queue = new array<ref DZS_CollabCall>();
	int m_MaxOpen;
	bool m_Paused;

	void DZS_CollabNet(string base, DZS_CollabListener l)
	{
		m_Base = base;
		m_L = l;
	}

	static string Trim(string base)
	{
		string b = base;
		b.TrimInPlace();
		while (b.Length() > 0)
		{
			string last = b.Substring(b.Length() - 1, 1);
			if (last != "/")
				break;
			b = b.Substring(0, b.Length() - 1);
		}
		return b;
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
		m_Ctx = api.GetRestContext(Trim(m_Base));
		return m_Ctx;
	}

	protected DZS_CollabCall Make(string ch, string tag, string path, string body, Managed data)
	{
		DZS_CollabCall c = new DZS_CollabCall();
		c.m_Net = this;
		c.m_Ch = ch;
		c.m_Tag = tag;
		c.m_Path = "/" + path;
		c.m_Body = body;
		c.m_Data = data;
		return c;
	}

	// The one open poll.
	bool PollOpen() { return m_PollCall != null; }

	void Poll(string body)
	{
		if (m_PollCall)
			return;
		m_PollCall = Make("poll", "poll", "poll", body, null);
		Start(m_PollCall);
	}

	// A request in the send channel; a waiting one with the same tag "live" is replaced by the newer.
	DZS_CollabCall Send(string tag, string path, string body, Managed data = null)
	{
		if (tag == "live")
		{
			foreach (DZS_CollabCall w : m_Queue)
			{
				if (w.m_Tag == "live")
				{
					w.m_Body = body;
					w.m_Data = data;
					return w;
				}
			}
		}
		DZS_CollabCall c = Make("send", tag, path, body, data);
		m_Queue.Insert(c);
		Pump();
		return c;
	}

	// A GET outside the channels (info of a server that is being added).
	void Info(string tag, Managed data = null)
	{
		DZS_CollabCall c = Make("info", tag, "info", "", data);
		c.m_Get = true;
		Start(c);
	}

	bool SendBusy() { return m_SendCall != null; }
	int Waiting() { return m_Queue.Count(); }

	float SendAge()
	{
		if (!m_SendCall)
			return 0;
		return GetGame().GetTickTime() - m_SendCall.m_T0;
	}

	float PollAge()
	{
		if (!m_PollCall)
			return 0;
		return GetGame().GetTickTime() - m_PollCall.m_T0;
	}

	float m_HoldUntil;

	void Pump()
	{
		if (m_SendCall || m_Queue.Count() == 0)
			return;
		if (GetGame().GetTickTime() < m_HoldUntil)
			return;
		if (m_Paused && m_Queue[0].m_Tag != "resume")
			return;
		m_SendCall = m_Queue[0];
		m_Queue.RemoveOrdered(0);
		Start(m_SendCall);
	}

	// The rejoin after a long interruption goes before everything collected.
	DZS_CollabCall SendFirst(string tag, string path, string body, Managed data = null)
	{
		DZS_CollabCall c = Make("send", tag, path, body, data);
		m_Queue.InsertAt(c, 0);
		Pump();
		return c;
	}

	// Sends the same request again (retry: unchanged, same request id), at the earliest after a pause.
	void Repeat(DZS_CollabCall c, float after = 0)
	{
		c.m_Answered = false;
		c.m_Tries++;
		if (c.m_Ch == "send")
		{
			m_Queue.InsertAt(c, 0);
			if (after > 0)
				m_HoldUntil = GetGame().GetTickTime() + after;
			Pump();
		}
		else if (c.m_Ch == "poll")
		{
			m_PollCall = c;
			Start(c);
		}
	}

	void DropAll()
	{
		m_Queue.Clear();
		if (m_SendCall)
			m_SendCall.m_Dropped = true;
		if (m_PollCall)
			m_PollCall.m_Dropped = true;
		m_SendCall = null;
		m_PollCall = null;
	}

	protected void Start(DZS_CollabCall c)
	{
		RestContext ctx = Ctx();
		if (!ctx)
			return;
		c.m_T0 = GetGame().GetTickTime();
		int open = 0;
		if (m_PollCall)
			open++;
		if (m_SendCall)
			open++;
		if (open > m_MaxOpen)
			m_MaxOpen = open;
		if (c.m_Get)
			ctx.GET(c, c.m_Path);
		else
			ctx.POST(c, c.m_Path, c.m_Body);
		if (c.m_Tag != "poll" && c.m_Tag != "live")
			DZS_Log.Info("collab: " + c.m_Tag + " sent, " + c.m_Body.Length() + " bytes");
	}

	static DZS_CollabReply Parse(string data)
	{
		DZS_CollabReply r = new DZS_CollabReply();
		int nl = data.IndexOf("\n");
		if (nl < 0)
		{
			r.m_Fail = "no status line";
			return r;
		}
		string first = data.Substring(0, nl);
		first.TrimInPlace();
		if (first != MAGIC)
		{
			r.m_Fail = "no status line";
			return r;
		}
		array<string> pieces = new array<string>();
		int at = nl + 1;
		int end = data.Length();
		while (at < end)
		{
			int want = Math.Min(4000, end - at);
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
			pieces.Insert(piece);
			at += want;
		}
		DZS_SrcDoc doc = DZS_SrcJson.Parse(pieces);
		if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
		{
			r.m_Fail = "JSON unlesbar";
			return r;
		}
		r.m_Doc = doc;
		r.m_J = doc.m_Root;
		r.m_Framed = true;
		return r;
	}

	void Answer(DZS_CollabCall c, string data, string err)
	{
		if (c.m_Answered)
			return;
		c.m_Answered = true;
		if (c == m_PollCall)
			m_PollCall = null;
		if (c == m_SendCall)
			m_SendCall = null;
		DZS_CollabReply r;
		if (err != "")
		{
			r = new DZS_CollabReply();
			r.m_Fail = err;
		}
		else
			r = Parse(data);
		if (c.m_Tag != "poll" && c.m_Tag != "live")
		{
			float secs = GetGame().GetTickTime() - c.m_T0;
			string res = "ok";
			if (!r.m_Framed)
				res = "unreachable (" + r.m_Fail + ")";
			else if (!r.Ok())
				res = "error " + r.Code();
			DZS_Log.Info("collab: " + c.m_Tag + " answered in " + DZS_Num.Fixed(secs, 2) + " s, " + data.Length() + " bytes, " + res);
		}
		DZS_CollabCall keep = c;
		if (!c.m_Dropped && m_L)
			m_L.OnCollab(keep, r);
		Pump();
	}
}

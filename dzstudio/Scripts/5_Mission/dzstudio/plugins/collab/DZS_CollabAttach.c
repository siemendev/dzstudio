// Attachments: the text of loot files the state
// names only by checksum. Who switches loot on or saves uploads the new texts before the operation that names them;
// the others fetch a text they lack by checksum and ask for it again when the server does not have it.
class DZS_CollabAttachMark : Managed
{
	string m_Sum;
	int m_Part;
	int m_Parts;
}

class DZS_CollabAttach : Managed
{
	DZS_CollabPlugin m_P;
	ref map<string, bool> m_Uploaded = new map<string, bool>();
	ref map<string, float> m_WantAt = new map<string, float>();
	// fetching one text at a time
	string m_GetSum;
	ref array<string> m_GetChunks;
	float m_RetryAt;
	float m_MissingSince = -1;
	bool m_SaidFetching;
	bool m_SaidMissing;
	float m_Now;
	float m_CheckAt;

	void DZS_CollabAttach(DZS_CollabPlugin p)
	{
		m_P = p;
	}

	void Reset()
	{
		m_Uploaded.Clear();
		m_WantAt.Clear();
		m_GetSum = "";
		m_GetChunks = null;
		m_MissingSince = -1;
		m_SaidFetching = false;
		m_SaidMissing = false;
		DZS_FileTexts h = DZS_FileTexts.Get();
		if (h)
			h.SetWaiting("", "");
	}

	// An own operation names files (loot switched on, consequences of a save): their texts go out before it.
	void BeforeOperation(array<string> text)
	{
		bool names = false;
		foreach (string t : text)
		{
			if (t.Contains("\"lootFiles\""))
			{
				names = true;
				break;
			}
		}
		if (!names)
			return;
		DZS_FileTexts h = DZS_FileTexts.Get();
		DZS_Instance inst = m_P.Shown();
		if (!h || !inst)
			return;
		array<string> names2 = new array<string>();
		array<string> sums = new array<string>();
		h.Named(inst, names2, sums);
		for (int i = 0; i < sums.Count(); i++)
		{
			if (m_Uploaded.Contains(sums[i]))
				continue;
			array<string> chunks = h.TextOf(sums[i]);
			if (chunks)
				Upload(sums[i], chunks, names2[i]);
		}
	}

	void Upload(string sum, array<string> chunks, string why)
	{
		array<ref array<string>> parts = DZS_CollabPlugin.PartsOf(chunks);
		int bytes = DZS_Files.Length(chunks);
		DZS_Log.Info("collab: attachment " + why + " " + sum + ", " + bytes + " bytes in " + parts.Count() + " part(s), before the operation");
		m_Uploaded.Set(sum, true);
		for (int p = 0; p < parts.Count(); p++)
		{
			DZS_CollabAttachMark mk = new DZS_CollabAttachMark();
			mk.m_Sum = sum;
			mk.m_Part = p;
			mk.m_Parts = parts.Count();
			string body = "{" + m_P.SessionHead() + ",\"checksum\":" + DZS_CollabJ.Q(sum) + ",\"seq\":" + m_P.m_S.m_Applied;
			body += ",\"bytes\":" + bytes + ",\"part\":" + p + ",\"parts\":" + parts.Count() + ",\"pieces\":" + DZS_CollabJ.List(parts[p]) + "}";
			m_P.SendInSession("attach/put", "attachment/put", body, mk);
		}
	}

	void OnPut(DZS_CollabCall c, DZS_CollabReply r)
	{
		DZS_CollabAttachMark mk = DZS_CollabAttachMark.Cast(c.m_Data);
		if (!mk)
			return;
		if (!r.m_Framed)
		{
			m_P.RepeatInSession(c, 1);
			return;
		}
		if (!r.Ok())
		{
			DZS_Log.Warn("collab: attachment/put " + mk.m_Sum + " part " + mk.m_Part + ": " + r.Code());
			m_Uploaded.Remove(mk.m_Sum);
			return;
		}
		if (r.B("complete"))
		{
			DZS_Log.Info("collab: attachment " + mk.m_Sum + " complete at the server (part " + mk.m_Part + " of " + mk.m_Parts + ")");
			m_P.DropQueued("attach/put", mk.m_Sum);
		}
	}

	// attachment_wanted: whoever has the text uploads it again without being asked.
	void OnWanted(string sum)
	{
		DZS_FileTexts h = DZS_FileTexts.Get();
		if (!h)
			return;
		array<string> chunks = h.TextOf(sum);
		if (!chunks)
		{
			DZS_Log.Info("collab: attachment " + sum + " wanted, not held here");
			return;
		}
		m_Uploaded.Remove(sum);
		Upload(sum, chunks, "wanted");
	}

	void Update(float dt)
	{
		m_Now += dt;
		if (m_Now < m_CheckAt)
			return;
		m_CheckAt = m_Now + 0.5;
		DZS_FileTexts h = DZS_FileTexts.Get();
		DZS_Instance inst = m_P.Shown();
		if (!h || !inst || !m_P.m_S || !m_P.m_S.m_Ready)
			return;
		array<string> names = new array<string>();
		array<string> sums = new array<string>();
		h.Named(inst, names, sums);
		string missing = "";
		foreach (string s : sums)
		{
			if (!h.TextOf(s))
			{
				missing = s;
				break;
			}
		}
		if (missing == "")
		{
			if (m_MissingSince >= 0)
			{
				DZS_Log.Info("collab: all loot texts here after " + DZS_Num.Fixed(m_Now - m_MissingSince, 1) + " s");
				h.SetWaiting("", "");
			}
			m_MissingSince = -1;
			m_SaidFetching = false;
			m_SaidMissing = false;
			return;
		}
		if (m_MissingSince < 0)
			m_MissingSince = m_Now;
		string who = m_P.LootSource();
		if (m_Now - m_MissingSince >= 60)
		{
			h.SetWaiting("Loot-Dateien nicht verfügbar", "Loot files not available");
			if (!m_SaidMissing)
			{
				m_SaidMissing = true;
				m_P.Msg("warning", "Loot-Dateien nicht verfügbar — Loot bleibt nur ansehbar", "Loot files not available — loot stays view-only", 2, "collab.loot");
			}
		}
		else
		{
			h.SetWaiting("Neue Loot-Dateien werden geholt …", "Fetching new loot files …");
			if (!m_SaidFetching)
			{
				m_SaidFetching = true;
				m_P.Msg("", "Neue Loot-Dateien von " + who + " werden geholt — Loot ist so lange nur ansehbar", "Fetching new loot files from " + who + " — loot is view-only until then", 2, "collab.loot");
			}
		}
		if (m_GetSum != "" || m_Now < m_RetryAt)
			return;
		m_GetSum = missing;
		m_GetChunks = new array<string>();
		AskPart(0);
	}

	protected void AskPart(int part)
	{
		string body = "{" + m_P.SessionHead() + ",\"checksum\":" + DZS_CollabJ.Q(m_GetSum) + ",\"part\":" + part + "}";
		m_P.SendInSession("attach/get", "attachment/get", body);
	}

	void OnGet(DZS_CollabCall c, DZS_CollabReply r)
	{
		if (m_GetSum == "")
			return;
		if (!r.m_Framed)
		{
			m_P.RepeatInSession(c, 1);
			return;
		}
		if (!r.Ok())
		{
			DZS_Log.Info("collab: attachment/get " + m_GetSum + ": " + r.Code());
			Want(m_GetSum);
			return;
		}
		array<string> pieces = DZS_CollabJ.Strings(r.N("pieces"));
		foreach (string pc : pieces)
			m_GetChunks.Insert(pc);
		int part = r.I("part");
		int parts = r.I("parts");
		if (part + 1 < parts)
		{
			AskPart(part + 1);
			return;
		}
		DZS_FileTexts h = DZS_FileTexts.Get();
		string sum = m_GetSum;
		array<string> chunks = m_GetChunks;
		m_GetSum = "";
		m_GetChunks = null;
		if (!h || !h.Take(m_P.Shown(), sum, chunks))
		{
			DZS_Log.Warn("collab: attachment " + sum + " did not fit its checksum");
			Want(sum);
			return;
		}
		DZS_Log.Info("collab: attachment " + sum + " fetched, " + DZS_Files.Length(chunks) + " bytes in " + parts + " part(s)");
	}

	// attachment_unknown or a wrong text: ask the one who uploaded it (at most once a minute), try again every 5 s.
	protected void Want(string sum)
	{
		m_GetSum = "";
		m_GetChunks = null;
		m_RetryAt = m_Now + 5;
		if (m_WantAt.Contains(sum) && m_Now - m_WantAt.Get(sum) < 60)
			return;
		m_WantAt.Set(sum, m_Now);
		string body = "{" + m_P.SessionHead() + ",\"checksum\":" + DZS_CollabJ.Q(sum) + "}";
		m_P.SendInSession("attach/want", "attachment/want", body);
		DZS_Log.Info("collab: attachment " + sum + " wanted from the server");
	}
}

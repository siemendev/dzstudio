// Reading pasted Spawner text over several frames: thousands of objects would stop the game for seconds.
class DZS_PasteJob : DZS_Job
{
	// shorter texts are read at once
	static const int AT_ONCE = 100000;
	// Substring of a long string costs about its length, and returns at most 8191 characters: long texts are cut
	// into pieces first, those into chunks
	static const int PIECE = 8100;
	static const int CHUNK = 900;

	string m_Clip;
	DZS_Instance m_Inst;
	bool m_AtOrigin;
	vector m_Target;
	protected DZS_Format m_Fmt;
	protected int m_From;
	protected int m_To;
	protected int m_At;
	protected bool m_Bare;
	protected ref array<string> m_Chunks = new array<string>();
	protected ref DZS_FormatReader m_Reader;

	// null when the text is no JSON object or list
	static DZS_PasteJob For(DZS_Format fmt, string clip)
	{
		// string.Trim() keeps only the first 8191 characters
		int from = 0;
		int to = clip.Length();
		while (from < to && " \t\r\n".IndexOf(clip.Get(from)) >= 0)
			from++;
		while (to > from && " \t\r\n".IndexOf(clip.Get(to - 1)) >= 0)
			to--;
		if (from >= to)
			return null;
		string first = clip.Get(from);
		if (first != "{" && first != "[")
			return null;
		DZS_PasteJob j = new DZS_PasteJob();
		j.m_Fmt = fmt;
		j.m_Clip = clip;
		j.m_From = from;
		j.m_To = to;
		j.m_At = from;
		j.m_Bare = first == "[";
		if (j.m_Bare)
			j.m_Chunks.Insert("{\"Objects\": ");
		return j;
	}

	void DZS_PasteJob()
	{
		m_De = "Einfügen";
		m_En = "Pasting";
		m_Cancelable = true;
	}

	DZS_ReadResult Result()
	{
		if (!m_Reader)
			return null;
		return m_Reader.Result();
	}

	override bool Step(float ms)
	{
		int t0 = TickCount(0);
		if (!m_Reader)
		{
			while (m_At < m_To)
			{
				int len = Math.Min(PIECE, m_To - m_At);
				string piece = m_Clip.Substring(m_At, len);
				for (int p = 0; p < len; p += CHUNK)
				{
					string c = piece.Substring(p, Math.Min(CHUNK, len - p));
					m_Chunks.Insert(c);
				}
				m_At += len;
				m_Share = 0.1 * (m_At - m_From) / (m_To - m_From);
				if (Over(t0, ms))
					return false;
			}
			if (m_Bare)
				m_Chunks.Insert("}");
			m_Reader = m_Fmt.BeginRead(m_Chunks);
		}
		float left = ms - (TickCount(0) - t0) / 10000.0;
		if (left <= 0)
			return false;
		bool done = m_Reader.Step(left);
		m_Share = 0.1 + 0.9 * m_Reader.Progress();
		return done;
	}
}

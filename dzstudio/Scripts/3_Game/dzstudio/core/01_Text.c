// Text as a sequence of chunks of at most DZS_CHUNK_BYTES bytes, split only between UTF-8 characters.
class DZS_TextWriter : Managed
{
	protected ref array<string> m_Chunks;
	protected string m_Cur;
	protected int m_Max;

	void DZS_TextWriter(int maxBytes = DZS_CHUNK_BYTES)
	{
		m_Chunks = new array<string>();
		m_Cur = "";
		m_Max = maxBytes;
	}

	void Append(string s)
	{
		int n = s.Length();
		if (n == 0)
			return;
		if (m_Cur.Length() + n <= m_Max)
		{
			m_Cur += s;
			return;
		}
		int start = 0;
		while (start < n)
		{
			int room = m_Max - m_Cur.Length();
			if (n - start <= room)
			{
				m_Cur += s.Substring(start, n - start);
				return;
			}
			int cut = DZS_Utf8.SafeCut(s, start + room);
			if (cut <= start)
			{
				Flush();
				continue;
			}
			m_Cur += s.Substring(start, cut - start);
			start = cut;
			Flush();
		}
	}

	protected void Flush()
	{
		if (m_Cur.Length() > 0)
			m_Chunks.Insert(m_Cur);
		m_Cur = "";
	}

	// The chunks belong to the caller; the writer may be gone by the time they are used.
	array<string> Finish()
	{
		Flush();
		array<string> result = new array<string>();
		result.Copy(m_Chunks);
		return result;
	}
}

// Reads a chunk sequence byte by byte without ever joining it into one string.
class DZS_TextReader : Managed
{
	protected ref array<string> m_Chunks;
	protected int m_Chunk;
	protected int m_Pos;
	// next quote and backslash of chunk m_SeenChunk at or after m_Pos (the chunk's length when there is none)
	protected int m_SeenChunk = -1;
	protected int m_NextQuote;
	protected int m_NextSlash;

	void DZS_TextReader(array<string> chunks)
	{
		m_Chunks = new array<string>();
		foreach (string c : chunks)
		{
			if (c.Length() > 0)
				m_Chunks.Insert(c);
		}
		m_Chunk = 0;
		m_Pos = 0;
	}

	bool AtEnd()
	{
		return m_Chunk >= m_Chunks.Count();
	}

	string Peek()
	{
		if (AtEnd())
			return "";
		return m_Chunks[m_Chunk].Get(m_Pos);
	}

	string Next()
	{
		if (AtEnd())
			return "";
		string c = m_Chunks[m_Chunk].Get(m_Pos);
		Advance(1);
		return c;
	}

	protected void Advance(int n)
	{
		m_Pos += n;
		while (m_Chunk < m_Chunks.Count() && m_Pos >= m_Chunks[m_Chunk].Length())
		{
			m_Pos -= m_Chunks[m_Chunk].Length();
			m_Chunk++;
		}
	}

	void SkipWhitespace()
	{
		while (!AtEnd())
		{
			string c = Peek();
			if (c == " " || c == "\n" || c == "\r" || c == "\t")
				Advance(1);
			else
				return;
		}
	}

	// Reads up to (not including) the first of the stop characters; returns the run, which may span chunks.
	string ReadRun(string stops)
	{
		string result = "";
		while (!AtEnd())
		{
			string chunk = m_Chunks[m_Chunk];
			int len = chunk.Length();
			int i = m_Pos;
			while (i < len && !stops.Contains(chunk.Get(i)))
				i++;
			if (i > m_Pos)
				result += DZS_Utf8.Part(chunk, m_Pos, i - m_Pos);
			int taken = i - m_Pos;
			Advance(taken);
			if (i < len)
				return result;
		}
		return result;
	}

	// ReadRun("\"\\") with the game's string search instead of a test per character: a JSON string's text up to
	// its closing quote or next escape. Each position is searched once per chunk, so long strings stay linear.
	string ReadStringRun()
	{
		string result = "";
		while (!AtEnd())
		{
			string chunk = m_Chunks[m_Chunk];
			int len = chunk.Length();
			if (m_SeenChunk != m_Chunk)
			{
				m_SeenChunk = m_Chunk;
				m_NextQuote = -1;
				m_NextSlash = -1;
			}
			if (m_NextQuote < m_Pos)
			{
				m_NextQuote = chunk.IndexOfFrom(m_Pos, "\"");
				if (m_NextQuote < 0)
					m_NextQuote = len;
			}
			if (m_NextSlash < m_Pos)
			{
				m_NextSlash = chunk.IndexOfFrom(m_Pos, "\\");
				if (m_NextSlash < 0)
					m_NextSlash = len;
			}
			int i = Math.Min(m_NextQuote, m_NextSlash);
			if (i > m_Pos)
				result += DZS_Utf8.Part(chunk, m_Pos, i - m_Pos);
			Advance(i - m_Pos);
			if (i < len)
				return result;
		}
		return result;
	}
}

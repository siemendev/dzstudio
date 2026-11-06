// Strict JSON reader for files: keeps the exact source text of every value, duplicate keys in their
// order, number text as written, and reports errors with line and column. Never the game's reader
// (it cuts texts at 1023 bytes).
class DZS_SrcNode : Managed
{
	static const int NUL = 0;
	static const int BOOL = 1;
	static const int NUM = 2;
	static const int STR = 3;
	static const int ARR = 4;
	static const int OBJ = 5;

	int m_Type;
	string m_Str;
	bool m_Bool;
	int m_Start;
	int m_End;
	int m_KeyStart;
	int m_KeyEnd;
	ref array<ref DZS_SrcNode> m_Items;
	ref array<string> m_Keys;

	bool IsObj() { return m_Type == OBJ; }
	bool IsArr() { return m_Type == ARR; }
	bool IsStr() { return m_Type == STR; }
	bool IsNum() { return m_Type == NUM; }
	bool IsBool() { return m_Type == BOOL; }
	bool IsNull() { return m_Type == NUL; }

	int Count()
	{
		if (m_Items)
			return m_Items.Count();
		return 0;
	}

	// Last occurrence of a key (duplicates: the last one counts).
	DZS_SrcNode Get(string key)
	{
		if (m_Type != OBJ)
			return null;
		for (int i = m_Keys.Count() - 1; i >= 0; i--)
		{
			if (m_Keys[i] == key)
				return m_Items[i];
		}
		return null;
	}

	int CountKey(string key)
	{
		int n = 0;
		if (m_Type != OBJ)
			return 0;
		foreach (string k : m_Keys)
		{
			if (k == key)
				n++;
		}
		return n;
	}

	string TypeName()
	{
		if (m_Type == NUL) return "null";
		if (m_Type == BOOL) return "bool";
		if (m_Type == NUM) return "number";
		if (m_Type == STR) return "text";
		if (m_Type == ARR) return "list";
		return "object";
	}

	// Plain JSON tree (for plugin data and other values the caller handles as a whole).
	DZS_Json ToJson()
	{
		if (m_Type == NUL)
			return DZS_Json.Null();
		if (m_Type == BOOL)
			return DZS_Json.Boolean(m_Bool);
		if (m_Type == NUM)
			return DZS_Json.Num(m_Str);
		if (m_Type == STR)
			return DZS_Json.Str(m_Str);
		if (m_Type == ARR)
		{
			DZS_Json a = DZS_Json.Arr();
			foreach (DZS_SrcNode it : m_Items)
			{
				DZS_Json t88 = it.ToJson();
				a.Add(t88);
			}
			return a;
		}
		DZS_Json o = DZS_Json.Obj();
		for (int i = 0; i < m_Keys.Count(); i++)
		{
			DZS_Json v = m_Items[i].ToJson();
			string k = m_Keys[i];
			o.Set(k, v);
		}
		return o;
	}
}

class DZS_SrcDoc : Managed
{
	ref array<string> m_Chunks;
	ref array<int> m_Starts;
	ref DZS_SrcNode m_Root;
	string m_Error;
	int m_Line;
	int m_Col;
	bool m_Bom;

	// Exact source bytes between two offsets.
	string Slice(int start, int end)
	{
		string r = "";
		if (end <= start)
			return r;
		int ci = FindChunk(start);
		int pos = start;
		while (pos < end && ci < m_Chunks.Count())
		{
			int cs = m_Starts[ci];
			string c = m_Chunks[ci];
			int from = pos - cs;
			int to = Math.Min(c.Length(), end - cs);
			if (to > from)
				r += DZS_Utf8.Part(c, from, to - from);
			pos = cs + c.Length();
			ci++;
		}
		return r;
	}

	string Raw(DZS_SrcNode n)
	{
		return Slice(n.m_Start, n.m_End);
	}

	protected int FindChunk(int offset)
	{
		int lo = 0;
		int hi = m_Starts.Count() - 1;
		while (lo < hi)
		{
			int mid = (lo + hi + 1) / 2;
			if (m_Starts[mid] <= offset)
				lo = mid;
			else
				hi = mid - 1;
		}
		return lo;
	}

	string ErrorText()
	{
		return m_Error + " (Zeile " + m_Line + ", Spalte " + m_Col + ")";
	}
}

class DZS_SrcJson
{
	protected ref array<string> m_Chunks;
	protected string m_Cur;
	protected int m_CurLen;
	protected int m_Ci;
	protected int m_Pos;
	protected int m_Offset;
	protected int m_Line;
	protected int m_LineStart;
	protected string m_Error;
	protected int m_ErrLine;
	protected int m_ErrCol;
	protected int m_ErrOffset;

	static DZS_SrcDoc Parse(array<string> chunks)
	{
		DZS_SrcJson p = new DZS_SrcJson();
		return p.Run(chunks);
	}

	protected DZS_SrcDoc Run(array<string> chunks)
	{
		DZS_SrcDoc doc = new DZS_SrcDoc();
		doc.m_Chunks = new array<string>();
		doc.m_Starts = new array<int>();
		int off = 0;
		foreach (string c : chunks)
		{
			if (c.Length() == 0)
				continue;
			doc.m_Chunks.Insert(c);
			doc.m_Starts.Insert(off);
			off += c.Length();
		}
		if (doc.m_Starts.Count() == 0)
			doc.m_Starts.Insert(0);
		m_Chunks = doc.m_Chunks;
		m_Ci = 0;
		m_Pos = 0;
		m_Offset = 0;
		m_Line = 1;
		m_LineStart = 0;
		LoadChunk();
		if (Byte() == 239 && ByteAt(1) == 187 && ByteAt(2) == 191)
		{
			Skip(3);
			doc.m_Bom = true;
			m_LineStart = 3;
		}
		DZS_SrcNode root = Value(0);
		if (root && m_Error == "")
		{
			SkipWs();
			if (!AtEnd())
				Fail("Zeichen nach dem Ende");
		}
		if (m_Error != "")
		{
			doc.m_Error = m_Error;
			doc.m_Line = m_ErrLine;
			doc.m_Col = m_ErrCol;
			return doc;
		}
		doc.m_Root = root;
		return doc;
	}

	protected void LoadChunk()
	{
		if (m_Ci < m_Chunks.Count())
		{
			m_Cur = m_Chunks[m_Ci];
			m_CurLen = m_Cur.Length();
		}
		else
		{
			m_Cur = "";
			m_CurLen = 0;
		}
	}

	protected bool AtEnd() { return m_Ci >= m_Chunks.Count(); }

	protected int Byte()
	{
		if (AtEnd())
			return -1;
		int b = m_Cur.Get(m_Pos).ToAscii();
		if (b < 0)
			b += 256;
		return b;
	}

	protected int ByteAt(int ahead)
	{
		int ci = m_Ci;
		int pos = m_Pos + ahead;
		while (ci < m_Chunks.Count() && pos >= m_Chunks[ci].Length())
		{
			pos -= m_Chunks[ci].Length();
			ci++;
		}
		if (ci >= m_Chunks.Count())
			return -1;
		int b = m_Chunks[ci].Get(pos).ToAscii();
		if (b < 0)
			b += 256;
		return b;
	}

	protected void Skip(int n)
	{
		for (int i = 0; i < n; i++)
		{
			if (AtEnd())
				return;
			int b = Byte();
			m_Pos++;
			m_Offset++;
			if (b == 10)
			{
				m_Line++;
				m_LineStart = m_Offset;
			}
			if (m_Pos >= m_CurLen)
			{
				m_Ci++;
				m_Pos = 0;
				LoadChunk();
			}
		}
	}

	protected void Fail(string msg)
	{
		if (m_Error != "")
			return;
		m_Error = msg;
		m_ErrLine = m_Line;
		m_ErrOffset = m_Offset;
		m_ErrCol = 1;
		// column in characters from the line start
		int off = m_LineStart;
		int ci = 0;
		while (ci < m_Chunks.Count() - 1 && m_Chunks[ci].Length() <= 0)
			ci++;
		int col = 1;
		int pos = off;
		int base = 0;
		for (int k = 0; k < m_Chunks.Count(); k++)
		{
			string c = m_Chunks[k];
			int len = c.Length();
			for (int i = 0; i < len; i++)
			{
				int at = base + i;
				if (at >= m_Offset)
				{
					m_ErrCol = col;
					return;
				}
				if (at >= off)
				{
					int b = c.Get(i).ToAscii();
					if (b < 0)
						b += 256;
					if (b < 128 || b >= 192)
						col++;
				}
			}
			base += len;
		}
		m_ErrCol = col;
	}

	protected void SkipWs()
	{
		while (!AtEnd())
		{
			int b = Byte();
			if (b == 32 || b == 9 || b == 10 || b == 13)
				Skip(1);
			else
				return;
		}
	}

	protected DZS_SrcNode Value(int depth)
	{
		if (depth > 200)
		{
			Fail("zu tief verschachtelt");
			return null;
		}
		SkipWs();
		if (AtEnd())
		{
			Fail("unerwartetes Dateiende");
			return null;
		}
		DZS_SrcNode n = new DZS_SrcNode();
		n.m_Start = m_Offset;
		int b = Byte();
		if (b == 123)
		{
			n.m_Type = DZS_SrcNode.OBJ;
			n.m_Items = new array<ref DZS_SrcNode>();
			n.m_Keys = new array<string>();
			Skip(1);
			SkipWs();
			if (Byte() == 125)
			{
				Skip(1);
				n.m_End = m_Offset;
				return n;
			}
			while (true)
			{
				SkipWs();
				if (Byte() != 34)
				{
					Fail("Feldname erwartet");
					return null;
				}
				int keyStart = m_Offset;
				string key;
				if (!Str(key))
					return null;
				int keyEnd = m_Offset;
				SkipWs();
				if (Byte() != 58)
				{
					Fail("„:“ erwartet");
					return null;
				}
				Skip(1);
				DZS_SrcNode v = Value(depth + 1);
				if (!v)
					return null;
				v.m_KeyStart = keyStart;
				v.m_KeyEnd = keyEnd;
				n.m_Keys.Insert(key);
				n.m_Items.Insert(v);
				SkipWs();
				int sep = Byte();
				if (sep == 44)
				{
					Skip(1);
					continue;
				}
				if (sep == 125)
				{
					Skip(1);
					break;
				}
				Fail("„,“ oder „}“ erwartet");
				return null;
			}
			n.m_End = m_Offset;
			return n;
		}
		if (b == 91)
		{
			n.m_Type = DZS_SrcNode.ARR;
			n.m_Items = new array<ref DZS_SrcNode>();
			Skip(1);
			SkipWs();
			if (Byte() == 93)
			{
				Skip(1);
				n.m_End = m_Offset;
				return n;
			}
			while (true)
			{
				DZS_SrcNode item = Value(depth + 1);
				if (!item)
					return null;
				n.m_Items.Insert(item);
				SkipWs();
				int sep2 = Byte();
				if (sep2 == 44)
				{
					Skip(1);
					continue;
				}
				if (sep2 == 93)
				{
					Skip(1);
					break;
				}
				Fail("„,“ oder „]“ erwartet");
				return null;
			}
			n.m_End = m_Offset;
			return n;
		}
		if (b == 34)
		{
			n.m_Type = DZS_SrcNode.STR;
			string s;
			if (!Str(s))
				return null;
			n.m_Str = s;
			n.m_End = m_Offset;
			return n;
		}
		if (b == 116 || b == 102 || b == 110)
		{
			string word = "";
			while (!AtEnd())
			{
				int wb = Byte();
				if (wb < 97 || wb > 122)
					break;
				word += m_Cur.Get(m_Pos);
				Skip(1);
			}
			if (word == "true")
			{
				n.m_Type = DZS_SrcNode.BOOL;
				n.m_Bool = true;
			}
			else if (word == "false")
				n.m_Type = DZS_SrcNode.BOOL;
			else if (word == "null")
				n.m_Type = DZS_SrcNode.NUL;
			else
			{
				Fail("unbekanntes Wort „" + word + "“");
				return null;
			}
			n.m_End = m_Offset;
			return n;
		}
		if (b == 45 || (b >= 48 && b <= 57))
		{
			n.m_Type = DZS_SrcNode.NUM;
			string num;
			if (!Number(num))
				return null;
			n.m_Str = num;
			n.m_End = m_Offset;
			return n;
		}
		Fail("unerwartetes Zeichen");
		return null;
	}

	protected bool Digits(out string s)
	{
		bool any = false;
		while (!AtEnd())
		{
			int b = Byte();
			if (b < 48 || b > 57)
				break;
			s += m_Cur.Get(m_Pos);
			Skip(1);
			any = true;
		}
		return any;
	}

	protected bool Number(out string s)
	{
		s = "";
		if (Byte() == 45)
		{
			s += "-";
			Skip(1);
		}
		if (Byte() == 48)
		{
			s += "0";
			Skip(1);
			int nb = Byte();
			if (nb >= 48 && nb <= 57)
			{
				Fail("führende Null");
				return false;
			}
		}
		else if (!Digits(s))
		{
			Fail("Zahl erwartet");
			return false;
		}
		if (Byte() == 46)
		{
			s += ".";
			Skip(1);
			if (!Digits(s))
			{
				Fail("Ziffer nach „.“ erwartet");
				return false;
			}
		}
		int e = Byte();
		if (e == 101 || e == 69)
		{
			s += m_Cur.Get(m_Pos);
			Skip(1);
			int sg = Byte();
			if (sg == 43 || sg == 45)
			{
				s += m_Cur.Get(m_Pos);
				Skip(1);
			}
			if (!Digits(s))
			{
				Fail("Ziffer im Exponenten erwartet");
				return false;
			}
		}
		int after = Byte();
		if ((after >= 97 && after <= 122) || (after >= 65 && after <= 90))
		{
			Fail("ungültige Zahl");
			return false;
		}
		return true;
	}

	protected int Hex4()
	{
		int v = 0;
		for (int i = 0; i < 4; i++)
		{
			int b = Byte();
			int d = -1;
			if (b >= 48 && b <= 57)
				d = b - 48;
			else if (b >= 97 && b <= 102)
				d = b - 87;
			else if (b >= 65 && b <= 70)
				d = b - 55;
			if (d < 0)
			{
				Fail("ungültige \\u-Folge");
				return -1;
			}
			v = v * 16 + d;
			Skip(1);
		}
		return v;
	}

	// Reads a string starting at the opening quote; runs between escapes are copied in one piece.
	protected bool Str(out string s)
	{
		s = "";
		Skip(1);
		while (true)
		{
			if (AtEnd())
			{
				Fail("Text ohne Ende");
				return false;
			}
			int start = m_Pos;
			int i = m_Pos;
			int stop = -1;
			while (i < m_CurLen)
			{
				int b = m_Cur.Get(i).ToAscii();
				if (b < 0)
					b += 256;
				if (b == 34 || b == 92 || b < 32)
				{
					stop = b;
					break;
				}
				i++;
			}
			if (i > start)
			{
				s += DZS_Utf8.Part(m_Cur, start, i - start);
				Skip(i - start);
			}
			if (stop < 0)
				continue;
			if (stop == 34)
			{
				Skip(1);
				return true;
			}
			if (stop < 32)
			{
				Fail("Steuerzeichen im Text");
				return false;
			}
			Skip(1);
			int e = Byte();
			Skip(1);
			if (e == 34)
				s += "\"";
			else if (e == 92)
				s += "\\";
			else if (e == 47)
				s += "/";
			else if (e == 98)
				{ int bs = 8; s += bs.AsciiToString(); }
			else if (e == 102)
				{ int ff = 12; s += ff.AsciiToString(); }
			else if (e == 110)
				s += "\n";
			else if (e == 114)
				s += "\r";
			else if (e == 116)
				s += "\t";
			else if (e == 117)
			{
				int cp = Hex4();
				if (cp < 0)
					return false;
				if (cp >= 55296 && cp <= 56319 && Byte() == 92 && ByteAt(1) == 117)
				{
					Skip(2);
					int lo = Hex4();
					if (lo < 0)
						return false;
					cp = 65536 + ((cp - 55296) << 10) + (lo - 56320);
				}
				s += DZS_Utf8.FromCodepoint(cp);
			}
			else
			{
				Fail("ungültige Escape-Folge");
				return false;
			}
		}
		return false;
	}
}

// The same reader over several frames: containers above m_Deep are walked with an explicit stack and the budget is
// checked between their items; values from m_Deep down are read whole (one object of a file is small).
class DZS_SrcJsonSteps : DZS_SrcJson
{
	protected ref DZS_SrcDoc m_Doc;
	protected ref array<ref DZS_SrcNode> m_Stack = new array<ref DZS_SrcNode>();
	protected ref array<int> m_State = new array<int>();
	protected int m_Deep;
	protected int m_Total;
	protected bool m_Done;

	void DZS_SrcJsonSteps(array<string> chunks, int deep)
	{
		m_Deep = deep;
		m_Doc = new DZS_SrcDoc();
		m_Doc.m_Chunks = new array<string>();
		m_Doc.m_Starts = new array<int>();
		int off = 0;
		foreach (string c : chunks)
		{
			if (c.Length() == 0)
				continue;
			m_Doc.m_Chunks.Insert(c);
			m_Doc.m_Starts.Insert(off);
			off += c.Length();
		}
		m_Total = off;
		if (m_Doc.m_Starts.Count() == 0)
			m_Doc.m_Starts.Insert(0);
		m_Chunks = m_Doc.m_Chunks;
		m_Ci = 0;
		m_Pos = 0;
		m_Offset = 0;
		m_Line = 1;
		m_LineStart = 0;
		LoadChunk();
		if (Byte() == 239 && ByteAt(1) == 187 && ByteAt(2) == 191)
		{
			Skip(3);
			m_Doc.m_Bom = true;
			m_LineStart = 3;
		}
		SkipWs();
		int b = Byte();
		if ((b == 123 || b == 91) && m_Deep > 0)
			Open(null, -1, -1, "");
		else
			Finish(Value(0));
	}

	bool IsDone() { return m_Done; }

	// Share of the bytes read, 0 … 1.
	float Progress()
	{
		if (m_Total <= 0)
			return 1;
		float total = m_Total;
		return m_Offset / total;
	}

	DZS_SrcDoc Result() { return m_Doc; }

	protected void Open(DZS_SrcNode parent, int keyStart, int keyEnd, string key)
	{
		DZS_SrcNode n = new DZS_SrcNode();
		n.m_Start = m_Offset;
		n.m_Items = new array<ref DZS_SrcNode>();
		if (Byte() == 123)
		{
			n.m_Type = DZS_SrcNode.OBJ;
			n.m_Keys = new array<string>();
		}
		else
			n.m_Type = DZS_SrcNode.ARR;
		n.m_KeyStart = keyStart;
		n.m_KeyEnd = keyEnd;
		Skip(1);
		if (parent)
		{
			if (parent.IsObj())
				parent.m_Keys.Insert(key);
			parent.m_Items.Insert(n);
		}
		m_Stack.Insert(n);
		m_State.Insert(0);
	}

	protected void Finish(DZS_SrcNode root)
	{
		m_Done = true;
		if (root && m_Error == "")
		{
			SkipWs();
			if (!AtEnd())
				Fail("Zeichen nach dem Ende");
		}
		if (m_Error != "" || !root)
		{
			if (m_Error == "")
				Fail("unerwartetes Dateiende");
			m_Doc.m_Error = m_Error;
			m_Doc.m_Line = m_ErrLine;
			m_Doc.m_Col = m_ErrCol;
			return;
		}
		m_Doc.m_Root = root;
	}

	protected void Close()
	{
		int top = m_Stack.Count() - 1;
		DZS_SrcNode n = m_Stack[top];
		Skip(1);
		n.m_End = m_Offset;
		m_Stack.Remove(top);
		m_State.Remove(top);
		if (m_Stack.Count() == 0)
			Finish(n);
	}

	// One item of the innermost open container; false on an error.
	protected bool Item()
	{
		int top = m_Stack.Count() - 1;
		DZS_SrcNode n = m_Stack[top];
		int state = m_State[top];
		SkipWs();
		if (AtEnd())
		{
			Fail("unerwartetes Dateiende");
			return false;
		}
		int b = Byte();
		if (state == 1)
		{
			if (b == 44)
			{
				Skip(1);
				m_State[top] = 2;
				return true;
			}
			if ((n.IsObj() && b == 125) || (n.IsArr() && b == 93))
			{
				Close();
				return true;
			}
			if (n.IsObj())
				Fail("„,“ oder „}“ erwartet");
			else
				Fail("„,“ oder „]“ erwartet");
			return false;
		}
		if (state == 0 && ((n.IsObj() && b == 125) || (n.IsArr() && b == 93)))
		{
			Close();
			return true;
		}
		int keyStart = -1;
		int keyEnd = -1;
		string key = "";
		if (n.IsObj())
		{
			if (b != 34)
			{
				Fail("Feldname erwartet");
				return false;
			}
			keyStart = m_Offset;
			if (!Str(key))
				return false;
			keyEnd = m_Offset;
			SkipWs();
			if (Byte() != 58)
			{
				Fail("„:“ erwartet");
				return false;
			}
			Skip(1);
			SkipWs();
			b = Byte();
		}
		m_State[top] = 1;
		if ((b == 123 || b == 91) && m_Stack.Count() < m_Deep)
		{
			Open(n, keyStart, keyEnd, key);
			return true;
		}
		DZS_SrcNode v = Value(m_Stack.Count());
		if (!v)
			return false;
		v.m_KeyStart = keyStart;
		v.m_KeyEnd = keyEnd;
		if (n.IsObj())
			n.m_Keys.Insert(key);
		n.m_Items.Insert(v);
		return true;
	}

	// Reads until about ms milliseconds are used; true when the document is complete (or failed).
	bool Step(float ms)
	{
		if (m_Done)
			return true;
		int t0 = TickCount(0);
		float limit = ms * 10000;
		int n = 0;
		while (!m_Done)
		{
			if (!Item())
			{
				Finish(null);
				return true;
			}
			n++;
			if ((n & 7) == 0 && TickCount(0) - t0 >= limit)
				return m_Done;
		}
		return true;
	}
}

// Writes JSON in the editor's fixed text form:
// 4 spaces per level, LF, ": ", one field per line, empty {} and [] on the field's line.
class DZS_TextForm
{
	static string Indent(int depth)
	{
		return DZS_Json.Indent(depth);
	}

	static string Str(string s)
	{
		DZS_TextWriter w = new DZS_TextWriter(1000000);
		DZS_Json.WriteString(w, s);
		array<string> c = w.Finish();
		if (c.Count() == 0)
			return "\"\"";
		return c[0];
	}

	// Any DZS_Json value in the text form; numbers keep their stored text.
	static void Value(DZS_TextWriter w, DZS_Json v, int depth)
	{
		if (v.m_Type == DZS_Json.ARR)
		{
			if (v.Count() == 0)
			{
				w.Append("[]");
				return;
			}
			w.Append("[\n");
			for (int i = 0; i < v.Count(); i++)
			{
				w.Append(Indent(depth + 1));
				Value(w, v.At(i), depth + 1);
				if (i < v.Count() - 1)
					w.Append(",");
				w.Append("\n");
			}
			w.Append(Indent(depth) + "]");
			return;
		}
		if (v.m_Type == DZS_Json.OBJ)
		{
			if (v.Count() == 0)
			{
				w.Append("{}");
				return;
			}
			w.Append("{\n");
			for (int k = 0; k < v.m_Keys.Count(); k++)
			{
				w.Append(Indent(depth + 1));
				DZS_Json.WriteString(w, v.m_Keys[k]);
				w.Append(": ");
				Value(w, v.Get(v.m_Keys[k]), depth + 1);
				if (k < v.m_Keys.Count() - 1)
					w.Append(",");
				w.Append("\n");
			}
			w.Append(Indent(depth) + "}");
			return;
		}
		v.Write(w, false, 0);
	}
}

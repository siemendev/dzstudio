// Own JSON tree: reads strings of any length (the game's reader cuts them at 1023 bytes), keeps
// numbers as their source text and object keys in their order.
class DZS_Json : Managed
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
	ref array<ref DZS_Json> m_Items;
	ref array<string> m_Keys;
	ref map<string, ref DZS_Json> m_Map;

	static DZS_Json Obj()
	{
		DZS_Json j = new DZS_Json();
		j.m_Type = OBJ;
		j.m_Keys = new array<string>();
		j.m_Map = new map<string, ref DZS_Json>();
		return j;
	}

	static DZS_Json Arr()
	{
		DZS_Json j = new DZS_Json();
		j.m_Type = ARR;
		j.m_Items = new array<ref DZS_Json>();
		return j;
	}

	static DZS_Json Str(string s)
	{
		DZS_Json j = new DZS_Json();
		j.m_Type = STR;
		j.m_Str = s;
		return j;
	}

	static DZS_Json Num(string raw)
	{
		DZS_Json j = new DZS_Json();
		j.m_Type = NUM;
		j.m_Str = raw;
		return j;
	}

	static DZS_Json Int(int v)
	{
		return Num(v.ToString());
	}

	static DZS_Json Boolean(bool b)
	{
		DZS_Json j = new DZS_Json();
		j.m_Type = BOOL;
		j.m_Bool = b;
		return j;
	}

	static DZS_Json Null()
	{
		return new DZS_Json();
	}

	bool IsObj() { return m_Type == OBJ; }
	bool IsArr() { return m_Type == ARR; }
	bool IsStr() { return m_Type == STR; }
	bool IsNum() { return m_Type == NUM; }
	bool IsBool() { return m_Type == BOOL; }

	DZS_Json Set(string key, DZS_Json v)
	{
		if (!m_Map.Contains(key))
			m_Keys.Insert(key);
		m_Map.Set(key, v);
		return this;
	}

	// Deep copy made by the game module: values a mission-module plugin built must not live on in
	// kernel data after the mission module is unloaded (seen as a crash when flushing the settings).
	DZS_Json Clone()
	{
		DZS_Json c = new DZS_Json();
		c.m_Type = m_Type;
		c.m_Str = m_Str;
		c.m_Bool = m_Bool;
		if (m_Type == ARR)
		{
			c.m_Items = new array<ref DZS_Json>();
			for (int i = 0; i < m_Items.Count(); i++)
			{
				DZS_Json item = m_Items[i];
				DZS_Json itemCopy = item.Clone();
				c.m_Items.Insert(itemCopy);
			}
		}
		else if (m_Type == OBJ)
		{
			c.m_Keys = new array<string>();
			c.m_Map = new map<string, ref DZS_Json>();
			for (int n = 0; n < m_Keys.Count(); n++)
			{
				string key = m_Keys[n];
				DZS_Json value = m_Map.Get(key);
				DZS_Json valueCopy = value.Clone();
				c.m_Keys.Insert(key);
				c.m_Map.Set(key, valueCopy);
			}
		}
		return c;
	}

	void Remove(string key)
	{
		if (!m_Map.Contains(key))
			return;
		m_Map.Remove(key);
		m_Keys.RemoveOrdered(m_Keys.Find(key));
	}

	DZS_Json Get(string key)
	{
		if (m_Type != OBJ)
			return null;
		return m_Map.Get(key);
	}

	bool Has(string key)
	{
		return m_Type == OBJ && m_Map.Contains(key);
	}

	DZS_Json Add(DZS_Json v)
	{
		m_Items.Insert(v);
		return this;
	}

	int Count()
	{
		if (m_Type == ARR)
			return m_Items.Count();
		if (m_Type == OBJ)
			return m_Keys.Count();
		return 0;
	}

	DZS_Json At(int i)
	{
		return m_Items[i];
	}

	string GetStr(string key, string def = "")
	{
		DZS_Json v = Get(key);
		if (!v || v.m_Type != STR)
			return def;
		return v.m_Str;
	}

	int GetInt(string key, int def = 0)
	{
		DZS_Json v = Get(key);
		if (!v || v.m_Type != NUM)
			return def;
		return v.m_Str.ToInt();
	}

	bool GetBool(string key, bool def = false)
	{
		DZS_Json v = Get(key);
		if (!v || v.m_Type != BOOL)
			return def;
		return v.m_Bool;
	}

	// ---- writing ----

	array<string> ToChunks(bool pretty = false)
	{
		DZS_TextWriter w = new DZS_TextWriter();
		Write(w, pretty, 0);
		if (pretty)
			w.Append("\n");
		return w.Finish();
	}

	void Write(DZS_TextWriter w, bool pretty, int depth)
	{
		if (m_Type == NUL)
			w.Append("null");
		else if (m_Type == BOOL)
			w.Append(DZS_Str.Bool(m_Bool));
		else if (m_Type == NUM)
			w.Append(m_Str);
		else if (m_Type == STR)
			WriteString(w, m_Str);
		else if (m_Type == ARR)
		{
			if (m_Items.Count() == 0)
			{
				w.Append("[]");
				return;
			}
			w.Append("[");
			for (int i = 0; i < m_Items.Count(); i++)
			{
				if (i > 0)
					w.Append(",");
				if (pretty)
					w.Append("\n" + Indent(depth + 1));
				DZS_Json wi = m_Items[i];
				wi.Write(w, pretty, depth + 1);
			}
			if (pretty)
				w.Append("\n" + Indent(depth));
			w.Append("]");
		}
		else
		{
			if (m_Keys.Count() == 0)
			{
				w.Append("{}");
				return;
			}
			w.Append("{");
			for (int k = 0; k < m_Keys.Count(); k++)
			{
				if (k > 0)
					w.Append(",");
				if (pretty)
					w.Append("\n" + Indent(depth + 1));
				WriteString(w, m_Keys[k]);
				if (pretty)
					w.Append(": ");
				else
					w.Append(":");
				string wk = m_Keys[k];
				DZS_Json wv = m_Map.Get(wk);
				wv.Write(w, pretty, depth + 1);
			}
			if (pretty)
				w.Append("\n" + Indent(depth));
			w.Append("}");
		}
	}

	static string Indent(int depth)
	{
		string s = "";
		for (int i = 0; i < depth; i++)
			s += "    ";
		return s;
	}

	static void WriteString(DZS_TextWriter w, string s)
	{
		w.Append("\"");
		int n = s.Length();
		int runStart = 0;
		for (int i = 0; i < n; i++)
		{
			int b = DZS_Utf8.Byte(s, i);
			string esc = "";
			if (b == 34)
				esc = "\\\"";
			else if (b == 92)
				esc = "\\\\";
			else if (b < 32)
			{
				if (b == 8) esc = "\\b";
				else if (b == 9) esc = "\\t";
				else if (b == 10) esc = "\\n";
				else if (b == 12) esc = "\\f";
				else if (b == 13) esc = "\\r";
				else esc = "\\u00" + HexByte(b);
			}
			if (esc != "")
			{
				if (i > runStart)
					w.Append(DZS_Utf8.Part(s, runStart, i - runStart));
				w.Append(esc);
				runStart = i + 1;
			}
		}
		if (n > runStart)
			w.Append(DZS_Utf8.Part(s, runStart, n - runStart));
		w.Append("\"");
	}

	static string HexByte(int b)
	{
		string digits = "0123456789abcdef";
		string hi = digits.Get(b >> 4);
		string lo = digits.Get(b & 15);
		return hi + lo;
	}

	// ---- reading ----

	static DZS_Json Parse(array<string> chunks, out string error)
	{
		DZS_TextReader r = new DZS_TextReader(chunks);
		error = "";
		DZS_Json v = ParseValue(r, error, 0);
		if (error != "")
			return null;
		r.SkipWhitespace();
		if (!r.AtEnd())
		{
			error = "trailing characters";
			return null;
		}
		return v;
	}

	static DZS_Json ParseText(string text, out string error)
	{
		array<string> one = new array<string>();
		one.Insert(text);
		return Parse(one, error);
	}

	protected static DZS_Json ParseValue(DZS_TextReader r, out string error, int depth)
	{
		if (depth > 64)
		{
			error = "nesting too deep";
			return null;
		}
		r.SkipWhitespace();
		if (r.AtEnd())
		{
			error = "unexpected end";
			return null;
		}
		string c = r.Peek();
		if (c == "{")
		{
			r.Next();
			DZS_Json o = Obj();
			r.SkipWhitespace();
			if (r.Peek() == "}")
			{
				r.Next();
				return o;
			}
			while (true)
			{
				r.SkipWhitespace();
				if (r.Peek() != "\"")
				{
					error = "expected key";
					return null;
				}
				r.Next();
				string key = ParseStringBody(r, error);
				if (error != "")
					return null;
				r.SkipWhitespace();
				if (r.Next() != ":")
				{
					error = "expected ':'";
					return null;
				}
				DZS_Json val = ParseValue(r, error, depth + 1);
				if (error != "")
					return null;
				o.Set(key, val);
				r.SkipWhitespace();
				string sep = r.Next();
				if (sep == "}")
					return o;
				if (sep != ",")
				{
					error = "expected ',' or '}'";
					return null;
				}
			}
		}
		if (c == "[")
		{
			r.Next();
			DZS_Json a = Arr();
			r.SkipWhitespace();
			if (r.Peek() == "]")
			{
				r.Next();
				return a;
			}
			while (true)
			{
				DZS_Json item = ParseValue(r, error, depth + 1);
				if (error != "")
					return null;
				a.Add(item);
				r.SkipWhitespace();
				string sep2 = r.Next();
				if (sep2 == "]")
					return a;
				if (sep2 != ",")
				{
					error = "expected ',' or ']'";
					return null;
				}
			}
		}
		if (c == "\"")
		{
			r.Next();
			string s = ParseStringBody(r, error);
			if (error != "")
				return null;
			return Str(s);
		}
		if (c == "t" || c == "f" || c == "n")
		{
			string word = r.ReadRun(" \t\r\n,]}");
			if (word == "true")
				return Boolean(true);
			if (word == "false")
				return Boolean(false);
			if (word == "null")
				return Null();
			error = "bad literal";
			return null;
		}
		string num = r.ReadRun(" \t\r\n,]}");
		if (num == "" || !IsNumberText(num))
		{
			error = "bad value";
			return null;
		}
		return Num(num);
	}

	static bool IsNumberText(string s)
	{
		string allowed = "0123456789+-.eE";
		for (int i = 0; i < s.Length(); i++)
		{
			if (!allowed.Contains(s.Get(i)))
				return false;
		}
		return true;
	}

	protected static string ParseStringBody(DZS_TextReader r, out string error)
	{
		string result = "";
		while (true)
		{
			result += r.ReadRun("\"\\");
			if (r.AtEnd())
			{
				error = "unterminated string";
				return "";
			}
			string c = r.Next();
			if (c == "\"")
				return result;
			string e = r.Next();
			if (e == "\"") result += "\"";
			else if (e == "\\") result += "\\";
			else if (e == "/") result += "/";
			else if (e == "b") result += DZS_Utf8.FromCodepoint(8);
			else if (e == "f") result += DZS_Utf8.FromCodepoint(12);
			else if (e == "n") result += "\n";
			else if (e == "r") result += "\r";
			else if (e == "t") result += "\t";
			else if (e == "u")
			{
				int cp = ReadHex4(r, error);
				if (error != "")
					return "";
				if (cp >= 55296 && cp < 56320)
				{
					string n1 = r.Next();
					string n2 = r.Next();
					if (n1 != "\\" || n2 != "u")
					{
						error = "bad surrogate";
						return "";
					}
					int lo = ReadHex4(r, error);
					if (error != "")
						return "";
					cp = 65536 + ((cp - 55296) << 10) + (lo - 56320);
				}
				if (cp == 0)
				{
					error = "null character";
					return "";
				}
				result += DZS_Utf8.FromCodepoint(cp);
			}
			else
			{
				error = "bad escape";
				return "";
			}
		}
		return result;
	}

	protected static int ReadHex4(DZS_TextReader r, out string error)
	{
		int v = 0;
		string digits = "0123456789abcdef";
		for (int i = 0; i < 4; i++)
		{
			string h = r.Next();
			h.ToLower();
			int d = digits.IndexOf(h);
			if (h == "" || d < 0)
			{
				error = "bad hex";
				return 0;
			}
			v = v * 16 + d;
		}
		return v;
	}
}

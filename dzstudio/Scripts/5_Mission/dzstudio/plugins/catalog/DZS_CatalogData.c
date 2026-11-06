// One entry of the catalog: an object (class or model path) or a template.
class DZS_CatEntry : Managed
{
	string m_Key;          // class name or model path as written into a file; "tpl:<id>" for templates
	string m_Low;          // m_Key in lower case: favourites, recent and "steht im Katalog" use it
	string m_Root;         // config root of a class, "" for model paths and templates
	string m_Cat;          // category key
	string m_Name;         // Spielname
	string m_BaseName;     // name without image sources
	string m_Model;        // model path of the class (for loadability and the preview)
	bool m_IsTemplate;
	string m_TplId;
	int m_TplCount;
	int m_TplMissing;
	int m_Base;            // place in the order without search text
	// search forms (folded: lower case, ä/ö/ü as a/o/u, ß as ss)
	string m_NameF;
	string m_NameJoin;
	ref array<string> m_NameWords;
	string m_ClassF;
	string m_ClassJoin;
	ref array<string> m_ClassWords;
	ref array<int> m_WordIds;
}

class DZS_CatText
{
	// Lower case with ä/ö/ü as a/o/u and ß as ss.
	static string Fold(string s)
	{
		string r = s;
		r.ToLower();
		r.Replace("ä", "a");
		r.Replace("ö", "o");
		r.Replace("ü", "u");
		r.Replace("Ä", "a");
		r.Replace("Ö", "o");
		r.Replace("Ü", "u");
		r.Replace("ß", "ss");
		r.Replace("é", "e");
		r.Replace("è", "e");
		return r;
	}

	static bool IsSep(string c)
	{
		return c == " " || c == "_" || c == "-" || c == "\\" || c == "/" || c == "." || c == "(" || c == ")" || c == "," || c == "'" || c == ":";
	}

	static bool IsUpper(string c)
	{
		int a = c.ToAscii();
		return a >= 65 && a <= 90;
	}

	static bool IsLower(string c)
	{
		int a = c.ToAscii();
		return (a >= 97 && a <= 122) || a >= 128;
	}

	// Words of a name: split at separators and at changes from lower to upper case (ConcretePipe →
	// concrete, pipe); every word folded.
	static array<string> Words(string text)
	{
		string s = text;
		s.Replace("’", "'");
		array<string> r = new array<string>();
		string cur = "";
		string prev = "";
		int n = s.Length();
		for (int i = 0; i < n; i++)
		{
			string c = s.Get(i);
			if (IsSep(c))
			{
				if (cur != "")
					r.Insert(Fold(cur));
				cur = "";
				prev = "";
				continue;
			}
			if (cur != "" && IsUpper(c) && IsLower(prev))
			{
				r.Insert(Fold(cur));
				cur = "";
			}
			cur += c;
			prev = c;
		}
		if (cur != "")
			r.Insert(Fold(cur));
		return r;
	}

	// The whole name without separators, folded (Barrel_Green → barrelgreen).
	static string Joined(array<string> words)
	{
		string r = "";
		foreach (string w : words)
			r += w;
		return r;
	}

	// Readable name: Land_ and StaticObj_ off, file name of a model path, _ as space.
	static string Readable(string key)
	{
		string n = key;
		if (n.Contains("\\") || n.Contains("/"))
		{
			n.Replace("/", "\\");
			int slash = n.LastIndexOf("\\");
			n = n.Substring(slash + 1, n.Length() - slash - 1);
			string low = n;
			low.ToLower();
			if (low.Length() > 4 && low.Substring(low.Length() - 4, 4) == ".p3d")
				n = n.Substring(0, n.Length() - 4);
		}
		string l2 = n;
		l2.ToLower();
		if (l2.IndexOf("land_") == 0)
			n = n.Substring(5, n.Length() - 5);
		else if (l2.IndexOf("staticobj_") == 0)
			n = n.Substring(10, n.Length() - 10);
		n.Replace("_", " ");
		n.TrimInPlace();
		return n;
	}

	// A name the game gives with placeholder characters counts as missing.
	static bool IsUsableName(string s)
	{
		string t = s;
		t.TrimInPlace();
		return t != "" && t.IndexOf("$") != 0;
	}
}

// Damerau distance between a search word and the beginning of a word.
class DZS_CatTypo
{
	// Codes of a folded word.
	static array<int> Codes(string w)
	{
		array<int> r = new array<int>();
		int n = w.Length();
		for (int i = 0; i < n; i++)
			r.Insert(w.Get(i).ToAscii());
		return r;
	}

	// True if some beginning of w differs from q by at most k changes (missing, extra, wrong, swapped).
	static bool PrefixWithin(array<int> q, array<int> w, int k)
	{
		int n = q.Count();
		int m = Math.Min(w.Count(), n + k);
		if (m < n - k)
			return false;
		int cols = m + 1;
		array<int> d = new array<int>();
		d.Resize((n + 1) * cols);
		for (int j = 0; j <= m; j++)
			d[j] = j;
		for (int i = 1; i <= n; i++)
		{
			d[i * cols] = i;
			int rowMin = i;
			for (int j2 = 1; j2 <= m; j2++)
			{
				int cost = 1;
				if (q[i - 1] == w[j2 - 1])
					cost = 0;
				int v = d[(i - 1) * cols + j2] + 1;
				int ins = d[i * cols + j2 - 1] + 1;
				if (ins < v)
					v = ins;
				int sub = d[(i - 1) * cols + j2 - 1] + cost;
				if (sub < v)
					v = sub;
				if (i > 1 && j2 > 1 && q[i - 1] == w[j2 - 2] && q[i - 2] == w[j2 - 1])
				{
					int tr = d[(i - 2) * cols + j2 - 2] + 1;
					if (tr < v)
						v = tr;
				}
				d[i * cols + j2] = v;
				if (v < rowMin)
					rowMin = v;
			}
			if (rowMin > k)
				return false;
		}
		for (int L = Math.Max(0, n - k); L <= m; L++)
		{
			if (d[n * cols + L] <= k)
				return true;
		}
		return false;
	}
}

// Result of one search: entries in order and, per entry, where the first search word hit the Spielname.
class DZS_CatResult : Managed
{
	ref array<DZS_CatEntry> m_Items;
	ref map<DZS_CatEntry, int> m_HitAt;
	int m_HitLen;

	void DZS_CatResult()
	{
		m_Items = new array<DZS_CatEntry>();
		m_HitAt = new map<DZS_CatEntry, int>();
	}
}

// The catalog search over a set of entries.
class DZS_CatSearch
{
	static const int LEVEL_TYPO = 9;
	static const int NO_HIT = 99;

	// Level 1..8 for a plain hit, 9 for a typo hit, 99 for none.
	static int Level(DZS_CatEntry e, string q, map<int, bool> typoWords)
	{
		if (e.m_NameF == q)
			return 1;
		if (e.m_NameF.IndexOf(q) == 0)
			return 2;
		foreach (string w : e.m_NameWords)
		{
			if (w.IndexOf(q) == 0)
				return 3;
		}
		if (e.m_NameF.Contains(q) || e.m_NameJoin.Contains(q))
			return 4;
		if (e.m_ClassF != "")
		{
			if (e.m_ClassF == q)
				return 5;
			if (e.m_ClassF.IndexOf(q) == 0)
				return 6;
			foreach (string cw : e.m_ClassWords)
			{
				if (cw.IndexOf(q) == 0)
					return 7;
			}
			if (e.m_ClassF.Contains(q) || e.m_ClassJoin.Contains(q))
				return 8;
		}
		if (typoWords)
		{
			foreach (int id : e.m_WordIds)
			{
				if (typoWords.Contains(id))
					return LEVEL_TYPO;
			}
		}
		return NO_HIT;
	}

	// Plain hit only (exclusions have no typo tolerance).
	static bool Hits(DZS_CatEntry e, string q)
	{
		return Level(e, q, null) < LEVEL_TYPO;
	}

	// Splits the search text into words to find and words to exclude ("-holz"); a lone "-" is skipped.
	static void Parse(string text, array<string> want, array<string> skip)
	{
		array<string> parts = new array<string>();
		string folded = DZS_CatText.Fold(text);
		folded.Split(" ", parts);
		foreach (string p : parts)
		{
			if (p == "" || p == "-")
				continue;
			if (p.IndexOf("-") == 0)
				skip.Insert(p.Substring(1, p.Length() - 1));
			else
				want.Insert(p);
		}
	}
}

// The entries of the catalog: the console list checked against the running game, plus the templates.
// Loading runs in slices over several frames; until it is done the catalog shows "Loading catalog …".
class DZS_CatalogStore : Managed
{
	static const string LIST_IN_MOD = "dzstudio/data/console-list.txt";
	static const string LIST_IN_PROFILE = "$profile:dzstudio\\console-list.txt";
	static const int SLICE = 500;
	protected int m_Slice = SLICE;

	ref array<ref DZS_CatEntry> m_Objects;
	ref array<ref DZS_CatEntry> m_Templates;
	ref map<string, DZS_CatEntry> m_ByLow;
	ref array<DZS_CatEntry> m_All;          // objects and templates in the order without search text
	protected ref array<string> m_Lines;
	protected int m_Next;
	protected int m_Stage;                  // 0 read, 1 check, 2 index, 3 ready
	protected int m_Dropped;
	protected float m_StartedAt;
	float m_ReadyAfter = -1;
	protected bool m_German;
	// typo index: every word of every entry once, with its codes
	protected ref array<string> m_Words;
	protected ref array<ref array<int>> m_WordCodes;
	protected ref map<string, int> m_WordIndex;

	void DZS_CatalogStore()
	{
		m_Objects = new array<ref DZS_CatEntry>();
		m_Templates = new array<ref DZS_CatEntry>();
		m_ByLow = new map<string, DZS_CatEntry>();
		m_All = new array<DZS_CatEntry>();
		m_Words = new array<string>();
		m_WordCodes = new array<ref array<int>>();
		m_WordIndex = new map<string, int>();
		m_German = DZS_Texts.IsGerman();
	}

	bool IsReady() { return m_Stage >= 3; }
	protected int m_NameRev;
	int CountObjects() { return m_Objects.Count(); }

	void Start()
	{
		m_StartedAt = GetGame().GetTickTime();
		string slice;
		if (GetCLIParam("dzsCatalogSlice", slice) && slice.ToInt() > 0)
			m_Slice = slice.ToInt();
		m_Lines = new array<string>();
		string path = LIST_IN_MOD;
		if (FileExist(LIST_IN_PROFILE))
			path = LIST_IN_PROFILE;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (!fh)
		{
			DZS_Log.Error("catalog: console list not found at " + path);
			m_Stage = 2;
			return;
		}
		string line;
		while (FGets(fh, line) >= 0)
			m_Lines.Insert(line);
		CloseFile(fh);
		DZS_Log.Info("catalog: console list " + path + ", " + m_Lines.Count() + " lines");
		m_Stage = 1;
		m_Next = 0;
	}

	// One slice of work per frame.
	void Step()
	{
		if (m_Stage == 1)
		{
			int end = Math.Min(m_Lines.Count(), m_Next + m_Slice);
			for (int i = m_Next; i < end; i++)
				CheckLine(m_Lines[i]);
			m_Next = end;
			if (m_Next >= m_Lines.Count())
			{
				m_Stage = 2;
				m_Lines = null;
			}
			return;
		}
		if (m_Stage == 2)
		{
			Finish();
			m_Stage = 3;
		}
	}

	protected void CheckLine(string line)
	{
		if (line == "" || line.IndexOf("#") == 0)
			return;
		line.Replace((13).AsciiToString(), "");
		array<string> p = new array<string>();
		line.Split("\t", p);
		if (p.Count() < 2)
			return;
		string key = p[0];
		DZS_CatEntry e = new DZS_CatEntry();
		e.m_Key = key;
		e.m_Low = key;
		e.m_Low.ToLower();
		e.m_Cat = p[1];
		if (key.Contains("\\") || key.Contains("/"))
		{
			if (!FileExist(key))
			{
				m_Dropped++;
				return;
			}
			e.m_Model = key;
		}
		else
		{
			e.m_Root = RootOf(key);
			if (e.m_Root == "")
			{
				m_Dropped++;
				return;
			}
			string model;
			GetGame().ConfigGetText(e.m_Root + " " + key + " model", model);
			if (model == "" || !FileExist(TrimSlash(model)))
			{
				m_Dropped++;
				return;
			}
			e.m_Model = model;
		}
		string listName = "";
		if (p.Count() >= 4)
		{
			if (m_German)
				listName = p[2];
			else
				listName = p[3];
		}
		e.m_Name = DZS_Texts.Shown(GameName(e, listName));
		e.m_BaseName = e.m_Name;
		m_Objects.Insert(e);
		m_ByLow.Set(e.m_Low, e);
	}

	static string TrimSlash(string path)
	{
		if (path.IndexOf("\\") == 0)
			return path.Substring(1, path.Length() - 1);
		return path;
	}

	static string RootOf(string cls)
	{
		if (GetGame().ConfigIsExisting("CfgVehicles " + cls))
			return "CfgVehicles";
		if (GetGame().ConfigIsExisting("CfgWeapons " + cls))
			return "CfgWeapons";
		if (GetGame().ConfigIsExisting("CfgMagazines " + cls))
			return "CfgMagazines";
		return "";
	}

	// The game's name, else the list's, else the readable class or file name.
	protected string GameName(DZS_CatEntry e, string listName)
	{
		if (e.m_Root != "")
		{
			string shown;
			GetGame().ConfigGetText(e.m_Root + " " + e.m_Key + " displayName", shown);
			if (DZS_CatText.IsUsableName(shown))
				return shown;
		}
		if (DZS_CatText.IsUsableName(listName))
			return listName;
		return DZS_CatText.Readable(e.m_Key);
	}

	// An image source's name in the game's language comes first. Returns how many changed.
	int ApplySourceNames(array<ref DZS_ImageSource> sources)
	{
		int changed = 0;
		foreach (DZS_CatEntry e : m_Objects)
		{
			string want = e.m_BaseName;
			foreach (DZS_ImageSource s : sources)
			{
				string got = s.GetName(e.m_Key, m_German);
				if (DZS_CatText.IsUsableName(got))
				{
					want = got;
					break;
				}
			}
			want = DZS_Texts.Shown(want);
			if (want != e.m_Name)
			{
				e.m_Name = want;
				m_NameRev++;
				Index(e);
				changed++;
			}
		}
		return changed;
	}

	protected void Finish()
	{
		foreach (DZS_CatEntry e : m_Objects)
			Index(e);
		Reorder();
		m_ReadyAfter = GetGame().GetTickTime() - m_StartedAt;
		DZS_Log.Info("catalog: " + m_Objects.Count() + " objects offered, " + m_Dropped + " not in this game, ready after " + Math.Round(m_ReadyAfter * 1000) + " ms");
	}

	void Index(DZS_CatEntry e)
	{
		e.m_NameF = DZS_CatText.Fold(e.m_Name);
		e.m_NameWords = DZS_CatText.Words(e.m_Name);
		e.m_NameJoin = DZS_CatText.Joined(e.m_NameWords);
		e.m_WordIds = new array<int>();
		if (e.m_IsTemplate)
		{
			e.m_ClassF = "";
			e.m_ClassWords = new array<string>();
			e.m_ClassJoin = "";
		}
		else
		{
			e.m_ClassF = DZS_CatText.Fold(e.m_Key);
			e.m_ClassWords = DZS_CatText.Words(e.m_Key);
			e.m_ClassJoin = DZS_CatText.Joined(e.m_ClassWords);
		}
		foreach (string w : e.m_NameWords)
			e.m_WordIds.Insert(WordId(w));
		foreach (string cw : e.m_ClassWords)
			e.m_WordIds.Insert(WordId(cw));
	}

	protected int WordId(string w)
	{
		int id;
		if (m_WordIndex.Find(w, id))
			return id;
		id = m_Words.Count();
		m_Words.Insert(w);
		array<int> t217 = DZS_CatTypo.Codes(w);
		m_WordCodes.Insert(t217);
		m_WordIndex.Set(w, id);
		return id;
	}

	// Order without search text: game name (folded), then class name.
	void Reorder()
	{
		m_All.Clear();
		array<string> keys = new array<string>();
		array<DZS_CatEntry> all = new array<DZS_CatEntry>();
		foreach (DZS_CatEntry o : m_Objects)
			all.Insert(o);
		foreach (DZS_CatEntry t : m_Templates)
			all.Insert(t);
		for (int i = 0; i < all.Count(); i++)
		{
			DZS_CatEntry e = all[i];
			string cls = e.m_ClassF;
			keys.Insert(e.m_NameF + "\t" + cls + "\t" + i.ToString());
		}
		keys.Sort();
		for (int k = 0; k < keys.Count(); k++)
		{
			array<string> parts = new array<string>();
			keys[k].Split("\t", parts);
			DZS_CatEntry s = all[parts[parts.Count() - 1].ToInt()];
			s.m_Base = k;
			m_All.Insert(s);
		}
	}

	protected ref map<string, int> m_NameCounts;
	protected string m_NameCountsSig;

	// Objects (not templates) per game name; counted again when the list or a name changed.
	int NameCount(string name)
	{
		int nObj = m_Objects.Count();
		string sObj = nObj.ToString();
		string sRev = m_NameRev.ToString();
		string sig = sObj + "|" + sRev;
		if (!m_NameCounts || sig != m_NameCountsSig)
		{
			m_NameCounts = new map<string, int>();
			m_NameCountsSig = sig;
			foreach (DZS_CatEntry e : m_Objects)
				m_NameCounts.Set(e.m_Name, m_NameCounts.Get(e.m_Name) + 1);
		}
		return m_NameCounts.Get(name);
	}

	DZS_CatEntry Find(string key)
	{
		string low = key;
		low.ToLower();
		return m_ByLow.Get(low);
	}

	// ---- templates ----

	DZS_CatEntry AddTemplate(string id, string name, int count, int missing)
	{
		DZS_CatEntry e = new DZS_CatEntry();
		e.m_IsTemplate = true;
		e.m_TplId = id;
		e.m_Key = "tpl:" + id;
		e.m_Low = e.m_Key;
		e.m_Cat = "templates";
		e.m_Name = name;
		e.m_TplCount = count;
		e.m_TplMissing = missing;
		Index(e);
		m_Templates.Insert(e);
		m_ByLow.Set(e.m_Low, e);
		return e;
	}

	void RemoveTemplate(string id)
	{
		for (int i = 0; i < m_Templates.Count(); i++)
		{
			if (m_Templates[i].m_TplId == id)
			{
				m_ByLow.Remove(m_Templates[i].m_Low);
				m_Templates.RemoveOrdered(i);
				break;
			}
		}
		Reorder();
	}

	void RenameTemplate(string id, string name)
	{
		foreach (DZS_CatEntry t : m_Templates)
		{
			if (t.m_TplId == id)
			{
				t.m_Name = name;
				Index(t);
			}
		}
		Reorder();
	}

	// ---- search ----

	// Typo hits: ids of words whose beginning is within 1 change (4+ letters) or 2 (8+ letters).
	protected map<int, bool> TypoWords(string q)
	{
		int n = q.Length();
		if (n < 4)
			return null;
		int k = 1;
		if (n >= 8)
			k = 2;
		array<int> qc = DZS_CatTypo.Codes(q);
		map<int, bool> r = new map<int, bool>();
		for (int i = 0; i < m_Words.Count(); i++)
		{
			array<int> wc = m_WordCodes[i];
			if (wc.Count() < n - k)
				continue;
			if (k == 1 && qc[0] != wc[0] && qc[1] != wc[1] && qc[0] != wc[1] && qc[1] != wc[0])
				continue;
			if (DZS_CatTypo.PrefixWithin(qc, wc, k))
				r.Set(i, true);
		}
		return r;
	}

	// Entries of a scope ("all", "favorites", "recent", or a category key) matching the text, in the order
	// of the search (weakest word counts, then the order without search text).
	DZS_CatResult Search(string text, array<DZS_CatEntry> scope, bool keepScopeOrder)
	{
		DZS_CatResult res = new DZS_CatResult();
		array<string> want = new array<string>();
		array<string> skip = new array<string>();
		DZS_CatSearch.Parse(text, want, skip);
		array<ref map<int, bool>> typos = new array<ref map<int, bool>>();
		foreach (string w : want)
			typos.Insert(TypoWords(w));
		array<int> keys = new array<int>();
		array<DZS_CatEntry> hits = new array<DZS_CatEntry>();
		for (int i = 0; i < scope.Count(); i++)
		{
			DZS_CatEntry e = scope[i];
			bool excluded = false;
			foreach (string s : skip)
			{
				if (s != "" && DZS_CatSearch.Hits(e, s))
				{
					excluded = true;
					break;
				}
			}
			if (excluded)
				continue;
			int worst = 0;
			for (int j = 0; j < want.Count(); j++)
			{
				int lv = DZS_CatSearch.Level(e, want[j], typos[j]);
				if (lv > worst)
					worst = lv;
				if (worst == DZS_CatSearch.NO_HIT)
					break;
			}
			if (worst == DZS_CatSearch.NO_HIT)
				continue;
			int order = e.m_Base;
			if (keepScopeOrder)
				order = i;
			if (want.Count() == 0 || keepScopeOrder)
				worst = 0;
			keys.Insert(worst * 100000 + order);
			hits.Insert(e);
		}
		map<int, DZS_CatEntry> byKey = new map<int, DZS_CatEntry>();
		for (int h = 0; h < keys.Count(); h++)
			byKey.Set(keys[h], hits[h]);
		keys.Sort();
		foreach (int key : keys)
			res.m_Items.Insert(byKey.Get(key));
		if (want.Count() > 0)
		{
			string first = want[0];
			res.m_HitLen = first.Length();
			foreach (DZS_CatEntry he : res.m_Items)
			{
				int at = he.m_NameF.IndexOf(first);
				if (at >= 0)
					res.m_HitAt.Set(he, at);
			}
		}
		return res;
	}
}

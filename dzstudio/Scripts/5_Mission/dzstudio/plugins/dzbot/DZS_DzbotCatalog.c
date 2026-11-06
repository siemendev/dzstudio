// dzbot as image source of the catalog: data of all entries once per
// revision (names in German and English, descriptions, image keys), images one by one for visible tiles, all kept in
// the profile folder. Catalogue requests never carry a token; their errors change nothing and show nothing.
class DZS_DzbotCatEntry : Managed
{
	string m_De;
	string m_En;
	string m_DescDe;
	string m_DescEn;
	string m_Image;
}

class DZS_DzbotImageAsk : Managed
{
	string m_Key;
}

class DZS_DzbotCatalog : DZS_ImageSource
{
	static const string DIR = "$profile:dzstudio\\dzbot";
	static const string DATA = "$profile:dzstudio\\dzbot\\catalog.jsonl";
	static const string REV = "$profile:dzstudio\\dzbot\\catalog-revision.txt";
	static const string IMAGES = "$profile:dzstudio\\dzbot\\images";

	DZS_DzbotPlugin m_P;
	string m_Revision;
	ref map<string, ref DZS_DzbotCatEntry> m_Entries = new map<string, ref DZS_DzbotCatEntry>();
	protected ref map<string, string> m_Have = new map<string, string>();
	protected ref map<string, bool> m_Asked = new map<string, bool>();
	protected ref map<string, bool> m_Settled = new map<string, bool>();
	protected ref map<string, bool> m_Batch = new map<string, bool>();
	protected bool m_DataAsked;
	protected ref array<ref DZS_DzbotImageJob> m_Jobs = new array<ref DZS_DzbotImageJob>();
	protected ref array<string> m_Lines;
	protected int m_LineAt;
	protected string m_LoadingRev;
	protected bool m_Registered;
	int m_ImagesWritten;

	void Start()
	{
		DZS_Files.MakeDirs(IMAGES);
		array<string> rev = DZS_Files.Read(REV);
		if (rev && FileExist(DATA))
		{
			string r = DZS_Files.Join(rev);
			r.Trim();
			m_Revision = r;
			array<string> chunks = DZS_Files.Read(DATA);
			BeginLines(chunks, r);
			DZS_Log.Info("dzbot: catalogue data on disk, revision " + r);
		}
		else
			m_DataAsked = true;
		m_P.m_Client.Get("cat", "catalog.revision", "/editor/catalog/revision");
	}

	void OnRevision(DZS_DzbotReply r)
	{
		m_DataAsked = false;
		if (!r.Ok())
			return;
		string rev = r.Str("revision");
		DZS_Log.Info("dzbot: catalogue revision " + rev + ", held " + m_Revision);
		if (rev != "" && rev != m_Revision)
		{
			m_DataAsked = true;
			m_P.m_Client.Get("cat", "catalog.data", "/editor/catalog");
		}
	}

	void OnData(DZS_DzbotReply r)
	{
		m_DataAsked = false;
		if (!r.Ok())
			return;
		DZS_DzbotSection sec = null;
		foreach (DZS_DzbotSection s : r.m_Sections)
		{
			if (s.m_Kind == "catalog")
				sec = s;
		}
		if (!sec)
			return;
		array<string> chunks = sec.m_Chunks;
		DZS_Files.MakeDirs(DIR);
		if (!DZS_Files.Write(DATA + ".tmp", chunks))
			return;
		DeleteFile(DATA);
		CopyFile(DATA + ".tmp", DATA);
		DeleteFile(DATA + ".tmp");
		array<string> rv = new array<string>();
		rv.Insert(sec.m_Rev);
		DZS_Files.Write(REV, rv);
		m_Revision = sec.m_Rev;
		int stored = DZS_Files.Length(chunks);
		DZS_Log.Info("dzbot: catalogue data revision " + sec.m_Rev + ", " + stored + " bytes stored of " + sec.m_Len);
		BeginLines(chunks, sec.m_Rev);
	}

	protected void BeginLines(array<string> chunks, string rev)
	{
		m_Lines = new array<string>();
		string cur = "";
		foreach (string c : chunks)
		{
			int from = 0;
			int n = c.Length();
			while (from < n)
			{
				int nl = c.IndexOfFrom(from, "\n");
				if (nl < 0)
				{
					cur += c.Substring(from, n - from);
					break;
				}
				cur += c.Substring(from, nl - from);
				if (cur != "")
					m_Lines.Insert(cur);
				cur = "";
				from = nl + 1;
			}
		}
		if (cur != "")
			m_Lines.Insert(cur);
		m_LineAt = 0;
		m_LoadingRev = rev;
		m_Entries.Clear();
	}

	protected void StepLines()
	{
		if (!m_Lines)
			return;
		int stop = Math.Min(m_LineAt + 150, m_Lines.Count());
		while (m_LineAt < stop)
		{
			string line = m_Lines[m_LineAt];
			m_LineAt++;
			array<string> one = new array<string>();
			one.Insert(line);
			DZS_SrcDoc doc = DZS_SrcJson.Parse(one);
			if (!doc || !doc.m_Root || !doc.m_Root.IsObj())
				continue;
			DZS_SrcNode root = doc.m_Root;
			string name = Text(root, "name");
			if (name == "")
				continue;
			DZS_DzbotCatEntry e = new DZS_DzbotCatEntry();
			e.m_De = Text(root, "de");
			e.m_En = Text(root, "en");
			e.m_DescDe = Text(root, "descDe");
			e.m_DescEn = Text(root, "descEn");
			e.m_Image = Text(root, "image");
			string low = name;
			low.ToLower();
			m_Entries.Set(low, e);
		}
		if (m_LineAt < m_Lines.Count())
			return;
		m_Lines = null;
		int images = 0;
		map<string, bool> keys = new map<string, bool>();
		for (int i = 0; i < m_Entries.Count(); i++)
		{
			DZS_DzbotCatEntry ce = m_Entries.GetElement(i);
			if (ce.m_Image != "")
			{
				keys.Set(ce.m_Image, true);
				images++;
			}
		}
		int removed = 0;
		array<string> names;
		array<bool> folders;
		DZS_Files.List(IMAGES, names, folders);
		for (int k = 0; k < names.Count(); k++)
		{
			string f = names[k];
			int dot = f.LastIndexOf(".");
			if (dot <= 0)
				continue;
			string key = f.Substring(0, dot);
			if (!keys.Contains(key))
			{
				DeleteFile(IMAGES + "\\" + f);
				removed++;
			}
		}
		m_Have.Clear();
		m_Asked.Clear();
		m_Settled.Clear();
		m_Batch.Clear();
		DZS_Log.Info("dzbot: catalogue " + m_Entries.Count() + " entries, " + images + " with image, revision " + m_LoadingRev + ", " + removed + " old image(s) deleted");
		DZS_CatalogApi api = DZS_CatalogApi.Get();
		if (api)
		{
			if (!m_Registered)
			{
				m_Registered = true;
				api.AddImageSource(this);
			}
			else
				api.SourcesChanged();
		}
	}

	static string Text(DZS_SrcNode o, string key)
	{
		DZS_SrcNode n = o.Get(key);
		if (n && n.IsStr())
			return n.m_Str;
		return "";
	}

	// Registers with the catalog once a session has it (the catalog lives per session), also while the
	// catalogue data is still on its way, so tiles can show that their pictures are coming.
	void OnSession()
	{
		m_Registered = false;
		m_Have.Clear();
		m_Asked.Clear();
		m_Settled.Clear();
		m_Batch.Clear();
		DZS_CatalogApi api = DZS_CatalogApi.Get();
		if (api)
		{
			m_Registered = true;
			api.AddImageSource(this);
		}
	}

	protected bool DataBusy()
	{
		return m_Lines != null || (m_DataAsked && m_Entries.Count() == 0);
	}

	protected bool Waiting(string key)
	{
		return m_Asked.Contains(key) && !m_Have.Contains(key) && !m_Settled.Contains(key);
	}

	override bool PhotoPending(string name)
	{
		if (m_P.m_Client.m_Blocked)
			return false;
		if (DataBusy())
			return true;
		DZS_DzbotCatEntry e = EntryOf(name);
		if (!e)
			return false;
		return e.m_Image != "" && Waiting(e.m_Image);
	}

	override bool PhotoProgress(out int done, out int total)
	{
		done = 0;
		total = 0;
		if (m_P.m_Client.m_Blocked)
		{
			m_Batch.Clear();
			return false;
		}
		if (DataBusy())
			return true;
		total = m_Batch.Count();
		for (int i = 0; i < total; i++)
		{
			if (!Waiting(m_Batch.GetKey(i)))
				done++;
		}
		if (done < total)
			return true;
		m_Batch.Clear();
		return false;
	}

	void Update()
	{
		StepLines();
		int budget = 40000;
		while (m_Jobs.Count() > 0 && budget > 0)
		{
			int t0 = TickCount(0);
			DZS_DzbotImageJob job = m_Jobs[0];
			bool done = job.Step(budget);
			budget -= TickCount(0) - t0;
			if (!done)
				break;
			m_Jobs.Remove(0);
			FinishImage(job);
		}
	}

	protected DZS_DzbotCatEntry EntryOf(string name)
	{
		string low = name;
		low.ToLower();
		return m_Entries.Get(low);
	}

	override string GetName(string name, bool german)
	{
		DZS_DzbotCatEntry e = EntryOf(name);
		if (!e)
			return "";
		if (german)
			return e.m_De;
		return e.m_En;
	}

	override string GetDescription(string name)
	{
		DZS_DzbotCatEntry e = EntryOf(name);
		if (!e)
			return "";
		if (DZS_Texts.IsGerman())
			return e.m_DescDe;
		return e.m_DescEn;
	}

	// The catalog asks only for visible tiles: the first question fetches the image. Nothing is fetched while the
	// list is read in: its end clears what was asked and deletes files it does not know, half-written ones too.
	override string GetPhoto(string name)
	{
		if (m_Lines)
			return "";
		DZS_DzbotCatEntry e = EntryOf(name);
		if (!e || e.m_Image == "")
			return "";
		string have;
		if (m_Have.Find(e.m_Image, have))
			return have;
		string dds = IMAGES + "\\" + e.m_Image + ".dds";
		string tga = IMAGES + "\\" + e.m_Image + ".tga";
		if (FileExist(dds))
		{
			m_Have.Set(e.m_Image, dds);
			return dds;
		}
		if (FileExist(tga))
		{
			m_Have.Set(e.m_Image, tga);
			return tga;
		}
		if (!m_Asked.Contains(e.m_Image))
		{
			m_Asked.Set(e.m_Image, true);
			m_Batch.Set(e.m_Image, true);
			DZS_DzbotImageAsk ask = new DZS_DzbotImageAsk();
			ask.m_Key = e.m_Image;
			m_P.m_Client.Get("cat", "catalog.image", "/editor/catalog/image/" + e.m_Image, ask);
		}
		return "";
	}

	void OnImage(DZS_DzbotCall call, DZS_DzbotReply r)
	{
		string asked = "";
		DZS_DzbotImageAsk ask = DZS_DzbotImageAsk.Cast(call.m_Data);
		if (ask)
		{
			asked = ask.m_Key;
			m_Settled.Set(asked, true);
		}
		if (!r.Ok())
			return;
		string key = r.Str("key");
		string fmt = r.Str("format");
		if (key == "" || (fmt != "dds" && fmt != "tga"))
			return;
		if (key == asked)
			m_Settled.Remove(key);
		DZS_DzbotImageJob job = new DZS_DzbotImageJob();
		job.m_Key = key;
		job.m_Path = IMAGES + "\\" + key + "." + fmt;
		int parts = 0;
		int longest = 0;
		foreach (DZS_DzbotSection s : r.m_Sections)
		{
			if (s.m_Kind != "image" || s.m_Key != key)
				continue;
			foreach (string piece : s.m_Chunks)
				job.m_Parts.Insert(piece);
			parts++;
			if (s.m_Len > longest)
				longest = s.m_Len;
		}
		job.m_Expect = r.Int("bytes");
		DZS_Log.Info("dzbot: image " + key + " " + fmt + " " + parts + " part(s), longest " + longest + " chars, " + job.m_Expect + " bytes expected");
		if (parts == 0 || !job.Begin())
		{
			m_Settled.Set(key, true);
			return;
		}
		m_Jobs.Insert(job);
	}

	protected void FinishImage(DZS_DzbotImageJob job)
	{
		if (job.m_Failed || (job.m_Expect > 0 && job.m_Written != job.m_Expect))
		{
			DZS_Log.Warn("dzbot: image " + job.m_Key + " wrong size " + job.m_Written + " of " + job.m_Expect);
			DeleteFile(job.m_Path);
			m_Settled.Set(job.m_Key, true);
			DZS_CatalogApi failApi = DZS_CatalogApi.Get();
			if (failApi)
				failApi.PhotosChanged();
			return;
		}
		m_ImagesWritten++;
		m_Have.Set(job.m_Key, job.m_Path);
		DZS_Log.Info("dzbot: image " + job.m_Key + " written, " + job.m_Written + " bytes");
		DZS_CatalogApi api = DZS_CatalogApi.Get();
		if (api)
			api.PhotosChanged();
	}
}

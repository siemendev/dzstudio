// Name dialog for templates: 1–60 characters
// without spaces at the ends, unique among the templates regardless of case.
class DZS_TemplateNameDialog : DZS_Dialog
{
	protected DZS_CatalogTemplates m_T;
	protected ref DZS_TextField m_Field;
	protected TextWidget m_Label;
	protected TextWidget m_Note;
	protected TextWidget m_Error;
	protected string m_Initial;
	protected string m_OwnId;
	protected string m_NoteText;
	protected string m_Blocked;
	protected string m_MainTag;
	ref array<ref DZS_TemplateObject> m_Objects;

	void DZS_TemplateNameDialog(DZS_CatalogTemplates t, string title, string mainLabel, string mainTag, string initial, string ownId, string note, string blocked)
	{
		m_T = t;
		m_Initial = initial;
		m_OwnId = ownId;
		m_NoteText = note;
		m_Blocked = blocked;
		m_MainTag = mainTag;
		Setup(SMALL, DZS_Texts.Get("catalog.tpl.kicker"), title, true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		AddFootButton(mainTag, mainLabel, "main");
	}

	override void BuildContent(Widget inner)
	{
		m_Note = DZS_W.Text(inner, DZS_Font.READ, 13, DZS_Color.Get("warning"));
		m_Note.SetText(m_NoteText);
		m_Label = DZS_W.Text(inner, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		m_Label.SetText(DZS_Texts.Upper(DZS_Texts.Get("catalog.tpl.name")));
		m_Field = new DZS_TextField("catalog.tplName", m_Initial, "textMuted", 372);
		m_Field.Build(inner);
		m_Field.m_OnCommit.Insert(OnFieldCommit);
		m_Error = DZS_W.Text(inner, DZS_Font.READ, 12, DZS_Color.Get("danger"));
		AddFocusField(m_Field);
		AddFootFocus();
	}

	void OnFieldCommit(DZS_NumberField f, string typed, DZS_FieldResult r)
	{
		r.m_Ok = true;
	}

	string Value()
	{
		string t = m_Field.GetText();
		if (t == "" && !m_Field.IsFocused())
			t = m_Initial;
		else if (t == "")
			t = m_Initial;
		t.TrimInPlace();
		return t;
	}

	override float LayoutContent(float width)
	{
		float y = 0;
		m_Note.Show(m_NoteText != "");
		if (m_NoteText != "")
		{
			DZS_W.Place(m_Note, 0, 0, width, DZS_Scale.Px(20));
			y += DZS_Scale.Px(26);
		}
		DZS_W.Place(m_Label, 0, y, width, DZS_Scale.Px(14));
		m_Field.Place(0, y + DZS_Scale.Px(18), DZS_Scale.Px(28));
		DZS_W.Place(m_Error, 0, y + DZS_Scale.Px(52), width, DZS_Scale.Px(18));
		return y + DZS_Scale.Px(72);
	}

	override void UpdateContent(float dt)
	{
		m_Field.Refresh();
		string why = m_T.NameProblem(Value(), m_OwnId);
		m_Error.SetText(why);
		m_Field.SetError(why != "");
		DZS_Button main = FootButton(m_MainTag);
		bool ok = why == "" && m_Blocked == "";
		string reason = why;
		if (m_Blocked != "")
			reason = m_Blocked;
		if (main && main.m_Hit.m_Enabled != ok)
			main.SetEnabled(ok, reason);
	}

	override void OnFoot(string tag)
	{
		if (tag == m_MainTag)
		{
			string name = Value();
			if (m_T.NameProblem(name, m_OwnId) != "" || m_Blocked != "")
				return;
			Close();
			if (tag == "rename")
				m_T.Rename(m_OwnId, name);
			else
				m_T.Save(name, m_Objects);
			return;
		}
		Close();
	}
}

// Templates on disk: one file per template in $profile:dzstudio\templates\,
// named by a random 16-character id; written to <id>.tmp first, then copied over and the .tmp deleted.
class DZS_CatalogTemplates : Managed
{
	static const string DIR = "$profile:dzstudio\\templates";
	static const int VERSION = 1;
	static const int MAX_OBJECTS = 2000;

	protected DZS_CatalogPlugin m_P;
	protected ref map<string, ref array<ref DZS_TemplateObject>> m_Objects;

	void DZS_CatalogTemplates(DZS_CatalogPlugin p)
	{
		m_P = p;
		m_Objects = new map<string, ref array<ref DZS_TemplateObject>>();
	}

	protected static string PathOf(string id, string ext)
	{
		return DIR + "\\" + id + ext;
	}

	void LoadAll()
	{
		DZS_Files.MakeDirs(DIR);
		array<string> names;
		array<bool> folders;
		DZS_Files.List(DIR, names, folders);
		map<string, bool> ids = new map<string, bool>();
		for (int i = 0; i < names.Count(); i++)
		{
			if (folders[i])
				continue;
			string low = names[i];
			low.ToLower();
			int dot = low.LastIndexOf(".");
			if (dot <= 0)
				continue;
			string ext = low.Substring(dot, low.Length() - dot);
			if (ext == ".json" || ext == ".tmp")
				ids.Set(names[i].Substring(0, dot), true);
		}
		foreach (string id, bool b : ids)
			LoadOne(id);
		m_P.TemplatesChanged();
	}

	// A readable .tmp replaces an unreadable or missing file of the same id; otherwise it is deleted.
	protected void LoadOne(string id)
	{
		string main = PathOf(id, ".json");
		string tmp = PathOf(id, ".tmp");
		string error;
		DZS_Json mj = null;
		if (FileExist(main))
		{
			array<string> t163 = DZS_Files.Read(main);
			mj = DZS_Json.Parse(t163, error);
		}
		if (FileExist(tmp))
		{
			string terr;
			array<string> t167 = DZS_Files.Read(tmp);
			DZS_Json tj = DZS_Json.Parse(t167, terr);
			if (tj && Valid(tj) && (!mj || !Valid(mj)))
			{
				if (DZS_Files.Copy(tmp, main))
				{
					DeleteFile(tmp);
					mj = tj;
					DZS_Log.Info("catalog: template " + id + " restored from its .tmp");
				}
			}
			else
				DeleteFile(tmp);
		}
		if (!mj)
		{
			if (FileExist(main))
				DZS_Log.Warn("catalog: template file " + id + ".json unreadable, skipped: " + error);
			return;
		}
		if (mj.GetInt("version", 0) > VERSION)
		{
			DZS_Log.Warn("catalog: template file " + id + ".json has version " + mj.GetInt("version", 0) + ", skipped");
			return;
		}
		if (!Valid(mj))
		{
			DZS_Log.Warn("catalog: template file " + id + ".json is no template, skipped");
			return;
		}
		array<ref DZS_TemplateObject> objs = FromJson(mj);
		m_Objects.Set(id, objs);
		m_P.m_Store.AddTemplate(id, mj.GetStr("name"), objs.Count(), 0);
	}

	protected static bool Valid(DZS_Json j)
	{
		if (!j || !j.IsObj())
			return false;
		string name = j.GetStr("name");
		DZS_Json objs = j.Get("objects");
		return name != "" && objs && objs.IsArr() && j.GetInt("version", 0) >= 1;
	}

	protected static float NumOf(DZS_Json j, float def)
	{
		if (!j || !j.IsNum())
			return def;
		return j.m_Str.ToFloat();
	}

	protected static vector VecOf(DZS_Json a)
	{
		if (!a || !a.IsArr() || a.Count() != 3)
			return vector.Zero;
		return Vector(NumOf(a.At(0), 0), NumOf(a.At(1), 0), NumOf(a.At(2), 0));
	}

	// Missing fields take the Object-Spawner defaults; an object without a name is passed over.
	protected static array<ref DZS_TemplateObject> FromJson(DZS_Json j)
	{
		array<ref DZS_TemplateObject> r = new array<ref DZS_TemplateObject>();
		DZS_Json objs = j.Get("objects");
		for (int i = 0; i < objs.Count(); i++)
		{
			DZS_Json o = objs.At(i);
			if (!o.IsObj() || o.GetStr("name") == "")
				continue;
			DZS_TemplateObject t = new DZS_TemplateObject();
			t.m_Name = o.GetStr("name");
			t.m_Pos = VecOf(o.Get("pos"));
			t.m_Ypr = VecOf(o.Get("ypr"));
			t.m_Scale = NumOf(o.Get("scale"), 1);
			t.m_Persist = o.GetInt("enableCEPersistency", 0) == 1;
			t.m_Custom = o.GetStr("customString");
			r.Insert(t);
		}
		return r;
	}

	protected static DZS_Json Vec(vector v, int decimals)
	{
		DZS_Json a = DZS_Json.Arr();
		for (int i = 0; i < 3; i++)
			a.Add(DZS_Json.Num(DZS_Num.Fixed(v[i], decimals, true)));
		return a;
	}

	protected static DZS_Json ToJson(string name, array<ref DZS_TemplateObject> objs)
	{
		DZS_Json j = DZS_Json.Obj();
		j.Set("version", DZS_Json.Int(VERSION));
		j.Set("name", DZS_Json.Str(name));
		DZS_Json arr = DZS_Json.Arr();
		foreach (DZS_TemplateObject o : objs)
		{
			DZS_Json e = DZS_Json.Obj();
			e.Set("name", DZS_Json.Str(o.m_Name));
			DZS_Json t264 = Vec(o.m_Pos, 4);
			e.Set("pos", t264);
			DZS_Json t265 = Vec(o.m_Ypr, 3);
			e.Set("ypr", t265);
			e.Set("scale", DZS_Json.Num(DZS_Num.Fixed(o.m_Scale, 4, true)));
			int persist = 0;
			if (o.m_Persist)
				persist = 1;
			e.Set("enableCEPersistency", DZS_Json.Int(persist));
			e.Set("customString", DZS_Json.Str(o.m_Custom));
			arr.Add(e);
		}
		j.Set("objects", arr);
		return j;
	}

	protected bool WriteFile(string id, DZS_Json j)
	{
		DZS_Files.MakeDirs(DIR);
		string tmp = PathOf(id, ".tmp");
		array<string> t282 = j.ToChunks(true);
		if (!DZS_Files.Write(tmp, t282))
			return false;
		if (!DZS_Files.Copy(tmp, PathOf(id, ".json")))
			return false;
		DeleteFile(tmp);
		return true;
	}

	string NameProblem(string name, string ownId)
	{
		string t = name;
		t.TrimInPlace();
		int chars = DZS_Names.Chars(t);
		if (chars < 1)
			return DZS_Texts.Get("catalog.tpl.noName");
		if (chars > 60)
			return DZS_Texts.Get("catalog.tpl.noName");
		string low = t;
		low.ToLower();
		foreach (DZS_CatEntry e : m_P.m_Store.m_Templates)
		{
			if (e.m_TplId == ownId)
				continue;
			string other = e.m_Name;
			other.ToLower();
			if (other == low)
				return DZS_Texts.Get("catalog.tpl.taken");
		}
		return "";
	}

	// Only objects whose name is in the catalog go into a template.
	string Save(string name, array<ref DZS_TemplateObject> objects)
	{
		if (!objects || NameProblem(name, "") != "")
			return "";
		array<ref DZS_TemplateObject> keep = new array<ref DZS_TemplateObject>();
		foreach (DZS_TemplateObject o : objects)
		{
			DZS_CatEntry ce = m_P.m_Store.Find(o.m_Name);
			if (ce && !ce.m_IsTemplate)
				keep.Insert(o);
		}
		if (keep.Count() == 0 || keep.Count() > MAX_OBJECTS)
			return "";
		string id;
		while (true)
		{
			id = DZS_Random.Code(16);
			if (!FileExist(PathOf(id, ".json")) && !FileExist(PathOf(id, ".tmp")))
				break;
		}
		string clean = name;
		clean.TrimInPlace();
		int t0 = TickCount(0);
		DZS_Json t337 = ToJson(clean, keep);
		if (!WriteFile(id, t337))
		{
			DZS_Log.Error("catalog: template " + clean + " could not be written");
			return "";
		}
		DZS_Log.Info("catalog: template '" + clean + "' saved as " + id + " with " + keep.Count() + " object(s) in " + (TickCount(0) - t0) / 10000 + " ms");
		m_Objects.Set(id, keep);
		m_P.m_Store.AddTemplate(id, clean, keep.Count(), 0);
		m_P.TemplatesChanged();
		m_P.Kernel().GetScreen().ShowMessage("catalog.tpl", "success", DZS_Texts.Fmt("catalog.tpl.saved", DZS_W.Literal(clean)), 3.0);
		return id;
	}

	array<ref DZS_TemplateObject> Objects(string id)
	{
		return m_Objects.Get(id);
	}

	void CountMissing()
	{
		foreach (DZS_CatEntry e : m_P.m_Store.m_Templates)
		{
			array<ref DZS_TemplateObject> objs = m_Objects.Get(e.m_TplId);
			int missing = 0;
			if (objs)
			{
				foreach (DZS_TemplateObject o : objs)
				{
					DZS_CatEntry ce = m_P.m_Store.Find(o.m_Name);
					if (!ce || ce.m_IsTemplate)
						missing++;
				}
			}
			e.m_TplMissing = missing;
		}
	}

	// Opens the save dialog for objects (from the selection, once there is one; from a test plugin until then).
	void AskSave(array<ref DZS_TemplateObject> objects, string suggestion)
	{
		int outside = 0;
		foreach (DZS_TemplateObject o : objects)
		{
			DZS_CatEntry ce = m_P.m_Store.Find(o.m_Name);
			if (!ce || ce.m_IsTemplate)
				outside++;
		}
		string note = "";
		if (outside > 0)
			note = DZS_Texts.Fmt("catalog.tpl.notIn", outside.ToString());
		string blocked = "";
		if (outside == objects.Count())
			blocked = DZS_Texts.Get("catalog.tpl.none");
		else if (objects.Count() - outside > MAX_OBJECTS)
			blocked = DZS_Texts.Get("catalog.tpl.tooMany");
		DZS_TemplateNameDialog d = new DZS_TemplateNameDialog(this, DZS_Texts.Get("catalog.tpl.save"), DZS_Texts.Get("catalog.tpl.saveBtn"), "save", suggestion, "", note, blocked);
		d.m_Objects = objects;
		d.Open();
	}

	void AskRename(DZS_CatEntry e)
	{
		DZS_TemplateNameDialog d = new DZS_TemplateNameDialog(this, DZS_Texts.Get("catalog.tpl.rename"), DZS_Texts.Get("catalog.tpl.renameBtn"), "rename", e.m_Name, e.m_TplId, "", "");
		d.Open();
	}

	void Rename(string id, string name)
	{
		array<ref DZS_TemplateObject> objs = m_Objects.Get(id);
		if (!objs)
			return;
		DZS_Json t408 = ToJson(name, objs);
		if (!WriteFile(id, t408))
			return;
		m_P.m_Store.RenameTemplate(id, name);
		m_P.TemplatesChanged();
		DZS_Log.Info("catalog: template " + id + " renamed to '" + name + "'");
	}

	void AskDelete(DZS_CatEntry e)
	{
		string kicker = DZS_Texts.Get("catalog.tpl.kicker");
		string title = DZS_Texts.Get("catalog.tpl.deleteTitle");
		string ask = DZS_Texts.Fmt("catalog.tpl.deleteAsk", DZS_W.Literal(e.m_Name));
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(kicker, title, ask);
		c.m_Tag = e.m_TplId;
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		c.AddFootButton("delete", DZS_Texts.Get("catalog.tpl.deleteTitle"), "danger");
		c.m_OnResult.Insert(OnDeleteResult);
		c.Open();
	}

	void OnDeleteResult(DZS_Dialog d, string tag)
	{
		if (tag != "delete")
			return;
		string id = d.m_Tag;
		DZS_CatEntry e = m_P.m_Store.Find("tpl:" + id);
		if (e && m_P.m_Hand == e)
			m_P.DropHand();
		DeleteFile(PathOf(id, ".json"));
		DeleteFile(PathOf(id, ".tmp"));
		m_Objects.Remove(id);
		m_P.m_Store.RemoveTemplate(id);
		m_P.TemplatesChanged();
		DZS_Log.Info("catalog: template " + id + " deleted");
	}
}

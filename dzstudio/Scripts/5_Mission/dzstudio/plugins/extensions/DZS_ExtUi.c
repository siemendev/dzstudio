// Extensions in dzStudio: Project › Extensions … lists the open project's extensions (add by the URL of a spec file,
// remove), and for extensions without a plugin the fields their spec offers: the project's, each block's (the
// selection's group first), its list elements' and each change's. Fields marked "tools" are shown, not edited.

// Text of a value as a field shows it, and a field's text as a value.
class DZS_ExtValues
{
	static string Shown(DZS_ExtField f, DZS_Json v)
	{
		if (!v)
			return "";
		if (f.m_Type == "select")
		{
			string ol = f.OptionLabel(v);
			if (ol != "")
				return ol;
		}
		if (v.IsBool())
		{
			if (v.m_Bool)
				return DZS_Texts.Pick("ja", "yes");
			return DZS_Texts.Pick("nein", "no");
		}
		if (v.IsStr())
		{
			string t = v.m_Str;
			if (f.m_Type == "text")
				t.Replace("\n", "\\n");
			return t;
		}
		if (v.IsNum())
			return v.m_Str;
		if (v.IsArr() && (f.m_Type == "list" || f.m_Type == "position"))
		{
			string joined = "";
			for (int i = 0; i < v.Count(); i++)
			{
				DZS_Json item = v.At(i);
				if (i > 0)
					joined += ", ";
				if (item.IsStr() || item.IsNum())
					joined += item.m_Str;
				else
					joined += DZS_ProjectRefs.Short(item, 20);
			}
			return joined;
		}
		return DZS_ProjectRefs.Short(v, 80);
	}

	// null with error "" for an empty text (the field goes back to its default)
	static DZS_Json Parse(DZS_ExtField f, string text, out string error)
	{
		error = "";
		string t = text;
		t.TrimInPlace();
		if (t == "")
			return null;
		if (f.m_Type == "number" || f.m_Type == "integer")
			return Number(f, t, f.m_Type == "integer", error);
		if (f.m_Type == "string")
			return DZS_Json.Str(t);
		if (f.m_Type == "text")
		{
			string raw = text;
			raw.Replace("\\n", "\n");
			return DZS_Json.Str(raw);
		}
		array<string> parts = new array<string>();
		t.Split(",", parts);
		DZS_Json arr = DZS_Json.Arr();
		foreach (string part : parts)
		{
			string p = part;
			p.TrimInPlace();
			if (p == "")
				continue;
			DZS_Json item;
			if (f.m_Type == "position" || f.m_Items == "number")
				item = Number(f, p, false, error);
			else if (f.m_Items == "integer")
				item = Number(f, p, true, error);
			else
				item = DZS_Json.Str(p);
			if (error != "")
				return null;
			arr.Add(item);
		}
		if (f.m_Type == "position" && arr.Count() != 3)
		{
			error = DZS_Texts.Pick("drei Zahlen x, y, z", "three numbers x, y, z");
			return null;
		}
		return arr;
	}

	protected static DZS_Json Number(DZS_ExtField f, string t, bool whole, out string error)
	{
		error = "";
		if (!DZS_Json.IsNumberText(t))
		{
			error = DZS_Texts.Pick("keine Zahl: ", "not a number: ") + t;
			return null;
		}
		if (whole && (t.IndexOf(".") >= 0 || t.IndexOf("e") >= 0 || t.IndexOf("E") >= 0))
		{
			error = DZS_Texts.Pick("keine ganze Zahl: ", "not a whole number: ") + t;
			return null;
		}
		// bounds hold for numbers and integers (and list items), not for positions
		if (f.m_Type != "position")
		{
			float n = t.ToFloat();
			if (f.m_HasMin && n < f.m_Min)
			{
				error = DZS_Texts.Pick("kleiner als ", "less than ") + f.m_MinText;
				return null;
			}
			if (f.m_HasMax && n > f.m_Max)
			{
				error = DZS_Texts.Pick("größer als ", "greater than ") + f.m_MaxText;
				return null;
			}
		}
		return DZS_Json.Num(t);
	}

	static string TypeHint(DZS_ExtField f)
	{
		if (f.m_Type == "number" || f.m_Type == "integer")
		{
			string r = DZS_Texts.Pick("Zahl", "Number");
			if (f.m_Type == "integer")
				r = DZS_Texts.Pick("Ganze Zahl", "Whole number");
			if (f.m_HasMin || f.m_HasMax)
			{
				string lo = "…";
				string hi = "…";
				if (f.m_HasMin)
					lo = f.m_MinText;
				if (f.m_HasMax)
					hi = f.m_MaxText;
				r += " " + lo + " – " + hi;
			}
			return r;
		}
		if (f.m_Type == "position")
			return "x, y, z";
		if (f.m_Type == "list")
			return DZS_Texts.Pick("Werte, mit Komma getrennt", "Values, separated by commas");
		if (f.m_Type == "text")
			return DZS_Texts.Pick("Text, \\n für einen Zeilenumbruch", "Text, \\n for a line break");
		return DZS_Texts.Pick("Text", "Text");
	}
}

// Project › Extensions …: the open project's extensions.
class DZS_ExtListDialog : DZS_LinesDialog
{
	protected DZS_Instance m_Inst;
	protected ref array<string> m_Urls = new array<string>();
	protected int m_SeenGen;
	protected string m_AskUrl;

	void DZS_ExtListDialog(DZS_Instance inst)
	{
		m_Inst = inst;
		m_SeenGen = DZS_ExtSpecs.Gen();
		Setup(LARGE, DZS_Texts.Pick("Projekt", "Project"), DZS_Texts.Pick("Erweiterungen", "Extensions"), false);
		AddFootButtonLeft("add", DZS_Texts.Pick("Erweiterung hinzufügen …", "Add extension …"), "side");
		AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "main");
	}

	void KeepScroll(float s)
	{
		m_Scroll = s;
	}

	override void BuildContent(Widget inner)
	{
		m_Parent = inner;
		Note(DZS_Texts.Pick("Daten anderer Programme und Plugins im Projekt. Felder, die die Spezifikation einer Erweiterung anbietet, bearbeitest du hier.", "Data of other programs and plugins in the project. Fields an extension's spec offers are edited here."));
		// the extensions the project's entities hold come from the index, built over several frames
		if (!IndexOrShell(m_Inst))
			return;
		array<string> urls = DZS_ProjectExtensions.Urls();
		m_Urls.Copy(urls);
		if (m_Urls.Count() == 0)
			Note(DZS_Texts.Pick("Keine Erweiterungen.", "No extensions."));
		for (int i = 0; i < m_Urls.Count(); i++)
		{
			string url = m_Urls[i];
			DZS_ExtSpec spec = DZS_ExtSpecs.Get(url);
			string title = url;
			if (spec)
				title = spec.Title();
			DZS_RLine l = Line(10, 30);
			Label(l, title, 0, "text", 14);
			DZS_ExtGeneric g = DZS_ProjectExtensions.Generic(url);
			if (g && spec && spec.m_Fields.Count() > 0)
				Knob(l, "side", DZS_Texts.Pick("Felder", "Fields"), "edit:" + i);
			Knob(l, "danger", DZS_Texts.Pick("Entfernen", "Remove"), "del:" + i);
			DZS_RLine ul = Line(0, 20);
			Label(ul, url, 0, "textQuiet", 12);
			string state = "";
			if (DZS_ProjectExtensions.Find(url))
				state = DZS_Texts.Pick("Plugin", "Plugin");
			else if (spec && spec.m_Fields.Count() > 0)
				state = spec.m_Fields.Count().ToString() + DZS_Texts.Pick(" Felder", " fields");
			else if (spec)
				state = DZS_Texts.Pick("keine Felder — nur ein Plugin bearbeitet sie", "no fields — only a plugin edits it");
			if (DZS_ProjectExtensions.IsRequired(url))
				state = Join(state, DZS_Texts.Pick("erforderlich", "required"));
			state = Join(state, DZS_ExtSpecs.StatusText(url));
			if (state != "")
				Note(state);
		}
		AddFootFocus();
	}

	protected static string Join(string a, string b)
	{
		if (a == "")
			return b;
		if (b == "")
			return a;
		return a + " · " + b;
	}

	override void Again()
	{
		Reopen();
	}

	// a spec arrived or failed: shown anew (when no other dialog is on top)
	override void UpdateContent(float dt)
	{
		super.UpdateContent(dt);
		if (m_Closed || DZS_ExtSpecs.Gen() == m_SeenGen)
			return;
		if (DZS_Kernel.Get().GetScreen().TopDialog() != this)
			return;
		Reopen();
	}

	void Reopen()
	{
		if (m_Closed)
			return;
		DZS_ExtListDialog again = new DZS_ExtListDialog(m_Inst);
		again.KeepScroll(m_Scroll);
		Close();
		again.Open();
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		int colon = tag.IndexOf(":");
		if (colon < 0)
			return;
		string what = tag.Substring(0, colon);
		int i = tag.Substring(colon + 1, tag.Length() - colon - 1).ToInt();
		if (i < 0 || i >= m_Urls.Count())
			return;
		string url = m_Urls[i];
		if (what == "edit")
		{
			DZS_ExtGeneric g = DZS_ProjectExtensions.Generic(url);
			if (!g)
				return;
			DZS_ExtFieldsDialog d = new DZS_ExtFieldsDialog(g, m_Inst, "");
			d.Open();
			return;
		}
		if (what != "del")
			return;
		m_AskUrl = url;
		string title = url;
		DZS_ExtSpec spec = DZS_ExtSpecs.Get(url);
		if (spec)
			title = spec.Title();
		string text = DZS_Texts.Pick("„", "'") + title;
		text += DZS_Texts.Pick("“ mit allen Werten im Projekt entfernen (auch an Blöcken, Änderungen und Objekten)? Rückgängig (Strg+Z) holt alles zurück.", "' with all its values in the project (also on blocks, changes and objects)? Undo (Ctrl+Z) brings all of it back.");
		DZS_ConfirmDialog c = new DZS_ConfirmDialog(DZS_Texts.Pick("Erweiterungen", "Extensions"), DZS_Texts.Pick("Erweiterung entfernen", "Remove extension"), text);
		c.AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button yes = c.AddFootButton("remove", DZS_Texts.Pick("Entfernen", "Remove"), "danger");
		yes.m_Hit.m_Primary = true;
		c.m_OnResult.Insert(OnRemoveAnswer);
		c.Open();
	}

	void OnRemoveAnswer(DZS_Dialog d, string result)
	{
		string url = m_AskUrl;
		if (result != "remove" || url == "")
		{
			m_AskUrl = "";
			return;
		}
		// removing goes over every entity's values: once the index of the state as it is now is there
		if (!IndexNow())
		{
			Defer("call", "remove");
			return;
		}
		m_AskUrl = "";
		DZS_ProjectExtensions.RemoveUrl(m_Inst, url);
		DZS_Log.Info("extensions: removed " + url);
		Reopen();
	}

	override void Resume(string tag)
	{
		if (tag == "remove")
			OnRemoveAnswer(null, "remove");
	}

	override void OnFoot(string tag)
	{
		if (tag == "add")
		{
			DZS_ExtAddDialog a = new DZS_ExtAddDialog(m_Inst, this);
			a.Open();
			return;
		}
		Close();
	}

	override void Cancel()
	{
		Close();
	}
}

// Add extension: paste the URL of its spec file, load it, see what it is, add it.
class DZS_ExtAddDialog : DZS_LinesDialog
{
	protected DZS_Instance m_Inst;
	protected DZS_ExtListDialog m_List;
	protected DZS_TextField m_UrlField;
	protected string m_Url;
	protected string m_Problem;
	protected int m_SeenGen;

	void DZS_ExtAddDialog(DZS_Instance inst, DZS_ExtListDialog list, string url = "", string problem = "")
	{
		m_Inst = inst;
		m_List = list;
		m_Url = url;
		m_Problem = problem;
		m_SeenGen = DZS_ExtSpecs.Gen();
		Setup(MEDIUM, DZS_Texts.Pick("Erweiterungen", "Extensions"), DZS_Texts.Pick("Erweiterung hinzufügen", "Add extension"), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button go = AddFootButton("add", DZS_Texts.Pick("Hinzufügen", "Add"), "main");
		go.m_Hit.m_Primary = true;
		string why = "";
		if (!Ready(why))
			go.SetEnabled(false, why);
	}

	// the loaded spec of the URL, ready to add; else why not
	protected bool Ready(out string why)
	{
		why = "";
		if (m_Url == "")
		{
			why = DZS_Texts.Pick("Erst die Spezifikation laden", "Load the spec first");
			return false;
		}
		if (DZS_ProjectExtensions.Urls(false).Find(m_Url) >= 0)
		{
			why = DZS_Texts.Pick("Das Projekt hat diese Erweiterung schon", "The project has this extension already");
			return false;
		}
		DZS_ExtSpecState st = DZS_ExtSpecs.State(m_Url);
		if (st.m_Loading || !st.m_Spec)
		{
			why = DZS_Texts.Pick("Spezifikation nicht geladen", "Spec not loaded");
			return false;
		}
		return true;
	}

	override void BuildContent(Widget inner)
	{
		m_Parent = inner;
		Note(DZS_Texts.Pick("Die Adresse (https) der Spezifikationsdatei der Erweiterung, z. B. aus ihrer Dokumentation.", "The address (https) of the extension's spec file, e.g. from its documentation."));
		DZS_RLine l = Line(6, 30);
		m_UrlField = Field(l, m_Url, "https://…/1.json", 0);
		Knob(l, "side", DZS_Texts.Pick("Laden", "Load"), "load");
		if (m_Problem != "")
			Note(m_Problem, "danger");
		if (m_Url != "")
		{
			DZS_ExtSpecState st = DZS_ExtSpecs.State(m_Url);
			if (st.m_Loading)
				Note(DZS_Texts.Pick("Wird geladen …", "Loading …"));
			else if (!st.m_Spec)
				Note(DZS_ExtSpecs.StatusText(m_Url), "danger");
			else
				Preview(st);
		}
		AddFootFocus();
	}

	protected void Preview(DZS_ExtSpecState st)
	{
		DZS_ExtSpec s = st.m_Spec;
		Heading(s.Title());
		string desc = s.Description();
		if (desc != "")
			Lines(desc);
		if (s.m_Owner != "")
			Note(DZS_Texts.Pick("Von: ", "By: ") + s.m_Owner);
		if (st.m_FromCache && st.m_Error != "")
			Note(DZS_ExtSpecs.StatusText(m_Url), "warning");
		int edit = s.CountEditable();
		int tools = s.m_Fields.Count() - edit;
		string fields = DZS_Texts.Pick("Keine Felder — nur ein Plugin oder ein anderes Programm bearbeitet sie.", "No fields — only a plugin or another program edits it.");
		if (s.m_Fields.Count() > 0)
		{
			fields = edit.ToString() + DZS_Texts.Pick(" Felder in dzStudio bearbeitbar, ", " fields editable in dzStudio, ");
			fields += tools.ToString() + DZS_Texts.Pick(" nur von Werkzeugen", " only by tools");
		}
		Note(fields, "text");
		if (s.m_Skipped.Count() > 0)
			Note(s.m_Skipped.Count().ToString() + DZS_Texts.Pick(" Felder der Spezifikation sind ungültig und fehlen", " fields of the spec are not valid and left out"), "warning");
		if (s.m_Required)
			Note(DZS_Texts.Pick("Erforderlich: ohne ein Plugin dafür lässt sich das Projekt nicht mehr auf einen Missionsordner anwenden.", "Required: without a plugin for it the project can no longer be applied to a mission folder."), "warning");
		if (DZS_ProjectExtensions.Find(m_Url))
			Note(DZS_Texts.Pick("Ein Plugin bearbeitet diese Erweiterung.", "A plugin edits this extension."));
	}

	// long text in lines of about 90 characters
	protected void Lines(string text)
	{
		array<string> words = new array<string>();
		text.Split(" ", words);
		string line = "";
		foreach (string w : words)
		{
			if (line != "" && line.Length() + w.Length() > 90)
			{
				Note(line, "text");
				line = "";
			}
			if (line != "")
				line += " ";
			line += w;
		}
		if (line != "")
			Note(line, "text");
	}

	override void UpdateContent(float dt)
	{
		if (m_Closed || m_Url == "" || DZS_ExtSpecs.Gen() == m_SeenGen)
			return;
		if (DZS_Kernel.Get().GetScreen().TopDialog() != this)
			return;
		Again(m_Url, "");
	}

	protected void Again(string url, string problem)
	{
		DZS_ExtAddDialog d = new DZS_ExtAddDialog(m_Inst, m_List, url, problem);
		Close();
		d.Open();
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		if (tag == "load")
			Load();
	}

	protected void Load()
	{
		string url = m_UrlField.GetText();
		url.TrimInPlace();
		if (!DZS_ExtSpecs.IsHttps(url))
		{
			Again(url, DZS_Texts.Pick("Nur Adressen mit https:// und einem Pfad.", "Only addresses with https:// and a path."));
			return;
		}
		DZS_ExtSpecs.Fetch(url);
		Again(url, "");
	}

	override void OnFoot(string tag)
	{
		if (tag != "add")
		{
			Close();
			return;
		}
		string typed = m_UrlField.GetText();
		typed.TrimInPlace();
		if (typed != m_Url)
		{
			Load();
			return;
		}
		string why;
		if (!Ready(why))
			return;
		DZS_ExtSpec s = DZS_ExtSpecs.Get(m_Url);
		DZS_ProjectExtensions.AddUrl(m_Inst, m_Url, s.m_Required);
		DZS_Log.Info("extensions: added " + m_Url + ", required " + DZS_Str.Bool(s.m_Required));
		Close();
		if (m_List)
			m_List.Reopen();
	}

	override void Cancel()
	{
		Close();
	}
}

// The targets of one extension's fields in the open project: the project, blocks (a group's first), changes.
class DZS_ExtFieldsDialog : DZS_LinesDialog
{
	static const int MAX_ROWS = 250;

	protected DZS_ExtGeneric m_G;
	protected DZS_Instance m_Inst;
	protected string m_GroupId;
	protected string m_FocusBlock;
	protected ref DZS_Json m_Project;
	protected ref array<ref DZS_ExtTarget> m_Targets = new array<ref DZS_ExtTarget>();

	void DZS_ExtFieldsDialog(DZS_ExtGeneric g, DZS_Instance inst, string groupId)
	{
		m_G = g;
		m_Inst = inst;
		m_GroupId = groupId;
		string gid = groupId;
		if (gid == "")
			gid = DZS_RulesDialog.SelectedGroup(inst);
		if (gid != "")
		{
			map<string, string> ids = DZS_ProjectV2.GroupIds(inst);
			m_FocusBlock = ids.Get(gid);
		}
		Setup(LARGE, DZS_Texts.Pick("Erweiterung", "Extension"), DZS_W.Literal(g.Title()), false);
		AddFootButton("close", DZS_Texts.Get("core.dialog.close"), "main");
	}

	void KeepScroll(float s)
	{
		m_Scroll = s;
	}

	override void Again()
	{
		Refresh();
	}

	override void BuildContent(Widget inner)
	{
		m_Parent = inner;
		DZS_ExtSpec spec = m_G.Spec();
		if (!spec)
		{
			Note(DZS_ExtSpecs.StatusText(m_G.Url()), "warning");
			AddFootFocus();
			return;
		}
		// the project as it is now, from the index built over several frames (never the whole project in one frame)
		if (!IndexOrShell(m_Inst))
			return;
		DZS_ExtIndex idx = DZS_EntityExts.IndexReady(m_Inst);
		if (idx)
			m_Project = idx.m_Root;
		if (!m_Project)
		{
			Note(DZS_Texts.Pick("Kein offenes Projekt.", "No open project."), "warning");
			AddFootFocus();
			return;
		}
		if (spec.m_Skipped.Count() > 0)
			Note(spec.m_Skipped.Count().ToString() + DZS_Texts.Pick(" Felder der Spezifikation sind ungültig und fehlen", " fields of the spec are not valid and left out"), "warning");
		int rows = 0;
		int more = 0;
		if (spec.HasAttach("project"))
		{
			Heading(DZS_Texts.Pick("Projekt", "Project"));
			DZS_ExtTarget pt = DZS_ExtTarget.Project();
			Row(pt, false);
			rows++;
		}
		DZS_Json blocks = m_Project.Get("blocks");
		DZS_Json changes = m_Project.Get("changes");
		bool head = false;
		for (int pass = 0; pass < 2; pass++)
		{
			for (int i = 0; i < blocks.Count(); i++)
			{
				DZS_Json b = blocks.At(i);
				bool focus = b.GetStr("id") == m_FocusBlock && m_FocusBlock != "";
				if ((pass == 0) != focus || !spec.TouchesBlock(b.GetStr("kind")))
					continue;
				if (rows >= MAX_ROWS)
				{
					more++;
					continue;
				}
				if (!head)
				{
					Heading(DZS_Texts.Pick("Blöcke", "Blocks"));
					head = true;
				}
				DZS_ExtTarget bt = DZS_ExtTarget.OfBlock(b);
				bt.m_GroupId = DZS_EntityExts.GroupOf(m_Inst, bt.m_BlockId);
				Row(bt, focus);
				rows++;
			}
		}
		bool chead = false;
		for (int k = 0; k < changes.Count(); k++)
		{
			DZS_Json c = changes.At(k);
			array<DZS_ExtField> cf = spec.FieldsFor("change", c.GetStr("block"));
			if (cf.Count() == 0)
				continue;
			if (rows >= MAX_ROWS)
			{
				more++;
				continue;
			}
			if (!chead)
			{
				Heading(DZS_Texts.Pick("Änderungen", "Changes"));
				chead = true;
			}
			DZS_ExtTarget ct = DZS_ExtTarget.OfChange(c);
			Row(ct, false);
			rows++;
		}
		if (more > 0)
			Note(more.ToString() + DZS_Texts.Pick(" weitere — mit einem anderen Werkzeug bearbeiten", " more — edit them with another tool"));
		if (m_Targets.Count() == 0)
			Note(DZS_Texts.Pick("Die Erweiterung bietet keine Felder für das, was das Projekt enthält.", "The extension offers no fields for what the project holds."));
		AddFootFocus();
	}

	protected void Row(DZS_ExtTarget t, bool focus)
	{
		DZS_RLine l = Line(2, 30);
		string tone = "text";
		if (focus)
			tone = "accentText";
		Label(l, t.m_Label, 0, tone, 14);
		Label(l, Summary(t), -1, "textQuiet", 13);
		Knob(l, "side", DZS_Texts.Pick("Bearbeiten", "Edit"), "edit:" + m_Targets.Count());
		m_Targets.Insert(t);
	}

	protected string Summary(DZS_ExtTarget t)
	{
		DZS_ExtSpec spec = m_G.Spec();
		array<DZS_ExtField> fields = spec.FieldsFor(t.m_Attach, t.m_Kind);
		int n = m_G.CountSet(m_Inst, t, fields);
		string s = DZS_Texts.Pick("Standardwerte", "defaults");
		if (n > 0)
			s = n.ToString() + DZS_Texts.Pick(DZS_Issues.Plural(n, " Wert", " Werte"), DZS_Issues.Plural(n, " value", " values"));
		if (t.m_Attach != "block" || !t.m_Json)
			return s;
		int els = 0;
		DZS_Json data = t.m_Json.Get("data");
		array<string> lists = spec.ElementLists(t.m_Kind);
		foreach (string list : lists)
		{
			DZS_Json items = null;
			if (data && data.IsObj())
				items = data.Get(list);
			if (!items || !items.IsArr())
				continue;
			for (int i = 0; i < items.Count(); i++)
			{
				if (DZS_EntityExts.Element(m_Inst, t.m_BlockId, list, i, m_G.Url()))
					els++;
			}
		}
		if (els > 0)
			s += " · " + els.ToString() + DZS_Texts.Pick(DZS_Issues.Plural(els, " Element mit Werten", " Elemente mit Werten"), DZS_Issues.Plural(els, " element with values", " elements with values"));
		return s;
	}

	void Refresh()
	{
		if (m_Closed)
			return;
		DZS_ExtFieldsDialog again = new DZS_ExtFieldsDialog(m_G, m_Inst, m_GroupId);
		again.KeepScroll(m_Scroll);
		Close();
		again.Open();
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		int colon = tag.IndexOf(":");
		if (colon < 0)
			return;
		string what = tag.Substring(0, colon);
		int i = tag.Substring(colon + 1, tag.Length() - colon - 1).ToInt();
		if (what != "edit" || i < 0 || i >= m_Targets.Count())
			return;
		if (!DZS_ExtGeneric.Alive(m_Inst, m_Targets[i]))
		{
			GoneNotice();
			Refresh();
			return;
		}
		DZS_ExtTargetDialog d = new DZS_ExtTargetDialog(m_G, m_Inst, this, null, m_Targets[i], null, null);
		d.Open();
	}

	override void OnFoot(string tag)
	{
		Close();
	}

	override void Cancel()
	{
		Close();
	}
}

// The fields of one target; a block also lists its elements that have fields. Edits a copy of the target's extension
// object ("work") that "Apply" stores; members no field names stay in it. Bound to the target's entity (its owner,
// taken once): closes when it is deleted, and checks again before storing.
class DZS_ExtTargetDialog : DZS_LinesDialog
{
	static const int MAX_ELEMENTS = 200;

	protected DZS_ExtGeneric m_G;
	protected DZS_Instance m_Inst;
	protected DZS_ExtFieldsDialog m_Over;
	// the block's dialog of an element's dialog
	protected DZS_ExtTargetDialog m_Outer;
	protected ref DZS_ExtTarget m_T;
	protected ref DZS_Json m_Work;
	// texts that did not parse, by field id, kept over a rebuild
	protected ref map<string, string> m_Bad;
	protected ref array<ref DZS_ExtField> m_Shown = new array<ref DZS_ExtField>();
	protected ref array<DZS_TextField> m_Inputs = new array<DZS_TextField>();
	protected ref array<ref DZS_ExtTarget> m_Elements = new array<ref DZS_ExtTarget>();
	protected ref array<string> m_Errors = new array<string>();
	protected int m_PickIndex = -1;

	void DZS_ExtTargetDialog(DZS_ExtGeneric g, DZS_Instance inst, DZS_ExtFieldsDialog over, DZS_ExtTargetDialog outer, DZS_ExtTarget t, DZS_Json work, map<string, string> bad)
	{
		m_G = g;
		m_Inst = inst;
		m_Over = over;
		m_Outer = outer;
		m_T = t;
		if (work)
			m_Work = work;
		else
		{
			DZS_Json h = g.Holder(inst, t);
			if (h && h.IsObj())
				m_Work = h.Clone();
			else
				m_Work = DZS_Json.Obj();
		}
		if (bad)
			m_Bad = bad;
		else
			m_Bad = new map<string, string>();
		Setup(LARGE, g.Title(), DZS_W.Literal(t.m_Label), true);
		AddFootButton("cancel", DZS_Texts.Get("core.dialog.cancel"), "side");
		DZS_Button go = AddFootButton("apply", DZS_Texts.Pick("Übernehmen", "Apply"), "main");
		go.m_Hit.m_Primary = true;
		Watch(inst);
	}

	override bool TargetAlive()
	{
		return DZS_ExtGeneric.Alive(m_Inst, m_T);
	}

	void KeepScroll(float s)
	{
		m_Scroll = s;
	}

	void SetErrors(array<string> errors)
	{
		m_Errors.Copy(errors);
	}

	override void BuildContent(Widget inner)
	{
		m_Parent = inner;
		DZS_ExtSpec spec = m_G.Spec();
		if (!spec)
		{
			AddFootFocus();
			return;
		}
		// an entity's target reads the index (its elements, whether it is still there), built over several frames
		if (m_T.m_Attach != "project" && !IndexOrShell(m_Inst))
			return;
		foreach (string e : m_Errors)
			Note(e, "danger");
		array<DZS_ExtField> fields = spec.FieldsFor(m_T.m_Attach, m_T.m_Kind, m_T.m_List);
		if (fields.Count() == 0 && m_T.m_Attach != "block")
			Note(DZS_Texts.Pick("Keine Felder.", "No fields."));
		foreach (DZS_ExtField f : fields)
			FieldRow(f);
		if (m_T.m_Attach == "block")
			Elements(spec);
		AddFootFocus();
	}

	protected void FieldRow(DZS_ExtField f)
	{
		int i = m_Shown.Count();
		m_Shown.Insert(f);
		m_Inputs.Insert(null);
		DZS_Json v = DZS_ExtGeneric.At(m_Work, f.m_Path);
		DZS_RLine l = Line(8, 30);
		Label(l, f.Label(), 220, "text", 13);
		if (!f.Editable() || !DZS_ExtGeneric.Writable(m_Work, f.m_Path))
		{
			string shown = DZS_ExtValues.Shown(f, v);
			string tone = "text";
			if (!v)
			{
				shown = DZS_ExtValues.Shown(f, f.m_Default);
				tone = "textQuiet";
				if (shown == "")
					shown = "—";
			}
			Label(l, shown, 0, tone, 13);
			string why = DZS_Texts.Pick("von Werkzeugen gesetzt", "set by tools");
			if (f.Editable())
				why = DZS_Texts.Pick("anderer Wert im Weg", "another value in the way");
			Label(l, why, -1, "textQuiet", 12);
		}
		else if (f.m_Type == "boolean")
		{
			bool on = false;
			if (v && v.IsBool())
				on = v.m_Bool;
			else if (!v && f.m_Default && f.m_Default.IsBool())
				on = f.m_Default.m_Bool;
			Switch(l, on, "bool:" + i);
			string state = "";
			if (!v)
				state = DZS_Texts.Pick("Standard", "default");
			Label(l, state, 0, "textQuiet", 12);
		}
		else if (f.m_Type == "select")
		{
			string chosen = DZS_ExtValues.Shown(f, v);
			if (!v)
			{
				chosen = DZS_ExtValues.Shown(f, f.m_Default);
				if (chosen == "")
					chosen = "—";
				chosen += DZS_Texts.Pick(" (Standard)", " (default)");
			}
			Knob(l, "side", chosen, "sel:" + i, "chevron-down");
		}
		else
		{
			string text = DZS_ExtValues.Shown(f, v);
			string bad;
			if (m_Bad.Find(f.m_Id, bad))
				text = bad;
			string hint = DZS_ExtValues.TypeHint(f);
			if (f.m_Default)
				hint = DZS_Texts.Pick("Standard: ", "Default: ") + DZS_ExtValues.Shown(f, f.m_Default);
			DZS_TextField tf = Field(l, text, hint, 0);
			m_Inputs[i] = tf;
			if (f.m_Type == "position" && SelectedPosition() != "")
				Knob(l, "side", DZS_Texts.Pick("Auswahl", "Selection"), "pos:" + i);
		}
		if (f.Editable() && v)
			Knob(l, "ghost", "", "clear:" + i, "x");
		string help = f.Help();
		if (help != "")
			Note(help);
	}

	// "x, y, z" of the lead selected object, "" when nothing is selected
	protected string SelectedPosition()
	{
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		if (!sel || !m_Inst)
			return "";
		string lead = sel.GetLead();
		if (lead == "")
		{
			array<string> picked = sel.GetSelection();
			if (picked.Count() > 0)
				lead = picked[0];
		}
		if (lead == "" || m_Inst.GetKind(lead) != DZS_KIND_OBJECT)
			return "";
		string pos;
		if (!m_Inst.GetField(lead, DZS_Obj.POS, pos))
			return "";
		vector p = DZS_Obj.QuickVec(pos);
		string r = DZS_PatchNum.Canon(p[0].ToString());
		r += ", " + DZS_PatchNum.Canon(p[1].ToString());
		r += ", " + DZS_PatchNum.Canon(p[2].ToString());
		return r;
	}

	// The block's list elements that have fields, as they are now and by identity (each target keeps its element);
	// a spawner group's selected objects first.
	protected void Elements(DZS_ExtSpec spec)
	{
		array<string> lists = spec.ElementLists(m_T.m_Kind);
		DZS_SelectionApi sel = DZS_SelectionApi.Get();
		DZS_ExtOwner block = DZS_ExtGeneric.OwnerOf(m_Inst, m_T);
		array<string> keys = DZS_EntityExts.ItemKeys(m_Inst, block);
		foreach (string list : lists)
		{
			Heading(list);
			array<string> inList = new array<string>();
			for (int pass = 0; pass < 2; pass++)
			{
				foreach (string key : keys)
				{
					if (key.IndexOf(list + "@") != 0)
						continue;
					string elem = key.Substring(list.Length() + 1, key.Length() - list.Length() - 1);
					bool picked = sel && sel.IsSelected(elem);
					if ((pass == 0) == picked)
						inList.Insert(key);
				}
			}
			if (inList.Count() == 0)
			{
				Note(DZS_Texts.Pick("Keine Elemente.", "No elements."));
				continue;
			}
			int shown = 0;
			foreach (string ik : inList)
			{
				if (shown >= MAX_ELEMENTS)
					break;
				string path = DZS_EntityExts.ItemPath(m_Inst, block, ik);
				int open = path.IndexOf("[");
				int n = path.Substring(open + 1, path.Length() - open - 2).ToInt();
				DZS_Json el = DZS_EntityExts.ItemJson(m_Inst, block, ik);
				DZS_ExtTarget et = DZS_ExtTarget.OfElement(m_T, list, n, el);
				et.m_Owner = DZS_EntityExts.ItemOwner(m_Inst, block, ik);
				int ikAt = ik.IndexOf("@");
				et.m_Selected = sel && sel.IsSelected(ik.Substring(ikAt + 1, ik.Length() - ikAt - 1));
				array<DZS_ExtField> ef = spec.FieldsFor("element", m_T.m_Kind, list);
				int filled = m_G.CountSet(m_Inst, et, ef);
				DZS_RLine l = Line(2, 28);
				string tone = "text";
				string label = et.m_Label;
				if (et.m_Selected)
				{
					tone = "accentText";
					label += DZS_Texts.Pick(" · ausgewählt", " · selected");
				}
				Label(l, label, 0, tone, 13);
				string sum = DZS_Texts.Pick("Standardwerte", "defaults");
				if (filled > 0)
					sum = filled.ToString() + DZS_Texts.Pick(DZS_Issues.Plural(filled, " Wert", " Werte"), DZS_Issues.Plural(filled, " value", " values"));
				Label(l, sum, -1, "textQuiet", 12);
				Knob(l, "side", DZS_Texts.Pick("Bearbeiten", "Edit"), "el:" + m_Elements.Count());
				m_Elements.Insert(et);
				shown++;
			}
			if (inList.Count() > shown)
				Note((inList.Count() - shown).ToString() + DZS_Texts.Pick(" weitere — Objekte auswählen, um sie zuerst zu sehen", " more — select objects to see them first"));
		}
	}

	// The texts into the work copy; problems as lines ("" each when all parse).
	protected array<string> Collect()
	{
		array<string> errors = new array<string>();
		m_Bad.Clear();
		for (int i = 0; i < m_Shown.Count(); i++)
		{
			DZS_TextField tf = m_Inputs[i];
			if (!tf)
				continue;
			DZS_ExtField f = m_Shown[i];
			string text = tf.GetText();
			string err;
			DZS_Json v = DZS_ExtValues.Parse(f, text, err);
			if (err != "")
			{
				errors.Insert(f.Label() + ": " + err);
				m_Bad.Set(f.m_Id, text);
				continue;
			}
			DZS_ExtGeneric.SetAt(m_Work, f.m_Path, v);
		}
		return errors;
	}

	override void Again()
	{
		array<string> errors = new array<string>();
		errors.Copy(m_Errors);
		Rebuild(errors);
	}

	void Rebuild(array<string> errors)
	{
		if (m_Closed)
			return;
		DZS_ExtTargetDialog d = new DZS_ExtTargetDialog(m_G, m_Inst, m_Over, m_Outer, m_T, m_Work, m_Bad);
		if (errors)
			d.SetErrors(errors);
		d.KeepScroll(m_Scroll);
		Close();
		d.Open();
	}

	override void Knobbed(DZS_Button b, string tag)
	{
		int colon = tag.IndexOf(":");
		if (colon < 0)
			return;
		string what = tag.Substring(0, colon);
		int i = tag.Substring(colon + 1, tag.Length() - colon - 1).ToInt();
		if (what == "el")
		{
			if (i < 0 || i >= m_Elements.Count())
				return;
			// what was typed so far stays when this dialog is rebuilt after the element's
			Collect();
			if (!DZS_ExtGeneric.Alive(m_Inst, m_Elements[i]))
			{
				GoneNotice();
				Rebuild(null);
				return;
			}
			DZS_ExtTargetDialog d = new DZS_ExtTargetDialog(m_G, m_Inst, null, this, m_Elements[i], null, null);
			d.Open();
			return;
		}
		if (i < 0 || i >= m_Shown.Count())
			return;
		DZS_ExtField f = m_Shown[i];
		if (what == "sel")
		{
			m_PickIndex = i;
			array<ref DZS_MenuItem> items = new array<ref DZS_MenuItem>();
			for (int o = 0; o < f.m_OptionValues.Count(); o++)
				items.Insert(Item(o.ToString(), DZS_ExtSpec.Text(f.m_OptionDe[o], f.m_OptionEn[o])));
			Pick(b, items, "sel");
			return;
		}
		array<string> errors = Collect();
		if (what == "clear")
		{
			DZS_ExtGeneric.SetAt(m_Work, f.m_Path, null);
			m_Bad.Remove(f.m_Id);
		}
		else if (what == "pos")
		{
			string pos = SelectedPosition();
			string err;
			DZS_Json pv = DZS_ExtValues.Parse(f, pos, err);
			if (pv)
				DZS_ExtGeneric.SetAt(m_Work, f.m_Path, pv);
			m_Bad.Remove(f.m_Id);
		}
		Rebuild(errors);
	}

	override void Picked(string what, string tag)
	{
		int i = m_PickIndex;
		if (what != "sel" || i < 0 || i >= m_Shown.Count())
			return;
		DZS_ExtField f = m_Shown[i];
		int o = tag.ToInt();
		if (o < 0 || o >= f.m_OptionValues.Count())
			return;
		array<string> errors = Collect();
		DZS_Json chosen = f.m_OptionValues[o].Clone();
		DZS_ExtGeneric.SetAt(m_Work, f.m_Path, chosen);
		Rebuild(errors);
	}

	override void Switched(string tag, bool on)
	{
		if (tag.IndexOf("bool:") != 0)
			return;
		int i = tag.Substring(5, tag.Length() - 5).ToInt();
		if (i < 0 || i >= m_Shown.Count())
			return;
		DZS_ExtField f = m_Shown[i];
		DZS_Json bv = DZS_Json.Boolean(on);
		DZS_ExtGeneric.SetAt(m_Work, f.m_Path, bv);
	}

	override void OnFoot(string tag)
	{
		if (tag != "apply")
		{
			Close();
			return;
		}
		array<string> errors = Collect();
		if (errors.Count() > 0)
		{
			Rebuild(errors);
			return;
		}
		if (!m_G.Store(m_Inst, m_T, m_Work))
		{
			Gone();
			return;
		}
		Close();
		if (m_Outer)
			m_Outer.Rebuild(null);
		else if (m_Over)
			m_Over.Refresh();
	}

	override void Cancel()
	{
		Close();
	}
}

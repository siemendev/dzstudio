// Spec files of project extensions (docs/extensions/README.md): fetched from their URL (https only) with the game's
// RestApi, kept in $profile:dzstudio\specs\<sha256 of the URL>.json and read from there when offline. Their "fields"
// let dzStudio show and edit an extension it has no plugin for (DZS_ExtGeneric).

// One input field of a spec.
class DZS_ExtField : Managed
{
	string m_Id;
	// the entity whose "extensions" holds the value: "project", "block", "change" or "element"
	string m_Attach;
	// for "block:<kind>" / "change:<kind>" / "element:<kind>.<list>"; "" = any kind
	string m_Kind;
	// for "element": the list in the block's data ("objects", "children" …)
	string m_List;
	// "number", "integer", "boolean", "string", "text", "select", "position", "list"
	string m_Type;
	// for "list": "string", "number" or "integer"
	string m_Items;
	// keys inside the entity's extension object (the project: inside its "data")
	ref array<string> m_Path = new array<string>();
	string m_LabelDe;
	string m_LabelEn;
	string m_HelpDe;
	string m_HelpEn;
	bool m_HasMin;
	bool m_HasMax;
	float m_Min;
	float m_Max;
	string m_MinText;
	string m_MaxText;
	string m_Step;
	ref array<ref DZS_Json> m_OptionValues = new array<ref DZS_Json>();
	ref array<string> m_OptionDe = new array<string>();
	ref array<string> m_OptionEn = new array<string>();
	ref DZS_Json m_Default;
	// "studio": dzStudio edits it; "tools": shown read-only, set by other programs and plugins
	string m_Edit;

	bool Editable() { return m_Edit == "studio"; }

	string Label() { return DZS_ExtSpec.Text(m_LabelDe, m_LabelEn); }

	string Help() { return DZS_ExtSpec.Text(m_HelpDe, m_HelpEn); }

	// whether it belongs to a target: its attachment, the kind (blocks, changes) and the list (elements)
	bool Fits(string attach, string kind, string list = "")
	{
		if (m_Attach != attach)
			return false;
		if (m_Kind != "" && m_Kind != kind)
			return false;
		return m_Attach != "element" || m_List == list;
	}

	// The label of an option value, "" when it is none of them.
	string OptionLabel(DZS_Json v)
	{
		if (!v)
			return "";
		for (int i = 0; i < m_OptionValues.Count(); i++)
		{
			DZS_Json o = m_OptionValues[i];
			if (DZS_ExtSpec.SameValue(o, v))
				return DZS_ExtSpec.Text(m_OptionDe[i], m_OptionEn[i]);
		}
		return "";
	}
}

class DZS_ExtSpec : Managed
{
	static const string KIND = "dzstudio-project-extension";

	string m_Url;
	string m_TitleDe;
	string m_TitleEn;
	string m_DescDe;
	string m_DescEn;
	string m_Owner;
	string m_SpecLink;
	bool m_Required;
	ref array<ref DZS_ExtField> m_Fields = new array<ref DZS_ExtField>();
	// fields left out because they are not valid: "id: why"
	ref array<string> m_Skipped = new array<string>();

	string Title()
	{
		string t = Text(m_TitleDe, m_TitleEn);
		if (t == "")
			return m_Url;
		return t;
	}

	string Description() { return Text(m_DescDe, m_DescEn); }

	int CountEditable()
	{
		int n = 0;
		foreach (DZS_ExtField f : m_Fields)
		{
			if (f.Editable())
				n++;
		}
		return n;
	}

	bool HasAttach(string attach)
	{
		foreach (DZS_ExtField f : m_Fields)
		{
			if (f.m_Attach == attach)
				return true;
		}
		return false;
	}

	// fields for a target: attach "block" / "change" with its kind, "element" with the block's kind and the list
	array<DZS_ExtField> FieldsFor(string attach, string kind, string list = "")
	{
		array<DZS_ExtField> r = new array<DZS_ExtField>();
		foreach (DZS_ExtField f : m_Fields)
		{
			if (f.Fits(attach, kind, list))
				r.Insert(f);
		}
		return r;
	}

	// the lists of a block kind that have element fields
	array<string> ElementLists(string kind)
	{
		array<string> r = new array<string>();
		foreach (DZS_ExtField f : m_Fields)
		{
			if (f.m_Attach == "element" && f.m_Kind == kind && r.Find(f.m_List) < 0)
				r.Insert(f.m_List);
		}
		return r;
	}

	// whether a block of the kind has fields of its own or of its elements
	bool TouchesBlock(string kind)
	{
		foreach (DZS_ExtField f : m_Fields)
		{
			if (f.m_Attach == "block" && (f.m_Kind == "" || f.m_Kind == kind))
				return true;
			if (f.m_Attach == "element" && f.m_Kind == kind)
				return true;
		}
		return false;
	}

	static string Text(string de, string en)
	{
		if (de == "")
			return en;
		if (en == "")
			return de;
		return DZS_Texts.Pick(de, en);
	}

	// {"en": …, "de": …} or a plain string (taken for both)
	protected static void Texts(DZS_Json v, out string de, out string en)
	{
		de = "";
		en = "";
		if (!v)
			return;
		if (v.IsStr())
		{
			de = v.m_Str;
			en = v.m_Str;
			return;
		}
		if (!v.IsObj())
			return;
		de = v.GetStr("de");
		en = v.GetStr("en");
	}

	static bool SameValue(DZS_Json a, DZS_Json b)
	{
		if (!a || !b)
			return false;
		if (a.m_Type != b.m_Type)
			return false;
		if (a.IsBool())
			return a.m_Bool == b.m_Bool;
		if (a.IsNum())
			return a.m_Str.ToFloat() == b.m_Str.ToFloat();
		if (a.IsStr())
			return a.m_Str == b.m_Str;
		array<string> ca = a.ToChunks(false);
		array<string> cb = b.ToChunks(false);
		return DZS_Files.Join(ca) == DZS_Files.Join(cb);
	}

	// A spec file read; null with 'error' when it is not one for this URL.
	static DZS_ExtSpec FromJson(string url, DZS_Json root, out string error)
	{
		error = "";
		if (!root || !root.IsObj())
		{
			error = "not a JSON object";
			return null;
		}
		if (root.GetStr("kind") != KIND)
		{
			error = "kind is not " + KIND;
			return null;
		}
		if (root.GetStr("$id") != url)
		{
			error = "$id is not the URL";
			return null;
		}
		DZS_ExtSpec s = new DZS_ExtSpec();
		s.m_Url = url;
		string tde, ten, dde, den;
		Texts(root.Get("title"), tde, ten);
		Texts(root.Get("description"), dde, den);
		s.m_TitleDe = tde;
		s.m_TitleEn = ten;
		s.m_DescDe = dde;
		s.m_DescEn = den;
		s.m_Owner = root.GetStr("owner");
		s.m_SpecLink = root.GetStr("spec");
		s.m_Required = root.GetBool("required");
		DZS_Json fields = root.Get("fields");
		if (fields && fields.IsArr())
		{
			array<string> ids = new array<string>();
			for (int i = 0; i < fields.Count(); i++)
			{
				DZS_Json fj = fields.At(i);
				string why = "";
				DZS_ExtField f = Field(fj, why);
				if (f && ids.Find(f.m_Id) >= 0)
				{
					why = "id used twice";
					f = null;
				}
				if (!f)
				{
					string fid = "#" + i;
					if (fj && fj.IsObj() && fj.GetStr("id") != "")
						fid = fj.GetStr("id");
					s.m_Skipped.Insert(fid + ": " + why);
					continue;
				}
				ids.Insert(f.m_Id);
				s.m_Fields.Insert(f);
			}
		}
		return s;
	}

	protected static bool ValidId(string id)
	{
		if (id == "")
			return false;
		string ok = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
		for (int i = 0; i < id.Length(); i++)
		{
			if (ok.IndexOf(id.Get(i)) < 0)
				return false;
		}
		return true;
	}

	protected static DZS_ExtField Field(DZS_Json fj, out string why)
	{
		why = "";
		if (!fj || !fj.IsObj())
		{
			why = "not an object";
			return null;
		}
		DZS_ExtField f = new DZS_ExtField();
		f.m_Id = fj.GetStr("id");
		if (!ValidId(f.m_Id))
		{
			why = "id missing or not [A-Za-z0-9_-]";
			return null;
		}
		if (!Attach(fj.GetStr("attach"), f))
		{
			why = "attach unknown";
			return null;
		}
		f.m_Type = fj.GetStr("type");
		array<string> types = {"number", "integer", "boolean", "string", "text", "select", "position", "list"};
		if (types.Find(f.m_Type) < 0)
		{
			why = "type unknown";
			return null;
		}
		f.m_Items = fj.GetStr("items", "string");
		if (f.m_Type == "list" && f.m_Items != "string" && f.m_Items != "number" && f.m_Items != "integer")
		{
			why = "items unknown";
			return null;
		}
		if (!Path(fj.GetStr("path"), f))
		{
			why = "path has an empty key";
			return null;
		}
		string lde, len, hde, hen;
		Texts(fj.Get("label"), lde, len);
		Texts(fj.Get("help"), hde, hen);
		if (lde == "" && len == "")
		{
			why = "label missing";
			return null;
		}
		f.m_LabelDe = lde;
		f.m_LabelEn = len;
		f.m_HelpDe = hde;
		f.m_HelpEn = hen;
		DZS_Json mn = fj.Get("min");
		if (mn && mn.IsNum())
		{
			f.m_HasMin = true;
			f.m_Min = mn.m_Str.ToFloat();
			f.m_MinText = mn.m_Str;
		}
		DZS_Json mx = fj.Get("max");
		if (mx && mx.IsNum())
		{
			f.m_HasMax = true;
			f.m_Max = mx.m_Str.ToFloat();
			f.m_MaxText = mx.m_Str;
		}
		DZS_Json st = fj.Get("step");
		if (st && st.IsNum())
			f.m_Step = st.m_Str;
		DZS_Json opts = fj.Get("options");
		if (opts && opts.IsArr())
		{
			for (int i = 0; i < opts.Count(); i++)
			{
				DZS_Json o = opts.At(i);
				if (!o || !o.IsObj() || !o.Has("value"))
					continue;
				DZS_Json ov = o.Get("value");
				if (!ov.IsStr() && !ov.IsNum() && !ov.IsBool())
					continue;
				string ode, oen;
				Texts(o.Get("label"), ode, oen);
				if (ode == "" && oen == "")
				{
					ode = DZS_ProjectRefs.Short(ov, 60);
					oen = ode;
				}
				DZS_Json ovCopy = ov.Clone();
				f.m_OptionValues.Insert(ovCopy);
				f.m_OptionDe.Insert(ode);
				f.m_OptionEn.Insert(oen);
			}
		}
		if (f.m_Type == "select" && f.m_OptionValues.Count() == 0)
		{
			why = "select without options";
			return null;
		}
		DZS_Json def = fj.Get("default");
		if (def && def.m_Type != DZS_Json.NUL)
			f.m_Default = def.Clone();
		f.m_Edit = fj.GetStr("edit", "tools");
		if (f.m_Edit != "studio" && f.m_Edit != "tools")
		{
			why = "edit is neither studio nor tools";
			return null;
		}
		return f;
	}

	// "project", "block", "block:<kind>", "change", "change:<kind>", "element:<kind>.<list>"
	protected static bool Attach(string attach, DZS_ExtField f)
	{
		int colon = attach.IndexOf(":");
		f.m_Attach = attach;
		if (colon < 0)
			return attach == "project" || attach == "block" || attach == "change";
		f.m_Attach = attach.Substring(0, colon);
		string rest = attach.Substring(colon + 1, attach.Length() - colon - 1);
		if (rest == "")
			return false;
		if (f.m_Attach == "block" || f.m_Attach == "change")
		{
			f.m_Kind = rest;
			return rest.IndexOf(".") < 0;
		}
		if (f.m_Attach != "element")
			return false;
		int dot = rest.IndexOf(".");
		if (dot <= 0 || dot >= rest.Length() - 1)
			return false;
		f.m_Kind = rest.Substring(0, dot);
		f.m_List = rest.Substring(dot + 1, rest.Length() - dot - 1);
		return f.m_List.IndexOf(".") < 0;
	}

	// "a.b" into keys inside the entity's extension object; empty: the field's id.
	protected static bool Path(string text, DZS_ExtField f)
	{
		string p = text;
		if (p == "")
			p = f.m_Id;
		array<string> segs = new array<string>();
		p.Split(".", segs);
		foreach (string seg : segs)
		{
			if (seg == "")
				return false;
			f.m_Path.Insert(seg);
		}
		return f.m_Path.Count() > 0;
	}
}

// One fetch in flight; it must live until the game answers.
class DZS_ExtSpecCall : RestCallback
{
	string m_Url;

	override void OnSuccess(string data, int dataSize)
	{
		DZS_ExtSpecs.Answer(this, data, dataSize, "");
	}

	override void OnError(int errorCode)
	{
		DZS_ExtSpecs.Answer(this, "", 0, "error " + errorCode);
	}

	override void OnTimeout()
	{
		DZS_ExtSpecs.Answer(this, "", 0, "timeout");
	}
}

// What dzStudio has of one spec URL.
class DZS_ExtSpecState : Managed
{
	string m_Url;
	ref DZS_ExtSpec m_Spec;
	bool m_Loading;
	// tried in this session (once per session unless asked again)
	bool m_Fetched;
	// the last fetch failed: why
	string m_Error;
	bool m_FromCache;
}

class DZS_ExtSpecs
{
	static const string DIR = "$profile:dzstudio\\specs";

	protected static ref map<string, ref DZS_ExtSpecState> s_States;
	protected static ref array<ref DZS_ExtSpecCall> s_Calls;
	// bumped whenever a spec or a state changes, for dialogs that wait
	protected static int s_Gen;

	static int Gen() { return s_Gen; }

	static string CachePath(string url)
	{
		return DIR + "\\" + DZS_Sha256.OfText(url) + ".json";
	}

	static bool IsHttps(string url)
	{
		string low = url;
		low.ToLower();
		if (low.IndexOf("https://") != 0 || url.IndexOf("https://") != 0)
			return false;
		int slash = url.IndexOfFrom(8, "/");
		return slash > 8 && slash < url.Length() - 1 && url.IndexOf(" ") < 0;
	}

	static DZS_ExtSpecState State(string url)
	{
		if (!s_States)
			s_States = new map<string, ref DZS_ExtSpecState>();
		DZS_ExtSpecState st;
		if (s_States.Find(url, st))
			return st;
		st = new DZS_ExtSpecState();
		st.m_Url = url;
		s_States.Set(url, st);
		LoadCache(st);
		return st;
	}

	static DZS_ExtSpec Get(string url)
	{
		return State(url).m_Spec;
	}

	// The spec from the cache now and fetched once per session.
	static void Want(string url)
	{
		DZS_ExtSpecState st = State(url);
		if (!st.m_Fetched)
			Fetch(url);
	}

	// Fetches the spec again (also when it was fetched in this session).
	static bool Fetch(string url)
	{
		DZS_ExtSpecState st = State(url);
		if (st.m_Loading)
			return true;
		st.m_Fetched = true;
		if (!IsHttps(url))
		{
			st.m_Error = "not an https URL";
			s_Gen++;
			return false;
		}
		RestApi api = GetRestApi();
		if (!api)
			api = CreateRestApi();
		if (!api)
		{
			st.m_Error = "no RestApi";
			s_Gen++;
			return false;
		}
		int slash = url.IndexOfFrom(8, "/");
		string origin = url.Substring(0, slash + 1);
		string rest = url.Substring(slash + 1, url.Length() - slash - 1);
		RestContext ctx = api.GetRestContext(origin);
		if (!ctx)
		{
			st.m_Error = "no connection";
			s_Gen++;
			return false;
		}
		if (!s_Calls)
			s_Calls = new array<ref DZS_ExtSpecCall>();
		DZS_ExtSpecCall c = new DZS_ExtSpecCall();
		c.m_Url = url;
		s_Calls.Insert(c);
		st.m_Loading = true;
		st.m_Error = "";
		s_Gen++;
		int r = ctx.GET(c, rest);
		DZS_Log.Info("extensions: spec request " + url + " -> " + r);
		return true;
	}

	static void Answer(DZS_ExtSpecCall c, string data, int size, string fail)
	{
		if (s_Calls)
			s_Calls.RemoveItem(c);
		if (!c)
			return;
		string url = c.m_Url;
		DZS_ExtSpecState st = State(url);
		st.m_Loading = false;
		s_Gen++;
		if (fail != "")
		{
			st.m_Error = fail;
			DZS_Log.Warn("extensions: spec " + url + " not fetched: " + fail);
			return;
		}
		if (data.Length() != size)
		{
			st.m_Error = "answer incomplete";
			DZS_Log.Warn("extensions: spec " + url + " incomplete, " + data.Length() + " of " + size + " bytes");
			return;
		}
		array<string> chunks = DZS_Files.Chunk(data);
		string err;
		DZS_Json root = DZS_Json.Parse(chunks, err);
		string why = "";
		DZS_ExtSpec spec = null;
		if (!root || err != "")
			why = "not JSON: " + err;
		else
			spec = DZS_ExtSpec.FromJson(url, root, why);
		if (!spec)
		{
			st.m_Error = why;
			DZS_Log.Warn("extensions: spec " + url + " refused: " + why);
			return;
		}
		st.m_Spec = spec;
		st.m_FromCache = false;
		st.m_Error = "";
		DZS_Files.MakeDirs(DIR);
		if (!DZS_Files.Write(CachePath(url), chunks))
			DZS_Log.Warn("extensions: spec " + url + " not cached");
		DZS_Log.Info("extensions: spec " + url + " loaded, " + spec.m_Fields.Count() + " field(s), " + spec.m_Skipped.Count() + " skipped");
	}

	protected static void LoadCache(DZS_ExtSpecState st)
	{
		string path = CachePath(st.m_Url);
		if (!DZS_Files.Exists(path))
			return;
		array<string> chunks = DZS_Files.Read(path);
		if (!chunks)
			return;
		string err;
		DZS_Json root = DZS_Json.Parse(chunks, err);
		if (!root || err != "")
			return;
		string why;
		DZS_ExtSpec spec = DZS_ExtSpec.FromJson(st.m_Url, root, why);
		if (!spec)
			return;
		st.m_Spec = spec;
		st.m_FromCache = true;
	}

	// For the extension list: "loading …", "offline copy", the error, or "".
	static string StatusText(string url)
	{
		DZS_ExtSpecState st = State(url);
		if (st.m_Loading)
			return DZS_Texts.Pick("Spezifikation wird geladen …", "Loading the spec …");
		if (st.m_Spec && st.m_Error != "")
			return DZS_Texts.Pick("Gespeicherte Spezifikation (", "Saved copy of the spec (") + st.m_Error + ")";
		if (!st.m_Spec && st.m_Error != "")
			return DZS_Texts.Pick("Spezifikation nicht lesbar: ", "Spec not readable: ") + st.m_Error;
		if (!st.m_Spec)
			return DZS_Texts.Pick("Spezifikation nicht geladen", "Spec not loaded");
		return "";
	}
}

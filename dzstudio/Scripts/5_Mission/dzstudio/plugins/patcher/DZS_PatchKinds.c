// Block kinds as declarations: where a block lives, how its identity key is read and which fields it has. A field's
// path addresses it inside the block's element: "nominal" (element text), "flags@deletable" (attribute of a child),
// "@x" (attribute of the block's own element), "fog/limits@max" (nested), "usage[]" (list value of <usage name=…/>).
// Its public name is the path with "/" and "@" read as "." ("flags.deletable", "x", "usage").
class DZS_PField : Managed
{
	static const int INT = 0;
	static const int NUM = 1;
	static const int FIX6 = 2;
	static const int STR = 3;
	static const int BOOL01 = 4;
	static const int LIST = 5;
	static const int ENUMS = 6;
	static const int BOOL = 7;
	static const int VEC3 = 8;

	string m_Name;
	string m_Path;
	int m_Fmt;
	bool m_Req;
	ref array<string> m_Enum;
	ref array<string> m_Elems = new array<string>();
	string m_Attr;
	bool m_List;

	void DZS_PField(string path, int fmt, bool req)
	{
		m_Path = path;
		m_Fmt = fmt;
		m_Req = req;
		string p = path;
		if (p.IndexOf("[]") == p.Length() - 2)
		{
			m_List = true;
			p = p.Substring(0, p.Length() - 2);
		}
		int at = p.IndexOf("@");
		string elems = p;
		if (at >= 0)
		{
			m_Attr = p.Substring(at + 1, p.Length() - at - 1);
			elems = p.Substring(0, at);
		}
		if (elems != "")
			elems.Split("/", m_Elems);
		m_Name = "";
		foreach (string e : m_Elems)
		{
			if (m_Name != "")
				m_Name += ".";
			m_Name += e;
		}
		if (m_Attr != "")
		{
			if (m_Name != "")
				m_Name += ".";
			m_Name += m_Attr;
		}
	}

	// The field's elements as a path prefix ("fog/limits"), without the attribute.
	string ElemPath(int count)
	{
		string r = "";
		for (int i = 0; i < count && i < m_Elems.Count(); i++)
		{
			if (r != "")
				r += "/";
			r += m_Elems[i];
		}
		return r;
	}
}

class DZS_PKind : Managed
{
	static const int SINGLE = 0;
	static const int MERGE = 1;
	static const int LAST = 2;
	static const int FIRST = 3;

	string m_Name;
	string m_File;
	string m_Root;
	string m_Elem;
	string m_ParentKind;
	string m_ParentKey;
	string m_Container;
	string m_KeyAttr;
	bool m_KeyCi;
	// coordinate keys: x/z attributes, or one attribute holding "x y z" (m_Vec)
	int m_Tol = -1;
	string m_Vec;
	// how a key of this kind names its parent: "childKey=parentKey" pairs (default: m_ParentKey=parent's key attribute)
	ref array<string> m_ParentMap = new array<string>();
	// a remove takes every match (identical loot rows), a set still needs an ordinal
	bool m_RemoveAll;
	// own blocks may repeat an existing key (a second container named like the first)
	bool m_Repeats;
	// the file is part of the key ("file": "env/wolf_territories.xml"): one kind for every territory file
	bool m_FileInKey;
	// a key without its parent part searches the members of every parent element (a zone anywhere in its file)
	bool m_AnyParent;
	int m_Dups = SINGLE;
	string m_SelfClose = "/>";
	bool m_Singleton;
	ref array<ref DZS_PField> m_Fields = new array<ref DZS_PField>();
	ref array<string> m_Order = new array<string>();
	ref array<string> m_AttrOrder = new array<string>();

	DZS_PField F(string path, int fmt, bool req)
	{
		DZS_PField f = new DZS_PField(path, fmt, req);
		m_Fields.Insert(f);
		if (f.m_Elems.Count() == 0 && f.m_Attr != "")
			m_AttrOrder.Insert(f.m_Attr);
		return f;
	}

	DZS_PField E(string path, array<string> values, bool req)
	{
		DZS_PField f = F(path, DZS_PField.ENUMS, req);
		f.m_Enum = values;
		return f;
	}

	DZS_PField Field(string name)
	{
		foreach (DZS_PField f : m_Fields)
		{
			if (f.m_Name == name)
				return f;
		}
		return null;
	}

	bool IsCoord()
	{
		return m_Tol >= 0 && m_Vec == "";
	}

	bool IsVec()
	{
		return m_Tol >= 0 && m_Vec != "";
	}

	// A territory file path as the key names it, lower-cased; "" when it is no env/<name>_territories.xml.
	static string EnvFile(string p)
	{
		string low = DZS_PatchText.Lower(p);
		low.Replace("\\", "/");
		string ok = "abcdefghijklmnopqrstuvwxyz0123456789_";
		if (low.IndexOf("env/") != 0)
			return "";
		string tail = "_territories.xml";
		int at = low.Length() - tail.Length();
		if (at <= 4 || low.Substring(at, tail.Length()) != tail)
			return "";
		for (int i = 4; i < at; i++)
		{
			if (!ok.Contains(low.Get(i)))
				return "";
		}
		return low;
	}

	// Extra rules for an own block of this kind; "" when it may be written. 'values' are its fields by name.
	string CheckOwn(map<string, ref DZS_Json> values)
	{
		return "";
	}

	// Values the patcher adds to an own block (e.g. an angle a group position must carry).
	void Complete(map<string, ref DZS_Json> values)
	{
	}
}

class DZS_PKindEvent : DZS_PKind
{
	override string CheckOwn(map<string, ref DZS_Json> values)
	{
		string name = "";
		DZS_Json n = values.Get("name");
		if (n)
			name = n.m_Str;
		if (!DZS_PKinds.KnownPrefix(name))
			return "prefix";
		DZS_Json limit = values.Get("limit");
		DZS_Json mx = values.Get("max");
		if (limit && mx && (limit.m_Str == "parent" || limit.m_Str == "mixed") && DZS_PatchNum.Same(mx.m_Str, "0"))
			return "max-zero";
		return "";
	}
}

class DZS_PKindSpawnPos : DZS_PKind
{
	override string CheckOwn(map<string, ref DZS_Json> values)
	{
		if (values.Contains("group") && !values.Contains("y"))
			return "group-needs-y";
		return "";
	}

	override void Complete(map<string, ref DZS_Json> values)
	{
		if (values.Contains("group") && !values.Contains("a"))
		{
			DZS_Json zero = DZS_Json.Num("0");
			values.Set("a", zero);
		}
	}
}

// The declarations of every block kind the patcher knows.
class DZS_PKinds
{
	protected static ref map<string, ref DZS_PKind> s_Kinds;

	static bool KnownPrefix(string name)
	{
		array<string> prefixes = {"Vehicle", "Static", "Item", "Infected", "Animal", "Ambient", "Trajectory"};
		foreach (string p : prefixes)
		{
			if (name.IndexOf(p) == 0 && name.Length() > p.Length())
				return true;
		}
		return false;
	}

	// Event kinds that have no spawn positions (they spawn in territories or around players).
	static bool NoPositions(string eventName)
	{
		array<string> prefixes = {"Animal", "Ambient", "Infected", "Trajectory"};
		foreach (string p : prefixes)
		{
			if (eventName.IndexOf(p) == 0)
				return true;
		}
		return false;
	}

	static DZS_PKind Get(string name)
	{
		if (!s_Kinds)
			Build();
		return s_Kinds.Get(name);
	}

	protected static void Add(DZS_PKind k)
	{
		s_Kinds.Set(k.m_Name, k);
	}

	protected static void Build()
	{
		s_Kinds = new map<string, ref DZS_PKind>();
		array<string> on = {"fixed", "player", "uniform"};
		array<string> limits = {"child", "mixed", "parent", "custom"};

		DZS_PKind ev = new DZS_PKindEvent();
		ev.m_Name = "event";
		ev.m_File = "db/events.xml";
		ev.m_Root = "events";
		ev.m_Elem = "event";
		ev.m_KeyAttr = "name";
		ev.m_Dups = DZS_PKind.MERGE;
		ev.F("@name", DZS_PField.STR, true);
		ev.F("nominal", DZS_PField.INT, true);
		ev.F("min", DZS_PField.INT, true);
		ev.F("max", DZS_PField.INT, true);
		ev.F("lifetime", DZS_PField.INT, true);
		ev.F("restock", DZS_PField.INT, true);
		ev.F("saferadius", DZS_PField.INT, true);
		ev.F("distanceradius", DZS_PField.INT, true);
		ev.F("cleanupradius", DZS_PField.INT, true);
		ev.F("secondary", DZS_PField.STR, false);
		ev.F("flags@deletable", DZS_PField.BOOL01, true);
		ev.F("flags@init_random", DZS_PField.BOOL01, true);
		ev.F("flags@remove_damaged", DZS_PField.BOOL01, true);
		ev.E("position", on, true);
		ev.E("limit", limits, true);
		ev.F("active", DZS_PField.BOOL01, true);
		ev.m_Order = {"nominal", "min", "max", "lifetime", "restock", "saferadius", "distanceradius", "cleanupradius", "secondary", "flags", "position", "limit", "active", "children"};
		Add(ev);

		DZS_PKind ch = new DZS_PKind();
		ch.m_Name = "eventChild";
		ch.m_File = "db/events.xml";
		ch.m_Elem = "child";
		ch.m_ParentKind = "event";
		ch.m_ParentKey = "event";
		ch.m_Container = "children";
		ch.m_KeyAttr = "type";
		ch.m_KeyCi = true;
		ch.m_Dups = DZS_PKind.MERGE;
		ch.F("@lootmax", DZS_PField.INT, true);
		ch.F("@lootmin", DZS_PField.INT, true);
		ch.F("@max", DZS_PField.INT, true);
		ch.F("@min", DZS_PField.INT, true);
		ch.F("@type", DZS_PField.STR, true);
		Add(ch);

		DZS_PKind sl = new DZS_PKind();
		sl.m_Name = "spawnList";
		sl.m_File = "cfgeventspawns.xml";
		sl.m_Root = "eventposdef";
		sl.m_Elem = "event";
		sl.m_KeyAttr = "name";
		sl.m_Dups = DZS_PKind.MERGE;
		sl.m_SelfClose = " />";
		sl.F("@name", DZS_PField.STR, true);
		sl.F("zone@smin", DZS_PField.INT, false);
		sl.F("zone@smax", DZS_PField.INT, false);
		sl.F("zone@dmin", DZS_PField.INT, false);
		sl.F("zone@dmax", DZS_PField.INT, false);
		sl.F("zone@r", DZS_PField.NUM, false);
		sl.m_Order = {"zone", "pos"};
		Add(sl);

		DZS_PKind sp = new DZS_PKindSpawnPos();
		sp.m_Name = "spawnPosition";
		sp.m_File = "cfgeventspawns.xml";
		sp.m_Elem = "pos";
		sp.m_ParentKind = "spawnList";
		sp.m_ParentKey = "event";
		sp.m_Tol = 500000;
		sp.m_SelfClose = " />";
		sp.F("@x", DZS_PField.FIX6, true);
		sp.F("@z", DZS_PField.FIX6, true);
		sp.F("@a", DZS_PField.FIX6, false);
		sp.F("@y", DZS_PField.FIX6, false);
		sp.F("@group", DZS_PField.STR, false);
		Add(sp);

		DZS_PKind gr = new DZS_PKind();
		gr.m_Name = "eventGroup";
		gr.m_File = "cfgeventgroups.xml";
		gr.m_Root = "eventgroupdef";
		gr.m_Elem = "group";
		gr.m_KeyAttr = "name";
		gr.m_KeyCi = true;
		gr.F("@name", DZS_PField.STR, true);
		gr.m_Order = {"child"};
		Add(gr);

		DZS_PKind gc = new DZS_PKind();
		gc.m_Name = "eventGroupChild";
		gc.m_File = "cfgeventgroups.xml";
		gc.m_Elem = "child";
		gc.m_ParentKind = "eventGroup";
		gc.m_ParentKey = "group";
		gc.m_KeyAttr = "type";
		gc.m_KeyCi = true;
		gc.m_Tol = 250000;
		gc.F("@type", DZS_PField.STR, true);
		gc.F("@deloot", DZS_PField.BOOL01, false);
		gc.F("@lootmax", DZS_PField.INT, false);
		gc.F("@lootmin", DZS_PField.INT, false);
		array<string> tf = {"true", "false"};
		gc.E("@spawnsecondary", tf, false);
		gc.F("@x", DZS_PField.FIX6, true);
		gc.F("@z", DZS_PField.FIX6, true);
		gc.F("@a", DZS_PField.FIX6, true);
		gc.F("@y", DZS_PField.FIX6, false);
		Add(gc);

		DZS_PKind ty = new DZS_PKind();
		ty.m_Name = "itemType";
		ty.m_File = "db/types.xml";
		ty.m_Root = "types";
		ty.m_Elem = "type";
		ty.m_KeyAttr = "name";
		ty.m_KeyCi = true;
		ty.m_Dups = DZS_PKind.LAST;
		ty.F("@name", DZS_PField.STR, true);
		ty.F("nominal", DZS_PField.INT, true);
		ty.F("lifetime", DZS_PField.INT, true);
		ty.F("restock", DZS_PField.INT, true);
		ty.F("min", DZS_PField.INT, true);
		ty.F("quantmin", DZS_PField.INT, true);
		ty.F("quantmax", DZS_PField.INT, true);
		ty.F("cost", DZS_PField.INT, true);
		ty.F("flags@count_in_cargo", DZS_PField.BOOL01, true);
		ty.F("flags@count_in_hoarder", DZS_PField.BOOL01, true);
		ty.F("flags@count_in_map", DZS_PField.BOOL01, true);
		ty.F("flags@count_in_player", DZS_PField.BOOL01, true);
		ty.F("flags@crafted", DZS_PField.BOOL01, true);
		ty.F("flags@deloot", DZS_PField.BOOL01, true);
		ty.F("category@name", DZS_PField.STR, false);
		ty.F("tag[]", DZS_PField.LIST, false);
		ty.F("usage[]", DZS_PField.LIST, false);
		ty.F("value[]", DZS_PField.LIST, false);
		ty.m_Order = {"nominal", "lifetime", "restock", "min", "quantmin", "quantmax", "cost", "flags", "category", "tag", "usage", "value"};
		Add(ty);

		DZS_PKind lt = new DZS_PKind();
		lt.m_Name = "lootType";
		lt.m_File = "mapgroupproto.xml";
		lt.m_Root = "prototype";
		lt.m_Elem = "group";
		lt.m_KeyAttr = "name";
		lt.m_KeyCi = true;
		lt.m_Dups = DZS_PKind.FIRST;
		lt.m_SelfClose = " />";
		lt.F("@name", DZS_PField.STR, true);
		lt.F("@lootmax", DZS_PField.INT, false);
		lt.F("usage[]", DZS_PField.LIST, false);
		lt.F("value[]", DZS_PField.LIST, false);
		lt.m_Order = {"usage", "value", "container", "dispatch"};
		Add(lt);

		DZS_PKind lc = new DZS_PKind();
		lc.m_Name = "lootContainer";
		lc.m_File = "mapgroupproto.xml";
		lc.m_Elem = "container";
		lc.m_ParentKind = "lootType";
		lc.m_ParentKey = "group";
		lc.m_KeyAttr = "name";
		lc.m_Repeats = true;
		lc.m_SelfClose = " />";
		lc.F("@name", DZS_PField.STR, true);
		lc.F("@lootmax", DZS_PField.INT, false);
		lc.F("category[]", DZS_PField.LIST, false);
		lc.F("tag[]", DZS_PField.LIST, false);
		lc.m_Order = {"category", "tag", "point"};
		Add(lc);

		DZS_PKind lp = new DZS_PKind();
		lp.m_Name = "lootPoint";
		lp.m_File = "mapgroupproto.xml";
		lp.m_Elem = "point";
		lp.m_ParentKind = "lootContainer";
		lp.m_ParentKey = "group";
		lp.m_ParentMap = {"group=group", "container=name", "containerOrdinal=ordinal"};
		lp.m_Vec = "pos";
		lp.m_Tol = 10000;
		lp.m_SelfClose = " />";
		lp.F("@pos", DZS_PField.VEC3, true);
		lp.F("@range", DZS_PField.FIX6, true);
		lp.F("@height", DZS_PField.FIX6, true);
		lp.F("@flags", DZS_PField.INT, false);
		Add(lp);

		DZS_PKind lr = new DZS_PKind();
		lr.m_Name = "lootRow";
		lr.m_File = "mapgrouppos.xml";
		lr.m_Root = "map";
		lr.m_Elem = "group";
		lr.m_KeyAttr = "name";
		lr.m_KeyCi = true;
		lr.m_Vec = "pos";
		lr.m_Tol = 10000;
		lr.m_RemoveAll = true;
		lr.m_SelfClose = " />";
		lr.F("@name", DZS_PField.STR, true);
		lr.F("@pos", DZS_PField.VEC3, true);
		lr.F("@rpy", DZS_PField.VEC3, true);
		lr.F("@a", DZS_PField.FIX6, true);
		Add(lr);

		DZS_PKind tt = new DZS_PKind();
		tt.m_Name = "territory";
		tt.m_FileInKey = true;
		tt.m_Root = "territory-type";
		tt.m_Elem = "territory";
		tt.m_KeyAttr = "color";
		tt.m_RemoveAll = true;
		tt.m_Repeats = true;
		tt.F("@color", DZS_PField.STR, true);
		tt.m_Order = {"zone"};
		Add(tt);

		DZS_PKind tz = new DZS_PKind();
		tz.m_Name = "territoryZone";
		tz.m_FileInKey = true;
		tz.m_Elem = "zone";
		tz.m_ParentKind = "territory";
		tz.m_ParentKey = "color";
		tz.m_ParentMap = {"color=color", "territoryOrdinal=ordinal"};
		tz.m_AnyParent = true;
		tz.m_KeyAttr = "name";
		tz.m_Tol = 50000;
		tz.m_RemoveAll = true;
		tz.F("@name", DZS_PField.STR, true);
		tz.F("@smin", DZS_PField.INT, true);
		tz.F("@smax", DZS_PField.INT, true);
		tz.F("@dmin", DZS_PField.INT, true);
		tz.F("@dmax", DZS_PField.INT, true);
		tz.F("@x", DZS_PField.NUM, true);
		tz.F("@z", DZS_PField.NUM, true);
		tz.F("@r", DZS_PField.NUM, true);
		Add(tz);

		DZS_PKind we = new DZS_PKind();
		we.m_Name = "weather";
		we.m_File = "cfgweather.xml";
		we.m_Root = "weather";
		we.m_Elem = "weather";
		we.m_Singleton = true;
		we.m_SelfClose = " />";
		we.F("@reset", DZS_PField.BOOL01, true);
		we.F("@enable", DZS_PField.BOOL01, false);
		array<string> phen = {"overcast", "fog", "rain", "windMagnitude", "windDirection", "snowfall"};
		foreach (string ph : phen)
		{
			we.F(ph + "/current@actual", DZS_PField.NUM, true);
			we.F(ph + "/current@time", DZS_PField.INT, true);
			we.F(ph + "/current@duration", DZS_PField.INT, true);
			we.F(ph + "/limits@min", DZS_PField.NUM, true);
			we.F(ph + "/limits@max", DZS_PField.NUM, true);
			we.F(ph + "/timelimits@min", DZS_PField.INT, true);
			we.F(ph + "/timelimits@max", DZS_PField.INT, true);
			we.F(ph + "/changelimits@min", DZS_PField.NUM, true);
			we.F(ph + "/changelimits@max", DZS_PField.NUM, true);
			if (ph == "rain" || ph == "snowfall")
			{
				we.F(ph + "/thresholds@min", DZS_PField.NUM, true);
				we.F(ph + "/thresholds@max", DZS_PField.NUM, true);
				we.F(ph + "/thresholds@end", DZS_PField.INT, true);
			}
		}
		we.F("storm@density", DZS_PField.NUM, true);
		we.F("storm@threshold", DZS_PField.NUM, true);
		we.F("storm@timeout", DZS_PField.INT, true);
		we.m_Order = {"overcast", "fog", "rain", "windMagnitude", "windDirection", "snowfall", "storm", "current", "limits", "timelimits", "changelimits", "thresholds"};
		Add(we);
	}
}

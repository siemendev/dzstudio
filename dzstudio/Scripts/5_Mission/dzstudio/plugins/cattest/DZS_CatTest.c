// Test plugin for section 5 (start with -dzsCatTest). The class directory, hand, recent entries, templates
// and image source go through the public interface (DZS_CatalogApi); "list", "search" and "timing" read the
// catalog's state for the log. Commands come from $profile:dzstudio\cattest.txt (editor/mod-tools/catcmd.sh).
class DZS_CatTestImages : DZS_ImageSource
{
	ref map<string, string> m_Photos;
	ref map<string, string> m_Texts;

	void DZS_CatTestImages()
	{
		m_Photos = new map<string, string>();
		m_Texts = new map<string, string>();
	}

	override string GetPhoto(string name)
	{
		string low = name;
		low.ToLower();
		return m_Photos.Get(low);
	}

	override string GetDescription(string name)
	{
		string low = name;
		low.ToLower();
		return m_Texts.Get(low);
	}
}

[DZS_RegisterPlugin(DZS_CatTest)]
class DZS_CatTest : DZS_Plugin
{
	static const string CMD_FILE = "$profile:dzstudio\\cattest.txt";
	protected bool m_On;
	protected float m_Poll;
	protected ref DZS_CatTestImages m_Images;
	protected ref array<string> m_Typing;
	protected float m_TypeT;

	override string GetName() { return "cattest"; }

	override void OnRegister()
	{
		string v;
		m_On = GetCLIParam("dzsCatTest", v);
		if (m_On)
			DZS_Log.Info("cattest on, commands from " + CMD_FILE);
	}

	override void OnUpdate(float dt)
	{
		if (!m_On)
			return;
		m_Poll -= dt;
		if (m_Poll > 0)
			return;
		m_Poll = 0.25;
		if (!FileExist(CMD_FILE))
			return;
		array<string> lines = new array<string>();
		FileHandle fh = OpenFile(CMD_FILE, FileMode.READ);
		string line;
		while (FGets(fh, line) >= 0)
		{
			line.Trim();
			if (line != "")
				lines.Insert(line);
		}
		CloseFile(fh);
		DeleteFile(CMD_FILE);
		foreach (string l : lines)
			Run(l);
	}

	protected void Log(string s)
	{
		DZS_Log.Info("cattest " + s);
	}

	protected DZS_CatalogPlugin Cat()
	{
		return DZS_CatalogPlugin.Cast(m_Kernel.GetPlugin("catalog"));
	}

	protected void Run(string cmd)
	{
		array<string> a = new array<string>();
		cmd.Split(" ", a);
		string c = a[0];
		string arg = "";
		if (a.Count() > 1)
			arg = cmd.Substring(c.Length() + 1, cmd.Length() - c.Length() - 1);
		DZS_CatalogApi api = DZS_CatalogApi.Get();
		if (c == "dir")
		{
			Log("dir '" + arg + "': spelling '" + api.Spelling(arg) + "', in catalog " + DZS_Str.Bool(api.InCatalog(arg)) + ", shown '" + api.ShownName(arg) + "'");
			return;
		}
		if (c == "hand")
		{
			api.TakeInHand(arg);
			Log("hand " + api.GetHand());
			return;
		}
		if (c == "drop")
		{
			api.DropHand();
			return;
		}
		if (c == "used")
		{
			api.NoteUsed(arg);
			Log("used " + arg);
			return;
		}
		if (c == "usedlist")
		{
			array<string> names = new array<string>();
			arg.Split(",", names);
			foreach (string un : names)
				api.NoteUsed(un);
			Log("used " + names.Count());
			return;
		}
		if (c == "tpl")
		{
			// tpl Name#Class@x,y,z@yaw,pitch,roll;Class@...
			array<string> p = new array<string>();
			arg.Split("#", p);
			array<ref DZS_TemplateObject> objs = new array<ref DZS_TemplateObject>();
			array<string> items = new array<string>();
			p[1].Split(";", items);
			foreach (string it : items)
			{
				array<string> f = new array<string>();
				it.Split("@", f);
				DZS_TemplateObject o = new DZS_TemplateObject();
				o.m_Name = f[0];
				if (f.Count() > 1)
					o.m_Pos = Vec(f[1]);
				if (f.Count() > 2)
					o.m_Ypr = Vec(f[2]);
				objs.Insert(o);
			}
			Log("tpl saved id '" + api.SaveTemplate(p[0], objs) + "'");
			return;
		}
		if (c == "tplask")
		{
			array<string> p2 = new array<string>();
			arg.Split("#", p2);
			array<ref DZS_TemplateObject> objs2 = new array<ref DZS_TemplateObject>();
			array<string> cls = new array<string>();
			p2[1].Split(";", cls);
			int k = 0;
			foreach (string cn : cls)
			{
				DZS_TemplateObject o2 = new DZS_TemplateObject();
				o2.m_Name = cn;
				o2.m_Pos = Vector(k * 3, 0, 0);
				objs2.Insert(o2);
				k++;
			}
			Cat().m_Templates.AskSave(objs2, p2[0]);
			return;
		}
		if (c == "photo")
		{
			array<string> pp = new array<string>();
			arg.Split(" ", pp);
			if (!m_Images)
			{
				m_Images = new DZS_CatTestImages();
				api.AddImageSource(m_Images);
			}
			string low = pp[0];
			low.ToLower();
			m_Images.m_Photos.Set(low, pp[1]);
			if (pp.Count() > 2)
				m_Images.m_Texts.Set(low, arg.Substring(pp[0].Length() + pp[1].Length() + 2, arg.Length() - pp[0].Length() - pp[1].Length() - 2));
			Cat().ForgetPhotos();
			Log("photo " + pp[0] + " -> " + pp[1]);
			return;
		}
		if (c == "cat")
		{
			DZS_CatalogPlugin gp = Cat();
			gp.SetCategory(a[1]);
			if (a.Count() > 2)
				gp.SetGroup(a[2]);
			Log("cat " + gp.m_Cat + " group '" + gp.m_Group + "'");
			return;
		}
		if (c == "full")
		{
			Cat().OpenFull();
			Log("full open");
			return;
		}
		if (c == "list")
		{
			DZS_CatalogPlugin cp = Cat();
			int n = cp.ListCount();
			int show = Math.Min(n, arg.ToInt());
			if (show <= 0)
				show = Math.Min(n, 12);
			Log("list " + cp.m_Cat + ": " + n + " entries, hand '" + api.GetHand() + "'");
			for (int i = 0; i < show; i++)
			{
				DZS_CatEntry e = cp.ListAt(i);
				Log("  " + i + ": " + e.m_Name + " | " + e.m_Key + " | " + e.m_Cat + "/" + e.m_Group);
			}
			return;
		}
		if (c == "search")
		{
			DZS_CatalogPlugin sp = Cat();
			int t0 = TickCount(0);
			DZS_CatResult res = sp.m_Store.Search(arg, sp.m_Store.m_All, false);
			Log("search '" + arg + "': " + res.m_Items.Count() + " in " + (TickCount(0) - t0) / 10000.0 + " ms");
			for (int j = 0; j < Math.Min(10, res.m_Items.Count()); j++)
				Log("  " + j + ": " + res.m_Items[j].m_Name + " | " + res.m_Items[j].m_Key);
			return;
		}
		if (c == "timing")
		{
			DZS_CatalogPlugin tp = Cat();
			float worst = 0;
			string text = "";
			for (int q = 0; q < arg.Length(); q++)
			{
				text += arg.Get(q);
				int t1 = TickCount(0);
				tp.m_Store.Search(text, tp.m_Store.m_All, false);
				float ms = (TickCount(0) - t1) / 10000.0;
				Log("timing '" + text + "' " + ms + " ms");
				worst = Math.Max(worst, ms);
			}
			Log("timing worst " + worst + " ms");
			return;
		}
		if (c == "counts")
		{
			DZS_CatalogPlugin kp = Cat();
			map<string, int> per = new map<string, int>();
			int named = 0;
			foreach (DZS_CatEntry ce : kp.m_Store.m_Objects)
			{
				per.Set(ce.m_Cat, per.Get(ce.m_Cat) + 1);
				string cg = ce.m_Cat + "/" + ce.m_Group;
				per.Set(cg, per.Get(cg) + 1);
			}
			int sum = 0;
			foreach (string ck, int cn2 : per)
			{
				Log("count " + ck + " " + cn2);
				if (!ck.Contains("/"))
					sum += cn2;
			}
			Log("count sum " + sum + ", offered " + kp.m_Store.CountObjects() + ", ready after " + kp.m_Store.m_ReadyAfter + " s");
			return;
		}
		Log("unknown command " + cmd);
	}

	protected vector Vec(string s)
	{
		array<string> p = new array<string>();
		s.Split(",", p);
		if (p.Count() != 3)
			return vector.Zero;
		return Vector(p[0].ToFloat(), p[1].ToFloat(), p[2].ToFloat());
	}
}

// The patcher without a user interface, from the command line (paths below $profile: unless they carry a prefix):
//   -dzsPatch=<project.json> -dzsPatchBase=<mission folder> -dzsPatchOut=<folder>
//       writes the changed files (never in place) and report.json into the output folder
//   -dzsPatchTest=<folder>   runs every conformance case in the folder (base/, project.json, expected/, report.json)
//   -dzsPatchSmoke=<folder>  reads every file the patcher handles in each mission folder below and writes it back
//   -dzsLootBlocksTest=<folder>  loot edits through today's writer and as building blocks, per mission folder below
//   -dzsPatchQuit            ends the game afterwards
class DZS_PatchActions : DZS_ActionHandler
{
	DZS_PatchApply m_A;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		return m_A.CanApply(reasonDe, reasonEn);
	}

	override void OnAction(string actionId)
	{
		if (actionId == "patcher.apply")
			m_A.Ask();
	}
}

[DZS_RegisterPlugin(DZS_PatchPlugin)]
class DZS_PatchPlugin : DZS_Plugin
{
	protected string m_Patch;
	protected string m_Test;
	protected string m_Smoke;
	protected string m_LootTest;
	protected bool m_Ran;
	protected float m_Wait;
	protected ref DZS_PatchApply m_Apply;
	protected ref DZS_PatchActions m_Actions;

	override string GetName() { return "patcher"; }

	override void OnRegister()
	{
		GetCLIParam("dzsPatch", m_Patch);
		GetCLIParam("dzsPatchTest", m_Test);
		GetCLIParam("dzsPatchSmoke", m_Smoke);
		GetCLIParam("dzsLootBlocksTest", m_LootTest);
		m_Apply = new DZS_PatchApply(m_Kernel);
		m_Actions = new DZS_PatchActions();
		m_Actions.m_A = m_Apply;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterAction("patcher", "patcher.apply", "Auf Missionsordner anwenden …", "Apply to mission folder …", null, m_Actions);
		ui.AddMenuEntry("project", "patcher.apply", 455);
		DZS_Json none = DZS_Json.Obj();
		m_Kernel.GetSettings().Define(DZS_PatchApply.SETTING, none);
	}

	DZS_PatchApply Applier()
	{
		return m_Apply;
	}

	override void OnUpdate(float dt)
	{
		if (m_Ran || (m_Patch == "" && m_Test == "" && m_Smoke == "" && m_LootTest == ""))
			return;
		m_Wait += dt;
		if (m_Wait < 1.0)
			return;
		m_Ran = true;
		if (m_Test != "")
			DZS_PatchTests.RunAll(Path(m_Test));
		if (m_Smoke != "")
			DZS_PatchTests.Smoke(Path(m_Smoke));
		if (m_LootTest != "")
		{
			array<string> names;
			array<bool> folders;
			string lootDir = Path(m_LootTest);
			DZS_Files.List(lootDir, names, folders);
			for (int i = 0; i < names.Count(); i++)
			{
				if (folders[i])
					DZS_LootBlocksTest.Run(lootDir + "\\" + names[i]);
			}
		}
		if (m_Patch != "")
		{
			string baseDir;
			string outDir;
			GetCLIParam("dzsPatchBase", baseDir);
			GetCLIParam("dzsPatchOut", outDir);
			string projectPath = Path(m_Patch);
			string basePath = Path(baseDir);
			string outPath = Path(outDir);
			PatchToFolder(projectPath, basePath, outPath);
		}
		if (IsCLIParam("dzsPatchQuit"))
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(GetGame().RequestExit, 500, false, IDC_MAIN_QUIT);
	}

	static string Path(string p)
	{
		string r = p;
		r.Replace("/", "\\");
		if (r.IndexOf(":") < 0)
			r = "$profile:" + r;
		return r;
	}

	// Reads a project, patches the base and writes the result; false when the project cannot be read.
	static bool PatchToFolder(string projectPath, string baseDir, string outDir)
	{
		array<string> chunks = DZS_Files.Read(projectPath);
		if (!chunks)
		{
			DZS_Log.Error("patch: cannot read " + projectPath);
			return false;
		}
		string err;
		DZS_PatchProject project = DZS_PatchProject.Read(chunks, err);
		if (!project)
		{
			DZS_Log.Error("patch: " + projectPath + ": " + err);
			return false;
		}
		int t0 = TickCount(0);
		DZS_Patcher p = DZS_Patcher.Run(project, baseDir);
		int ms = (TickCount(0) - t0) / 10000;
		p.m_Report.Log();
		DZS_Files.MakeDirs(outDir);
		foreach (string rel : p.m_OutPaths)
		{
			string full = outDir + "\\" + rel;
			full.Replace("/", "\\");
			int slash = full.LastIndexOf("\\");
			DZS_Files.MakeDirs(full.Substring(0, slash));
			array<string> data = p.m_Out.Get(rel);
			if (!DZS_Files.Write(full, data))
				DZS_Log.Error("patch: cannot write " + full);
		}
		array<string> report = p.m_Report.Write();
		DZS_Files.Write(outDir + "\\report.json", report);
		string counts = "applied " + p.m_Report.Count("applied") + ", overrode " + p.m_Report.Count("overrode") + ", already " + p.m_Report.Count("already") + ", invalid " + p.m_Report.Count("invalid");
		DZS_Log.Info("patch: done in " + ms + " ms, " + p.m_OutPaths.Count() + " files, " + counts + " -> " + outDir);
		return true;
	}
}

// The conformance runner and the smoke run over whole mission folders.
class DZS_PatchTests
{
	static void RunAll(string dir)
	{
		array<string> names;
		array<bool> folders;
		DZS_Files.List(dir, names, folders);
		array<string> cases = new array<string>();
		for (int i = 0; i < names.Count(); i++)
		{
			if (folders[i])
				cases.Insert(names[i]);
		}
		cases.Sort();
		int pass = 0;
		int fail = 0;
		foreach (string c : cases)
		{
			if (RunCase(dir, c))
				pass++;
			else
				fail++;
		}
		DZS_Log.Info("patchtest: " + pass + " passed, " + fail + " failed");
	}

	static bool RunCase(string dir, string name)
	{
		string caseDir = dir + "\\" + name;
		array<string> chunks = DZS_Files.Read(caseDir + "\\project.json");
		if (!chunks)
		{
			DZS_Log.Info("patchtest: FAIL " + name + ": no project.json");
			return false;
		}
		string err;
		DZS_PatchProject project = DZS_PatchProject.Read(chunks, err);
		if (!project)
		{
			DZS_Log.Info("patchtest: FAIL " + name + ": " + err);
			return false;
		}
		string trip = RoundTrip(project);
		if (trip != "")
		{
			DZS_Log.Info("patchtest: FAIL " + name + ": project round trip " + trip);
			return false;
		}
		int t0 = TickCount(0);
		DZS_Patcher p = DZS_Patcher.Run(project, caseDir + "\\base");
		int ms = (TickCount(0) - t0) / 10000;
		string outDir = dir + "-out\\" + name;
		DZS_Files.DeleteTree(outDir);
		DZS_Files.MakeDirs(outDir);
		foreach (string rel0 : p.m_OutPaths)
		{
			string full = outDir + "\\" + rel0;
			full.Replace("/", "\\");
			int slash = full.LastIndexOf("\\");
			DZS_Files.MakeDirs(full.Substring(0, slash));
			array<string> data0 = p.m_Out.Get(rel0);
			DZS_Files.Write(full, data0);
		}
		array<string> reportOut = p.m_Report.Write();
		DZS_Files.Write(outDir + "\\report.json", reportOut);
		string problem = "";
		array<string> expected = new array<string>();
		string expDir = caseDir + "\\expected";
		if (DZS_Files.IsFolder(expDir))
			expected = DZS_Files.ListTree(expDir);
		foreach (string rel : expected)
		{
			if (problem != "")
				break;
			string full2 = expDir + "\\" + rel;
			full2.Replace("/", "\\");
			array<string> want = DZS_Files.Read(full2);
			array<string> got = p.m_Out.Get(rel);
			if (!got)
				problem = rel + " not written";
			else
				problem = Diff(rel, want, got);
		}
		foreach (string rel2 : p.m_OutPaths)
		{
			if (problem == "" && expected.Find(rel2) < 0)
				problem = rel2 + " written but not expected";
		}
		if (problem == "")
		{
			array<string> wantReport = DZS_Files.Read(caseDir + "\\report.json");
			if (!wantReport)
				problem = "no report.json";
			else
			{
				DZS_SrcDoc wd = DZS_SrcJson.Parse(wantReport);
				if (wd.m_Error != "")
					problem = "report.json: " + wd.ErrorText();
				else
				{
					DZS_Json wj = wd.m_Root.ToJson();
					array<string> wantC = wj.ToChunks(false);
					DZS_Json gj = p.m_Report.ToJson();
					array<string> gotC = gj.ToChunks(false);
					problem = Diff("report.json", wantC, gotC);
				}
			}
		}
		if (problem != "")
		{
			DZS_Log.Info("patchtest: FAIL " + name + ": " + problem);
			p.m_Report.Log();
			return false;
		}
		DZS_Log.Info("patchtest: PASS " + name + " (" + ms + " ms)");
		return true;
	}

	// "" when the project reads back the same after writing it.
	static string RoundTrip(DZS_PatchProject project)
	{
		array<string> written = project.Write();
		string err;
		DZS_PatchProject again = DZS_PatchProject.Read(written, err);
		if (!again)
			return err;
		DZS_Json a = project.ToJson();
		DZS_Json b = again.ToJson();
		array<string> ac = a.ToChunks(false);
		array<string> bc = b.ToChunks(false);
		return Diff("project", ac, bc);
	}

	// "" when equal, else the first differing byte offset with some text around it on both sides.
	static string Diff(string what, array<string> want, array<string> got)
	{
		if (DZS_Files.SameContent(want, got))
			return "";
		string a = DZS_Files.Join(want);
		string b = DZS_Files.Join(got);
		int n = Math.Min(a.Length(), b.Length());
		int at = 0;
		while (at < n && a.Get(at) == b.Get(at))
			at++;
		int from = Math.Max(0, at - 60);
		string ca = DZS_Utf8.Part(a, from, Math.Min(a.Length(), at + 60) - from);
		string cb = DZS_Utf8.Part(b, from, Math.Min(b.Length(), at + 60) - from);
		ca.Replace("\r", "\\r");
		ca.Replace("\n", "\\n");
		ca.Replace("\t", "\\t");
		cb.Replace("\r", "\\r");
		cb.Replace("\n", "\\n");
		cb.Replace("\t", "\\t");
		return what + " differs at byte " + at + " (expected " + a.Length() + " bytes, got " + b.Length() + ")\n  expected: " + ca + "\n  got:      " + cb;
	}

	// Every file the patcher handles, in each mission folder below 'dir': read, resolve every block by its own key,
	// write back without edits; then an empty project. Nothing may change.
	static void Smoke(string dir)
	{
		array<string> names;
		array<bool> folders;
		DZS_Files.List(dir, names, folders);
		array<string> files = {"db/events.xml", "cfgeventspawns.xml", "cfgeventgroups.xml", "cfgeffectarea.json", "cfgweather.xml", "cfggameplay.json", "db/types.xml", "mapgroupproto.xml", "mapgrouppos.xml"};
		int bad = 0;
		for (int i = 0; i < names.Count(); i++)
		{
			if (!folders[i])
				continue;
			string mission = dir + "\\" + names[i];
			DZS_PatchFiles fileSet = new DZS_PatchFiles(mission);
			foreach (string rel : files)
			{
				int t0 = TickCount(0);
				DZS_PatchFile f = fileSet.Get(rel);
				int ms = (TickCount(0) - t0) / 10000;
				if (!f.m_Exists)
				{
					DZS_Log.Info("patchsmoke: " + names[i] + " " + rel + " missing");
					continue;
				}
				if (f.m_Error != "")
				{
					DZS_Log.Info("patchsmoke: FAIL " + names[i] + " " + rel + " " + f.m_ErrorText);
					bad++;
					continue;
				}
				array<ref DZS_XmlPatch> none = new array<ref DZS_XmlPatch>();
				array<string> back = DZS_XmlWriter.Apply(f.m_Xml, none);
				bool same = DZS_Files.SameContent(back, f.m_Chunks);
				if (!same)
					bad++;
				int t1 = TickCount(0);
				string keys = "";
				if (!f.m_IsJson)
					keys = KeyCheck(f);
				int kms = (TickCount(0) - t1) / 10000;
				DZS_Log.Info("patchsmoke: " + names[i] + " " + f.m_Path + " " + DZS_Files.Length(f.m_Chunks) + " bytes, read " + ms + " ms, same bytes " + same + ", keys " + keys + " in " + kms + " ms");
			}
			DZS_PatchProject empty = new DZS_PatchProject();
			int t2 = TickCount(0);
			DZS_Patcher p = DZS_Patcher.Run(empty, mission);
			int pms = (TickCount(0) - t2) / 10000;
			if (p.m_OutPaths.Count() > 0)
				bad++;
			DZS_Log.Info("patchsmoke: " + names[i] + " empty project: " + p.m_OutPaths.Count() + " files written, " + pms + " ms");
		}
		DZS_Log.Info("patchsmoke: done, " + bad + " problems");
	}

	// Resolves every block of the file's kinds by its own key, as an editor would address it (ordinals only where a
	// container name repeats); returns counts of unique / duplicate / not found per kind.
	protected static string KeyCheck(DZS_PatchFile f)
	{
		DZS_Patcher p = new DZS_Patcher();
		array<string> kinds = {"event", "eventChild", "spawnList", "spawnPosition", "eventGroup", "eventGroupChild", "itemType", "lootType", "lootContainer", "lootPoint", "lootRow"};
		string r = "";
		foreach (string kn : kinds)
		{
			DZS_PKind k = DZS_PKinds.Get(kn);
			if (DZS_PatchText.Lower(f.m_Path) != k.m_File)
				continue;
			array<DZS_XmlNode> nodes = new array<DZS_XmlNode>();
			array<ref DZS_Json> keys = new array<ref DZS_Json>();
			Collect(p, f, k, nodes, keys);
			int ok = 0;
			int multi = 0;
			int lost = 0;
			for (int i = 0; i < nodes.Count(); i++)
			{
				DZS_PTarget t = p.Resolve(f, k, keys[i], false);
				if (t.m_Ambiguous || t.m_Nodes.Count() > 1)
					multi++;
				else if (t.m_Nodes.Count() == 1 && t.m_Nodes[0] == nodes[i])
					ok++;
				else if (t.m_Nodes.Count() == 1 && k.m_Dups != DZS_PKind.SINGLE)
					ok++;
				else
					lost++;
			}
			r += kn + " " + ok + "/" + multi + "/" + lost + " ";
		}
		return r;
	}

	// Every element of a kind with its key, through up to two parent levels.
	protected static void Collect(DZS_Patcher p, DZS_PatchFile f, DZS_PKind k, array<DZS_XmlNode> nodes, array<ref DZS_Json> keys)
	{
		DZS_XmlNode root = f.m_Xml.m_Root;
		array<DZS_XmlNode> tops = new array<DZS_XmlNode>();
		array<ref DZS_Json> topKeys = new array<ref DZS_Json>();
		DZS_PKind top = k;
		while (top.m_ParentKind != "")
			top = DZS_PKinds.Get(top.m_ParentKind);
		foreach (DZS_XmlNode c : root.m_Children)
		{
			if (c.m_Name != top.m_Elem)
				continue;
			tops.Insert(c);
			DZS_Json none = DZS_Json.Obj();
			topKeys.Insert(KeyOf(top, top, c, none, 0));
		}
		if (k == top)
		{
			nodes.Copy(tops);
			foreach (DZS_Json tk : topKeys)
				keys.Insert(tk);
			return;
		}
		DZS_PKind mid = k;
		if (DZS_PKinds.Get(k.m_ParentKind) != top)
			mid = DZS_PKinds.Get(k.m_ParentKind);
		for (int i = 0; i < tops.Count(); i++)
		{
			array<DZS_XmlNode> mids = new array<DZS_XmlNode>();
			p.Members(f, tops[i], mid, mids);
			foreach (DZS_XmlNode m : mids)
			{
				int ord = 0;
				if (mid.m_KeyAttr != "" && mid.m_Dups == DZS_PKind.SINGLE && !mid.IsCoord() && !mid.IsVec())
					ord = OrdinalAmong(mids, m, mid.m_KeyAttr);
				DZS_Json midKey = KeyOf(mid, top, m, topKeys[i], ord);
				if (mid == k)
				{
					nodes.Insert(m);
					keys.Insert(midKey);
					continue;
				}
				array<DZS_XmlNode> leaves = new array<DZS_XmlNode>();
				p.Members(f, m, k, leaves);
				foreach (DZS_XmlNode l : leaves)
				{
					nodes.Insert(l);
					keys.Insert(KeyOf(k, mid, l, midKey, 0));
				}
			}
		}
	}

	// 1-based position among same-named siblings when the name repeats, else 0.
	protected static int OrdinalAmong(array<DZS_XmlNode> sibs, DZS_XmlNode n, string attr)
	{
		int count = 0;
		int at = 0;
		foreach (DZS_XmlNode s : sibs)
		{
			if (s.Get(attr) != n.Get(attr))
				continue;
			count++;
			if (s == n)
				at = count;
		}
		if (count < 2)
			return 0;
		return at;
	}

	protected static DZS_Json KeyOf(DZS_PKind k, DZS_PKind pk, DZS_XmlNode n, DZS_Json parentKey, int ordinal)
	{
		DZS_Json key = DZS_Json.Obj();
		if (k.m_ParentKind != "")
		{
			array<string> pairs = new array<string>();
			pairs.Copy(k.m_ParentMap);
			if (pairs.Count() == 0)
				pairs.Insert(k.m_ParentKey + "=" + pk.m_KeyAttr);
			foreach (string pair : pairs)
			{
				int eq = pair.IndexOf("=");
				string mine = pair.Substring(0, eq);
				string theirs = pair.Substring(eq + 1, pair.Length() - eq - 1);
				DZS_Json v = parentKey.Get(theirs);
				if (v)
				{
					DZS_Json vc = DZS_Json.Str(v.m_Str);
					key.Set(mine, vc);
				}
			}
		}
		if (k.m_KeyAttr != "")
		{
			DZS_Json nj = DZS_Json.Str(n.Get(k.m_KeyAttr));
			key.Set(k.m_KeyAttr, nj);
		}
		if (k.IsCoord())
		{
			DZS_Json xj = DZS_Json.Num(n.Get("x"));
			DZS_Json zj = DZS_Json.Num(n.Get("z"));
			key.Set("x", xj);
			key.Set("z", zj);
		}
		if (k.IsVec())
		{
			DZS_Json vj = DZS_Json.Str(n.Get(k.m_Vec));
			key.Set(k.m_Vec, vj);
		}
		if (ordinal > 0)
		{
			DZS_Json oj = DZS_Json.Num(ordinal.ToString());
			key.Set("ordinal", oj);
		}
		return key;
	}
}

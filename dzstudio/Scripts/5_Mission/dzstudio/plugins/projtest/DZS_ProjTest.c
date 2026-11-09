// Test plugin for section 4 (start with -dzsProjTest). Creates and changes objects only through
// operations of the public kernel interface, runs the number and format checks in the game.
// Commands come from $profile:dzstudio\projtest.txt (editor/mod-tools/projcmd.sh).
[DZS_RegisterPlugin(DZS_ProjTest)]
class DZS_ProjTest : DZS_Plugin
{
	static const string CMD_FILE = "$profile:dzstudio\\projtest.txt";
	protected bool m_On;
	protected float m_Poll;
	protected int m_StressCycles;
	protected int m_StressCycle;
	protected int m_StressStep;
	protected int m_StressN;
	protected string m_StressName;
	protected float m_StressWait;
	protected float m_StressPause = 0.2;
	protected string m_HeapMode;
	protected int m_HeapLeft;
	protected int m_HeapDone;
	protected string m_HeapFolder;
	protected ref array<string> m_SynChunks;
	protected string m_LastFolder;
	protected float m_FpsLeft;
	protected float m_FpsTime;
	protected int m_FpsFrames;
	protected float m_FpsWorst;
	protected int m_FpsRays;
	protected int m_FpsRaysMax;
	protected int m_FpsRaysZero;
	protected int m_ReopenLeft;
	protected float m_ReopenWait;
	protected int m_ReopenDone;
	protected int m_SettleNext = -1;
	protected ref array<int> m_SettleSkip;

	override string GetName() { return "projtest"; }

	override void OnRegister()
	{
		string v;
		m_On = GetCLIParam("dzsProjTest", v);
		if (!m_On)
			return;
		m_Kernel.GetTypes().RegisterOperationType("projtest", "projtest:edit", 1);
		DZS_Log.Info("projtest on, commands from " + CMD_FILE);
	}

	override void OnUpdate(float dt)
	{
		if (!m_On)
			return;
		if (m_StressCycles > 0)
			StressTick(dt);
		if (m_ReopenLeft > 0)
			ReopenTick(dt);
		if (m_HeapLeft > 0)
			HeapTick();
		if (m_FpsLeft > 0)
			FpsTick(dt);
		if (m_SettleNext >= 0)
			SettleTick();
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

	protected void SettleTick()
	{
		DZS_Instance inst = m_Kernel.GetShown();
		array<string> ids = inst.GetChildren(FirstGroup(inst));
		if (m_SettleNext >= ids.Count())
		{
			Log("settleeach done, " + ids.Count() + " objects");
			m_SettleNext = -1;
			return;
		}
		int i = m_SettleNext;
		m_SettleNext++;
		if (m_SettleSkip.Find(i) >= 0)
			return;
		DZS_SelectionApi sapi = DZS_SelectionApi.Cast(m_Kernel.GetService("selection"));
		array<string> one = new array<string>();
		string id = ids[i];
		one.Insert(id);
		sapi.SetSelection(one, id);
		bool ok = m_Kernel.GetUi().Trigger("placement.surface");
		Log("settleeach " + i + " " + ok);
	}

	protected Widget m_EditProbe;

	protected void BoxProbe(string name, vector at)
	{
		Object o = DZS_ScenePlugin.Make(name, at, "0 0 0", 1, true);
		if (!o)
		{
			Log("boxprobe " + name + ": not made");
			return;
		}
		o.Update();
		vector cb[2];
		bool hasCb = o.GetCollisionBox(cb);
		vector cl[2];
		o.ClippingInfo(cl);
		array<int> kinds = {ObjIntersectView, ObjIntersectGeom, ObjIntersectFire};
		array<string> kindNames = {"view", "geom", "fire"};
		for (int k = 0; k < kinds.Count(); k++)
		{
			float low = 1e9;
			float high = -1e9;
			int hits = 0;
			for (int i = 0; i <= 20; i++)
			{
				for (int j = 0; j <= 20; j++)
				{
					float lx = cl[0][0] + (cl[1][0] - cl[0][0]) * (0.01 + 0.98 * i / 20.0);
					float lz = cl[0][2] + (cl[1][2] - cl[0][2]) * (0.01 + 0.98 * j / 20.0);
					vector w0 = o.ModelToWorld(Vector(lx, cl[0][1] - 3, lz));
					vector w1 = o.ModelToWorld(Vector(lx, cl[1][1] + 3, lz));
					vector cp, cd;
					int comp;
					set<Object> res = new set<Object>();
					bool hit = DayZPhysics.RaycastRV(w0, w1, cp, cd, comp, res, null, null, false, false, kinds[k], 0);
					if (!hit)
						continue;
					bool mine = false;
					foreach (Object ro : res)
					{
						if (ro == o || (ro && ro.GetType() == ""))
							mine = true;
					}
					if (!mine)
						continue;
					vector l = o.WorldToModel(cp);
					float ly = l[1];
					hits++;
					if (ly < low)
						low = ly;
					if (ly > high)
						high = ly;
				}
			}
			Log("boxprobe " + name + " " + kindNames[k] + ": rays hit " + hits + "/441, lowest bottom " + low + ", highest bottom " + high);
		}
		Log("boxprobe " + name + ": collision " + hasCb + " " + cb[0] + " " + cb[1] + ", clipping " + cl[0] + " " + cl[1]);
		GetGame().ObjectDelete(o);
	}

	protected void Log(string s)
	{
		DZS_Log.Info("projtest " + s);
	}

	protected string FirstGroup(DZS_Instance inst)
	{
		array<string> g = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string id : g)
		{
			string u;
			inst.GetField(id, "unreadable", u);
			if (inst.GetKind(id) == DZS_KIND_GROUP && u != "1")
				return id;
		}
		return "";
	}

	protected string ObjectAt(DZS_Instance inst, int i)
	{
		array<string> ids = inst.GetChildren(FirstGroup(inst));
		if (i < 0 || i >= ids.Count())
			return "";
		return ids[i];
	}

	protected map<string, string> Fields(string name, float x, float y, float z, float yaw, float pitch, float roll, float scale, string custom = "")
	{
		map<string, string> f = new map<string, string>();
		f.Set(DZS_Obj.NAME, name);
		f.Set(DZS_Obj.POS, DZS_Obj.Vec(x, y, z));
		f.Set(DZS_Obj.YPR, DZS_Obj.Vec(yaw, pitch, roll));
		f.Set(DZS_Obj.SCALE, DZS_Obj.Num(scale));
		f.Set(DZS_Obj.PERSIST, "0");
		f.Set(DZS_Obj.CUSTOM, custom);
		return f;
	}

	protected ref DZS_LootVanilla m_Vanilla;

	protected void VanillaDone(DZS_LootVanilla v)
	{
		Log("vanillamission " + v.m_Map + " ok " + v.m_Ok + " " + v.m_Why + ", complete " + DZS_LootVanilla.HaveMission(v.m_Map) + " in " + DZS_LootVanilla.MissionDir(v.m_Map));
	}

	protected void Run(string cmd)
	{
		Log("cmd " + cmd);
		array<string> a = new array<string>();
		cmd.Split(" ", a);
		DZS_Instance inst = m_Kernel.GetShown();
		string c = a[0];
		if (c == "set" && !inst)
			inst = null;
		if (c == "f32")
		{
			F32Checks();
			return;
		}
		DZS_ProjectsPlugin pp0 = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (c == "newproj")
		{
			string npMap = "chernarus";
			if (a.Count() > 2)
				npMap = a[2];
			pp0.RequestNew(a[1], npMap);
			return;
		}
		if (c == "heap")
		{
			m_HeapMode = a[1];
			m_HeapLeft = a[2].ToInt();
			m_HeapFolder = a[3];
			m_HeapDone = 0;
			return;
		}
		if (c == "open")
		{
			m_LastFolder = a[1];
			pp0.LoadProject(a[1], a.Count() < 3 || a[2] != "sync");
			return;
		}
		if (c == "binwrite")
		{
			// binwrite — FileSerializer: int 0x04030201, bool true, int 0, then a byte through FPrint
			FileSerializer fs = new FileSerializer();
			bool bwo = fs.Open("$profile:dzstudio\\bin.test", FileMode.WRITE);
			int v1 = 67305985;
			fs.Write(v1);
			bool v2 = true;
			fs.Write(v2);
			int v3 = 0;
			fs.Write(v3);
			fs.Close();
			DZS_Log.Info("projtest binwrite open " + bwo);
			return;
		}
		if (c == "near")
		{
			// near X Z R — map objects with a CfgVehicles class within R metres
			vector nc = Vector(a[1].ToFloat(), 0, a[2].ToFloat());
			nc[1] = GetGame().SurfaceY(nc[0], nc[2]);
			array<Object> found = new array<Object>();
			array<CargoBase> cargos = new array<CargoBase>();
			GetGame().GetObjectsAtPosition3D(nc, a[3].ToFloat(), found, cargos);
			map<string, int> seenTypes = new map<string, int>();
			foreach (Object nfo : found)
			{
				string ft = nfo.GetType();
				if (ft == "" || seenTypes.Contains(ft) || !GetGame().ConfigIsExisting("CfgVehicles " + ft))
					continue;
				seenTypes.Set(ft, 1);
				DZS_Log.Info("projtest near " + ft + " " + nfo.GetPosition());
			}
			return;
		}
		if (c == "charhand")
		{
			// charhand CLASS [AMMO] — the steered figure gets CLASS into its hands (what it held goes), loaded with AMMO
			PlayerBase chp = PlayerBase.Cast(GetGame().GetPlayer());
			if (!chp)
			{
				Log("charhand: no figure");
				return;
			}
			EntityAI chOld = chp.GetHumanInventory().GetEntityInHands();
			if (chOld)
				GetGame().ObjectDelete(chOld);
			EntityAI chNew = chp.GetHumanInventory().CreateInHands(a[1]);
			bool chLoaded = false;
			Weapon_Base chW = Weapon_Base.Cast(chNew);
			if (chW && a.Count() > 2)
				chLoaded = chW.SpawnAmmo(a[2]);
			Log("charhand " + a[1] + ": " + (chNew != null) + ", loaded " + chLoaded);
			return;
		}
		if (c == "editprobe")
		{
			// editprobe N — edit boxes in the game's styles (layout dzs_editprobe), field N gets the focus; "editprobe off"
			if (a.Count() > 1 && a[1] == "off")
			{
				if (m_EditProbe)
					m_EditProbe.Unlink();
				m_EditProbe = null;
				return;
			}
			if (!m_EditProbe)
			{
				m_EditProbe = GetGame().GetWorkspace().CreateWidgets("dzstudio/gui/layouts/dzs_editprobe.layout", null);
				for (int edpI = 0; edpI < 7; edpI++)
				{
					EditBoxWidget edpW = EditBoxWidget.Cast(m_EditProbe.FindAnyWidget("ep" + edpI));
					if (!edpW)
						continue;
					edpW.SetText("Probe " + edpI + " focus");
					if (edpI == 5)
						edpW.SetTextColor(ARGB(255, 255, 255, 255));
				}
			}
			int epn = 0;
			if (a.Count() > 1)
				epn = a[1].ToInt();
			SetFocus(m_EditProbe.FindAnyWidget("ep" + epn));
			Log("editprobe focus ep" + epn);
			return;
		}
		if (c == "boxprobe")
		{
			// boxprobe CLASS,CLASS,… — model boxes and the lowest point rays find, on an object 300 m above the camera
			array<string> bnames = new array<string>();
			a[1].Split(",", bnames);
			vector bc = GetGame().GetCurrentCameraPosition();
			for (int bi = 0; bi < bnames.Count(); bi++)
				BoxProbe(bnames[bi], bc + Vector(bi * 40, 300 + bi * 25, 0));
			return;
		}
		if (c == "settleeach")
		{
			// settleeach [i,j,…] — every object of the first group alone through "Settle on ground", one per frame; listed ones skipped
			m_SettleSkip = new array<int>();
			if (a.Count() > 1)
			{
				array<string> sk = new array<string>();
				a[1].Split(",", sk);
				foreach (string sks : sk)
					m_SettleSkip.Insert(sks.ToInt());
			}
			m_SettleNext = 0;
			return;
		}
		if (c == "groundcheck")
		{
			// groundcheck — per object of the first group: gap of the box bottom above the terrain at the foot and the corners
			array<string> gk_gids = inst.GetChildren(FirstGroup(inst));
			for (int gk_gi = 0; gk_gi < gk_gids.Count(); gk_gi++)
			{
				DZS_PlaceObj gk_go = DZS_PlaceObj.FromStateQuick(inst, gk_gids[gk_gi]);
				vector gk_gf = gk_go.Foot();
				float gk_gt = GetGame().SurfaceY(gk_gf[0], gk_gf[2]);
				float gk_gmin = 1e9;
				float gk_gmax = -1e9;
				for (int gk_gc = 0; gk_gc < 4; gk_gc++)
				{
					float gk_gx = gk_go.m_BMin[0] + (gk_go.m_BMax[0] - gk_go.m_BMin[0]) * 0.1;
					float gk_gz = gk_go.m_BMin[2] + (gk_go.m_BMax[2] - gk_go.m_BMin[2]) * 0.1;
					if (gk_gc & 1)
						gk_gx = gk_go.m_BMax[0] - (gk_go.m_BMax[0] - gk_go.m_BMin[0]) * 0.1;
					if (gk_gc & 2)
						gk_gz = gk_go.m_BMax[2] - (gk_go.m_BMax[2] - gk_go.m_BMin[2]) * 0.1;
					vector gk_gw = gk_go.Local(Vector(gk_gx, gk_go.m_BMin[1], gk_gz));
					float gk_gs = GetGame().SurfaceY(gk_gw[0], gk_gw[2]);
					float gk_ggap = gk_gw[1] - gk_gs;
					if (gk_ggap < gk_gmin)
						gk_gmin = gk_ggap;
					if (gk_ggap > gk_gmax)
						gk_gmax = gk_ggap;
				}
				float gk_gcen = gk_gf[1] - gk_gt;
				Log("groundcheck " + gk_gi + " " + gk_go.m_Name + " foot gap " + gk_gcen + " corners " + gk_gmin + " .. " + gk_gmax + " box y " + gk_go.m_BMin[1]);
			}
			return;
		}
		if (c == "ray")
		{
			// ray X Y Z — what a depth ray from the camera to this point meets
			vector rt = Vector(a[1].ToFloat(), a[2].ToFloat(), a[3].ToFloat());
			vector rc = GetGame().GetCurrentCameraPosition();
			vector rp, rd;
			int rcomp;
			set<Object> rres = new set<Object>();
			bool rhit = DayZPhysics.RaycastRV(rc, rt, rp, rd, rcomp, rres, null, null, false, false, ObjIntersectView, 0);
			string rwhat = "";
			foreach (Object ro : rres)
			{
				if (ro)
					rwhat += ro.GetType() + " ";
			}
			DZS_SceneApi rscene = DZS_SceneApi.Get();
			foreach (Object rdo : rres)
			{
				if (!rdo)
					continue;
				string rpar = "none";
				if (rdo.GetParent())
					rpar = Object.Cast(rdo.GetParent()).GetType() + " " + Object.Cast(rdo.GetParent()).GetPosition();
				Log("ray object '" + rdo.GetType() + "' class " + rdo.ClassName() + " shape " + rdo.GetShapeName() + " pos " + rdo.GetPosition() + " parent " + rpar + " id '" + rscene.IdOf(rdo) + "'");
			}
			float rsy = GetGame().SurfaceY(rp[0], rp[2]);
			Log("ray hit " + rhit + " at " + rp + " objects " + rres.Count() + " [" + rwhat + "] terrain " + rsy + " free " + DZS_WorldLines.Free(rt));
			vector rdir = (rt - rc).Normalized();
			vector rfrom = rp + rdir * 0.3;
			for (int rn = 0; rn < 4 && rhit; rn++)
			{
				set<Object> rres2 = new set<Object>();
				rhit = DayZPhysics.RaycastRV(rfrom, rt, rp, rd, rcomp, rres2, null, null, false, false, ObjIntersectView, 0);
				float rsy2 = GetGame().SurfaceY(rp[0], rp[2]);
				Log("ray again from " + rfrom + " hit " + rhit + " at " + rp + " objects " + rres2.Count() + " terrain " + rsy2);
				rfrom = rp + rdir * 0.3;
			}
			vector wfrom = rc + rdir * 0.01;
			array<int> kinds = {ObjIntersectGeom, ObjIntersectFire, ObjIntersectIFire, ObjIntersectNone};
			foreach (int kind : kinds)
			{
				set<Object> rres3 = new set<Object>();
				bool kh = DayZPhysics.RaycastRV(wfrom, rt, rp, rd, rcomp, rres3, null, null, false, false, kind, 0);
				Log("ray kind " + kind + " hit " + kh + " at " + rp + " objects " + rres3.Count());
			}
			vector hp, hn;
			float hf;
			bool bh = DayZPhysics.RayCastBullet(wfrom, rt, PhxInteractionLayers.BUILDING | PhxInteractionLayers.TERRAIN, null, null, hp, hn, hf);
			Log("ray bullet hit " + bh + " at " + hp + " fraction " + hf);
			return;
		}
				if (c == "surf")
		{
			// surf X Z0 Z1 STEP — terrain height along a line (finding water of a wanted depth)
			float sx = a[1].ToFloat();
			string surfLine = "surf";
			for (float sz = a[2].ToFloat(); sz <= a[3].ToFloat(); sz += a[4].ToFloat())
			{
				float sy = GetGame().SurfaceY(sx, sz);
				surfLine += " " + sz + ":" + DZS_Num.Fixed(sy, 2);
			}
			Log(surfLine);
			return;
		}
		if (c == "alphaprobe")
		{
			AlphaProbe(a[1].ToFloat());
			return;
		}
		if (c == "bits")
		{
			bool fin;
			float bv = DZS_DzeReader.FromBits(a[1].ToInt(), fin);
			float rv;
			DZS_F32.FromText(a[2], rv);
			int m, e;
			DZS_F32.Decompose(bv, m, e);
			int m2, e2;
			DZS_F32.Decompose(rv, m2, e2);
			string bt = DZS_F32.ToText(bv);
			Log("bits " + a[1] + " -> " + bt + " m " + m + " e " + e + " | text " + a[2] + " m " + m2 + " e " + e2 + " same " + (bv == rv));
			return;
		}
		if (c == "dze")
		{
			DzeProbe(a[1]);
			return;
		}
		if (c == "binread")
		{
			BinProbe(a[1]);
			return;
		}
				if (c == "rest")
		{
			RestProbe(a[1], a[2], a[3]);
			return;
		}
				if (c == "lootprobe")
		{
			LootProbe(a[1], a[2]);
			return;
		}
		if (c == "reopen")
		{
			m_ReopenLeft = a[1].ToInt();
			m_ReopenWait = 0;
			return;
		}
		if (!inst)
		{
			Log("no instance");
			return;
		}
		if (c == "save")
		{
			pp0.StartSave(false, "");
			return;
		}
		if (c == "action")
		{
			Log("action " + a[1] + " triggered " + m_Kernel.GetUi().Trigger(a[1]));
			return;
		}
		if (c == "vanillamission")
		{
			// vanillamission MAP — fetch the whole vanilla mission of a map (a patch base)
			m_Vanilla = new DZS_LootVanilla();
			m_Vanilla.m_OnDone.Insert(VanillaDone);
			Log("vanillamission " + a[1] + " started " + m_Vanilla.StartMission(a[1]));
			return;
		}
		if (c == "lootedit")
		{
			// lootedit TYPE MIX — the loot editor's way of changing a type, with the test mix MIX of DZS_LootBlocksTest
			DZS_LootPlugin lpe = DZS_LootPlugin.Cast(m_Kernel.GetPlugin("loot"));
			DZS_LootEEntry le = lpe.Effective(inst, a[1]);
			if (!le)
			{
				Log("lootedit " + a[1] + ": no entry");
				return;
			}
			DZS_LootEEntry edited = le.Copy();
			DZS_LootBlocksTest.Edit(edited, a[2].ToInt());
			DZS_OpBuilder lb = inst.BeginOperation("loot:edit", 1);
			lpe.SetEntry(lb, inst, a[1], edited);
			DZS_SubmitResult lr = lb.Commit("Loot", "Loot");
			Log("lootedit " + a[1] + " mix " + a[2] + ": " + lr.Ok());
			return;
		}
		if (c == "v2")
		{
			// v2 BASE OUT — the open project as building blocks, applied to the mission folder BASE, written to OUT
			string v2Base = DZS_PatchPlugin.Path(a[1]);
			string v2Out = DZS_PatchPlugin.Path(a[2]);
			DZS_Json v2p = DZS_ProjectV2.Build(m_Kernel, inst, pp0.m_Open.m_Name, pp0.m_Open.m_MapKey);
			DZS_Files.MakeDirs(v2Out);
			DZS_TextWriter vw = new DZS_TextWriter();
			DZS_TextForm.Value(vw, v2p, 0);
			vw.Append("\n");
			DZS_Files.Write(v2Out + "\\project.v2.json", vw.Finish());
			string v2Err;
			DZS_Patcher vp = DZS_ProjectV2.Apply(v2p, v2Base, v2Out, v2Err);
			if (!vp)
			{
				Log("v2: " + v2Err);
				return;
			}
			vp.m_Report.Log();
			string v2Line = "v2: " + v2p.Get("blocks").Count() + " blocks, " + v2p.Get("changes").Count() + " changes, ";
			v2Line += vp.m_OutPaths.Count().ToString() + " files, applied " + vp.m_Report.Count("applied");
			v2Line += ", invalid " + vp.m_Report.Count("invalid");
			Log(v2Line);
			return;
		}
		if (c == "slicetest")
		{
			// slicetest — sliced text form against ExportState, once plain and once with a move of the first
			// object between two slices (the slices must still give the state from before the move)
			array<string> plain = inst.ExportState();
			DZS_StateSlicer s1 = inst.BeginSlices();
			while (!s1.Step(4)) {}
			inst.EndSlices(s1);
			Log("slicetest plain: same " + DZS_Files.Same(plain, s1.m_Chunks) + ", " + DZS_Files.Length(plain) + " bytes, " + s1.Count() + " elements");
			DZS_StateSlicer s2 = inst.BeginSlices();
			s2.Step(0.01);
			string firstObj = "";
			array<string> sgroups = inst.GetChildren(DZS_PROJECT_ID);
			foreach (string sg : sgroups)
			{
				array<string> sobjs = inst.GetChildren(sg);
				if (sobjs.Count() > 0)
					firstObj = sobjs[sobjs.Count() - 1];
			}
			if (firstObj != "")
			{
				string spos;
				inst.GetField(firstObj, DZS_Obj.POS, spos);
				vector sv = DZS_Obj.QuickVec(spos);
				DZS_OpBuilder sb = inst.BeginOperation("projtest:edit", 1);
				sb.SetField(firstObj, DZS_Obj.POS, DZS_Obj.Vec(sv[0] + 5, sv[1], sv[2]));
				sb.Commit("Verschieben", "Move");
			}
			int steps = 1;
			while (!s2.Step(0.5))
				steps++;
			inst.EndSlices(s2);
			Log("slicetest with a move of " + firstObj + " after " + s2.Written() + " written: same as before the move " + DZS_Files.Same(plain, s2.m_Chunks) + ", same as now " + DZS_Files.Same(inst.ExportState(), s2.m_Chunks) + ", " + steps + " steps");
			return;
		}
		if (c == "esttest")
		{
			// esttest N — an operation creating N objects (not submitted): estimate against the real text form
			int en = a[1].ToInt();
			DZS_Operation eop = new DZS_Operation("projtest:edit", 1);
			array<string> egs = inst.GetChildren(DZS_PROJECT_ID);
			string eg = egs[0];
			for (int ei = 0; ei < en; ei++)
			{
				DZS_ElementData ed = new DZS_ElementData();
				ed.m_Id = "est-" + ei;
				ed.m_Kind = DZS_KIND_OBJECT;
				ed.m_Parent = eg;
				ed.m_Fields.Set(DZS_Obj.NAME, "Barrel_Blue");
				ed.m_Fields.Set(DZS_Obj.POS, DZS_Obj.Vec(Math.RandomFloat(0, 15000), Math.RandomFloat(0, 400), Math.RandomFloat(0, 15000)));
				ed.m_Fields.Set(DZS_Obj.YPR, DZS_Obj.Vec(Math.RandomFloat(0, 360), 0, 0));
				ed.m_Fields.Set(DZS_Obj.SCALE, "1");
				if (ei % 7 == 0)
					ed.m_Fields.Set(DZS_Obj.CUSTOM, "Text with \"quotes\" and \\ backslash, " + ei);
				DZS_Change ec = DZS_Change.Create(ed);
				eop.AddChange(ec);
			}
			eop.Seal("I9-1", "a", "Test", DZS_Origin.LOCAL);
			int et0 = TickCount(0);
			int est = eop.EstimateText();
			float estMs = (TickCount(0) - et0) / 10000.0;
			int real = DZS_Files.Length(eop.ToText());
			Log("esttest " + en + ": estimate " + est + " real " + real + " over " + ((est - real) * 100.0 / real) + " % in " + estMs + " ms");
			return;
		}
		if (c == "add")
		{
			int n = 1;
			if (a.Count() > 1)
				n = a[1].ToInt();
			string cls = "Barrel_Blue";
			if (a.Count() > 2)
				cls = a[2];
			DZS_CameraPlugin cam = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
			vector base = cam.GetPosition();
			DZS_OpBuilder b = inst.BeginOperation("projtest:edit", 1);
			string g = DZS_ActiveGroup.ForNewObjects(inst, b);
			for (int i = 0; i < n; i++)
			{
				float x = base[0] + Math.RandomFloatInclusive(-40, 40);
				float z = base[2] + 60 + Math.RandomFloatInclusive(0, 60);
				float y = GetGame().SurfaceY(x, z);
				b.Create(DZS_KIND_OBJECT, g, DZS_END, Fields(cls, x, y, z, Math.RandomFloatInclusive(0, 360), 0, 0, 1));
			}
			DZS_SubmitResult r = b.Commit("Objekte anlegen", "Add objects");
			Log("add " + n + " " + cls + ": " + r.Ok());
		}
		else if (c == "place")
		{
			// place CLASS X Z [DY YAW PITCH ROLL SCALE [CUSTOM]] — Y is the terrain plus DY, or DY itself as "=Y"
			float qx = a[2].ToFloat();
			float qz = a[3].ToFloat();
			float qdy, qyaw, qpitch, qroll;
			float qbase = GetGame().SurfaceY(qx, qz);
			if (a.Count() > 4 && a[4].IndexOf("=") == 0)
			{
				qbase = 0;
				a[4] = a[4].Substring(1, a[4].Length() - 1);
			}
			float qscale = 1;
			if (a.Count() > 4) qdy = a[4].ToFloat();
			if (a.Count() > 5) qyaw = a[5].ToFloat();
			if (a.Count() > 6) qpitch = a[6].ToFloat();
			if (a.Count() > 7) qroll = a[7].ToFloat();
			if (a.Count() > 8) qscale = a[8].ToFloat();
			string qcustom = "";
			if (a.Count() > 9) qcustom = a[9];
			DZS_OpBuilder qb = inst.BeginOperation("projtest:edit", 1);
			qb.Create(DZS_KIND_OBJECT, FirstGroup(inst), DZS_END, Fields(a[1], qx, qbase + qdy, qz, qyaw, qpitch, qroll, qscale, qcustom));
			Log("place " + a[1] + ": " + qb.Commit("Objekt anlegen", "Add object").Ok());
		}
		else if (c == "sel")
		{
			// sel 0,2,3 — select objects by index (first is the lead)
			DZS_SelectionApi sapi = DZS_SelectionApi.Cast(m_Kernel.GetService("selection"));
			array<string> sidx = new array<string>();
			a[1].Split(",", sidx);
			array<string> sids = new array<string>();
			foreach (string si : sidx)
				sids.Insert(ObjectAt(inst, si.ToInt()));
			sapi.SetSelection(sids, sids[0]);
			Log("sel " + a[1]);
		}
		else if (c == "addmix")
		{
			DZS_CameraPlugin cm = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
			vector p = cm.GetPosition();
			float bx = p[0];
			float bz = p[2] + 80;
			DZS_OpBuilder mb = inst.BeginOperation("projtest:edit", 1);
			string mg = FirstGroup(inst);
			mb.Create(DZS_KIND_OBJECT, mg, DZS_END, Fields("Land_Wall_Gate_FenR", bx + 0.678, GetGame().SurfaceY(bx, bz) + 0.125, bz + 0.567, 37.5, 10.25, -5.125, 1.5));
			mb.Create(DZS_KIND_OBJECT, mg, DZS_END, Fields("DZ\\plants\\bush\\b_corylusavellana_2s.p3d", bx + 12.25, GetGame().SurfaceY(bx + 12, bz) + 0.5, bz - 10.75, 37.5, 5.73, 2.865, 1.2));
			mb.Create(DZS_KIND_OBJECT, mg, DZS_END, Fields("DZ\\rocks\\rock_apart1.p3d", bx - 14.125, GetGame().SurfaceY(bx - 14, bz) + 1, bz + 6.875, -90, 0, 0, 0.75));
			mb.Create(DZS_KIND_OBJECT, mg, DZS_END, Fields("StaticFlagPole", bx + 20, GetGame().SurfaceY(bx + 20, bz), bz + 20, 46.2, 0, 0, 1, "Flag_DayZ"));
			Log("addmix: " + mb.Commit("Mischung anlegen", "Add mix").Ok());
		}
		else if (c == "hide" || c == "unhide" || c == "lock" || c == "unlock")
		{
			string id = ObjectAt(inst, a[1].ToInt());
			DZS_OpBuilder hb = inst.BeginOperation("projtest:edit", 1);
			if (c == "hide" || c == "unhide")
				hb.SetHidden(id, c == "hide");
			else
				hb.SetLocked(id, c == "lock");
			Log(c + " " + a[1] + " (" + id + "): " + hb.Commit(c, c).Ok());
		}
		else if (c == "move")
		{
			string mid = ObjectAt(inst, a[1].ToInt());
			string pos;
			inst.GetField(mid, DZS_Obj.POS, pos);
			float px, py, pz;
			DZS_Obj.ParseVec(pos, px, py, pz);
			px += a[2].ToFloat();
			DZS_OpBuilder vb = inst.BeginOperation("projtest:edit", 1);
			vb.SetField(mid, DZS_Obj.POS, DZS_Obj.Vec(px, py, pz));
			Log("move " + a[1] + ": " + vb.Commit("Objekt verschieben", "Move object").Ok());
		}
		else if (c == "rename")
		{
			string rid = ObjectAt(inst, a[1].ToInt());
			DZS_OpBuilder rb = inst.BeginOperation("projtest:edit", 1);
			rb.SetField(rid, DZS_Obj.NAME, a[2]);
			Log("rename " + a[1] + ": " + rb.Commit("Klasse ändern", "Change class").Ok());
		}
		else if (c == "look")
		{
			string lid = ObjectAt(inst, a[1].ToInt());
			string lpos;
			inst.GetField(lid, DZS_Obj.POS, lpos);
			vector lp = DZS_Obj.QuickVec(lpos);
			DZS_CameraPlugin lc = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
			lc.SetPose(lp + Vector(0, 12, -22), 0, -28, false);
			Log("look at " + lpos);
		}
		else if (c == "poseg")
		{
			// poseg X Z HEIGHT YAW PITCH — camera HEIGHT m above the terrain
			DZS_CameraPlugin pg = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
			float pgx = a[1].ToFloat();
			float pgz = a[2].ToFloat();
			float pgy = GetGame().SurfaceY(pgx, pgz) + a[3].ToFloat();
			pg.SetPose(Vector(pgx, pgy, pgz), a[4].ToFloat(), a[5].ToFloat(), false);
			Log("poseg " + pg.GetPosition());
		}
		else if (c == "pose")
		{
			DZS_CameraPlugin pc = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
			pc.SetPose(Vector(a[1].ToFloat(), a[2].ToFloat(), a[3].ToFloat()), a[4].ToFloat(), a[5].ToFloat(), false);
			Log("pose " + pc.GetPosition() + " " + pc.GetYaw() + " " + pc.GetPitch());
		}
		else if (c == "stress")
		{
			// stress N CYCLES NAME [PAUSE] — per cycle: add N, delete all, undo, redo, undo, undo, redo, delete all
			m_StressN = a[1].ToInt();
			m_StressCycles = a[2].ToInt();
			m_StressName = a[3];
			if (a.Count() > 4)
				m_StressPause = a[4].ToFloat();
			m_StressCycle = 0;
			m_StressStep = 0;
			m_StressWait = 0;
			Log("stress start " + m_StressN + " x " + m_StressCycles + " " + m_StressName);
		}
		else if (c == "selfirst")
		{
			// selfirst N [LEAD] — select the first N objects of the active group
			DZS_SelectionApi fsel = DZS_SelectionApi.Cast(m_Kernel.GetService("selection"));
			array<string> fall = inst.GetChildren(DZS_ActiveGroup.Get(inst));
			array<string> fpick = new array<string>();
			int fn = a[1].ToInt();
			for (int fi2 = 0; fi2 < fn && fi2 < fall.Count(); fi2++)
				fpick.Insert(fall[fi2]);
			string flead = "";
			if (fpick.Count() > 0)
				flead = fpick[0];
			if (a.Count() > 2 && a[2].ToInt() < fpick.Count())
				flead = fpick[a[2].ToInt()];
			fsel.SetSelection(fpick, flead);
			Log("selfirst " + fpick.Count());
		}
		else if (c == "prof")
		{
			if (a[1] == "on")
			{
				DZS_Kernel.s_ProfTicks.Clear();
				DZS_Kernel.s_ProfFrames = 0;
				DZS_Kernel.s_Prof = true;
			}
			else
			{
				DZS_Kernel.s_Prof = false;
				int pf = Math.Max(1, DZS_Kernel.s_ProfFrames);
				string line = "prof " + pf + " frames, ms/frame:";
				for (int pi = 0; pi < DZS_Kernel.s_ProfTicks.Count(); pi++)
				{
					float ms = DZS_Kernel.s_ProfTicks.GetElement(pi) / 10000.0 / pf;
					if (ms >= 0.05)
						line += " " + DZS_Kernel.s_ProfTicks.GetKey(pi) + " " + ms;
				}
				Log(line);
			}
		}
		else if (c == "fps")
		{
			m_FpsLeft = a[1].ToFloat();
			m_FpsTime = 0;
			m_FpsFrames = 0;
			m_FpsWorst = 0;
			m_FpsRays = 0;
			m_FpsRaysMax = 0;
			m_FpsRaysZero = 0;
		}
		else if (c == "grid")
		{
			// grid N CLASS SPACING — N objects in a square grid 40 m in front of the camera, one operation
			int gn = a[1].ToInt();
			float sp = a[3].ToFloat();
			DZS_CameraPlugin gcam = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
			vector gbase = gcam.GetPosition();
			int side = Math.Ceil(Math.Sqrt(gn));
			DZS_OpBuilder gb = inst.BeginOperation("projtest:edit", 1);
			string gg = DZS_ActiveGroup.ForNewObjects(inst, gb);
			for (int gi = 0; gi < gn; gi++)
			{
				int gcol = gi % side;
				int grow = gi / side;
				int ghalf = side / 2;
				float gx = gbase[0] + (gcol - ghalf) * sp;
				float gz = gbase[2] + 40 + grow * sp;
				float gy = GetGame().SurfaceY(gx, gz);
				gb.Create(DZS_KIND_OBJECT, gg, DZS_END, Fields(a[2], gx, gy, gz, 0, 0, 0, 1));
			}
			DZS_SubmitResult gr = gb.Commit("Raster anlegen", "Add grid");
			Log("grid " + gn + " " + a[2] + ": " + gr.Ok());
		}
		else if (c == "stressstop")
			m_StressCycles = 0;
		else if (c == "textprobe")
		{
			// textprobe — the same words with each apostrophe form, in a dialog (title, body) and a message
			string apos = "A: server's | B: server’s | C: ‘x’ „y“ \"z\"";
			DZS_ConfirmDialog tp = new DZS_ConfirmDialog("Probe", "it's · it’s", apos);
			tp.AddFootButton("ok", "OK", "main");
			tp.Open();
			m_Kernel.GetScreen().ShowMessage("projtest.msg", "info", "Message: it's · it’s · don't", 30.0);
			Log("textprobe " + apos.Length() + " " + apos);
		}
		else if (c == "mouse")
		{
			// mouse — pointer as the game reports it, the screen size and the widget under it
			int pmx, pmy, psw, psh;
			GetMousePos(pmx, pmy);
			GetScreenSize(psw, psh);
			Widget pun = GetWidgetUnderCursor();
			string punName = "none";
			if (pun)
				punName = pun.GetName();
			Log("mouse " + pmx + " " + pmy + " screen " + psw + " " + psh + " under " + punName);
		}
		else if (c == "msghome")
		{
			m_Kernel.GetScreen().ShowMessage("projtest.msg", "info", "Meldung vor dem Hauptmenü", 2.0);
			m_Kernel.GetUi().Trigger("projects.home");
			Log("msghome");
		}
		else if (c == "undo")
			Log("undo " + inst.Undo());
		else if (c == "redo")
			Log("redo " + inst.Redo());
		else if (c == "seldump")
		{
			// every object of the first group: index, selected, position, point on the screen (screenshot pixels)
			DZS_SelectionApi dsel = DZS_SelectionApi.Cast(m_Kernel.GetService("selection"));
			array<string> dids = inst.GetChildren(FirstGroup(inst));
			int sw, sh;
			GetScreenSize(sw, sh);
			float k = 1920.0 / sw;
			for (int di = 0; di < dids.Count() && di < 40; di++)
			{
				string dpos;
				inst.GetField(dids[di], DZS_Obj.POS, dpos);
				vector dp = DZS_Obj.QuickVec(dpos);
				vector ds = GetGame().GetScreenPos(dp + "0 0.4 0");
				Log("seldump #" + di + " sel " + dsel.IsSelected(dids[di]) + " pos " + dpos + " screen " + Math.Round(ds[0] * k) + " " + Math.Round(ds[1] * k));
			}
			Log("seldump history " + inst.GetHistoryPosition() + "/" + inst.GetHistoryLength());
		}
		else if (c == "state")
			Log("state unsaved " + inst.GetUnsavedCount() + " mode '" + inst.GetSaveMode() + "' saving " + inst.IsSaving() + " failed " + inst.IsSaveFailed() + " elements " + inst.CountElements() + " history " + inst.GetHistoryPosition() + "/" + inst.GetHistoryLength());
		else if (c == "dump")
			Dump(inst);
		else if (c == "set")
		{
			if (a[2] == "true" || a[2] == "false")
				m_Kernel.GetSettings().Set(a[1], DZS_Json.Boolean(a[2] == "true"));
			Log("set " + a[1] + " = " + a[2]);
		}
		else if (c == "stable")
			Stable(inst);
		else if (c == "roundtrip")
			RoundTrip(inst, a[1].ToInt());
	}

	// One isolated part of opening a project per frame: read (file), parse (file + format), state (kernel only).
	protected void HeapTick()
	{
		int t0 = TickCount(0);
		int n = 0;
		string path = DZS_LocalStore.Path(m_HeapFolder, "objects.json");
		if (m_HeapMode == "read")
		{
			array<string> chunks = DZS_Files.Read(path);
			n = chunks.Count();
		}
		else if (m_HeapMode == "parse")
		{
			array<string> pchunks = DZS_Files.Read(path);
			DZS_Format fmt = m_Kernel.FormatForFile("objects.json");
			DZS_ReadResult rr = fmt.Read(pchunks);
			n = rr.m_Objects.Count();
		}
		else if (m_HeapMode == "json")
		{
			array<string> jchunks = DZS_Files.Read(path);
			DZS_SrcDoc doc = DZS_SrcJson.Parse(jchunks);
			if (doc.m_Root)
				n = 1;
		}
		else if (m_HeapMode == "rcopy" || m_HeapMode == "rfresh")
		{
			array<string> cc = new array<string>();
			FileHandle fh = OpenFile(path, FileMode.READ);
			while (fh)
			{
				string part;
				if (m_HeapMode == "rfresh")
					part = ReadOne(fh);
				else
				{
					string raw1;
					int got = ReadFile(fh, raw1, 1000);
					if (got > 0)
						part = raw1.Substring(0, raw1.Length());
					else
						part = "";
				}
				if (part == "")
					break;
				cc.Insert(part);
			}
			CloseFile(fh);
			DZS_SrcDoc cdoc = DZS_SrcJson.Parse(cc);
			if (cdoc.m_Root)
				n = cc.Count();
		}
		else if (m_HeapMode == "build" || m_HeapMode == "buildshow")
		{
			array<string> bchunks = DZS_Files.Read(path);
			DZS_Format bfmt = m_Kernel.FormatForFile("objects.json");
			DZS_ReadResult brr = bfmt.Read(bchunks);
			DZS_Instance binst = m_Kernel.NewLoadInstance();
			DZS_OpBuilder bb = binst.BeginOperation("core:load", 1);
			map<string, string> pf = new map<string, string>();
			bb.Create(DZS_KIND_PROJECT, "", "", pf, false, false, null, DZS_PROJECT_ID);
			map<string, string> gf = new map<string, string>();
			gf.Set(DZS_Obj.GROUP_NAME, "Objects");
			gf.Set(DZS_Obj.GROUP_FILE, "objects.json");
			gf.Set(DZS_Obj.GROUP_FORMAT, bfmt.GetId());
			string bgid = bb.Create(DZS_KIND_GROUP, DZS_PROJECT_ID, DZS_END, gf);
			for (int oi = 0; oi < brr.m_Objects.Count(); oi++)
			{
				DZS_ReadObject ro = brr.m_Objects[oi];
				array<ref DZS_Companion> oc = new array<ref DZS_Companion>();
				for (int ci = 0; ci < ro.m_Data.m_Companions.Count(); ci++)
					oc.Insert(ro.m_Data.m_Companions.GetElement(ci));
				bb.Create(DZS_KIND_OBJECT, bgid, DZS_END, ro.m_Data.m_Fields, false, false, oc);
			}
			string berr;
			binst.LoadBuilt(bb, berr);
			n = binst.CountElements();
			if (m_HeapMode == "buildshow")
			{
				DZS_Instance was = m_Kernel.GetShown();
				m_Kernel.AddLoaded(binst);
				m_Kernel.ShowInstance(binst);
				if (was)
					m_Kernel.CloseInstance(was);
			}
		}
		else if (m_HeapMode == "chain" || m_HeapMode == "local")
		{
			for (int ch = 0; ch < 1000; ch++)
			{
				DZS_Json wt = DZS_Json.Arr();
				string joined;
				if (m_HeapMode == "chain")
					joined = DZS_SpawnerFormat.Join(wt.ToChunks());
				else
				{
					array<string> wtc = wt.ToChunks();
					joined = DZS_SpawnerFormat.Join(wtc);
				}
				n += joined.Length();
			}
		}
		else if (m_HeapMode == "clone" || m_HeapMode == "touch")
		{
			for (int cl = 0; cl < 200; cl++)
			{
				DZS_Json list = DZS_Json.Arr();
				for (int ci2 = 0; ci2 < 12; ci2++)
				{
					DZS_Json one = DZS_Json.Obj();
					one.Set("provider", DZS_Json.Str("local"));
					one.Set("loc", DZS_Json.Str("Project " + ci2));
					one.Set("opened", DZS_Json.Str("2026-11-02 18:00:00"));
					list.Add(one);
				}
				if (m_HeapMode == "touch")
					m_Kernel.GetSettings().Set("projtest.nested", list);
				else
				{
					DZS_Json clonedList = list.Clone();
					n += clonedList.Count();
				}
			}
		}
		else if (m_HeapMode == "fs" || m_HeapMode == "fsc")
		{
			DZS_Instance finst = m_Kernel.GetShown();
			string fg = DZS_ActiveGroup.Get(finst);
			array<string> fids = finst.GetChildren(fg);
			array<ref DZS_PlaceObj> fobjs = new array<ref DZS_PlaceObj>();
			for (int fi = 0; fi < fids.Count(); fi++)
			{
				if (m_HeapMode == "fsc")
					fobjs.Insert(DZS_PlaceObj.FromState(finst, fids[fi]));
				else
				{
					DZS_PlaceObj fo = DZS_PlaceObj.FromState(finst, fids[fi]);
					fobjs.Insert(fo);
				}
			}
			n = fobjs.Count();
		}
		else if (m_HeapMode == "dsets" || m_HeapMode == "dsetsl" || m_HeapMode == "dpairs")
		{
			DZS_Instance dinst = m_Kernel.GetShown();
			string dg = DZS_ActiveGroup.Get(dinst);
			array<string> dids = dinst.GetChildren(dg);
			if (m_HeapMode == "dpairs")
			{
				array<ref DZS_PlaceObj> dobjs = new array<ref DZS_PlaceObj>();
				for (int di = 0; di < dids.Count(); di++)
				{
					DZS_PlaceObj dob = DZS_PlaceObj.FromState(dinst, dids[di]);
					dobjs.Insert(dob);
				}
				for (int dx = 0; dx < dobjs.Count(); dx++)
				{
					for (int dy = dx + 1; dy < dobjs.Count(); dy++)
					{
						DZS_PlaceObj dp = dobjs[dx];
						DZS_PlaceObj dq = dobjs[dy];
						vector dd = dp.m_Pos - dq.m_Pos;
						if (Math.AbsFloat(dd[0]) > 0.001 || Math.AbsFloat(dd[1]) > 0.001 || Math.AbsFloat(dd[2]) > 0.001)
							continue;
						n++;
					}
				}
			}
			else
			{
				map<int, ref array<string>> dsets = new map<int, ref array<string>>();
				for (int dk = 0; dk < dids.Count(); dk++)
				{
					if (!dsets.Contains(dk))
					{
						if (m_HeapMode == "dsets")
							dsets.Set(dk, new array<string>());
						else
						{
							array<string> dnew = new array<string>();
							dsets.Set(dk, dnew);
						}
					}
					dsets.Get(dk).Insert(dids[dk]);
				}
				n = dsets.Count();
			}
		}
		else if (m_HeapMode == "dph" || m_HeapMode == "dpnc")
		{
			DZS_Instance hinst = m_Kernel.GetShown();
			string hg = DZS_ActiveGroup.Get(hinst);
			array<string> hids = hinst.GetChildren(hg);
			array<ref DZS_PlaceObj> hobjs = new array<ref DZS_PlaceObj>();
			for (int hi = 0; hi < hids.Count(); hi++)
			{
				DZS_PlaceObj hob = DZS_PlaceObj.FromState(hinst, hids[hi]);
				hobjs.Insert(hob);
			}
			DZS_PlaceObj hp;
			DZS_PlaceObj hq;
			vector hd;
			for (int hx = 0; hx < hobjs.Count(); hx++)
			{
				for (int hy = hx + 1; hy < hobjs.Count(); hy++)
				{
					if (m_HeapMode == "dph")
					{
						hp = hobjs[hx];
						hq = hobjs[hy];
						hd = hp.m_Pos - hq.m_Pos;
						if (Math.AbsFloat(hd[0]) > 0.001 || Math.AbsFloat(hd[1]) > 0.001 || Math.AbsFloat(hd[2]) > 0.001)
							continue;
						n++;
					}
					else
					{
						DZS_PlaceObj np = hobjs[hx];
						DZS_PlaceObj nq = hobjs[hy];
						vector nd = np.m_Pos - nq.m_Pos;
						if (Math.AbsFloat(nd[0]) <= 0.001 && Math.AbsFloat(nd[1]) <= 0.001 && Math.AbsFloat(nd[2]) <= 0.001)
							n++;
					}
				}
			}
		}
		else if (m_HeapMode == "c1" || m_HeapMode == "c2" || m_HeapMode == "c3" || m_HeapMode == "c4")
		{
			array<float> cv = new array<float>();
			for (int cz = 0; cz < 1000; cz++)
				cv.Insert(cz * 0.5);
			int hits = 0;
			if (m_HeapMode == "c1")
			{
				for (int c1 = 0; c1 < 600000; c1++)
				{
					float v1 = cv[c1 % 1000];
					if (v1 > 1)
						continue;
					hits++;
				}
			}
			else if (m_HeapMode == "c2")
			{
				for (int c2a = 0; c2a < 600; c2a++)
				{
					for (int c2b = 0; c2b < 1000; c2b++)
					{
						float v2 = cv[c2b];
						if (v2 > 1)
							continue;
						hits++;
					}
				}
			}
			else if (m_HeapMode == "c3")
			{
				for (int c3a = 0; c3a < 600; c3a++)
				{
					foreach (float v3 : cv)
					{
						if (v3 > 1)
							continue;
						hits++;
					}
				}
			}
			else
			{
				int c4 = 0;
				while (c4 < 600000)
				{
					float v4 = cv[c4 % 1000];
					c4++;
					if (v4 > 1)
						continue;
					hits++;
				}
			}
			n = hits;
		}
		else if (m_HeapMode == "jsonsyn")
		{
			if (!m_SynChunks)
			{
				array<string> raw = DZS_Files.Read(path);
				string all = DZS_Files.Join(raw);
				m_SynChunks = new array<string>();
				int len = all.Length();
				for (int at = 0; at < len; at += 1000)
				{
					int take = len - at;
					if (take > 1000)
						take = 1000;
					string piece = all.Substring(at, take);
					m_SynChunks.Insert(piece);
				}
			}
			array<string> sc = new array<string>();
			sc.Copy(m_SynChunks);
			DZS_SrcDoc sdoc = DZS_SrcJson.Parse(sc);
			if (sdoc.m_Root)
				n = sc.Count();
		}
		else if (m_HeapMode == "state")
		{
			DZS_Instance inst = m_Kernel.GetShown();
			array<string> st = inst.ExportState();
			string error;
			DZS_Instance copy = m_Kernel.OpenFromState(st, error);
			n = copy.CountElements();
			m_Kernel.CloseInstance(copy);
		}
		m_HeapDone++;
		m_HeapLeft--;
		float ms = (TickCount(0) - t0) / 10000.0;
		Log("heap " + m_HeapMode + " " + m_HeapDone + " n " + n + " " + ms + " ms");
	}

	protected void FpsTick(float dt)
	{
		m_FpsTime += dt;
		m_FpsFrames++;
		m_FpsWorst = Math.Max(m_FpsWorst, dt);
		int rays = DZS_WorldLines.RaysLastFrame();
		m_FpsRays += rays;
		m_FpsRaysMax = Math.Max(m_FpsRaysMax, rays);
		if (rays == 0)
			m_FpsRaysZero++;
		m_FpsLeft -= dt;
		if (m_FpsLeft > 0)
			return;
		float avg = m_FpsFrames / Math.Max(0.001, m_FpsTime);
		float minFps = 1 / Math.Max(0.001, m_FpsWorst);
		Log("fps " + m_FpsTime + " s: frames " + m_FpsFrames + " avg " + avg + " min " + minFps + " rays/frame avg " + (m_FpsRays / Math.Max(1, m_FpsFrames)) + " max " + m_FpsRaysMax + " frames without rays " + m_FpsRaysZero);
	}

	protected string ReadOne(FileHandle fh)
	{
		string fresh;
		int got = ReadFile(fh, fresh, 1000);
		if (got <= 0)
			return "";
		return fresh;
	}

	protected ref array<ref DZS_RestProbeCall> m_RestCalls = new array<ref DZS_RestProbeCall>();

	// RestApi measuring (answers to section 9 (1)): rest get FILE x | rest file FILE PATHKIND (profile, abs, rel)
	protected void RestProbe(string how, string file, string kind)
	{
		RestApi api = GetRestApi();
		if (!api)
			api = CreateRestApi();
		api.SetOption(ERestOption.ERESTOPTION_READOPERATION, 60);
		RestContext ctx = api.GetRestContext("https://raw.githubusercontent.com/");
		string req = "BohemiaInteractive/DayZ-Central-Economy/master/dayzOffline.chernarusplus/" + file;
		DZS_RestProbeCall cb = new DZS_RestProbeCall();
		cb.m_Start = GetGame().GetTickTime();
		cb.m_What = how + " " + file + " " + kind;
		m_RestCalls.Insert(cb);
		if (how == "get")
		{
			int r = ctx.GET(cb, req);
			Log("rest get started " + r);
			return;
		}
		string target = "$profile:dzstudio\\rest-" + file;
		if (kind == "abs")
		{
			string prof;
			GetCLIParam("profiles", prof);
			target = prof + "\\dzstudio\\rest-" + file;
		}
		else if (kind == "rel")
			target = "rest-" + file;
		cb.m_Target = target;
		int rf = ctx.FILE(cb, req, target);
		Log("rest file started " + rf + " -> " + target);
	}

	protected string Hex2(int b)
	{
		string d = "0123456789abcdef";
		int hi = (b >> 4) & 15;
		int lo = b & 15;
		return d.Substring(hi, 1) + d.Substring(lo, 1);
	}

	// See-through check: objects around the point under the screen centre;
	// those with hidden selections get a texture with 40 % alpha on each.
	protected void AlphaProbe(float radius)
	{
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetCurrentCameraDirection();
		vector hit, hn;
		int comp;
		set<Object> res = new set<Object>();
		DayZPhysics.RaycastRV(from, from + dir * 500, hit, hn, comp, res, null, null, false, false, ObjIntersectView, 0);
		array<Object> objs = new array<Object>();
		GetGame().GetObjectsAtPosition3D(hit, radius, objs, null);
		int total = 0;
		int ai = 0;
		int withSel = 0;
		foreach (Object o : objs)
		{
			total++;
			EntityAI e = EntityAI.Cast(o);
			if (!e)
				continue;
			ai++;
			array<string> hs = new array<string>();
			GetGame().ConfigGetTextArray("CfgVehicles " + o.GetType() + " hiddenSelections", hs);
			if (hs.Count() == 0)
				continue;
			withSel++;
			for (int i = 0; i < hs.Count(); i++)
				e.SetObjectTexture(i, "#(argb,8,8,3)color(1,1,1,0.4,ca)");
			Log("alphaprobe " + o.GetType() + " selections " + hs.Count());
		}
		Log("alphaprobe at " + hit + ": " + total + " objects, " + ai + " EntityAI, " + withSel + " with hidden selections");
	}

	// Reads $saves:dze-test\FILE like the import does and logs every value.
	protected void DzeProbe(string file)
	{
		string path = "$saves:dze-test\\" + file;
		float t0 = GetGame().GetTickTime();
		DZS_DzeImport im = DZS_DzeImport.From(path, file);
		float ms = (GetGame().GetTickTime() - t0) * 1000;
		if (im.m_Data.m_Error != "")
		{
			Log("dze " + file + ": not imported (" + im.m_Data.m_Error + ") " + im.ErrorText());
			return;
		}
		DZS_Issues issues = im.m_Read.m_Issues;
		Log("dze " + file + ": map " + im.m_Data.m_Map + " -> " + im.m_MapKey + ", camera " + im.m_Data.m_Cam.ToString() + " inside " + im.CameraInside() + ", " + im.m_Read.m_Objects.Count() + " object(s) in " + ms + " ms, size " + im.m_Data.m_Size);
		int shown = Math.Min(im.m_Read.m_Objects.Count(), 10);
		for (int i = 0; i < shown; i++)
		{
			DZS_ReadObject ro = im.m_Read.m_Objects[i];
			string nm = ro.m_Data.m_Fields.Get(DZS_Obj.NAME);
			string ps = ro.m_Data.m_Fields.Get(DZS_Obj.POS);
			string yp = ro.m_Data.m_Fields.Get(DZS_Obj.YPR);
			string sc = ro.m_Data.m_Fields.Get(DZS_Obj.SCALE);
			bool hid = im.m_Hidden[i];
			bool loc = im.m_Locked[i];
			Log("dze   #" + (i + 1) + " " + nm + " pos " + ps + " ypr " + yp + " scale " + sc + " hidden " + hid + " locked " + loc);
		}
		foreach (DZS_Issue is : issues.m_Items)
			Log("dze   issue " + is.m_Severity + ": " + is.m_What + " | " + is.m_De);
	}

	// Binary read check for the .dze import.
	protected void BinProbe(string path)
	{
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (!fh)
		{
			Log("binread cannot open " + path);
			return;
		}
		int buf[64];
		int total = 0;
		string hex = "";
		while (true)
		{
			int n = ReadFile(fh, buf, 256);
			if (n <= 0)
				break;
			for (int i = 0; i < n; i++)
			{
				int w = buf[i / 4];
				int b = (w >> ((i % 4) * 8)) & 255;
				if (total + i < 48 || (total + i >= 237))
					hex += Hex2(b) + " ";
			}
			total += n;
		}
		CloseFile(fh);
		Log("binread static " + total + " bytes: " + hex);
		array<int> dyn = new array<int>();
		dyn.Resize(64);
		fh = OpenFile(path, FileMode.READ);
		int nd = ReadFile(fh, dyn, 256);
		CloseFile(fh);
		Log("binread dyn " + nd + " first " + dyn[0] + " " + dyn[1]);
		FileSerializer fs = new FileSerializer();
		if (fs.Open(path, FileMode.READ))
		{
			string tag;
			int ver;
			string mapName;
			bool r1 = fs.Read(tag);
			bool r2 = fs.Read(ver);
			bool r3 = fs.Read(mapName);
			Log("binread serializer " + r1 + " " + tag + " " + r2 + " " + ver + " " + r3 + " " + mapName);
			fs.Close();
		}
	}

	// Builds a part of the loot data and frees it again, to find what crashes on freeing.
	protected void LootProbe(string mode, string arg)
	{
		string folder = arg;
		string protoPath = DZS_LocalStore.Path(folder, DZS_LootFiles.PROTO);
		Log("lootprobe " + mode + " build");
		if (mode == "files")
		{
			DZS_LootFiles f = DZS_LootFiles.Read(folder);
			Log("lootprobe built " + f.m_Index.Count());
			f = null;
		}
		else if (mode == "files5" || mode == "mask")
		{
			if (mode == "mask")
			{
				array<string> mp = new array<string>();
				arg.Split(",", mp);
				folder = mp[0];
				DZS_LootFiles.s_Skip = mp[1].ToInt();
			}
			for (int r5 = 0; r5 < 5; r5++)
			{
				DZS_LootFiles f5 = DZS_LootFiles.Read(folder);
				Log("lootprobe read " + r5 + ": " + f5.m_Index.Count() + " entries, " + f5.m_Rows.Count() + " rows");
				f5 = null;
				Log("lootprobe freed " + r5);
			}
		}
		else if (mode == "sum" || mode == "lim" || mode == "rows")
		{
			array<string> s1 = DZS_Files.Read(protoPath);
			if (mode == "sum")
				Log("lootprobe sum " + DZS_Files.Checksum(s1));
			if (mode == "lim")
			{
				array<string> l1 = DZS_Files.Read(DZS_LocalStore.Path(folder, DZS_LootFiles.LIMITS));
				DZS_XmlDoc ld = DZS_Xml.Parse(l1);
				Log("lootprobe limits " + ld.m_Root.m_Children.Count());
				ld = null;
			}
			DZS_XmlDoc sd = DZS_Xml.Parse(s1);
			array<ref DZS_LootRow> rr = new array<ref DZS_LootRow>();
			if (mode == "rows")
			{
				foreach (DZS_XmlNode sn : sd.m_Root.m_Children)
				{
					DZS_LootRow row = new DZS_LootRow();
					row.m_Node = sn;
					rr.Insert(row);
				}
			}
			Log("lootprobe built " + sd.m_Root.m_Children.Count());
			sd = null;
			Log("lootprobe doc freed");
			rr = null;
		}
		else if (mode == "both")
		{
			array<string> b1 = DZS_Files.Read(protoPath);
			array<string> b2 = DZS_Files.Read(DZS_LocalStore.Path(folder, DZS_LootFiles.POS));
			DZS_XmlDoc bd1 = DZS_Xml.Parse(b1);
			DZS_XmlDoc bd2 = DZS_Xml.Parse(b2);
			Log("lootprobe built both");
			bd1 = null;
			Log("lootprobe proto doc freed");
			bd2 = null;
		}
		else if (mode == "chunks")
		{
			array<string> ch = DZS_Files.Read(protoPath);
			DZS_XmlDoc d0 = new DZS_XmlDoc();
			d0.SetSource(ch);
			Log("lootprobe built " + d0.Length());
			d0 = null;
			ch = null;
		}
		else if (mode == "proto" || mode == "pos")
		{
			string path = protoPath;
			if (mode == "pos")
				path = DZS_LocalStore.Path(folder, DZS_LootFiles.POS);
			array<string> ch2 = DZS_Files.Read(path);
			DZS_XmlDoc d = DZS_Xml.Parse(ch2);
			Log("lootprobe built " + d.m_Root.m_Children.Count());
			d = null;
		}
		else if (mode == "objs")
		{
			int n = arg.ToInt();
			array<ref DZS_XmlNode> list = new array<ref DZS_XmlNode>();
			for (int i = 0; i < n; i++)
			{
				DZS_XmlNode nd = new DZS_XmlNode();
				DZS_XmlAttr at = new DZS_XmlAttr();
				at.m_Raw = "1.000000 2.000000 3.000000";
				nd.m_Attrs.Insert(at);
				list.Insert(nd);
			}
			Log("lootprobe built " + list.Count());
			list = null;
		}
		else if (mode == "deep")
		{
			int dn = arg.ToInt();
			DZS_XmlNode top = new DZS_XmlNode();
			for (int k = 0; k < dn; k++)
			{
				DZS_XmlNode kid = new DZS_XmlNode();
				top.m_Children.Insert(kid);
				for (int j = 0; j < 25; j++)
				{
					DZS_XmlNode leaf = new DZS_XmlNode();
					kid.m_Children.Insert(leaf);
				}
			}
			Log("lootprobe built " + dn * 26);
			top = null;
		}
		Log("lootprobe " + mode + " freed");
	}

	protected void ReopenTick(float dt)
	{
		m_ReopenWait -= dt;
		if (m_ReopenWait > 0)
			return;
		m_ReopenWait = 4;
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		string folder = m_LastFolder;
		if (pp.m_Open)
			folder = pp.m_Open.m_Folder;
		int t0 = TickCount(0);
		pp.CloseProject(true);
		pp.LoadProject(folder);
		float ms = (TickCount(0) - t0) / 10000.0;
		m_ReopenDone++;
		m_ReopenLeft--;
		DZS_Instance inst = m_Kernel.GetShown();
		int n = 0;
		if (inst)
			n = inst.CountElements();
		Log("reopen " + m_ReopenDone + " elements " + n + " " + ms + " ms");
	}

	protected void StressTick(float dt)
	{
		m_StressWait -= dt;
		if (m_StressWait > 0)
			return;
		m_StressWait = m_StressPause;
		DZS_Instance inst = m_Kernel.GetShown();
		if (!inst)
			return;
		int t0 = TickCount(0);
		string what;
		bool ok;
		if (m_StressStep == 0)
		{
			what = "add";
			DZS_CameraPlugin cam = DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
			vector base = cam.GetPosition();
			DZS_OpBuilder b = inst.BeginOperation("projtest:edit", 1);
			string g = DZS_ActiveGroup.ForNewObjects(inst, b);
			for (int i = 0; i < m_StressN; i++)
			{
				float x = base[0] + Math.RandomFloatInclusive(-60, 60);
				float z = base[2] + 40 + Math.RandomFloatInclusive(0, 100);
				float y = GetGame().SurfaceY(x, z);
				b.Create(DZS_KIND_OBJECT, g, DZS_END, Fields(m_StressName, x, y, z, Math.RandomFloatInclusive(0, 360), 0, 0, 1));
			}
			DZS_SubmitResult r = b.Commit("Objekte anlegen", "Add objects");
			ok = r.Ok();
		}
		else if (m_StressStep == 1 || m_StressStep == 7)
		{
			what = "delete";
			DZS_OpBuilder d = inst.BeginOperation("projtest:edit", 1);
			string dg = DZS_ActiveGroup.Get(inst);
			array<string> ids = inst.GetChildren(dg);
			for (int k = 0; k < ids.Count(); k++)
				d.Delete(ids[k]);
			DZS_SubmitResult dr = d.Commit("Objekte löschen", "Delete objects");
			ok = dr.Ok();
		}
		else if (m_StressStep == 2 || m_StressStep == 4 || m_StressStep == 5)
		{
			what = "undo";
			ok = inst.Undo();
		}
		else
		{
			what = "redo";
			ok = inst.Redo();
		}
		float ms = (TickCount(0) - t0) / 10000.0;
		Log("stress " + m_StressCycle + "." + m_StressStep + " " + what + " " + ok + " elements " + inst.CountElements() + " " + ms + " ms");
		m_StressStep++;
		if (m_StressStep > 7)
		{
			m_StressStep = 0;
			m_StressCycle++;
			if (m_StressCycle >= m_StressCycles)
			{
				m_StressCycles = 0;
				Log("stress done");
			}
		}
	}

	// The project as building blocks, built again without change, must equal the blocks and changes of its project file
	// (the extensions left out: the editor part carries the camera).
	protected void Stable(DZS_Instance inst)
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		array<string> disk = DZS_Files.Read(DZS_LocalStore.Path(pp.m_Open.m_Folder, DZS_ProjectFile.NAME));
		DZS_Json root = DZS_WorkFolder.RootOf(disk);
		if (!root)
		{
			Log("stable: project file not readable");
			return;
		}
		root.Remove("extensions");
		DZS_Json built = DZS_ProjectV2.Build(m_Kernel, inst, pp.m_Open.m_Name, pp.m_Open.m_MapKey);
		array<string> a = DZS_ProjectFile.Text(root);
		array<string> b = DZS_ProjectFile.Text(built);
		string onDisk = DZS_Files.Join(a);
		string again = DZS_Files.Join(b);
		Log("stable " + DZS_ProjectFile.NAME + ": " + (again == onDisk) + " (" + again.Length() + " / " + onDisk.Length() + " bytes, " + built.Get("blocks").Count() + " blocks)");
	}

	protected void Dump(DZS_Instance inst)
	{
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			string gname, file;
			inst.GetField(g, DZS_Obj.GROUP_NAME, gname);
			inst.GetField(g, DZS_Obj.GROUP_FILE, file);
			array<string> ids = inst.GetChildren(g);
			Log("group '" + gname + "' " + file + " hidden " + inst.IsHidden(g) + " locked " + inst.IsLocked(g) + " objects " + ids.Count());
			for (int i = 0; i < ids.Count() && i < 30; i++)
			{
				string n, p, y, s;
				inst.GetField(ids[i], DZS_Obj.NAME, n);
				inst.GetField(ids[i], DZS_Obj.POS, p);
				inst.GetField(ids[i], DZS_Obj.YPR, y);
				inst.GetField(ids[i], DZS_Obj.SCALE, s);
				Log("  #" + i + " " + n + " pos " + p + " ypr " + y + " scale " + s + " hidden " + inst.IsHidden(ids[i]) + " locked " + inst.IsLocked(ids[i]));
				int rv;
				string rd;
				if (inst.GetCompanion(ids[i], DZS_SpawnerFormat.READ_TYPE, rv, rd))
					Log("    read " + rd);
			}
		}
	}

	// Bit-exact round trip: n class objects with random values written by the
	// format and read back must give the same float32 bits (same shortest text), names and order.
	protected void RoundTrip(DZS_Instance inst, int n)
	{
		DZS_OpBuilder b = inst.BeginOperation("projtest:edit", 1);
		string g = FirstGroup(inst);
		array<string> keys = {"Land_Wall_Gate_FenR", "Barrel_Blue", "Land_Shed_M1", "StaticFlagPole"};
		for (int i = 0; i < n; i++)
		{
			float x = Math.RandomFloatInclusive(0, 15360);
			float z = Math.RandomFloatInclusive(0, 15360);
			float y = Math.RandomFloatInclusive(0, 600);
			b.Create(DZS_KIND_OBJECT, g, DZS_END, Fields(keys[i % 4], x, y, z, Math.RandomFloatInclusive(-720, 720), Math.RandomFloatInclusive(-90, 90), Math.RandomFloatInclusive(-180, 180), Math.RandomFloatInclusive(0.5, 3), "rt" + i));
		}
		b.Commit("Rundlauf-Objekte", "Round trip objects");
		DZS_Format f = m_Kernel.GetFormat("spawner");
		float t0 = GetGame().GetTickTime();
		DZS_WriteResult w = f.Write(inst, g);
		float t1 = GetGame().GetTickTime();
		DZS_ReadResult r = f.Read(w.m_Chunks);
		float t2 = GetGame().GetTickTime();
		array<string> ids = inst.GetChildren(g);
		int bad = 0;
		if (r.m_Objects.Count() != ids.Count())
			bad++;
		array<string> fk = {DZS_Obj.NAME, DZS_Obj.POS, DZS_Obj.YPR, DZS_Obj.SCALE, DZS_Obj.PERSIST, DZS_Obj.CUSTOM};
		for (int k = 0; k < ids.Count() && k < r.m_Objects.Count(); k++)
		{
			foreach (string key : fk)
			{
				string a1;
				inst.GetField(ids[k], key, a1);
				string a2 = r.m_Objects[k].m_Data.m_Fields.Get(key);
				if (a1 != a2)
				{
					bad++;
					if (bad < 5)
						Log("roundtrip diff #" + k + " " + key + ": '" + a1 + "' vs '" + a2 + "'");
				}
			}
		}
		DZS_WriteResult w2 = f.Write(inst, g);
		string first = DZS_Files.Join(w.m_Chunks);
		string second = DZS_Files.Join(w2.m_Chunks);
		bool stable = first == second;
		Log("roundtrip " + ids.Count() + " objects: " + bad + " differences, stable " + stable + ", bytes " + DZS_Files.Length(w.m_Chunks) + ", write " + (t1 - t0) + " s, read " + (t2 - t1) + " s");
	}

	protected void F32Checks()
	{
		array<float> vals = {1234.56789, 46.2, 1.0, 0.33333333, 12345.678, 10.25, -1.5707964};
		array<string> want = {"1234.5679", "46.2", "1.0", "0.33333334", "12345.678", "10.25", "-1.5707964"};
		int bad = 0;
		for (int i = 0; i < vals.Count(); i++)
		{
			string t = DZS_F32.ToText(vals[i]);
			Log("f32 " + want[i] + " -> " + t);
			if (t != want[i])
				bad++;
		}
		float third = 1.0 / 3.0;
		Log("f32 1/3 -> " + DZS_F32.ToText(third));
		float tiny = DZS_F32.Compose(1, -149);
		string tt = DZS_F32.ToText(tiny);
		Log("f32 2^-149 -> " + tt + " (" + tt.Length() + " chars)");
		Log("f32 max -> " + DZS_F32.ToText(FLT_MAX));
		float back;
		DZS_F32.FromText("12345.6780", back);
		Log("f32 read 12345.6780 -> " + DZS_F32.ToText(back));
		bool inf = DZS_F32.FromText("1e39", back);
		Log("f32 read 1e39 finite " + inf);
		int n = 100000;
		int fails = 0;
		int withE = 0;
		float t0 = GetGame().GetTickTime();
		for (int k = 0; k < n; k++)
		{
			float m = Math.RandomFloatInclusive(1, 2);
			int e = Math.RandomIntInclusive(-140, 127);
			float v = m;
			if (e > 0)
			{
				for (int q = 0; q < e; q++)
					v *= 2;
			}
			else
			{
				for (int q2 = 0; q2 < -e; q2++)
					v *= 0.5;
			}
			if (Math.RandomInt(0, 2) == 1)
				v = -v;
			string s = DZS_F32.ToText(v);
			if (s.Contains("e") || s.Contains("E"))
				withE++;
			float r;
			if (!DZS_F32.FromText(s, r) || r != v)
			{
				fails++;
				if (fails < 5)
					Log("f32 roundtrip fail " + s);
			}
		}
		Log("f32 random " + n + ": " + fails + " fail(s), " + withE + " with exponent, examples bad " + bad + ", " + (GetGame().GetTickTime() - t0) + " s");
	}
}

class DZS_RestProbeCall : RestCallback
{
	float m_Start;
	string m_What;
	string m_Target;

	override void OnError(int errorCode)
	{
		DZS_Log.Info("projtest rest " + m_What + " error " + errorCode);
	}

	override void OnTimeout()
	{
		DZS_Log.Info("projtest rest " + m_What + " timeout");
	}

	override void OnSuccess(string data, int dataSize)
	{
		float t = GetGame().GetTickTime() - m_Start;
		string head = data.Substring(0, Math.Min(40, data.Length()));
		DZS_Log.Info("projtest rest " + m_What + " ok size " + dataSize + " length " + data.Length() + " after " + t + " s head " + head);
	}

	override void OnFileCreated(string fileName, int dataSize)
	{
		float t = GetGame().GetTickTime() - m_Start;
		int len = 0;
		array<string> back = DZS_Files.Read(m_Target);
		if (back)
			len = DZS_Files.Length(back);
		DZS_Log.Info("projtest rest " + m_What + " file " + fileName + " size " + dataSize + " read back " + len + " after " + t + " s");
	}
}


// Test plugin for the camera (start with -dzsCamTest). It is a second plugin that only uses the public
// interface: it sets the camera, listens to "camera.changed", and runs commands from
// $profile:dzstudio\camtest.txt so measurements can be taken in the game from outside.
[DZS_RegisterPlugin(DZS_CamTest)]
class DZS_CamTest : DZS_Plugin
{
	static const string CMD_FILE = "$profile:dzstudio\\camtest.txt";

	protected bool m_On;
	protected float m_Poll;
	protected int m_Changed;
	protected float m_TraceLeft;
	protected float m_TraceT;
	protected float m_TargetFps;
	protected int m_Burn = 1000;
	protected float m_FpsAcc;
	protected int m_FpsFrames;
	protected ref array<Object> m_Spawned;
	protected ref DZS_CamTestMarks m_MarkOwner;

	override string GetName() { return "camtest"; }

	override void OnRegister()
	{
		string v;
		m_On = GetCLIParam("dzsCamTest", v);
		m_Spawned = new array<Object>();
		if (m_On)
			DZS_Log.Info("camtest on, commands from " + CMD_FILE);
	}

	protected DZS_CameraPlugin Cam()
	{
		return DZS_CameraPlugin.Cast(m_Kernel.GetPlugin("camera"));
	}

	override void OnPluginEvent(string topic, Managed payload)
	{
		if (!m_On || topic != "camera.changed")
			return;
		m_Changed++;
		DZS_CameraState s = DZS_CameraState.Cast(payload);
		if (m_Changed == 1 && s)
			DZS_Log.Info("camtest: first camera.changed " + s.m_Pos.ToString());
	}

	override void OnUpdate(float dt)
	{
		if (!m_On)
			return;
		m_FpsAcc += dt;
		m_FpsFrames++;
		if (m_TargetFps > 0)
			Burn(dt);
		m_Poll -= dt;
		if (m_Poll <= 0)
		{
			m_Poll = 0.25;
			if (FileExist(CMD_FILE))
				RunFile();
		}
		if (m_TraceLeft > 0)
		{
			m_TraceT += dt;
			m_TraceLeft -= dt;
			Log("trace t=" + m_TraceT.ToString() + " dt=" + dt.ToString());
		}
	}

	// Slows the frame down to the target rate by spinning; the spin count follows the measured frame time.
	protected void Burn(float dt)
	{
		float want = 1.0 / m_TargetFps;
		if (dt < want * 0.97)
			m_Burn = Math.Round(m_Burn * 1.1) + 100;
		else if (dt > want * 1.03)
			m_Burn = Math.Max(0, Math.Round(m_Burn * 0.92));
		float x = 0;
		for (int i = 0; i < m_Burn; i++)
			x += Math.Sin(i);
		if (x == 12345.678)
			DZS_Log.Info("never");
	}

	protected void Log(string what)
	{
		DZS_CameraPlugin cam = Cam();
		vector p = cam.GetPosition();
		float ground = GetGame().SurfaceY(p[0], p[2]);
		string t = "camtest " + what + " pos " + p[0].ToString() + " " + p[1].ToString() + " " + p[2].ToString();
		t += " yaw " + cam.GetYaw().ToString() + " pitch " + cam.GetPitch().ToString();
		float agl = p[1] - ground;
		t += " agl " + agl.ToString() + " stage " + cam.GetStage().ToString();
		t += " anarchy " + DZS_Str.Bool(cam.IsAnarchy()) + " changed " + m_Changed.ToString();
		DZS_Log.Info(t);
	}

	protected void RunFile()
	{
		array<string> lines = DZS_Settings.ReadLines(CMD_FILE);
		DeleteFile(CMD_FILE);
		if (!lines)
			return;
		foreach (string line : lines)
		{
			line.Replace("\r", "");
			line.Replace("\n", "");
			line.TrimInPlace();
			if (line.Length() > 0)
				Run(line);
		}
	}

	protected void Run(string line)
	{
		array<string> a = new array<string>();
		line.Split(" ", a);
		string c = a[0];
		DZS_CameraPlugin cam = Cam();
		DZS_Log.Info("camtest cmd: " + line);
		if (c == "log")
			Log("log");
		else if (c == "char")
		{
			DZS_CameraChar ch = cam.GetCharacter();
			string where = "none";
			bool chIn = false;
			bool chInv = false;
			string health = "none";
			if (ch)
			{
				chIn = ch.IsIn();
				chInv = ch.IsInvulnerable();
				health = ch.HealthText();
				Man man = ch.GetMan();
				if (man)
				{
					vector chOri = man.GetOrientation();
					vector chPos = man.GetPosition();
					where = chPos.ToString() + " yaw " + chOri[0];
				}
			}
			vector cpos = cam.GetPosition();
			DZS_Log.Info("camtest char: in " + chIn + " at " + where + " " + health + " invulnerable " + chInv + " camera " + cpos.ToString() + " yaw " + cam.GetYaw() + " pitch " + cam.GetPitch());
		}
		else if (c == "charlift")
		{
			DZS_CameraChar cl = cam.GetCharacter();
			if (cl && cl.GetMan())
			{
				vector cp = cl.GetMan().GetPosition();
				cl.GetMan().SetPosition(Vector(cp[0], cp[1] + a[1].ToFloat(), cp[2]));
				DZS_Log.Info("camtest charlift to " + cl.GetMan().GetPosition().ToString());
			}
		}
		else if (c == "charkill")
		{
			DZS_CameraChar ck = cam.GetCharacter();
			if (ck && ck.GetMan())
				ck.GetMan().SetHealth("", "", 0);
			DZS_Log.Info("camtest charkill");
		}
		else if (c == "pose")
		{
			cam.SetPose(Vector(a[1].ToFloat(), a[2].ToFloat(), a[3].ToFloat()), a[4].ToFloat(), a[5].ToFloat(), false);
			Log("pose");
		}
		else if (c == "poseg")
		{
			float x = a[1].ToFloat();
			float z = a[2].ToFloat();
			cam.SetPose(Vector(x, GetGame().SurfaceY(x, z) + a[3].ToFloat(), z), a[4].ToFloat(), a[5].ToFloat(), false);
			Log("poseg");
		}
		else if (c == "trace")
		{
			m_TraceLeft = a[1].ToFloat();
			m_TraceT = 0;
		}
		else if (c == "fps")
		{
			m_TargetFps = a[1].ToFloat();
			float fpsA = m_FpsFrames / Math.Max(0.001, m_FpsAcc);
			DZS_Log.Info("camtest fps target " + m_TargetFps.ToString() + ", measured so far " + fpsA.ToString());
			m_FpsAcc = 0;
			m_FpsFrames = 0;
		}
		else if (c == "fpsnow")
		{
			float fpsB = m_FpsFrames / Math.Max(0.001, m_FpsAcc);
			DZS_Log.Info("camtest fps measured " + fpsB.ToString());
			m_FpsAcc = 0;
			m_FpsFrames = 0;
		}
		else if (c == "setb")
		{
			m_Kernel.GetSettings().Set(a[1], DZS_Json.Boolean(a[2] == "true"));
			DZS_Log.Info("camtest setting " + a[1] + " = " + a[2]);
		}
		else if (c == "setn")
		{
			m_Kernel.GetSettings().Set(a[1], DZS_Json.Num(a[2]));
			DZS_Log.Info("camtest setting " + a[1] + " = " + a[2]);
		}
		else if (c == "action")
		{
			bool ok = m_Kernel.GetUi().Trigger(a[1]);
			DZS_Log.Info("camtest action " + a[1] + " -> " + DZS_Str.Bool(ok));
		}
		else if (c == "dialog")
		{
			m_Kernel.GetScreen().SetKeyboardOwner("dialog", a[1] == "on");
			DZS_Log.Info("camtest dialog " + a[1]);
		}
		else if (c == "spawn")
		{
			float sx = a[2].ToFloat();
			float sz = a[3].ToFloat();
			Object o = GetGame().CreateObjectEx(a[1], Vector(sx, GetGame().SurfaceY(sx, sz), sz), ECE_PLACE_ON_SURFACE);
			if (o && a.Count() > 4)
				o.SetScale(a[4].ToFloat());
			if (o)
				m_Spawned.Insert(o);
			DZS_Log.Info("camtest spawned " + a[1] + " " + DZS_Str.Bool(o != null));
		}
		else if (c == "focus")
		{
			vector mn = Vector(a[1].ToFloat(), a[2].ToFloat(), a[3].ToFloat());
			vector mx = Vector(a[4].ToFloat(), a[5].ToFloat(), a[6].ToFloat());
			cam.FocusOn(mn, mx, true);
		}
		else if (c == "project")
		{
			// screen x of a point at eye level, yaw offset a[1] degrees, 100 m away
			float off = a[1].ToFloat();
			vector dir = DZS_CameraPlugin.Dir(cam.GetYaw() + off, 0);
			vector pt = cam.GetPosition() + dir * 100;
			vector sp = GetGame().GetScreenPos(pt);
			int sw, sh;
			GetScreenSize(sw, sh);
			DZS_Log.Info("camtest project yaw+" + off + " -> screen " + sp[0] + " of " + sw + ", y " + sp[1] + " of " + sh + ", fov setting " + cam.HorizontalFov());
		}
		else if (c == "screenpos")
		{
			vector wp = Vector(a[1].ToFloat(), a[2].ToFloat(), a[3].ToFloat());
			vector s2 = GetGame().GetScreenPos(wp);
			DZS_Log.Info("camtest screenpos " + wp.ToString() + " -> " + s2.ToString());
		}
		else if (c == "pick")
		{
			vector hit;
			bool got = cam.PickScene(hit);
			float pd = vector.Distance(hit, cam.GetPosition());
			DZS_Log.Info("camtest pick " + DZS_Str.Bool(got) + " " + hit.ToString() + " dist " + pd.ToString());
		}
		else if (c == "clip")
		{
			string clip;
			GetGame().CopyFromClipboard(clip);
			DZS_Log.Info("camtest clipboard '" + clip + "'");
		}
		else if (c == "time")
		{
			int yr, mo, dy, hr, mi;
			GetGame().GetWorld().GetDate(yr, mo, dy, hr, mi);
			GetGame().GetWorld().SetDate(yr, mo, dy, a[1].ToInt(), 0);
			DZS_Log.Info("camtest time " + a[1]);
		}
		else if (c == "fog")
		{
			GetGame().GetWeather().GetFog().Set(a[1].ToFloat(), 0, 1000);
			GetGame().GetWeather().GetOvercast().Set(0, 0, 1000);
		}
		else if (c == "mouse")
		{
			int mouseX, mouseY;
			GetMousePos(mouseX, mouseY);
			string ms = "camtest mouse " + mouseX.ToString() + "," + mouseY.ToString();
			ms += " cursor " + DZS_Str.Bool(m_Kernel.GetScreen().IsCursorVisible());
			ms += " overScene " + DZS_Str.Bool(m_Kernel.GetScreen().IsPointerOverScene());
			DZS_Log.Info(ms);
		}
		else if (c == "hits")
		{
			cam.m_LogHits = a[1] == "on";
		}
		else if (c == "hint")
		{
			m_Kernel.GetScreen().ShowMessage("camtest", "info", "Test", 2.0);
		}
		else if (c == "icons")
		{
			int n = a[1].ToInt();
			for (int i = 0; i < 9; i++)
				m_Kernel.GetScreen().SetStatusIcon("camtest.icon" + i, i < n, "info", "Testsymbol " + i, "info");
		}
		else if (c == "segments")
		{
			int ns = a[1].ToInt();
			for (int j = 0; j < ns; j++)
			{
				DZS_StatusText seg = new DZS_StatusText("camtest.seg" + j, DZS_Font.LABEL, 160);
				m_Kernel.GetScreen().AddStatusLeft(seg);
				seg.SetText("Test segment " + j + " with a fairly long name");
			}
		}
		else if (c == "group")
		{
			string name = "";
			for (int g = 1; g < a.Count(); g++)
			{
				if (g > 1)
					name += " ";
				name += a[g];
			}
			m_Kernel.GetScreen().SetStatusGroup(name);
		}
		else if (c == "map")
		{
			DZS_CameraMap mp = cam.GetMap();
			if (mp)
				Log(mp.Describe());
		}
		else if (c == "nummsg")
		{
			string nt = DZS_Num.Count(20000) + " · " + DZS_Num.Count(2000) + " · " + DZS_Num.Count(999);
			m_Kernel.GetScreen().ShowMessage("camtest.num", "info", DZS_W.Literal("Zahlenformat: " + nt), 10);
			DZS_Log.Info("camtest: number message '" + nt + "'");
		}
		else if (c == "mapat")
		{
			vector mapS = cam.GetMap().ScreenOf(a[1].ToFloat(), a[2].ToFloat());
			DZS_Log.Info("camtest: map screen of " + a[1] + " " + a[2] + " = " + mapS[0] + " " + mapS[1] + " (ref " + (mapS[0] / DZS_Scale.Factor()).ToString() + " " + (mapS[1] / DZS_Scale.Factor()).ToString() + ")");
		}
		else if (c == "marks")
		{
			if (a[1] == "off")
				DZS_MapMarks.Clear("camtest");
			else
			{
				if (!m_MarkOwner)
					m_MarkOwner = new DZS_CamTestMarks();
				array<ref DZS_MapMark> ms2 = new array<ref DZS_MapMark>();
				for (int mk2 = 2; mk2 + 1 < a.Count(); mk2 += 2)
				{
					DZS_MapMark mk = new DZS_MapMark();
					mk.m_Id = "m" + (mk2 / 2);
					mk.m_X = a[mk2].ToFloat();
					mk.m_Z = a[mk2 + 1].ToFloat();
					mk.m_Ring = a[1] == "ring";
					if (mk2 == 2)
						mk.m_Color = ARGB(255, 0x3B, 0x82, 0xF6);
					else
						mk.m_Color = ARGB(255, 0xF9, 0x73, 0x16);
					mk.m_Tip = "Testmarke " + mk.m_Id;
					ms2.Insert(mk);
				}
				DZS_MapMarks.Set("camtest", ms2, m_MarkOwner);
			}
		}
	}
}

class DZS_CamTestMarks : DZS_MapMarkOwner
{
	override void OnMapMarkAction(string markId)
	{
		DZS_Log.Info("camtest: mark action " + markId);
	}
}

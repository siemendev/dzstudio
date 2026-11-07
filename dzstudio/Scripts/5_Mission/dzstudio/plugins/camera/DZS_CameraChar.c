// Game character: a figure of the game in first person to walk the
// scene. Not part of the project. While it is steered, keyboard and mouse belong to the game (DZS_Screen.SetGameControl);
// the free camera stays where it was and takes over again on Pos1.
class DZS_CameraChar : Managed
{
	static const string CLASS_NAME = "SurvivorM_Mirek";
	static const float DEATH_BACK = 3.0;

	protected DZS_CameraPlugin m_P;
	protected DZS_Kernel m_Kernel;
	protected Man m_Man;
	protected bool m_In;
	protected bool m_Invuln;
	protected float m_DeadFor = -1;
	protected ref DZS_Plate m_Plate;
	protected ref DZS_Plate m_Hint;
	protected bool m_ProbeDone;

	void DZS_CameraChar(DZS_CameraPlugin p, DZS_Kernel kernel)
	{
		m_P = p;
		m_Kernel = kernel;
	}

	bool IsIn() { return m_In; }
	bool Exists() { return m_Man != null; }
	bool IsInvulnerable() { return m_Invuln; }
	Man GetMan() { return m_Man; }

	protected Man Make(vector pos, float yaw)
	{
		Entity e = GetGame().CreatePlayer(null, CLASS_NAME, pos, 0, "NONE");
		Man m = Man.Cast(e);
		if (!m)
		{
			DZS_Log.Warn("character: " + CLASS_NAME + " not made");
			return null;
		}
		m.SetOrientation(Vector(yaw, 0, 0));
		DZS_Log.Info("character made at " + pos.ToString() + " yaw " + yaw);
		GiveLoadout(m);
		return m;
	}

	// Map key of the running world for the loadout files (data/loadouts/<key>.txt).
	static string LoadoutKey()
	{
		string w = GetGame().GetWorldName();
		w.ToLower();
		if (w == "enoch")
			return "livonia";
		if (w == "sakhal")
			return "sakhal";
		if (w == "nasdara")
			return "badlands";
		return "chernarus";
	}

	static string LoadoutPath(string key)
	{
		string own = "$profile:dzstudio\\loadouts\\" + key + ".txt";
		if (FileExist(own))
			return own;
		string bundled = "dzstudio/data/loadouts/" + key + ".txt";
		if (FileExist(bundled))
			return bundled;
		return "dzstudio/data/loadouts/chernarus.txt";
	}

	// Where-words of loadout files written before 0.6 are still understood.
	static string LoadoutWhere(string w)
	{
		if (w == "tragen")
			return "wear";
		if (w == "schulter")
			return "shoulder";
		if (w == "anbau")
			return "attach";
		if (w == "laden")
			return "load";
		if (w == "rucksack")
			return "backpack";
		if (w == "weste")
			return "vest";
		if (w == "irgendwo")
			return "anywhere";
		return w;
	}

	// A new figure gets the test loadout of its map. Worn and held items come first, then grenades, then the rest;
	// what finds no room on the figure goes into a second backpack at its feet, or onto the ground.
	protected void GiveLoadout(Man man)
	{
		PlayerBase pb = PlayerBase.Cast(man);
		if (!pb)
			return;
		string key = LoadoutKey();
		string path = LoadoutPath(key);
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (!fh)
		{
			DZS_Log.Warn("character loadout: cannot read " + path);
			return;
		}
		array<string> lines = new array<string>();
		string line;
		while (FGets(fh, line) >= 0)
			lines.Insert(line);
		CloseFile(fh);
		array<ref array<ref array<string>>> groups = new array<ref array<ref array<string>>>();
		int unknown = 0;
		foreach (string raw : lines)
		{
			string ln = raw.Trim();
			if (ln == "" || ln.IndexOf("#") == 0)
				continue;
			ln.Replace("\t", " ");
			array<string> parts = new array<string>();
			ln.Split(" ", parts);
			array<string> cols = new array<string>();
			foreach (string pc : parts)
			{
				if (pc != "")
					cols.Insert(pc);
			}
			if (cols.Count() < 2)
				continue;
			cols[0] = LoadoutWhere(cols[0]);
			string cls = cols[1];
			if (!GetGame().ConfigIsExisting("CfgVehicles " + cls) && !GetGame().ConfigIsExisting("CfgWeapons " + cls) && !GetGame().ConfigIsExisting("CfgMagazines " + cls))
			{
				DZS_Log.Warn("character loadout " + key + ": unknown class " + cls);
				unknown++;
				continue;
			}
			bool follows = cols[0] == "attach" || cols[0] == "load";
			if (!follows || groups.Count() == 0)
				groups.Insert(new array<ref array<string>>());
			groups[groups.Count() - 1].Insert(cols);
		}
		array<ref array<ref array<string>>> ordered = new array<ref array<ref array<string>>>();
		for (int pass = 0; pass < 3; pass++)
		{
			foreach (array<ref array<string>> g : groups)
			{
				array<string> first = g[0];
				string head = first[0];
				int rank = 2;
				if (head == "wear" || head == "hand" || head == "shoulder")
					rank = 0;
				else if (IsGrenade(first[1]))
					rank = 1;
				if (rank == pass)
					ordered.Insert(g);
			}
		}
		m_Made = 0;
		m_Spare = 0;
		m_Dropped = 0;
		m_Failed = 0;
		m_SpareBag = null;
		foreach (array<ref array<string>> og : ordered)
			GiveGroup(pb, key, og);
		DZS_Log.Info("character loadout " + key + " from " + path + ": " + m_Made + " made, " + m_Spare + " in the second backpack, " + m_Dropped + " dropped, " + m_Failed + " not placed, " + unknown + " unknown class(es)");
	}

	protected int m_Made;
	protected int m_Spare;
	protected int m_Dropped;
	protected int m_Failed;
	protected EntityAI m_SpareBag;

	static bool IsGrenade(string cls)
	{
		string low = cls;
		low.ToLower();
		return GetGame().IsKindOf(cls, "Grenade_Base") || low.Contains("40mm");
	}

	protected void GiveGroup(PlayerBase pb, string key, array<ref array<string>> group)
	{
		EntityAI base = null;
		EntityAI weapon = null;
		foreach (array<string> cols : group)
		{
			string where = cols[0];
			string cls = cols[1];
			int count = 1;
			if (cols.Count() > 2)
				count = Math.Max(1, cols[2].ToInt());
			for (int k = 0; k < count; k++)
			{
				EntityAI e = null;
				bool done = false;
				if (where == "wear" || where == "shoulder")
					e = pb.GetInventory().CreateAttachment(cls);
				else if (where == "hand")
					e = pb.GetHumanInventory().CreateInHands(cls);
				else if (where == "attach" && base)
					e = base.GetInventory().CreateAttachment(cls);
				else if (where == "load")
				{
					Weapon_Base wb = Weapon_Base.Cast(weapon);
					done = wb && wb.SpawnAmmo(cls);
				}
				else if (where == "backpack" || where == "vest")
				{
					string slot = "Back";
					if (where == "vest")
						slot = "Vest";
					EntityAI holder = pb.FindAttachmentBySlotName(slot);
					if (holder)
						e = holder.GetInventory().CreateInInventory(cls);
				}
				if (!e && !done && where != "load" && where != "attach")
					e = pb.GetInventory().CreateInInventory(cls);
				if (!e && !done && where != "load" && where != "attach")
					e = Overflow(pb, cls);
				if (e || done)
					m_Made++;
				else
				{
					m_Failed++;
					DZS_Log.Warn("character loadout " + key + ": " + cls + " (" + where + ") not placed");
				}
				if (e && where != "attach")
					base = e;
				if (e && (where == "hand" || where == "shoulder"))
					weapon = e;
			}
		}
	}

	// Into a second backpack of the worn kind at the figure's feet; when that is full, onto the ground beside it.
	protected EntityAI Overflow(PlayerBase pb, string cls)
	{
		vector at = pb.GetPosition() + pb.GetDirection() * 0.8;
		if (!m_SpareBag)
		{
			string bag = "AliceBag_Green";
			EntityAI worn = pb.FindAttachmentBySlotName("Back");
			if (worn)
				bag = worn.GetType();
			m_SpareBag = EntityAI.Cast(GetGame().CreateObjectEx(bag, at, ECE_PLACE_ON_SURFACE));
		}
		EntityAI e = null;
		if (m_SpareBag)
			e = m_SpareBag.GetInventory().CreateInInventory(cls);
		if (e)
		{
			m_Spare++;
			return e;
		}
		e = EntityAI.Cast(GetGame().CreateObjectEx(cls, at, ECE_PLACE_ON_SURFACE));
		if (e)
			m_Dropped++;
		return e;
	}

	void Toggle()
	{
		if (m_In)
			Leave();
		else
			Enter();
	}

	void Enter()
	{
		if (m_In)
			return;
		ProbeKeys();
		if (!m_Man || !m_Man.IsAlive())
		{
			Drop();
			vector cam = m_P.GetPosition();
			vector ground = Vector(cam[0], GetGame().SurfaceY(cam[0], cam[2]), cam[2]);
			m_Man = Make(ground, m_P.GetYaw());
			if (!m_Man)
				return;
		}
		m_P.BeforeCharacter();
		m_Invuln = m_Kernel.GetSettings().GetBool("camera.playerInvulnerable", true);
		GetGame().SelectPlayer(null, m_Man);
		ApplyDamage();
		m_P.SetCameraActive(false);
		ShowGameHud(true);
		m_Kernel.GetScreen().SetGameControl(true);
		m_In = true;
		m_DeadFor = -1;
		ShowPlate();
		DZS_Log.Info("character in at " + m_Man.GetPosition().ToString() + ", invulnerable " + m_Invuln + ", camera " + m_P.GetPosition().ToString());
	}

	void Leave()
	{
		if (!m_In)
			return;
		m_In = false;
		HidePlate();
		ShowGameHud(false);
		m_Kernel.GetScreen().SetGameControl(false);
		ClearPlayerEffects();
		m_P.SetCameraActive(true);
		string at = "gone";
		if (m_Man)
			at = m_Man.GetPosition().ToString();
		DZS_Log.Info("character out (figure " + at + "), camera " + m_P.GetPosition().ToString() + " yaw " + m_P.GetYaw() + " pitch " + m_P.GetPitch());
	}

	// Damage, shock and unconsciousness of the figure tint, blur and mute the game only while it is steered.
	// The figure stays the game's player after leaving it and renews its effects every tick, so they are
	// stopped every frame outside it.
	protected void StopPlayerEffects()
	{
		PPEManagerStatic.GetPPEManager().StopAllEffects(PPERequesterCategory.GAMEPLAY_EFFECTS);
		Mission mission = GetGame().GetMission();
		if (mission && mission.GetEffectWidgets())
		{
			mission.GetEffectWidgets().StopAllEffects();
			mission.GetEffectWidgets().ClearSuspendRequests();
		}
	}

	protected void ClearPlayerEffects()
	{
		StopPlayerEffects();
		if (m_Man)
			m_Man.SetMasterAttenuation("");
		GetGame().GetSoundScene().SetSoundVolume(g_Game.m_volume_sound, 0);
	}

	// Insert: for this stay in the figure only; the setting stays.
	void ToggleInvulnerable()
	{
		if (!m_In)
			return;
		m_Invuln = !m_Invuln;
		ApplyDamage();
		ShowPlate();
		string de = "Unverwundbar aus";
		string en = "Invulnerable off";
		if (m_Invuln)
		{
			de = "Unverwundbar an";
			en = "Invulnerable on";
		}
		m_Kernel.GetScreen().ShowMessage("camera.invulnerable", "", DZS_Texts.Pick(de, en), 2.0);
		DZS_Log.Info("character invulnerable " + m_Invuln);
	}

	// The game's own display (quick bar, status icons) only while the figure is steered.
	protected void ShowGameHud(bool show)
	{
		Mission mission = GetGame().GetMission();
		if (!mission)
			return;
		Hud hud = mission.GetHud();
		if (!hud)
			return;
		hud.ShowHudUI(show);
		hud.ShowQuickbarPlayer(show);
		HideGameCrosshair();
	}

	// The game's crosshair frame of the hud (dot, weapon cross) stays off in the figure and after it;
	// its selector only shows children, so the hidden frame keeps them off.
	protected static Widget s_Cross;

	static void HideGameCrosshair()
	{
		if (!s_Cross)
		{
			Widget ws = GetGame().GetWorkspace();
			if (!ws)
				return;
			s_Cross = ws.FindAnyWidget("CrossHairs");
		}
		Widget ch = s_Cross;
		if (ch && ch.IsVisible())
		{
			ch.Show(false);
			DZS_Log.Info("game crosshair hidden");
		}
	}

	protected void ApplyDamage()
	{
		if (!m_Man)
			return;
		m_Man.SetAllowDamage(!m_Invuln);
		PlayerBase pb = PlayerBase.Cast(m_Man);
		if (pb)
			pb.SetModifiers(!m_Invuln);
	}

	// T: the figure to the point under the pointer; the camera stays.
	void ToPoint(vector hit)
	{
		if (m_Man && m_Man.IsAlive())
		{
			m_Man.SetPosition(hit);
			DZS_Log.Info("character moved to " + hit.ToString());
			return;
		}
		Drop();
		m_Man = Make(hit, m_P.GetYaw());
	}

	void Update(float dt)
	{
		if (m_In)
			HideGameCrosshair();
		if (!m_Man)
			return;
		if (!m_In)
			StopPlayerEffects();
		if (m_Man.IsAlive())
		{
			if (m_In && m_Invuln)
				KeepWhole();
			return;
		}
		if (m_DeadFor < 0)
		{
			m_DeadFor = 0;
			DZS_Log.Info("character died at " + m_Man.GetPosition().ToString());
		}
		m_DeadFor += dt;
		if (m_DeadFor >= DEATH_BACK)
		{
			Leave();
			Drop();
			DZS_Log.Info("character gone after " + DZS_Num.Fixed(DEATH_BACK, 1) + " s");
		}
	}

	// Health, blood and shock stay full while invulnerable; hunger, thirst, heat and cold stay neutral.
	protected void KeepWhole()
	{
		array<string> kinds = {"", "Blood", "Shock"};
		foreach (string k : kinds)
		{
			float mx = m_Man.GetMaxHealth("", k);
			if (mx > 0 && m_Man.GetHealth("", k) < mx)
				m_Man.SetHealth("", k, mx);
		}
		PlayerBase pb = PlayerBase.Cast(m_Man);
		if (!pb)
			return;
		if (pb.GetStatWater())
			pb.GetStatWater().Set(pb.GetStatWater().GetMax());
		if (pb.GetStatEnergy())
			pb.GetStatEnergy().Set(pb.GetStatEnergy().GetMax());
		if (pb.GetStatHeatComfort())
			pb.GetStatHeatComfort().Set(0);
		if (pb.GetStatHeatBuffer())
			pb.GetStatHeatBuffer().Set(0);
		if (pb.GetStatTremor())
			pb.GetStatTremor().Set(0);
		if (pb.GetStatToxicity())
			pb.GetStatToxicity().Set(0);
	}

	void Drop()
	{
		if (m_Man)
			GetGame().ObjectDelete(m_Man);
		m_Man = null;
		m_DeadFor = -1;
	}

	void End()
	{
		if (m_In)
			Leave();
		Drop();
	}

	// Health of the figure for the log (acceptance: alive, health full after a fall).
	string HealthText()
	{
		if (!m_Man)
			return "none";
		string survival = "";
		PlayerBase pb = PlayerBase.Cast(m_Man);
		if (pb && pb.GetStatWater())
			survival = " water " + DZS_Num.Fixed(pb.GetStatWater().Get(), 0) + " energy " + DZS_Num.Fixed(pb.GetStatEnergy().Get(), 0) + " heatcomfort " + DZS_Num.Fixed(pb.GetStatHeatComfort().Get(), 2) + " heatbuffer " + DZS_Num.Fixed(pb.GetStatHeatBuffer().Get(), 1) + " modifiers " + pb.GetModifiersManager().IsModifiersEnable() + " uncon " + pb.IsUnconscious() + " selected " + (GetGame().GetPlayer() == m_Man);
		return "alive " + m_Man.IsAlive() + survival + " health " + DZS_Num.Fixed(m_Man.GetHealth("", ""), 1) + "/" + DZS_Num.Fixed(m_Man.GetMaxHealth("", ""), 1) + " blood " + DZS_Num.Fixed(m_Man.GetHealth("", "Blood"), 0) + " shock " + DZS_Num.Fixed(m_Man.GetHealth("", "Shock"), 0);
	}

	// ---- display: top left, 12 px from the edge ----

	protected void ShowPlate()
	{
		HidePlate();
		Widget root = m_Kernel.GetScreen().GetRoot();
		string tone = "warning";
		string icon = "shield-off";
		string label = DZS_Texts.Pick("Verwundbar", "Vulnerable");
		if (m_Invuln)
		{
			tone = "success";
			icon = "shield";
			label = DZS_Texts.Pick("Unverwundbar", "Invulnerable");
		}
		m_Plate = new DZS_Plate(root, tone, label);
		if (m_Plate.m_Icon)
			DZS_W.SetIcon(m_Plate.m_Icon, icon);
		float pad = DZS_Scale.Px(12);
		m_Plate.Place(pad, pad);
		DZS_UiRegistry ui = m_Kernel.GetUi();
		string back = ui.FirstBinding("camera.player");
		string sw = ui.FirstBinding("camera.invulnerable");
		string hint = DZS_Texts.Pick("[" + back + "] zurück zum Editor · [" + sw + "] umschalten", "[" + back + "] back to the editor · [" + sw + "] toggle");
		m_Hint = new DZS_Plate(root, "textMuted", hint, "read", 12);
		m_Hint.m_Line.SetColor(DZS_Color.Get("textMuted"));
		m_Hint.Place(pad, pad + m_Plate.m_H + DZS_Scale.Px(6));
	}

	protected void HidePlate()
	{
		if (m_Plate)
			m_Plate.Destroy();
		m_Plate = null;
		if (m_Hint)
			m_Hint.Destroy();
		m_Hint = null;
	}

	// To verify in the game before the build: which inputs of the game hold Home or Insert in the current (default) binding.
	protected void ProbeKeys()
	{
		if (m_ProbeDone)
			return;
		m_ProbeDone = true;
		UAInputAPI api = GetUApi();
		TIntArray ids = new TIntArray();
		api.GetActiveInputs(ids);
		int hits = 0;
		foreach (int id : ids)
		{
			UAInput inp = api.GetInputByID(id);
			if (!inp)
				continue;
			int was = inp.AlternativeIndex();
			for (int a = 0; a < inp.AlternativeCount(); a++)
			{
				inp.SelectAlternative(a);
				for (int k = 0; k < inp.BindKeyCount(); k++)
				{
					int key = inp.GetBindKey(k);
					string nm = api.GetButtonName(key);
					if (key == KeyCode.KC_HOME || key == KeyCode.KC_INSERT)
					{
						hits++;
						DZS_Log.Info("character keys: input " + id + " hash " + inp.NameHash() + " bound to " + nm);
					}
				}
			}
			inp.SelectAlternative(was);
		}
		DZS_Log.Info("character keys: " + ids.Count() + " game inputs, " + hits + " bound to Pos1/Einfg (KC_HOME " + KeyCode.KC_HOME + ", KC_INSERT " + KeyCode.KC_INSERT + ")");
	}
}

[DZS_RegisterPlugin(DZS_SpawnerPlugin)]
class DZS_SpawnerPlugin : DZS_Plugin
{
	override string GetName() { return "spawner"; }

	override void OnRegister()
	{
		m_Kernel.GetTypes().RegisterCompanionType("spawner", DZS_SpawnerFormat.READ_TYPE, 1);
		m_Kernel.GetTypes().RegisterCompanionType("spawner", DZS_SpawnerFormat.ROOT_TYPE, 1);
		m_Kernel.GetTypes().RegisterOperationType("spawner", DZS_SpawnerFix.OP, 1);
		m_Kernel.RegisterFormat(new DZS_SpawnerFormat());
	}
}

// "Correct spelling": a model path refused only because of the
// spelling of its folder gets that spelling; the rest of the path stays.
class DZS_SpawnerFix : DZS_IssueFix
{
	static const string OP = "spawner:edit";

	string m_Id;
	string m_Name;

	override bool Apply()
	{
		DZS_Instance inst = DZS_Kernel.Get().GetShown();
		if (!inst)
			return false;
		DZS_OpBuilder b = inst.BeginOperation(OP, 1);
		b.SetField(m_Id, DZS_Obj.NAME, m_Name);
		DZS_SubmitResult r = b.Commit("Schreibweise korrigieren", "Correct spelling");
		DZS_Log.Info("spawner: spelling of " + m_Id + " -> " + m_Name + " " + r.Ok());
		return r.Ok();
	}

	// The allowed spelling of the folder in the path, or "" when the path is not refused for its spelling alone.
	static string Fixed(string name)
	{
		string low = name;
		low.ToLower();
		array<string> allowed = DZS_SpawnerFormat.AllowedRoots();
		foreach (string p : allowed)
		{
			string lp = p;
			lp.ToLower();
			int at = low.IndexOf(lp);
			if (at < 0)
				continue;
			string head = name.Substring(0, at);
			int after = at + p.Length();
			string tail = name.Substring(after, name.Length() - after);
			string fixedName = head + p + tail;
			if (DZS_SpawnerFormat.PathAllowed(fixedName))
				return fixedName;
		}
		return "";
	}
}

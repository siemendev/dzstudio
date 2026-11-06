// Makes the group "Objekte" when new objects have no group to go into. The project
// provider sets it (it knows files and colours); the kernel asks it inside the operation that sets the objects.
class DZS_GroupMaker : Managed
{
	string NewObjectGroup(DZS_Instance inst, DZS_OpBuilder b) { return ""; }
}

// The active group belongs to the open project and is held with it in the kernel.
// It is view: no operation, no undo step, not saved. Plugins read it and never keep their own.
class DZS_ActiveGroup
{
	static bool TakesObjects(DZS_Instance inst, string g)
	{
		if (!inst || g == "" || !inst.Has(g) || inst.GetKind(g) != DZS_KIND_GROUP)
			return false;
		string unreadable;
		inst.GetField(g, "unreadable", unreadable);
		if (unreadable == "1")
			return false;
		string fmt;
		inst.GetField(g, DZS_Obj.GROUP_FORMAT, fmt);
		return fmt != "loot";
	}

	// The active group, or the first group of the list that can be active; "" if none can.
	static string Get(DZS_Instance inst)
	{
		if (!inst)
			return "";
		if (TakesObjects(inst, inst.m_ActiveGroup))
			return inst.m_ActiveGroup;
		inst.m_ActiveGroup = "";
		array<string> groups = inst.GetChildren(DZS_PROJECT_ID);
		foreach (string g : groups)
		{
			if (TakesObjects(inst, g))
			{
				inst.m_ActiveGroup = g;
				break;
			}
		}
		return inst.m_ActiveGroup;
	}

	static bool Set(DZS_Instance inst, string g)
	{
		if (!TakesObjects(inst, g))
			return false;
		inst.m_ActiveGroup = g;
		DZS_Log.Info("active group " + g);
		return true;
	}

	// Name for hints and the status bar; "Objekte" while no group exists that new objects would go into.
	static string Name(DZS_Instance inst)
	{
		string g = Get(inst);
		if (g == "")
			return DZS_Texts.Get("core.group.objects");
		string name;
		inst.GetField(g, DZS_Obj.GROUP_NAME, name);
		return name;
	}

	// For an operation that sets new objects: the active group, or a new group "Objekte" made in the same
	// operation (its undo removes it again); it becomes active once the operation is in.
	static string ForNewObjects(DZS_Instance inst, DZS_OpBuilder b)
	{
		string g = Get(inst);
		if (g != "")
			return g;
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k || !k.GetGroupMaker())
			return "";
		g = k.GetGroupMaker().NewObjectGroup(inst, b);
		inst.m_ActiveGroup = g;
		return g;
	}
}

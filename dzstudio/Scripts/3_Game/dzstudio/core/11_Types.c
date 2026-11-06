class DZS_KindInfo : Managed
{
	string m_Name;
	int m_Version;
	ref array<string> m_ParentKinds;
}

// Every operation type, element kind and companion type is named "plugin:type" and carries a version.
class DZS_TypeRegistry : Managed
{
	protected ref map<string, int> m_OpTypes;
	protected ref map<string, ref DZS_KindInfo> m_Kinds;
	protected ref map<string, int> m_CompanionTypes;

	void DZS_TypeRegistry()
	{
		m_OpTypes = new map<string, int>();
		m_Kinds = new map<string, ref DZS_KindInfo>();
		m_CompanionTypes = new map<string, int>();
	}

	protected bool CheckName(string pluginName, string typeName)
	{
		if (DZS_Str.PluginOfType(typeName) != pluginName)
		{
			DZS_Log.Error("type '" + typeName + "' must start with '" + pluginName + ":'");
			return false;
		}
		return true;
	}

	bool RegisterOperationType(string pluginName, string typeName, int version)
	{
		if (!CheckName(pluginName, typeName) || version < 1)
			return false;
		m_OpTypes.Set(typeName, version);
		return true;
	}

	bool RegisterElementKind(string pluginName, string kindName, int version, array<string> parentKinds)
	{
		if (!CheckName(pluginName, kindName) || version < 1)
			return false;
		DZS_KindInfo k = new DZS_KindInfo();
		k.m_Name = kindName;
		k.m_Version = version;
		k.m_ParentKinds = new array<string>();
		if (parentKinds)
			k.m_ParentKinds.Copy(parentKinds);
		m_Kinds.Set(kindName, k);
		return true;
	}

	bool RegisterCompanionType(string pluginName, string typeName, int version)
	{
		if (!CheckName(pluginName, typeName) || version < 1)
			return false;
		m_CompanionTypes.Set(typeName, version);
		return true;
	}

	bool KnowsOperation(string typeName, int version)
	{
		int v;
		return m_OpTypes.Find(typeName, v) && version <= v;
	}

	bool KnowsKind(string kindName)
	{
		return m_Kinds.Contains(kindName);
	}

	bool AllowsParent(string kindName, string parentKind)
	{
		DZS_KindInfo k = m_Kinds.Get(kindName);
		if (!k)
			return false;
		if (parentKind == "")
			return k.m_ParentKinds.Count() == 0;
		return k.m_ParentKinds.Find(parentKind) >= 0;
	}

	bool KnowsCompanion(string typeName, int version)
	{
		int v;
		return m_CompanionTypes.Find(typeName, v) && version <= v;
	}

	int OperationVersion(string typeName)
	{
		int v;
		m_OpTypes.Find(typeName, v);
		return v;
	}
}

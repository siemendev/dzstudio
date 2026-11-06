// Base class of every plugin, bundled or third-party. Register a plugin by putting
// [DZS_RegisterPlugin(MyPlugin)] in front of its class; it may live in any script module.
class DZS_Plugin : DZS_EventListener
{
	protected DZS_Kernel m_Kernel;

	// Fixed name, the same on every installation; prefixes its types ("name:type"), settings and actions ("name.x").
	string GetName() { return ""; }
	int GetVersion() { return 1; }
	int GetRequiredApiVersion() { return DZS_API_VERSION; }

	void Bind(DZS_Kernel kernel) { m_Kernel = kernel; }

	// Register operation types, element kinds, companion types, settings, actions, menu entries, panels, tools.
	void OnRegister() {}
	void OnSessionStart(string world) {}
	void OnSessionEnd() {}
	void OnUpdate(float dt) {}
}

class DZS_RegisterPlugin
{
	void DZS_RegisterPlugin(typename pluginType)
	{
		DZS_PluginRegistry.Add(pluginType);
	}
}

class DZS_PluginRegistry
{
	protected static ref array<typename> s_Types;
	protected static ref array<string> s_Names;

	// Mission-module classes register again on every mission load; the newest typename of a class wins.
	// Only the stored names are compared: typenames of an unloaded module must not be touched.
	static void Add(typename t)
	{
		if (!s_Types)
		{
			s_Types = new array<typename>();
			s_Names = new array<string>();
		}
		string name = t.ToString();
		int idx = s_Names.Find(name);
		if (idx >= 0)
		{
			s_Types[idx] = t;
			return;
		}
		s_Names.Insert(name);
		s_Types.Insert(t);
	}

	static array<typename> Types()
	{
		if (!s_Types)
		{
			s_Types = new array<typename>();
			s_Names = new array<string>();
		}
		return s_Types;
	}
}

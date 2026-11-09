// Project › Extensions …: the open project's extensions, adding one by the URL of its spec file, removing one, and the
// fields of extensions without a plugin (DZS_ExtUi.c).
class DZS_ExtActions : DZS_ActionHandler
{
	DZS_ExtPlugin m_P;

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		reasonDe = "Kein Projekt offen";
		reasonEn = "No project open";
		return m_P.OpenInstance() != null;
	}

	override void OnAction(string actionId)
	{
		if (actionId == "extensions.manage")
			m_P.OpenList();
	}
}

[DZS_RegisterPlugin(DZS_ExtPlugin)]
class DZS_ExtPlugin : DZS_Plugin
{
	protected ref DZS_ExtActions m_Actions;

	override string GetName() { return "extensions"; }

	override void OnRegister()
	{
		m_Actions = new DZS_ExtActions();
		m_Actions.m_P = this;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterAction("extensions", "extensions.manage", "Erweiterungen …", "Extensions …", null, m_Actions);
		ui.SetWhen("extensions.manage", "scene");
		ui.AddMenuEntry("project", "extensions.manage", 457);
	}

	// the shown instance of an open project, null otherwise
	DZS_Instance OpenInstance()
	{
		DZS_ProjectsPlugin pp = DZS_ProjectsPlugin.Cast(m_Kernel.GetPlugin("projects"));
		if (!pp || !pp.m_Open)
			return null;
		return m_Kernel.GetShown();
	}

	void OpenList()
	{
		DZS_Instance inst = OpenInstance();
		if (!inst)
			return;
		DZS_ExtListDialog d = new DZS_ExtListDialog(inst);
		d.Open();
	}
}

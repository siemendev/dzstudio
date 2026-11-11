// The editor's layers (Objects, Spawns, Effects, Territories): a segmented switch at the front of the top bar, one on
// at a time, keys 1–4. Objects is the normal editor; every other layer takes over the scene, the tool slot of the top
// bar, the catalogue (only what it creates) and the right bar (its list and inspector instead of the project). The
// switch is the rail at the left screen edge (DZS_LayerRail).
class DZS_LayerEntry : Managed
{
	string m_Key;
	string m_Name;
	string m_Sub;
	string m_Icon;
}

class DZS_Layer : Managed
{
	string Id() { return ""; }
	bool IsOn() { return false; }
	void TurnOff() {}
	// What the catalogue shows while the layer is on: a title ("Vehicles"), the classes it offers and own entries.
	string CatalogTitle() { return ""; }
	bool Offers(string cls) { return false; }
	void Extra(array<ref DZS_LayerEntry> into) {}
	// changes when the own entries change (a new event), so the catalogue asks again
	int ExtraRev() { return 0; }
	// A catalogue entry chosen: the next click in the scene (or the drop of a drag) creates it.
	void Arm(string key, bool drag) {}
	string Armed() { return ""; }
}

class DZS_Layers
{
	protected static ref array<DZS_Layer> s_All;

	static void Add(DZS_Layer l)
	{
		if (!s_All)
			s_All = new array<DZS_Layer>();
		s_All.Insert(l);
	}

	// Turns every layer off except 'keep' ("" = back to Objects).
	static void Only(string keep)
	{
		if (!s_All)
			return;
		foreach (DZS_Layer l : s_All)
		{
			if (l.Id() != keep && l.IsOn())
				l.TurnOff();
		}
	}

	static string Active()
	{
		DZS_Layer l = Current();
		if (!l)
			return "";
		return l.Id();
	}

	static DZS_Layer Current()
	{
		if (!s_All)
			return null;
		foreach (DZS_Layer l : s_All)
		{
			if (l.IsOn())
				return l;
		}
		return null;
	}

	// After a layer went on or off: the top bar's slot, actions of other layers and the right bar follow.
	static void Sync()
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return;
		string a = Active();
		k.GetUi().SetActiveLayer(a);
		k.GetUi().SetPanelShown("projects.groups", a == "");
	}
}

// Ground point under the pointer ray (stepping along it), shared by the layers.
class DZS_LayerRay
{
	static bool Ground(vector from, vector dir, out vector hit)
	{
		float step = 2;
		vector prev = from;
		for (int i = 1; i < 1500; i++)
		{
			vector p = from + dir * (i * step);
			if (p[1] <= GetGame().SurfaceY(p[0], p[2]))
			{
				vector a = prev;
				vector b = p;
				for (int k = 0; k < 12; k++)
				{
					vector m = (a + b) * 0.5;
					if (m[1] <= GetGame().SurfaceY(m[0], m[2]))
						b = m;
					else
						a = m;
				}
				hit = b;
				return true;
			}
			prev = p;
		}
		return false;
	}

	static bool Pointer(DZS_Screen scr, out vector hit)
	{
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetPointerDirection();
		if (!scr.IsCursorVisible())
			dir = GetGame().GetCurrentCameraDirection();
		return Ground(from, dir, hit);
	}

	// Flies the camera to look at a ground spot of this radius.
	static void Focus(float x, float z, float r)
	{
		DZS_CameraPlugin cam = DZS_CameraPlugin.Cast(DZS_Kernel.Get().GetPlugin("camera"));
		if (!cam)
			return;
		float y = GetGame().SurfaceY(x, z);
		float rr = Math.Max(r, 6);
		cam.FocusOn(Vector(x - rr, y, z - rr), Vector(x + rr, y + 2, z + rr));
	}
}

// The Objects layer: the editor as it is; its switch entry lives here, the other layers register their own.
class DZS_ObjectsLayerActions : DZS_ActionHandler
{
	DZS_Kernel m_K;

	override bool IsActionChecked(string actionId) { return DZS_Layers.Active() == ""; }

	override bool IsActionEnabled(string actionId, out string reasonDe, out string reasonEn)
	{
		if (!m_K.GetShown())
		{
			reasonDe = "Kein Projekt offen";
			reasonEn = "No project open";
			return false;
		}
		return true;
	}

	override void OnAction(string actionId)
	{
		DZS_Layers.Only("");
		DZS_Layers.Sync();
	}
}

[DZS_RegisterPlugin(DZS_LayersPlugin)]
class DZS_LayersPlugin : DZS_Plugin
{
	protected ref DZS_ObjectsLayerActions m_Actions;

	override string GetName() { return "layers"; }

	override void OnRegister()
	{
		m_Actions = new DZS_ObjectsLayerActions();
		m_Actions.m_K = m_Kernel;
		DZS_UiRegistry ui = m_Kernel.GetUi();
		ui.RegisterKeyGroup("layers", "Ebenen", "Layers");
		DZS_LayersPlugin.Register(ui, "layers", "layers.objects", "objects", "Objekte", "Objects", "box", "1", 10, m_Actions);
	}

	// One entry of the layer switch: action with its key, tool in group "layers", tooltip "Ebene Spawns · 2".
	static void Register(DZS_UiRegistry ui, string plugin, string id, string layer, string de, string en, string icon, string key, int order, DZS_ActionHandler h)
	{
		array<string> keys = {key};
		ui.RegisterAction(plugin, id, "Ebene " + de, en + " layer", keys, h);
		ui.SetGroup(id, "layers");
		ui.RegisterTool(plugin, id, de, en, "layers", icon, order);
		ui.SetToolLayer(id, layer);
		ui.SetToolTip(id, "Ebene " + de, en + " layer", id);
	}
}

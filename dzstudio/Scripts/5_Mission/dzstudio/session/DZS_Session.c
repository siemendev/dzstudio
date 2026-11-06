// One editor session: a loaded map with the kernel running on it. Lives as long as the mission.
class DZS_Session : Managed
{
	protected string m_World;
	protected Camera m_StartCamera;
	protected bool m_Started;

	void Start()
	{
		m_World = GetGame().GetWorldName();
		DZS_Log.Info("session start, world " + m_World);
		PlaceStartCamera();
		GetDayZGame().LoadingHide(true);
		string sheet;
		if (GetCLIParam("dzsFontSheetSession", sheet))
			DZS_FontSheet.Show(null);
		DZS_Kernel kernel = DZS_Kernel.Start(m_World);
		if (!kernel.GetShown() && DZS_Handover.s_Kind == "")
			kernel.OpenEmpty();
		m_Started = true;
	}

	// Fixed view over the middle of the map until a camera plugin takes over.
	protected void PlaceStartCamera()
	{
		float size = GetGame().GetWorld().GetWorldSize();
		float x = size * 0.5;
		float z = size * 0.5;
		vector pos = Vector(x, GetGame().SurfaceY(x, z) + 80, z);
		m_StartCamera = Camera.Cast(GetGame().CreateObject("staticcamera", pos, true));
		if (m_StartCamera)
		{
			m_StartCamera.SetActive(true);
			m_StartCamera.LookAt(Vector(x + 150, GetGame().SurfaceY(x + 150, z + 150), z + 150));
		}
	}

	void Update(float dt)
	{
		if (!m_Started)
			return;
		DZS_Kernel kernel = DZS_Kernel.Get();
		if (kernel)
			kernel.Update(dt);
	}

	void End()
	{
		if (!m_Started)
			return;
		m_Started = false;
		DZS_Kernel kernel = DZS_Kernel.Get();
		if (kernel)
			kernel.Shutdown();
		if (m_StartCamera)
			GetGame().ObjectDelete(m_StartCamera);
		DZS_Log.Info("session end");
	}
}

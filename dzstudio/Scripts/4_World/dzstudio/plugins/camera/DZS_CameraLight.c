// Camera light. Lights are spawned by class name, so the class lives in the world module.
class DZS_CameraLight : SpotLightBase
{
	void DZS_CameraLight()
	{
		SetVisibleDuringDaylight(true);
		SetRadiusTo(150);
		SetSpotLightAngle(70);
		SetCastShadow(false);
		SetBrightnessTo(4);
		SetDiffuseColor(1, 0.97, 0.92);
		SetAmbientColor(1, 0.97, 0.92);
		EnableSpecular(false);
	}
}

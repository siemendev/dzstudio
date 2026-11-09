// The ground under a position, for positions a project gives above the ground (an own effect area's aboveGround).
class DZS_PatchGround : Managed
{
	// The ground height at x/z; false when it is unknown.
	bool Height(float x, float z, out float y)
	{
		y = 0;
		return false;
	}
}

// The game's ground: the terrain of the world that is loaded, which is the project's map while dzStudio edits it.
class DZS_PatchGroundGame : DZS_PatchGround
{
	override bool Height(float x, float z, out float y)
	{
		y = GetGame().SurfaceY(x, z);
		return true;
	}
}

// A flat ground at height 0, so the conformance cases come out the same in every implementation.
class DZS_PatchGroundFlat : DZS_PatchGround
{
	override bool Height(float x, float z, out float y)
	{
		y = 0;
		return true;
	}
}

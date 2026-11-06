// Public interfaces of the scene and the placement plugins. Plugins ask
// DZS_SceneApi.Get() / DZS_SelectionApi.Get() and never touch the plugins' classes.

// The objects of the shown project as the scene draws them.
class DZS_SceneApi : Managed
{
	static DZS_SceneApi Get()
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return null;
		return DZS_SceneApi.Cast(k.GetService("scene"));
	}

	// The game object drawn for a project object, or null (hidden, unknown class).
	Object GetObject(string id) { return null; }
	// The project object a game object stands for, or "" (map objects, previews).
	string IdOf(Object o) { return ""; }
	// All drawn game objects (to keep them out of rays).
	void CollectObjects(array<Object> into) {}
	// Bounding box of a name in its own axes, unscaled.
	bool GetBox(string name, out vector boxMin, out vector boxMax) { return false; }
	// A view during a gesture: the object stands here until ClearView or the next operation on it.
	void SetView(string id, vector pos, vector ypr, float scale) {}
	void ClearView(string id) {}
	// The view an object stands in during a gesture, if any (panels show the intermediate value).
	bool GetView(string id, out vector pos, out vector ypr, out float scale) { return false; }
	// Makes the drawn object of an id a moving body for the physics tool; false if the
	// game does not simulate it. EndPhysics leaves the body where it is until ClearView or the next operation.
	bool StartPhysics(string id) { return false; }
	void EndPhysics(string id) {}
	// A local game object that belongs to no project object (views such as a brush stroke); the caller deletes it.
	Object MakeTemp(string name, vector pos, vector ypr, float scale) { return null; }
	// The object is not drawn until ClearView or the next operation on it (eraser during a stroke).
	void HideView(string id) {}
	// The scene takes this object for the id when the operation that creates the id comes in (no second model).
	void Adopt(string id, Object o) {}
	// Objects of a newly shown project not placed yet (they come over the next frames).
	int PendingObjects() { return 0; }
	// Hits on the padded box of objects the game's rays cannot meet (no collision box), merged into hits.
	void AddBoxHits(vector from, vector dir, float len, array<Object> exclude, array<ref DZS_RayHit> hits) {}
}

// Selection, lead object and active group (view, never an operation).
class DZS_SelectionApi : Managed
{
	static DZS_SelectionApi Get()
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return null;
		return DZS_SelectionApi.Cast(k.GetService("selection"));
	}

	// Ids in the order they were added; the last added is not necessarily the lead.
	array<string> GetSelection() { return new array<string>(); }
	string GetLead() { return ""; }
	// Changes whenever the selection changes (count, lead, first and last); cheap, no copy of the list.
	string Signature() { return ""; }
	bool IsSelected(string id) { return false; }
	// Replaces the selection; lead "" = the last of ids.
	void SetSelection(array<string> ids, string lead = "") {}
	void Add(array<string> ids) {}
	void Remove(array<string> ids) {}
	// The object highlighted in the scene or in the group list.
	string GetHover() { return ""; }
	void SetListHover(string id) {}
	// The group new objects go into.
	string GetActiveGroup() { return ""; }
	void SetActiveGroup(string groupId) {}
	// Esc stage "Pick object" (align, swap) and the swap state of the catalog.
	bool IsSwapping() { return false; }
	void SwapTo(string name) {}
	// Starts "Swap" for the selection.
	void StartSwap() {}
	// A knob in the object toolbar for an action of any plugin; order decides
	// the place (bundled: 10 Straighten … 80 Details …, plugins from 100). An action that is checked shows as on.
	void AddBarButton(string actionId, string icon, string labelDe, string labelEn, int order, bool singleOnly = false) {}
	// Height offset: foot point above the first surface below it.
	bool MeasureHeightOffset(string id, out float offset) { return false; }
	// Common bounding box (world axes) of the model boxes of these objects, as placing uses it.
	bool GetBounds(array<string> ids, out vector boxMin, out vector boxMax) { return false; }
	// Place on the map: whether an object is in the hand, and setting it on the terrain at a map point (with
	// the hand's turns and height offset; Shift keeps the series as in the scene).
	bool HasHand() { return false; }
	bool HandIsTemplate() { return false; }
	bool PlaceHandAt(float x, float z) { return false; }
}

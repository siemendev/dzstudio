// Public interface of the properties plugin: other plugins add rows to the properties part
// of the project bar; they appear in the look of the other rows, in the order the plugins load.
class DZS_PropertyRow : Managed
{
	// Built once into the rows area of the part.
	void Build(Widget parent) {}
	// Whether the row shows for these selected objects (one or several).
	bool AppliesTo(DZS_Instance inst, array<string> ids) { return false; }
	// Lays the row out at y; returns its height (screen pixels).
	float Layout(DZS_Instance inst, array<string> ids, float y, float width) { return 0; }
	void Show(bool show) {}
	void Update(float dt) {}
}

class DZS_PropertiesApi : Managed
{
	static DZS_PropertiesApi Get()
	{
		DZS_Kernel k = DZS_Kernel.Get();
		if (!k)
			return null;
		return DZS_PropertiesApi.Cast(k.GetService("properties"));
	}

	void AddRow(DZS_PropertyRow row) {}
	// Label of a row in the look of the part (Beschriftungsschrift, Text leise).
	static TextWidget RowLabel(Widget parent, string text)
	{
		TextWidget t = DZS_W.Text(parent, DZS_Font.LABEL, 10, DZS_Color.Get("textQuiet"));
		t.SetText(DZS_Texts.Upper(text));
		return t;
	}
}

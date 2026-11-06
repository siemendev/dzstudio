// The Objekt-Kachel on the Musterseite: row, small and card, each normal, in the hand, marked; a template with missing objects.
class DZS_CatSheetOwner : DZS_CatTileOwner {}

class DZS_CatSheetPart : DZS_SheetPart
{
	protected ref DZS_CatSheetOwner m_Owner = new DZS_CatSheetOwner();
	protected ref array<ref DZS_CatTile> m_Tiles = new array<ref DZS_CatTile>();
	protected ref DZS_CatEntry m_Tpl;
	protected DZS_CatalogPlugin m_P;

	override string TitleDe() { return "Objekt-Kachel — Zeile, klein, Karte; normal, in der Hand, markiert, Favorit, Vorlage"; }
	override string TitleEn() { return "Object tile — row, small, card; normal, in the hand, marked, favourite, template"; }

	override void Build(Widget inner)
	{
		DZS_CatalogService svc = DZS_CatalogService.Cast(DZS_Kernel.Get().GetService("catalog"));
		if (!svc || !svc.m_P || !svc.m_P.m_Store.IsReady())
			return;
		m_P = svc.m_P;
		DZS_CatEntry e = m_P.m_Store.Find("Barrel_Blue");
		if (!e)
			return;
		m_Tpl = new DZS_CatEntry();
		m_Tpl.m_Key = "tpl:sheet";
		m_Tpl.m_Low = "tpl:sheet";
		m_Tpl.m_Cat = "vorlagen";
		m_Tpl.m_Name = DZS_Texts.Pick("Lager am Fluss", "Camp by the river");
		m_Tpl.m_IsTemplate = true;
		m_Tpl.m_TplCount = 12;
		m_Tpl.m_TplMissing = 2;
		array<string> forms = {"row", "row", "row", "row", "small", "small", "card", "card", "card"};
		for (int i = 0; i < forms.Count(); i++)
		{
			DZS_CatTile t = new DZS_CatTile(inner, forms[i], m_Owner);
			if (i == 3)
				t.Bind(m_Tpl);
			else
				t.Bind(e);
			t.m_InHand = i == 1 || i == 5 || i == 8;
			t.m_Fav = i == 2;
			t.m_Marked = i == 7;
			if (i != 3)
				t.SetPhoto(m_P.PhotoOf(e));
			m_Tiles.Insert(t);
		}
	}

	override float Layout(float width, float y)
	{
		if (m_Tiles.Count() == 0)
			return 0;
		float gap = DZS_Scale.Px(8);
		float rowW = Math.Min(DZS_Scale.Px(300), (width - gap) / 2);
		float h = 0;
		for (int i = 0; i < 4; i++)
		{
			int col = i - (i / 2) * 2;
			int line = i / 2;
			m_Tiles[i].Place(col * (rowW + gap), y + line * (DZS_Scale.Px(48) + gap), rowW);
		}
		h = 2 * (DZS_Scale.Px(48) + gap);
		float x = 0;
		float s = DZS_Scale.Px(32);
		m_Tiles[4].Place(x, y + h, s);
		m_Tiles[5].Place(x + s + gap, y + h, s);
		x = 2 * (s + gap) + DZS_Scale.Px(16);
		float cw = DZS_Scale.Px(128);
		for (int c = 6; c < 9; c++)
		{
			m_Tiles[c].Place(x, y + h, cw);
			x += cw + gap;
		}
		return h + m_Tiles[6].Height();
	}

	override void Update(float dt)
	{
		foreach (DZS_CatTile t : m_Tiles)
			t.Refresh(m_P.SubText(t.m_Entry), dt);
	}
}

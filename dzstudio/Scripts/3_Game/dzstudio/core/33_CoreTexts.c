// Language table of the kernel's surface (German, English).
class DZS_CoreTexts
{
	static void Register()
	{
		DZS_Texts.Add("core.brand", "DZSTUDIO", "DZSTUDIO");
		DZS_Texts.Add("core.menu.project", "Projekt", "Project");
		DZS_Texts.Add("core.menu.edit", "Bearbeiten", "Edit");
		DZS_Texts.Add("core.menu.view", "Ansicht", "View");
		DZS_Texts.Add("core.provider.local", "Lokal", "Local");
		DZS_Texts.Add("core.save", "Speichern", "Save");
		DZS_Texts.Add("core.save.none", "Keine ungespeicherten Änderungen", "No unsaved changes");
		DZS_Texts.Add("core.search.clear", "Suche leeren", "Clear search");
		DZS_Texts.Add("core.dialog.close", "Schließen", "Close");
		DZS_Texts.Add("core.dialog.closeButton", "Schließen", "Close");
		DZS_Texts.Add("core.dialog.cancel", "Abbrechen", "Cancel");
		DZS_Texts.Add("core.on", "an", "on");
		DZS_Texts.Add("core.off", "aus", "off");
		DZS_Texts.Add("core.blocked.map", "Bei offener Karte nicht verfügbar", "Not available while the map is open");
		DZS_Texts.Add("core.blocked.fullsearch", "Bei offener Vollbild-Suche nicht verfügbar", "Not available while the full-screen search is open");
		DZS_Texts.Add("core.bar.collapse", "Leiste einklappen", "Collapse bar");

		DZS_Texts.Add("core.settings.kicker", "Editor", "Editor");
		DZS_Texts.Add("core.settings.title", "Einstellungen", "Settings");
		DZS_Texts.Add("core.settings.default", "Standard: %1", "Default: %1");
		DZS_Texts.Add("core.settings.resetAll", "Alles zurücksetzen …", "Reset everything …");
		DZS_Texts.Add("core.settings.resetTab", "%1 zurücksetzen …", "Reset %1 …");
		DZS_Texts.Add("core.settings.resetTabAsk", "Alle Einstellungen im Reiter %1 auf Standard setzen?", "Set all settings in the tab %1 to default?");
		DZS_Texts.Add("core.settings.resetAllAsk", "Alle Einstellungen und die ganze Tastenbelegung auf Standard setzen?", "Set all settings and all key bindings to default?");
		DZS_Texts.Add("core.settings.reset", "Zurücksetzen", "Reset");
		DZS_Texts.Add("core.settings.allDefault", "Alles steht auf Standard", "Everything is at default");
		DZS_Texts.Add("core.setting.pointerHints", "Hinweise am Zeiger", "Hints at the pointer");
		DZS_Texts.Add("core.setting.pointerHints.expl", "Zeilen mit Tasten im Hinweis neben dem Mauszeiger", "Lines with keys in the hint next to the pointer");

		DZS_Texts.Add("core.keys.press", "Taste drücken …", "Press a key …");
		DZS_Texts.Add("core.keys.escCancels", "Esc bricht ab", "Esc cancels");
		DZS_Texts.Add("core.keys.remove", "Taste entfernen", "Remove key");
		DZS_Texts.Add("core.keys.mustKeep", "Ohne Taste kommt man aus der Spielfigur nicht zurück", "Without a key there is no way back from the character");
		DZS_Texts.Add("core.keys.characterNote", "Diese Taste bekommt das Spiel in der Spielfigur nicht", "The game does not get this key in the character");
		DZS_Texts.Add("core.keys.search", "Aktion oder Taste suchen", "Search action or key");
		DZS_Texts.Add("core.keys.conflict1", "1 Taste doppelt belegt", "1 key bound twice");
		DZS_Texts.Add("core.keys.conflictN", "%1 Tasten doppelt belegt", "%1 keys bound twice");
		DZS_Texts.Add("core.keys.alsoBound", "Auch belegt: %1 (%2)", "Also bound: %1 (%2)");
		DZS_Texts.Add("core.keys.shiftTempo", "Wirkt nur mit Umschalt rechts — Umschalt links ist Tempo-Taste", "Works only with right Shift — left Shift is the speed key");
		DZS_Texts.Add("core.keys.altTempo", "Wirkt nur mit Alt rechts — Alt links ist Tempo-Taste", "Works only with right Alt — left Alt is the speed key");
		DZS_Texts.Add("core.keys.defaultTaken", "Standardtaste %1 ist belegt durch %2", "Default key %1 is taken by %2");
		DZS_Texts.Add("core.keys.ctrlStops", "Mit Strg steht die Kamera", "Ctrl stops the camera");
		DZS_Texts.Add("core.keys.onlySingle", "Nur eine einzelne Taste, nicht Strg", "Only a single key, not Ctrl");
		DZS_Texts.Add("core.keys.none", "Keine Aktion gefunden", "No action found");
		DZS_Texts.Add("core.keys.fixed", "Feste Tasten", "Fixed keys");
		DZS_Texts.Add("core.when.selection", "mit Auswahl", "with a selection");
		DZS_Texts.Add("core.when.hand", "mit Objekt in der Hand", "with an object in hand");
		DZS_Texts.Add("core.when.character", "in der Spielfigur", "in the character");
		DZS_Texts.Add("core.when.drag", "beim Ziehen", "while dragging");
		DZS_Texts.Add("core.when.hold", "halten", "hold");

		DZS_Texts.Add("core.fixed.esc.key", "Esc", "Esc");
		DZS_Texts.Add("core.fixed.esc", "Beendet das Oberste: Aufnahme, Menü, Dialog, Feld, Karte, freie Sicht, Geste, Auswahl", "Ends the topmost thing: recording, menu, dialog, field, map, free look, gesture, selection");
		DZS_Texts.Add("core.fixed.click.key", "Linksklick", "Left click");
		DZS_Texts.Add("core.fixed.click", "Auswählen, absetzen, Knöpfe bedienen", "Select, place, use knobs");
		DZS_Texts.Add("core.fixed.dclick.key", "Doppelklick", "Double click");
		DZS_Texts.Add("core.fixed.dclick", "Öffnen und setzen, wo die Funktion es vorsieht", "Open and set where the function provides it");
		DZS_Texts.Add("core.fixed.rclick.key", "Rechtsklick", "Right click");
		DZS_Texts.Add("core.fixed.rclick", "Kontextmenü", "Context menu");
		DZS_Texts.Add("core.fixed.rdrag.key", "Rechte Maustaste ziehen", "Right mouse drag");
		DZS_Texts.Add("core.fixed.rdrag", "Kamera drehen und neigen", "Turn and tilt the camera");
		DZS_Texts.Add("core.fixed.altrdrag.key", "Alt Rechte Maustaste ziehen", "Alt Right mouse drag");
		DZS_Texts.Add("core.fixed.altrdrag", "Um einen Punkt kreisen", "Orbit around a point");
		DZS_Texts.Add("core.fixed.wheel.key", "Mausrad", "Wheel");
		DZS_Texts.Add("core.fixed.wheel", "Kameratempo, Rollen, Zoomen (auch mit Umschalt, Strg, Alt)", "Camera speed, scrolling, zoom (also with Shift, Ctrl, Alt)");
		DZS_Texts.Add("core.fixed.modclick.key", "Umschalt Strg Alt an der Maus", "Shift Ctrl Alt with the mouse");
		DZS_Texts.Add("core.fixed.modclick", "Auswahl erweitern oder umschalten, beim Ziehen und Absetzen", "Extend or toggle the selection, while dragging and placing");
		DZS_Texts.Add("core.fixed.ctrl.key", "Strg", "Ctrl");
		DZS_Texts.Add("core.fixed.ctrl", "Hält die Kamera an", "Stops the camera");
		DZS_Texts.Add("core.fixed.fieldkeys.key", "Pfeile Enter Tab Leertaste Pos1 Ende Rück Entf", "Arrows Enter Tab Space Home End Backspace Del");
		DZS_Texts.Add("core.fixed.fieldkeys", "In Menüs, Dialogen, Feldern, Vollbild-Suche, Karte und Hauptmenü", "In menus, dialogs, fields, full-screen search, map and main menu");
		DZS_Texts.Add("core.fixed.fieldctrl.key", "Strg A C V X Z", "Ctrl A C V X Z");
		DZS_Texts.Add("core.fixed.fieldctrl", "In Feldern: markieren, kopieren, einfügen, ausschneiden, rückgängig", "In fields: select, copy, paste, cut, undo");
	}
}

from lib import *

# Bohemia's mission folders (github.com/BohemiaInteractive/DayZ-Central-Economy), as dayzOffline.<map>/
VAN = os.environ.get('DAYZ_VANILLA') or os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'vanilla-files')

def vanilla(mission, rel):
    with open(os.path.join(VAN, 'dayzOffline.' + mission, rel), 'rb') as f:
        return f.read().decode('utf-8')

GP = 'cfggameplay.json'
EA_CH = 'cfgEffectArea.json'
EA = 'cfgeffectarea.json'

def area(name, x, z):
    return '''\t\t{ \t"AreaName": "%s",
\t\t\t"Type": "ContaminatedArea_Static",
\t\t\t"TriggerType": "ContaminatedTrigger",
\t\t\t"Data": {
\t\t\t\t"Pos": [ %s, 0, %s ],
\t\t\t\t"Radius": 75,
\t\t\t\t"PosHeight": 20,
\t\t\t\t"NegHeight": 3,
\t\t\t\t"InnerPartDist": 80,
\t\t\t\t"OuterOffset": 30,
\t\t\t\t"ParticleName": "graphics/particles/contaminated_area_gas_bigass"
\t\t\t\t},
\t\t\t"PlayerData": {
\t\t\t\t"AroundPartName": "graphics/particles/contaminated_area_gas_around",
\t\t\t\t"TinyPartName": "graphics/particles/contaminated_area_gas_around_tiny",
\t\t\t\t"PPERequesterType": "PPERequester_ContaminatedAreaTint"
\t\t\t}
\t\t}''' % (name, x, z)

def area_file(areas, safe='\t"SafePositions":[\n\t\t[434, 13624],\n\t\t[360, 10986]\n\t]\n'):
    t = '{\n    "Areas": [\n' + ',\n'.join(areas) + '\n\t]'
    if safe:
        t += ',\n' + safe
    else:
        t += '\n'
    return t + '}'

def own_area(name, typ="ContaminatedArea_Static"):
    return {"AreaName": name, "Type": typ, "TriggerType": "ContaminatedTrigger",
            "Data": {"Pos": [1000, 0, 2000], "Radius": 50, "PosHeight": 20, "NegHeight": 5, "InnerRingCount": 1,
                     "InnerPartDist": 50, "OuterRingToggle": True, "OuterPartDist": 40, "OuterOffset": 0,
                     "VerticalLayers": 0, "VerticalOffset": 0,
                     "ParticleName": "graphics/particles/contaminated_area_gas_bigass",
                     "EffectInterval": 0, "EffectDuration": 0, "EffectModifier": False},
            "PlayerData": {"AroundPartName": "graphics/particles/contaminated_area_gas_around",
                           "TinyPartName": "graphics/particles/contaminated_area_gas_around_tiny",
                           "PPERequesterType": "PPERequester_ContaminatedAreaTint"}}

def own_area_text(name, ind, unit):
    i1, i2 = ind + unit, ind + unit + unit
    return (ind + '{\n'
            + i1 + '"AreaName": "%s",\n' % name
            + i1 + '"Type": "ContaminatedArea_Static",\n'
            + i1 + '"TriggerType": "ContaminatedTrigger",\n'
            + i1 + '"Data": {\n'
            + i2 + '"Pos": [1000.000000, 0.000000, 2000.000000],\n'
            + i2 + '"Radius": 50,\n'
            + i2 + '"PosHeight": 20,\n'
            + i2 + '"NegHeight": 5,\n'
            + i2 + '"InnerRingCount": 1,\n'
            + i2 + '"InnerPartDist": 50,\n'
            + i2 + '"OuterRingToggle": true,\n'
            + i2 + '"OuterPartDist": 40,\n'
            + i2 + '"OuterOffset": 0,\n'
            + i2 + '"VerticalLayers": 0,\n'
            + i2 + '"VerticalOffset": 0,\n'
            + i2 + '"ParticleName": "graphics/particles/contaminated_area_gas_bigass",\n'
            + i2 + '"EffectInterval": 0,\n'
            + i2 + '"EffectDuration": 0,\n'
            + i2 + '"EffectModifier": false\n'
            + i1 + '},\n'
            + i1 + '"PlayerData": {\n'
            + i2 + '"AroundPartName": "graphics/particles/contaminated_area_gas_around",\n'
            + i2 + '"TinyPartName": "graphics/particles/contaminated_area_gas_around_tiny",\n'
            + i2 + '"PPERequesterType": "PPERequester_ContaminatedAreaTint"\n'
            + i1 + '}\n'
            + ind + '}')

def effect_areas():
    base = area_file([area("Ship-Bow", 13917, 11178), area("Ship-Stern", 13800, 11100)])
    B = {"name": "Ship-Bow"}
    E = "effectArea"
    changes = [
        S(B, "Data.Radius", N("75"), N("90"), kind=E),
        S(B, "Data.InnerRingCount", None, N("2"), kind=E),
        S(B, "Data.Pos", [13917, 0, 11178], [13920, 0, N("11178.5")], kind=E),
        S(B, "Data.OuterOffset", N("30"), N("30.0"), kind=E),
        S({"name": "ship-bow"}, "Data.Radius", None, N("1"), kind=E),
        R({"name": "Ship-Stern"}, kind=E),
        R({"x": 360, "z": 10986}, kind="safePosition"),
        S({"x": 434, "z": 13624}, "x", None, N("1"), kind="safePosition"),
        S(B, "AreaName", None, "X", kind=E),
        S(B, "Data.Radius", None, "big", kind=E),
    ]
    blocks = [
        {"id": "b-gas", "kind": E, "parent": None, "data": own_area("Own-Gas")},
        {"id": "b-dup", "kind": E, "parent": None, "data": own_area("Ship-Bow")},
        {"id": "b-inc", "kind": E, "parent": None, "data": {"AreaName": "Half", "Type": "SpookyArea"}},
        {"id": "b-safe", "kind": "safePosition", "parent": None, "data": {"x": 100, "z": 200}},
        {"id": "b-safe2", "kind": "safePosition", "parent": None, "data": {"x": N("434.0"), "z": 13624}},
    ]
    out = rep(base, ',\n' + area("Ship-Stern", 13800, 11100), ',\n' + own_area_text("Own-Gas", "\t\t", "\t"))
    out = rep(out, '"Radius": 75,', '"Radius": 90,')
    out = rep(out, '"Pos": [ 13917, 0, 11178 ]', '"Pos": [13920.000000, 0.000000, 11178.500000]')
    out = rep(out, '_bigass"\n\t\t\t\t},\n\t\t\t"PlayerData": {\n\t\t\t\t"AroundPartName": "graphics/particles/contaminated_area_gas_around",\n\t\t\t\t"TinyPartName": "graphics/particles/contaminated_area_gas_around_tiny",\n\t\t\t\t"PPERequesterType": "PPERequester_ContaminatedAreaTint"\n\t\t\t}\n\t\t},',
              '_bigass",\n\t\t\t\t"InnerRingCount": 2\n\t\t\t\t},\n\t\t\t"PlayerData": {\n\t\t\t\t"AroundPartName": "graphics/particles/contaminated_area_gas_around",\n\t\t\t\t"TinyPartName": "graphics/particles/contaminated_area_gas_around_tiny",\n\t\t\t\t"PPERequesterType": "PPERequester_ContaminatedAreaTint"\n\t\t\t}\n\t\t},')
    out = rep(out, '\t\t[434, 13624],\n\t\t[360, 10986]\n', '\t\t[434, 13624],\n\t\t[100.000000, 200.000000]\n')
    entries = [
        ch(0, "set", E, B, "applied", EA_CH, "Data.Radius"),
        ch(1, "set", E, B, "applied", EA_CH, "Data.InnerRingCount"),
        ch(2, "set", E, B, "applied", EA_CH, "Data.Pos"),
        ch(3, "set", E, B, "already", EA_CH, "Data.OuterOffset"),
        ch(4, "set", E, {"name": "ship-bow"}, "orphaned", EA_CH, "Data.Radius"),
        ch(5, "remove", E, {"name": "Ship-Stern"}, "applied", EA_CH),
        ch(6, "remove", "safePosition", {"x": 360, "z": 10986}, "applied", EA_CH),
        ch(7, "set", "safePosition", {"x": 434, "z": 13624}, "invalid", EA_CH, "x", detail="bad-op"),
        ch(8, "set", E, B, "invalid", EA_CH, "AreaName", detail="unknown-field"),
        ch(9, "set", E, B, "invalid", EA_CH, "Data.Radius", detail="bad-value"),
        bl(0, E, "b-gas", "applied", EA_CH),
        bl(1, E, "b-dup", "collision", EA_CH),
        bl(2, E, "b-inc", "invalid", EA_CH, "incomplete"),
        bl(3, "safePosition", "b-safe", "applied", EA_CH),
        bl(4, "safePosition", "b-safe2", "collision", EA_CH),
    ]
    case("effect-areas", {EA_CH: base}, {"changes": changes, "blocks": blocks}, {EA_CH: out},
         {"entries": entries, "files": [fl(EA_CH, "written")]})

def effect_area_sakhal():
    base = vanilla('sakhal', EA)
    blocks = [
        {"id": "b-gas", "kind": "effectArea", "parent": None, "data": own_area("Own-Gas")},
        {"id": "b-safe", "kind": "safePosition", "parent": None, "data": {"x": 100, "z": 200}},
    ]
    tail = '\n\t\t}\n    ]\n}'
    assert base.endswith(tail)
    out = base[:-len(tail)] + '\n\t\t},\n' + own_area_text("Own-Gas", "\t\t", "\t") + \
        '\n    ],\n    "SafePositions": [\n        [100.000000, 200.000000]\n    ]\n}'
    entries = [bl(0, "effectArea", "b-gas", "applied", EA), bl(1, "safePosition", "b-safe", "applied", EA)]
    case("effect-area-sakhal", {EA: base}, {"blocks": blocks}, {EA: out},
         {"entries": entries, "files": [fl(EA, "written")]})

def effect_area_broken():
    good = area_file([area("Ship-Bow", 13917, 11178)])
    variants = {
        "comma": rep(good, '_bigass"\n', '_bigass",\n'),
        "comment": rep(good, '"Radius": 75,', '"Radius": 75, // smaller than vanilla'),
        "int": rep(good, '"InnerPartDist": 80,', '"InnerPartDist": 80.0,'),
    }
    for name, base in variants.items():
        changes = [S({"name": "Ship-Bow"}, "Data.Radius", None, N("90"), kind="effectArea")]
        detail = "int-field" if name == "int" else "parse"
        entries = [ch(0, "set", "effectArea", {"name": "Ship-Bow"}, "invalid", EA, "Data.Radius", detail="base-invalid")]
        case("effect-area-broken-" + name, {EA: base}, {"changes": changes}, {},
             {"entries": entries, "files": [fl(EA, "broken", detail)]})

def effect_area_refused():
    base = area_file([area("Ship-Bow", 13917, 11178)])
    B = {"name": "Ship-Bow"}
    changes = [
        S(B, "Data.Radius", None, N("90"), kind="effectArea"),
        S(B, "Data.InnerPartDist", None, N("80.5"), kind="effectArea"),
    ]
    entries = [
        ch(0, "set", "effectArea", B, "invalid", EA, "Data.Radius", detail="output-invalid"),
        ch(1, "set", "effectArea", B, "invalid", EA, "Data.InnerPartDist", detail="output-invalid"),
    ]
    case("effect-area-refused", {EA: base}, {"changes": changes}, {},
         {"entries": entries, "files": [fl(EA, "invalid", "int-field")]})

def G(path, frm=None, to=None, has_from=True):
    c = {"op": "set", "block": "gameplaySetting", "key": {"path": path}}
    if has_from and frm is not None:
        c["from"] = frm
    c["to"] = to
    return c

def gch(i, path, status, detail=None, current=None, op="set"):
    return ch(i, op, "gameplaySetting", {"path": path}, status, GP, detail=detail, current=current)

def gameplay_settings():
    base = vanilla('chernarusplus', GP)
    SM = "PlayerData.StaminaData.staminaMax"
    changes = [
        G(SM, N("100"), N("120")),
        G("GeneralData.disableBaseDamage", False, True),
        G("GeneralData.disableBaseDamage", None, True),
        G("WorldsData.objectSpawnersArr", None, ["x.json"]),
        G("PlayerData.StaminaData", None, N("1")),
        G("GeneralData.newFlag", None, True),
        G("NewSection.sub.leaf", None, N("5.50")),
        {"op": "remove", "block": "gameplaySetting", "key": {"path": "GeneralData.disableContainerDamage"}},
        {"op": "remove", "block": "gameplaySetting", "key": {"path": "GeneralData.nothing"}},
        G("GeneralData.disableRespawnDialog", None, {"a": 1}),
        G(SM, N("100"), N("130")),
        G("WorldsData.environmentMinTemps", None, [N("-5"), N("-8.50")]),
    ]
    out = rep(base, '"staminaMax": 100.0,', '"staminaMax": 130,')
    out = rep(out, '"disableBaseDamage": false,\n\t\t"disableContainerDamage": false,',
              '"disableBaseDamage": true,')
    out = rep(out, '"disableRespawnInUnconsciousness": false\n\t},',
              '"disableRespawnInUnconsciousness": false,\n\t\t"newFlag": true\n\t},')
    import re as _re
    m = _re.search(r'"environmentMinTemps": \[[^\]]*\]', out)
    out = out[:m.start()] + '"environmentMinTemps": [-5, -8.5]' + out[m.end():]
    assert out.rstrip().endswith('}')
    body = out.rstrip()[:-1]
    last = body.rstrip()
    trail = out[len(last):]
    ind = '\t'
    out = last + ',\n' + ind + '"NewSection": {\n\t\t"sub": {\n\t\t\t"leaf": 5.5\n\t\t}\n\t}' + trail
    entries = [
        gch(0, SM, "applied"),
        gch(1, "GeneralData.disableBaseDamage", "applied"),
        gch(2, "GeneralData.disableBaseDamage", "already"),
        gch(3, "WorldsData.objectSpawnersArr", "invalid", "registration-list"),
        gch(4, "PlayerData.StaminaData", "invalid", "not-a-leaf"),
        gch(5, "GeneralData.newFlag", "applied"),
        gch(6, "NewSection.sub.leaf", "applied"),
        gch(7, "GeneralData.disableContainerDamage", "applied", op="remove"),
        gch(8, "GeneralData.nothing", "gone", op="remove"),
        gch(9, "GeneralData.disableRespawnDialog", "invalid", "bad-value"),
        gch(10, SM, "overrode", current="120"),
        gch(11, "WorldsData.environmentMinTemps", "applied"),
    ]
    case("gameplay-settings", {GP: base}, {"changes": changes}, {GP: out},
         {"entries": entries, "files": [fl(GP, "written")], "notes": ["enableCfgGameplayFile"]})

SPAWNER = {"name": "KotH", "objects": [
    {"name": "Land_Mil_Barracks1", "pos": [100, N("5.5"), 200], "ypr": [90, 0, 0]},
    {"name": "Barrel_Blue", "pos": [101, 5, 201], "scale": N("1.5"), "enableCEPersistency": 1, "customString": "x"},
]}
SPAWNER_TEXT = '''{
    "Objects": [
        {
            "name": "Land_Mil_Barracks1",
            "pos": [100.000000, 5.500000, 200.000000],
            "ypr": [90.000000, 0.000000, 0.000000],
            "scale": 1,
            "enableCEPersistency": false,
            "customString": ""
        },
        {
            "name": "Barrel_Blue",
            "pos": [101.000000, 5.000000, 201.000000],
            "ypr": [0.000000, 0.000000, 0.000000],
            "scale": 1.5,
            "enableCEPersistency": true,
            "customString": "x"
        }
    ]
}
'''
SMALL = '''{
\t"version": 123,
\t"WorldsData":
\t{
\t\t"lightingConfig": 1%s
\t}
}
'''

def reg_case(name, base_files, blocks, expected, entries, files, notes, slug="test", changes=None):
    proj = {"blocks": blocks, "changes": changes or []}
    if slug:
        proj["slug"] = slug
    case(name, base_files, proj, expected,
         {"entries": entries, "files": files, "notes": notes})

KP = 'dzstudio/test/b-koth.json'

def registrations():
    blk = {"id": "b-koth", "kind": "spawnerGroup", "parent": None, "data": SPAWNER}
    # no objectSpawnersArr at all
    reg_case("registration-none", {GP: SMALL % ''}, [blk],
             {GP: SMALL % (',\n\t\t"objectSpawnersArr": ["%s"]' % KP), KP: SPAWNER_TEXT},
             [bl(0, "spawnerGroup", "b-koth", "applied", KP), reg("b-koth", KP, "applied")],
             [fl(GP, "written"), fl(KP, "created")], ["enableCfgGameplayFile"])
    # already registered
    base = SMALL % (',\n\t\t"objectSpawnersArr": ["spawners/a.json", "%s"]' % KP)
    reg_case("registration-existing", {GP: base}, [blk], {KP: SPAWNER_TEXT},
             [bl(0, "spawnerGroup", "b-koth", "applied", KP), reg("b-koth", KP, "already")],
             [fl(GP, "unchanged"), fl(KP, "created")], ["enableCfgGameplayFile"])
    # duplicated foreign entries, two own groups, one broken group
    base = SMALL % ',\n\t\t"objectSpawnersArr": ["spawners/a.json", "spawners/b.json", "/spawners/a.json", "spawners\\\\b.json"]'
    blk2 = {"id": "b-two", "kind": "spawnerGroup", "parent": None, "data": {"objects": [{"name": "Barrel_Red", "pos": [1, 2, 3]}]}}
    bad = {"id": "b-bad", "kind": "spawnerGroup", "parent": None, "data": {"name": "nothing"}}
    badid = {"id": "B Bad", "kind": "spawnerGroup", "parent": None, "data": SPAWNER}
    two = '''{
    "Objects": [
        {
            "name": "Barrel_Red",
            "pos": [1.000000, 2.000000, 3.000000],
            "ypr": [0.000000, 0.000000, 0.000000],
            "scale": 1,
            "enableCEPersistency": false,
            "customString": ""
        }
    ]
}
'''
    TP = 'dzstudio/test/b-two.json'
    reg_case("registration-duplicated", {GP: base}, [blk, bad, blk2, badid],
             {GP: SMALL % (',\n\t\t"objectSpawnersArr": ["spawners/a.json", "spawners/b.json", "%s", "%s"]' % (KP, TP)),
              KP: SPAWNER_TEXT, TP: two},
             [bl(0, "spawnerGroup", "b-koth", "applied", KP), bl(1, "spawnerGroup", "b-bad", "invalid", detail="incomplete"),
              bl(2, "spawnerGroup", "b-two", "applied", TP), bl(3, "spawnerGroup", "B Bad", "invalid", detail="bad-id"),
              reg(None, "/spawners/a.json", "deduplicated"), reg(None, "spawners\\b.json", "deduplicated"),
              reg("b-koth", KP, "applied"), reg("b-two", TP, "applied")],
             [fl(GP, "written"), fl(KP, "created"), fl(TP, "created")], ["enableCfgGameplayFile"])
    # no cfggameplay.json in the base
    reg_case("registration-missing-gameplay", {"init.c": "void main() {}\n"}, [blk], {},
             [bl(0, "spawnerGroup", "b-koth", "invalid", GP, "missing-file")],
             [fl(GP, "missing")], [])
    # no slug
    reg_case("registration-no-slug", {GP: SMALL % ''}, [blk], {},
             [bl(0, "spawnerGroup", "b-koth", "invalid", detail="no-slug")], [], [], slug=None)
    # Sakhal: restricted-area default stays registered
    base = vanilla('sakhal', GP)
    out = rep(base, '"objectSpawnersArr": [],', '"objectSpawnersArr": ["%s"],' % KP)
    reg_case("registration-sakhal", {GP: base}, [blk], {GP: out, KP: SPAWNER_TEXT},
             [ch(0, "set", "gameplaySetting", {"path": "WorldsData.playerRestrictedAreaFiles"}, "invalid", GP, detail="registration-list"),
              bl(0, "spawnerGroup", "b-koth", "applied", KP), reg("b-koth", KP, "applied")],
             [fl(GP, "written"), fl(KP, "created")], ["enableCfgGameplayFile"],
             changes=[G("WorldsData.playerRestrictedAreaFiles", None, [])])

def run():
    effect_areas()
    effect_area_sakhal()
    effect_area_broken()
    effect_area_refused()
    gameplay_settings()
    registrations()

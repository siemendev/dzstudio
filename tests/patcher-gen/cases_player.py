from lib import *
from cases_json import vanilla, SMALL, SPAWNER, SPAWNER_TEXT, GP

PS = 'cfgplayerspawnpoints.xml'
PRA = "WorldsData.playerRestrictedAreaFiles"
GEAR = "PlayerData.spawnGearPresetFiles"

def split(text):
    a = text.index('    <hop>')
    b = text.index('    <travel>')
    return text[:a], text[a:b], text[b:]

def player_spawns():
    base = vanilla('chernarusplus', PS)
    fresh, hop, travel = split(base)
    S_ = "playerSpawnSection"
    G = "playerSpawnGroup"
    P = "playerSpawnPosition"
    WC = {"section": "fresh", "group": "WestCherno"}
    changes = [
        S({"section": "fresh"}, "spawn_params.min_dist_player", N("65"), N("80"), kind=S_),
        S({"section": "hop"}, "group_params.enablegroups", "false", "true", kind=S_),
        S({"section": "fresh"}, "spawn_params.min_dist_trigger", None, N("50"), kind=S_),
        S({"section": "nope"}, "spawn_params.min_dist_player", None, N("1"), kind=S_),
        R({"section": "hop"}, kind=S_),
        S({"section": "fresh", "name": "WestCherno"}, "lifetime", None, N("300"), kind=G),
        S(dict(WC, x=N("6063.0185"), z=N("1931.9072")), "pos", {"x": N("6063.018555"), "z": N("1931.907227")},
          {"x": 6070, "z": 1935}, kind=P),
        R(dict(WC, x=N("5933.964844"), z=N("2171.072998")), kind=P),
        R({"section": "hop", "group": "WestCherno", "x": N("5933.964844"), "z": N("2171.072998")}, kind=P),
        R({"section": "fresh", "name": "EastCherno"}, kind=G),
        S({"section": "hop"}, "generator_params.allow_in_water", None, "maybe", kind=S_),
    ]
    blocks = [
        {"id": "b-g", "kind": G, "parent": {"section": "fresh"}, "data": {"name": "NWAF", "counter": 3}},
        {"id": "b-g-1", "kind": P, "parent": "b-g", "data": {"x": 4500, "z": 10000}},
        {"id": "b-g-2", "kind": P, "parent": "b-g", "data": {"x": N("4600.25"), "z": 10100}},
        {"id": "b-p", "kind": P, "parent": {"section": "hop", "group": "Balota"}, "data": {"x": N("4400.5"), "z": 2400}},
        {"id": "b-near", "kind": P, "parent": {"section": "hop", "group": "Balota"}, "data": {"x": N("4320.9"), "z": N("2399.3")}},
        {"id": "b-dup", "kind": G, "parent": {"section": "fresh"}, "data": {"name": "WestCherno"}},
        {"id": "b-s", "kind": S_, "parent": None, "data": {}},
    ]
    f = rep(fresh, '<min_dist_player>65</min_dist_player>', '<min_dist_player>80</min_dist_player>')
    f = rep(f, '''            <max_dist_static>2</max_dist_static>
        </spawn_params>''', '''            <max_dist_static>2</max_dist_static>
            <min_dist_trigger>50</min_dist_trigger>
        </spawn_params>''')
    f = rep(f, '<group name="WestCherno">', '<group name="WestCherno" lifetime="300">')
    f = rep(f, '<pos x="6063.018555" z="1931.907227" />', '<pos x="6070.000000" z="1935.000000" />')
    f = rep(f, '                <pos x="5933.964844" z="2171.072998" />\n', '')
    east = f[f.index('            <group name="EastCherno">'):f.index('            <group name="WestElektro">')]
    f = rep(f, east, '')
    f = rep(f, '''        </generator_posbubbles>
    </fresh>''', '''            <group name="NWAF" counter="3">
                <pos x="4500.000000" z="10000.000000" />
                <pos x="4600.250000" z="10100.000000" />
            </group>
        </generator_posbubbles>
    </fresh>''')
    h = rep(hop, '<enablegroups>false</enablegroups>', '<enablegroups>true</enablegroups>')
    h = rep(h, '''                <pos x="4030.898926" z="3116.196533" />
            </group>''', '''                <pos x="4030.898926" z="3116.196533" />
                <pos x="4400.500000" z="2400.000000" />
            </group>''')
    entries = [
        ch(0, "set", S_, {"section": "fresh"}, "applied", PS, "spawn_params.min_dist_player"),
        ch(1, "set", S_, {"section": "hop"}, "applied", PS, "group_params.enablegroups"),
        ch(2, "set", S_, {"section": "fresh"}, "applied", PS, "spawn_params.min_dist_trigger"),
        ch(3, "set", S_, {"section": "nope"}, "invalid", PS, "spawn_params.min_dist_player", detail="bad-key"),
        ch(4, "remove", S_, {"section": "hop"}, "invalid", PS, detail="bad-op"),
        ch(5, "set", G, {"section": "fresh", "name": "WestCherno"}, "applied", PS, "lifetime"),
        ch(6, "set", P, changes[6]["key"], "applied", PS, "pos"),
        ch(7, "remove", P, changes[7]["key"], "applied", PS),
        ch(8, "remove", P, changes[8]["key"], "gone", PS),
        ch(9, "remove", G, {"section": "fresh", "name": "EastCherno"}, "applied", PS),
        ch(10, "set", S_, {"section": "hop"}, "invalid", PS, "generator_params.allow_in_water", detail="bad-value"),
        bl(0, G, "b-g", "applied", PS),
        bl(1, P, "b-g-1", "applied", PS),
        bl(2, P, "b-g-2", "applied", PS),
        bl(3, P, "b-p", "applied", PS),
        bl(4, P, "b-near", "collision", PS),
        bl(5, G, "b-dup", "collision", PS),
        bl(6, S_, "b-s", "invalid", detail="unknown-kind"),
    ]
    case("player-spawns", {PS: base}, {"changes": changes, "blocks": blocks}, {PS: f + h + travel},
         {"entries": entries, "files": [fl(PS, "written")]})

ATTR = {"healthMin": N("0.7"), "healthMax": 1, "quantityMin": 0, "quantityMax": 0}
GEAR_DATA = {
    "name": "Hunter", "spawnWeight": 2, "characterTypes": ["SurvivorM_Mirek", "SurvivorF_Eva"],
    "attachmentSlotItemSets": [{"slotName": "Body", "discreteItemSets": [
        {"itemType": "HuntingJacket_Brown", "spawnWeight": 1, "attributes": ATTR,
         "complexChildrenTypes": [{"itemType": "Mag_CZ527_5rnd", "attributes": ATTR, "quickBarSlot": 2,
                                   "complexChildrenTypes": [{"itemType": "Ammo_762x39", "attributes": ATTR}]}],
         "simpleChildrenTypes": ["Rag"]},
        {"itemType": "", "spawnWeight": 1, "attributes": ATTR}]}],
    "discreteUnsortedItemSets": [{"name": "food", "spawnWeight": 1, "attributes": ATTR, "simpleChildrenTypes": ["Apple"],
                                  "simpleChildrenUseDefaultAttributes": True}],
}

def gear_out(attr):
    def leaf(itemType, slot, kids=None, simple=None):
        return {"itemType": itemType, "attributes": attr, "quickBarSlot": slot, "complexChildrenTypes": kids or [],
                "simpleChildrenTypes": simple or [], "simpleChildrenUseDefaultAttributes": False}
    ammo = leaf("Ammo_762x39", -1)
    mag = leaf("Mag_CZ527_5rnd", 2, [ammo])
    jacket = {"itemType": "HuntingJacket_Brown", "spawnWeight": 1, "attributes": attr, "quickBarSlot": -1,
              "complexChildrenTypes": [mag], "simpleChildrenTypes": ["Rag"], "simpleChildrenUseDefaultAttributes": False}
    empty = {"itemType": "", "spawnWeight": 1, "attributes": attr, "quickBarSlot": -1, "complexChildrenTypes": [],
             "simpleChildrenTypes": [], "simpleChildrenUseDefaultAttributes": False}
    return {"spawnWeight": 2, "name": "Hunter", "characterTypes": ["SurvivorM_Mirek", "SurvivorF_Eva"],
            "attachmentSlotItemSets": [{"slotName": "Body", "discreteItemSets": [jacket, empty]}],
            "discreteUnsortedItemSets": [{"name": "food", "spawnWeight": 1, "attributes": attr, "complexChildrenTypes": [],
                                          "simpleChildrenTypes": ["Apple"], "simpleChildrenUseDefaultAttributes": True}]}

PRA_DATA = {"areaName": "Bunker", "PRABoxes": [[[2, 2, 3], [N("108.5"), 0, 0], [N("2570"), N("15.22"), N("5963.8")]]],
            "safePositions3D": [[N("2575.12"), N("15.25"), N("5954.31")]]}
PRA_TEXT = '''{
    "areaName": "Bunker",
    "PRABoxes": [
        [
            [2, 2, 3],
            [108.5, 0, 0],
            [2570.000000, 15.220000, 5963.800000]
        ]
    ],
    "safePositions3D": [
        [2575.120000, 15.250000, 5954.310000]
    ]
}
'''

def own_files():
    base = vanilla('chernarusplus', GP)
    KP = 'dzstudio/test/b-koth.json'
    GF = 'dzstudio/test/b-gear.json'
    PF = 'dzstudio/test/b-pra.json'
    PO = 'dzstudio/test/b-poly.json'
    poly = {"PRAPolygons": [[[1, 2], [3, 4], [5, N("6.5")]]], "safePositions2D": [[10, 20]]}
    blocks = [
        {"id": "b-koth", "kind": "spawnerGroup", "parent": None, "data": SPAWNER},
        {"id": "b-gear", "kind": "gearPreset", "parent": None, "data": GEAR_DATA},
        {"id": "b-pra", "kind": "restrictedArea", "parent": None, "data": PRA_DATA},
        {"id": "b-poly", "kind": "restrictedArea", "parent": None, "data": poly},
        {"id": "b-g0", "kind": "gearPreset", "parent": None, "data": dict(GEAR_DATA, spawnWeight=0)},
        {"id": "b-g1", "kind": "gearPreset", "parent": None, "data": {"spawnWeight": 1, "attachmentSlotItemSets": [
            {"slotName": "Hands", "discreteItemSets": [{"itemType": "Apple", "spawnWeight": 1}]}]}},
        {"id": "b-g2", "kind": "gearPreset", "parent": None, "data": {"spawnWeight": 1, "colour": "red"}},
        {"id": "b-p0", "kind": "restrictedArea", "parent": None, "data": {"areaName": "x", "PRABoxes": PRA_DATA["PRABoxes"]}},
        {"id": "b-p1", "kind": "restrictedArea", "parent": None, "data": {"PRAPolygons": [[[1, 2], [3, 4]]], "safePositions2D": [[1, 2]]}},
    ]
    out = rep(base, '"objectSpawnersArr": [],', '"objectSpawnersArr": ["%s"],' % KP)
    out = rep(out, '''		"wetnessWeightModifiers": [1.0, 1.0, 1.33, 1.66, 2.0]
	},''', '''		"wetnessWeightModifiers": [1.0, 1.0, 1.33, 1.66, 2.0],
		"playerRestrictedAreaFiles": ["%s", "%s"]
	},''' % (PF, PO))
    pd = base[base.index('"PlayerData"'):base.index('"WorldsData"')]
    end = pd.rindex('\t},')
    pd2 = pd[:end].rstrip('\n') + ',\n\t\t"spawnGearPresetFiles": ["%s"]\n' % GF + pd[end:]
    out = rep(out, pd, pd2)
    poly_text = '''{
    "areaName": "",
    "PRAPolygons": [
        [
            [1.000000, 2.000000],
            [3.000000, 4.000000],
            [5.000000, 6.500000]
        ]
    ],
    "safePositions2D": [
        [10.000000, 20.000000]
    ]
}
'''
    entries = [
        bl(0, "spawnerGroup", "b-koth", "applied", KP),
        bl(1, "gearPreset", "b-gear", "applied", GF),
        bl(2, "restrictedArea", "b-pra", "applied", PF),
        bl(3, "restrictedArea", "b-poly", "applied", PO),
        bl(4, "gearPreset", "b-g0", "invalid", detail="bad-value"),
        bl(5, "gearPreset", "b-g1", "invalid", detail="incomplete"),
        bl(6, "gearPreset", "b-g2", "invalid", detail="unknown-field"),
        bl(7, "restrictedArea", "b-p0", "invalid", detail="incomplete"),
        bl(8, "restrictedArea", "b-p1", "invalid", detail="bad-value"),
        reg("b-koth", KP, "applied"),
        reg("b-gear", GF, "applied", field=GEAR),
        reg("b-pra", PF, "applied", field=PRA),
        reg("b-poly", PO, "applied", field=PRA),
    ]
    case("registration-own-files", {GP: base}, {"slug": "test", "blocks": blocks},
         {GP: out, KP: SPAWNER_TEXT, GF: jtext(gear_out({"healthMin": N("0.7"), "healthMax": 1, "quantityMin": 0, "quantityMax": 0})) + '\n',
          PF: PRA_TEXT, PO: poly_text},
         {"entries": entries, "files": [fl(GP, "written"), fl(GF, "created"), fl(KP, "created"), fl(PO, "created"), fl(PF, "created")],
          "notes": ["enableCfgGameplayFile"]})

    # Sakhal: the warhead storage stays first in the list, also when the list has to be made
    sak = vanilla('sakhal', GP)
    blk = {"id": "b-pra", "kind": "restrictedArea", "parent": None, "data": PRA_DATA}
    case("registration-pra-sakhal", {GP: sak}, {"map": "sakhal", "slug": "test", "blocks": [blk]},
         {GP: rep(sak, '"playerRestrictedAreaFiles": ["pra/warheadstorage.json"]', '"playerRestrictedAreaFiles": ["pra/warheadstorage.json", "%s"]' % PF),
          PF: PRA_TEXT},
         {"entries": [bl(0, "restrictedArea", "b-pra", "applied", PF), reg("b-pra", PF, "applied", field=PRA)],
          "files": [fl(GP, "written"), fl(PF, "created")], "notes": ["enableCfgGameplayFile"]})
    case("registration-pra-seed", {GP: SMALL % ''}, {"map": "sakhal", "slug": "test", "blocks": [blk]},
         {GP: SMALL % (',\n\t\t"playerRestrictedAreaFiles": ["pra/warheadstorage.json", "%s"]' % PF), PF: PRA_TEXT},
         {"entries": [bl(0, "restrictedArea", "b-pra", "applied", PF),
                      reg(None, "pra/warheadstorage.json", "applied", field=PRA), reg("b-pra", PF, "applied", field=PRA)],
          "files": [fl(GP, "written"), fl(PF, "created")], "notes": ["enableCfgGameplayFile"]})

def run():
    player_spawns()
    own_files()

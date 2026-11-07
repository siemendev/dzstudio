from lib import *

PR = 'mapgroupproto.xml'
PO = 'mapgrouppos.xml'
T2, T4, T6 = '\t\t', '\t\t\t\t', '\t\t\t\t\t\t'

def proto(groups, eol='\n'):
    t = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<prototype>\n' + T2 + '<defaults>\n' + T4 + \
        '<default name="group" lootmax="6" />\n' + T4 + '<default name="container" lootmax="4" />\n' + T2 + '</defaults>\n' + \
        ''.join(groups) + '</prototype>\n'
    return t.replace('\n', eol)

def group(name, lootmax, body):
    lm = ' lootmax="%s"' % lootmax if lootmax is not None else ''
    return T2 + '<group name="%s"%s>\n' % (name, lm) + body + T2 + '</group>\n'

def cont(name, lootmax, body):
    lm = ' lootmax="%s"' % lootmax if lootmax is not None else ''
    return T4 + '<container name="%s"%s>\n' % (name, lm) + body + T4 + '</container>\n'

def pt(pos, rng="0.5", h="1", flags=None):
    f = ' flags="%s"' % flags if flags else ''
    return T6 + '<point pos="%s" range="%s" height="%s"%s />\n' % (pos, rng, h, f)

def li(elem, name, ind=T6):
    return ind + '<%s name="%s" />\n' % (elem, name)

def V(*xs):
    return [N(str(x)) for x in xs]

SHED = group("Land_Shed_M1", 2, li("usage", "Industrial", T4) + li("usage", "Farm", T4) + cont("lootFloor", 2,
    li("category", "tools") + li("category", "containers") + li("tag", "floor") + li("tag", "ground") +
    pt("-0.238281 -1.174019 -0.895996", "0.497570", "1.243926", 32) +
    pt("0.504883 -1.174019 0.807373", "0.587867", "1.469666", 32) +
    pt("0.852539 -1.174019 -0.850342", "0.595474", "1.488685", 32)))
GARAGE = group("Land_Garage_Row", None, cont("lootFloor", None, pt("1 0 1")) +
    T4 + '<dispatch>\n' + T6 + '<proxy type="Wheel" pos="0 0 0" rpy="0 0 0" />\n' + T4 + '</dispatch>\n')
DUP1 = group("Land_Dup", 1, cont("lootFloor", 1, pt("0 0 0")))
DUP2 = group("land_dup", 9, cont("lootFloor", 1, pt("0 0 0")))
GONE = group("Land_Gone", 1, cont("lootFloor", 1, pt("0 0 0")))

def loot_types():
    base = proto([SHED, GARAGE, DUP1, DUP2, GONE])
    LT, LC, LP = "lootType", "lootContainer", "lootPoint"
    SH = {"name": "Land_Shed_M1"}
    SC = {"group": "Land_Shed_M1", "name": "lootFloor"}
    def P(*xs, g="Land_Shed_M1", c="lootFloor"):
        return {"group": g, "container": c, "pos": V(*xs)}
    changes = [
        S({"name": "land_shed_m1"}, "lootmax", N("2"), N("4"), kind=LT),
        S(SH, "usage", fp(["name=Industrial", "name=Farm"]), ["Industrial", "Town"], kind=LT),
        S({"name": "Land_Garage_Row"}, "lootmax", None, N("3"), kind=LT),
        S({"name": "Land_Garage_Row"}, "usage", None, ["Industrial"], kind=LT),
        S({"name": "LAND_DUP"}, "lootmax", N("1"), None, kind=LT),
        R({"name": "Land_Gone"}, kind=LT),
        R({"name": "Land_Never"}, kind=LT),
        S(SC, "lootmax", N("2"), N("3"), kind=LC),
        S({"group": "Land_Shed_M1", "name": "lootfloor"}, "lootmax", None, N("1"), kind=LC),
        S(SC, "tag", fp(["name=floor", "name=ground"]), ["floor"], kind=LC),
        S(P("0.504883", "-1.174019", "0.807373"), "range", N("0.587867"), N("0.7"), kind=LP),
        S(P("0.51", "-1.174019", "0.807373"), "height", None, N("2"), kind=LP),
        S(P("0.52", "-1.174019", "0.807373"), "height", None, N("3"), kind=LP),
        R(P("-0.238281", "-1.174019", "-0.895996"), kind=LP),
        S({"group": "Land_Shed_M1", "container": "lootFloor", "pos": "0.852539 -1.174019 -0.850342"}, "pos", None, V("0.9", "-1.174019", "-0.85"), kind=LP),
        S(P(1, 0, 1, g="Land_Garage_Row"), "flags", None, N("16"), kind=LP),
        S(P(1, 0, 1, g="Land_Garage_Row"), "flags", None, "x", kind=LP),
    ]
    blocks = [
        {"id": "b-c", "kind": LC, "parent": {"group": "Land_Garage_Row"}, "data": {"name": "lootShelves", "lootmax": 2, "category": ["tools"], "tag": ["shelves"]}},
        {"id": "b-c-p1", "kind": LP, "parent": "b-c", "data": {"pos": V(0, 1, 0), "range": N("0.5"), "height": 1, "flags": 32}},
        {"id": "b-c-p2", "kind": LP, "parent": "b-c", "data": {"pos": V("0.005", 1, 0), "range": N("0.5"), "height": 1}},
        {"id": "b-p", "kind": LP, "parent": {"group": "Land_Shed_M1", "container": "lootFloor"}, "data": {"pos": V(1, 1, 1), "range": N("0.3"), "height": N("0.5")}},
        {"id": "b-p2", "kind": LP, "parent": {"group": "Land_Shed_M1", "container": "lootFloor"}, "data": {"pos": V("0.504883", "-1.174019", "0.807373"), "range": 1, "height": 1}},
        {"id": "b-t", "kind": LT, "parent": None, "data": {"name": "Land_New_Shed", "lootmax": 3, "usage": ["Farm"]}},
        {"id": "b-t-c-p", "kind": LP, "parent": "b-t-c", "data": {"pos": V(0, 0, 0), "range": N("0.5"), "height": 1}},
        {"id": "b-t-c", "kind": LC, "parent": "b-t", "data": {"name": "lootFloor", "lootmax": 2, "category": ["tools"], "tag": ["floor"]}},
        {"id": "b-t2", "kind": LT, "parent": None, "data": {"name": "LAND_SHED_M1"}},
        {"id": "b-x", "kind": LC, "parent": {"group": "Land_Missing"}, "data": {"name": "lootFloor"}},
        {"id": "b-y", "kind": LP, "parent": {"group": "Land_Shed_M1"}, "data": {"pos": V(5, 5, 5), "range": 1, "height": 1}},
    ]
    shed = group("Land_Shed_M1", 4, li("usage", "Industrial", T4) + li("usage", "Town", T4) + cont("lootFloor", 3,
        li("category", "tools") + li("category", "containers") + li("tag", "floor") +
        pt("0.504883 -1.174019 0.807373", "0.700000", "2.000000", 32) +
        pt("0.900000 -1.174019 -0.850000", "0.595474", "1.488685", 32) +
        T6 + '<point pos="1.000000 1.000000 1.000000" range="0.300000" height="0.500000" />\n'))
    garage = T2 + '<group name="Land_Garage_Row" lootmax="3">\n' + li("usage", "Industrial", T4) + \
        cont("lootFloor", None, pt("1 0 1") .replace(' />', ' flags="16" />')) + \
        T4 + '<container name="lootShelves" lootmax="2">\n' + li("category", "tools") + li("tag", "shelves") + \
        T6 + '<point pos="0.000000 1.000000 0.000000" range="0.500000" height="1.000000" flags="32" />\n' + T4 + '</container>\n' + \
        T4 + '<dispatch>\n' + T6 + '<proxy type="Wheel" pos="0 0 0" rpy="0 0 0" />\n' + T4 + '</dispatch>\n' + T2 + '</group>\n'
    new = T2 + '<group name="Land_New_Shed" lootmax="3">\n' + li("usage", "Farm", T4) + \
        T4 + '<container name="lootFloor" lootmax="2">\n' + li("category", "tools") + li("tag", "floor") + \
        T6 + '<point pos="0.000000 0.000000 0.000000" range="0.500000" height="1.000000" />\n' + T4 + '</container>\n' + T2 + '</group>\n'
    out = proto([shed, garage, group("Land_Dup", None, cont("lootFloor", 1, pt("0 0 0"))), DUP2, new])
    st = ["applied", "applied", "applied", "applied", "applied", "applied", "gone", "applied", "orphaned", "applied",
          "applied", "applied", "orphaned", "applied", "applied", "applied", "invalid"]
    entries = []
    for i, c in enumerate(changes):
        d = None
        if i == 8:
            d = "case-mismatch"
        if i == 16:
            d = "bad-value"
        entries.append(ch(i, c["op"], c["block"], c["key"], st[i], PR, c.get("field"), detail=d))
    entries += [
        bl(0, LC, "b-c", "applied", PR), bl(1, LP, "b-c-p1", "applied", PR), bl(2, LP, "b-c-p2", "collision", PR),
        bl(3, LP, "b-p", "applied", PR), bl(4, LP, "b-p2", "collision", PR), bl(5, LT, "b-t", "applied", PR),
        bl(6, LP, "b-t-c-p", "applied", PR), bl(7, LC, "b-t-c", "applied", PR), bl(8, LT, "b-t2", "collision", PR),
        bl(9, LC, "b-x", "orphaned", PR, "parent-missing"), bl(10, LP, "b-y", "invalid", PR, "bad-parent"),
    ]
    case("loot-types", {PR: base}, {"changes": changes, "blocks": blocks}, {PR: out},
         {"entries": entries, "files": [fl(PR, "written")]})

def loot_duplicate_containers():
    g = group("Land_Smokestack_Medium", 5, li("usage", "Industrial", T4) +
              cont("lootFloor", 2, li("category", "tools") + pt("1 0 1")) +
              cont("lootFloor", 3, li("category", "food") + pt("2 0 2") + pt("2 0 2")))
    base = proto([g], '\r\n')
    LC, LP = "lootContainer", "lootPoint"
    G = "Land_Smokestack_Medium"
    changes = [
        S({"group": G, "name": "lootFloor"}, "lootmax", None, N("4"), kind=LC),
        S({"group": G, "name": "lootFloor", "ordinal": N("2")}, "lootmax", N("3"), N("4"), kind=LC),
        S({"group": G, "container": "lootFloor", "pos": V(1, 0, 1)}, "range", None, N("0.6"), kind=LP),
        S({"group": G, "container": "lootFloor", "containerOrdinal": N("1"), "pos": V(1, 0, 1)}, "range", N("0.5"), N("0.6"), kind=LP),
        S({"group": G, "container": "lootFloor", "containerOrdinal": N("2"), "pos": V(2, 0, 2)}, "height", None, N("2"), kind=LP),
        R({"group": G, "container": "lootFloor", "containerOrdinal": N("2"), "pos": V(2, 0, 2), "ordinal": N("2")}, kind=LP),
    ]
    blocks = [
        {"id": "b-p", "kind": LP, "parent": {"group": G, "container": "lootFloor", "containerOrdinal": N("2")}, "data": {"pos": V(3, 0, 3), "range": N("0.5"), "height": 1}},
        {"id": "b-q", "kind": LP, "parent": {"group": G, "container": "lootFloor"}, "data": {"pos": V(4, 0, 4), "range": N("0.5"), "height": 1}},
    ]
    g2 = group(G, 5, li("usage", "Industrial", T4) +
               cont("lootFloor", 2, li("category", "tools") + pt("1 0 1", "0.600000")) +
               cont("lootFloor", 4, li("category", "food") + pt("2 0 2") +
                    T6 + '<point pos="3.000000 0.000000 3.000000" range="0.500000" height="1.000000" />\n'))
    out = proto([g2], '\r\n')
    st = ["ambiguous", "applied", "ambiguous", "applied", "ambiguous", "applied"]
    entries = [ch(i, c["op"], c["block"], c["key"], st[i], PR, c.get("field")) for i, c in enumerate(changes)]
    entries += [bl(0, LP, "b-p", "applied", PR), bl(1, LP, "b-q", "ambiguous", PR)]
    case("loot-duplicate-containers-crlf", {PR: base}, {"changes": changes, "blocks": blocks}, {PR: out},
         {"entries": entries, "files": [fl(PR, "written")]})

def row(name, pos, rpy, a):
    return '    <group name="%s" pos="%s" rpy="%s" a="%s" />\n' % (name, pos, rpy, a)

POHEAD = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>\n<map>\n'

def loot_rows():
    rows = [
        row("Land_Misc_FeedShack", "80.262581 113.793480 4422.178223", "-0.000000 0.000000 -70.013718", "160.013718"),
        row("Land_wreck_truck01_aban1_blue", "100.000000 5.000000 200.000000", "0.000000 0.000000 10.000000", "80.000000"),
        row("Land_House_1M4_Dam2", "2946.913330 421.467346 6332.023926", "0.000000 0.000000 0.000000", "90.000000"),
        row("Land_House_1M4_Dam2", "2946.913330 421.467346 6332.023926", "0.000000 0.000000 0.000000", "90.000000"),
        row("Land_Tenement_Small_Brown", "500.000000 10.000000 500.000000", "0 0 0", "90"),
        row("Land_Tenement_Small_Brown_Ghost1", "500.000000 10.000000 500.000000", "0 0 0", "90"),
    ]
    base = POHEAD + ''.join(rows) + '</map>\n'
    LR = "lootRow"
    FS = {"name": "Land_Misc_FeedShack", "pos": V("80.262581", "113.793480", "4422.178223")}
    HD = {"name": "Land_House_1M4_Dam2", "pos": V("2946.91333", "421.467346", "6332.023926")}
    changes = [
        S({"name": "Land_Wreck_Truck01_aban1_blue", "pos": V(100, 5, 200)}, "a", N("80"), N("81"), kind=LR),
        S(FS, "pos", V("80.262581", "113.79348", "4422.178223"), V("80.3", "113.79348", "4422.178223"), kind=LR),
        S(HD, "a", None, N("1"), kind=LR),
        R(HD, kind=LR),
        R({"name": "Land_Tenement_Small_Brown", "pos": V(500, 10, 500)}, kind=LR),
        R({"name": "Land_Tenement_Small_Brown", "pos": V("500.02", 10, 500)}, kind=LR),
        S({"name": "Land_Misc_FeedShack", "pos": V("80.27", "113.79348", "4422.178223")}, "a", N("160.013718"), N("160.013718"), kind=LR),
        S({"name": "Land_Misc_FeedShack"}, "a", None, N("1"), kind=LR),
    ]
    blocks = [
        {"id": "b-r", "kind": LR, "parent": None, "data": {"name": "Land_Container_1Bo", "pos": V(4960, "9.5", 2500), "rpy": V(0, 0, 90), "a": 0}},
        {"id": "b-r2", "kind": LR, "parent": None, "data": {"name": "Land_Container_1Bo", "pos": V("4960.005", "9.5", 2500), "rpy": V(0, 0, 90), "a": 0}},
        {"id": "b-r3", "kind": LR, "parent": None, "data": {"name": "land_wreck_truck01_aban1_BLUE", "pos": V(100, 5, 200), "rpy": V(0, 0, 0), "a": 0}},
        {"id": "b-r4", "kind": LR, "parent": None, "data": {"name": "Land_Container_1Bo", "pos": V(1, 1, 1), "rpy": V(0, 0, 0)}},
        {"id": "b-r5", "kind": LR, "parent": None, "data": {"name": "Land_Container_1Bo", "pos": V(1, 1), "rpy": V(0, 0, 0), "a": 0}},
    ]
    out = POHEAD + ''.join([
        row("Land_Misc_FeedShack", "80.300000 113.793480 4422.178223", "-0.000000 0.000000 -70.013718", "160.013718"),
        row("Land_wreck_truck01_aban1_blue", "100.000000 5.000000 200.000000", "0.000000 0.000000 10.000000", "81.000000"),
        rows[5],
        row("Land_Container_1Bo", "4960.000000 9.500000 2500.000000", "0.000000 0.000000 90.000000", "0.000000"),
    ]) + '</map>\n'
    st = ["applied", "applied", "ambiguous", "applied", "applied", "gone", "already", "invalid"]
    entries = []
    for i, c in enumerate(changes):
        entries.append(ch(i, c["op"], LR, c["key"], st[i], PO, c.get("field"), detail="bad-key" if i == 7 else None))
    entries += [bl(0, LR, "b-r", "applied", PO), bl(1, LR, "b-r2", "collision", PO), bl(2, LR, "b-r3", "collision", PO),
                bl(3, LR, "b-r4", "invalid", PO, "incomplete"), bl(4, LR, "b-r5", "invalid", PO, "bad-value")]
    case("loot-rows", {PO: base}, {"changes": changes, "blocks": blocks}, {PO: out},
         {"entries": entries, "files": [fl(PO, "written")]})

def run():
    loot_types()
    loot_duplicate_containers()
    loot_rows()

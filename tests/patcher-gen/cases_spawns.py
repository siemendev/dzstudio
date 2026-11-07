from lib import *
from cases_events import EV, HEAD, TAIL

SP = 'cfgeventspawns.xml'
GR = 'cfgeventgroups.xml'
SPHEAD = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>\n<eventposdef>\n'
SPTAIL = '</eventposdef>\n'

def pk(event, x, z, **kw):
    k = {"event": event, "x": N(str(x)), "z": N(str(z))}
    k.update(kw)
    return k

def spawn_tolerance():
    base = SPHEAD + '''\t<event name="VehicleA">
\t\t<zone smin="1" smax="3" dmin="3" dmax="5" r="45" />
\t\t<pos x="341" z="1000" a="10" />
\t\t<pos x="500.000000" z="500.000000" />
\t</event>
\t<event name="VehicleB">
\t\t<pos x="341.000000" z="1000.000000" a="10.000000" />
\t</event>
''' + SPTAIL
    ka = pk("VehicleA", "341.000000", 1000)
    changes = [
        S(ka, "pos", {"x": N("341"), "z": N("1000"), "a": N("10")}, {"x": N("342"), "z": N("1000"), "a": N("20")}, kind="spawnPosition"),
        S(pk("VehicleA", "500.499", 500), "a", None, N("45"), kind="spawnPosition"),
        S(pk("VehicleA", "500.5", 500), "a", N("45"), N("46"), kind="spawnPosition"),
        S(pk("VehicleA", "500.501", 500), "a", None, N("1"), kind="spawnPosition"),
        S(pk("VehicleA", 500, "499.5"), "a", N("46"), N("46"), kind="spawnPosition"),
        R(pk("VehicleB", 341, 1000), kind="spawnPosition"),
        S(pk("VehicleB", 341, 1000), "a", None, N("1"), kind="spawnPosition"),
        S({"name": "VehicleA"}, "zone.dmax", N("5"), N("7"), kind="spawnList"),
        S({"name": "VehicleB"}, "zone.r", None, N("30.0"), kind="spawnList"),
    ]
    out = SPHEAD + '''\t<event name="VehicleA">
\t\t<zone smin="1" smax="3" dmin="3" dmax="7" r="45" />
\t\t<pos x="342.000000" z="1000" a="20.000000" />
\t\t<pos x="500.000000" z="500.000000" a="46.000000" />
\t</event>
\t<event name="VehicleB">
\t\t<zone r="30" />
\t</event>
''' + SPTAIL
    entries = [
        ch(0, "set", "spawnPosition", ka, "applied", SP, "pos"),
        ch(1, "set", "spawnPosition", changes[1]["key"], "applied", SP, "a"),
        ch(2, "set", "spawnPosition", changes[2]["key"], "applied", SP, "a"),
        ch(3, "set", "spawnPosition", changes[3]["key"], "orphaned", SP, "a"),
        ch(4, "set", "spawnPosition", changes[4]["key"], "already", SP, "a"),
        ch(5, "remove", "spawnPosition", changes[5]["key"], "applied", SP),
        ch(6, "set", "spawnPosition", changes[6]["key"], "orphaned", SP, "a"),
        ch(7, "set", "spawnList", {"name": "VehicleA"}, "applied", SP, "zone.dmax"),
        ch(8, "set", "spawnList", {"name": "VehicleB"}, "applied", SP, "zone.r"),
    ]
    case("spawn-tolerance", {SP: base}, {"changes": changes}, {SP: out},
         {"entries": entries, "files": [fl(SP, "written")]})

def spawn_ordinal():
    base = SPHEAD + '''\t<event name="VehicleC">
\t\t<pos x="100" z="100" a="0" />
\t\t<pos x="100" z="100" a="0" />
\t\t<pos x="100.2" z="100" a="0" />
\t</event>
\t<event name="VehicleD">
\t\t<pos x="5" z="5" a="1" />
\t</event>
\t<event name="VehicleD">
\t\t<pos x="5" z="5" a="2" />
\t</event>
''' + SPTAIL
    c = lambda **kw: pk("VehicleC", 100, 100, **kw)
    changes = [
        S(c(), "a", None, N("9"), kind="spawnPosition"),
        R(c(ordinal=N("2")), kind="spawnPosition"),
        S(c(ordinal=N("2")), "a", None, N("9"), kind="spawnPosition"),
        S(c(ordinal=N("3")), "a", N("0"), N("90"), kind="spawnPosition"),
        R(c(ordinal=N("4")), kind="spawnPosition"),
        S(pk("VehicleD", 5, 5), "a", None, N("3"), kind="spawnPosition"),
        R(pk("VehicleD", 5, 5, ordinal=N("2")), kind="spawnPosition"),
        R(c(), kind="spawnPosition"),
    ]
    out = SPHEAD + '''\t<event name="VehicleC">
\t\t<pos x="100" z="100" a="0" />
\t\t<pos x="100.2" z="100" a="90.000000" />
\t</event>
\t<event name="VehicleD">
\t\t<pos x="5" z="5" a="1" />
\t</event>
\t<event name="VehicleD">
\t</event>
''' + SPTAIL
    st = ["ambiguous", "applied", "orphaned", "applied", "gone", "ambiguous", "applied", "ambiguous"]
    entries = []
    for i, chg in enumerate(changes):
        entries.append(ch(i, chg["op"], "spawnPosition", chg["key"], st[i], SP, chg.get("field")))
    case("spawn-ordinal", {SP: base}, {"changes": changes}, {SP: out},
         {"entries": entries, "files": [fl(SP, "written")]})

def spawn_own_selfclose():
    base = SPHEAD + '''\t<event name="VehicleE" />
\t<event name="InfectedVillage" />
\t<event name="StaticTrain">
\t\t<pos x="5587.466" z="2063.353" a="0" y="7.75" group="Train_A"/>
\t</event>
''' + SPTAIL
    blocks = [
        {"id": "b-e1", "kind": "spawnPosition", "parent": {"event": "VehicleE"}, "data": {"x": 10, "z": 20, "a": 30}},
        {"id": "b-e2", "kind": "spawnPosition", "parent": {"event": "VehicleE"}, "data": {"x": N("10.4"), "z": 20}},
        {"id": "b-e3", "kind": "spawnPosition", "parent": {"event": "VehicleE"}, "data": {"x": 11, "z": 20}},
        {"id": "b-g", "kind": "spawnPosition", "parent": {"event": "StaticTrain"}, "data": {"x": 100, "z": 200, "y": 5, "group": "Train_A"}},
        {"id": "b-g2", "kind": "spawnPosition", "parent": {"event": "StaticTrain"}, "data": {"x": 300, "z": 200, "group": "Train_A"}},
        {"id": "b-inf", "kind": "spawnPosition", "parent": {"event": "InfectedVillage"}, "data": {"x": 1, "z": 1}},
        {"id": "b-new", "kind": "spawnPosition", "parent": {"event": "VehicleNew"}, "data": {"x": 1, "z": 2}},
        {"id": "b-bad", "kind": "spawnPosition", "parent": {}, "data": {"x": 1, "z": 2}},
        {"id": "b-unk", "kind": "spawnPosition", "parent": {"event": "VehicleE"}, "data": {"x": 50, "z": 50, "b": 1}},
    ]
    out = SPHEAD + '''\t<event name="VehicleE">
\t\t<pos x="10.000000" z="20.000000" a="30.000000" />
\t\t<pos x="11.000000" z="20.000000" />
\t</event>
\t<event name="InfectedVillage" />
\t<event name="StaticTrain">
\t\t<pos x="5587.466" z="2063.353" a="0" y="7.75" group="Train_A"/>
\t\t<pos x="100.000000" z="200.000000" a="0.000000" y="5.000000" group="Train_A" />
\t</event>
\t<event name="VehicleNew">
\t\t<pos x="1.000000" z="2.000000" />
\t</event>
''' + SPTAIL
    entries = [
        bl(0, "spawnPosition", "b-e1", "applied", SP),
        bl(1, "spawnPosition", "b-e2", "collision", SP),
        bl(2, "spawnPosition", "b-e3", "applied", SP),
        bl(3, "spawnPosition", "b-g", "applied", SP),
        bl(4, "spawnPosition", "b-g2", "invalid", SP, "group-needs-y"),
        bl(5, "spawnPosition", "b-inf", "invalid", SP, "no-positions"),
        bl(6, "spawnPosition", "b-new", "applied", SP),
        bl(7, "spawnPosition", "b-bad", "invalid", SP, "bad-parent"),
        bl(8, "spawnPosition", "b-unk", "invalid", SP, "unknown-field"),
    ]
    case("spawn-own-selfclosing", {SP: base}, {"blocks": blocks}, {SP: out},
         {"entries": entries, "files": [fl(SP, "written")]})

def commented():
    vx = '''    <event name="VehicleX">
        <nominal>2</nominal>
        <min>1</min>
        <max>2</max>
        <lifetime>300</lifetime>
        <restock>0</restock>
        <saferadius>500</saferadius>
        <distanceradius>500</distanceradius>
        <cleanupradius>200</cleanupradius>
        <flags deletable="0" init_random="0" remove_damaged="1"/>
        <position>fixed</position>
        <limit>child</limit>
        <active>1</active>
        <children>
            <!--<child lootmax="0" lootmin="0" max="1" min="1" type="Foo"/>-->
            <child lootmax="0" lootmin="0" max="1" min="1" type="Bar"/>
        </children>
    </event>
'''
    old = '''    <!-- <event name="VehicleOld">
        <nominal>3</nominal>
    </event> -->
'''
    base = HEAD + old + vx + TAIL
    sp = SPHEAD + '''\t<event name="VehicleX">
\t\t<!--<pos x="10" z="10" a="0" />-->
\t\t<pos x="20" z="20" a="0" />
\t</event>
''' + SPTAIL
    from cases_events import own_event_data
    changes = [
        S({"name": "VehicleOld"}, "nominal", None, N("4")),
        R({"event": "VehicleX", "type": "Foo"}, kind="eventChild"),
        S({"event": "VehicleX", "type": "bar"}, "max", N("1"), N("2"), kind="eventChild"),
        R(pk("VehicleX", 10, 10), kind="spawnPosition"),
    ]
    blocks = [
        {"id": "b-old", "kind": "event", "parent": None, "data": own_event_data("VehicleOld")},
        {"id": "b-foo", "kind": "eventChild", "parent": {"event": "VehicleX"}, "data": {"type": "Foo", "min": 1, "max": 1, "lootmin": 0, "lootmax": 0}},
        {"id": "b-p", "kind": "spawnPosition", "parent": {"event": "VehicleX"}, "data": {"x": 10, "z": 10}},
    ]
    out = rep(base, 'max="1" min="1" type="Bar"/>\n', 'max="2" min="1" type="Bar"/>\n            <child lootmax="0" lootmin="0" max="1" min="1" type="Foo"/>\n')
    out = rep(out, TAIL, '''    <event name="VehicleOld">
        <nominal>1</nominal>
        <min>1</min>
        <max>1</max>
        <lifetime>300</lifetime>
        <restock>0</restock>
        <saferadius>500</saferadius>
        <distanceradius>500</distanceradius>
        <cleanupradius>200</cleanupradius>
        <flags deletable="0" init_random="0" remove_damaged="1"/>
        <position>fixed</position>
        <limit>child</limit>
        <active>1</active>
        <children/>
    </event>
''' + TAIL)
    sp_out = rep(sp, '\t\t<pos x="20" z="20" a="0" />\n', '\t\t<pos x="20" z="20" a="0" />\n\t\t<pos x="10.000000" z="10.000000" />\n')
    entries = [
        ch(0, "set", "event", {"name": "VehicleOld"}, "orphaned", EV, "nominal"),
        ch(1, "remove", "eventChild", {"event": "VehicleX", "type": "Foo"}, "gone", EV),
        ch(2, "set", "eventChild", {"event": "VehicleX", "type": "bar"}, "applied", EV, "max"),
        ch(3, "remove", "spawnPosition", changes[3]["key"], "gone", SP),
        bl(0, "event", "b-old", "applied", EV),
        bl(1, "eventChild", "b-foo", "applied", EV),
        bl(2, "spawnPosition", "b-p", "applied", SP),
    ]
    case("commented-out", {EV: base, SP: sp}, {"changes": changes, "blocks": blocks}, {EV: out, SP: sp_out},
         {"entries": entries, "files": [fl(SP, "written"), fl(EV, "written")]})

GRHEAD = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<eventgroupdef>\n'

def event_groups():
    base = GRHEAD + '''    <!--pos x="5587.466" z="2063.353" a="0" y="7.591" group="Train_A"/-->
    <group name="Train_A">
        <child type="Wagon_Red" deloot="0" lootmax="3" lootmin="1" x="0" z="0" a="78.123" y="1.9"/>
        <child type="Wagon_Box" deloot="0" lootmax="3" lootmin="2" x="12.085" z="2.740" a="256.739" y="1.789"/>
        <child type="Wagon_Box" deloot="0" lootmax="3" lootmin="2" x="12.2" z="2.740" a="10" y="1.789"/>
    </group>
    <group name="Train_B">
        <child type="Wagon_Box" deloot="0" lootmax="3" lootmin="2" x="12.085" z="2.740" a="256.739" y="1.789"/>
    </group>
</eventgroupdef>
'''
    def gk(group, type, x, z, **kw):
        k = {"group": group, "type": type, "x": N(str(x)), "z": N(str(z))}
        k.update(kw)
        return k
    G = "eventGroupChild"
    changes = [
        S(gk("train_a", "wagon_box", "12.085", "2.74"), "a", None, N("1"), kind=G),
        S(gk("Train_A", "Wagon_Box", "12.0", "2.74", ordinal=N("1")), "lootmax", N("3"), N("4"), kind=G),
        S(gk("Train_A", "WAGON_BOX", "12.336", "2.74"), "a", N("10"), N("15"), kind=G),
        R(gk("Train_B", "Wagon_Box", "12.085", "2.740"), kind=G),
        R({"name": "TRAIN_B"}, kind="eventGroup"),
        S(gk("Train_C", "Wagon_Box", 0, 0), "a", None, N("1"), kind=G),
        S(gk("Train_A", "Wagon_Red", "0.25", 0), "y", N("1.9"), N("2"), kind=G),
        S(gk("Train_A", "Wagon_Red", "0.250001", 0), "y", None, N("3"), kind=G),
    ]
    blocks = [
        {"id": "b-grp", "kind": "eventGroup", "parent": None, "data": {"name": "Ruins_1"}},
        {"id": "b-grp-c1", "kind": G, "parent": "b-grp", "data": {"type": "Land_Ruin", "x": 0, "z": 0, "a": 0, "y": 0}},
        {"id": "b-grp-c2", "kind": G, "parent": "b-grp", "data": {"type": "land_ruin", "x": N("0.1"), "z": 0, "a": 90}},
        {"id": "b-add", "kind": G, "parent": {"group": "train_a"}, "data": {"type": "Wagon_Red", "x": N("0.2"), "z": 0, "a": 0}},
        {"id": "b-add2", "kind": G, "parent": {"group": "Train_A"}, "data": {"type": "Wagon_Tank", "deloot": 0, "lootmax": 3, "lootmin": 1, "x": 80, "z": 20, "a": 250, "y": N("1.5")}},
        {"id": "b-dup", "kind": "eventGroup", "parent": None, "data": {"name": "train_a"}},
    ]
    out = GRHEAD + '''    <!--pos x="5587.466" z="2063.353" a="0" y="7.591" group="Train_A"/-->
    <group name="Train_A">
        <child type="Wagon_Red" deloot="0" lootmax="3" lootmin="1" x="0" z="0" a="78.123" y="2.000000"/>
        <child type="Wagon_Box" deloot="0" lootmax="4" lootmin="2" x="12.085" z="2.740" a="256.739" y="1.789"/>
        <child type="Wagon_Box" deloot="0" lootmax="3" lootmin="2" x="12.2" z="2.740" a="15.000000" y="1.789"/>
        <child type="Wagon_Tank" deloot="0" lootmax="3" lootmin="1" x="80.000000" z="20.000000" a="250.000000" y="1.500000"/>
    </group>
    <group name="Ruins_1">
        <child type="Land_Ruin" x="0.000000" z="0.000000" a="0.000000" y="0.000000"/>
    </group>
</eventgroupdef>
'''
    st = ["ambiguous", "applied", "applied", "applied", "applied", "orphaned", "applied", "orphaned"]
    entries = [ch(i, c["op"], c["block"], c["key"], st[i], GR, c.get("field")) for i, c in enumerate(changes)]
    entries += [
        bl(0, "eventGroup", "b-grp", "applied", GR),
        bl(1, G, "b-grp-c1", "applied", GR),
        bl(2, G, "b-grp-c2", "collision", GR),
        bl(3, G, "b-add", "collision", GR),
        bl(4, G, "b-add2", "applied", GR),
        bl(5, "eventGroup", "b-dup", "collision", GR),
    ]
    case("event-groups", {GR: base}, {"changes": changes, "blocks": blocks}, {GR: out},
         {"entries": entries, "files": [fl(GR, "written")]})

def run():
    spawn_tolerance()
    spawn_ordinal()
    spawn_own_selfclose()
    commented()
    event_groups()

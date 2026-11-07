from lib import *

EV = 'db/events.xml'
SEDAN = '''    <event name="VehicleCivilianSedan">
        <nominal>8</nominal>
        <min>5</min>
        <max>11</max>
        <lifetime>300</lifetime>
        <restock>0</restock>
        <saferadius>500</saferadius>
        <distanceradius>500</distanceradius>
        <cleanupradius>200</cleanupradius>
        <flags deletable="0" init_random="0" remove_damaged="1"/>
        <position>fixed</position>
        <limit>mixed</limit>
        <active>1</active>
        <children>
            <child lootmax="0" lootmin="0" max="5" min="3" type="CivilianSedan"/>
            <child lootmax="0" lootmin="0" max="5" min="3" type="CivilianSedan_Black"/>
            <child lootmax="0" lootmin="0" max="5" min="3" type="CivilianSedan_Wine"/>
        </children>
    </event>
'''
HELI = '''    <event name="StaticHeliCrash">
        <nominal>3</nominal>
        <min>1</min>
        <max>3</max>
        <lifetime>2100</lifetime>
        <restock>0</restock>
        <saferadius>1000</saferadius>
        <distanceradius>1000</distanceradius>
        <cleanupradius>1000</cleanupradius>
        <flags deletable="1" init_random="0" remove_damaged="0"/>
        <position>fixed</position>
        <limit>child</limit>
        <active>1</active>
        <children>
            <child lootmax="15" lootmin="10" max="3" min="1" type="Wreck_UH1Y"/>
        </children>
    </event>
'''
HEAD = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<events>\n'
TAIL = '</events>\n'
SK = {"name": "VehicleCivilianSedan"}

def events_fields():
    base = HEAD + SEDAN + HELI + TAIL
    changes = [
        S(SK, "nominal", N("8"), N("12")),
        S(SK, "max", N("11"), N("11")),
        S(SK, "flags.remove_damaged", N("1"), N("0")),
        S({"event": "VehicleCivilianSedan", "type": "civiliansedan_black"}, "max", N("5"), N("2"), kind="eventChild"),
        R({"event": "VehicleCivilianSedan", "type": "CivilianSedan_Wine"}, kind="eventChild"),
        S({"name": "StaticHeliCrash"}, "secondary", None, "InfectedArmy"),
        S(SK, "nominal", N("12"), N("14")),
        S(SK, "lifetime", None, "abc"),
        S(SK, "bogus", None, N("1")),
        S(SK, "flags.deletable", None, True),
    ]
    out = base
    out = rep(out, '<nominal>8</nominal>', '<nominal>14</nominal>')
    out = rep(out, 'remove_damaged="1"/>', 'remove_damaged="0"/>')
    out = rep(out, 'max="5" min="3" type="CivilianSedan_Black"', 'max="2" min="3" type="CivilianSedan_Black"')
    out = rep(out, '            <child lootmax="0" lootmin="0" max="5" min="3" type="CivilianSedan_Wine"/>\n', '')
    out = rep(out, '        <cleanupradius>1000</cleanupradius>\n', '        <cleanupradius>1000</cleanupradius>\n        <secondary>InfectedArmy</secondary>\n')
    out = rep(out, 'deletable="0" init_random="0" remove_damaged="0"/>', 'deletable="1" init_random="0" remove_damaged="0"/>')
    entries = [
        ch(0, "set", "event", SK, "applied", EV, "nominal"),
        ch(1, "set", "event", SK, "already", EV, "max"),
        ch(2, "set", "event", SK, "applied", EV, "flags.remove_damaged"),
        ch(3, "set", "eventChild", {"event": "VehicleCivilianSedan", "type": "civiliansedan_black"}, "applied", EV, "max"),
        ch(4, "remove", "eventChild", {"event": "VehicleCivilianSedan", "type": "CivilianSedan_Wine"}, "applied", EV),
        ch(5, "set", "event", {"name": "StaticHeliCrash"}, "applied", EV, "secondary"),
        ch(6, "set", "event", SK, "applied", EV, "nominal"),
        ch(7, "set", "event", SK, "invalid", EV, "lifetime", detail="bad-value"),
        ch(8, "set", "event", SK, "invalid", EV, "bogus", detail="unknown-field"),
        ch(9, "set", "event", SK, "applied", EV, "flags.deletable"),
    ]
    case("event-fields", {EV: base}, {"changes": changes}, {EV: out},
         {"entries": entries, "files": [fl(EV, "written")]})

def events_case_overrode():
    base = HEAD + SEDAN + TAIL
    changes = [
        S({"name": "vehicleCivilianSedan"}, "nominal", N("8"), N("9")),
        R({"name": "VEHICLECIVILIANSEDAN"}),
        S(SK, "nominal", N("6"), N("10")),
        R({"name": "VehicleMissing"}),
        S({"name": "VehicleMissing"}, "nominal", N("1"), N("2")),
        R({"event": "VehicleCivilianSedan", "type": "Nope"}, kind="eventChild"),
        S({"event": "VehicleMissing", "type": "CivilianSedan"}, "max", None, N("1"), kind="eventChild"),
        {"op": "rename", "block": "event", "key": SK},
        S(SK, "nominal", None, N("1"), kind="noSuchKind"),
    ]
    out = rep(base, '<nominal>8</nominal>', '<nominal>10</nominal>')
    entries = [
        ch(0, "set", "event", {"name": "vehicleCivilianSedan"}, "orphaned", EV, "nominal", detail="case-mismatch"),
        ch(1, "remove", "event", {"name": "VEHICLECIVILIANSEDAN"}, "gone", EV, detail="case-mismatch"),
        ch(2, "set", "event", SK, "overrode", EV, "nominal", current="8"),
        ch(3, "remove", "event", {"name": "VehicleMissing"}, "gone", EV),
        ch(4, "set", "event", {"name": "VehicleMissing"}, "orphaned", EV, "nominal"),
        ch(5, "remove", "eventChild", {"event": "VehicleCivilianSedan", "type": "Nope"}, "gone", EV),
        ch(6, "set", "eventChild", {"event": "VehicleMissing", "type": "CivilianSedan"}, "orphaned", EV, "max"),
        ch(7, "rename", "event", SK, "invalid", detail="bad-op"),
        ch(8, "set", "noSuchKind", SK, "invalid", field="nominal", detail="unknown-kind"),
    ]
    case("event-case-overrode-gone", {EV: base}, {"changes": changes}, {EV: out},
         {"entries": entries, "files": [fl(EV, "written")]})

def events_duplicate():
    dup1 = '''    <event name="ItemDup">
        <nominal>2</nominal>
        <min>1</min>
        <max>2</max>
        <lifetime>300</lifetime>
        <restock>0</restock>
        <saferadius>0</saferadius>
        <distanceradius>0</distanceradius>
        <cleanupradius>0</cleanupradius>
        <flags deletable="0" init_random="0" remove_damaged="0"/>
        <position>fixed</position>
        <limit>mixed</limit>
        <active>1</active>
        <children>
            <child lootmax="0" lootmin="0" max="2" min="1" type="Pen_Black"/>
        </children>
    </event>
'''
    dup2 = dup1.replace('Pen_Black', 'Pen_Red').replace('<nominal>2</nominal>', '<nominal>3</nominal>')
    base = HEAD + dup1 + SEDAN + dup2 + TAIL
    k = {"name": "ItemDup"}
    changes = [
        S(k, "nominal", None, N("5")),
        R({"event": "ItemDup", "type": "pen_red"}, kind="eventChild"),
        S({"event": "ItemDup", "type": "PEN_BLACK"}, "min", N("1"), N("2"), kind="eventChild"),
        S(k, "lifetime", N("300"), N("300")),
    ]
    out = base.replace('<nominal>2</nominal>', '<nominal>5</nominal>').replace('<nominal>3</nominal>', '<nominal>5</nominal>')
    out = rep(out, '            <child lootmax="0" lootmin="0" max="2" min="1" type="Pen_Red"/>\n', '')
    out = rep(out, 'max="2" min="1" type="Pen_Black"', 'max="2" min="2" type="Pen_Black"')
    entries = [
        ch(0, "set", "event", k, "applied", EV, "nominal"),
        ch(1, "remove", "eventChild", {"event": "ItemDup", "type": "pen_red"}, "applied", EV),
        ch(2, "set", "eventChild", {"event": "ItemDup", "type": "PEN_BLACK"}, "applied", EV, "min"),
        ch(3, "set", "event", k, "already", EV, "lifetime"),
    ]
    case("event-duplicate-merge", {EV: base}, {"changes": changes}, {EV: out},
         {"entries": entries, "files": [fl(EV, "written")]})

SP = 'cfgeventspawns.xml'
SPHEAD = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>\n<eventposdef>\n'

def own_event_data(name, limit="child", mx=1):
    return {"name": name, "nominal": 1, "min": 1, "max": mx, "lifetime": 300, "restock": 0, "saferadius": 500,
            "distanceradius": 500, "cleanupradius": 200,
            "flags": {"deletable": 0, "init_random": 0, "remove_damaged": 1},
            "position": "fixed", "limit": limit, "active": 1}

def events_own():
    ev = HEAD + SEDAN + TAIL
    sp = SPHEAD + '''\t<event name="VehicleCivilianSedan">
\t\t<pos x="12071.933594" z="9129.989258" a="317.953339" />
\t\t<pos x="12302.375001" z="9051.289062" a="60.399284" />
\t</event>
</eventposdef>
'''
    blocks = [
        {"id": "b-car", "kind": "event", "parent": None, "data": own_event_data("VehicleFixedOffroad")},
        {"id": "b-car-child", "kind": "eventChild", "parent": "b-car", "data": {"type": "OffroadHatchback", "min": 1, "max": 1, "lootmin": 0, "lootmax": 0}},
        {"id": "b-car-pos", "kind": "spawnPosition", "parent": "b-car", "data": {"x": N("7500.5"), "z": N("7600.25"), "a": 90}},
        {"id": "b-dup", "kind": "event", "parent": None, "data": own_event_data("VehicleCivilianSedan")},
        {"id": "b-bad", "kind": "event", "parent": None, "data": own_event_data("FixedCar")},
        {"id": "b-add", "kind": "eventChild", "parent": {"event": "VehicleCivilianSedan"}, "data": {"type": "CivilianSedan_Green", "min": 1, "max": 2, "lootmin": 0, "lootmax": 0}},
        {"id": "b-add2", "kind": "eventChild", "parent": {"event": "VehicleCivilianSedan"}, "data": {"type": "civiliansedan", "min": 1, "max": 2, "lootmin": 0, "lootmax": 0}},
        {"id": "b-anim", "kind": "spawnPosition", "parent": {"event": "AnimalWolf"}, "data": {"x": 1, "z": 2}},
        {"id": "b-inc", "kind": "event", "parent": None, "data": {"name": "VehicleX", "nominal": 1}},
        {"id": "b-near", "kind": "spawnPosition", "parent": {"event": "VehicleCivilianSedan"}, "data": {"x": N("12071.733594"), "z": N("9129.989258")}},
        {"id": "b-mx", "kind": "event", "parent": None, "data": own_event_data("VehicleMixedZero", "mixed", 0)},
        {"id": "b-orph", "kind": "eventChild", "parent": "b-bad", "data": {"type": "X", "min": 1, "max": 1, "lootmin": 0, "lootmax": 0}},
        {"id": "b-car", "kind": "event", "parent": None, "data": own_event_data("VehicleOther")},
        {"id": "b-pos2", "kind": "spawnPosition", "parent": {"event": "VehicleCivilianSedan"}, "data": {"x": 100, "z": N("200.1234567"), "a": N("-0.0000001")}},
    ]
    ev_out = rep(ev, '''            <child lootmax="0" lootmin="0" max="5" min="3" type="CivilianSedan_Wine"/>
''', '''            <child lootmax="0" lootmin="0" max="5" min="3" type="CivilianSedan_Wine"/>
            <child lootmax="0" lootmin="0" max="2" min="1" type="CivilianSedan_Green"/>
''')
    ev_out = rep(ev_out, TAIL, '''    <event name="VehicleFixedOffroad">
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
        <children>
            <child lootmax="0" lootmin="0" max="1" min="1" type="OffroadHatchback"/>
        </children>
    </event>
''' + TAIL)
    sp_out = rep(sp, '''\t\t<pos x="12302.375001" z="9051.289062" a="60.399284" />
''', '''\t\t<pos x="12302.375001" z="9051.289062" a="60.399284" />
\t\t<pos x="100.000000" z="200.123457" a="0.000000" />
''')
    sp_out = rep(sp_out, '</eventposdef>\n', '''\t<event name="VehicleFixedOffroad">
\t\t<pos x="7500.500000" z="7600.250000" a="90.000000" />
\t</event>
</eventposdef>
''')
    entries = [
        bl(0, "event", "b-car", "applied", EV),
        bl(1, "eventChild", "b-car-child", "applied", EV),
        bl(2, "spawnPosition", "b-car-pos", "applied", SP),
        bl(3, "event", "b-dup", "collision", EV),
        bl(4, "event", "b-bad", "invalid", EV, "prefix"),
        bl(5, "eventChild", "b-add", "applied", EV),
        bl(6, "eventChild", "b-add2", "collision", EV),
        bl(7, "spawnPosition", "b-anim", "invalid", SP, "no-positions"),
        bl(8, "event", "b-inc", "invalid", EV, "incomplete"),
        bl(9, "spawnPosition", "b-near", "collision", SP),
        bl(10, "event", "b-mx", "invalid", EV, "max-zero"),
        bl(11, "eventChild", "b-orph", "orphaned", EV, "parent-missing"),
        bl(12, "event", "b-car", "invalid", None, "bad-id"),
        bl(13, "spawnPosition", "b-pos2", "applied", SP),
    ]
    case("event-own-blocks", {EV: ev, SP: sp}, {"blocks": blocks}, {EV: ev_out, SP: sp_out},
         {"entries": entries, "files": [fl(SP, "written"), fl(EV, "written")]})

def run():
    events_fields()
    events_case_overrode()
    events_duplicate()
    events_own()

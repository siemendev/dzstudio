from lib import *
from cases_json import vanilla

GL = 'db/globals.xml'
MS = 'db/messages.xml'
EC = 'db/economy.xml'
CO = 'cfgeconomycore.xml'
SP = 'cfgspawnabletypes.xml'
RP = 'cfgrandompresets.xml'

def globals_case():
    base = vanilla('chernarusplus', GL)
    K = "global"
    changes = [
        S({"name": "AnimalMaxCount"}, "value", N("200"), N("250"), kind=K),
        S({"name": "LootDamageMax"}, "value", N("0.82"), N("0.9"), kind=K),
        S({"name": "ZombieMaxCount"}, "value", N("1000"), N("800.5"), kind=K),
        S({"name": "TimeLogin"}, "value", N("10"), N("20"), kind=K),
        S({"name": "NoSuchVar"}, "value", None, N("1"), kind=K),
        R({"name": "TimePenalty"}, kind=K),
        S({"name": "FoodDecay"}, "value", N("1"), N("1"), kind=K),
        S({"name": "LootDamageMin"}, "type", N("1"), N("0"), kind=K),
        S({"name": "animalmaxcount"}, "value", None, N("1"), kind=K),
    ]
    blocks = [
        {"id": "b-v", "kind": K, "parent": None, "data": {"name": "MyServerVar", "type": 0, "value": 5}},
        {"id": "b-dup", "kind": K, "parent": None, "data": {"name": "AnimalMaxCount", "type": 0, "value": 5}},
        {"id": "b-int", "kind": K, "parent": None, "data": {"name": "Other", "type": 0, "value": N("1.5")}},
        {"id": "b-type", "kind": K, "parent": None, "data": {"name": "Other", "type": 2, "value": 1}},
        # two own globals of one name would leave the game one of them: the second is refused
        {"id": "b-v2", "kind": K, "parent": None, "data": {"name": "MyServerVar", "type": 0, "value": 7}},
    ]
    out = rep(base, '<var name="AnimalMaxCount" type="0" value="200"/>', '<var name="AnimalMaxCount" type="0" value="250"/>')
    out = rep(out, '<var name="LootDamageMax" type="1" value="0.82"/>', '<var name="LootDamageMax" type="1" value="0.9"/>')
    out = rep(out, '<var name="TimeLogin" type="0" value="15"/>', '<var name="TimeLogin" type="0" value="20"/>')
    out = rep(out, '    <var name="TimePenalty" type="0" value="20"/>\n', '')
    out = rep(out, '</variables>', '    <var name="MyServerVar" type="0" value="5"/>\n</variables>')
    entries = [
        ch(0, "set", K, {"name": "AnimalMaxCount"}, "applied", GL, "value"),
        ch(1, "set", K, {"name": "LootDamageMax"}, "applied", GL, "value"),
        ch(2, "set", K, {"name": "ZombieMaxCount"}, "invalid", GL, "value", detail="bad-value"),
        ch(3, "set", K, {"name": "TimeLogin"}, "overrode", GL, "value", current="15"),
        ch(4, "set", K, {"name": "NoSuchVar"}, "orphaned", GL, "value"),
        ch(5, "remove", K, {"name": "TimePenalty"}, "applied", GL),
        ch(6, "set", K, {"name": "FoodDecay"}, "already", GL, "value"),
        ch(7, "set", K, {"name": "LootDamageMin"}, "invalid", GL, "type", detail="bad-value"),
        ch(8, "set", K, {"name": "animalmaxcount"}, "orphaned", GL, "value", detail="case-mismatch"),
        bl(0, K, "b-v", "applied", GL),
        bl(1, K, "b-dup", "collision", GL),
        bl(2, K, "b-int", "invalid", GL, "bad-value"),
        bl(3, K, "b-type", "invalid", GL, "bad-value"),
        bl(4, K, "b-v2", "collision", GL),
    ]
    case("globals", {GL: base}, {"changes": changes, "blocks": blocks}, {GL: out},
         {"entries": entries, "files": [fl(GL, "written")]})

WELCOME = '''    <message>
        <delay>2</delay>
        <onconnect>1</onconnect>
        <text>Welcome to my server #name</text>
    </message>
'''

def messages():
    van = vanilla('chernarusplus', MS)
    assert '\r\n' in van
    base = rep(van, '</messages>', WELCOME.replace('\n', '\r\n') + '</messages>')
    K = "message"
    W = {"text": "Welcome to my server #name"}
    changes = [
        S(W, "delay", N("2"), N("5"), kind=K),
        S(W, "text", "Welcome to my server #name", "Welcome to #name & friends", kind=K),
        S({"text": "Welcome to #name & friends"}, "delay", None, N("1"), kind=K),
        S({"text": "Check our web page at www.dayz.com"}, "repeat", None, N("10"), kind=K),
        S(W, "repeat", None, N("30"), kind=K),
        R({"text": "Nope"}, kind=K),
        S(W, "delay", None, N("1.5"), kind=K),
    ]
    blocks = [
        {"id": "b-m", "kind": K, "parent": None, "data": {"repeat": 15, "text": "Join our Discord <3"}},
        {"id": "b-dup", "kind": K, "parent": None, "data": {"onconnect": 1, "text": "Welcome to my server #name"}},
        {"id": "b-bad", "kind": K, "parent": None, "data": {"delay": 1}},
    ]
    out = rep(base, WELCOME.replace('\n', '\r\n') + '</messages>', '''    <message>
        <delay>5</delay>
        <repeat>30</repeat>
        <onconnect>1</onconnect>
        <text>Welcome to #name &amp; friends</text>
    </message>
    <message>
        <repeat>15</repeat>
        <text>Join our Discord &lt;3</text>
    </message>
'''.replace('\n', '\r\n') + '</messages>')
    entries = [
        ch(0, "set", K, W, "applied", MS, "delay"),
        ch(1, "set", K, W, "applied", MS, "text"),
        ch(2, "set", K, {"text": "Welcome to #name & friends"}, "orphaned", MS, "delay"),
        ch(3, "set", K, {"text": "Check our web page at www.dayz.com"}, "orphaned", MS, "repeat"),
        ch(4, "set", K, W, "applied", MS, "repeat"),
        ch(5, "remove", K, {"text": "Nope"}, "gone", MS),
        ch(6, "set", K, W, "invalid", MS, "delay", detail="bad-value"),
        bl(0, K, "b-m", "applied", MS),
        bl(1, K, "b-dup", "collision", MS),
        bl(2, K, "b-bad", "invalid", MS, "incomplete"),
    ]
    case("messages", {MS: base}, {"changes": changes, "blocks": blocks}, {MS: out},
         {"entries": entries, "files": [fl(MS, "written")]})

    # own messages may share a text (one text at several times), all are written; a text of the base still clashes
    D = "Join our Discord"
    blocks = [
        {"id": "b-1", "kind": K, "parent": None, "data": {"onconnect": 1, "text": D}},
        {"id": "b-2", "kind": K, "parent": None, "data": {"delay": 2, "onconnect": 1, "text": D}},
        {"id": "b-base", "kind": K, "parent": None, "data": {"repeat": 5, "text": "Welcome to my server #name"}},
        {"id": "b-3", "kind": K, "parent": None, "data": {"repeat": 5, "text": D}},
    ]
    out = rep(base, '</messages>', """    <message>
        <onconnect>1</onconnect>
        <text>Join our Discord</text>
    </message>
    <message>
        <delay>2</delay>
        <onconnect>1</onconnect>
        <text>Join our Discord</text>
    </message>
    <message>
        <repeat>5</repeat>
        <text>Join our Discord</text>
    </message>
""".replace('\n', '\r\n') + '</messages>')
    case("messages-own-duplicates", {MS: base}, {"changes": [], "blocks": blocks}, {MS: out},
         {"entries": [bl(0, K, "b-1", "applied", MS), bl(1, K, "b-2", "applied", MS),
                      bl(2, K, "b-base", "collision", MS), bl(3, K, "b-3", "applied", MS)],
          "files": [fl(MS, "written")]})

    # Sakhal ships no messages.xml: an own message creates it, a change still finds nothing
    made = '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<messages>
    <message>
        <deadline>175</deadline>
        <shutdown>1</shutdown>
        <text>Restart in #tmin minutes</text>
    </message>
    <message>
        <delay>1</delay>
        <onconnect>1</onconnect>
        <text>Hello</text>
    </message>
</messages>
'''
    blocks = [
        {"id": "b-r", "kind": K, "parent": None, "data": {"deadline": 175, "shutdown": True, "text": "Restart in #tmin minutes"}},
        {"id": "b-h", "kind": K, "parent": None, "data": {"text": "Hello", "onconnect": 1, "delay": 1}},
    ]
    case("messages-missing", {"init.c": "void main() {}\n"},
         {"map": "sakhal", "changes": [S(W, "delay", None, N("5"), kind=K)], "blocks": blocks}, {MS: made},
         {"entries": [ch(0, "set", K, W, "invalid", MS, "delay", detail="missing-file"),
                      bl(0, K, "b-r", "applied", MS), bl(1, K, "b-h", "applied", MS)],
          "files": [fl(MS, "created")]})

def economy():
    base = vanilla('chernarusplus', EC)
    K = "economy"
    changes = [
        S({"name": "zombies"}, "save", N("0"), N("1"), kind=K),
        S({"name": "dynamic"}, "init", N("1"), True, kind=K),
        S({"name": "weather"}, "init", None, N("1"), kind=K),
        R({"name": "custom"}, kind=K),
        S({"name": "custom"}, "respawn", N("0"), True, kind=K),
        S({"name": "custom"}, "init", None, None, kind=K),
    ]
    out = rep(base, '<zombies init="1" load="0" respawn="1" save="0"/>', '<zombies init="1" load="0" respawn="1" save="1"/>')
    out = rep(out, '<custom init="0" load="0" respawn="0" save="0"/>', '<custom init="0" load="0" respawn="1" save="0"/>')
    entries = [
        ch(0, "set", K, {"name": "zombies"}, "applied", EC, "save"),
        ch(1, "set", K, {"name": "dynamic"}, "already", EC, "init"),
        ch(2, "set", K, {"name": "weather"}, "invalid", EC, "init", detail="bad-key"),
        ch(3, "remove", K, {"name": "custom"}, "invalid", EC, detail="bad-op"),
        ch(4, "set", K, {"name": "custom"}, "applied", EC, "respawn"),
        ch(5, "set", K, {"name": "custom"}, "invalid", EC, "init", detail="bad-value"),
        bl(0, K, "b-e", "invalid", detail="unknown-kind"),
    ]
    blocks = [{"id": "b-e", "kind": K, "parent": None, "data": {"init": 1}}]
    case("economy", {EC: base}, {"changes": changes, "blocks": blocks}, {EC: out},
         {"entries": entries, "files": [fl(EC, "written")]})

def economy_core():
    base = vanilla('chernarusplus', CO)
    D = "ceDefault"
    C = "ceRootClass"
    changes = [
        S({"name": "dyn_radius"}, "value", "30", "40", kind=D),
        S({"name": "log_ce_loop"}, "value", "false", "true", kind=D),
        R({"name": "save_events_startup"}, kind=D),
        S({"name": "BoatScript"}, "reportMemoryLOD", "no", "yes", kind=C),
        S({"name": "BoatScript"}, "act", "car", "bike", kind=C),
        R({"name": "NoSuchClass"}, kind=C),
        S({"name": "dyn_radius"}, "value", None, N("40"), kind=D),
    ]
    blocks = [
        {"id": "b-d", "kind": D, "parent": None, "data": {"name": "log_ce_animals", "value": "true"}},
        {"id": "b-c", "kind": C, "parent": None, "data": {"name": "DZ_MyVehicle", "act": "car"}},
        {"id": "b-dup", "kind": D, "parent": None, "data": {"name": "dyn_radius", "value": "1"}},
        {"id": "b-core", "kind": "economyCore", "parent": None, "data": {}},
    ]
    out = rep(base, '<default name="dyn_radius" value="30" />', '<default name="dyn_radius" value="40" />')
    out = rep(out, '<default name="log_ce_loop" value="false"/>', '<default name="log_ce_loop" value="true"/>')
    out = rep(out, '\t\t<default name="save_events_startup" value="true"/>\n', '')
    out = rep(out, '<rootclass name="BoatScript" act="car" reportMemoryLOD="no" />', '<rootclass name="BoatScript" act="car" reportMemoryLOD="yes" />')
    out = rep(out, '\t</classes>', '\t\t<rootclass name="DZ_MyVehicle" act="car" />\n\t</classes>')
    out = rep(out, '\t</defaults>', '\t\t<default name="log_ce_animals" value="true" />\n\t</defaults>')
    entries = [
        ch(0, "set", D, {"name": "dyn_radius"}, "applied", CO, "value"),
        ch(1, "set", D, {"name": "log_ce_loop"}, "applied", CO, "value"),
        ch(2, "remove", D, {"name": "save_events_startup"}, "applied", CO),
        ch(3, "set", C, {"name": "BoatScript"}, "applied", CO, "reportMemoryLOD"),
        ch(4, "set", C, {"name": "BoatScript"}, "invalid", CO, "act", detail="bad-value"),
        ch(5, "remove", C, {"name": "NoSuchClass"}, "gone", CO),
        ch(6, "set", D, {"name": "dyn_radius"}, "invalid", CO, "value", detail="bad-value"),
        bl(0, D, "b-d", "applied", CO),
        bl(1, C, "b-c", "applied", CO),
        bl(2, D, "b-dup", "collision", CO),
        bl(3, "economyCore", "b-core", "invalid", detail="unknown-kind"),
    ]
    case("economy-core", {CO: base}, {"changes": changes, "blocks": blocks}, {CO: out},
         {"entries": entries, "files": [fl(CO, "written")]})

def ignore_list():
    real = 'cfgIgnoreList.xml'
    base = vanilla('chernarusplus', real)
    K = "ignoredType"
    changes = [R({"name": "spear"}, kind=K), R({"name": "Nope"}, kind=K), S({"name": "Bandage"}, "name", None, "X", kind=K)]
    blocks = [
        {"id": "b-i", "kind": K, "parent": None, "data": {"name": "Mag_Custom_30Rnd"}},
        {"id": "b-dup", "kind": K, "parent": None, "data": {"name": "bandage"}},
    ]
    out = rep(base, '\t<type name="Spear"></type>\n', '')
    out = rep(out, '</ignore>', '\t<type name="Mag_Custom_30Rnd"></type>\n</ignore>')
    entries = [
        ch(0, "remove", K, {"name": "spear"}, "applied", real),
        ch(1, "remove", K, {"name": "Nope"}, "gone", real),
        ch(2, "set", K, {"name": "Bandage"}, "applied", real, "name"),
        bl(0, K, "b-i", "applied", real),
        bl(1, K, "b-dup", "collision", real),
    ]
    out = rep(out, '<type name="Bandage"></type>', '<type name="X"></type>')
    case("ignore-list", {real: base}, {"changes": changes, "blocks": blocks}, {real: out},
         {"entries": entries, "files": [fl(real, "written")]})

SPHEAD = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>\n\n<spawnabletypes>\n'
SPBODY = '''\t<type name="Barrel_Blue">
\t\t<hoarder />
\t</type>
\t<type name="NVGoggles">
\t\t<damage min="0.0" max="0.32" />
\t</type>
\t<type name="GiftWrapPaper">
\t\t<damage min="0.0" max="0.32" />
\t</type>
\t<type name="M4A1">
\t\t<damage min="0.45" max="0.85" />
\t\t<attachments chance="1.00">
\t\t\t<item name="M4_OEBttstck" chance="1.00" />
\t\t</attachments>
\t\t<attachments chance="1.00">
\t\t\t<item name="BUISOptic" chance="0.50" />
\t\t\t<item name="M4_CarryHandleOptic" chance="1.00" />
\t\t</attachments>
\t\t<cargo preset="mixArmy" />
\t</type>
\t<!--<type name="Commented"><hoarder /></type>-->
\t<type name="ZmbM_Mummy">
\t\t<cargo preset="ContaminatedCargo" />
\t</type>
\t<type name="ZmbM_Mummy">
\t\t<cargo chance="0.20">
\t\t\t<item name="BandageDressing" chance="0.2" />
\t\t</cargo>
\t</type>
'''

def spawnable():
    base = SPHEAD + SPBODY + '</spawnabletypes>\n'
    T = "spawnable"
    A = "spawnableAttachments"
    C = "spawnableCargo"
    changes = [
        S({"name": "barrel_blue"}, "hoarder", N("1"), False, kind=T),
        S({"name": "NVGoggles"}, "hoarder", None, True, kind=T),
        S({"name": "NVGoggles"}, "damage.max", N("0.32"), N("0.5"), kind=T),
        S({"type": "M4A1"}, "chance", None, N("0.5"), kind=A),
        S({"type": "M4A1", "ordinal": N("2")}, "item", fp(["name=BUISOptic\tchance=0.5", "name=M4_CarryHandleOptic\tchance=1"]),
          [{"name": "ACOGOptic", "chance": N("0.50")}, {"name": "M4_CarryHandleOptic", "chance": 1}], kind=A),
        S({"type": "M4A1"}, "preset", "mixArmy", "mixHunter", kind=C),
        S({"type": "ZmbM_Mummy"}, "chance", N("0.20"), N("0.3"), kind=C),
        R({"type": "M4A1", "ordinal": N("1")}, kind=A),
        R({"name": "Nope"}, kind=T),
        R({"name": "giftwrappaper"}, kind=T),
        S({"name": "Commented"}, "hoarder", None, True, kind=T),
        S({"type": "M4A1", "ordinal": N("2")}, "item", None, [{"name": "X", "chance": "high"}], kind=A),
    ]
    blocks = [
        {"id": "b-r", "kind": T, "parent": None, "data": {"name": "MyRifle", "damage": {"min": N("0.1"), "max": N("0.5")}}},
        {"id": "b-r-a", "kind": A, "parent": "b-r", "data": {"chance": N("0.6"), "item": [{"name": "M4_Suppressor", "chance": 1}]}},
        {"id": "b-r-c", "kind": C, "parent": "b-r", "data": {"preset": "mixArmy"}},
        {"id": "b-nv", "kind": A, "parent": {"type": "nvgoggles"}, "data": {"chance": 1, "item": [{"name": "Battery9V", "chance": 1}]}},
        {"id": "b-both", "kind": C, "parent": {"type": "NVGoggles"}, "data": {"preset": "x", "chance": 1}},
        {"id": "b-none", "kind": A, "parent": {"type": "NVGoggles"}, "data": {"item": ["Battery9V"]}},
        {"id": "b-dup", "kind": T, "parent": None, "data": {"name": "m4a1", "hoarder": True}},
        {"id": "b-h", "kind": T, "parent": None, "data": {"name": "MyCrate", "hoarder": True}},
    ]
    out = rep(base, '''\t<type name="Barrel_Blue">
\t\t<hoarder />
\t</type>''', '''\t<type name="Barrel_Blue">
\t</type>''')
    out = rep(out, '''\t<type name="NVGoggles">
\t\t<damage min="0.0" max="0.32" />
\t</type>''', '''\t<type name="NVGoggles">
\t\t<hoarder />
\t\t<damage min="0.0" max="0.5" />
\t\t<attachments chance="1">
\t\t\t<item name="Battery9V" chance="1" />
\t\t</attachments>
\t</type>''')
    out = rep(out, '''\t<type name="GiftWrapPaper">
\t\t<damage min="0.0" max="0.32" />
\t</type>
''', '')
    out = rep(out, '''\t\t<attachments chance="1.00">
\t\t\t<item name="M4_OEBttstck" chance="1.00" />
\t\t</attachments>
''', '')
    out = rep(out, '''\t\t\t<item name="BUISOptic" chance="0.50" />
\t\t\t<item name="M4_CarryHandleOptic" chance="1.00" />''', '''\t\t\t<item name="ACOGOptic" chance="0.5" />
\t\t\t<item name="M4_CarryHandleOptic" chance="1" />''')
    out = rep(out, '<cargo preset="mixArmy" />', '<cargo preset="mixHunter" />')
    out = rep(out, '<cargo chance="0.20">', '<cargo chance="0.3">')
    out = rep(out, '</spawnabletypes>', '''\t<type name="MyRifle">
\t\t<damage min="0.1" max="0.5" />
\t\t<attachments chance="0.6">
\t\t\t<item name="M4_Suppressor" chance="1" />
\t\t</attachments>
\t\t<cargo preset="mixArmy" />
\t</type>
\t<type name="MyCrate">
\t\t<hoarder />
\t</type>
</spawnabletypes>''')
    entries = [
        ch(0, "set", T, {"name": "barrel_blue"}, "applied", SP, "hoarder"),
        ch(1, "set", T, {"name": "NVGoggles"}, "applied", SP, "hoarder"),
        ch(2, "set", T, {"name": "NVGoggles"}, "applied", SP, "damage.max"),
        ch(3, "set", A, {"type": "M4A1"}, "ambiguous", SP, "chance"),
        ch(4, "set", A, {"type": "M4A1", "ordinal": N("2")}, "applied", SP, "item"),
        ch(5, "set", C, {"type": "M4A1"}, "applied", SP, "preset"),
        ch(6, "set", C, {"type": "ZmbM_Mummy"}, "applied", SP, "chance"),
        ch(7, "remove", A, {"type": "M4A1", "ordinal": N("1")}, "applied", SP),
        ch(8, "remove", T, {"name": "Nope"}, "gone", SP),
        ch(9, "remove", T, {"name": "giftwrappaper"}, "applied", SP),
        ch(10, "set", T, {"name": "Commented"}, "orphaned", SP, "hoarder"),
        ch(11, "set", A, {"type": "M4A1", "ordinal": N("2")}, "invalid", SP, "item", detail="bad-value"),
        bl(0, T, "b-r", "applied", SP),
        bl(1, A, "b-r-a", "applied", SP),
        bl(2, C, "b-r-c", "applied", SP),
        bl(3, A, "b-nv", "applied", SP),
        bl(4, C, "b-both", "invalid", SP, "bad-value"),
        bl(5, A, "b-none", "invalid", SP, "incomplete"),
        bl(6, T, "b-dup", "collision", SP),
        bl(7, T, "b-h", "applied", SP),
    ]
    case("spawnable-types", {SP: base}, {"changes": changes, "blocks": blocks}, {SP: out},
         {"entries": entries, "files": [fl(SP, "written")]})

RPBASE = '''<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<randompresets>
\t\t<cargo chance="0.15" name="foodHermit">
\t\t\t\t<item name="TunaCan" chance="0.11" />
\t\t\t\t<item name="SardinesCan" chance="0.11" />
\t\t</cargo>
\t\t<attachments chance="0.30" name="optics">
\t\t\t\t<item name="ACOGOptic" chance="0.10" />
\t\t</attachments>
</randompresets>
'''

def random_presets():
    C = "cargoPreset"
    A = "attachmentsPreset"
    changes = [
        S({"name": "foodHermit"}, "chance", N("0.15"), N("0.25"), kind=C),
        S({"name": "foodHermit"}, "item", fp(["name=TunaCan\tchance=0.11", "name=SardinesCan\tchance=0.11"]),
          [{"name": "TunaCan", "chance": N("0.2")}, {"name": "Apple", "chance": N("0.05")}], kind=C),
        S({"name": "optics"}, "chance", N("0.3"), N("0.4"), kind=A),
        S({"name": "optics"}, "chance", None, N("0.4"), kind=C),
        S({"name": "FOODHERMIT"}, "chance", None, N("0.4"), kind=C),
        R({"name": "nope"}, kind=C),
    ]
    blocks = [
        {"id": "b-c", "kind": C, "parent": None, "data": {"name": "myLoot", "chance": N("0.5"), "item": [{"name": "Apple", "chance": N("0.5")}]}},
        {"id": "b-a", "kind": A, "parent": None, "data": {"name": "foodHermit", "chance": 1}},
        {"id": "b-dup", "kind": C, "parent": None, "data": {"name": "foodHermit", "chance": 1}},
        {"id": "b-bad", "kind": C, "parent": None, "data": {"name": "x"}},
    ]
    out = rep(RPBASE, '<cargo chance="0.15" name="foodHermit">', '<cargo chance="0.25" name="foodHermit">')
    out = rep(out, '''\t\t\t\t<item name="TunaCan" chance="0.11" />
\t\t\t\t<item name="SardinesCan" chance="0.11" />''', '''\t\t\t\t<item name="TunaCan" chance="0.2" />
\t\t\t\t<item name="Apple" chance="0.05" />''')
    out = rep(out, '<attachments chance="0.30" name="optics">', '<attachments chance="0.4" name="optics">')
    out = rep(out, '</randompresets>', '''\t\t<cargo chance="0.5" name="myLoot">
\t\t\t\t<item name="Apple" chance="0.5" />
\t\t</cargo>
\t\t<attachments chance="1" name="foodHermit" />
</randompresets>''')
    entries = [
        ch(0, "set", C, {"name": "foodHermit"}, "applied", RP, "chance"),
        ch(1, "set", C, {"name": "foodHermit"}, "applied", RP, "item"),
        ch(2, "set", A, {"name": "optics"}, "applied", RP, "chance"),
        ch(3, "set", C, {"name": "optics"}, "orphaned", RP, "chance"),
        ch(4, "set", C, {"name": "FOODHERMIT"}, "orphaned", RP, "chance", detail="case-mismatch"),
        ch(5, "remove", C, {"name": "nope"}, "gone", RP),
        bl(0, C, "b-c", "applied", RP),
        bl(1, A, "b-a", "applied", RP),
        bl(2, C, "b-dup", "collision", RP),
        bl(3, C, "b-bad", "invalid", RP, "incomplete"),
    ]
    case("random-presets", {RP: RPBASE}, {"changes": changes, "blocks": blocks}, {RP: out},
         {"entries": entries, "files": [fl(RP, "written")]})

def globals_duplicate():
    base = vanilla('chernarusplus', GL)
    K = "global"
    blocks = [
        # a block marked duplicate is written although the base has its name: the file then holds it twice
        {"id": "b-twice", "kind": K, "parent": None, "duplicate": True, "data": {"name": "AnimalMaxCount", "type": 0, "value": 350}},
        {"id": "b-again", "kind": K, "parent": None, "duplicate": True, "data": {"name": "AnimalMaxCount", "type": 0, "value": 400}},
        {"id": "b-plain", "kind": K, "parent": None, "data": {"name": "ZombieMaxCount", "type": 0, "value": 1500}},
        {"id": "b-flag", "kind": K, "parent": None, "duplicate": "yes", "data": {"name": "TimeLogin", "type": 0, "value": 5}},
    ]
    out = rep(base, '</variables>', '''    <var name="AnimalMaxCount" type="0" value="350"/>
    <var name="AnimalMaxCount" type="0" value="400"/>
</variables>''')
    entries = [
        bl(0, K, "b-twice", "applied", GL),
        bl(1, K, "b-again", "applied", GL),
        bl(2, K, "b-plain", "collision", GL),
        bl(3, K, "b-flag", "collision", GL),
    ]
    case("globals-duplicate", {GL: base}, {"blocks": blocks}, {GL: out},
         {"entries": entries, "files": [fl(GL, "written")]})

SPITEMS = SPHEAD + '''\t<type name="CanvasBag_Medical">
\t\t<cargo chance="0.6">
\t\t\t<item name="BandageDressing" />
\t\t</cargo>
\t\t<cargo chance="0.5">
\t\t\t<item name="bandagedressing" chance="0.5" />
\t\t\t<item name="Rag" chance="0.50" />
\t\t</cargo>
\t</type>
\t<type name="OffroadHatchback">
\t\t<attachments chance="1.00">
\t\t\t<item name="HeadlightH7" chance="0.40" />
\t\t</attachments>
\t\t<attachments chance="1.00">
\t\t\t<item name="HeadlightH7" chance="0.40" />
\t\t</attachments>
\t</type>
\t<type name="Barrel_Blue">
\t\t<hoarder />
\t</type>
</spawnabletypes>
'''

def spawnable_items():
    I = "spawnableItem"
    T = "spawnable"
    changes = [
        R({"name": "BandageDressing"}, kind=I),
        R({"name": "HeadlightH7", "type": "offroadhatchback"}, kind=I),
        R({"name": "Rag", "type": "OffroadHatchback"}, kind=I),
        R({"name": "Nope"}, kind=I),
        S({"name": "Rag"}, "chance", None, 1, kind=I),
        R({}, kind=I),
        # two sets under an element the base lacks write one element with both values
        S({"name": "Barrel_Blue"}, "damage.min", None, N("0.0"), kind=T),
        S({"name": "Barrel_Blue"}, "damage.max", None, N("0.0"), kind=T),
    ]
    out = rep(SPITEMS, '''\t\t<cargo chance="0.6">
\t\t\t<item name="BandageDressing" />
\t\t</cargo>
\t\t<cargo chance="0.5">
\t\t\t<item name="bandagedressing" chance="0.5" />
\t\t\t<item name="Rag" chance="0.50" />
\t\t</cargo>''', '''\t\t<cargo chance="0.6">
\t\t</cargo>
\t\t<cargo chance="0.5">
\t\t\t<item name="Rag" chance="0.5" />
\t\t</cargo>''')
    out = rep(out, '''\t\t<attachments chance="1.00">
\t\t\t<item name="HeadlightH7" chance="0.40" />
\t\t</attachments>''', '''\t\t<attachments chance="1.00">
\t\t</attachments>''', 2)
    out = rep(out, '''\t\t<hoarder />
\t</type>''', '''\t\t<hoarder />
\t\t<damage min="0" max="0" />
\t</type>''')
    entries = [
        ch(0, "remove", I, {"name": "BandageDressing"}, "applied", SP),
        ch(1, "remove", I, {"name": "HeadlightH7", "type": "offroadhatchback"}, "applied", SP),
        ch(2, "remove", I, {"name": "Rag", "type": "OffroadHatchback"}, "gone", SP),
        ch(3, "remove", I, {"name": "Nope"}, "gone", SP),
        ch(4, "set", I, {"name": "Rag"}, "invalid", SP, "chance", detail="bad-op"),
        ch(5, "remove", I, {}, "invalid", SP, detail="bad-key"),
        ch(6, "set", T, {"name": "Barrel_Blue"}, "applied", SP, "damage.min"),
        ch(7, "set", T, {"name": "Barrel_Blue"}, "applied", SP, "damage.max"),
    ]
    case("spawnable-items", {SP: SPITEMS}, {"changes": changes}, {SP: out},
         {"entries": entries, "files": [fl(SP, "written")]})

def preset_items():
    I = "presetItem"
    changes = [
        R({"name": "tunacan"}, kind=I),
        R({"name": "ACOGOptic", "preset": "nope"}, kind=I),
        R({"name": "ACOGOptic", "preset": "optics"}, kind=I),
    ]
    out = rep(RPBASE, '\t\t\t\t<item name="TunaCan" chance="0.11" />\n', '')
    out = rep(out, '''\t\t<attachments chance="0.30" name="optics">
\t\t\t\t<item name="ACOGOptic" chance="0.10" />
\t\t</attachments>''', '''\t\t<attachments chance="0.30" name="optics">
\t\t</attachments>''')
    entries = [
        ch(0, "remove", I, {"name": "tunacan"}, "applied", RP),
        ch(1, "remove", I, {"name": "ACOGOptic", "preset": "nope"}, "gone", RP),
        ch(2, "remove", I, {"name": "ACOGOptic", "preset": "optics"}, "applied", RP),
    ]
    case("preset-items", {RP: RPBASE}, {"changes": changes}, {RP: out},
         {"entries": entries, "files": [fl(RP, "written")]})

def run():
    globals_case()
    globals_duplicate()
    messages()
    economy()
    economy_core()
    ignore_list()
    spawnable()
    random_presets()
    spawnable_items()
    preset_items()

import re
from lib import *
from cases_json import vanilla
from cases_events import EV, HEAD, TAIL, SEDAN

WE = 'cfgweather.xml'
TY = 'db/types.xml'

def weather_data(text):
    d = {"reset": 0}
    for ph in ["overcast", "fog", "rain", "windMagnitude", "windDirection", "snowfall"]:
        body = re.search(r'<%s>(.*?)</%s>' % (ph, ph), text, re.S).group(1)
        o = {}
        for el in ["current", "limits", "timelimits", "changelimits", "thresholds"]:
            m = re.search(r'^\s*<%s ([^>]*?)\s*/>' % el, body, re.M)
            if m:
                o[el] = {k: N(v) for k, v in re.findall(r'(\w+)="([^"]*)"', m.group(1))}
        d[ph] = o
    m = re.search(r'<storm ([^>]*?)/>', text)
    d["storm"] = {k: N(v) for k, v in re.findall(r'(\w+)="([^"]*)"', m.group(1))}
    return d

def weather():
    base = vanilla('chernarusplus', WE)
    assert '\r\n' in base
    data = weather_data(base)
    data["fog"]["limits"]["max"] = N("0.20")
    data["storm"]["threshold"] = N("0.5")
    out = rep(base, '<weather reset="0" enable="0">', '<weather reset="0" enable="1">')
    out = rep(out, '<limits min="0.02" max="0.08" />', '<limits min="0.02" max="0.2" />')
    out = rep(out, 'threshold="0.9"', 'threshold="0.5"')
    second = weather_data(base)
    blocks = [
        {"id": "b-w", "kind": "weather", "parent": None, "data": data},
        {"id": "b-w2", "kind": "weather", "parent": None, "data": second},
    ]
    case("weather-crlf", {WE: base}, {"blocks": blocks}, {WE: out},
         {"entries": [bl(0, "weather", "b-w", "applied", WE), bl(1, "weather", "b-w2", "collision")],
          "files": [fl(WE, "written")]})

    made = '''<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<weather reset="0" enable="1">
    <overcast>
        <current actual="0.45" time="120" duration="240" />
        <limits min="0" max="1" />
        <timelimits min="600" max="900" />
        <changelimits min="0" max="1" />
    </overcast>
    <fog>
        <current actual="0.05" time="120" duration="240" />
        <limits min="0.02" max="0.08" />
        <timelimits min="900" max="900" />
        <changelimits min="0" max="1" />
    </fog>
    <rain>
        <current actual="0" time="60" duration="120" />
        <limits min="0" max="1" />
        <timelimits min="60" max="120" />
        <changelimits min="0" max="1" />
        <thresholds min="0.6" max="1" end="60" />
    </rain>
    <windMagnitude>
        <current actual="8" time="120" duration="240" />
        <limits min="0" max="20" />
        <timelimits min="120" max="240" />
        <changelimits min="0" max="20" />
    </windMagnitude>
    <windDirection>
        <current actual="0" time="120" duration="240" />
        <limits min="-3.14" max="3.14" />
        <timelimits min="60" max="120" />
        <changelimits min="-1" max="1" />
    </windDirection>
    <snowfall>
        <current actual="0" time="0" duration="32768" />
        <limits min="0" max="0" />
        <timelimits min="300" max="3600" />
        <changelimits min="0" max="0" />
        <thresholds min="1" max="1" end="120" />
    </snowfall>
    <storm density="1" threshold="0.9" timeout="45" />
</weather>
'''
    case("weather-missing", {"init.c": "void main() {}\n"}, {"blocks": [{"id": "b-w", "kind": "weather", "parent": None, "data": weather_data(base)}]},
         {WE: made}, {"entries": [bl(0, "weather", "b-w", "applied", WE)], "files": [fl(WE, "created")]})

    on = rep(base, 'enable="0"', 'enable="1"')
    half = {"reset": 0, "fog": {"limits": {"min": 0, "max": 1}}}
    case("weather-already", {WE: on}, {"blocks": [{"id": "b-w", "kind": "weather", "parent": None, "data": weather_data(base)},
                                                  {"id": "b-h", "kind": "weather", "parent": None, "data": half}]},
         {}, {"entries": [bl(0, "weather", "b-w", "already", WE), bl(1, "weather", "b-h", "collision")], "files": [fl(WE, "unchanged")]})
    case("weather-incomplete", {WE: base}, {"blocks": [{"id": "b-h", "kind": "weather", "parent": None, "data": half}]},
         {}, {"entries": [bl(0, "weather", "b-h", "invalid", detail="incomplete")], "files": [fl(WE, "unchanged")]})

TYHEAD = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>\n<types>\n'
AKM = '''    <type name="AKM">
        <nominal>3</nominal>
        <lifetime>28800</lifetime>
        <restock>0</restock>
        <min>2</min>
        <quantmin>-1</quantmin>
        <quantmax>-1</quantmax>
        <cost>100</cost>
        <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0"/>
        <category name="weapons"/>
        <usage name="Military"/>
        <value name="Tier3"/>
        <value name="Tier4"/>
    </type>
'''
def apple(name, nominal):
    return '''    <type name="%s">
        <nominal>%d</nominal>
        <lifetime>3600</lifetime>
        <restock>0</restock>
        <min>5</min>
        <quantmin>-1</quantmin>
        <quantmax>-1</quantmax>
        <cost>100</cost>
        <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0"/>
        <category name="food"/>
        <usage name="Farm"/>
    </type>
''' % (name, nominal)

def types():
    base = TYHEAD + AKM + '    <!--<type name="Banana"><nominal>1</nominal></type>-->\n' + apple("Apple", 10) + apple("apple", 20) + '</types>\n'
    K = {"name": "akm"}
    T = "itemType"
    changes = [
        S(K, "nominal", N("3"), N("5"), kind=T),
        S(K, "value", fp(["name=Tier3", "name=Tier4"]), ["Tier4", "Tier5"], kind=T),
        S(K, "usage", "sha256:0000", ["Military", {"user": "TownUser"}], kind=T),
        S(K, "tag", None, ["floor"], kind=T),
        S({"name": "Apple"}, "nominal", N("20"), N("25"), kind=T),
        R({"name": "Banana"}, kind=T),
        S(K, "flags.deloot", N("0"), N("1"), kind=T),
        S(K, "usage", None, "notalist", kind=T),
        S({"name": "APPLE"}, "category.name", "food", "fruit", kind=T),
        S(K, "value", fp(["name=Tier4", "name=Tier5"]), ["Tier4", "Tier5"], kind=T),
    ]
    pen = {"name": "Pen_Gold", "nominal": 5, "lifetime": 3600, "restock": 0, "min": 2, "quantmin": -1, "quantmax": -1,
           "cost": 100, "flags": {"count_in_cargo": 0, "count_in_hoarder": 0, "count_in_map": 1, "count_in_player": 0,
                                  "crafted": 0, "deloot": 0},
           "category": {"name": "tools"}, "usage": ["Town"], "value": ["Tier1", "Tier2"]}
    dup = dict(pen, name="akm")
    blocks = [
        {"id": "b-pen", "kind": T, "parent": None, "data": pen},
        {"id": "b-dup", "kind": T, "parent": None, "data": dup},
    ]
    out = rep(base, '<nominal>3</nominal>', '<nominal>5</nominal>')
    out = rep(out, '        <value name="Tier3"/>\n        <value name="Tier4"/>\n',
              '        <value name="Tier4"/>\n        <value name="Tier5"/>\n')
    out = rep(out, '        <usage name="Military"/>\n', '        <usage name="Military"/>\n        <usage user="TownUser"/>\n')
    out = rep(out, '        <category name="weapons"/>\n', '        <category name="weapons"/>\n        <tag name="floor"/>\n')
    out = rep(out, '<nominal>20</nominal>', '<nominal>25</nominal>')
    out = rep(out, 'crafted="0" deloot="0"/>\n        <category name="weapons"/>', 'crafted="0" deloot="1"/>\n        <category name="weapons"/>')
    out = rep(out, '''        <category name="food"/>
        <usage name="Farm"/>
    </type>
</types>''', '''        <category name="fruit"/>
        <usage name="Farm"/>
    </type>
    <type name="Pen_Gold">
        <nominal>5</nominal>
        <lifetime>3600</lifetime>
        <restock>0</restock>
        <min>2</min>
        <quantmin>-1</quantmin>
        <quantmax>-1</quantmax>
        <cost>100</cost>
        <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0"/>
        <category name="tools"/>
        <usage name="Town"/>
        <value name="Tier1"/>
        <value name="Tier2"/>
    </type>
</types>''')
    entries = [
        ch(0, "set", T, K, "applied", TY, "nominal"),
        ch(1, "set", T, K, "applied", TY, "value"),
        ch(2, "set", T, K, "overrode", TY, "usage", current=fp(["name=Military"])),
        ch(3, "set", T, K, "applied", TY, "tag"),
        ch(4, "set", T, {"name": "Apple"}, "applied", TY, "nominal"),
        ch(5, "remove", T, {"name": "Banana"}, "gone", TY),
        ch(6, "set", T, K, "applied", TY, "flags.deloot"),
        ch(7, "set", T, K, "invalid", TY, "usage", detail="bad-value"),
        ch(8, "set", T, {"name": "APPLE"}, "applied", TY, "category.name"),
        ch(9, "set", T, K, "already", TY, "value"),
        bl(0, T, "b-pen", "applied", TY),
        bl(1, T, "b-dup", "collision", TY),
    ]
    case("item-types", {TY: base}, {"changes": changes, "blocks": blocks}, {TY: out},
         {"entries": entries, "files": [fl(TY, "written")]})

def missing_files():
    changes = [S({"name": "VehicleCivilianSedan"}, "nominal", None, N("1"))]
    blocks = [{"id": "b-p", "kind": "spawnPosition", "parent": {"event": "VehicleCivilianSedan"}, "data": {"x": 1, "z": 2}}]
    case("missing-files", {"init.c": "void main() {}\n"}, {"changes": changes, "blocks": blocks}, {},
         {"entries": [ch(0, "set", "event", {"name": "VehicleCivilianSedan"}, "invalid", EV, "nominal", detail="missing-file"),
                      bl(0, "spawnPosition", "b-p", "invalid", "cfgeventspawns.xml", "missing-file")],
          "files": [fl("cfgeventspawns.xml", "missing"), fl(EV, "missing")]})

def file_case():
    real = 'DB/Events.XML'
    base = HEAD + SEDAN + TAIL
    changes = [S({"name": "VehicleCivilianSedan"}, "nominal", N("8"), N("9"))]
    case("file-name-case", {real: base}, {"changes": changes}, {real: rep(base, '<nominal>8</nominal>', '<nominal>9</nominal>')},
         {"entries": [ch(0, "set", "event", {"name": "VehicleCivilianSedan"}, "applied", real, "nominal")],
          "files": [fl(real, "written")]})

def empty():
    base = {EV: HEAD + SEDAN + TAIL, 'cfggameplay.json': vanilla('chernarusplus', 'cfggameplay.json'),
            'cfgEffectArea.json': vanilla('chernarusplus', 'cfgEffectArea.json')}
    case("empty-project", base, {}, {}, {})

def run():
    weather()
    types()
    missing_files()
    file_case()
    empty()

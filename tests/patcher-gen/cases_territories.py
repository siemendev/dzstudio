from lib import *

ZF = 'env/zombie_territories.xml'
WF = 'env/wolf_territories.xml'

def z(name, dmin, dmax, x, zz, r, smin="0", smax="0"):
    return '        <zone name="%s" smin="%s" smax="%s" dmin="%s" dmax="%s" x="%s" z="%s" r="%s"/>\n' % (name, smin, smax, dmin, dmax, x, zz, r)

def terr(color, zones):
    return '    <territory color="%s">\n' % color + ''.join(zones) + '    </territory>\n'

def doc(parts):
    t = '<?xml version="1.0" encoding="UTF-8"?>\n<territory-type>\n' + ''.join(parts) + '</territory-type>\n'
    return t.replace('\n', '\r\n')

def zk(file, name, x, zz, **kw):
    k = {"file": file, "name": name, "x": N(str(x)), "z": N(str(zz))}
    k.update(kw)
    return k

def territories():
    VIL = "InfectedVillageTier1"
    zbase = doc([terr("2193199729", [z(VIL, 8, 12, "12913.3", "8020", "80"), z(VIL, 4, 6, "13296.3", "12996.3", "60")]),
                 terr("1124502272", [z("InfectedArmy", 3, 5, "4500", "10200", "100")])])
    wbase = doc([terr("4291611852", [z("Water", 0, 0, "10597.5", "13432.5", "60"), z("Rest", 0, 0, "10837.5", "13942.5", "247.5")]),
                 terr("4291611852", [z("Water", 0, 0, "10597.5", "13432.5", "60"), z("Graze", 0, 0, "11000", "13000", "200")])])
    TZ = "territoryZone"
    changes = [
        S(zk(ZF, VIL, "12913.3", 8020), "dmax", N("12"), N("15"), kind=TZ),
        S(zk(ZF, "InfectedArmy", "4500.03", 10200), "r", N("100"), N("120"), kind=TZ),
        R(zk(ZF, VIL, "13296.3", "12996.3"), kind=TZ),
        S(zk("env/../cfggameplay.json", VIL, 1, 1), "dmax", None, N("1"), kind=TZ),
        S(zk(ZF, "InfectedArmy", 4500, 10200, color="1124502272"), "dmin", N("3"), N("4"), kind=TZ),
        S(zk(ZF, "InfectedArmy", 4600, 10200), "dmin", None, N("4"), kind=TZ),
        S(zk(WF, "Water", "10597.5", "13432.5"), "r", None, N("80"), kind=TZ),
        R(zk(WF, "Water", "10597.5", "13432.5"), kind=TZ),
        R({"file": WF, "color": "4291611852", "ordinal": N("2")}, kind="territory"),
    ]
    blocks = [
        {"id": "b-z", "kind": TZ, "parent": {"file": ZF, "color": "1124502272"},
         "data": {"name": "InfectedArmy", "smin": 0, "smax": 0, "dmin": 2, "dmax": 4, "x": N("4700.5"), "z": 10300, "r": 50}},
        {"id": "b-t", "kind": "territory", "parent": None, "data": {"file": ZF, "color": "4278190335"}},
        {"id": "b-t-z", "kind": TZ, "parent": "b-t",
         "data": {"name": "InfectedCity", "smin": 0, "smax": 0, "dmin": 3, "dmax": 5, "x": 6500, "z": 2500, "r": 80}},
        {"id": "b-bad", "kind": "territory", "parent": None, "data": {"file": "cfggameplay.json", "color": "1"}},
    ]
    zout = doc([terr("2193199729", [z(VIL, 8, 15, "12913.3", "8020", "80")]),
                terr("1124502272", [z("InfectedArmy", 4, 5, "4500", "10200", "120"), z("InfectedArmy", 2, 4, "4700.5", "10300", "50")]),
                terr("4278190335", [z("InfectedCity", 3, 5, "6500", "2500", "80")])])
    wout = doc([terr("4291611852", [z("Rest", 0, 0, "10837.5", "13942.5", "247.5")])])
    st = ["applied", "applied", "applied", "invalid", "applied", "orphaned", "ambiguous", "applied", "applied"]
    entries = []
    for i, c in enumerate(changes):
        f = ZF
        if i >= 6:
            f = WF
        if i == 3:
            entries.append(ch(i, c["op"], c["block"], c["key"], "invalid", None, c.get("field"), detail="bad-key"))
            continue
        entries.append(ch(i, c["op"], c["block"], c["key"], st[i], f, c.get("field")))
    entries += [
        bl(0, TZ, "b-z", "applied", ZF),
        bl(1, "territory", "b-t", "applied", ZF),
        bl(2, TZ, "b-t-z", "applied", ZF),
        bl(3, "territory", "b-bad", "invalid", None, "bad-file"),
    ]
    case("territories", {ZF: zbase, WF: wbase}, {"changes": changes, "blocks": blocks}, {ZF: zout, WF: wout},
         {"entries": entries, "files": [fl(WF, "written"), fl(ZF, "written")]})

def run():
    territories()

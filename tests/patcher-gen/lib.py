import json, os, shutil, hashlib
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'patcher')

def fp(tokens):
    return 'sha256:' + hashlib.sha256('\n'.join(tokens).encode()).hexdigest()

def rep(text, old, new, count=1):
    assert text.count(old) == count, (old, text.count(old))
    return text.replace(old, new)

def write(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(content.encode('utf-8') if isinstance(content, str) else content)

import re
def N(text):
    return '#N' + text + '#'

def dump(o):
    t = json.dumps(o, indent=4, ensure_ascii=False) + '\n'
    return re.sub(r'"#N([^#]*)#"', r'\1', t)

def case(name, base, project, expected, report):
    d = os.path.join(ROOT, name)
    if os.path.exists(d):
        shutil.rmtree(d)
    for rel, content in base.items():
        write(os.path.join(d, 'base', rel), content)
    p = {"format": "dzstudio-project", "version": 2, "map": project.get("map", "chernarusplus")}
    if "slug" in project:
        p["slug"] = project["slug"]
    p["blocks"] = project.get("blocks", [])
    p["changes"] = project.get("changes", [])
    write(os.path.join(d, 'project.json'), dump(p))
    for rel, content in expected.items():
        write(os.path.join(d, 'expected', rel), content)
    r = {"format": "dzstudio-patch-report", "version": 1,
         "entries": report.get("entries", []), "files": report.get("files", []), "notes": report.get("notes", [])}
    write(os.path.join(d, 'report.json'), dump(r))

def ch(i, op, kind, key, status, file=None, field=None, detail=None, current=None):
    e = {"what": "change", "index": i, "op": op, "kind": kind, "key": key}
    if field is not None: e["field"] = field
    e["status"] = status
    if file: e["file"] = file
    if detail: e["detail"] = detail
    if current is not None: e["current"] = current
    return e

def bl(i, kind, id, status, file=None, detail=None):
    e = {"what": "block", "index": i, "kind": kind, "id": id, "status": status}
    if file: e["file"] = file
    if detail: e["detail"] = detail
    return e

def reg(id, value, status, file="cfggameplay.json", field="WorldsData.objectSpawnersArr"):
    e = {"what": "registration"}
    if id: e["id"] = id
    e["field"] = field
    e["value"] = value
    e["status"] = status
    e["file"] = file
    return e

def fl(path, status, detail=None):
    e = {"path": path, "status": status}
    if detail: e["detail"] = detail
    return e

def S(key, field, frm=None, to=None, kind="event", has_from=True):
    c = {"op": "set", "block": kind, "key": key, "field": field}
    if has_from and frm is not None: c["from"] = frm
    c["to"] = to
    return c

def R(key, kind="event"):
    return {"op": "remove", "block": kind, "key": key}

def jtext(v, indent='', unit='    '):
    """JSON as the patcher writes it: scalar arrays on one line; N("…") keeps a number's text."""
    if isinstance(v, bool):
        return 'true' if v else 'false'
    if isinstance(v, str):
        if v.startswith('#N') and v.endswith('#'):
            return v[2:-1]
        return json.dumps(v, ensure_ascii=False)
    if isinstance(v, (int, float)):
        return str(v)
    if isinstance(v, list):
        if not v:
            return '[]'
        if all(not isinstance(i, (list, dict)) for i in v):
            return '[' + ', '.join(jtext(i) for i in v) + ']'
        return '[' + ','.join('\n' + indent + unit + jtext(i, indent + unit, unit) for i in v) + '\n' + indent + ']'
    if not v:
        return '{}'
    return '{' + ','.join('\n' + indent + unit + json.dumps(k) + ': ' + jtext(x, indent + unit, unit) for k, x in v.items()) + '\n' + indent + '}'

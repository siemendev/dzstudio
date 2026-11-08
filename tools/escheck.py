#!/usr/bin/env python3
"""A static check of dzStudio's Enforce Script without the game.

DayZ compiles the scripts only when the game starts, so a plain build (CI) cannot find script errors. This reads every
class of dzstudio/Scripts and reports the errors the game compiler would stop at, as far as they can be decided
without the game's own scripts:

- a member (method, field) that the receiver's class does not have, when the receiver's class and all its bases are
  dzStudio classes; a protected or private member used from outside its class line
- a call with too many or too few arguments for such a method
- a variable declared twice in one function (Enforce allows a name only once per function, any block)
- a variable named like a type (map, set, out …)
- `override` without a method of that name in the (known) bases
- a DZS_ class that does not exist, or one of a later script module used from an earlier one
- unbalanced brackets

Engine classes and functions (Widget, GetGame …) are unknown here and never reported.

    python3 tools/escheck.py [dzstudio/Scripts] [--only FILE ...]
Exit status 1 when something was found.
"""
import os
import re
import sys

PRIMITIVES = {"int", "float", "bool", "string", "vector", "void", "auto", "typename", "func", "Class", "array",
              "map", "set", "ref", "autoptr", "TStringArray", "TIntArray", "TFloatArray", "TBoolArray", "TStringMap"}
MODIFIERS = {"static", "protected", "private", "override", "proto", "native", "ref", "const", "owned", "notnull", "out",
             "inout", "external", "volatile", "event", "reference", "autoptr", "sealed", "modded", "local", "private"}
KEYWORDS = {"if", "else", "for", "foreach", "while", "do", "return", "break", "continue", "switch", "case", "default",
            "new", "delete", "this", "super", "null", "NULL", "true", "false", "class", "enum", "typedef", "extends",
            "thread", "goto", "typename", "and", "or", "not", "volatile"}
RESERVED_VAR = {"map", "set", "array", "out", "inout", "string", "int", "float", "bool", "vector", "class", "typename",
                "func", "ref", "auto", "const", "static", "new", "this", "super", "null", "Class", "Object"}
# what every class has (Class / Managed in the engine)
BUILTIN_MEMBERS = {"Cast", "CastTo", "ClassName", "Type", "IsInherited", "ToString", "GetDebugName", "IsInstanceOf"}
MODULES = {"1_Core": 1, "2_GameLib": 2, "3_Game": 3, "4_World": 4, "5_Mission": 5}

TOKEN = re.compile(r'''
    (?P<ws>\s+) |
    (?P<lc>//[^\n]*) |
    (?P<bc>/\*.*?\*/) |
    (?P<str>"(?:\\.|[^"\\])*") |
    (?P<chr>'(?:\\.|[^'\\])*') |
    (?P<num>\d+\.\d*(?:[eE][-+]?\d+)?|\d*\.\d+(?:[eE][-+]?\d+)?|0[xX][0-9a-fA-F]+|\d+) |
    (?P<id>[A-Za-z_][A-Za-z0-9_]*) |
    (?P<op>::|==|!=|<=|>=|&&|\|\||\+\+|--|\+=|-=|\*=|/=|<<|>>|&=|\|=|\^=|%=|[{}()\[\];,.<>=+\-*/%!&|^~?:#@])
''', re.S | re.X)


class Tok:
    __slots__ = ("k", "t", "line")

    def __init__(self, k, t, line):
        self.k, self.t, self.line = k, t, line

    def __repr__(self):
        return self.t


def tokenize(text):
    out = []
    line = 1
    pos = 0
    n = len(text)
    while pos < n:
        m = TOKEN.match(text, pos)
        if not m:
            out.append(Tok("op", text[pos], line))
            pos += 1
            continue
        kind = m.lastgroup
        s = m.group(kind)
        if kind not in ("ws", "lc", "bc"):
            out.append(Tok(kind, s, line))
        line += s.count("\n")
        pos = m.end()
    return out


class Method:
    def __init__(self, name, ret, params, mods, line, cls, file):
        self.name, self.ret, self.params, self.mods, self.line, self.cls, self.file = name, ret, params, mods, line, cls, file
        self.body = None
        # overloads of the same name in the same class (Enforce allows them)
        self.alts = []

    def arity(self):
        need = sum(1 for p in self.params if not p["default"])
        return need, len(self.params)


class Field:
    def __init__(self, name, type_, mods, line, cls):
        self.name, self.type, self.mods, self.line, self.cls = name, type_, mods, line, cls


class Klass:
    def __init__(self, name, base, file, line, module, modded):
        self.name, self.base, self.file, self.line, self.module, self.modded = name, base, file, line, module, modded
        self.methods = {}
        self.fields = {}


class Checker:
    def __init__(self, root):
        self.root = root
        self.classes = {}
        self.globals = {}
        self.enums = set()
        self.typedefs = {}
        self.problems = []
        self.files = []
        # engine functions dzStudio calls (tools/escheck-engine.txt); None while harvesting it
        self.engine = set()
        self.harvest = None

    # ---- reading ----

    def module_of(self, path):
        for part in path.replace("\\", "/").split("/"):
            if part in MODULES:
                return MODULES[part]
        return 9

    def read_all(self):
        for dirpath, _, names in os.walk(self.root):
            for nm in sorted(names):
                if nm.endswith(".c"):
                    p = os.path.join(dirpath, nm)
                    with open(p, encoding="utf-8", errors="replace") as f:
                        toks = tokenize(f.read())
                    self.files.append((p, toks))
                    self.declarations(p, toks)

    def report(self, path, line, msg):
        self.problems.append((os.path.relpath(path), line, msg))

    def skip_balanced(self, toks, i, open_, close):
        depth = 0
        while i < len(toks):
            if toks[i].t == open_:
                depth += 1
            elif toks[i].t == close:
                depth -= 1
                if depth == 0:
                    return i
            i += 1
        return len(toks) - 1

    def read_type(self, toks, i):
        """A type at i (ident, generics, [] suffix); returns (text, next index) or (None, i)."""
        if i >= len(toks) or toks[i].k != "id" or toks[i].t in KEYWORDS - {"typename"}:
            return None, i
        t = toks[i].t
        j = i + 1
        if j < len(toks) and toks[j].t == "<":
            depth = 0
            k = j
            while k < len(toks):
                if toks[k].t == "<":
                    depth += 1
                elif toks[k].t == ">":
                    depth -= 1
                    if depth == 0:
                        break
                elif toks[k].t == ">>":
                    depth -= 2
                    if depth <= 0:
                        break
                elif toks[k].t in (";", "{", "}", "(", ")", "="):
                    return None, i
                k += 1
            t = t + "".join(x.t for x in toks[j:k + 1])
            j = k + 1
        return t, j

    def declarations(self, path, toks):
        module = self.module_of(path)
        i = 0
        depth_check = []
        n = len(toks)
        while i < n:
            t = toks[i]
            if t.t == "[":
                i = self.skip_balanced(toks, i, "[", "]") + 1
                continue
            if t.t == "enum" and i + 1 < n:
                self.enums.add(toks[i + 1].t)
                j = i
                while j < n and toks[j].t != "{":
                    j += 1
                i = self.skip_balanced(toks, j, "{", "}") + 1
                continue
            if t.t == "typedef":
                j = i + 1
                typ, j = self.read_type(toks, j)
                if j < n:
                    self.typedefs[toks[j].t] = typ
                while j < n and toks[j].t != ";":
                    j += 1
                i = j + 1
                continue
            modded = False
            if t.t in ("modded", "sealed") and i + 1 < n and toks[i + 1].t == "class":
                modded = t.t == "modded"
                i += 1
                t = toks[i]
            if t.t == "class":
                name = toks[i + 1].t
                j = i + 2
                base = None
                if toks[j].t in (":", "extends"):
                    base = toks[j + 1].t
                    j += 2
                while j < n and toks[j].t != "{":
                    j += 1
                end = self.skip_balanced(toks, j, "{", "}")
                if modded and name in self.classes:
                    k = self.classes[name]
                else:
                    k = Klass(name, base, path, toks[i].line, module, modded)
                    if name not in self.classes or not modded:
                        self.classes[name] = k
                self.members(path, toks, j + 1, end, k)
                i = end + 1
                continue
            # a global function or variable
            j = i
            mods = set()
            while j < n and toks[j].t in MODIFIERS:
                mods.add(toks[j].t)
                j += 1
            typ, j2 = self.read_type(toks, j)
            if typ and j2 < n and toks[j2].k == "id" and j2 + 1 < n and toks[j2 + 1].t in ("=", ";", "["):
                self.globals[toks[j2].t] = Field(toks[j2].t, typ, mods, toks[j2].line, None)
                while j2 < n and toks[j2].t != ";":
                    j2 += 1
                i = j2 + 1
                continue
            if typ and j2 < n and toks[j2].k == "id" and j2 + 1 < n and toks[j2 + 1].t == "(":
                name = toks[j2].t
                close = self.skip_balanced(toks, j2 + 1, "(", ")")
                params = self.params(toks, j2 + 2, close)
                m = Method(name, typ, params, mods, toks[j2].line, None, path)
                self.globals[name] = m
                k2 = close + 1
                if k2 < n and toks[k2].t == "{":
                    end = self.skip_balanced(toks, k2, "{", "}")
                    m.body = (k2, end)
                    i = end + 1
                    continue
                i = k2 + 1
                continue
            i += 1

    def params(self, toks, a, b):
        out = []
        cur = []
        depth = 0
        for x in toks[a:b]:
            if x.t in ("(", "<", "[", "{"):
                depth += 1
            elif x.t in (")", ">", "]", "}"):
                depth -= 1
            elif x.t == ">>":
                depth -= 2
            if x.t == "," and depth == 0:
                out.append(cur)
                cur = []
            else:
                cur.append(x)
        if cur:
            out.append(cur)
        res = []
        for p in out:
            ts = [x for x in p]
            default = any(x.t == "=" for x in ts)
            if default:
                ts = ts[:[x.t for x in ts].index("=")]
            words = [x for x in ts if x.t not in MODIFIERS or x.t == "ref"]
            mods = [x.t for x in ts if x.t in MODIFIERS]
            if not words:
                continue
            name = words[-1].t
            typ, _ = self.read_type(words, 0) if words[0].t != "ref" else self.read_type(words, 1)
            res.append({"name": name, "type": typ or words[0].t, "default": default, "mods": mods,
                        "line": words[-1].line})
        if len(res) == 1 and res[0]["name"] == "void":
            return []
        return res

    def members(self, path, toks, a, b, k):
        i = a
        while i < b:
            t = toks[i]
            if t.t == "[":
                i = self.skip_balanced(toks, i, "[", "]") + 1
                continue
            if t.t == ";":
                i += 1
                continue
            mods = set()
            while i < b and toks[i].t in MODIFIERS:
                mods.add(toks[i].t)
                i += 1
            if i >= b:
                break
            if toks[i].t == "enum" or toks[i].t == "class":
                j = i
                while j < b and toks[j].t != "{":
                    j += 1
                i = self.skip_balanced(toks, j, "{", "}") + 1
                continue
            typ, j = self.read_type(toks, i)
            if not typ:
                i += 1
                continue
            # constructor / destructor: void Name( / void ~Name(
            if j < b and toks[j].t == "~":
                j += 1
            if j < b and toks[j].k == "id" and j + 1 < b and toks[j + 1].t == "(":
                name = toks[j].t
                close = self.skip_balanced(toks, j + 1, "(", ")")
                params = self.params(toks, j + 2, close)
                m = Method(name, typ, params, mods, toks[j].line, k.name, path)
                if name in k.methods and k.methods[name].file == path and "override" not in mods:
                    k.methods[name].alts.append(m)
                else:
                    k.methods[name] = m
                e = close + 1
                if e < b and toks[e].t == "{":
                    end = self.skip_balanced(toks, e, "{", "}")
                    m.body = (e, end)
                    i = end + 1
                else:
                    while e < b and toks[e].t != ";":
                        e += 1
                    i = e + 1
                continue
            # fields: Type a [= x], b;
            while j < b and toks[j].k == "id":
                k.fields[toks[j].t] = Field(toks[j].t, typ, mods, toks[j].line, k.name)
                j += 1
                if j < b and toks[j].t == "[":
                    j = self.skip_balanced(toks, j, "[", "]") + 1
                if j < b and toks[j].t == "=":
                    depth = 0
                    while j < b:
                        if toks[j].t in ("(", "{", "["):
                            depth += 1
                        elif toks[j].t in (")", "}", "]"):
                            depth -= 1
                        elif toks[j].t in (",", ";") and depth == 0:
                            break
                        j += 1
                if j < b and toks[j].t == ",":
                    j += 1
                    continue
                break
            while j < b and toks[j].t != ";":
                j += 1
            i = j + 1

    # ---- class lines ----

    def chain(self, name):
        """The class and its bases while they are known; (list, fully known)."""
        out = []
        seen = set()
        cur = self.base_name(name)
        while cur:
            if cur in seen:
                break
            seen.add(cur)
            k = self.classes.get(cur)
            if not k:
                return out, cur in ("Managed", "Class")
            out.append(k)
            if k.modded and not k.base:
                return out, False
            cur = k.base
        return out, True

    def base_name(self, t):
        if not t:
            return None
        t = t.strip()
        if t.startswith("ref "):
            t = t[4:]
        if "<" in t:
            return None
        t = self.typedefs.get(t, t)
        if t and "<" in t:
            return None
        return t

    def member(self, cls, name):
        ch, full = self.chain(cls)
        for k in ch:
            if name in k.methods:
                return k.methods[name], full
            if name in k.fields:
                return k.fields[name], full
        return None, full

    def is_type(self, name):
        return (name in PRIMITIVES or name in self.classes or name in self.enums or name in self.typedefs
                or (name[:1].isupper() and name not in self.globals))

    def subclass_of(self, cls, base):
        ch, _ = self.chain(cls)
        return any(k.name == base for k in ch)

    # ---- checks ----

    def check(self, only=None):
        for path, toks in self.files:
            if only and os.path.abspath(path) not in only:
                continue
            self.brackets(path, toks)
        for k in list(self.classes.values()):
            if only and os.path.abspath(k.file) not in only:
                continue
            ch, full = self.chain(k.name)
            all_methods = []
            for m0 in k.methods.values():
                all_methods.append(m0)
                all_methods.extend(m0.alts)
            for m in all_methods:
                if "override" in m.mods and full and m.name != k.name:
                    found = any(m.name in b.methods for b in ch[1:]) or m.name in BUILTIN_MEMBERS
                    if not found:
                        self.report(m.file, m.line, "override %s.%s: no %s in %s's bases" % (k.name, m.name, m.name, k.name))
                if m.body:
                    self.body(m.file, self.tokens_of(m.file), m, k)
        for g in self.globals.values():
            if isinstance(g, Method) and g.body and (not only or os.path.abspath(g.file) in only):
                self.body(g.file, self.tokens_of(g.file), g, None)

    def tokens_of(self, path):
        for p, t in self.files:
            if p == path:
                return t
        return []

    def brackets(self, path, toks):
        stack = []
        pairs = {")": "(", "]": "[", "}": "{"}
        for t in toks:
            if t.t in "([{" and t.k == "op":
                stack.append(t)
            elif t.t in ")]}" and t.k == "op":
                if not stack or stack[-1].t != pairs[t.t]:
                    self.report(path, t.line, "unbalanced %s" % t.t)
                    return
                stack.pop()
        if stack:
            self.report(path, stack[-1].line, "unclosed %s" % stack[-1].t)

    def body(self, path, toks, m, k):
        a, b = m.body
        locals_ = {}
        for p in m.params:
            if p["name"] in locals_:
                self.report(path, p["line"], "parameter %s twice" % p["name"])
            locals_[p["name"]] = p["type"]
            if p["name"] in RESERVED_VAR:
                self.report(path, p["line"], "parameter named like a type: %s" % p["name"])
        module = self.module_of(path)
        i = a + 1
        while i < b:
            t = toks[i]
            prev = toks[i - 1].t if i > 0 else ""
            # declarations
            if t.k == "id" and prev in ("{", "}", ";", "(", ",", ")", "else", ":") and t.t not in KEYWORDS and t.t not in self.globals:
                start = i
                while start < b and toks[start].t in ("ref", "const", "autoptr", "static", "auto"):
                    start += 1
                typ, j = self.read_type(toks, start)
                if typ and j < b and toks[j].k == "id" and toks[j].t not in KEYWORDS and j + 1 < b \
                        and toks[j + 1].t in ("=", ";", ",", ":", "[", ")") and self.is_type(toks[start].t) \
                        and not (prev == "(" and toks[j + 1].t == ")" and toks[i - 2].t not in ("foreach",)) \
                        and toks[start].t != toks[j].t:
                    if prev in ("(", ",") and not self.in_decl_context(toks, i):
                        pass
                    else:
                        self.declare(path, toks, j, typ, locals_)
                        # more declarators: int a, b;
                        q = j + 1
                        while q < b and toks[q].t == "," and prev not in ("(", ","):
                            if q + 2 < b and toks[q + 1].k == "id" and toks[q + 2].t in ("=", ";", ","):
                                self.declare(path, toks, q + 1, typ, locals_)
                                q += 2
                            else:
                                break
                        i = j + 1
                        continue
            # member access
            if t.t == "." and i + 1 < b and toks[i + 1].k == "id":
                self.access(path, toks, i, locals_, k, module)
            # a call without receiver: a method of the class line, a global function, or the engine's
            if t.k == "id" and prev not in (".", "new", "~") and i + 1 < b and toks[i + 1].t == "(" and t.t not in KEYWORDS \
                    and t.t not in locals_ and t.t not in self.classes and not self.is_primitive(t.t):
                self.unqualified(path, t, k, toks, i)
            # a field of the class line: m_X
            if t.k == "id" and t.t.startswith("m_") and prev != "." and t.t not in locals_ and k:
                mem, full = self.member(k.name, t.t)
                if mem is None and full:
                    self.report(path, t.line, "%s has no field %s" % (k.name, t.t))
            # new DZS_X(...): the first constructor in the class line (Enforce inherits them)
            if t.t == "new" and i + 2 < b and toks[i + 1].t in self.classes and toks[i + 2].t == "(":
                ck = self.classes[toks[i + 1].t]
                close = self.skip_balanced(toks, i + 2, "(", ")")
                args = self.count_args(toks, i + 3, close)
                line_, full = self.chain(ck.name)
                ctor_cls = None
                for c in line_:
                    if c.name in c.methods:
                        ctor_cls = c
                        break
                if ctor_cls:
                    if not self.fits(ctor_cls.name, ctor_cls.name, args):
                        m0 = ctor_cls.methods[ctor_cls.name]
                        need, most = m0.arity()
                        self.report(path, t.line, "new %s takes %s argument(s), got %d" % (ck.name, need if need == most else "%d–%d" % (need, most), args))
                elif args > 0 and full:
                    self.report(path, t.line, "new %s: no constructor takes arguments" % ck.name)
            if t.k == "id" and t.t.startswith("DZS_") and prev != "." and t.t not in self.classes and t.t not in self.globals \
                    and t.t not in self.enums and t.t not in self.typedefs and t.t not in locals_ \
                    and not (k and self.member(k.name, t.t)[0]):
                self.report(path, t.line, "unknown name %s" % t.t)
            if t.k == "id" and t.t in self.classes and prev != ".":
                ck = self.classes[t.t]
                if ck.module > module and not ck.modded:
                    self.report(path, t.line, "%s is in a later script module (%d) than this file (%d)" % (t.t, ck.module, module))
            i += 1

    def is_primitive(self, name):
        return name in PRIMITIVES

    def unqualified(self, path, t, k, toks, i):
        name = t.t
        if name in self.globals or name in self.enums or name in self.typedefs:
            if isinstance(self.globals.get(name), Method):
                g = self.globals[name]
                close = self.skip_balanced(toks, i + 1, "(", ")")
                args = self.count_args(toks, i + 2, close)
                need, most = g.arity()
                if args < need or args > most:
                    self.report(path, t.line, "%s takes %s argument(s), got %d" % (name, need if need == most else "%d–%d" % (need, most), args))
            return
        if k:
            mem, full = self.member(k.name, name)
            if isinstance(mem, Method):
                close = self.skip_balanced(toks, i + 1, "(", ")")
                args = self.count_args(toks, i + 2, close)
                need, most = mem.arity()
                if not self.fits(k.name, name, args):
                    self.report(path, t.line, "%s.%s takes %s argument(s), got %d" % (k.name, name, need if need == most else "%d–%d" % (need, most), args))
                return
            if mem is not None:
                return
            if not full:
                return
        if self.harvest is not None:
            self.harvest.add(name)
            return
        if name not in self.engine:
            self.report(path, t.line, "unknown function %s (not in the class line, not global, not among the engine functions dzStudio uses)" % name)

    def in_decl_context(self, toks, i):
        """A declaration right after '(' or ',' only counts in a foreach/for header or a catch-like spot."""
        depth = 0
        q = i - 1
        while q > 0:
            if toks[q].t == ")":
                depth += 1
            elif toks[q].t == "(":
                if depth == 0:
                    return toks[q - 1].t in ("foreach", "for")
                depth -= 1
            elif toks[q].t in (";", "{", "}"):
                return False
            q -= 1
        return False

    def declare(self, path, toks, j, typ, locals_):
        name = toks[j].t
        if name in locals_:
            self.report(path, toks[j].line, "variable %s declared twice in one function" % name)
        if name in RESERVED_VAR:
            self.report(path, toks[j].line, "variable named like a type: %s" % name)
        locals_[name] = typ

    def type_of(self, toks, i, locals_, k):
        """Type of the receiver ending at token i-1 (the token before '.'), or None."""
        t = toks[i - 1]
        if t.k == "id":
            before = toks[i - 2].t if i >= 2 else ""
            if before == ".":
                return None
            if t.t == "this" and k:
                return k.name
            if t.t == "super" and k:
                return k.base
            if t.t in locals_:
                return locals_[t.t]
            if k:
                mem, _ = self.member(k.name, t.t)
                if isinstance(mem, Field):
                    return mem.type
            if t.t in self.classes:
                return "static:" + t.t
            return None
        if t.t == ")":
            # a call: X.Get() or Get()
            q = self.skip_back(toks, i - 1)
            if q is None or q < 1 or toks[q - 1].k != "id":
                return None
            fn = toks[q - 1].t
            if q >= 2 and toks[q - 2].t == ".":
                owner = self.type_of(toks, q - 2 + 0, locals_, k) if False else self.type_of_expr(toks, q - 2, locals_, k)
                if not owner:
                    return None
                cls = owner[7:] if owner.startswith("static:") else owner
                mem, _ = self.member(self.base_name(cls), fn) if self.base_name(cls) else (None, False)
                if isinstance(mem, Method):
                    return mem.ret
                return None
            if fn in self.globals and isinstance(self.globals[fn], Method):
                return self.globals[fn].ret
            if k:
                mem, _ = self.member(k.name, fn)
                if isinstance(mem, Method):
                    return mem.ret
            return None
        return None

    def type_of_expr(self, toks, dot, locals_, k):
        """Type of what stands before the '.' at index dot."""
        return self.type_of(toks, dot, locals_, k)

    def skip_back(self, toks, i):
        depth = 0
        while i >= 0:
            if toks[i].t == ")":
                depth += 1
            elif toks[i].t == "(":
                depth -= 1
                if depth == 0:
                    return i
            i -= 1
        return None

    def access(self, path, toks, i, locals_, k, module):
        owner = self.type_of(toks, i, locals_, k)
        if not owner:
            return
        static = owner.startswith("static:")
        cls = self.base_name(owner[7:] if static else owner)
        if not cls or cls not in self.classes:
            return
        name = toks[i + 1].t
        if name in BUILTIN_MEMBERS:
            return
        mem, full = self.member(cls, name)
        if mem is None:
            if full:
                self.report(path, toks[i + 1].line, "%s has no member %s" % (cls, name))
            return
        if "protected" in mem.mods or "private" in mem.mods:
            inside = k and (k.name == mem.cls or self.subclass_of(k.name, mem.cls) or self.subclass_of(mem.cls, k.name))
            if not inside:
                self.report(path, toks[i + 1].line, "%s.%s is %s" % (cls, name, "protected" if "protected" in mem.mods else "private"))
        if isinstance(mem, Method) and i + 2 < len(toks) and toks[i + 2].t == "(":
            close = self.skip_balanced(toks, i + 2, "(", ")")
            args = self.count_args(toks, i + 3, close)
            need, most = mem.arity()
            if not self.fits(cls, name, args):
                self.report(path, toks[i + 1].line, "%s.%s takes %s argument(s), got %d" % (cls, name, need if need == most else "%d–%d" % (need, most), args))

    def fits(self, cls, name, args):
        ch, _ = self.chain(cls)
        for k in ch:
            m = k.methods.get(name)
            if not m:
                continue
            for x in [m] + m.alts:
                need, most = x.arity()
                if need <= args <= most:
                    return True
        return False

    def count_args(self, toks, a, b):
        if a >= b:
            return 0
        depth = 0
        n = 1
        for x in toks[a:b]:
            if x.t in ("(", "[", "{"):
                depth += 1
            elif x.t in (")", "]", "}"):
                depth -= 1
            elif x.t == "," and depth == 0:
                n += 1
        return n


ENGINE_LIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "escheck-engine.txt")


def main():
    args = sys.argv[1:]
    root = "dzstudio/Scripts"
    only = None
    harvest = False
    if args and not args[0].startswith("--"):
        root = args.pop(0)
    if args and args[0] == "--harvest":
        harvest = True
        args.pop(0)
    if args and args[0] == "--only":
        only = {os.path.abspath(p) for p in args[1:]}
    c = Checker(root)
    if harvest:
        c.harvest = set()
    elif os.path.exists(ENGINE_LIST):
        with open(ENGINE_LIST, encoding="utf-8") as f:
            c.engine = {l.strip() for l in f if l.strip() and not l.startswith("#")}
    c.read_all()
    c.check(only)
    if harvest:
        with open(ENGINE_LIST, "w", encoding="utf-8") as f:
            f.write("# Engine functions dzStudio calls without a receiver (written by escheck.py --harvest from code that\n")
            f.write("# compiles in the game). A new engine function used in a script goes in here by hand.\n")
            for n in sorted(c.harvest):
                f.write(n + "\n")
        print("escheck: %d engine function(s) written to %s" % (len(c.harvest), ENGINE_LIST), file=sys.stderr)
    for p, line, msg in sorted(set(c.problems)):
        print("%s:%d: %s" % (p, line, msg))
    print("escheck: %d file(s), %d class(es), %d problem(s)" % (len(c.files), len(c.classes), len(set(c.problems))), file=sys.stderr)
    return 1 if c.problems else 0


if __name__ == "__main__":
    sys.exit(main())

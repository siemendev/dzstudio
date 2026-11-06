#!/usr/bin/env python3
"""Builds dzstudio/data/pinsel-sets.txt, the brush sets that ship with DZSTUDIO, from the catalog's console list.

One set per tree species found in the tree folders (all sizes and growth forms, file names t_<Species>_… and
b_<Species>_…; dead wood d_…, cut stumps, test and trail-marked models left out), one set per variant that is a model
of its own (summer, winter, late autumn, dark, frozen, swamp), and "Büsche gemischt" with every bush model.
Height offsets come from input/boxes-<version>.txt (measured in the game: start DZSTUDIO with -dzsBrushBoxes): the
model's footprint radius 0.3 m above its foot (eight level rays against the geometry) times tan 15°, so that the
foot sinks as far as the ground rises over that radius on a 15° slope. Roots make the measured radius large for big
broadleaf trees, so it is capped at 1 m; where no ray hit, 0.3 m is used.
Usage: build_sets.py [--boxes FILE]"""
import math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
LIST = os.path.join(ROOT, 'dzstudio', 'data', 'konsolenliste.txt')
OUT = os.path.join(ROOT, 'dzstudio', 'data', 'pinsel-sets.txt')

TREES = {
    'piceaabies': ('Fichte', 'Spruce'), 'pinussylvestris': ('Kiefer', 'Pine'), 'betulapendula': ('Birke', 'Birch'),
    'fagussylvatica': ('Buche', 'Beech'), 'quercusrobur': ('Eiche', 'Oak'), 'larixdecidua': ('Lärche', 'Larch'),
    'fraxinusexcelsior': ('Esche', 'Ash'), 'juglansregia': ('Walnuss', 'Walnut'), 'malusdomestica': ('Apfelbaum', 'Apple tree'),
    'populusalba': ('Silberpappel', 'White poplar'), 'populusnigra': ('Schwarzpappel', 'Black poplar'),
    'prunusdomestica': ('Pflaumenbaum', 'Plum tree'), 'pyruscommunis': ('Birnbaum', 'Pear tree'),
    'robiniapseudoacacia': ('Robinie', 'Black locust'), 'salixalba': ('Silberweide', 'White willow'),
    'sorbus': ('Eberesche', 'Rowan'), 'carpinus': ('Hainbuche', 'Hornbeam'), 'acer': ('Ahorn', 'Maple'),
    'naked': ('Kahler Baum', 'Bare tree'), 'town': ('Stadtbaum', 'Town tree'),
}
# the same species with an E (another model series)
ALIAS = {'betulapendulae': 'betulapendula', 'fagussylvaticae': 'fagussylvatica'}
VARIANTS = {'summer': ('Sommer', 'summer'), 'winter': ('Winter', 'winter'), 'latefall': ('Spätherbst', 'late autumn'),
            'dark': ('dunkel', 'dark'), 'frozen': ('gefroren', 'frozen'), 'swamp': ('Sumpf', 'swamp'),
            'leafless': ('kahl', 'leafless')}
TREE_DIRS = ('\\tree\\',)
SAKHAL_DIRS = ('\\dark\\', '\\frozen\\', '\\latefall\\', '\\winter\\')
BUSH_DIRS = ('\\bush\\',)
TAN15 = math.tan(math.radians(15))


def parse(path):
    low = path.lower()
    base = low.split('\\')[-1][:-4]
    if '\\cutted' in low or '_test' in base or '_trail_' in base:
        return None
    m = re.match(r'([tbd])_([a-z]+)(?:_(.*))?$', base)
    if not m:
        return None
    kind, species, rest = m.group(1), m.group(2), m.group(3) or ''
    species = ALIAS.get(species, species)
    variant = ''
    for part in rest.split('_'):
        if part in VARIANTS:
            variant = part
    return kind, species, variant


def main():
    boxes = {}
    args = sys.argv[1:]
    bf = os.path.join(HERE, 'input', 'boxes-1.30.txt')
    if '--boxes' in args:
        bf = args[args.index('--boxes') + 1]
    if os.path.exists(bf):
        for line in open(bf, encoding='utf-8'):
            p = line.rstrip('\n').split('\t')
            if len(p) >= 7:
                boxes[p[0].lower()] = [float(x) for x in p[1:]]
    names = []
    for line in open(LIST, encoding='utf-8'):
        if line.startswith('#'):
            continue
        n = line.split('\t')[0]
        if n.lower().startswith('dz\\plants'):
            names.append(n)
    sets = {}
    bushes = []
    missing = []
    for n in names:
        low = n.lower()
        r = parse(n)
        if not r:
            continue
        kind, species, variant = r
        in_tree = any(d in low for d in TREE_DIRS) or any(d in low for d in SAKHAL_DIRS)
        in_bush = any(d in low for d in BUSH_DIRS)
        if kind == 'd':
            continue
        if species in TREES and (in_tree or in_bush) and not (in_bush and kind == 'b' and not in_tree):
            sets.setdefault((species, variant), []).append(n)
        elif in_bush or (kind == 'b' and species not in TREES and in_tree):
            bushes.append(n)
        else:
            missing.append(n)

    def offset(n):
        b = boxes.get(n.lower())
        if not b:
            return 0.0
        r = b[6] if len(b) > 6 else -1
        if r <= 0:
            r = 0.3
        r = min(r, 1.0)
        return -math.ceil(r * TAN15 * 100) / 100

    out = ['# dzstudio pinsel-sets 1 · DayZ 1.30 · built by tools/brush/build_sets.py',
           '# set<TAB>German name<TAB>English name, then entries: name share randomYaw tilt slope slopeMax heightOffset scaleMin scaleMax']
    def key(k):
        sp, var = k
        return (TREES[sp][0].lower().replace('ä', 'a'), var != '', VARIANTS.get(var, ('', ''))[0].lower())
    for k in sorted(sets, key=key):
        sp, var = k
        de, en = TREES[sp]
        if var:
            de = '%s (%s)' % (de, VARIANTS[var][0])
            en = '%s (%s)' % (en, VARIANTS[var][1])
        out.append('set\tBäume: %s\tTrees: %s' % (de, en))
        for n in sorted(sets[k], key=str.lower):
            out.append('%s\t1\t1\t3\t0\t15\t%.2f\t0.9\t1.1' % (n, offset(n)))
    out.append('set\tBüsche gemischt\tMixed bushes')
    for n in sorted(bushes, key=str.lower):
        out.append('%s\t1\t1\t3\t0\t15\t%.2f\t0.9\t1.1' % (n, offset(n)))
    open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('%d sets, %d entries, %d bushes, %d with box, not used: %d' % (len(sets) + 1, sum(len(v) for v in sets.values()) + len(bushes), len(bushes), len(boxes), len(missing)))
    if '--all' in args:
        for n in all_models_for_boxes(sets, bushes):
            print(n)


def all_models_for_boxes(sets, bushes):
    r = []
    for v in sets.values():
        r += v
    return r + bushes


if __name__ == '__main__':
    main()

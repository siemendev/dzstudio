#!/usr/bin/env python3
# Writes the conformance cases into ../patcher/<case>/ (base/, project.json, expected/, report.json).
# Run them in the game: -dzsPatchTest=<the patcher folder, copied below $profile:> -dzsPatchQuit
import os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lib, cases_events, cases_spawns, cases_json, cases_misc, cases_loot

if os.path.exists(lib.ROOT):
    shutil.rmtree(lib.ROOT)
for m in (cases_events, cases_spawns, cases_json, cases_misc, cases_loot):
    m.run()
print(len(os.listdir(lib.ROOT)), "cases in", os.path.normpath(lib.ROOT))

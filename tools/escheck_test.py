#!/usr/bin/env python3
"""Tests of escheck.py: python3 tools/escheck_test.py"""
import os
import tempfile
import unittest

import escheck


def run(source):
    with tempfile.TemporaryDirectory() as d:
        os.makedirs(os.path.join(d, "5_Mission"))
        with open(os.path.join(d, "5_Mission", "a.c"), "w") as f:
            f.write(source)
        c = escheck.Checker(d)
        c.read_all()
        c.check()
        return sorted(msg for _, _, msg in c.problems)


class Clashes(unittest.TestCase):
    def test_field_shadowing_a_base_field(self):
        p = run("""
class DZS_Base : Managed
{
	protected string m_Title;
}
class DZS_Sub : DZS_Base
{
	protected string m_Title;
	void F() { m_Title = ""; }
}
""")
        self.assertEqual(p, ["DZS_Sub.m_Title shadows the field of DZS_Base"])

    def test_field_and_method_of_one_name(self):
        p = run("""
class DZS_Base : Managed
{
	int m_A;
	void Size() {}
}
class DZS_Sub : DZS_Base
{
	int Size;
	void m_A() {}
}
""")
        self.assertEqual(p, ["field DZS_Sub.Size is named like a method of DZS_Base",
                             "method DZS_Sub.m_A is named like a field of DZS_Base"])

    def test_method_named_like_an_engine_or_own_type(self):
        p = run("""
class DZS_Other : Managed {}
class DZS_A : Managed
{
	protected void Input(int a) {}
	static int Pos() { return 0; }
	void DZS_Other() {}
}
""")
        self.assertEqual(p, ["method named like a type or engine name: DZS_A.DZS_Other",
                             "method named like a type or engine name: DZS_A.Input",
                             "method named like a type or engine name: DZS_A.Pos"])

    def test_variables_named_like_types(self):
        p = run("""
class DZS_A : Managed
{
	protected string set;
	static string F(string map)
	{
		string out = map;
		return out;
	}
}
""")
        self.assertEqual(p, ["field named like a type: DZS_A.set", "parameter named like a type: map",
                             "variable named like a type: out"])

    def test_no_false_positives(self):
        p = run("""
class DZS_Base : Managed
{
	protected ref map<string, ref array<int>> m_Map = new map<string, ref array<int>>(), m_Other;
	void DZS_Base() {}
	void ~DZS_Base() {}
	void Draw(int a) {}
}
class DZS_Sub : DZS_Base
{
	protected ref map<string, bool> m_Own = new map<string, bool>();
	void DZS_Sub() {}
	override void Draw(int a) { m_Map.Clear(); }
	void LayerInput() {}
}
class DZS_Unrelated : Managed
{
	protected ref map<string, bool> m_Own;
	void Draw(int a) {}
}
""")
        self.assertEqual(p, [])


if __name__ == "__main__":
    unittest.main()

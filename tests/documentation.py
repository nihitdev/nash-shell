#!/usr/bin/env python3
"""Execute shipped config recipes with isolated tools; verify local documentation links."""
import os, pathlib, re, subprocess, sys, tempfile, unittest
NASH=str(pathlib.Path(sys.argv.pop(1)).resolve())
ROOT=pathlib.Path(__file__).resolve().parents[1]
class DocumentationTests(unittest.TestCase):
    def run_source(self,source,env):
        return subprocess.run([NASH,'--no-config','-c',source],env=env,capture_output=True,text=True,timeout=5)
    def test_example_config(self):
        with tempfile.TemporaryDirectory() as d:
            directory=pathlib.Path(d)
            # A documentation recipe must work regardless of installed optional tools.
            for name in ['eza','bat','batcat','fastfetch']:
                tool=directory/name
                tool.write_text('#!/bin/sh\nprintf "'+name+'-fixture\\n"\n')
                tool.chmod(0o755)
            env=dict(os.environ,PATH=d+':/usr/bin:/bin',HOME=d,XDG_CONFIG_HOME=d,XDG_DATA_HOME=d)
            result=self.run_source(f'source {ROOT}/examples/config.nash\nconfig get prompt.engine\nll\nview README.md',env)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stdout,'native\neza-fixture\nbat-fixture\n')
            self.assertNotIn('fastfetch-fixture',result.stdout,'example should leave the banner opt-in')
    def test_fastfetch_recipe(self):
        with tempfile.TemporaryDirectory() as d:
            tool=pathlib.Path(d,'fastfetch');tool.write_text('#!/bin/sh\nprintf "banner-fixture\\n"\n');tool.chmod(0o755)
            env=dict(os.environ,PATH=d+':/usr/bin:/bin')
            result=self.run_source("has fastfetch && alias ff='fastfetch'\nff\nhas fastfetch && fastfetch",env)
            self.assertEqual((result.returncode,result.stdout),(0,'banner-fixture\nbanner-fixture\n'),result.stderr)
    def test_local_links(self):
        files=[ROOT/'README.md',*sorted((ROOT/'docs').glob('*.md'))]
        for file in files:
            for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)',file.read_text()):
                if target.startswith(('http:','https:','#','mailto:')):continue
                target=target.split('#',1)[0]
                with self.subTest(file=file.name,target=target):self.assertTrue((file.parent/target).exists())
if __name__=='__main__':unittest.main()

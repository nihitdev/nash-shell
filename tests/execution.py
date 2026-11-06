#!/usr/bin/env python3
"""Black-box shell behavior and Starship protocol regressions."""
import os, pathlib, subprocess, sys, tempfile, unittest
NASH = str(pathlib.Path(sys.argv.pop(1)).resolve())
class ShellTests(unittest.TestCase):
    def run_shell(self, source, **kwargs):
        return subprocess.run([NASH, '--no-config', '-c', source], text=True, capture_output=True, timeout=10, **kwargs)
    def check(self, source, output='', code=0):
        r=self.run_shell(source)
        self.assertEqual((r.returncode,r.stdout),(code,output),r.stderr)
    def test_commands(self):
        self.check("printf '%s\\n' hello", 'hello\n')
        self.check('nash_missing_command_83828', code=127)
        self.check('false',code=1)
    def test_status_and_conditionals(self):
        self.check('false; echo $?; false && echo no; true || echo no; false || echo yes','1\nyes\n')
        self.check('true || false && echo left-associative','left-associative\n')
    def test_quotes(self):
        self.check('X=world; echo "hello $X" \'$X\' a\\ b ""', 'hello world $X a b \n')
        self.check("printf '<%s>\\n' '' a\"b\"c", '<>\n<abc>\n')
        self.check('echo "a\\qb"', 'a\\qb\n')
    def test_variables(self):
        self.check("A='one two'; printf '<%s>\\n' $A; printf '<%s>\\n' \"$A\"", '<one>\n<two>\n<one two>\n')
        self.check('export NASH_TEST=ok; printenv NASH_TEST; NASH_TEST=temp printenv NASH_TEST; echo $NASH_TEST', 'ok\ntemp\nok\n')
        self.check('unset NASH_TEST; echo ${NASH_TEST:-fallback}', 'fallback\n')
    def test_pipelines(self):
        self.check("printf 'abc\\n' | tr a-z A-Z | cat",'ABC\n')
        self.check('yes | head -n 1', 'y\n')
        self.check('false | true; echo $?; true | false; echo $?', '0\n1\n')
        self.check('cd / | cat; pwd', os.getcwd()+'\n')
    def test_substitution(self):
        self.check('echo "a$(printf \'b\\n\\n\')c"', 'abc\n')
        self.check('echo $(echo $(echo nested))','nested\n')
        self.check('echo "$(printf \'%s\' \"hello world\")"', 'hello world\n')
        self.check("echo $(printf '%070000d' 1) | wc -c",'70001\n')
    def test_glob_redirections(self):
        with tempfile.TemporaryDirectory() as d:
            pathlib.Path(d,'a.txt').write_text('A')
            pathlib.Path(d,'b.txt').write_text('B')
            self.check(f'cd {d}; echo *.txt; echo "*.txt"; echo unmatched*', 'a.txt b.txt\n*.txt\nunmatched*\n')
            self.check(f'cd {d}; echo one > out; echo two >> out; cat < out; echo visible', 'one\ntwo\nvisible\n')
            r=self.run_shell(f'cd {d}; cat missing 2> errors; echo $?; cat errors')
            self.assertEqual(r.returncode,0); self.assertTrue(r.stdout.startswith('1\n')); self.assertEqual(r.stderr,'')
            self.check(f'cd {d}; echo 2 > digit; cat digit', '2\n')
    def test_dup_redirections(self):
        pass
        r=self.run_shell('echo stderr >&2; echo stdout')
        self.assertEqual((r.stdout,r.stderr),('stdout\n','stderr\n'))

    def test_background(self):
        self.check('sleep 0.02 & wait; echo $?','0\n')
        self.check('false & wait; echo $?','1\n')
    def test_take_creates_and_enters_directory(self):
        with tempfile.TemporaryDirectory() as d:
            target=pathlib.Path(d, 'new folder', 'nested')
            result=self.run_shell(f'take "{target}"; pwd; echo "$PWD"; echo "$OLDPWD"')
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stdout,str(target)+'\n'+str(target)+'\n'+os.getcwd()+'\n')
            self.assertTrue(target.is_dir())
            self.check(f'take "{target}"; pwd',str(target)+'\n')
    def test_take_errors_do_not_change_directory(self):
        with tempfile.TemporaryDirectory() as d:
            occupied=pathlib.Path(d,'occupied')
            occupied.write_text('not a directory')
            result=self.run_shell(f'take "{occupied}/child"; echo $?; pwd')
            self.assertEqual(result.returncode,0)
            self.assertEqual(result.stdout,'1\n'+os.getcwd()+'\n')
            self.assertIn('take:',result.stderr)
            self.check('take; echo $?', '2\n')
            self.check('take one two; echo $?', '2\n')
    def test_aliases(self):
        self.check("alias hi='echo hello'\nhi world",'hello world\n')
    def test_parse_errors(self):
        for source in ['echo "unfinished','echo hi |','echo >','echo hi &&','cat << EOF','; |']:
            with self.subTest(source=source): self.assertEqual(self.run_shell(source).returncode,2)
    def test_starship_protocol(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d,'starship'); p.write_text('#!/usr/bin/python3\nimport sys,os\nprint("|".join(sys.argv[1:])+"|shell="+os.environ.get("STARSHIP_SHELL",""),end="")\n'); p.chmod(0o755)
            env=dict(os.environ,PATH=d+':'+os.environ['PATH'])
            r=self.run_shell('false; nash-prompt --engine starship',env=env)
            self.assertIn('prompt|--status|1|',r.stdout)
            self.assertIn('|--path|'+os.getcwd(),r.stdout)
            self.assertIn('|shell=nash',r.stdout)
            self.assertIn('|--right|',self.run_shell('nash-prompt --engine starship --right',env=env).stdout)
    @unittest.skipUnless(__import__('shutil').which('starship'),'Starship unavailable')
    def test_real_starship(self):
        with tempfile.TemporaryDirectory() as d:
            config=pathlib.Path(d,'starship.toml')
            config.write_text('add_newline = false\nformat = "$character$status$directory"\n[character]\nsuccess_symbol = "OK"\nerror_symbol = "FAIL"\n[status]\ndisabled = false\nformat = "[$status]($style)"\n[directory]\ntruncation_length = 0\n')
            env=dict(os.environ,STARSHIP_CONFIG=str(config),TERM='xterm-256color')
            r=self.run_shell('false; nash-prompt --engine starship',env=env)
            self.assertIn('FAIL',r.stdout); self.assertIn('1',r.stdout)
            config.write_text('add_newline = false\nformat = "α\\n$git_branch$git_status$character"\n[character]\nsuccess_symbol = "❯"\n')
            subprocess.run(['git','init','-q',d],check=True)
            pathlib.Path(d,'dirty').write_text('x')
            r=self.run_shell(f'cd {d}; nash-prompt --engine starship',env=env)
            self.assertIn('α\n',r.stdout); self.assertIn('❯',r.stdout)
            self.assertTrue('master' in r.stdout or 'main' in r.stdout,r.stdout)
    def test_fallback(self):
        env=dict(os.environ,PATH='/nonexistent',NO_COLOR='1')
        r=self.run_shell('false; nash-prompt --engine starship',env=env)
        self.assertIn('1',r.stdout); self.assertIn('❯',r.stdout)

    def test_assignment_scope(self):
        self.check('X=parent; X=temporary pwd >/dev/null; echo $X','parent\n')
        self.check('X=parent; X=temporary source /dev/null; echo $X','parent\n')
    def test_no_double_substitution(self):
        with tempfile.TemporaryDirectory() as d:
            marker=pathlib.Path(d,'marker')
            self.check(f'printf "%s\\n" "$(echo x >> {marker}; echo value)"','value\n')
            self.assertEqual(marker.read_text(),'x\n')
    def test_redirection_restore_on_error(self):
        with tempfile.TemporaryDirectory() as d:
            r=self.run_shell(f'X=value > {d}/out < {d}/missing; echo visible')
            self.assertEqual(r.stdout,'visible\n')
    def test_large_pipeline(self):
        self.check('seq 1 100000 | wc -l','100000\n')
    def test_multiline_quotes(self):
        self.check('echo "one\ntwo"','one\ntwo\n')
    def test_mixed_quotes(self):
        self.check('X=value; echo \'literal $X\'"$X"', 'literal $Xvalue\n')
        self.check('X=\'$HOME\'; echo "$X"','$HOME\n')
        self.check('echo "~"', '~\n')
    def test_starship_failure_timeout(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d,'starship');p.write_text('#!/bin/sh\nsleep 10\n');p.chmod(0o755)
            env=dict(os.environ,PATH=d+':'+os.environ['PATH'],NO_COLOR='1')
            import time
            start=time.monotonic();r=self.run_shell('nash-prompt --engine starship',env=env)
            self.assertLess(time.monotonic()-start,4)
            self.assertIn('❯',r.stdout)
    def test_scripts(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d,'script.nash');p.write_text('X=ok\necho "$X"\nexit 7\n')
            r=subprocess.run([NASH,'--no-config',str(p)],text=True,capture_output=True,timeout=5)
            self.assertEqual((r.returncode,r.stdout),(7,'ok\n'))
    def test_quoted_redirect_target(self):
        with tempfile.TemporaryDirectory() as d:
            self.check(f"cd {d}; echo file > '&1'; cat '&1'",'file\n')
    def test_unexported_assignment(self):
        self.check('unset NASH_LOCAL_ONLY; NASH_LOCAL_ONLY=private; printenv NASH_LOCAL_ONLY; echo $?','1\n')
    def test_native_default_ignores_starship(self):
        with tempfile.TemporaryDirectory() as d:
            marker=pathlib.Path(d,'called')
            tool=pathlib.Path(d,'starship')
            tool.write_text('#!/bin/sh\ntouch "'+str(marker)+'"\nprintf inherited-prompt\n')
            tool.chmod(0o755)
            env=dict(os.environ,PATH=d+':'+os.environ['PATH'],STARSHIP_CONFIG='/unused/fish.toml',NO_COLOR='1')
            result=self.run_shell('nash-prompt',env=env)
            self.assertIn('nash ',result.stdout)
            self.assertIn('\n❯ ',result.stdout)
            self.assertFalse(marker.exists())
    def test_config_commands(self):
        self.check('config set prompt.engine starship; config get prompt.engine; config set history.limit 42; config get history.limit','starship\n42\n')
        self.check('config set autosuggestions.enabled false; config get autosuggestions.enabled','false\n')
        self.check('config set prompt.engine wrong; config get prompt.engine','native\n')
        for command in ['config set unknown value','config set history.limit 0','config set prompt.timeout_ms huge','config set colors.enabled maybe','config hook wrong true','config bind Ctrl-F wrong']:
            with self.subTest(command=command):self.assertEqual(self.run_shell(command).returncode,2)
    def test_has_optional_tool(self):
        self.check('has tool_that_does_not_exist_983843 && echo no; has /bin/echo && echo yes','yes\n')
    def test_config_file_source(self):
        with tempfile.TemporaryDirectory() as d:
            config=pathlib.Path(d,'config.nash')
            config.write_text("config set prompt.engine native\nalias greet='echo hello'\nexport NASH_CONFIG_MARKER=ready\n")
            self.check(f'source {config}\ngreet\necho $NASH_CONFIG_MARKER','hello\nready\n')
    def test_empty_command(self):
        self.check('')
if __name__=='__main__': unittest.main()

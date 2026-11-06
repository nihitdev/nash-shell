#!/usr/bin/env python3
"""Deterministic black-box regressions and Bash comparisons for Nash's supported subset."""
import itertools, os, pathlib, shutil, subprocess, sys, tempfile, unittest
NASH=str(pathlib.Path(sys.argv.pop(1)).resolve())
BASH=shutil.which('bash')
class ReliabilityTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.cwd=pathlib.Path(self.tmp.name)
        self.env=dict(os.environ)
        for key in ['BASH_ENV','ENV','SHELLOPTS','BASHOPTS'] : self.env.pop(key,None)
    def tearDown(self):self.tmp.cleanup()
    def run_shell(self,source,stdin=None,reference=False):
        command=[BASH,'--noprofile','--norc','-O','expand_aliases','-c',source] if reference else [NASH,'--no-config','-c',source]
        return subprocess.run(command,input=stdin,text=True,capture_output=True,timeout=5,cwd=self.cwd,env=self.env)
    def check(self,source,stdout='',status=0,stdin=None):
        result=self.run_shell(source,stdin)
        self.assertEqual((result.returncode,result.stdout),(status,stdout),result.stderr)
    def test_aliases_use_complete_tokens(self):
        self.check("alias ec='printf WRONG'\nec\"ho\" ok",'ok\n')
        self.check("alias cho='printf WRONG'\n\\echo ok",'ok\n')
        self.check("alias hi='echo alias'\nX=x hi\n> out hi\ncat out",'alias\nalias\n')
        self.check("alias hi='echo alias'\necho $(hi)\necho \"$(hi)\"",'alias\nalias\n')
    def test_escaped_trailing_space_is_preserved(self):
        self.check("printf '<%s>\\n' trailing\\ ",'<trailing >\n')
    def test_alias_cycles_are_bounded(self):
        result=self.run_shell("alias a=b\nalias b=a\na")
        self.assertEqual(result.returncode,127)
        self.check("alias echo='echo prefix'\necho hello",'prefix hello\n')
    def test_assignment_order_and_scope(self):
        self.check('unset A B; A=one B=$A; echo "$A,$B"','one,one\n')
        self.check('unset A B; A=one B=$(printenv A); printf "<%s>\\n" "$B"','<>\n')
        self.check('unset A B; A=one B=$(printenv A) printenv B; echo "${A:-unset}"','one\nunset\n')
        self.check('X=old; X=new echo "$X"; echo "$X"','old\nold\n')
    def test_assignment_substitution_status(self):
        self.check('A=$(false); echo $?; A=$(false) B=$(true); echo $?; A=$(true) B=$(false); echo $?','1\n0\n1\n')
        self.check('A=$(false) && echo no; A=$(false) || echo recovered','recovered\n')
        self.check('A=$(false) true; echo $?','0\n')
        self.check('A=$(sleep 0.01 & sleep 0.05); echo $?; B=$(false & sleep 0.05); echo $?', '0\n0\n')
    def test_expanded_defaults_preserve_quotes(self):
        self.check('unset X Y; Y=fallback; echo "${X:-$Y}"; echo "${X:-${MISSING:-nested}}"','fallback\nnested\n')
        self.check('unset X; printf "<%s>\\n" ${X:-"two words"}; printf "<%s>\\n" ${X:-two words}','<two words>\n<two>\n<words>\n')
        self.check('X=keep; echo "${X:-$(echo wrong > marker)}"; test ! -e marker','keep\n')
    def test_nested_command_delimiters(self):
        self.check('test "$$" = "$(echo $$)"')
        self.check('echo "$(echo \"$(printf \'%s\' \')\')\")"',')\n')
        self.check("echo $(printf '%s' '\\q')",'\\q\n')
        self.check('A=$(printf \'x\\n\\n\'); echo "$A"','x\n')
    def test_standard_descriptor_prefixes(self):
        self.check('echo ok 1> out; cat 0< out','ok\n')
        self.check("echo '2' > out; cat out",'2\n')
        result=self.run_shell('echo error 1>&2; echo output')
        self.assertEqual((result.stdout,result.stderr),('output\n','error\n'))
    def test_redirected_read_is_not_buffered_across_files(self):
        self.check("printf 'first\\nsecond\\n' > data; read X < data; read Y < data; echo \"$X,$Y\"",'first,first\n')
        self.check('read X < /dev/null; echo yes > data; read Y < data; echo "$Y"','yes\n')
    def test_read_options_and_partial_eof(self):
        self.check('IFS= read -r X; printf "<%s>\\n" "$X"','<  a\\b  >\n',stdin='  a\\b  \n')
        self.check('read X; S=$?; printf "%s:<%s>\\n" "$S" "$X"','1:<partial>\n',stdin='partial')
        self.check('read X Y; printf "<%s><%s>\\n" "$X" "$Y"','<one two><three>\n',stdin='one\\ two three\n')
        self.check('IFS=: read X Y Z; printf "<%s><%s><%s>\\n" "$X" "$Y" "$Z"','<a><><b>\n',stdin='a::b\n')
        self.assertEqual(self.run_shell('read bad-name',stdin='data\n').returncode,2)
    def test_stdin_scripts_share_input_with_read(self):
        result=subprocess.run([NASH,'--no-config'],input='read X\nshared input\necho "$X"\n',text=True,capture_output=True,timeout=5,cwd=self.cwd,env=self.env)
        self.assertEqual((result.returncode,result.stdout),(0,'shared input\n'),result.stderr)
    def test_builtin_statuses_are_truthful(self):
        self.assertEqual(self.run_shell('type nash_missing_928813').returncode,1)
        self.assertEqual(self.run_shell('wait 99999999').returncode,127)
        self.check('cd / /tmp; echo $?','2\n')
        self.check('cd -- /tmp; pwd','/tmp\n')
        self.check('env NASH_ENV_AUDIT=passed printenv NASH_ENV_AUDIT','passed\n')
    def test_exit_validation(self):
        self.check('exit nope; echo must-not-run',status=2)
        self.check('exit -1',status=255)
        self.check('exit 257',status=1)
        self.check('exit 1 2; echo must-not-run',status=1)
    @unittest.skipUnless(BASH,'Bash reference not installed')
    def test_read_against_bash(self):
        for ifs,data,names in itertools.product(['',':',' :\t'],['a::b\n', '  a b  \n', ':a: :b:\n','a\\ b c\n','partial',''],['A','A B C']):
            source="IFS='"+ifs+"' read -r "+names+"; S=$?; printf '%s:<%s><%s><%s>\\n' \"$S\" \"$A\" \"$B\" \"$C\""
            with self.subTest(ifs=ifs,data=data,names=names):
                actual=self.run_shell(source,stdin=data);reference=self.run_shell(source,stdin=data,reference=True)
                self.assertEqual((actual.returncode,actual.stdout),(reference.returncode,reference.stdout),actual.stderr)
    @unittest.skipUnless(BASH,'Bash reference not installed')
    def test_supported_commands_against_bash(self):
        cases=[
            "alias ec='printf WRONG'\nec\"ho\" ok",
            "alias hi='echo alias'\nX=x hi", "alias hi='echo alias'\n> out hi; cat out",
            "alias hi='echo alias'\necho $(hi)",
            'unset A B; A=one B=$A; printf "<%s>\\n" "$A" "$B"',
            'A=$(false); echo $?','A=$(false) B=$(true); echo $?',
            'echo correct 1> out; cat 0< out',
            "printf 'a\\nb\\n' > data; read X < data; read Y < data; echo \"$X,$Y\"",
            'read X < /dev/null; echo yes > data; read Y < data; echo "$Y"',
            'echo hel\\\nlo', 'exit -1', 'exit nope', 'exit 257',
            'cd -- /tmp; pwd','cd / /tmp; test $? -ne 0','type missing_nash_audit_95832',
            'echo "$(printf ok # ignored )\n)"',
            "touch '\\one'; A='\\*'; printf '<%s>\\n' $A",
            'env NASH_ENV_AUDIT=passed printenv NASH_ENV_AUDIT',
        ]
        for value in ['', 'word', 'two words', '  spaced  ', 'a*b', 'a\\b']:
            for word in ['$A','"$A"','pre${A}post','${MISSING:-$A}','${MISSING:-"$A"}','"${MISSING:-$A}"','${MISSING:-${OTHER:-"a b"}}']:
                cases.append("unset MISSING OTHER; A='"+value+"'; printf '<%s>\\n' "+word)
        for ifs,value in itertools.product(['',':',' :\t',','],[':a::b:', 'a : b', 'a: :b', ':', '  ', ',a,,b,']):
            cases.append("IFS='"+ifs+"'; A='"+value+"'; printf '<%s>\\n' $A")
        self.comparisons=len(cases)
        for source in cases:
            with self.subTest(source=source):
                reference=self.run_shell(source,reference=True);actual=self.run_shell(source)
                self.assertEqual((actual.returncode,actual.stdout),(reference.returncode,reference.stdout),actual.stderr)

if __name__=='__main__': unittest.main()

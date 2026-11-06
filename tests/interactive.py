#!/usr/bin/env python3
"""PTY tests use real terminal ownership and real editor byte sequences."""
import fcntl, os, pathlib, pty, re, select, signal, struct, sys, tempfile, termios, time, unittest
NASH=str(pathlib.Path(sys.argv.pop(1)).resolve())
class TerminalTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name)
        (self.root/'nash').mkdir()
        config='config set prompt.engine '+getattr(self,'engine','native')+"\nconfig set prompt.left 'N> '\nconfig set colors.enabled false\nalias config_loaded='echo config-script-loaded'\nexport NASH_CONFIG_TEST=loaded\n"
        config+=getattr(self,'extra_config','')
        if self._testMethodName=='test_fastfetch_startup_once':
            config+='config set integrations.fastfetch.enabled true\n'
            tool=self.root/'fastfetch'
            tool.write_text('#!/bin/sh\nprintf "fastfetch-startup-fixture\\n"\n')
            tool.chmod(0o755)
        (self.root/'nash/config.nash').write_text(config)
        # Native script wins over conflicting legacy TOML.
        (self.root/'nash/config.toml').write_text('[prompt]\nleft="WRONG> "\n')
        if getattr(self,'engine','native')=='starship':
            fake=self.root/'starship'
            fake.write_text('#!/usr/bin/python3\nimport sys,json,os\nwith open(os.environ["NASH_PROMPT_LOG"],"a") as f: f.write(json.dumps(sys.argv[1:])+"\\n")\nprint("R" if "--right" in sys.argv else "α\\nN> ",end="")\n')
            fake.chmod(0o755)
        self.pid,self.fd=pty.fork()
        if self.pid==0:
            os.environ.update(PATH=str(self.root)+':'+os.environ['PATH'],NASH_PROMPT_LOG=str(self.root/'prompt-log'),XDG_CONFIG_HOME=str(self.root),XDG_DATA_HOME=str(self.root),TERM='xterm-256color')
            os.execv(NASH,[NASH])
        fcntl.ioctl(self.fd,termios.TIOCSWINSZ,struct.pack('HHHH',24,100,0,0))
        self.pending=b'';self.raw_output=b'';self.initial_output=self.until_prompt()
    def tearDown(self):
        try:os.kill(self.pid,signal.SIGKILL)
        except ProcessLookupError:pass
        os.waitpid(self.pid,0);os.close(self.fd);self.temp.cleanup()
    def send(self,data):os.write(self.fd,data)
    def until(self,needle,timeout=5):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            pos=self.pending.find(needle)
            if pos>=0:
                out=self.pending[:pos+len(needle)];self.pending=self.pending[pos+len(needle):];return out
            if select.select([self.fd],[],[],0.05)[0]:
                try:
                    chunk=os.read(self.fd,65536)
                    self.raw_output+=chunk
                    self.pending+=chunk
                    self.pending=self.pending.replace(b'\x1b[?2004h',b'\x00PROMPT\x00')
                    self.pending=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',self.pending)
                    self.pending=self.pending.replace(b'\r\r\n',b'\r\n')
                except OSError:break
        self.fail(f'missing {needle!r}: {self.pending!r}')
    def until_prompt(self, timeout=5):
        return self.until(b'\x00PROMPT\x00',timeout)+self.until(b'N> ',timeout)
    def command(self,source,output):
        self.send(source+b'\r');self.until(b'\r\n'+output+b'\r\n');self.until_prompt()
    def test_fastfetch_startup_once(self):
        self.assertEqual(self.initial_output.count(b'fastfetch-startup-fixture'),1)
        self.send(b'echo after-banner\r')
        output=self.until(b'\r\nafter-banner\r\n')+self.until_prompt()
        self.assertNotIn(b'fastfetch-startup-fixture',output)
    def test_exit_too_many_arguments_keeps_interactive_shell(self):
        self.send(b'exit 1 2\r');self.until(b'too many arguments');self.until_prompt()
        self.command(b'echo $?',b'1')
    def test_builtin_read_interrupt(self):
        self.send(b'read -p "$(printf audit-%s ready)" X\r');self.until(b'audit-ready');self.send(b'\x03');self.until_prompt(3)
        self.command(b'echo $?',b'130')
        self.command(b'echo recovered',b'recovered')
    def test_completion_at_input_start(self):
        self.command(b'echo stable\x01\t\x05',b'stable')
    def test_edit_and_unicode(self):
        self.command(b'echo abX\x1b[D\x7fc',b'acX')
        self.command('echo café\x7f'.encode(),b'caf')
    def test_history_suggestion_search(self):
        self.command(b'echo history-marker',b'history-marker')
        self.command(b'echo hist\x1b[C',b'history-marker')
        self.command(b'\x12',b'history-marker')
        self.assertTrue((self.root/'nash/history').exists())
    def test_ghost_suggestions_without_history(self):
        (self.root/'ghost-new-file').write_text('fresh-ghost-file\n')
        base=str(self.root).encode()+b'/'
        self.command(b'unset NO_COLOR; config set colors.enabled true; echo ghost-colors',b'ghost-colors')
        self.send(b'cat '+base+b'ghost-new-f')
        # Wait for the actual idle-generated suffix, without a Tab key or matching history.
        self.until(b'ghost-new-file')
        self.assertIn(b'\x1b[90mile\x1b[0m',self.raw_output)
        self.command(b'\x1b[C', b'fresh-ghost-file')
        self.command(b'echo ghost-typed-only', b'ghost-typed-only')
        self.command(b'echo ghost-typed', b'ghost-typed')
    def test_ghost_suggestion_dismissal(self):
        (self.root/'ghost-cancel-file').write_text('must-not-run\n')
        self.send(b'cat '+str(self.root).encode()+b'/ghost-cancel-f')
        self.until(b'ghost-cancel-file')
        self.send(b'\x03');self.until_prompt()
        self.command(b'echo ghost-dismissed',b'ghost-dismissed')
    def test_interrupt_and_foreground(self):
        self.send(b'sleep 10\r');time.sleep(.2);self.send(b'\x03');self.until_prompt(3)
        self.command(b'echo $?',b'130')
        self.send(b'cat\r');time.sleep(.1);self.send(b'foreground-data\r');self.until(b'foreground-data\r\n');self.until(b'foreground-data\r\n');self.send(b'\x04');self.until_prompt()
    def test_stop_resume(self):
        self.send(b'sleep 10\r');time.sleep(.2);self.send(b'\x1a');self.until(b'stopped');self.until_prompt()
        self.send(b'fg\r');time.sleep(.15);self.send(b'\x03');self.until_prompt(3)
    def test_pipeline_stop_resume(self):
        self.send(b'sleep 10 | cat\r');time.sleep(.2);self.send(b'\x1a')
        self.until(b'stopped');self.until_prompt()
        self.send(b'fg\r');time.sleep(.2)
        if select.select([self.fd],[],[],.1)[0]:
            output=os.read(self.fd,65536)
            self.assertNotIn(b'\x1b[?2004h',output,'foreground pipeline returned before interrupt')
        self.send(b'\x03');self.until_prompt(3)
        self.command(b'echo $?',b'130')
    def test_multiline(self):
        self.send(b'echo "one\r');self.until(b'... ')
        self.send(b'two"\r');self.until(b'one\r\ntwo\r\n');self.until_prompt()
        self.send(b'echo continued |\r');self.until(b'... ')
        self.send(b'cat\r');self.until(b'\r\ncontinued\r\n');self.until_prompt()
    def test_paste(self):
        self.command(b'\x1b[200~echo pasted\x1b[201~',b'pasted')
    def test_ctrl_c_input(self):
        self.send(b'echo abandoned\x03');self.until(b'^C');self.until_prompt()
        self.command(b'echo $?',b'130')
    def test_completion(self):
        f=self.root/'unique-completion';f.write_text('completed\n')
        self.command(('cat '+str(self.root)+'/unique-comp').encode()+b'\t',b'completed')
    def test_completion_quotes_fuzzy_and_menu(self):
        (self.root/'two words').write_text('quoted-file\n')
        (self.root/'fuzzy-target').write_text('fuzzy-file\n')
        (self.root/'cycle-alpha').write_text('alpha-choice\n')
        (self.root/'cycle-beta').write_text('beta-choice\n')
        (self.root/'unique-middle').write_text('middle\n')
        base=str(self.root).encode()+b'/'
        self.command(b'cat "'+base+b'two w\t', b'quoted-file')
        self.command(b'cat '+base+b'fzt\t', b'fuzzy-file')
        self.command(b'cat '+base+b'cycle-\t\t', b'alpha-choice')
        self.command(b'cat '+base+b'cycle-\t\x1b[Z', b'beta-choice')
        self.command(b'echo '+base+b'unique-miZZ tail'+b'\x1b[D'*7+b'\t', base+b'unique-middle tail')
    def test_completion_git_branch(self):
        import shutil, subprocess
        if not shutil.which('git'): self.skipTest('git unavailable')
        repo=self.root/'repository';repo.mkdir()
        subprocess.run(['git','init','-q',str(repo)],check=True)
        subprocess.run(['git','-C',str(repo),'symbolic-ref','HEAD','refs/heads/main'],check=True)
        subprocess.run(['git','-C',str(repo),'-c','user.name=Test','-c','user.email=test@example.com','-c','commit.gpgsign=false','commit','--allow-empty','-qm','fixture'],check=True)
        subprocess.run(['git','-C',str(repo),'branch','nebula-feature'],check=True)
        self.send(b'cd '+str(repo).encode()+b'\r');self.until_prompt()
        self.send(b'git switch nebula-f\t\r');self.until_prompt()
        self.command(b'git branch --show-current',b'nebula-feature')
    def test_prompt_config_and_hooks(self):
        self.command(b'config_loaded',b'config-script-loaded')
        self.command(b'echo $NASH_CONFIG_TEST',b'loaded')
        self.command(b'config get colors.enabled',b'false')
        self.command(b'pwd',os.getcwd().encode())
class StarshipTerminalTests(TerminalTests):
    engine='starship'
    # Failing hooks must not overwrite the previous command's status.
    extra_config="config hook precmd 'false'\n"
    def test_starship_context_and_refresh(self):
        import json
        log=self.root/'prompt-log'
        before=len(log.read_text().splitlines())
        self.send(b'echo typing')
        time.sleep(.15)
        self.assertEqual(len(log.read_text().splitlines()),before)
        self.send(b'\x03');self.until_prompt()
        self.send(b'false\r');self.until_prompt()
        records=[json.loads(line) for line in log.read_text().splitlines()]
        args=records[-2]
        self.assertEqual(args[args.index('--status')+1],'1')
        self.assertEqual(args[args.index('--terminal-width')+1],'100')
        self.assertIn('--right',records[-1])
        self.send(b'sleep 0.03\r');self.until_prompt()
        args=json.loads(log.read_text().splitlines()[-2])
        self.assertGreaterEqual(int(args[args.index('--cmd-duration')+1]),20)
if __name__=='__main__':unittest.main()

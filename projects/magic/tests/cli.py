"""Finite ordinary-process caller checks. Invoked beneath the workspace native wrapper."""
import pathlib
import subprocess
import sys

binary = str(pathlib.Path(sys.argv[1]).resolve())
fixtures = pathlib.Path(sys.argv[2])
passed = 0


def check(name, arguments, source, expected_status, fragments):
    global passed
    result = subprocess.run([binary, *arguments], input=source, text=True, capture_output=True, timeout=10)
    print(f'CASE {name}: exit {result.returncode}')
    print(result.stdout, end='')
    print(result.stderr, end='')
    assert result.returncode == expected_status, (name, result.returncode)
    output = result.stdout + result.stderr
    for fragment in fragments:
        assert fragment in output, (name, fragment)
    passed += 1


s1 = (fixtures / 'S1.verse').read_text()
check('stdin composes new quantity', [], s1.replace('12 hp', '31 hp'), 0,
      ['heal 31 hp instant', 'game.heal', 'invented placeholders'])
check('stdin composes delivery', [], s1.replace('via contact', 'via ray'), 0,
      ['spoken release via ray', 'willing touched actor at release', 'heal 12 hp instant'])
check('stdin exact byte ceiling', [], s1 + ' ' * (8192 - len(s1)), 0, ['Exact Common:', 'heal 12 hp instant'])
check('stdin above byte ceiling', [], s1 + ' ' * (8193 - len(s1)), 1, ['SourceLimit at byte 8192'])
check('unknown authored phrase', [], s1.replace('sela', 'mystery'), 1, ['UnknownPhrase at byte', '[mystery]'])
check('ambiguous duration', [], s1 + ' for 2 s', 1, ['AmbiguousScope at byte', '[for]'])
check('help labels invented language', ['--help'], '', 0, ['invented, not Elvish', 'executes no game effects'])
check('missing file is I/O failure', [str(fixtures / 'absent.verse')], '', 2, ['Cannot open verse file.'])
check('extra arguments refuse', ['a', 'b'], '', 2, ['Usage: magic_inspect'])
print(f'{passed} CLI process scenarios passed.')

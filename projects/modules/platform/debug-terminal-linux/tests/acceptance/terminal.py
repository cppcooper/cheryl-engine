"""Exercise previously built terminal probes; never configure or build the project."""

import argparse
import json
import os
from pathlib import Path
import re
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET


LAUNCHER = r'''
import json
import os
from pathlib import Path
import socket
import sys
import time

command = sys.argv[sys.argv.index("-e") + 1:]
streams = [os.readlink(f"/proc/self/fd/{index}") for index in range(3)]
extra = []
for value in os.listdir("/proc/self/fd"):
    if int(value) > 2:
        try:
            extra.append(os.readlink(f"/proc/self/fd/{value}"))
        except FileNotFoundError:
            pass
record = Path(os.environ["CHERYL_TERMINAL_TEST_RECORD"])
temporary = record.with_suffix(".tmp")
temporary.write_text(json.dumps({
    "pid": os.getpid(), "command": command, "streams": streams, "extra": extra
}))
temporary.replace(record)
output = os.open(os.environ["CHERYL_TERMINAL_TEST_OUTPUT"], os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
os.dup2(output, 1)
os.dup2(output, 2)
os.close(output)
failure = os.environ.get("CHERYL_TERMINAL_TEST_FAILURE")
if failure == "exit":
    sys.exit(3)
if failure in ("acknowledgment", "start"):
    address = command[command.index("--socket") + 1]
    channel = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    channel.connect(address)
    if failure == "start":
        channel.shutdown(socket.SHUT_RD)
    channel.send(b"?" if failure == "acknowledgment" else b"R")
    time.sleep(10)
    sys.exit(4)
os.execv(command[0], command)
'''


def wait_for(predicate, description):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise RuntimeError(f"Timed out waiting for {description}")


def alive(pid):
    try:
        state = Path(f"/proc/{pid}/stat").read_text().rsplit(") ", 1)[1][0]
        return state != "Z"
    except FileNotFoundError:
        return False


class Case:
    def __init__(self, root, name, launchers):
        self.root = root / name
        self.root.mkdir()
        self.launchers = launchers
        self.record_path = self.root / "launcher.json"
        self.output_path = self.root / "viewer.log"
        self.process = None
        self.record = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        if self.process and self.process.poll() is None:
            self.process.kill()
            self.process.communicate(timeout=8)
        self.load_record()
        if self.record and alive(self.record["pid"]):
            os.kill(self.record["pid"], signal.SIGTERM)
            wait_for(lambda: not alive(self.record["pid"]), "viewer cleanup")
        if self.record:
            directory = self.capture_directory()
            if directory.parent == Path("/tmp") and directory.name.startswith("cheryl-terminal-"):
                shutil.rmtree(directory, ignore_errors=True)

    def load_record(self):
        if self.record is None and self.record_path.exists():
            try:
                self.record = json.loads(self.record_path.read_text())
            except json.JSONDecodeError:
                pass
        return self.record

    def capture_directory(self):
        command = self.record["command"]
        return Path(command[command.index("--output") + 1]).parent

    def viewer_output(self):
        return self.output_path.read_text(errors="replace") if self.output_path.exists() else ""

    def start(self, executable, arguments, *, headless=False, environment=None):
        env = os.environ.copy()
        for key in tuple(env):
            if key.startswith("GTEST_"):
                del env[key]
        env.pop("WAYLAND_DISPLAY", None)
        env.pop("DISPLAY", None)
        if not headless:
            env["DISPLAY"] = ":cheryl-terminal-probe"
        env["PATH"] = str(self.launchers) + os.pathsep + env.get("PATH", "")
        env["CHERYL_TERMINAL_TEST_RECORD"] = str(self.record_path)
        env["CHERYL_TERMINAL_TEST_OUTPUT"] = str(self.output_path)
        env.pop("CHERYL_TERMINAL_TEST_FAILURE", None)
        if environment:
            env.update(environment)
        self.process = subprocess.Popen(
            [str(executable), *arguments], cwd=self.root, env=env, text=True,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )

    def finish(self, expected=0, input=None):
        output, error = self.process.communicate(input=input, timeout=8)
        assert self.process.returncode == expected, (self.process.returncode, output, error)
        return output, error

    def read_error_until(self, marker):
        received = bytearray()

        def ready():
            while select.select([self.process.stderr], [], [], 0)[0]:
                chunk = os.read(self.process.stderr.fileno(), 8192)
                if not chunk:
                    raise RuntimeError(f"Application ended before stderr marker {marker!r}")
                received.extend(chunk)
            return marker.encode() in received

        wait_for(ready, f"stderr marker {marker!r}")
        return received.decode(errors="replace")

    def check_launcher(self, active):
        self.load_record()
        assert bool(self.record) == active, self.record
        if active:
            assert self.record["streams"] == ["/dev/null"] * 3, self.record
            assert not self.record["extra"], self.record

    def closed(self):
        wait_for(lambda: not alive(self.record["pid"]), "normal viewer closure")
        wait_for(lambda: not self.capture_directory().exists(), "capture cleanup")


def check_output(case, executable, arguments, forwarded, active, *, headless=False):
    case.start(executable, ["normal", *arguments], headless=headless)
    output, error = case.finish()
    case.check_launcher(active)
    if active:
        case.closed()
    viewed = case.viewer_output()
    observed = viewed if active else output + error
    for marker in (
        "c-output", "c-error", "cpp-output", "cpp-error", "native-output", "native-error",
        "worker-record", "engine-record", "exit-record", "engine-exit-record", "buffered-tail",
    ):
        assert marker in observed, (marker, observed)
        if active:
            assert marker not in output + error, (marker, output, error)
    actual = [line for line in observed.splitlines() if line.startswith("argument[")]
    expected = [f"argument[{index}]={value}" for index, value in enumerate(["normal", *forwarded], 1)]
    assert actual == expected, (actual, expected)
    assert "without normal shutdown" not in viewed, viewed
    log = (case.root / "logs" / "terminal-probe.log").read_text()
    assert "engine-record" in log and "engine-exit-record" in log, log
    return observed


def check_lifetime(root, launchers, executable):
    with Case(root, "burst", launchers) as case:
        case.start(executable, ["burst", "--debug-terminal"])
        output, error = case.finish()
        case.check_launcher(True)
        case.closed()
        viewed = case.viewer_output()
        assert re.findall(r"burst\[(\d+)\] ", viewed) == [str(index) for index in range(512)]
        assert "buffered-tail" in viewed and "engine-exit-record" in viewed, viewed[-8192:]
        assert "burst[" not in output + error
    with Case(root, "manual-close", launchers) as case:
        case.start(executable, ["hold", "--debug-terminal"])
        wait_for(lambda: "native-error" in case.viewer_output(), "live process output")
        case.check_launcher(True)
        os.kill(case.record["pid"], signal.SIGTERM)
        wait_for(lambda: not alive(case.record["pid"]), "manual viewer closure")
        assert case.process.poll() is None, "Closing the viewer terminated the application"
        notice = case.read_error_until("until shutdown.\n")
        output, error = case.finish(input="\n")
        assert "viewer closed" in notice, notice
        assert "after-close" not in output + error + case.viewer_output()
        log = (case.root / "logs" / "terminal-probe.log").read_text()
        assert "engine-after-close" in log and "engine-exit-record" in log, log
        assert not case.capture_directory().exists()
    with Case(root, "abnormal", launchers) as case:
        case.start(executable, ["crash", "--debug-terminal"])
        output, error = case.finish(expected=86)
        case.check_launcher(True)
        wait_for(lambda: "without normal shutdown" in case.viewer_output(), "retained abnormal output")
        assert alive(case.record["pid"]), "The abnormal viewer did not remain open"
        assert "abnormal-record" in case.viewer_output()
        assert "abnormal-record" not in output + error
        assert "exit-record" not in case.viewer_output()
    with Case(root, "killed", launchers) as case:
        case.start(executable, ["hold", "--debug-terminal"])
        wait_for(lambda: "native-error" in case.viewer_output(), "output before process loss")
        case.process.kill()
        output, error = case.finish(expected=-signal.SIGKILL)
        case.check_launcher(True)
        wait_for(lambda: "without normal shutdown" in case.viewer_output(), "retained signal termination")
        assert alive(case.record["pid"]), "The signal-terminated viewer did not remain open"
        assert "exit-record" not in case.viewer_output()


def check_reports(root, launchers, executable, linked):
    flag = ["--debug-terminal"] if linked else []
    for style in ("fast", "threadsafe"):
        with Case(root, f"reports-{style}", launchers) as case:
            xml = case.root / "results.xml"
            case.start(executable, [*flag, "--gtest_filter=terminal_report.*-terminal_report.failure",
                                   f"--gtest_death_test_style={style}", f"--gtest_output=xml:{xml}"])
            output, error = case.finish()
            case.check_launcher(linked)
            if linked:
                case.closed()
            viewed = case.viewer_output()
            assert "[  PASSED  ]" in output, output
            assert "framework-diagnostic" in error, error
            assert "[  PASSED  ]" not in viewed and "framework-diagnostic" not in viewed, viewed
            for marker in ("application-output", "application-error"):
                assert marker in (viewed if linked else output + error), (marker, viewed, output, error)
                if linked:
                    assert marker not in output + error, (output, error)
            for marker in ("captured-output", "captured-error", "death-record"):
                assert marker not in output + error + viewed, (marker, output, error, viewed)
            tree = ET.parse(xml).getroot()
            assert tree.attrib["failures"] == "0" and tree.attrib["tests"] == "4", tree.attrib
    with Case(root, "assertion", launchers) as case:
        case.start(executable, [*flag, "--gtest_filter=terminal_report.failure"])
        output, error = case.finish(expected=1)
        case.check_launcher(linked)
        if linked:
            case.closed()
        assert "framework-assertion" in output and "[  FAILED  ]" in output, (output, error)
        assert "framework-assertion" not in case.viewer_output()
    for name, argument, marker in (
        ("help", "--help", "Test Selection:"),
        ("discovery", "--gtest_list_tests", "terminal_report."),
    ):
        with Case(root, f"framework-{name}", launchers) as case:
            case.start(executable, [*flag, argument])
            output, error = case.finish()
            case.check_launcher(False)
            assert marker in output, (output, error)
    with Case(root, "environment-warning", launchers) as case:
        case.start(executable, [*flag, "--gtest_filter=terminal_report.pass"], environment={"GTEST_REPEAT": "invalid"})
        output, error = case.finish()
        case.check_launcher(linked)
        if linked:
            case.closed()
        assert "GTEST_REPEAT" in output + error, (output, error)
        assert "GTEST_REPEAT" not in case.viewer_output()


def main():
    if not __debug__:
        raise RuntimeError("Run without Python optimization so acceptance assertions remain enabled")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--configuration", required=True, help="The actual CMake configuration, such as Debug or Release")
    parser.add_argument("--inclusion", choices=("AUTO", "ON", "OFF"), required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    executable = build / "tests-acceptance-engine-terminal"
    reports = build / "tests-acceptance-engine-terminal-reports"
    debug = args.configuration == "Debug"
    linked = args.inclusion == "ON" or (args.inclusion == "AUTO" and debug)
    automatic = linked and debug
    with tempfile.TemporaryDirectory(prefix="cheryl-terminal-acceptance-") as temporary:
        root = Path(temporary)
        launchers = root / "bin"
        launchers.mkdir()
        for name in ("konsole", "xterm"):
            path = launchers / name
            path.write_text(f"#!{sys.executable}\n" + LAUNCHER)
            path.chmod(0o700)
        with Case(root, "baseline", launchers) as case:
            observed = check_output(case, executable, ["--no-debug-terminal"],
                                    [] if linked else ["--no-debug-terminal"], False)
            assert f"terminal-linked={int(linked)} debug={int(debug)}" in observed, observed
        policies = (
            ("default", [], [], automatic, False),
            ("enable", ["--debug-terminal"], [], linked, False),
            ("disable-last", ["--debug-terminal", "--no-debug-terminal"], [], False, False),
            ("enable-last", ["--no-debug-terminal", "--debug-terminal"], [], linked, False),
            ("help", ["--debug-terminal", "--help"], ["--help"], False, False),
            ("listing", ["--debug-terminal", "--gtest_list_tests"], ["--gtest_list_tests"], False, False),
            ("listing-false", ["--debug-terminal", "--gtest_list_tests=FALSE"], ["--gtest_list_tests=FALSE"], linked, False),
            ("listing-last", ["--debug-terminal", "--gtest_list_tests", "--gtest_list_tests=0"],
             ["--gtest_list_tests", "--gtest_list_tests=0"], linked, False),
            ("separator", ["--no-debug-terminal", "--", "--debug-terminal"], ["--", "--debug-terminal"], False, False),
            ("headless", ["--debug-terminal"], [], False, True),
        )
        for name, arguments, forwarded, active, headless in policies:
            with Case(root, name, launchers) as case:
                check_output(case, executable, arguments, forwarded if linked else arguments, active, headless=headless)
        if linked:
            for failure in ("exit", "acknowledgment", "start"):
                with Case(root, f"fallback-{failure}", launchers) as case:
                    case.start(executable, ["hold", "--debug-terminal"],
                               environment={"CHERYL_TERMINAL_TEST_FAILURE": failure})
                    prefix = case.read_error_until("worker-record\n")
                    assert "terminal unavailable" in prefix and "c-error" in prefix, prefix
                    case.check_launcher(True)
                    wait_for(lambda: not Path(f"/proc/{case.record['pid']}").exists(), "failed launcher reaping")
                    assert case.process.poll() is None, "Startup fallback terminated the application"
                    assert not case.capture_directory().exists()
                    output, error = case.finish(input="\n")
                    assert "c-output" in output and "after-close" in error, (output, error)
            check_lifetime(root, launchers, executable)
        check_reports(root, launchers, reports, linked)
    print(f"PASS terminal policy: {args.configuration}, {args.inclusion}")
    if linked:
        print("PASS controlled native viewer lifetime, output capture, fallback and report separation")
    else:
        print("Terminal omitted: viewer lifetime and report separation require an included configuration")


if __name__ == "__main__":
    main()

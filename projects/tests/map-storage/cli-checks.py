"""Private process acceptance; uses only Python's already available standard library."""

import argparse
import copy
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile


# Independent literal expectations, not data recovered from a CLI-created file.
EXPECTED = {
    "format": "cheryl.map-fixture", "version": 1,
    "id": "fixture:two-layers", "width": 3, "height": 2,
    "layers": [
        {"id": "ground", "cells": ["fixture:grass", "fixture:stone", "fixture:grass",
                                    "fixture:grass", "fixture:grass", "fixture:stone"]},
        {"id": "overlay", "cells": [None, "fixture:wall", None, None, None, "fixture:wall"]},
    ],
    "spawns": [{"name": "entry", "layer": "ground", "x": 1, "y": 1}],
}


def check(condition, detail):
    if not condition:
        raise AssertionError(detail)


def document(value):
    return json.dumps(value, indent=2).encode("utf-8") + b"\t\n"


class Harness:
    def __init__(self, cli, case, work_root):
        self.cli = str(Path(cli).absolute())
        work_root = Path(work_root).absolute()
        work_root.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(prefix=case + "-", dir=work_root))
        self.files = self.root / "files"
        self.calls = self.root / "calls"
        self.files.mkdir()
        self.calls.mkdir()
        self.count = 0
        print("CLI evidence: " + str(self.root), flush=True)

    def run(self, arguments, expected, *, stdout_file=None, stdout_label="read-only fixture descriptor"):
        self.count += 1
        prefix = self.calls / f"{self.count:03d}"
        command = [self.cli, *map(str, arguments)]
        with subprocess.Popen(command, cwd=self.files,
                              stdout=stdout_file if stdout_file is not None else subprocess.PIPE,
                              stderr=subprocess.PIPE) as process:
            try:
                out, err = process.communicate(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
                out, err = process.communicate()
                prefix.with_suffix(".stdout").write_bytes(out or b"")
                prefix.with_suffix(".stderr").write_bytes(err)
                raise AssertionError(f"CLI watchdog expired: {command}; evidence {prefix}")
            prefix.with_suffix(".stdout").write_bytes(out or b"")
            prefix.with_suffix(".stderr").write_bytes(err)
            prefix.with_suffix(".json").write_text(json.dumps({
                "command": command, "cwd": str(self.files), "pid": process.pid,
                "actual_exit": process.returncode, "expected_exit": expected,
                "stdout_sink": stdout_label if stdout_file is not None else "captured",
            }, indent=2) + "\n")
            check(process.returncode == expected,
                  f"expected exit {expected}, got {process.returncode}: {command}; evidence {prefix}\n"
                  + err.decode("utf-8", errors="replace"))
        return out or b"", err

    def success(self, command, path):
        native = Path(path)
        if not native.is_absolute():
            native = self.files / native
        before = native.read_bytes() if command == "verify" else None
        out, err = self.run([command, path], 0)
        check(not err, f"success wrote stderr: {err!r}")
        for part in (f"{command}:", "map=fixture:two-layers", "dimensions=3x2",
                     "layers=[ground,overlay]", "spawn=entry@ground(1,1)"):
            check(part.encode() in out, f"missing summary {part}: {out!r}")
        if command == "verify":
            check(native.read_bytes() == before, f"successful verify rewrote {path}")
        return out

    def rejection(self, command, path, category, operation, location, *, offset=False, io=False):
        out, err = self.run([command, path], 2)
        check(not out, f"rejection wrote stdout: {out!r}")
        origin = Path(path)
        if not origin.is_absolute():
            origin = self.files / origin  # Preserve components, including link/.. .
        for part in (f"category={category}", f'operation="{operation}"', f'origin="{origin}"',
                     f'location="{location}', "detail:"):
            check(part.encode() in err, f"missing diagnostic {part}: {err!r}")
        if offset:
            check(b"byte_offset=" in err, f"missing byte offset: {err!r}")
        if io:
            check(b"io_error=generic:" in err, f"missing native error: {err!r}")
        return err

    def verify_bytes(self, path, expected_exit):
        before = path.read_bytes()
        out, err = self.run(["verify", path], expected_exit)
        check(path.read_bytes() == before, f"verify rewrote {path}")
        if expected_exit == 3:
            check(not out and b"mismatch:" in err and b"complete snapshot" in err,
                  f"missing complete-snapshot mismatch: {out!r} {err!r}")
        return out, err


def round_trip(h):
    path = h.files / "fixture with spaces;literal.map.json"
    h.success("round-trip", path)
    written = path.read_bytes()
    check(json.loads(written) == EXPECTED, "round-trip differs from independent literal")
    h.success("verify", path)  # A new exec, not a function pretending to be main.
    check(path.read_bytes() == written, "fresh verification changed bytes")
    h.success("round-trip", path)  # Deliberate replacement must remain deterministic.
    check(path.read_bytes() == written, "repeated round-trip is nondeterministic")


def comparison(h):
    variants = []

    def variant(name, change):
        value = copy.deepcopy(EXPECTED)
        change(value)
        variants.append((name, value))

    variant("map-id", lambda v: v.update(id="fixture:different"))
    variant("dimensions", lambda v: v.update(width=2, height=3))
    variant("layer-order", lambda v: v["layers"].reverse())
    variant("layer-count", lambda v: v["layers"].append({"id": "third", "cells": [None] * 6}))

    def rename_layer(value):
        value["layers"][0]["id"] = "renamed"
        value["spawns"][0]["layer"] = "renamed"

    variant("layer-id", rename_layer)
    for layer in range(2):
        for cell in range(6):
            value = copy.deepcopy(EXPECTED)
            value["layers"][layer]["cells"][cell] = (
                "unbound:changed" if value["layers"][layer]["cells"][cell] is None else None)
            variants.append((f"cell-{layer}-{cell}", value))
    for field, replacement in (("name", "other"), ("layer", "overlay"), ("x", 0), ("y", 0)):
        variant("spawn-" + field, lambda v, f=field, r=replacement: v["spawns"][0].update({f: r}))
    variant("spawn-count", lambda v: v["spawns"].append({"name": "other", "layer": "ground", "x": 2, "y": 0}))
    variant("no-spawns", lambda v: v.update(spawns=[]))
    for name, value in variants:
        path = h.files / (name + ".map.json")
        path.write_bytes(document(value))
        h.verify_bytes(path, 3)


def malformed(h):
    path = h.files / "malformed.map.json"
    path.write_bytes(b"{]")
    before = path.read_bytes()
    h.rejection("verify", path, "Malformed", "parse", "$", offset=True)
    check(path.read_bytes() == before, "malformed verification changed bytes")


def truncated(h):
    path = h.files / "truncated.map.json"
    path.write_bytes(document(EXPECTED)[:-14])
    before = path.read_bytes()
    h.rejection("verify", path, "Malformed", "parse", "$", offset=True)
    check(path.read_bytes() == before, "truncated verification changed bytes")


def format_version(h):
    for field, value, category in (("version", 2, "UnsupportedVersion"),
                                   ("format", "other.map", "UnsupportedFormat")):
        candidate = copy.deepcopy(EXPECTED)
        candidate[field] = value
        path = h.files / (field + ".map.json")
        path.write_bytes(document(candidate))
        before = path.read_bytes()
        h.rejection("verify", path, category, "parse", "$." + field)
        check(path.read_bytes() == before, f"{field} verification changed bytes")


def missing(h):
    absent = h.files / "absent.map.json"
    h.rejection("verify", absent, "Open", "load", "$path", io=True)
    check(not absent.exists(), "verify created missing file")
    path = h.files / "missing-parent" / "map.json"
    for command, operation in (("verify", "load"), ("round-trip", "save")):
        h.rejection(command, path, "Open", operation, "$path", io=True)
        check(not path.parent.exists(), f"{command} created missing parent")


def profiles(h):
    backing = h.files / "backing.map.json"
    backing.write_bytes(document(EXPECTED))
    before = backing.read_bytes()
    link = h.files / "link"
    link.symlink_to(backing)
    dangling = h.files / "dangling"
    dangling.symlink_to(h.files / "absent")
    directory = h.files / "directory"
    directory.mkdir()
    fifo = h.files / "fifo"
    os.mkfifo(fifo, 0o600)
    for path in (link, dangling, directory, fifo):
        state = path.lstat()
        for command, operation in (("verify", "load"), ("round-trip", "save")):
            h.rejection(command, path, "InvalidValue", operation, "$path")
            after = path.lstat()
            check((after.st_ino, after.st_mode) == (state.st_ino, state.st_mode), f"{command} changed {path}")
            check(backing.read_bytes() == before, f"{command} changed symlink target")
    check(os.readlink(link) == str(backing), "changed final link")
    check(not (h.files / "absent").exists(), "created dangling link target")
    check(not list(directory.iterdir()), "changed rejected directory")


def arguments(h):
    path = h.files / "existing.map.json"
    path.write_bytes(document(EXPECTED))
    before = path.read_bytes()
    cases = [[], ["verify"], ["round-trip"], ["unknown", str(path)], ["VERIFY", str(path)],
             ["verify", ""], ["round-trip", ""], ["verify", str(path), "extra"],
             ["round-trip", str(path), "extra"], ["--help", "extra"]]
    for args in cases:
        out, err = h.run(args, 64)
        check(not out and b"Usage:" in err, f"usage routing: {out!r} {err!r}")
        check(path.read_bytes() == before, "bad arguments changed existing file")
        check(set(h.files.iterdir()) == {path}, "bad arguments created implicit files")


def help_case(h):
    out, err = h.run(["--help"], 0)
    check(not err and b"Usage:" in out, "help routing")
    for phrase in (b"may replace an existing regular file", b"verify only reads", b"explicit nonempty path"):
        check(phrase in out, f"missing usage contract {phrase!r}")
    check(not list(h.files.iterdir()), "help created a file")


def native_path(h):
    base = h.files / "base"
    other = h.files / "other"
    base.mkdir()
    (other / "sub").mkdir(parents=True)
    (base / "link").symlink_to(other / "sub", target_is_directory=True)
    neighbor = base / "map.json"
    native = other / "map.json"
    different = copy.deepcopy(EXPECTED)
    different["id"] = "fixture:neighbor"
    neighbor.write_bytes(document(different))
    sentinel = neighbor.read_bytes()
    for path in ("base/link/../map.json", str(base / "link" / ".." / "map.json")):
        native.write_bytes(document(EXPECTED))
        before = native.read_bytes()
        h.success("verify", path)
        check(native.read_bytes() == before and neighbor.read_bytes() == sentinel, "native verify changed candidates")
        h.success("round-trip", path)
        h.success("verify", path)
        check(json.loads(native.read_bytes()) == EXPECTED, "wrong native replacement")
        check(neighbor.read_bytes() == sentinel, "replaced lexical neighbor")
        native.unlink()  # Only this owned fixture file, to test absent native target.
        h.success("round-trip", path)
        h.success("verify", path)
        check(json.loads(native.read_bytes()) == EXPECTED, "wrong native creation")
        check(neighbor.read_bytes() == sentinel, "absent-target save replaced neighbor")


def failed_write_retry(h):
    owned = h.files / "owned"
    owned.mkdir()
    path = owned / "predecessor.map.json"
    predecessor = copy.deepcopy(EXPECTED)
    predecessor["id"] = "fixture:predecessor"
    predecessor["layers"][0]["cells"][0] = "fixture:stone"
    path.write_bytes(document(predecessor))
    before = path.read_bytes()
    h.verify_bytes(path, 3)  # Valid full load, intentionally different from CLI fixture.
    previous_mode = stat.S_IMODE(owned.stat().st_mode)
    try:
        owned.chmod(0o500)
        diagnostic = h.rejection("round-trip", path, "Open", "save", "$create", io=True)
        check(b"save completed" not in diagnostic, "failed save claimed completion")
        check(path.read_bytes() == before, "failed CLI write changed predecessor bytes")
        h.verify_bytes(path, 3)  # Successful reload; mismatch is expected, not rejection.
        check(json.loads(path.read_bytes()) == predecessor, "failed write changed decoded predecessor")
        check(list(owned.iterdir()) == [path], "failed create left staging entries")
    finally:
        owned.chmod(previous_mode)  # Restore only this disposable directory, also on failed assertions.
    h.success("round-trip", path)  # Explicit retry after restoring the fixture permission.
    h.success("verify", path)
    check(json.loads(path.read_bytes()) == EXPECTED, "retry did not publish fixture")


def output_failure(h):
    path = h.files / "committed.map.json"
    predecessor = copy.deepcopy(EXPECTED)
    predecessor["id"] = "fixture:predecessor"
    path.write_bytes(document(predecessor))
    sink = h.files / "readonly-stdout"
    sink.write_bytes(b"stdout descriptor is deliberately read-only\n")
    before = sink.read_bytes()
    with sink.open("rb") as stdout:
        out, err = h.run(["round-trip", path], 1, stdout_file=stdout)
    check(not out and b"unexpected failure during output" in err, f"missing output failure: {err!r}")
    check(b"save completed; subsequent failure does not undo that publication" in err, "missing publication fact")
    check(sink.read_bytes() == before, "changed read-only stdout sentinel")
    check(json.loads(path.read_bytes()) == EXPECTED, "post-save output failure did not leave new fixture")
    h.success("verify", path)
    path.write_bytes(document(predecessor))
    reader, writer = os.pipe()
    os.close(reader)
    with os.fdopen(writer, "wb") as stdout:
        out, err = h.run(["round-trip", path], 1, stdout_file=stdout, stdout_label="pipe without reader")
    check(not out and b"unexpected failure during output" in err, f"missing broken-pipe failure: {err!r}")
    check(b"save completed" in err, "broken pipe lost the publication fact")
    check(json.loads(path.read_bytes()) == EXPECTED, "broken pipe lost committed fixture")
    h.success("verify", path)


CASES = {"round_trip": round_trip, "comparison": comparison, "malformed": malformed,
         "truncated": truncated, "format_version": format_version, "missing": missing,
         "profiles": profiles, "arguments": arguments, "help": help_case, "native_path": native_path,
         "failed_write_retry": failed_write_retry, "output_failure": output_failure}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--case", required=True, choices=CASES)
    parser.add_argument("--work-root", required=True)
    args = parser.parse_args()
    harness = Harness(args.cli, args.case, args.work_root)
    CASES[args.case](harness)
    print(f"{args.case}: passed; {harness.count} independent executable invocations; evidence retained")

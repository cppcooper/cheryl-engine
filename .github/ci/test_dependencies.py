"""Check dependency and shared environment cache rules without installing packages."""

from copy import deepcopy
from contextlib import redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location("ci_dependencies", Path(__file__).with_name("dependencies.py"))
dependencies = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dependencies)

CONFIGURATION = {
    "cache_version": 2,
    "windows": {"ccache_version": "4.14", "ccache_package_version": "4.14.0"},
    "linux": {
        "apt_common": ["ccache", "ninja-build"],
        "apt_extra": {"headless": ["clang-18"], "modules": ["libgl-dev"]},
    },
}
ENVIRONMENT = {
    "RUNNER_OS": "Linux", "RUNNER_ARCH": "X64", "ImageOS": "ubuntu24",
    "ImageVersion": "20261012.1.0", "GITHUB_REPOSITORY": "cppcooper/cheryl-engine",
    "GITHUB_EVENT_NAME": "push", "GITHUB_REF": "refs/heads/main",
    "GITHUB_RUN_ID": "12345", "GITHUB_RUN_ATTEMPT": "2", "GITHUB_SHA": "a" * 40,
}
BASELINE = "b" * 40
REQUIREMENTS = b"Jinja2==3.1.6\n"
PYTHON_TAG = "cpython-313"
URIS = ["'https://archive.example.test/a_1_amd64.deb' a_1_amd64.deb 42 MD5Sum:abc",
        "'https://archive.example.test/b_2_amd64.deb' b_2_amd64.deb 64 MD5Sum:def"]
Y_FIELDS = ("environment-prefix", "tools-key", "pip-key")


def keys(profile="headless", *, environment=None, configuration=None,
         baseline=BASELINE, requirements=REQUIREMENTS, python_tag=PYTHON_TAG):
    return dependencies.environment_keys(
        profile, baseline, requirements, python_tag=python_tag,
        environment=ENVIRONMENT if environment is None else environment,
        configuration=CONFIGURATION if configuration is None else configuration)


def apt_key(profile="headless", uris=URIS, *, environment=None, configuration=None):
    return dependencies.apt_cache_key(
        profile, uris, environment=ENVIRONMENT if environment is None else environment,
        configuration=CONFIGURATION if configuration is None else configuration)


class IdentityTests(unittest.TestCase):
    def test_prefix(self):
        result = keys()
        prefix = result["environment-prefix"]
        self.assertTrue(prefix.startswith("actions-env-cache-v2-Linux-X64-headless-"))
        for name in ("tools-key", "pip-key"):
            self.assertTrue(result[name].startswith(prefix), name)
        self.assertTrue(apt_key().startswith(prefix))

    def test_revision(self):
        changed = deepcopy(CONFIGURATION)
        changed["cache_version"] = 3
        original = keys()
        revised = keys(configuration=changed)
        for name in Y_FIELDS:
            self.assertNotEqual(original[name], revised[name], name)
            self.assertTrue(revised[name].startswith("actions-env-cache-v3-"), name)
        self.assertNotEqual(apt_key(), apt_key(configuration=changed))

    def test_runner(self):
        original = keys()
        for name, value in (("RUNNER_OS", "Windows"), ("RUNNER_ARCH", "ARM64"),
                            ("ImageOS", "ubuntu26"), ("ImageVersion", "20261013.1.0")):
            with self.subTest(name=name):
                environment = {**ENVIRONMENT, name: value}
                changed = keys(environment=environment)
                for field in Y_FIELDS:
                    self.assertNotEqual(original[field], changed[field], field)
                self.assertNotEqual(apt_key(), apt_key(environment=environment))

    def test_composition(self):
        prefixes = {keys(profile)["environment-prefix"] for profile in ("headless", "modules", "docs")}
        self.assertEqual(len(prefixes), 3)
        self.assertNotEqual(apt_key(), apt_key("modules"))

    def test_package_inputs(self):
        changed = deepcopy(CONFIGURATION)
        changed["linux"]["apt_common"].append("pkg-config")
        original = keys()
        revised = keys(configuration=changed)
        for name in Y_FIELDS:
            self.assertNotEqual(original[name], revised[name], name)
        self.assertNotEqual(apt_key(), apt_key(configuration=changed))

    def test_run_independent(self):
        original = keys()
        changes = {"GITHUB_REF": "refs/pull/42/merge", "GITHUB_EVENT_NAME": "pull_request",
                   "GITHUB_RUN_ID": "67890", "GITHUB_RUN_ATTEMPT": "9", "GITHUB_SHA": "c" * 40}
        revised = keys(environment={**ENVIRONMENT, **changes})
        for name in Y_FIELDS:
            self.assertEqual(original[name], revised[name], name)
        self.assertEqual(apt_key(), apt_key(environment={**ENVIRONMENT, **changes}))

    def test_tool_versions(self):
        environment = {**ENVIRONMENT, "RUNNER_OS": "Windows", "ImageOS": "win25"}
        original = keys(environment=environment)["tools-key"]
        self.assertNotEqual(original, keys(environment=environment, baseline="d" * 40)["tools-key"])
        for field, value in (("ccache_version", "4.15"), ("ccache_package_version", "4.15.0")):
            with self.subTest(field=field):
                changed = deepcopy(CONFIGURATION)
                changed["windows"][field] = value
                self.assertNotEqual(original, keys(environment=environment, configuration=changed)["tools-key"])

    def test_python_inputs(self):
        original = keys()["pip-key"]
        self.assertNotEqual(original, keys(requirements=b"Jinja2==3.1.7\n")["pip-key"])
        self.assertNotEqual(original, keys(python_tag="cpython-314")["pip-key"])

    def test_apt_downloads(self):
        original = apt_key()
        self.assertEqual(original, apt_key(uris=list(reversed(URIS))))
        self.assertNotEqual(original, apt_key(uris=[URIS[0]]))
        changed = [URIS[0].replace("a_1_", "a_2_"), URIS[1]]
        self.assertNotEqual(original, apt_key(uris=changed))
        self.assertNotEqual(original, apt_key(uris=[URIS[0].replace("MD5Sum:abc", "MD5Sum:xyz"), URIS[1]]))

    def test_pure_metadata(self):
        configuration = deepcopy(CONFIGURATION)
        environment = dict(ENVIRONMENT)
        with patch.object(dependencies.subprocess, "run", side_effect=AssertionError("Unexpected command")), \
             patch.object(dependencies.subprocess, "check_output", side_effect=AssertionError("Unexpected Git query")):
            keys(environment=environment, configuration=configuration)
            apt_key(environment=environment, configuration=configuration)
        self.assertEqual(configuration, CONFIGURATION)
        self.assertEqual(environment, ENVIRONMENT)

    def test_invalid_revision(self):
        for revision in (0, -1, True, "2", None):
            with self.subTest(revision=revision):
                configuration = {**CONFIGURATION, "cache_version": revision}
                with self.assertRaises(ValueError):
                    keys(configuration=configuration)

    def test_invalid_runner(self):
        for name, value in (("RUNNER_OS", "Linux\nextra=value"), ("RUNNER_ARCH", "../X64"),
                            ("RUNNER_OS", ""), ("RUNNER_ARCH", None)):
            with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                keys(environment={**ENVIRONMENT, name: value})

    def test_invalid_component(self):
        for changes in ({"baseline": ""}, {"requirements": "Jinja2==3.1.6"},
                        {"python_tag": ""}, {"python_tag": "cpython-313\nextra=value"}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                keys(**changes)
        for uris in ("one URI", [URIS[0], None]):
            with self.subTest(uris=uris), self.assertRaises(ValueError):
                apt_key(uris=uris)


class WriterTests(unittest.TestCase):
    def writer(self, **changes):
        return dependencies.environment_metadata(
            "headless", environment={**ENVIRONMENT, **changes}, configuration=CONFIGURATION)["environment-cache-writer"]

    def test_main(self):
        self.assertEqual(self.writer(), "true")
        self.assertEqual(self.writer(GITHUB_EVENT_NAME="workflow_dispatch"), "true")

    def test_other_refs(self):
        for ref in ("refs/heads/develop", "refs/heads/feature", "refs/pull/3/merge", "refs/tags/main", "main"):
            with self.subTest(ref=ref):
                self.assertEqual(self.writer(GITHUB_REF=ref), "false")

    def test_untrusted_events(self):
        for event in ("pull_request", "pull_request_target", "workflow_run", "issue_comment", "repository_dispatch"):
            with self.subTest(event=event):
                self.assertEqual(self.writer(GITHUB_EVENT_NAME=event), "false")

    def test_other_repositories(self):
        for repository in ("someone/cheryl-engine", "cppcooper/fork", ""):
            with self.subTest(repository=repository):
                self.assertEqual(self.writer(GITHUB_REPOSITORY=repository), "false")


class DependencyScopeTests(unittest.TestCase):
    def scope(self, **changes):
        return dependencies.dependency_scope(environment={**ENVIRONMENT, **changes})

    def test_branch_writers(self):
        for branch in ("main", "develop"):
            for event in ("push", "workflow_dispatch"):
                with self.subTest(branch=branch, event=event):
                    result = self.scope(GITHUB_REF=f"refs/heads/{branch}", GITHUB_EVENT_NAME=event)
                    self.assertEqual(result["dependency-role"], branch)
                    self.assertEqual(result["dependency-cache-writer"], "true")

    def test_pull_base(self):
        for base in ("main", "develop"):
            with self.subTest(base=base):
                result = self.scope(GITHUB_EVENT_NAME="pull_request", GITHUB_REF="refs/pull/3/merge",
                                    GITHUB_BASE_REF=base)
                self.assertEqual(result["dependency-role"], base)
                self.assertEqual(result["dependency-cache-writer"], "false")

    def test_no_pull_writer(self):
        result = self.scope(GITHUB_EVENT_NAME="pull_request", GITHUB_REF="refs/heads/main", GITHUB_BASE_REF="main")
        self.assertEqual(result["dependency-cache-writer"], "false")

    def test_no_other_writer(self):
        changes = ({"GITHUB_REPOSITORY": "someone/cheryl-engine"},
                   {"GITHUB_REF": "refs/heads/feature"}, {"GITHUB_REF": "refs/tags/main"},
                   {"GITHUB_EVENT_NAME": "workflow_run"})
        for change in changes:
            with self.subTest(change=change):
                self.assertEqual(self.scope(**change)["dependency-cache-writer"], "false")


class CommandTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        directory = self.root / ".github/ci"
        directory.mkdir(parents=True)
        (directory / "environment.json").write_text(json.dumps(CONFIGURATION), encoding="utf-8")
        (directory / "vcpkg.json").write_text(json.dumps({"builtin-baseline": BASELINE}), encoding="utf-8")
        (directory / "requirements-modules.txt").write_bytes(REQUIREMENTS)
        (self.root / ".gitmodules").write_text('[submodule "extern/example"]\n', encoding="utf-8")
        self.environment = {**ENVIRONMENT, "RUNNER_TEMP": str(self.root / "runner-temp"),
                            "CHERYL_CI_DEPENDENCY_CACHE_DIR": str(self.root / "build/cache/dependencies")}

    def metadata(self, *, gitlinks=b"160000 commit aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\textern/example\0", changes=None,
                 profile="headless"):
        emissions = {}

        def emit(variable, values):
            emissions.setdefault(variable, {}).update(values)

        with patch.object(dependencies, "ROOT", self.root), \
             patch.dict(dependencies.os.environ, {**self.environment, **(changes or {})}, clear=True), \
             patch.object(dependencies.subprocess, "check_output", return_value=gitlinks), \
             patch.object(dependencies, "run", return_value=".git/modules\n"), \
             patch.object(dependencies, "emit", side_effect=emit):
            dependencies.metadata(profile)
        self.emissions = emissions
        return emissions["GITHUB_OUTPUT"]

    def test_submodules_independent(self):
        original = self.metadata()
        (self.root / ".gitmodules").write_text('[submodule "extern/changed"]\n', encoding="utf-8")
        changed = self.metadata(gitlinks=b"160000 commit dddddddddddddddddddddddddddddddddddddddd\textern/changed\0")
        self.assertNotEqual(original["submodule-key"], changed["submodule-key"])
        for name in Y_FIELDS:
            self.assertEqual(original[name], changed[name], name)

    def test_branch_separation(self):
        main = self.metadata()
        develop = self.metadata(changes={"GITHUB_REF": "refs/heads/develop"})
        self.assertTrue(main["submodule-key"].startswith("build-deps-v2-Linux-X64-main-sources-"))
        self.assertTrue(develop["submodule-key"].startswith("build-deps-v2-Linux-X64-develop-sources-"))
        self.assertEqual(main["submodule-main-prefix"], develop["submodule-main-prefix"])
        for name in Y_FIELDS:
            self.assertEqual(main[name], develop[name], name)
        self.assertEqual(develop["dependency-cache-writer"], "true")
        self.assertEqual(develop["environment-cache-writer"], "false")

    def test_pull_metadata(self):
        for base in ("main", "develop"):
            with self.subTest(base=base):
                branch = self.metadata(changes={"GITHUB_REF": f"refs/heads/{base}"})
                pull = self.metadata(changes={"GITHUB_EVENT_NAME": "pull_request", "GITHUB_REF": "refs/pull/3/merge",
                                              "GITHUB_BASE_REF": base})
                self.assertEqual(branch["submodule-key"], pull["submodule-key"])
                self.assertEqual(pull["dependency-cache-writer"], "false")
                self.assertEqual(pull["environment-cache-writer"], "false")

    def test_manifest_dependencies(self):
        original = self.metadata()
        manifest = {"builtin-baseline": BASELINE, "dependencies": ["freetype", "harfbuzz"]}
        (self.root / ".github/ci/vcpkg.json").write_text(json.dumps(manifest), encoding="utf-8")
        changed = self.metadata()
        self.assertEqual(original["submodule-key"], changed["submodule-key"])
        for name in Y_FIELDS:
            self.assertEqual(original[name], changed[name], name)

    def test_windows_pool(self):
        original = None
        for profile in ("headless", "modules"):
            for ref in ("refs/heads/main", "refs/heads/develop", "refs/pull/3/merge"):
                with self.subTest(profile=profile, ref=ref):
                    metadata = self.metadata(profile=profile, changes={"RUNNER_OS": "Windows", "GITHUB_REF": ref})
                    environment = self.emissions["GITHUB_ENV"]
                    pool = self.root / "build/cache/vcpkg"
                    installed = self.root / "build/ci/vcpkg_installed"
                    values = {name: environment[name] for name in (
                        "CHERYL_CI_VCPKG_CACHE_DIR", "VCPKG_BINARY_SOURCES", "VCPKG_DOWNLOADS")}
                    self.assertEqual(values["CHERYL_CI_VCPKG_CACHE_DIR"], pool.as_posix())
                    self.assertEqual(values["VCPKG_BINARY_SOURCES"], f"clear;files,{pool.as_posix()}/archives,readwrite")
                    self.assertEqual(values["VCPKG_DOWNLOADS"], (pool / "downloads").as_posix())
                    self.assertTrue((pool / "archives").is_dir())
                    self.assertTrue((pool / "downloads").is_dir())
                    self.assertEqual(metadata["vcpkg-installed-dir"], installed.as_posix())
                    self.assertEqual(environment["CHERYL_CI_VCPKG_INSTALLED_DIR"], installed.as_posix())
                    self.assertNotIn("CHERYL_CI_DEPENDENCY_CACHE_KEY", environment)
                    self.assertTrue({"dependency-key", "dependency-exact-prefix", "dependency-profile-prefix",
                                     "dependency-runner-prefix"}.isdisjoint(metadata))
                    if original is None:
                        original = values
                    else:
                        self.assertEqual(original, values)

    def test_requirements_inputs(self):
        original = self.metadata()
        (self.root / ".github/ci/requirements-modules.txt").write_bytes(b"Jinja2==3.1.7\n")
        changed = self.metadata()
        self.assertNotEqual(original["pip-key"], changed["pip-key"])
        for name in ("environment-prefix", "tools-key", "submodule-key"):
            self.assertEqual(original[name], changed[name], name)

    def test_environment_image(self):
        original = self.metadata()
        changed = self.metadata(changes={"ImageVersion": "20261013.1.0"})
        self.assertEqual(original["submodule-key"], changed["submodule-key"])
        for name in Y_FIELDS:
            self.assertNotEqual(original[name], changed[name], name)

    def test_metadata_runs(self):
        original = self.metadata()
        changed = self.metadata(changes={"GITHUB_REF": "refs/heads/develop", "GITHUB_RUN_ID": "99999",
                                         "GITHUB_RUN_ATTEMPT": "3"})
        for name in Y_FIELDS:
            self.assertEqual(original[name], changed[name], name)
        self.assertEqual(original["environment-cache-writer"], "true")
        self.assertEqual(changed["environment-cache-writer"], "false")

    def test_apt_plan(self):
        output = "Reading package lists...\n" + "\n".join(reversed(URIS)) + "\n"
        with patch.object(dependencies, "ROOT", self.root), \
             patch.dict(dependencies.os.environ, self.environment, clear=True), \
             patch.object(dependencies, "run", return_value=output) as command, \
             patch.object(dependencies, "emit") as emit:
            dependencies.apt_plan("headless")
        self.assertIn("--print-uris", command.call_args.args[0])
        self.assertEqual(command.call_args.kwargs["environment"]["LC_ALL"], "C")
        self.assertEqual(emit.call_args.args, ("GITHUB_OUTPUT", {
            "needed": "true", "key": apt_key(environment=self.environment)}))

    def test_apt_empty(self):
        with patch.object(dependencies, "ROOT", self.root), \
             patch.dict(dependencies.os.environ, self.environment, clear=True), \
             patch.object(dependencies, "run", return_value="No packages to download.\n"), \
             patch.object(dependencies, "emit") as emit:
            dependencies.apt_plan("modules")
        self.assertEqual(emit.call_args.args[1]["needed"], "false")

    def test_windows_install(self):
        vcpkg = self.root / "vcpkg with spaces"
        for profile in ("headless", "modules"):
            with self.subTest(profile=profile), patch.object(dependencies, "ROOT", self.root), \
                 patch.dict(dependencies.os.environ, {"CHERYL_CI_VCPKG_ROOT": str(vcpkg)}, clear=True), \
                 patch.object(dependencies, "run") as command:
                dependencies.windows_install(profile)
            expected = [str(vcpkg / "vcpkg.exe"), "install", "--triplet=x64-windows", "--host-triplet=x64-windows",
                        f"--x-manifest-root={self.root.as_posix()}/.github/ci",
                        f"--x-install-root={self.root.as_posix()}/build/ci/vcpkg_installed",
                        "--x-no-default-features", "--x-feature=hwloc"]
            if profile == "modules":
                expected.append("--x-feature=native")
            command.assert_called_once_with(expected, cwd=self.root)

    def test_windows_profile(self):
        with patch.object(dependencies, "run") as command, self.assertRaises(ValueError):
            dependencies.windows_install("docs")
        command.assert_not_called()

    def test_linux_install(self):
        environment = {"CHERYL_CI_VCPKG_ROOT": str(self.root / "vcpkg"),
                       "CHERYL_CI_VCPKG_INSTALLED_DIR": str(self.root / "installed")}
        with patch.object(dependencies, "ROOT", self.root), \
             patch.dict(dependencies.os.environ, environment, clear=True), patch.object(dependencies, "run") as command:
            dependencies.linux_install("modules")
        arguments = command.call_args.args[0]
        self.assertEqual(arguments[1], str(self.root / "tools/dependencies/prepare.py"))
        self.assertNotIn("--native", arguments)

    def test_cache_does_not_select_packages(self):
        cache = self.root / "build/cache/vcpkg/archives"
        (cache / "aa").mkdir(parents=True)
        (cache / "aa" / ("a" * 64 + ".zip")).write_bytes(b"older cached package")
        manifest = {"builtin-baseline": BASELINE, "dependencies": ["freetype", "zlib"],
                    "features": {"hwloc": {"dependencies": ["hwloc"]}, "native": {"dependencies": ["hidapi"]}}}
        manifest_path = self.root / ".github/ci/vcpkg.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        with patch.object(dependencies, "ROOT", self.root), \
             patch.dict(dependencies.os.environ, {
                 "CHERYL_CI_VCPKG_ROOT": str(self.root / "vcpkg"),
                 "VCPKG_BINARY_SOURCES": f"clear;files,{cache.as_posix()},readwrite",
             }, clear=True), patch.object(dependencies, "run") as command:
            dependencies.windows_install("modules")
        command.assert_called_once()
        arguments = command.call_args.args[0]
        self.assertEqual(arguments[1], "install")
        self.assertIn(f"--x-manifest-root={manifest_path.parent.as_posix()}", arguments)
        self.assertFalse(any(argument.startswith(("--only-binarycaching", "--no-downloads", "--only-downloads"))
                             for argument in arguments))
        self.assertEqual(json.loads(manifest_path.read_text(encoding="utf-8")), manifest)

    def test_windows_operation(self):
        with patch("sys.argv", ["dependencies.py", "windows-install", "--profile", "modules"]), \
             patch.object(dependencies, "windows_install") as install:
            dependencies.main()
        install.assert_called_once_with("modules")


class WindowsToolsTests(unittest.TestCase):
    def setUp(self):
        output = redirect_stdout(io.StringIO())
        output.__enter__()
        self.addCleanup(output.__exit__, None, None, None)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve() / "checkout with spaces"
        configuration = self.root / ".github/ci/environment.json"
        configuration.parent.mkdir(parents=True)
        configuration.write_text(json.dumps(CONFIGURATION), encoding="utf-8")
        configuration.with_name("vcpkg.json").write_text(json.dumps({"builtin-baseline": BASELINE}), encoding="utf-8")
        vcpkg = self.root / "vcpkg"
        (vcpkg / ".git").mkdir(parents=True)
        (vcpkg / "vcpkg.exe").write_bytes(b"vcpkg fixture")
        self.tools = self.root / "cached tools"
        chocolatey = self.root / "chocolatey"
        self.package = chocolatey / "lib/ccache/tools/bin/ccache.exe"
        self.package.parent.mkdir(parents=True)
        self.package.write_bytes(b"ccache fixture")
        self.github_path = self.root / "github-path.txt"
        self.github_path.write_text("existing path\n", encoding="utf-8")
        self.commands = []
        self.install_results = []
        self.version = CONFIGURATION["windows"]["ccache_version"]
        self.install = ["choco", "install", "ccache", "--version=4.14.0", "--yes", "--no-progress"]
        self.addCleanup(patch.stopall)
        patch.object(dependencies, "ROOT", self.root).start()
        patch.dict(dependencies.os.environ, {
            "CHERYL_CI_VCPKG_ROOT": str(vcpkg), "CHERYL_CI_CCACHE_TOOLS": str(self.tools),
            "ChocolateyInstall": str(chocolatey), "GITHUB_PATH": str(self.github_path),
        }, clear=True).start()
        patch.object(dependencies.subprocess, "run", side_effect=self.execute).start()
        self.sleep = patch("time.sleep").start()

    def execute(self, command, **options):
        self.commands.append(command)
        if command == self.install:
            result = self.install_results.pop(0) if self.install_results else 0
            return subprocess.CompletedProcess(command, result)
        return subprocess.CompletedProcess(command, 0, stdout=f"ccache version {self.version}\n")

    def assert_published(self):
        self.assertEqual((self.tools / "ccache.exe").read_bytes(), b"ccache fixture")
        self.assertEqual(self.github_path.read_text(encoding="utf-8").splitlines(), ["existing path", str(self.tools)])

    def test_install(self):
        dependencies.windows_tools()
        self.assertEqual(self.commands.count(self.install), 1)
        self.sleep.assert_not_called()
        self.assert_published()

    def test_retry_install(self):
        self.install_results = [1, 1, 0]
        dependencies.windows_tools()
        self.assertEqual(self.commands.count(self.install), 3)
        self.assertEqual([entry.args[0] for entry in self.sleep.call_args_list], [10, 20])
        self.assert_published()

    def test_install_failure(self):
        self.install_results = [1, 1, 1]
        with self.assertRaises(subprocess.CalledProcessError) as failure:
            dependencies.windows_tools()
        self.assertEqual(failure.exception.cmd, self.install)
        self.assertEqual(self.commands.count(self.install), 3)
        self.assertEqual([entry.args[0] for entry in self.sleep.call_args_list], [10, 20])
        self.assertFalse(self.tools.exists())
        self.assertEqual(self.github_path.read_text(encoding="utf-8"), "existing path\n")

    def test_cached(self):
        self.tools.mkdir()
        (self.tools / "ccache.exe").write_bytes(b"ccache fixture")
        dependencies.windows_tools()
        self.assertNotIn(self.install, self.commands)
        self.sleep.assert_not_called()
        self.assert_published()

    def test_wrong_version(self):
        self.version = "unexpected"
        with self.assertRaisesRegex(RuntimeError, "Unexpected ccache version"):
            dependencies.windows_tools()
        self.assertEqual(self.commands.count(self.install), 1)
        self.sleep.assert_not_called()
        self.assertEqual(self.github_path.read_text(encoding="utf-8"), "existing path\n")


class PreparedTests(unittest.TestCase):
    def setUp(self):
        output = redirect_stdout(io.StringIO())
        output.__enter__()
        self.addCleanup(output.__exit__, None, None, None)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve() / "checkout"
        self.manifest = {
            "builtin-baseline": BASELINE,
            "dependencies": ["icu", {"name": "freetype", "default-features": False}, "harfbuzz"],
            "features": {"hwloc": {"dependencies": ["hwloc"]}, "native": {"dependencies": ["hidapi"]}},
        }
        self.manifest_path = self.root / ".github/ci/vcpkg.json"
        self.manifest_path.parent.mkdir(parents=True)
        self.manifest_path.write_text(json.dumps(self.manifest), encoding="utf-8")
        self.installed = self.root / "build/ci/vcpkg_installed"
        self.status = self.installed / "vcpkg/status"
        self.status.parent.mkdir(parents=True)
        self.status.write_text("\n\n".join(self.package(name) for name in (
            "icu", "freetype", "harfbuzz", "hwloc", "hidapi", "vcpkg-cmake", "vcpkg-cmake-config")) + "\n",
            encoding="utf-8")
        for directory, filename in (("include", "unicode/utypes.h"), ("lib", "icuuc.lib"),
                                    ("bin", "icuuc.dll"), ("share", "icu/vcpkg_abi_info.txt"),
                                    ("debug/lib", "icuucd.lib"), ("debug/bin", "icuucd.dll")):
            file = self.installed / "x64-windows" / directory / filename
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"prepared dependency")
        self.state_path = self.installed / dependencies.PREPARED_STATE
        self.addCleanup(patch.stopall)
        patch.object(dependencies, "ROOT", self.root).start()
        patch.object(dependencies, "run", side_effect=AssertionError("Verification must not install dependencies")).start()
        patch.dict(dependencies.os.environ, ENVIRONMENT, clear=True).start()

    @staticmethod
    def package(name, *, triplet="x64-windows", status="install ok installed", feature=None):
        fields = [f"Package: {name}", "Version: 1.2.3", f"Architecture: {triplet}", f"Status: {status}"]
        if feature:
            fields.append(f"Feature: {feature}")
        return "\n".join(fields)

    def state(self, changes):
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        state.update(changes)
        self.state_path.write_text(json.dumps(state), encoding="utf-8")

    def test_shared_installation(self):
        dependencies.windows_stamp("modules")
        original = self.state_path.read_bytes()
        for profile in ("headless", "modules"):
            with self.subTest(profile=profile):
                dependencies.windows_verify(profile)
                self.assertEqual(self.state_path.read_bytes(), original)

    def test_relocated(self):
        dependencies.windows_stamp("modules")
        relocated = self.root.parent / "consumer checkout"
        shutil.copytree(self.root, relocated)
        with patch.object(dependencies, "ROOT", relocated):
            dependencies.windows_verify("modules")

    def test_linux_transfer(self):
        (self.installed / "x64-windows").rename(self.installed / "x64-linux-dynamic")
        self.status.write_text(self.status.read_text(encoding="utf-8").replace("x64-windows", "x64-linux-dynamic"), encoding="utf-8")
        dependencies.native_stamp("modules", triplet=dependencies.LINUX_TRIPLET)
        dependencies.native_verify("headless", triplet=dependencies.LINUX_TRIPLET)
        state = json.loads(self.state_path.read_text(encoding="utf-8"))
        self.assertEqual(state["target_triplet"], "x64-linux-dynamic")
        self.assertEqual(state["features"], ["hwloc"])

    def test_headless_subset(self):
        self.status.write_text("\n\n".join(self.package(name) for name in (
            "icu", "freetype", "harfbuzz", "hwloc")), encoding="utf-8")
        dependencies.windows_stamp("headless")
        dependencies.windows_verify("headless")
        with self.assertRaisesRegex(ValueError, "features"):
            dependencies.windows_verify("modules")

    def test_manifest_change(self):
        dependencies.windows_stamp("modules")
        self.manifest["dependencies"].append("zlib")
        self.manifest_path.write_text(json.dumps(self.manifest), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "manifest"):
            dependencies.windows_verify("modules")

    def test_new_package_installed(self):
        self.manifest["dependencies"].append("zlib")
        self.manifest_path.write_text(json.dumps(self.manifest), encoding="utf-8")
        original = self.status.read_bytes()

        def complete_install(*arguments, **options):
            self.assertIn(f"--x-manifest-root={self.manifest_path.parent.as_posix()}", arguments[0])
            with self.status.open("a", encoding="utf-8") as output:
                output.write("\n" + self.package("zlib") + "\n")
            (self.installed / "x64-windows/include/zlib.h").write_bytes(b"new dependency")

        with patch.dict(dependencies.os.environ, {"CHERYL_CI_VCPKG_ROOT": str(self.root / "vcpkg")}), \
             patch.object(dependencies, "run", side_effect=complete_install) as command:
            dependencies.windows_install("modules")
        command.assert_called_once()
        dependencies.windows_stamp("modules")
        dependencies.windows_verify("modules")
        self.assertNotEqual(self.status.read_bytes(), original)
        self.assertEqual(json.loads(self.state_path.read_text(encoding="utf-8"))["status_sha256"],
                         dependencies.digest(self.status.read_bytes()))

    def test_conditional_dependency(self):
        self.manifest["dependencies"].append({"name": "linux-only", "platform": "linux"})
        self.manifest_path.write_text(json.dumps(self.manifest), encoding="utf-8")
        dependencies.windows_stamp("modules")
        dependencies.windows_verify("modules")

    def test_identity_mismatch(self):
        for changes in ({"baseline": "c" * 40}, {"target_triplet": "x64-windows-static"},
                        {"host_triplet": "arm64-windows"}, {"manifest_sha256": "a" * 64}):
            with self.subTest(changes=changes):
                dependencies.windows_stamp("modules")
                self.state(changes)
                with self.assertRaisesRegex(ValueError, "manifest or triplet"):
                    dependencies.windows_verify("modules")

    def test_source_identity(self):
        dependencies.windows_stamp("modules")
        for environment in ({"GITHUB_SHA": "c" * 40}, {"GITHUB_RUN_ID": "67890"}):
            with self.subTest(environment=environment), patch.dict(dependencies.os.environ, environment):
                with self.assertRaisesRegex(ValueError, "source revision, or workflow run"):
                    dependencies.windows_verify("modules")
        with patch.dict(dependencies.os.environ, {"GITHUB_RUN_ATTEMPT": "3"}):
            dependencies.windows_verify("modules")

    def test_invalid_source(self):
        for environment in ({"GITHUB_SHA": ""}, {"GITHUB_SHA": "not-a-revision"}, {"GITHUB_RUN_ID": "0"},
                            {"GITHUB_RUN_ID": "01"}, {"GITHUB_RUN_ID": "-1"}, {"GITHUB_RUN_ID": ""}):
            with self.subTest(environment=environment), patch.dict(dependencies.os.environ, environment):
                with self.assertRaisesRegex(ValueError, "source"):
                    dependencies.windows_stamp("modules")

    def test_metadata_schema(self):
        for changes in ({"schema": 2}, {"schema": True}, {"extra": "unknown"}):
            with self.subTest(changes=changes):
                dependencies.windows_stamp("modules")
                self.state(changes)
                with self.assertRaisesRegex(ValueError, "metadata"):
                    dependencies.windows_verify("modules")
        for state in ([], None, {"schema": 1}):
            with self.subTest(state=state):
                self.state_path.write_text(json.dumps(state), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "metadata"):
                    dependencies.windows_verify("modules")

    def test_unknown_features(self):
        for features in ([], ["hwloc", "native", "unknown"], "modules", None):
            with self.subTest(features=features):
                dependencies.windows_stamp("modules")
                self.state({"features": features})
                with self.assertRaisesRegex(ValueError, "features"):
                    dependencies.windows_verify("headless")

    def test_missing_metadata(self):
        with self.assertRaises(FileNotFoundError):
            dependencies.windows_verify("modules")

    def test_missing_status(self):
        self.status.unlink()
        with self.assertRaises(FileNotFoundError):
            dependencies.windows_stamp("modules")
        self.assertFalse(self.state_path.exists())

    def test_empty_tree(self):
        for directory in ("include", "lib", "share", "debug/lib", "bin", "debug/bin"):
            with self.subTest(directory=directory):
                path = self.installed / "x64-windows" / directory
                preserved = self.installed / "preserved"
                path.rename(preserved)
                path.mkdir()
                with self.assertRaisesRegex(ValueError, "missing files"):
                    dependencies.windows_stamp("modules")
                path.rmdir()
                preserved.rename(path)

    def test_incomplete_packages(self):
        original = self.status.read_text(encoding="utf-8")
        for package in (self.package("hidapi", status="deinstall ok half-installed"),
                        self.package("hidapi", status="install ok half-installed", feature="extra")):
            with self.subTest(package=package):
                self.status.write_text(original.replace(self.package("hidapi"), package), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "incomplete installed package: hidapi"):
                    dependencies.windows_stamp("modules")
                self.assertFalse(self.state_path.exists())

    def test_no_target_packages(self):
        for status in ("", self.package("hidapi", triplet="arm64-windows"),
                       self.package("hidapi", feature="extra")):
            with self.subTest(status=status):
                self.status.write_text(status, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "no installed core packages for x64-windows"):
                    dependencies.windows_stamp("modules")

    def test_status_changed(self):
        dependencies.windows_stamp("modules")
        with self.status.open("a", encoding="utf-8") as output:
            output.write("\n" + self.package("unrelated-package") + "\n")
        with self.assertRaisesRegex(ValueError, "status changed"):
            dependencies.windows_verify("modules")

    def test_status_records(self):
        original = self.status.read_text(encoding="utf-8")
        self.status.write_bytes((original + "\n" + self.package("harfbuzz", feature="freetype") + "\n").replace(
            "\n", "\r\n").encode())
        dependencies.windows_stamp("modules")
        dependencies.windows_verify("modules")

    def test_hold_and_removed_packages(self):
        original = self.status.read_text(encoding="utf-8")
        for want in ("deinstall", "purge"):
            with self.subTest(want=want):
                status = original.replace(self.package("hidapi"), self.package("hidapi", status="hold ok installed"))
                status += "\n" + self.package("old-package", status=f"{want} ok not-installed") + "\n"
                status += "\n" + self.package("zlib", status="install ok installed") + "\n"
                self.status.write_text(status, encoding="utf-8")
                dependencies.windows_stamp("modules")
                dependencies.windows_verify("modules")
                installed = dependencies.installed_status(self.status.read_bytes())
                self.assertIn("hidapi", installed)
                self.assertIn("zlib", installed)
                self.assertNotIn("old-package", installed)

    def test_malformed_status_line(self):
        original = self.status.read_text(encoding="utf-8")
        for status in ("install", "unknown ok installed", "install error installed", "install ok unknown",
                       "install ok installed extra"):
            with self.subTest(status=status):
                self.status.write_text(original.replace(self.package("hidapi"), self.package("hidapi", status=status)),
                                       encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "Malformed vcpkg installed package status: hidapi"):
                    dependencies.windows_stamp("modules")

    def test_invalid_status(self):
        self.status.write_text(self.package("icu") + "\nPackage: duplicate\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Malformed"):
            dependencies.windows_stamp("modules")

    def test_prepared_operations(self):
        for operation, name in (("windows-stamp", "windows_stamp"), ("windows-verify", "windows_verify")):
            with self.subTest(operation=operation), patch("sys.argv", ["dependencies.py", operation, "--profile", "modules"]), \
                 patch.object(dependencies, name) as command:
                dependencies.main()
            command.assert_called_once_with("modules")


class PreparedCmakeTests(unittest.TestCase):
    def test_harfbuzz_imports(self):
        helper = Path(__file__).resolve().parents[2] / "tools/dependencies/prepared.cmake"
        for debug in (False, True):
            with self.subTest(debug=debug), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                prefix = root / "installed/x64-windows"
                for name in ("include/harfbuzz/hb.h", "lib/harfbuzz.lib", "bin/harfbuzz.dll"):
                    path = prefix / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b"fixture")
                if debug:
                    for name in ("debug/lib/harfbuzz.lib", "debug/bin/harfbuzz.dll"):
                        path = prefix / name
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_bytes(b"debug fixture")
                (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
set(CMAKE_SYSTEM_NAME Windows)
project(cheryl LANGUAGES NONE)
set(CMAKE_FIND_LIBRARY_PREFIXES "")
set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
set(VCPKG_INSTALLED_DIR "{(root / 'installed').as_posix()}")
set(VCPKG_TARGET_TRIPLET x64-windows)
set(CHERYL_BUILD_WORKERS OFF)
set(CHERYL_NATIVE_INPUT OFF)
include("{helper.as_posix()}")
get_target_property(kind harfbuzz::harfbuzz TYPE)
get_target_property(import_library harfbuzz::harfbuzz IMPORTED_IMPLIB_RELEASE)
if(NOT kind STREQUAL "SHARED_LIBRARY" OR NOT import_library MATCHES "/lib/harfbuzz.lib$")
    message(FATAL_ERROR "The prepared HarfBuzz DLL/import pair is incomplete")
endif()
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/runtime.txt" CONTENT "$<TARGET_FILE:harfbuzz::harfbuzz>")
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/link.txt" CONTENT "$<TARGET_LINKER_FILE:harfbuzz::harfbuzz>")
''', encoding="utf-8")
                for configuration in ("Release", "Debug", "RelWithDebInfo"):
                    build = root / configuration
                    result = subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                                             f"-DCMAKE_BUILD_TYPE={configuration}"], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    directory = prefix / "debug" if debug and configuration == "Debug" else prefix
                    self.assertEqual(Path((build / "runtime.txt").read_text(encoding="utf-8")), directory / "bin/harfbuzz.dll")
                    self.assertEqual(Path((build / "link.txt").read_text(encoding="utf-8")), directory / "lib/harfbuzz.lib")

    def test_harfbuzz_supplied(self):
        helper = Path(__file__).resolve().parents[2] / "tools/dependencies/prepared.cmake"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "installed/x64-windows/lib").mkdir(parents=True)
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
set(CMAKE_SYSTEM_NAME Windows)
project(cheryl LANGUAGES NONE)
set(VCPKG_INSTALLED_DIR "{(root / 'installed').as_posix()}")
set(VCPKG_TARGET_TRIPLET x64-windows)
set(CHERYL_BUILD_WORKERS OFF)
set(CHERYL_NATIVE_INPUT OFF)
add_library(harfbuzz INTERFACE)
set_property(TARGET harfbuzz PROPERTY SUPPLIED_MARKER preserved)
include("{helper.as_posix()}")
get_target_property(marker harfbuzz SUPPLIED_MARKER)
if(TARGET cheryl_prepared_harfbuzz OR NOT marker STREQUAL "preserved")
    message(FATAL_ERROR "A supplied HarfBuzz target was replaced")
endif()
''', encoding="utf-8")
            result = subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build"), "-G", "Ninja"],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_shared_hwloc_closure(self):
        for system in ("Linux", "Windows"):
            with self.subTest(system=system):
                self.check_hwloc_configurations(system, debug=False)

    def test_debug_hwloc_closure(self):
        for system in ("Linux", "Windows"):
            with self.subTest(system=system):
                self.check_hwloc_configurations(system, debug=True)

    def test_windows_hwloc_debug_pair(self):
        for debug in ("library", "runtime"):
            with self.subTest(debug=debug):
                self.check_hwloc_configurations("Windows", debug=debug)

    def test_windows_hwloc_names(self):
        for runtime in ("hwloc-15.dll", "libhwloc-15.dll", "libhwloc.dll"):
            with self.subTest(runtime=runtime):
                self.check_hwloc_configurations("Windows", debug=True, runtime=runtime)

    def check_hwloc_configurations(self, system, *, debug, runtime=None):
        helper = Path(__file__).resolve().parents[2] / "tools/dependencies/prepared.cmake"
        windows = system == "Windows"
        triplet = "x64-windows" if windows else "x64-linux-dynamic"
        runtime = runtime or ("hwloc.dll" if windows else "libhwloc.so")
        runtime_directory = "bin" if windows else "lib"
        library = "hwloc.lib" if windows else "libhwloc.so"
        complete_debug = debug is True
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / "installed" / triplet
            (prefix / "include").mkdir(parents=True)
            (prefix / "lib/pkgconfig").mkdir(parents=True)
            (prefix / runtime_directory).mkdir(exist_ok=True)
            (prefix / "include/hwloc.h").write_bytes(b"fixture")
            (prefix / "lib/libhwloc.a").write_bytes(b"wrong static fixture")
            (prefix / "lib" / library).write_bytes(b"shared link fixture")
            (prefix / runtime_directory / runtime).write_bytes(b"shared runtime fixture")
            (prefix / "lib/pkgconfig/hwloc.pc").write_text("Libs: -L${libdir} -lhwloc -lm\nLibs.private: -lpthread\n", encoding="utf-8")
            if windows:
                (prefix / "include/harfbuzz").mkdir()
                (prefix / "include/harfbuzz/hb.h").write_bytes(b"fixture")
                (prefix / "lib/harfbuzz.lib").write_bytes(b"import fixture")
                (prefix / "bin/harfbuzz.dll").write_bytes(b"runtime fixture")
            if complete_debug or debug == "library":
                (prefix / "debug/lib").mkdir(parents=True)
                (prefix / "debug/lib" / library).write_bytes(b"debug link fixture")
            if complete_debug or debug == "runtime":
                (prefix / "debug" / runtime_directory).mkdir(parents=True, exist_ok=True)
                (prefix / "debug" / runtime_directory / runtime).write_bytes(b"debug runtime fixture")
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
set(CMAKE_SYSTEM_NAME {system})
project(cheryl LANGUAGES NONE)
set(CMAKE_FIND_LIBRARY_PREFIXES "{'' if windows else 'lib'}")
set(CMAKE_FIND_LIBRARY_SUFFIXES "{'.lib' if windows else '.so;.a'}")
set(VCPKG_INSTALLED_DIR "{(root / 'installed').as_posix()}")
set(VCPKG_TARGET_TRIPLET {triplet})
set(CHERYL_NATIVE_INPUT OFF)
include("{helper.as_posix()}")
get_target_property(location cheryl_prepared_hwloc IMPORTED_LOCATION)
get_target_property(closure cheryl_prepared_hwloc INTERFACE_LINK_LIBRARIES)
get_target_property(configurations cheryl_prepared_hwloc IMPORTED_CONFIGURATIONS)
if(NOT location STREQUAL "{(prefix / runtime_directory / runtime).as_posix()}" OR closure)
    message(FATAL_ERROR "Incorrect shared prepared closure: ${{location}} / ${{closure}}")
endif()
if(NOT configurations STREQUAL "{'RELEASE;DEBUG' if complete_debug else 'RELEASE'}")
    message(FATAL_ERROR "Incorrect prepared configurations: ${{configurations}}")
endif()
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/hwloc.txt" CONTENT "$<TARGET_FILE:cheryl_prepared_hwloc>")
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/hwloc-link.txt" CONTENT "$<TARGET_LINKER_FILE:cheryl_prepared_hwloc>")
''', encoding="utf-8")
            self.check_generated_locations(root, prefix, debug=complete_debug,
                                           runtime=f"{runtime_directory}/{runtime}", library=f"lib/{library}")

    def test_sdk_shared_export(self):
        repository = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "source.cpp").write_text("int fixture() { return 0; }\n", encoding="utf-8")
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
project(sdk_export LANGUAGES CXX)
set(CHERYL_REPOSITORY_ROOT "{repository.as_posix()}")
set(CHERYL_BUILD_ROOT "${{CMAKE_BINARY_DIR}}")
set(CHERYL_BUILD_SHARED_LIBS ON)
set(VERSION_PROJECT_ENGINE 1.0.0)
set(VERSION_PROJECT_CHERYL 1.0.0)
set(TARGET_LIB_ENGINE engine)
set(TARGET_LIB_CHERYL cheryl)
set(TARGET_LIB_ENGINE_S_LOGGING_CONFIG logging)
set(CHERYL_LOG_PROFILE release)
set(CHERYL_LOG_COMPILED_MASK "")
add_library(engine SHARED source.cpp)
add_library(public_native SHARED source.cpp)
add_library(cheryl INTERFACE)
target_link_libraries(cheryl INTERFACE engine)
add_library(logging INTERFACE)
target_compile_definitions(logging INTERFACE CHERYL_LOG_PROFILE=2)
add_library(ctti INTERFACE)
add_library(glm INTERFACE)
add_library(glm::glm-header-only ALIAS glm)
add_library(harfbuzz::harfbuzz SHARED IMPORTED)
if(WIN32)
    set_target_properties(harfbuzz::harfbuzz PROPERTIES
        IMPORTED_LOCATION "${{CMAKE_SOURCE_DIR}}/harfbuzz.dll"
        IMPORTED_IMPLIB "${{CMAKE_SOURCE_DIR}}/harfbuzz.lib")
else()
    set_property(TARGET harfbuzz::harfbuzz PROPERTY IMPORTED_LOCATION "${{CMAKE_SOURCE_DIR}}/libharfbuzz.so")
endif()
set_property(GLOBAL PROPERTY CHERYL_SHARED_DEPENDENCIES "harfbuzz::harfbuzz|engine;public_native|engine")
target_link_libraries(engine PUBLIC public_native PRIVATE harfbuzz::harfbuzz)
include("{(repository / 'cmake/CherylSdk.cmake').as_posix()}")
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/build-links.txt"
    CONTENT "$<TARGET_GENEX_EVAL:engine,$<TARGET_PROPERTY:engine,LINK_LIBRARIES>>")
''', encoding="utf-8")
            build = root / "build"
            result = subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                                     "-DCMAKE_BUILD_TYPE=Release"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            links = (build / "build-links.txt").read_text(encoding="utf-8")
            self.assertIn("harfbuzz::harfbuzz", links)
            self.assertIn("public_native", links)
            public_export = next((build / "CMakeFiles/Export").rglob("CherylTargets.cmake")).read_text(encoding="utf-8")
            self.assertIn("Cheryl::Dependency_public_native", public_export)
            self.assertNotIn("harfbuzz::harfbuzz", public_export)
            prefix = root / "sdk"
            package = prefix / "lib/cmake/Cheryl"
            package.mkdir(parents=True)
            for name in ("CherylConfig.cmake", "CherylConfigVersion.cmake", "CherylNativeTargets.cmake"):
                shutil.copy2(build / name, package / name)
            for export in (build / "CMakeFiles/Export").rglob("CherylTargets*.cmake"):
                shutil.copy2(export, package / export.name)
                for name in re.findall(r'(?m)^\s+IMPORTED_(?:LOCATION|IMPLIB)[A-Z_]* "\$\{_IMPORT_PREFIX\}/([^"]+)"',
                                       export.read_text(encoding="utf-8")):
                    artifact = prefix / name
                    artifact.parent.mkdir(parents=True, exist_ok=True)
                    artifact.write_bytes(b"configured export fixture")
            (prefix / "include/cheryl").mkdir(parents=True)
            (prefix / "include/glm").mkdir()
            consumer = root / "consumer"
            consumer.mkdir()
            (consumer / "source.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            (consumer / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.28)
project(sdk_consumer LANGUAGES CXX)
find_package(Cheryl CONFIG REQUIRED)
add_executable(consumer source.cpp)
target_link_libraries(consumer PRIVATE Cheryl::Engine)
if(TARGET harfbuzz::harfbuzz OR TARGET Freetype::Freetype OR TARGET ICU::uc)
    message(FATAL_ERROR "Shared Cheryl discovered a private native package")
endif()
''', encoding="utf-8")
            result = subprocess.run(["cmake", "-S", str(consumer), "-B", str(root / "consumer-build"), "-G", "Ninja",
                                     f"-DCMAKE_PREFIX_PATH={prefix}", "-DCMAKE_DISABLE_FIND_PACKAGE_harfbuzz=TRUE",
                                     "-DCMAKE_DISABLE_FIND_PACKAGE_Freetype=TRUE", "-DCMAKE_DISABLE_FIND_PACKAGE_ICU=TRUE",
                                     "-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux native link inputs")
    def test_sdk_static_native_links(self):
        repository = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            native = root / "native.v1+prefix"
            native.mkdir()
            for name in ("libdw.so", "libelf.so", "libhwloc.so", "libicuuc.so", "libpthread.a"):
                (native / name).write_bytes(b"native link fixture")
            (root / "source.cpp").write_text("int fixture() { return 0; }\n", encoding="utf-8")
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
project(sdk_static_export LANGUAGES CXX)
include("{(repository / 'cmake/CherylLibraries.cmake').as_posix()}")
set(CHERYL_REPOSITORY_ROOT "{repository.as_posix()}")
set(CHERYL_BUILD_ROOT "${{CMAKE_BINARY_DIR}}")
set(CHERYL_BUILD_SHARED_LIBS OFF)
set(VERSION_PROJECT_ENGINE 1.0.0)
set(VERSION_PROJECT_CHERYL 1.0.0)
set(TARGET_LIB_ENGINE engine)
set(TARGET_LIB_CHERYL cheryl)
set(TARGET_LIB_ENGINE_S_LOGGING_CONFIG logging)
set(TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS signals)
set(CHERYL_LOG_PROFILE release)
set(CHERYL_LOG_COMPILED_MASK "")
set(CHERYL_HWLOC_TARGET supplied_hwloc)
add_library(engine STATIC source.cpp)
add_library(cheryl INTERFACE)
target_link_libraries(cheryl INTERFACE engine)
foreach(target IN ITEMS logging signals ctti glm cheryl_icu backward)
    add_library(${{target}} INTERFACE)
endforeach()
target_compile_definitions(logging INTERFACE CHERYL_LOG_PROFILE=2)
add_library(glm::glm-header-only ALIAS glm)
add_library(Threads::Threads INTERFACE IMPORTED)
add_library(Backward::Interface ALIAS backward)
target_link_libraries(backward INTERFACE "{native.as_posix()}/libdw.so"
    "$<$<BOOL:{native.as_posix()}/libelf.so>:{native.as_posix()}/libelf.so>"
    "$<$<BOOL:{native.as_posix()}/libpthread.a>:{native.as_posix()}/libpthread.a>")
add_library(Cheryl::ICU ALIAS cheryl_icu)
add_library(ICU::uc SHARED IMPORTED)
set_property(TARGET ICU::uc PROPERTY IMPORTED_LOCATION "{native.as_posix()}/libicuuc.so")
target_link_libraries(cheryl_icu INTERFACE ICU::uc)
add_library(supplied_hwloc INTERFACE IMPORTED)
set_property(TARGET supplied_hwloc PROPERTY INTERFACE_LINK_LIBRARIES "{native.as_posix()}/libhwloc.so")
target_link_libraries(engine PRIVATE Backward::Interface Cheryl::ICU supplied_hwloc)
cheryl_shared_dependency(ICU::uc OWNER engine)
cheryl_shared_dependency(supplied_hwloc OWNER engine)
include("{(repository / 'cmake/CherylSdk.cmake').as_posix()}")
''', encoding="utf-8")
            build = root / "build"
            result = subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                                     "-DCMAKE_BUILD_TYPE=Release"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            exports = "\n".join(path.read_text(encoding="utf-8")
                                for path in (build / "CMakeFiles/Export").rglob("CherylTargets*.cmake"))
            self.assertNotIn(native.as_posix(), exports)
            self.assertNotIn("supplied_hwloc", exports)
            self.assertNotIn("ICU::uc", exports)
            self.assertIn("Threads::Threads", exports)
            records = json.loads((build / "cheryl-sdk.json").read_text(encoding="utf-8"))["shared_runtime_dependencies"]
            self.assertEqual({entry["artifact"] for entry in records}, {"libdw.so", "libelf.so", "libhwloc.so", "libicuuc.so"})
            for entry in records:
                self.assertEqual(entry["deployment"], "sdk")
                self.assertIn(entry["sdk_target"], exports)

    def test_sdk_native_headers(self):
        helper = Path(__file__).resolve().parents[2] / "cmake/CherylSdkHeaders.cmake"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, content in {
                "provider/include/TGUI/TGUI.hpp": "selected native API\n",
                "provider/include/TGUI/Config.hpp": "source configuration\n",
                "provider/generated/TGUI/Config.hpp": "generated native configuration\n",
                "provider/include/Unrelated/private.h": "not a toolkit header\n",
            }.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
project(sdk_headers LANGUAGES NONE)
set(CMAKE_INSTALL_INCLUDEDIR include)
include("{helper.as_posix()}")
add_library(provider INTERFACE IMPORTED)
set_property(TARGET provider PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:$<BUILD_INTERFACE:${{CMAKE_SOURCE_DIR}}/provider/generated>>;$<BUILD_INTERFACE:${{CMAKE_SOURCE_DIR}}/provider/include>;$<INSTALL_INTERFACE:include>")
add_library(toolkit INTERFACE)
target_link_libraries(toolkit INTERFACE provider)
add_library(TGUI::TGUI ALIAS toolkit)
cheryl_sdk_native_headers(TGUI::TGUI TGUI TGUI.hpp Config.hpp)
''', encoding="utf-8")
            build = root / "build"
            result = subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja"],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run(["cmake", "--install", str(build), "--prefix", str(root / "sdk"),
                                     "--component", "CherylSDK"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            installed = root / "sdk/include"
            self.assertEqual((installed / "TGUI/TGUI.hpp").read_text(encoding="utf-8"), "selected native API\n")
            self.assertEqual((installed / "TGUI/Config.hpp").read_text(encoding="utf-8"), "generated native configuration\n")
            self.assertFalse((installed / "Unrelated").exists())

    def test_sdk_packaged_closure(self):
        template = Path(__file__).resolve().parents[2] / "cmake/templates/CherylConfig.cmake.in"
        for linkage, hwloc in (("static", True), ("static", False), ("shared", True)):
            with self.subTest(linkage=linkage, hwloc=hwloc), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                modules = root / "modules"
                modules.mkdir()
                (modules / "FindThreads.cmake").write_text('set(Threads_FOUND TRUE)\n', encoding="utf-8")
                native_targets = ["Freetype", "HarfBuzz", "ICU"] + (["Hwloc"] if hwloc else [])
                native_config = "".join(f"add_library(Cheryl::Native_{name} SHARED IMPORTED)\n"
                                        for name in native_targets)
                native_config = native_config.replace("\n", "\\n")
                (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.28)
project(sdk_fixture LANGUAGES NONE)
list(PREPEND CMAKE_MODULE_PATH "${{CMAKE_SOURCE_DIR}}/modules")
include(CMakePackageConfigHelpers)
set(sdk_linkage {linkage})
set(sdk_hwloc {'ON' if hwloc else 'OFF'})
set(sdk_runtime_present ON)
set(sdk_json OFF)
set(sdk_opengl OFF)
set(sdk_native_glfw OFF)
set(ICU_VERSION 78.1)
set(CMAKE_INSTALL_BINDIR bin)
set(CMAKE_INSTALL_LIBDIR lib)
configure_package_config_file("{template.as_posix()}" "${{CMAKE_SOURCE_DIR}}/sdk/lib/cmake/Cheryl/CherylConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Cheryl)
file(WRITE "${{CMAKE_SOURCE_DIR}}/sdk/lib/cmake/Cheryl/CherylTargets.cmake" "")
file(WRITE "${{CMAKE_SOURCE_DIR}}/sdk/lib/cmake/Cheryl/CherylNativeTargets.cmake" "{native_config}")
find_package(Cheryl REQUIRED CONFIG PATHS "${{CMAKE_SOURCE_DIR}}/sdk" NO_DEFAULT_PATH)
if(NOT TARGET Cheryl::Native_Freetype OR NOT TARGET Cheryl::Native_HarfBuzz OR NOT TARGET Cheryl::Native_ICU)
    message(FATAL_ERROR "The packaged native runtime targets are missing")
endif()
if({'ON' if hwloc else 'OFF'} AND NOT TARGET Cheryl::Native_Hwloc)
    message(FATAL_ERROR "Selected packaged hwloc was not loaded")
elseif(NOT {'ON' if hwloc else 'OFF'} AND TARGET Cheryl::Native_Hwloc)
    message(FATAL_ERROR "An omitted hwloc provider was discovered")
endif()
if(TARGET Freetype::Freetype OR TARGET ICU::uc OR TARGET harfbuzz::harfbuzz OR TARGET PkgConfig::CHERYL_HWLOC)
    message(FATAL_ERROR "The SDK discovered a private host dependency")
endif()
''', encoding="utf-8")
                result = subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build"), "-G", "Ninja",
                                         "-DCMAKE_DISABLE_FIND_PACKAGE_Freetype=TRUE", "-DCMAKE_DISABLE_FIND_PACKAGE_ICU=TRUE",
                                         "-DCMAKE_DISABLE_FIND_PACKAGE_harfbuzz=TRUE", "-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE"],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def check_generated_locations(self, root, prefix, *, debug, runtime, library):
        for configuration in ("Release", "Debug", "RelWithDebInfo"):
            with self.subTest(configuration=configuration):
                build = root / configuration
                result = subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                                         f"-DCMAKE_BUILD_TYPE={configuration}"], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                directory = prefix / "debug" if debug and configuration == "Debug" else prefix
                self.assertEqual(Path((build / "hwloc.txt").read_text(encoding="utf-8")), directory / runtime)
                self.assertEqual(Path((build / "hwloc-link.txt").read_text(encoding="utf-8")), directory / library)


if __name__ == "__main__":
    unittest.main()

"""Check dependency and shared environment cache rules without installing packages."""

from copy import deepcopy
import importlib.util
import json
from pathlib import Path
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

    def test_windows_operation(self):
        with patch("sys.argv", ["dependencies.py", "windows-install", "--profile", "modules"]), \
             patch.object(dependencies, "windows_install") as install:
            dependencies.main()
        install.assert_called_once_with("modules")


if __name__ == "__main__":
    unittest.main()

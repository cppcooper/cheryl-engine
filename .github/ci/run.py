"""Build or check one explicitly selected CI composition."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


def run(command, source, environment):
    print("+ " + subprocess.list2cmdline(list(map(str, command))), flush=True)
    subprocess.run(command, cwd=source, env=environment, check=True)


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--profile", choices=("headless", "modules", "consumer"), required=True)
    parser.add_argument("--phase", choices=("all", "build", "test"), default="all")
    parser.add_argument("--build-type", choices=("Release", "Debug", "RelWithDebInfo"), default="Release")
    parser.add_argument("--linkage", choices=("static", "shared"), default="static")
    parser.add_argument("--hwloc", choices=("ON", "OFF"), default="OFF")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--vcpkg-root", type=Path)
    parser.add_argument("--sdk", action="store_true")
    args = parser.parse_args(arguments)
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    source = args.source.resolve()
    build = args.build_dir.resolve()
    modules = args.profile == "modules"
    testing = args.profile != "consumer"
    environment = os.environ.copy()
    if sys.platform == "win32" and args.linkage == "shared":
        environment["PATH"] = str(build) + os.pathsep + environment.get("PATH", "")
    if testing:
        environment.setdefault("CHERYL_STB_ALLOCATION_TTF", str(source / "assets/fonts/DejaVuSans.ttf"))
    flag = lambda value: "ON" if value else "OFF"
    configure = ["cmake", "-S", source, "-B", build, "-G", "Ninja",
                 f"-DCMAKE_BUILD_TYPE={args.build_type}", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
                 f"-DCHERYL_BUILD_TESTS={flag(testing)}", "-DCHERYL_BUILD_ALL_TESTS=OFF",
                 f"-DCHERYL_BUILD_ACCEPTANCE_TESTS={flag(testing)}", "-DCHERYL_BUILD_CONSUMER_TESTS=ON",
                 "-DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF", f"-DCHERYL_BUILD_DEMO={flag(modules)}",
                 f"-DCHERYL_BUILD_FIRST_GAME={flag(modules)}",
                 f"-DCHERYL_BUILD_NATIVE_GLFW={flag(modules)}", f"-DCHERYL_BUILD_OPENGL={flag(modules)}",
                 f"-DCHERYL_BUILD_UI_TGUI={flag(modules)}", f"-DCHERYL_BUILD_UI_RMLUI={flag(modules)}",
                 f"-DCHERYL_BUILD_AUDIO_MINIAUDIO={flag(modules)}",
                 f"-DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC={args.hwloc}",
                 f"-DCHERYL_BUILD_WORKER_TOPOLOGY_JSON={flag(testing)}", "-DGAINPUT_ENABLE_HID=OFF",
                 "-DGLFW_BUILD_WAYLAND=OFF", "-Dgtest_force_shared_crt=ON",
                 f"-DCHERYL_BUILD_SHARED_LIBS={flag(args.linkage == 'shared')}",
                 f"-DCHERYL_BUILD_SDK={flag(args.sdk)}", "-DCHERYL_BUILD_DOCS=OFF",
                 "-DCMAKE_INSTALL_LIBDIR=lib"]
    if modules and sys.platform == "win32":
        configure.append(f"-DPython_EXECUTABLE:FILEPATH={Path(sys.executable).resolve().as_posix()}")
    if sys.platform == "win32" and environment.get("CMAKE_CXX_COMPILER_LAUNCHER") == "ccache":
        # Keep MSVC debug information in the cached object instead of a compiler PDB.
        configure += ["-DCMAKE_POLICY_DEFAULT_CMP0141=NEW",
                      "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"]
    if args.vcpkg_root:
        features = (["hwloc"] if args.hwloc == "ON" else []) + (["native"] if modules else [])
        configure += [f"-DCMAKE_TOOLCHAIN_FILE={args.vcpkg_root.resolve() / 'scripts/buildsystems/vcpkg.cmake'}",
                      f"-DVCPKG_MANIFEST_DIR={source / '.github/ci'}", "-DVCPKG_TARGET_TRIPLET=x64-windows",
                      "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON",
                      f"-DVCPKG_MANIFEST_FEATURES={';'.join(features)}",
                      f"-DCMAKE_PROJECT_INCLUDE={source / '.github/ci/windows-hwloc.cmake'}"]
    targets = ["consumer-cengine", "consumer-headers-cheryl"]
    if args.sdk:
        targets.append("cheryl-sdk")
    consumers = ["cheryl-consumer"]
    suites = ["tests-engine", "tests-logging", "acceptance-engine", "tests-worker-topology-json"]
    if testing:
        targets += suites + ["consumer-worker-topology"]
        if args.hwloc == "ON":
            targets.append("tests-worker-topology-composition")
            suites.append("tests-worker-topology-composition")
    if modules:
        owners = ("native-glfw", "opengl", "ui-tgui", "ui-rmlui", "audio-miniaudio")
        module_suites = [f"tests-{owner}" for owner in owners] + ["tests-ui-coexist"]
        targets += module_suites + [f"consumer-module-{owner}" for owner in owners] + ["demo", "first-game"]
        suites += module_suites
        consumers += [f"cheryl-{owner}-consumer" for owner in owners]
    if args.phase != "test":
        run(configure, source, environment)
        run(["cmake", "--build", build, "--target", *targets, "--parallel", str(args.jobs)], source, environment)
    if args.phase == "build":
        return
    if testing:
        # Owner runners only: no aggregates or native desktop/device suites.
        pattern = "^(" + "|".join(name + r"\." for name in suites) + "|consumer-worker-topology$)"
        run(["ctest", "--test-dir", build, "--parallel", str(args.jobs), "--timeout", "120",
             "--no-tests=error", "--output-on-failure", "--output-junit", build / "junit.xml", "-R", pattern], source, environment)
        cases = ET.parse(build / "junit.xml").getroot().findall(".//testcase")
        skipped = [case.attrib["name"] for case in cases if case.find("skipped") is not None]
        print(f"Completed {len(cases) - len(skipped)} checks; {len(skipped)} skipped.", flush=True)
        for name in skipped:
            print(f"Not accepted (skipped): {name}", flush=True)
    suffix = ".exe" if sys.platform == "win32" else ""
    for consumer in consumers:
        run([build / (consumer + suffix)], source, environment)


if __name__ == "__main__":
    main()

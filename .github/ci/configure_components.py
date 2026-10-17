"""Configure isolated component compositions without building source targets."""

import argparse
import os
from pathlib import Path
import subprocess


MODULES = {
    "startup-neutral": ("runtime/startup", ["-DCHERYL_STARTUP_OPENGL=OFF"]),
    "startup-opengl": ("runtime/startup", ["-DCHERYL_STARTUP_OPENGL=ON"]),
    "native": ("platform/native-glfw", []),
    "opengl": ("graphics/opengl", []),
    "tgui": ("ui/tgui", []),
    "rmlui": ("ui/rmlui", []),
    "audio": ("audio/miniaudio", []),
    "workers-json": ("runtime/workers", ["-DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF", "-DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=ON"]),
    "workers-native": ("runtime/workers", ["-DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=ON", "-DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF"]),
}


def fixture(source, destination, *, supplied):
    destination.mkdir(parents=True)
    selections = ("STARTUP", "WORKERS", "NATIVE_GLFW", "OPENGL", "UI_TGUI", "UI_RMLUI", "AUDIO_MINIAUDIO",
                  "DEBUG_TERMINAL_LINUX", "DEMO", "FIRST_GAME", "TUTORIAL_GAME", "SDK", "DOCS", "ALL_TESTS", "ACCEPTANCE_TESTS")
    lines = ['cmake_minimum_required(VERSION 3.28)', 'project(component_host LANGUAGES C CXX)',
             'set(BUILD_SHARED_LIBS ON)', 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)']
    lines += [f'set(CHERYL_BUILD_{name} OFF)' for name in selections]
    lines += [f'set(CHERYL_BUILD_TESTS {"OFF" if supplied else "ON"})',
              f'set(CHERYL_BUILD_CONSUMER_TESTS {"OFF" if supplied else "ON"})',
              f'add_subdirectory("{source.as_posix()}" engine EXCLUDE_FROM_ALL)']
    lines += ['if(TARGET Cheryl::Workers OR TARGET Cheryl::Startup OR TARGET Cheryl::NativeGLFW)',
              '  message(FATAL_ERROR "An omitted component was created")', 'endif()']
    if supplied:
        lines += ['set(CHERYL_STARTUP_OPENGL OFF)',
                  f'add_subdirectory("{source.as_posix()}/projects/modules/runtime/startup" startup EXCLUDE_FROM_ALL)',
                  'get_target_property(before cheryl_assembly INTERFACE_LINK_LIBRARIES)',
                  'if("module_startup" IN_LIST before)',
                  '  message(FATAL_ERROR "Available supplied module was selected implicitly")', 'endif()',
                  'cheryl_select_public_component(Cheryl::Startup)',
                  'cheryl_select_public_component(Cheryl::Startup)',
                  'get_target_property(after cheryl_assembly INTERFACE_LINK_LIBRARIES)',
                  'list(FILTER after EXCLUDE REGEX "^::@")',
                  'if(NOT after STREQUAL "Cheryl::Engine;module_startup")',
                  '  message(FATAL_ERROR "Supplied assembly selection is incorrect: ${after}")', 'endif()',
                  'if(NOT BUILD_SHARED_LIBS OR CMAKE_POSITION_INDEPENDENT_CODE)',
                  '  message(FATAL_ERROR "Component configuration changed host settings")', 'endif()']
    else:
        lines += ['if(NOT CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC OR NOT CHERYL_BUILD_WORKER_TOPOLOGY_JSON)',
                  '  message(FATAL_ERROR "Legacy provider defaults were unexpectedly rewritten")', 'endif()']
    (destination / "CMakeLists.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-root", type=Path, default=Path("build"))
    parser.add_argument("--vcpkg-root", type=Path, required=True)
    parser.add_argument("--installed-dir", type=Path, required=True)
    parser.add_argument("--triplet", default="x64-windows" if os.name == "nt" else "x64-linux-dynamic")
    parser.add_argument("--case", action="append", choices=["engine-only", "supplied", *MODULES])
    args = parser.parse_args(arguments)
    source = args.source.resolve()
    build_root = args.build_root.resolve()
    for name in args.case or ["engine-only", *MODULES, "supplied"]:
        build = build_root / ("sdk-config-" + name)
        if build.exists():
            raise ValueError(f"Use a fresh matrix output directory; preserving existing files: {build}")
        if name in {"engine-only", "supplied"}:
            entrypoint = build_root / ("sdk-config-" + name + "-source")
            fixture(source, entrypoint, supplied=name == "supplied")
            options = []
        else:
            directory, options = MODULES[name]
            entrypoint = source / "projects/modules" / directory
        command = ["cmake", "-S", str(entrypoint), "-B", str(build), "-G", "Ninja",
                   "-DCMAKE_BUILD_TYPE=Release", "-DCHERYL_BUILD_SHARED_LIBS=ON",
                   f"-DCMAKE_TOOLCHAIN_FILE={args.vcpkg_root.resolve() / 'scripts/buildsystems/vcpkg.cmake'}",
                   f"-DVCPKG_INSTALLED_DIR={args.installed_dir.resolve()}", f"-DVCPKG_TARGET_TRIPLET={args.triplet}",
                   f"-DVCPKG_MANIFEST_DIR={source / '.github/ci'}", "-DVCPKG_MANIFEST_INSTALL=OFF",
                   f"-DCMAKE_PROJECT_INCLUDE={source / 'tools/dependencies/prepared.cmake'}",
                   "-DCHERYL_BUILD_TESTS=OFF", "-DCHERYL_BUILD_CONSUMER_TESTS=OFF", "-DCHERYL_BUILD_SDK=OFF", *options]
        if name in {"native", "opengl", "startup-opengl"}:
            command += ["-DGAINPUT_ENABLE_HID=OFF", "-DGLFW_BUILD_WAYLAND=OFF"]
        if name in {"opengl", "startup-opengl"}:
            command.append(f"-DCHERYL_NATIVE_GLFW_SOURCE={source / 'projects/modules/platform/native-glfw'}")
        if name == "startup-opengl":
            command.append(f"-DCHERYL_OPENGL_SOURCE={source / 'projects/modules/graphics/opengl'}")
        print("+ " + subprocess.list2cmdline(command), flush=True)
        build.mkdir(parents=True)
        result = subprocess.run(command, text=True, capture_output=True)
        (build / "configure.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            print(result.stdout + result.stderr, flush=True)
            result.check_returncode()
        if name in {"tgui", "rmlui"}:
            # Upstream cache initializers must preserve the owned font-service
            # configuration after normal variable scopes disappear.
            print("+ " + subprocess.list2cmdline(command) + " (reconfigure)", flush=True)
            result = subprocess.run(command, text=True, capture_output=True)
            (build / "reconfigure.log").write_text(result.stdout + result.stderr, encoding="utf-8")
            if result.returncode:
                print(result.stdout + result.stderr, flush=True)
                result.check_returncode()
        print(f"Configured {name}; no source targets built.", flush=True)


if __name__ == "__main__":
    main()

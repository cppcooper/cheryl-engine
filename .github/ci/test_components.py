"""Exercise component selection and dependency ownership without building the engine."""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ComponentTests(unittest.TestCase):
    def configure(self, body):
        with tempfile.TemporaryDirectory(prefix="cheryl-components-") as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.28)\nproject(probe LANGUAGES NONE)\n'
                f'include("{ROOT.as_posix()}/cmake/CherylLibraries.cmake")\n'
                f'include("{ROOT.as_posix()}/cmake/CherylPublicHeaders.cmake")\n'
                + body, encoding="utf-8")
            return subprocess.run(["cmake", "-S", str(root), "-B", str(root / "out"), "-G", "Ninja"],
                                  text=True, capture_output=True, check=False)

    def test_runtime_copies(self):
        result = self.configure(f'''
include("{ROOT.as_posix()}/cmake/CherylRuntimeDependencies.cmake")
file(WRITE "${{CMAKE_BINARY_DIR}}/first/freetype.dll" "same runtime")
file(WRITE "${{CMAKE_BINARY_DIR}}/second/freetype.dll" "same runtime")
cheryl_runtime_dependency_location(selected freetype.dll
  "${{CMAKE_BINARY_DIR}}/first/freetype.dll" "${{CMAKE_BINARY_DIR}}/second/freetype.dll")
if(NOT selected STREQUAL "${{CMAKE_BINARY_DIR}}/first/freetype.dll")
  message(FATAL_ERROR "Identical DLL copies were not resolved to the first location")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_runtime_conflict(self):
        result = self.configure(f'''
include("{ROOT.as_posix()}/cmake/CherylRuntimeDependencies.cmake")
file(WRITE "${{CMAKE_BINARY_DIR}}/first/freetype.dll" "one runtime")
file(WRITE "${{CMAKE_BINARY_DIR}}/second/freetype.dll" "another runtime")
cheryl_runtime_dependency_location(selected freetype.dll
  "${{CMAKE_BINARY_DIR}}/first/freetype.dll" "${{CMAKE_BINARY_DIR}}/second/freetype.dll")
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Native DLL dependencies have conflicting locations: freetype.dll", result.stderr)

    def runtime_classification(self, windows, excluded, bundled):
        expected_system = ";".join(excluded)
        expected_payload = ";".join(bundled)
        return self.configure(f'''
set(WIN32 {"TRUE" if windows else "FALSE"})
set(ENV{{SystemRoot}} "${{CMAKE_BINARY_DIR}}/Win.dows+[Host]")
include("{ROOT.as_posix()}/cmake/CherylRuntimeDependencies.cmake")
function(file operation)
  if(NOT operation STREQUAL "GET_RUNTIME_DEPENDENCIES")
    if(operation STREQUAL "TO_CMAKE_PATH")
      _file(TO_CMAKE_PATH "${{ARGV1}}" converted)
      set(${{ARGV2}} "${{converted}}" PARENT_SCOPE)
    else()
      _file(${{ARGV}})
    endif()
    return()
  endif()
  cmake_parse_arguments(probe "" "RESOLVED_DEPENDENCIES_VAR;UNRESOLVED_DEPENDENCIES_VAR;CONFLICTING_DEPENDENCIES_PREFIX"
    "LIBRARIES;DIRECTORIES;PRE_EXCLUDE_REGEXES;POST_EXCLUDE_REGEXES" ${{ARGN}})
  set(system_names "{expected_system}")
  set(payload_names "{expected_payload}")
  foreach(name IN LISTS system_names payload_names)
    set(excluded FALSE)
    foreach(pattern IN LISTS probe_PRE_EXCLUDE_REGEXES)
      if(name MATCHES "${{pattern}}")
        set(excluded TRUE)
      endif()
    endforeach()
    if(name IN_LIST system_names AND NOT excluded)
      message(FATAL_ERROR "OS runtime would be packaged: ${{name}}")
    elseif(name IN_LIST payload_names AND excluded)
      message(FATAL_ERROR "Third-party runtime would be omitted: ${{name}}")
    endif()
  endforeach()
  if(WIN32)
    file(TO_CMAKE_PATH "$ENV{{SystemRoot}}" system_root)
    set(host_paths "${{system_root}}/System32/SHELL32.dll" "${{system_root}}/kernel32.dll")
    string(TOUPPER "${{system_root}}/System32/USER32.dll" alternate_case)
    list(APPEND host_paths "${{alternate_case}}")
    set(native_paths "${{system_root}}-private/freetype.dll" "${{CMAKE_BINARY_DIR}}/native/harfbuzz.dll")
    foreach(library IN LISTS host_paths native_paths)
      set(excluded FALSE)
      foreach(pattern IN LISTS probe_POST_EXCLUDE_REGEXES)
        if(library MATCHES "${{pattern}}")
          set(excluded TRUE)
        endif()
      endforeach()
      if(library IN_LIST host_paths AND NOT excluded)
        message(FATAL_ERROR "OS imports would be scanned recursively: ${{library}}")
      elseif(library IN_LIST native_paths AND excluded)
        message(FATAL_ERROR "Third-party runtime path would be omitted: ${{library}}")
      endif()
    endforeach()
  elseif(probe_POST_EXCLUDE_REGEXES)
    message(FATAL_ERROR "Windows path exclusions leaked into Linux runtime scanning")
  endif()
  set(${{probe_RESOLVED_DEPENDENCIES_VAR}} "${{payload_names}}" PARENT_SCOPE)
  set(${{probe_UNRESOLVED_DEPENDENCIES_VAR}} "" PARENT_SCOPE)
  set(${{probe_CONFLICTING_DEPENDENCIES_PREFIX}}_FILENAMES "" PARENT_SCOPE)
endfunction()
cheryl_runtime_dependencies(actual seed-runtime)
if(NOT actual STREQUAL "seed-runtime;{expected_payload}")
  message(FATAL_ERROR "Native closure was lost: ${{actual}}")
endif()
''')

    def test_linux_runtime_baseline(self):
        result = self.runtime_classification(False,
            ["ld-linux-x86-64.so.2", "libc.so.6", "libmvec.so.1", "libstdc++.so.6", "libgcc_s.so.1",
             "libnss_files.so.2", "libX11.so.6", "libxcb.so.1", "libwayland-client.so.0", "libxkbcommon.so.0",
             "libGLX.so.0", "libOpenGL.so.0", "libEGL.so.1", "libdrm_amdgpu.so.1", "libgbm.so.1",
             "libasound.so.2", "libpulse-simple.so.0", "libjack.so.0", "libudev.so.1"],
            ["libicuuc.so.78", "libicui18n.so.78", "libicudata.so.78", "libfreetype.so.6", "libharfbuzz.so.0",
             "libhwloc.so.15", "libdw.so.1", "libelf.so.1", "libbrotlidec.so.1", "libglib-2.0.so.0",
             "libgraphite2.so.3", "libpcre2-8.so.0", "libz.so.1", "libzstd.so.1", "libusb-1.0.so.0"])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_windows_runtime_baseline(self):
        result = self.runtime_classification(True,
            ["icu.dll", "ICU.DLL", "opengl32.dll", "api-ms-win-crt-runtime-l1-1-0.dll",
             "ext-ms-win-ntuser-window-l1-1-0.dll", "VCRUNTIME140_1.dll", "MSVCP140.dll", "ucrtbase.dll"],
            ["icuuc78.dll", "freetype.dll", "harfbuzz.dll", "hwloc-15.dll", "hidapi.dll", "libpng16.dll"])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_runtime_empty(self):
        result = self.configure(f'''
include("{ROOT.as_posix()}/cmake/CherylRuntimeDependencies.cmake")
function(file operation)
  message(FATAL_ERROR "Empty runtime closure should not inspect artifacts")
endfunction()
cheryl_runtime_dependencies(actual)
if(actual)
  message(FATAL_ERROR "Empty runtime closure returned artifacts")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_runtime_missing(self):
        result = self.configure(f'''
include("{ROOT.as_posix()}/cmake/CherylRuntimeDependencies.cmake")
function(file operation)
  if(operation STREQUAL "TO_CMAKE_PATH")
    _file(TO_CMAKE_PATH "${{ARGV1}}" converted)
    set(${{ARGV2}} "${{converted}}" PARENT_SCOPE)
    return()
  endif()
  cmake_parse_arguments(probe "" "RESOLVED_DEPENDENCIES_VAR;UNRESOLVED_DEPENDENCIES_VAR"
    "LIBRARIES;DIRECTORIES;PRE_EXCLUDE_REGEXES;POST_EXCLUDE_REGEXES;CONFLICTING_DEPENDENCIES_PREFIX" ${{ARGN}})
  set(${{probe_RESOLVED_DEPENDENCIES_VAR}} "" PARENT_SCOPE)
  set(${{probe_UNRESOLVED_DEPENDENCIES_VAR}} "libmissing.so.1" PARENT_SCOPE)
endfunction()
cheryl_runtime_dependencies(actual seed-runtime)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot resolve required native runtime dependencies: libmissing.so.1", result.stderr)

    def runtime_install_probe(self, soname="libfixture.so.2", conflict=False):
        return self.configure(f'''
set(WIN32 FALSE)
include("{ROOT.as_posix()}/cmake/CherylRuntimeDependencies.cmake")
# This exercises Linux staging even when the host uses Windows drive paths.
set(CMAKE_INSTALL_PREFIX "/prefix")
set(ENV{{DESTDIR}} "${{CMAKE_BINARY_DIR}}/stage")
set(staged "$ENV{{DESTDIR}}${{CMAKE_INSTALL_PREFIX}}")
file(WRITE "${{CMAKE_BINARY_DIR}}/source/libfixture.so.2.7" "runtime bytes")
if({"TRUE" if conflict else "FALSE"})
  file(WRITE "${{staged}}/lib/libfixture.so.2" "conflicting bytes")
endif()
function(cheryl_runtime_dependencies result)
  set(${{result}} "${{ARGN}}" PARENT_SCOPE)
endfunction()
set(CHERYL_PATCHELF_EXECUTABLE "${{CMAKE_COMMAND}}")
set(CHERYL_NOTICE_PYTHON "${{CMAKE_COMMAND}}")
function(execute_process)
  cmake_parse_arguments(probe "" "RESULT_VARIABLE;OUTPUT_VARIABLE;ERROR_VARIABLE" "COMMAND" ${{ARGV}})
  if("--print-soname" IN_LIST probe_COMMAND)
    set(${{probe_OUTPUT_VARIABLE}} "{soname}" PARENT_SCOPE)
  elseif("--set-rpath" IN_LIST probe_COMMAND)
    list(GET probe_COMMAND -1 artifact)
    if(NOT artifact STREQUAL "${{staged}}/lib/libfixture.so.2.7" OR NOT EXISTS "${{artifact}}")
      message(FATAL_ERROR "Runtime patch missed DESTDIR: ${{artifact}}")
    endif()
    set_property(GLOBAL APPEND PROPERTY patched "${{artifact}}")
  elseif("--destination" IN_LIST probe_COMMAND)
    list(FIND probe_COMMAND "--destination" index)
    math(EXPR index "${{index}} + 1")
    list(GET probe_COMMAND ${{index}} notice_destination)
    if(NOT notice_destination STREQUAL "${{staged}}/share/licenses/native")
      message(FATAL_ERROR "Native notices missed DESTDIR: ${{notice_destination}}")
    endif()
    set_property(GLOBAL PROPERTY notices_collected TRUE)
  else()
    message(FATAL_ERROR "Unexpected external command")
  endif()
  set(${{probe_RESULT_VARIABLE}} 0 PARENT_SCOPE)
endfunction()
cheryl_sdk_install_runtimes("${{CMAKE_INSTALL_PREFIX}}/lib" "${{CMAKE_BINARY_DIR}}/source/libfixture.so.2.7")
get_property(patched GLOBAL PROPERTY patched)
get_property(notices_collected GLOBAL PROPERTY notices_collected)
if(NOT patched STREQUAL "${{staged}}/lib/libfixture.so.2.7" OR NOT notices_collected)
  message(FATAL_ERROR "Runtime staging did not patch and collect exactly once")
endif()
file(READ_SYMLINK "${{staged}}/lib/libfixture.so.2" soname_target)
if(NOT soname_target STREQUAL "libfixture.so.2.7")
  message(FATAL_ERROR "Versioned runtime did not get a relative SONAME alias")
endif()
''')

    def test_runtime_destdir(self):
        result = self.runtime_install_probe()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_runtime_soname_conflict(self):
        result = self.runtime_install_probe(conflict=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Conflicting SDK runtime SONAME: libfixture.so.2", result.stderr)

    def test_runtime_soname_path(self):
        result = self.runtime_install_probe(soname="../escape.so")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsafe SDK runtime SONAME: ../escape.so", result.stderr)

    def test_windows_runtime_install(self):
        result = self.configure(f'''
set(WIN32 TRUE)
unset(ENV{{DESTDIR}})
include("{ROOT.as_posix()}/cmake/CherylRuntimeDependencies.cmake")
set(CMAKE_INSTALL_PREFIX "${{CMAKE_BINARY_DIR}}/prefix")
file(WRITE "${{CMAKE_BINARY_DIR}}/source/fixture.dll" "runtime bytes")
function(cheryl_runtime_dependencies result)
  set(${{result}} "${{ARGN}}" PARENT_SCOPE)
endfunction()
function(execute_process)
  message(FATAL_ERROR "Windows runtime installation invoked a Linux tool")
endfunction()
cheryl_sdk_install_runtimes("${{CMAKE_INSTALL_PREFIX}}/bin" "${{CMAKE_BINARY_DIR}}/source/fixture.dll")
file(READ "${{CMAKE_INSTALL_PREFIX}}/bin/fixture.dll" installed)
if(NOT installed STREQUAL "runtime bytes")
  message(FATAL_ERROR "The Windows runtime was not installed intact")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_shared_runtime(self):
        result = self.configure('''
add_library(first INTERFACE)
add_library(second INTERFACE)
add_library(runtime SHARED IMPORTED)
set_target_properties(runtime PROPERTIES IMPORTED_LOCATION "/unused/libruntime.so")
cheryl_shared_dependency(runtime OWNER first)
cheryl_shared_dependency(runtime OWNER second)
cheryl_shared_dependency(runtime OWNER first)
get_property(declarations GLOBAL PROPERTY CHERYL_SHARED_DEPENDENCIES)
if(NOT declarations STREQUAL "runtime|first;runtime|second")
  message(FATAL_ERROR "Shared runtime consumers were lost or repeated: ${declarations}")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_shared_rejects_archive(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(archive STATIC IMPORTED)
cheryl_shared_dependency(archive OWNER owner)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires a shared implementation", result.stderr)

    def test_shared_wrapper(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(runtime UNKNOWN IMPORTED)
set_target_properties(runtime PROPERTIES IMPORTED_LOCATION "/unused/libruntime.so.2")
add_library(wrapper INTERFACE IMPORTED)
set_target_properties(wrapper PROPERTIES INTERFACE_LINK_LIBRARIES runtime)
cheryl_shared_dependency(wrapper OWNER owner)
get_property(declarations GLOBAL PROPERTY CHERYL_SHARED_DEPENDENCIES)
if(NOT declarations STREQUAL "runtime|owner")
  message(FATAL_ERROR "The wrapper did not resolve to its runtime: ${declarations}")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_embedded_uses_shared(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(runtime SHARED IMPORTED)
set_target_properties(runtime PROPERTIES IMPORTED_LOCATION "/unused/libruntime.so")
add_library(archive STATIC IMPORTED)
set_target_properties(archive PROPERTIES IMPORTED_LOCATION "/unused/libarchive.a"
  INTERFACE_LINK_LIBRARIES runtime)
cheryl_shared_dependency(runtime OWNER owner)
cheryl_private_dependency(archive OWNER owner)
get_property(embedded GLOBAL PROPERTY CHERYL_PRIVATE_DEPENDENCIES)
if(NOT embedded STREQUAL "archive|owner")
  message(FATAL_ERROR "Shared runtime was treated as an embedded archive: ${embedded}")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_system_interface(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(system_api INTERFACE IMPORTED)
set_target_properties(system_api PROPERTIES INTERFACE_LINK_LIBRARIES system_api_import)
cheryl_shared_dependency(system_api OWNER owner SYSTEM)
get_target_property(system system_api CHERYL_SHARED_DEPENDENCY_SYSTEM)
if(NOT system)
  message(FATAL_ERROR "The operating-system dependency was not identified")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_selection(self):
        result = self.configure('''
add_library(cengine INTERFACE)
add_library(Cheryl::Engine ALIAS cengine)
cheryl_register_public_component(cengine)
add_library(optional INTERFACE)
cheryl_register_public_component(optional FEATURE CHERYL_HAS_OPTIONAL)
get_target_property(before cheryl_assembly INTERFACE_LINK_LIBRARIES)
if("optional" IN_LIST before)
  message(FATAL_ERROR "Availability selected the optional component")
endif()
cheryl_select_public_component(optional)
cheryl_select_public_component(optional)
get_target_property(after cheryl_assembly INTERFACE_LINK_LIBRARIES)
if(NOT after STREQUAL "Cheryl::Engine;optional")
  message(FATAL_ERROR "Selection was omitted or repeated: ${after}")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_shared_rejected(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(dependency SHARED IMPORTED)
cheryl_private_dependency(dependency OWNER owner)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires a static dependency", result.stderr)

    def test_owner_collision(self):
        result = self.configure('''
add_library(first INTERFACE)
add_library(second INTERFACE)
add_library(dependency STATIC IMPORTED)
add_library(alias ALIAS dependency)
cheryl_private_dependency(dependency OWNER first)
cheryl_private_dependency(alias OWNER second)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already belongs to first", result.stderr)

    def test_imported_archive(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(dependency UNKNOWN IMPORTED)
set_target_properties(dependency PROPERTIES IMPORTED_LOCATION "/unused/libdependency.a")
cheryl_private_dependency(dependency OWNER owner)
get_target_property(pic dependency POSITION_INDEPENDENT_CODE)
if(pic)
  message(FATAL_ERROR "Supplied dependency settings were changed")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_interface_owner(self):
        result = self.configure('''
add_library(first INTERFACE)
add_library(second INTERFACE)
add_library(archive STATIC IMPORTED)
add_library(wrapper INTERFACE IMPORTED)
set_target_properties(wrapper PROPERTIES INTERFACE_LINK_LIBRARIES archive)
cheryl_private_dependency(wrapper OWNER first)
cheryl_private_dependency(archive OWNER second)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already belongs to first", result.stderr)

    def test_archive_aliases(self):
        result = self.configure('''
add_library(first INTERFACE)
add_library(second INTERFACE)
add_library(a STATIC IMPORTED)
add_library(b UNKNOWN IMPORTED)
set_target_properties(a b PROPERTIES IMPORTED_LOCATION "/unused/libimplementation.a")
cheryl_private_dependency(a OWNER first)
cheryl_private_dependency(b OWNER second)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already belongs to first", result.stderr)

    def test_public_wrapper(self):
        result = self.configure('''
set(CMAKE_SYSTEM_NAME Linux)
add_library(owner SHARED IMPORTED)
add_library(archive STATIC IMPORTED)
add_library(wrapper INTERFACE IMPORTED)
set_target_properties(wrapper PROPERTIES INTERFACE_LINK_LIBRARIES archive)
cheryl_private_dependency(wrapper OWNER owner PUBLIC_API)
string(SHA256 identity archive)
get_property(exported GLOBAL PROPERTY "CHERYL_DEPENDENCY_PUBLIC_API_${identity}")
if(NOT exported)
  message(FATAL_ERROR "The wrapper lost its native authoring exports")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_objects_rejected(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(dependency OBJECT IMPORTED)
cheryl_private_dependency(dependency OWNER owner)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("OBJECT_LIBRARY dependency is not supported", " ".join(result.stderr.split()))

    def test_transitive_shared(self):
        for kind in ("STATIC", "UNKNOWN"):
            with self.subTest(kind=kind):
                result = self.configure(f'''
add_library(owner INTERFACE)
add_library(root {kind} IMPORTED)
set_target_properties(root PROPERTIES IMPORTED_LOCATION "/unused/libroot.a"
  INTERFACE_LINK_LIBRARIES "$<LINK_ONLY:$<BUILD_INTERFACE:$<TARGET_NAME_IF_EXISTS:child>>>")
add_library(child SHARED IMPORTED)
cheryl_private_dependency(root OWNER owner)
''')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("requires a static child", result.stderr)

    def test_transitive_owner(self):
        result = self.configure('''
add_library(first INTERFACE)
add_library(second INTERFACE)
add_library(root STATIC IMPORTED)
add_library(child STATIC IMPORTED)
set_target_properties(root PROPERTIES INTERFACE_LINK_LIBRARIES "$<LINK_ONLY:child>")
cheryl_private_dependency(root OWNER first)
cheryl_private_dependency(child OWNER second)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already belongs to first", result.stderr)

    def test_conditional_shared(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(root STATIC IMPORTED)
add_library(child SHARED IMPORTED)
set_target_properties(root PROPERTIES INTERFACE_LINK_LIBRARIES "$<$<CONFIG:Debug>:child>")
cheryl_private_dependency(root OWNER owner)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires a static child", result.stderr)

    def test_cycle_and_headers(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(root STATIC IMPORTED)
add_library(child STATIC IMPORTED)
add_library(headers INTERFACE IMPORTED)
set_target_properties(root PROPERTIES INTERFACE_LINK_LIBRARIES "child;headers")
set_target_properties(child PROPERTIES INTERFACE_LINK_LIBRARIES root)
set_target_properties(headers PROPERTIES INTERFACE_LINK_LIBRARIES "$<TARGET_NAME_IF_EXISTS:absent>")
cheryl_private_dependency(root OWNER owner)
get_property(dependencies GLOBAL PROPERTY CHERYL_PRIVATE_DEPENDENCIES)
list(LENGTH dependencies count)
if(NOT count EQUAL 2)
  message(FATAL_ERROR "The cycle lost or duplicated an implementation: ${dependencies}")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_empty_wrapper(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(headers INTERFACE IMPORTED)
cheryl_private_dependency(headers OWNER owner)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot establish a static implementation behind headers", result.stderr)

    def test_public_and_system_links(self):
        result = self.configure('''
add_library(cengine SHARED IMPORTED)
add_library(Cheryl::Engine ALIAS cengine)
cheryl_register_public_component(cengine)
add_library(Threads::Threads INTERFACE IMPORTED)
add_library(X11::X11 SHARED IMPORTED)
add_library(OpenGL::GL SHARED IMPORTED)
add_library(owner INTERFACE)
add_library(root STATIC IMPORTED)
set_target_properties(root PROPERTIES INTERFACE_LINK_LIBRARIES
  "Cheryl::Engine;Threads::Threads;X11::X11;OpenGL::GL;dl;pthread;-Wl,--as-needed;/unused/libsystem.so")
cheryl_private_dependency(root OWNER owner)
get_property(dependencies GLOBAL PROPERTY CHERYL_PRIVATE_DEPENDENCIES)
if(NOT dependencies STREQUAL "root|owner")
  message(FATAL_ERROR "Public or system libraries were given a private owner: ${dependencies}")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_raw_archive(self):
        result = self.configure('''
add_library(owner INTERFACE)
add_library(root STATIC IMPORTED)
set_target_properties(root PROPERTIES INTERFACE_LINK_LIBRARIES "$<LINK_ONLY:/unused/libchild.a>")
cheryl_private_dependency(root OWNER owner)
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("links unidentified archive /unused/libchild.a", result.stderr)
        self.assertIn("Supply an imported STATIC or UNKNOWN target", " ".join(result.stderr.split()))

    def test_system_archive(self):
        result = self.configure('''
set(UNIX TRUE)
set(CMAKE_SYSROOT /target-sdk)
add_library(owner INTERFACE)
add_library(root STATIC IMPORTED)
set_target_properties(root PROPERTIES INTERFACE_LINK_LIBRARIES
  "/usr/lib/librt.a;/lib64/libdl.a;/target-sdk/usr/lib/libpthread.a")
cheryl_private_dependency(root OWNER owner)
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for archive in ("/private/lib/librt.a", "/usr/lib/libfreetype.a", "/usr/library/librt.a", "/private/child.Lib"):
            with self.subTest(archive=archive):
                result = self.configure(f'''
set(UNIX TRUE)
add_library(owner INTERFACE)
add_library(root STATIC IMPORTED)
set_target_properties(root PROPERTIES INTERFACE_LINK_LIBRARIES "{archive}")
cheryl_private_dependency(root OWNER owner)
''')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("links unidentified archive", result.stderr)

    def test_windows_system_archive(self):
        result = self.configure('''
set(UNIX FALSE)
set(WIN32 TRUE)
cheryl_is_system_archive(sdk "/SDK/Windows Kits/10/Lib/10.0.26100.0/um/x64/User32.Lib")
cheryl_is_system_archive(private "/private/User32.Lib")
cheryl_is_system_archive(unknown "/SDK/Windows Kits/10/Lib/10.0.26100.0/um/x64/freetype.lib")
cheryl_is_system_archive(escaped "/SDK/Windows Kits/10/Lib/../../../private/user32.lib")
if(NOT sdk OR private OR unknown OR escaped)
  message(FATAL_ERROR "Windows SDK exception was missing or too broad")
endif()
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def toolkit_fixture(self, directory, toolkit):
        root = Path(directory)
        upstream = root / "upstream"
        upstream.mkdir()
        if toolkit == "rmlui":
            body = '''
project(RmlUi VERSION 6.3 LANGUAGES NONE)
set(RMLUI_FONT_ENGINE freetype CACHE STRING "Font engine")
add_library(rmlui_core SHARED)
'''
            font_source = upstream / "Source/Core/FontEngineDefault"
            font_source.mkdir(parents=True)
            for source in ("FontEngineInterfaceDefault", "FontFace", "FontFaceHandleDefault", "FontFaceLayer", "FontFamily", "FontProvider"):
                (font_source / f"{source}.cpp").write_text("", encoding="utf-8")
            expected = {"RMLUI_FONT_ENGINE": "none"}
            setup = '''
function(cheryl_fix_rmlui_placeholder)
endfunction()
'''
            helper = "RmlUi"
        else:
            body = f'''
include("{ROOT.as_posix()}/extern/tgui/cmake/Macros.cmake")
tgui_set_option(TGUI_BACKEND SFML_GRAPHICS STRING "Backend")
tgui_set_option(TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE FALSE BOOL "FreeType")
set(TGUI_VERSION 1.13.0)
add_library(tgui SHARED)
set_property(TARGET tgui PROPERTY SOURCES "${{CMAKE_CURRENT_SOURCE_DIR}}/Loading/ImageLoader.cpp")
'''
            expected = {"TGUI_BACKEND": "Custom", "TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE": "OFF"}
            setup = ""
            helper = "TGUI"
        (upstream / "CMakeLists.txt").write_text(body, encoding="utf-8")
        checks = "\n".join(
            f'if(NOT "$CACHE{{{name}}}" STREQUAL "{value}")\n'
            f'  message(FATAL_ERROR "Owned {name} cache drifted: $CACHE{{{name}}}")\nendif()'
            for name, value in expected.items())
        (root / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.28)
project(toolkit_probe LANGUAGES CXX)
set(CHERYL_BUILD_SHARED_LIBS OFF)
set(CHERYL_{toolkit.upper()}_SOURCE "{upstream.as_posix()}")
include("{ROOT.as_posix()}/projects/modules/ui/{toolkit}/cmake/{helper}.cmake")
{setup}
cheryl_add_{toolkit}_source()
{checks}
''', encoding="utf-8")
        return ["cmake", "-S", str(root), "-B", str(root / "out"), "-G", "Ninja"]

    def test_toolkit_reconfigure(self):
        for toolkit in ("rmlui", "tgui"):
            with self.subTest(toolkit=toolkit), tempfile.TemporaryDirectory(prefix="cheryl-toolkit-") as directory:
                command = self.toolkit_fixture(directory, toolkit)
                for iteration in range(2):
                    result = subprocess.run(command, text=True, capture_output=True, check=False)
                    self.assertEqual(result.returncode, 0, f"Configure {iteration + 1}:\n" + result.stdout + result.stderr)

    def test_toolkit_cache_preserved(self):
        incompatible = (("rmlui", "RMLUI_FONT_ENGINE", "freetype"),
                        ("tgui", "TGUI_BACKEND", "SFML_GRAPHICS"),
                        ("tgui", "TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE", "ON"))
        for toolkit, name, value in incompatible:
            with self.subTest(option=name), tempfile.TemporaryDirectory(prefix="cheryl-toolkit-") as directory:
                command = self.toolkit_fixture(directory, toolkit)
                result = subprocess.run([*command, f"-D{name}:STRING={value}"], text=True, capture_output=True, check=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Cheryl UI needs", result.stderr)
                cache = (Path(directory) / "out/CMakeCache.txt").read_text(encoding="utf-8")
                self.assertIn(f"{name}:STRING={value}\n", cache)


if __name__ == "__main__":
    unittest.main()

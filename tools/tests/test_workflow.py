"""Verify workflow generation, tool invocation and build-specific clangd caches."""

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).parents[1]))
import setup_workflow
import workflow


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="mainverte workflow ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        options = setup_workflow.parser().parse_args([])
        for name in ("qt_root", "tools_bin", "sdk", "ndk", "jdk", "llvm"):
            setattr(options, name, (self.root / name).as_posix())
        options.clazy = (self.root / "clazy.exe").as_posix()
        required = [
            "qt_root/6.12.0/mingw_64/bin/qmake.exe",
            *(f"qt_root/Tools/mingw1310_64/bin/{name}.exe" for name in ("gcc", "g++", "gdb")),
            "qt_root/Tools/Ninja/ninja.exe", "tools_bin/cmake.exe", "tools_bin/ctest.exe",
            "sdk/platform-tools/adb.exe", "ndk/build/cmake/android.toolchain.cmake",
            "jdk/bin/java.exe", "clazy.exe",
            *(f"llvm/{name}.exe" for name in ("clangd", "clang-tidy", "clang-format")),
            *(f"qt_root/6.12.0/{kit}/lib/cmake/Qt6/qt.toolchain.cmake"
              for kit in ("android_x86_64", "android_arm64_v8a")),
        ]
        for name in required:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        setup_workflow.generate(self.root, options)

    def read(self, name):
        return json.loads((self.root / name).read_text(encoding="utf-8-sig"))

    def binary(self, name):
        preset = next(item for item in self.read("CMakeUserPresets.json")["configurePresets"]
                      if item["name"] == name)
        binary = Path(preset["binaryDir"].replace("${sourceDir}", self.root.as_posix()))
        binary.mkdir(parents=True, exist_ok=True)
        (binary / "compile_commands.json").write_text("[]")
        return binary

    def test_generated_presets_and_python_tasks_match_application(self):
        app_cmake = (Path(__file__).parents[2] / "app_qt/CMakeLists.txt").read_text()
        app_target = re.search(r"qt_add_executable\(\s*(\w+)", app_cmake).group(1)
        presets = self.read("CMakeUserPresets.json")
        settings = self.read(".vscode/settings.json")
        machine = self.read(".vscode/mainverte-tools.json")
        self.assertEqual(settings["cmake.cmakePath"], machine["cmake"])
        self.assertEqual(settings["cmake.ctestPath"], machine["ctest"])
        self.assertTrue(settings["cmake.automaticReconfigure"])
        self.assertEqual(settings["cmake.copyCompileCommands"], "${workspaceFolder}/compile_commands.json")
        self.assertEqual(settings["clangd.onConfigChanged"], "restart")
        self.assertTrue(settings["clangd.onConfigChangedForceEnable"])
        self.assertFalse(any(arg.startswith("--compile-commands-dir") for arg in settings["clangd.arguments"]))
        configurations = presets["configurePresets"]
        self.assertEqual(len(configurations), 6)
        self.assertEqual(len({preset["binaryDir"] for preset in configurations}), 6)
        self.assertEqual(len(presets["testPresets"]), 3)
        for preset in configurations:
            self.assertEqual(preset["cacheVariables"]["CMAKE_PROJECT_TOP_LEVEL_INCLUDES"], "${sourceDir}/tools/clangd.cmake")
            self.assertEqual(preset["cacheVariables"]["BUILD_TESTING"], preset["name"].startswith("desktop-"))
        for build in presets["buildPresets"]:
            if not build["name"].endswith("-tests"):
                self.assertEqual(build["targets"], [app_target])
        tasks = self.read(".vscode/tasks.json")["tasks"]
        for task in tasks:
            self.assertEqual(task["command"], sys.executable)
            self.assertEqual(task["args"][0], "${workspaceFolder}/tools/workflow.py")
        launches = self.read(".vscode/launch.json")["configurations"]
        self.assertEqual(len(launches), 6)
        for launch in launches:
            self.assertIn(launch["preLaunchTask"], {task["label"] for task in tasks})
            if launch["type"] == "cppdbg":
                self.assertEqual(launch["cwd"], "${workspaceFolder}")
                self.assertTrue(launch["program"].endswith(f"/app_qt/{app_target}.exe"))
            else:
                self.assertIn("debug-sysroot/app_process64", launch["targetCreateCommands"][-1])
                self.assertIn("process handle SIGSTOP --pass false --stop false --notify false", launch["processCreateCommands"])

    def test_analysis_uses_textual_includes_for_different_clang_versions(self):
        binary = self.binary("android-x86_64-debug")
        header = binary / "cmake_pch.hxx"
        header.write_text("#include <vector>\n")
        entry = {"file": "source.cpp", "directory": str(binary),
                 "command": f'clang++ -Xclang -include-pch -Xclang "{header}.pch" -Xclang -include -Xclang "{header}" -c source.cpp'}
        (binary / "compile_commands.json").write_text(json.dumps([entry, entry]))
        analysis = workflow.analysis_database(binary)
        sanitized = json.loads((analysis / "compile_commands.json").read_text())
        self.assertEqual(len(sanitized), 2)
        self.assertNotIn("-include-pch", sanitized[0]["command"])
        self.assertNotIn("cmake_pch.hxx", sanitized[0]["command"])
        self.assertEqual((analysis / "pch-0.h").read_text(), header.read_text())

    def test_configure_only_notifies_clangd_after_success(self):
        for preset, database in (("android-x86_64-debug", [{"file": "android.cpp"}]),
                                 ("desktop-x86_64-debug", [{"file": "desktop.cpp"}])):
            binary = self.binary(preset)
            (binary / "compile_commands.json").write_text(json.dumps(database))
            with patch.object(workflow, "run"):
                workflow.execute(self.root, workflow.parser().parse_args(["--action", "configure", "--preset", preset]))
            self.assertEqual(self.read("compile_commands.json"), database)
        with patch.object(workflow, "run", side_effect=subprocess.CalledProcessError(1, ["cmake"])):
            with self.assertRaises(subprocess.CalledProcessError):
                workflow.execute(self.root, workflow.parser().parse_args(["--action", "configure"]))
        self.assertEqual(self.read("compile_commands.json"), [{"file": "desktop.cpp"}])

    def test_release_packaging_selects_renamed_target_without_device(self):
        binary = self.binary("android-arm64-release")
        app = binary / "app_qt"
        app.mkdir()
        (app / "android-appMainVerte-deployment-settings.json").touch()
        settings = app / f"android-{workflow.APP_TARGET}-deployment-settings.json"
        settings.touch()
        with patch.object(workflow, "run") as run:
            workflow.execute(self.root, workflow.parser().parse_args(["--action", "package", "--preset", "android-arm64-release"]))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertEqual(commands[-2][-1], f"{workflow.APP_TARGET}_prepare_apk_dir")
        self.assertIn(settings, commands[-1])
        self.assertIn("--release", commands[-1])
        self.assertNotIn("--device", commands[-1])

    def test_test_action_builds_test_executables_before_ctest(self):
        self.binary("desktop-x86_64-debug")
        with patch.object(workflow, "run") as run:
            workflow.execute(self.root, workflow.parser().parse_args(["--action", "test"]))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertEqual(commands[-2][1:], ["--build", "--preset", "desktop-x86_64-debug-tests"])
        self.assertEqual(commands[-1][1:], ["--preset", "desktop-x86_64-debug", "--output-on-failure", "--no-tests=error"])

    def test_android_debug_waits_for_main_and_starts_background_server(self):
        binary = self.binary("android-x86_64-debug")
        app = binary / "app_qt"
        output = app / "android-build"
        output.mkdir(parents=True)
        (app / f"android-{workflow.APP_TARGET}-deployment-settings.json").touch()
        (output / "AndroidManifest.xml").write_text(
            '<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="example.app">'
            '<application><activity android:name="QtActivity"/></application></manifest>')
        server = self.root / "ndk/toolchains/llvm/prebuilt/windows-x86_64/lib/clang/21/lib/linux/x86_64/lldb-server"
        server.parent.mkdir(parents=True)
        server.touch()

        def result(command, *args, **kwargs):
            text = ""
            if "ro.product.cpu.abilist" in command:
                text = "x86_64,x86\n"
            elif "pidof" in command:
                text = "123\n"
            elif "/proc/123/status" in command:
                text = "State:\tT (stopped)\n"
            return subprocess.CompletedProcess(command, 0, text)

        process = Mock()
        process.poll.return_value = None
        with patch.object(workflow, "run", side_effect=result) as run, \
                patch.object(workflow, "background", return_value=process) as background, \
                patch.object(workflow.time, "sleep"):
            workflow.execute(self.root, workflow.parser().parse_args([
                "--action", "debug", "--preset", "android-x86_64-debug", "--serial", "emulator-5554"]))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertTrue(any("--wait-for-native-debugger" in command for command in commands))
        self.assertTrue(any("/proc/123/status" in command for command in commands))
        self.assertEqual(background.call_args.args[0][-3:], ["127.0.0.1:5039", "--attach", "123"])
@unittest.skipUnless(shutil.which("cmake") and shutil.which("clangd"),
                     "CMake and clangd required for index-cache integration")
class ClangdCacheTests(unittest.TestCase):
    def test_each_build_uses_its_own_index_cache(self):
        with tempfile.TemporaryDirectory(prefix="mainverte clangd ") as temporary:
            root = Path(temporary)
            helper = Path(__file__).parents[1] / "clangd.cmake"
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.25)\nproject(CacheTest NONE)\n")
            source = root / "probe.cpp"
            source.write_text("// clazy:skip\nint cache_probe() { return 42; }\n")
            for name in ("desktop_debug", "android_debug", "desktop_debug"):
                binary = root / "build" / name
                subprocess.run([
                    "cmake", "-S", str(root), "-B", str(binary), "-G", "Ninja",
                    f"-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES={helper.as_posix()}",
                ], check=True, capture_output=True, text=True)
                self.assertIn(f"CompilationDatabase: '{binary.as_posix()}'",
                              (root / ".clangd").read_text())
                (binary / "compile_commands.json").write_text(json.dumps([{
                    "directory": str(binary), "file": str(source),
                    "arguments": ["clang++", "-c", str(source)],
                }]))
                # The VS Code restart notification copy must not redirect the
                # actual project's index cache back to the repository root.
                shutil.copyfile(binary / "compile_commands.json",
                                root / "compile_commands.json")
                server = subprocess.Popen([
                    "clangd", "--background-index", "-j=1", "--log=error",
                ], cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE)

                def send(message):
                    body = json.dumps(message).encode()
                    server.stdin.write(f"Content-Length: {len(body)}\r\n\r\n".encode() + body)
                    server.stdin.flush()

                try:
                    send({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                          "params": {"rootUri": root.as_uri(), "capabilities": {}}})
                    send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
                    send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
                          "params": {"textDocument": {"uri": source.as_uri(),
                                    "languageId": "cpp", "version": 1,
                                    "text": source.read_text()}}})
                    index = binary / ".cache/clangd/index"
                    deadline = time.monotonic() + 15
                    while not list(index.glob("*.idx")) and time.monotonic() < deadline:
                        time.sleep(0.1)
                    self.assertTrue(list(index.glob("*.idx")),
                                    f"No per-build index in {index}")
                finally:
                    server.terminate()
                    server.communicate(timeout=5)
            self.assertFalse((root / ".cache/clangd/index").exists())


if __name__ == "__main__":
    unittest.main()

"""Generate machine-local CMake presets and VS Code tasks using Python 3.9+."""

import argparse
import json
import os
from pathlib import Path
import sys


APP_TARGET = "mainverte_app_qt"
BUILDS = (
    ("desktop-x86_64-debug", "desktop_x86_64", "Debug", "mingw_64"),
    ("desktop-x86_64-release", "desktop_x86_64", "Release", "mingw_64"),
    ("desktop-x86_64-profile", "desktop_x86_64", "RelWithDebInfo", "mingw_64"),
    ("android-x86_64-debug", "android_x86_64", "Debug", "android_x86_64"),
    ("android-arm64-debug", "android_arm64", "Debug", "android_arm64_v8a"),
    ("android-arm64-release", "android_arm64", "Release", "android_arm64_v8a"),
)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def parser():
    options = argparse.ArgumentParser(description=__doc__)
    local_app_data = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local"))
    defaults = {
        "qt-root": "C:/Qt", "qt-version": "6.12.0",
        "tools-bin": str(Path.home() / "Documents/System/bin"),
        "sdk": str(local_app_data / "Android/Sdk"),
        "ndk": str(local_app_data / "Android/Sdk/ndk/29.0.14206865"),
        "jdk": "C:/Program Files/Java/jdk-21.0.11", "llvm": "C:/Program Files/LLVM/bin",
        "clazy": "C:/Qt/Tools/Qt Creator 21.0.0-rc1/bin/clang/bin/clazy-standalone.exe",
    }
    for name, default in defaults.items():
        options.add_argument(f"--{name}", default=default)
    return options


def generate(root, options):
    qt_root, tools, sdk, ndk, jdk, llvm = (
        Path(getattr(options, name)).as_posix()
        for name in ("qt_root", "tools_bin", "sdk", "ndk", "jdk", "llvm")
    )
    qt = f"{qt_root}/{options.qt_version}"
    clazy = Path(options.clazy).as_posix()
    compiler = f"{qt_root}/Tools/mingw1310_64/bin"
    required = [
        f"{qt}/mingw_64/bin/qmake.exe", f"{compiler}/g++.exe", f"{compiler}/gcc.exe",
        f"{compiler}/gdb.exe", f"{tools}/cmake.exe", f"{tools}/ctest.exe",
        f"{qt_root}/Tools/Ninja/ninja.exe", f"{sdk}/platform-tools/adb.exe",
        f"{ndk}/build/cmake/android.toolchain.cmake", f"{jdk}/bin/java.exe",
        f"{llvm}/clangd.exe", f"{llvm}/clang-tidy.exe", f"{llvm}/clang-format.exe", clazy,
    ]
    required.extend(f"{qt}/{kit}/lib/cmake/Qt6/qt.toolchain.cmake"
                    for kit in ("android_x86_64", "android_arm64_v8a"))
    for path in required:
        if not Path(path).is_file():
            raise ValueError(f"Required tool not found: {path}")

    editor = root / ".vscode"
    editor.mkdir(exist_ok=True)
    write_json(editor / "mainverte-tools.json", {
        "qt": qt, "qtRoot": qt_root, "toolsBin": tools,
        "cmake": f"{tools}/cmake.exe", "ctest": f"{tools}/ctest.exe",
        "sdk": sdk, "ndk": ndk, "jdk": jdk, "llvm": llvm, "clazy": clazy,
    })
    configure, build, test, tasks, launch = [], [], [], [], []
    for name, directory, config, kit in BUILDS:
        android = name.startswith("android-")
        binary = f"${{sourceDir}}/build/{directory}/{config}"
        variables = {
            "CMAKE_BUILD_TYPE": config, "CMAKE_MAKE_PROGRAM": f"{qt_root}/Tools/Ninja/ninja.exe",
            "BUILD_TESTING": not android,
            "CMAKE_PROJECT_TOP_LEVEL_INCLUDES": "${sourceDir}/tools/clangd.cmake",
        }
        environment = {
            "PATH": f"{tools};{qt}/mingw_64/bin;{compiler};$penv{{PATH}}",
            "QT_QPA_FONTDIR": f"{os.environ.get('WINDIR', 'C:/Windows')}/Fonts",
        }
        if android:
            abi = "x86_64" if kit == "android_x86_64" else "arm64-v8a"
            variables.update({
                "CMAKE_TOOLCHAIN_FILE": f"{qt}/{kit}/lib/cmake/Qt6/qt.toolchain.cmake",
                "QT_HOST_PATH": f"{qt}/mingw_64", "ANDROID_SDK_ROOT": sdk,
                "ANDROID_NDK_ROOT": ndk, "ANDROID_ABI": abi, "QT_ANDROID_ABIS": abi,
            })
            environment.update(JAVA_HOME=jdk, ANDROID_SDK_ROOT=sdk, ANDROID_NDK_ROOT=ndk)
        else:
            variables.update(CMAKE_C_COMPILER=f"{compiler}/gcc.exe",
                             CMAKE_CXX_COMPILER=f"{compiler}/g++.exe",
                             CMAKE_PREFIX_PATH=f"{qt}/mingw_64")
        configure.append(dict(name=name, inherits="workflow-base", binaryDir=binary,
                              cacheVariables=variables, environment=environment))
        build.append(dict(name=name, configurePreset=name, targets=[APP_TARGET]))
        if not android:
            build.append(dict(name=f"{name}-tests", configurePreset=name))
            test.append(dict(name=name, configurePreset=name, output={"outputOnFailure": True},
                             execution={"noTestsAction": "error"}))
        for action in ["build"] + (["package", "debug", "deploy"] if android else ["test"]):
            tasks.append({
                "label": f"{action} {name}", "type": "process", "command": sys.executable,
                "args": ["${workspaceFolder}/tools/workflow.py", "--action", action, "--preset", name],
                "problemMatcher": ["$gcc"], "group": "build",
            })
        binary = f"${{workspaceFolder}}/build/{directory}/{config}"
        if android:
            arch = "x86_64" if abi == "x86_64" else "aarch64"
            launch.append({
                "name": name, "type": "lldb", "request": "launch", "preLaunchTask": f"debug {name}",
                "targetCreateCommands": [
                    "platform select remote-android",
                    f"settings set target.exec-search-paths '{binary}/debug-sysroot' '{binary}/app_qt' '{qt}/{kit}/lib'",
                    f"target create --arch {arch}-unknown-linux-android '{binary}/debug-sysroot/app_process64'",
                ],
                "processCreateCommands": ["gdb-remote 127.0.0.1:5039",
                                          "process handle SIGSTOP --pass false --stop false --notify false"],
            })
        else:
            launch.append({
                "name": name, "type": "cppdbg", "request": "launch", "preLaunchTask": f"build {name}",
                "program": f"{binary}/app_qt/{APP_TARGET}.exe", "cwd": "${workspaceFolder}",
                "MIMode": "gdb", "miDebuggerPath": f"{compiler}/gdb.exe",
                "environment": [{"name": "PATH", "value": f"{qt}/mingw_64/bin;{compiler};${{env:PATH}}"}],
            })
    write_json(root / "CMakeUserPresets.json", dict(version=6, configurePresets=configure,
                                                  buildPresets=build, testPresets=test))
    write_json(editor / "tasks.json", dict(version="2.0.0", tasks=tasks))
    write_json(editor / "launch.json", dict(version="0.2.0", configurations=launch))
    write_json(editor / "settings.json", {
        "cmake.cmakePath": f"{tools}/cmake.exe", "cmake.ctestPath": f"{tools}/ctest.exe",
        "cmake.useCMakePresets": "always", "cmake.configureOnOpen": False,
        "cmake.automaticReconfigure": True,
        "cmake.copyCompileCommands": "${workspaceFolder}/compile_commands.json",
        "clangd.onConfigChanged": "restart", "clangd.onConfigChangedForceEnable": True,
        "C_Cpp.intelliSenseEngine": "disabled", "clangd.path": f"{llvm}/clangd.exe",
        "clangd.arguments": ["--background-index", "--clang-tidy",
                             f"--query-driver={compiler}/*.exe,{ndk}/toolchains/llvm/prebuilt/windows-x86_64/bin/*.exe"],
        "[cpp]": {"editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"},
    })


if __name__ == "__main__":
    try:
        generate(Path(__file__).resolve().parent.parent, parser().parse_args())
        print("Workflow ready. Select a CMake preset and an F5 launch configuration in VS Code.")
    except (OSError, ValueError) as error:
        sys.exit(str(error))

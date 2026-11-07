"""Build, test, analyze and debug MainVerte using the generated local presets."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET


APP_TARGET = "mainverte_app_qt"


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def run(command, root, environment, capture=False, check=True):
    return subprocess.run([str(part) for part in command], cwd=root, env=environment,
                          check=check, text=True, stdout=subprocess.PIPE if capture else None)


def background(command, root, environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL):
    # Keep emulator/server processes alive after the task exits, without opening
    # a console window. The emulator's own graphical interface remains available.
    flags = subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    return subprocess.Popen([str(part) for part in command], cwd=root, env=environment,
                            stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                            creationflags=flags, close_fds=True)


def analysis_database(binary):
    """Retain textual forced includes without loading the kit's binary PCH."""
    directory = binary / "analysis"
    directory.mkdir(exist_ok=True)
    commands = read_json(binary / "compile_commands.json")
    copies = {}
    for entry in commands:
        command = re.sub(r' -Xclang -include-pch -Xclang ("[^"]*"|\S+)', "", entry["command"])
        match = re.search(r'(?:-Xclang )?-include (?:-Xclang )?("[^"]*cmake_pch[.]hxx"|\S*cmake_pch[.]hxx)', command)
        if match:
            header = match[1].strip('"')
            if header not in copies:
                copies[header] = directory / f"pch-{len(copies)}.h"
                shutil.copyfile(header, copies[header])
            command = command.replace(match[0], f'-include "{copies[header].as_posix()}"')
        entry["command"] = command
    (directory / "compile_commands.json").write_text(json.dumps(commands, indent=2), encoding="utf-8")
    return directory


def parser():
    options = argparse.ArgumentParser(description=__doc__)
    options.add_argument("--action", choices=("configure", "build", "package", "test", "deploy", "debug",
                                             "emulator", "editor", "tidy", "clazy", "format"), default="build")
    options.add_argument("--preset", default="desktop-x86_64-debug")
    options.add_argument("--serial", default=os.environ.get("ANDROID_SERIAL"))
    options.add_argument("--file", type=Path)
    return options


def execute(root, options):
    local = root / ".vscode/mainverte-tools.json"
    if not local.is_file():
        raise ValueError("Run python tools/setup_workflow.py first.")
    machine = read_json(local)
    # Windows environments sometimes contain both PATH and Path. Keep one entry
    # so CTest inherits the same Qt/compiler runtime paths as the build process.
    environment = {key: value for key, value in os.environ.items() if key.lower() != "path"}
    environment.update(JAVA_HOME=machine["jdk"], ANDROID_SDK_ROOT=machine["sdk"],
                       ANDROID_NDK_ROOT=machine["ndk"],
                       QT_QPA_FONTDIR=f"{os.environ.get('WINDIR', 'C:/Windows')}/Fonts")
    environment["PATH"] = os.pathsep.join((machine["toolsBin"], f'{machine["qt"]}/mingw_64/bin',
                                           f'{machine["qtRoot"]}/Tools/mingw1310_64/bin', os.environ.get("PATH", "")))
    action, preset = options.action, options.preset
    if action == "editor":
        code = shutil.which("code", path=environment["PATH"])
        if not code:
            raise ValueError("VS Code's code command was not found in PATH.")
        run([code, "--new-window", root], root, environment)
        return
    if action == "emulator":
        background([f'{machine["sdk"]}/emulator/emulator.exe', "-avd", "MainVertePhone"], root, environment)
        return
    presets = read_json(root / "CMakeUserPresets.json")["configurePresets"]
    selected = next((item for item in presets if item["name"] == preset), None)
    if selected is None:
        raise ValueError(f"Unknown preset: {preset}")
    binary = Path(selected["binaryDir"].replace("${sourceDir}", root.as_posix()))
    android = preset.startswith("android-")
    if action in ("format", "tidy", "clazy"):
        source = options.file
        if source is not None and not source.is_absolute():
            source = root / source
        if source is None or not source.is_file():
            raise ValueError("Supply --file with an existing source file.")
        if action == "format":
            run([f'{machine["llvm"]}/clang-format.exe', "--dry-run", "--Werror", source], root, environment)
        else:
            if not (binary / "compile_commands.json").is_file():
                raise ValueError("Build this preset first to generate headers and the compilation database.")
            tool = f'{machine["llvm"]}/clang-tidy.exe' if action == "tidy" else machine["clazy"]
            run([tool, "-p", analysis_database(binary), source], root, environment)
        return
    cmake = machine["cmake"]
    run([cmake, "--preset", preset], root, environment)
    # Publish only after successful configuration; the VS Code watcher restarts
    # clangd after the build's .clangd/database/cache selection is ready.
    shutil.copyfile(binary / "compile_commands.json", root / "compile_commands.json")
    if action == "configure":
        return
    if action == "test":
        if android:
            raise ValueError("Android CTest requires a device test runner; use desktop test presets.")
        run([cmake, "--build", "--preset", f"{preset}-tests"], root, environment)
        run([machine["ctest"], "--preset", preset, "--output-on-failure", "--no-tests=error"], root, environment)
        return
    run([cmake, "--build", "--preset", preset], root, environment)
    if action == "build":
        return
    if not android:
        raise ValueError("Use the VS Code F5 desktop launch configuration.")
    settings = binary / f"app_qt/android-{APP_TARGET}-deployment-settings.json"
    if not settings.is_file():
        raise ValueError(f"Qt Android deployment settings not found: {settings}")
    output = binary / "app_qt/android-build"
    deploy = [f'{machine["qt"]}/mingw_64/bin/androiddeployqt.exe', "--input", settings,
              "--output", output, "--jdk", machine["jdk"]]
    release = selected["cacheVariables"]["CMAKE_BUILD_TYPE"] == "Release"
    if action != "debug" and release:
        deploy.append("--release")
    if action != "package":
        serial = options.serial
        adb = [f'{machine["sdk"]}/platform-tools/adb.exe']
        if not serial:
            devices = re.findall(r"^(\S+)\s+device\s*$", run(adb + ["devices"], root, environment, capture=True).stdout, re.M)
            if len(devices) != 1:
                raise ValueError("Connect one device, or supply --serial / ANDROID_SERIAL. Start MainVertePhone with --action emulator.")
            serial = devices[0]
        adb += ["-s", serial]
        abi = selected["cacheVariables"]["ANDROID_ABI"]
        device_abis = run(adb + ["shell", "getprop", "ro.product.cpu.abilist"], root, environment, capture=True).stdout.strip()
        if abi not in device_abis.split(","):
            raise ValueError(f"Device {serial} does not support {abi} ({device_abis}).")
        if action == "deploy" and release:
            debug_key = Path.home() / ".android/debug.keystore"
            if not debug_key.is_file():
                debug_key.parent.mkdir(exist_ok=True)
                run([f'{machine["jdk"]}/bin/keytool.exe', "-genkeypair", "-keystore", debug_key,
                     "-storepass", "android", "-keypass", "android", "-alias", "androiddebugkey",
                     "-dname", "CN=Android Debug,O=Android,C=US", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000"], root, environment)
            deploy += ["--sign", debug_key, "androiddebugkey", "--storepass", "android", "--keypass", "android"]
        deploy += ["--reinstall", "--device", serial]
    run([cmake, "--build", binary, "--target", f"{APP_TARGET}_prepare_apk_dir"], root, environment)
    run(deploy, root, environment)
    if action == "package":
        return
    manifest = ET.parse(output / "AndroidManifest.xml").getroot()
    package = manifest.attrib["package"]
    activity = manifest.find("application/activity").attrib["{http://schemas.android.com/apk/res/android}name"]
    run(adb + ["shell", "am", "force-stop", package], root, environment)
    start = adb + ["shell", "am", "start", "-n", f"{package}/{activity}"]
    if action == "debug":
        start += ["--es", "applicationArguments", "--wait-for-native-debugger"]
    run(start, root, environment)
    if action != "debug":
        return
    process_id = ""
    for _ in range(30):
        result = run(adb + ["shell", "pidof", package], root, environment, capture=True, check=False)
        process_id = result.stdout.strip()
        if process_id:
            break
        time.sleep(0.2)
    if not process_id.isdigit():
        raise ValueError("Application exited before the debugger could attach. Check adb logcat.")
    for _ in range(150):
        status = run(adb + ["shell", "run-as", package, "cat", f"/proc/{process_id}/status"], root, environment, capture=True).stdout
        if re.search(r"^State:\s+T\s", status, re.M):
            break
        time.sleep(0.2)
    else:
        raise ValueError("Timed out waiting for the native startup pause. Check adb logcat; force-stop the app before retrying.")
    sysroot = binary / "debug-sysroot"
    sysroot.mkdir(exist_ok=True)
    for source, destination in (("/system/bin/app_process64", "app_process64"), ("/system/bin/linker64", "linker64")):
        run(adb + ["pull", source, sysroot / destination], root, environment)
    arch = "aarch64" if abi == "arm64-v8a" else "x86_64"
    servers = list(Path(machine["ndk"]).glob(f"toolchains/llvm/prebuilt/windows-x86_64/lib/clang/*/lib/linux/{arch}/lldb-server"))
    if len(servers) != 1:
        raise ValueError("NDK lldb-server not found unambiguously.")
    run(adb + ["push", servers[0], "/data/local/tmp/mainverte-lldb-server"], root, environment)
    run(adb + ["shell", "run-as", package, "cp", "/data/local/tmp/mainverte-lldb-server", "./mainverte-lldb-server"], root, environment)
    run(adb + ["shell", "run-as", package, "chmod", "700", "./mainverte-lldb-server"], root, environment)
    run(adb + ["forward", "tcp:5039", "tcp:5039"], root, environment)
    with (binary / "lldb-server.log").open("w") as stdout, (binary / "lldb-server-error.log").open("w") as stderr:
        server = background(adb + ["shell", "run-as", package, "./mainverte-lldb-server", "gdbserver",
                                   "127.0.0.1:5039", "--attach", process_id], root, environment, stdout, stderr)
    time.sleep(0.7)
    if server.poll() is not None:
        raise ValueError(f'lldb-server exited: {(binary / "lldb-server-error.log").read_text()}')
    print(f"Native debugger ready on localhost:5039 ({serial}). See lldb-server*.log if attachment fails.")


if __name__ == "__main__":
    try:
        execute(Path(__file__).resolve().parent.parent, parser().parse_args())
    except subprocess.CalledProcessError as error:
        print(f"Command failed ({error.returncode}): {subprocess.list2cmdline([str(part) for part in error.cmd])}", file=sys.stderr)
        sys.exit(1)
    except (OSError, ValueError, KeyError, ET.ParseError) as error:
        sys.exit(str(error))

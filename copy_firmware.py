Import("env")

import os
import re
import shutil


def copy_firmware(source, target, env):
    firmware_src = str(target[0])
    release_dir = os.path.join(env["PROJECT_DIR"], "release")
    os.makedirs(release_dir, exist_ok=True)

    version = "unknown"
    main_cpp = os.path.join(env["PROJECT_DIR"], "src", "main.cpp")

    try:
        with open(main_cpp, "r", encoding="utf-8") as file:
            content = file.read()
            match = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', content)
            if match:
                version = match.group(1)
                print(f"[POST-BUILD] Found FW_VERSION: {version}")
            else:
                print(f"[POST-BUILD] WARNING: Could not find FW_VERSION in {main_cpp}")
    except Exception as error:
        print(f"[POST-BUILD] ERROR reading {main_cpp}: {error}")

    suffix = re.sub(r"\.", "", version)

    if os.path.exists(firmware_src):
        destination = os.path.join(release_dir, f"firmware{suffix}.bin")
        shutil.copy(firmware_src, destination)
        print(f"[POST-BUILD] Firmware copied -> release/firmware{suffix}.bin (v{version})")
    else:
        print(f"[POST-BUILD] ERROR: Source firmware not found at {firmware_src}")


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
Import("env")

import os
import shutil

APP_BIN = "$BUILD_DIR/${PROGNAME}.bin"
PROJECT_DIR = env.subst("$PROJECT_DIR")
MERGED_BIN = os.path.join(PROJECT_DIR, "TamaPoke-CardputerADV-v0.7-MERGED.bin")
APP_COPY = os.path.join(PROJECT_DIR, "TamaPoke-CardputerADV-v0.7-firmware.bin")


def merge_bin(source, target, env):
    board = env.BoardConfig()
    chip = board.get("build.mcu", "esp32s3")

    flash_images = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        flash_images += [str(offset), '"%s"' % env.subst(image)]

    flash_images += [env.subst("$ESP32_APP_OFFSET"), '"%s"' % env.subst(APP_BIN)]

    cmd = " ".join([
        '"%s"' % env.subst("$PYTHONEXE"),
        '"%s"' % env.subst("$OBJCOPY"),
        "--chip", str(chip),
        "merge_bin",
        "-o", '"%s"' % MERGED_BIN,
    ] + flash_images)

    print("Creating single merged flash image...")
    result = env.Execute(cmd)
    if result != 0:
        print("WARNING: merged BIN creation failed; normal firmware.bin still exists.")
    else:
        print("MERGED BIN: %s" % MERGED_BIN)

    shutil.copyfile(env.subst(APP_BIN), APP_COPY)
    print("APP BIN: %s" % APP_COPY)


env.AddPostAction(APP_BIN, merge_bin)

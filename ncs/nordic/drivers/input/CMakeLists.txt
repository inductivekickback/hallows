# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_property(ALLOW_EMPTY TRUE)

# zephyr-keep-sorted-start
zephyr_library_sources_ifdef(CONFIG_SNES_GAMEPAD input_snes_gamepad.c)
# zephyr-keep-sorted-stop

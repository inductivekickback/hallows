#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_sources_ifdef(CONFIG_BT_HALLOWS hallows.c)
zephyr_sources_ifdef(CONFIG_BT_HALLOWS_CLIENT hallows_client.c)

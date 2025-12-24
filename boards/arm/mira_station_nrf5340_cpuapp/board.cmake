board_runner_args(jlink "--device=nRF5340_xxAA_APP" "--speed=4000")
board_runner_args(pyocd "--target=nrf5340" "--frequency=4000000")

include(${ZEPHYR_BASE}/boards/common/nrfjprog.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.cmake)
include(${ZEPHYR_BASE}/boards/common/pyocd.cmake)

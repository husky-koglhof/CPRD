BOARD=$1
$HOME/.platformio/penv/bin/platformio device monitor -p /dev/cu.SLAB_USBtoUART${1} -b 115200
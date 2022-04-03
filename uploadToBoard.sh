BOARD=$1
$HOME/.platformio/penv/bin/platformio run upload --upload-port /dev/cu.SLAB_USBtoUART${1} -b 115200
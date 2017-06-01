#!/bin/bash

ESPTOOL=esptool
RBOOT=/home/niklas/firmware/rboot.bin
ROM0=/home/niklas/firmware/rom0.bin
ROM1=/home/niklas/firmware/rom1.bin
ROM2=/home/niklas/firmware/rom2.bin
ROM3=/home/niklas/firmware/rom3.bin
ROM4=/home/niklas/firmware/rom4.bin
ROM5=/home/niklas/firmware/rom5.bin
SPIFFS=/home/niklas/firmware/spiffs.bin


if ! echo "$DEVNAME" | grep "/dev/ttyUSB" > /dev/null
then
	exit
fi

TTY=${DEVNAME#/dev/}
LOCKFILE="/var/lock/badge-${TTY}.lock"

dolog() {
	echo "$1" >> "/var/log/badge-flash-${TTY}.log"
}

rmlock() {
	rm "$LOCKFILE"
}

trap rmlock EXIT

dolog "New badge detected"

if [ -e "$LOCKFILE" ]
then
	dolog "Lock file $LOCKFILE exists, aborting"
	exit
fi

touch "$LOCKFILE"

dolog "Flashing..."
"$ESPTOOL" -vv -cd nodemcu -cb 921600 -cp "/dev/$TTY" -ca 0x00000 -cf "$RBOOT" -ca 0x2000 -cf "$ROM0" -ca 0x82000 -cf "$ROM1" -ca 0x102000 -cf "$ROM2" -ca 0x182000 -cf "$ROM3" -ca 0x202000 -cf "$ROM4" -ca 0x282000 -cf "$ROM5" -ca 0x300000 -cf "$SPIFFS" 2>&1 >> "/var/log/badge-flash-${TTY}.log"
dolog "Flashing done, exit code $?"

for i in $(seq 1 10)
do
	dolog ""
done

#!/bin/bash

cp -rf ido_device/* ../device/
if [ $? -ne 0 ]; then
    echo "device patch failed!!!"
    exit 1
fi

cp -rf ido_vendor/* ../vendor/
if [ $? -ne 0 ]; then
    echo "vendor patch failed!!!"
    exit 1
fi

cp -rf ido_build/* ../build/
if [ $? -ne 0 ]; then
    echo "build patch failed!!!"
    exit 1
fi

cp -rf ../kernel/linux/config/linux-5.10/rk3568 ../kernel/linux/config/linux-5.10/purple_pi_oh
if [ $? -ne 0 ]; then
    echo "kernel patch failed!!!"
    exit 1
fi

cp -rf ido_base/ohos.para ../base/startup/init/services/etc/param/ohos.para
if [ $? -ne 0 ]; then
	echo "base patch failed!!!"
	exit 1
fi

cp -rf ido_drivers/* ../drivers/
if [ $? -eq 0 ]; then
    echo "drivers patch complete!"
else
    echo "patch failed!!!"
    exit 1
fi

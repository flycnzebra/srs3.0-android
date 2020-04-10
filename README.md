# srs3.0-android
cd trunk
./configure --static
make

cp objs/srs to android /system/bin/
cp conf/srs.conf to android /etc/srs/srs.conf
mk android /etc/srs/objs

run in adb shell: srs -c /etc/srs/srs.conf

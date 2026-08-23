sudo rmmod hello_driver
clear && make
sudo insmod hello_driver.ko
sudo dmesg | tail

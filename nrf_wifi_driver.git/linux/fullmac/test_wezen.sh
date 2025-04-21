#rm -rf /lib/firmware/nrf/wifi/*
#Working Firmware
#cp /root/ajay/Working_PCIE/nrf_wifi_*.bimg /lib/firmware/nrf/wifi/.

#service kdump-tools status
#systemctl enable kdump-tools

#/etc/init.d/kdump-tools start
#/etc/init.d/kexec start
#kdump-config show


#(Default) For OFFLINE Mode with SECURE_DOMAIN 
#make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=NONE BUS_IF=PCIE SECURE_DOMAIN=Y

#For OFFLINE Mode with HOSTLOADING
#make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=HEX BUS_IF=PCIE SECURE_DOMAIN=Y

#For OFFLINE Mode(TX) and RX INLINE and SECURE_DOMAIN  with CODESCAPE Loading
#make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=NONE BUS_IF=PCIE SECURE_DOMAIN=Y INLINE_MODE_RX=Y

#For OFFLINE Mode(TX) and RX INLINE and SECURE_DOMAIN  with HOSTLOADING
#make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=HEX BUS_IF=PCIE SECURE_DOMAIN=Y INLINE_MODE_RX=Y

#For OFFLINE MODE(TX) with RX INLINE and SECURE_DOMAIN
#make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=NONE BUS_IF=PCIE SECURE_DOMAIN=Y INLINE_MODE_RX=Y

#For OFFLINE Mode(TX) and RX INLINE and SECURE_DOMAIN and CMD_RX_BUFF  with HOSTLOADING
#make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=HEX BUS_IF=PCIE SECURE_DOMAIN=Y INLINE_MODE_RX=Y CMD_RX_BUFF=Y

#For OFFLINE Mode(TX) and RX INLINE and SECURE_DOMAIN  with HOSTLOADING with DEBUGFS CONF SUPPORT
make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=HEX BUS_IF=PCIE SECURE_DOMAIN=Y INLINE_MODE_RX=Y MODE=REG

#For OFFLINE Mode(TX) and RX INLINE and SECURE_DOMAIN and CMD_RX_BUFF  with HOSTLOADING with SOFT_HPQM
#make clean all PLATFORM=WEZEN FUNC=WLAN LOW_POWER=0 HOST_CFG80211=Y INLINE_MODE=N RF=C0 CONFIG=72 FW_LOAD=HEX BUS_IF=PCIE SECURE_DOMAIN=Y INLINE_MODE_RX=Y CMD_RX_BUFF=Y SOFT_HPQM=Y

lsmod | grep nrf_wifi_fmac_reg
/etc/init.d/network-manager stop
killall -9 wpa_supplicant
killall -9 wpa_supplicant
pkill -9 wpa_cli
rfkill unblock all
modprobe cfg80211
insmod nrf_wifi_fmac_reg.ko
sleep 2
ifconfig nrf_wifi up 192.168.1.77
sleep 2 
iw nrf_wifi set power_save off
ip link show
sleep 2
iw dev nrf_wifi info
sleep 2
sudo ifconfig nrf_wifi up
sudo iw dev nrf_wifi scan | grep HOST
#sudo iw dev nrf_wifi scan | grep UMAC


KROOT = /lib/modules/$(shell uname -r)/build
ROOT_INC_DIR	= $(shell cd $(PWD); cd ../../../nrfxlib/nrf_wifi; pwd)
OSAL_DIR        = ../../../nrfxlib/nrf_wifi
LINUX_SHIM_INC_DIR = $(shell cd $(PWD); pwd)
LINUX_SHIM_DIR = .
HOST_CFG80211?=N
FW_LOAD?=PATCH

define func_err=

Usage make FUNC=<WLAN|PWR|WLAN_PWR|HAL_TB>
    where:
	WLAN = Only WLAN functionality enabled
    	PWR = Only Power Monitoring functionality enabled
    	WLAN_PWR = Both WLAN and Power Monitoring functionalities enabled
    	HAL_TB = Only HAL test bench functionality enabled
endef

define mode_err=

Usage make MODE=<REG|RADIO-TEST|DEBUG>
    where:
	REG = Regular mode
    	RADIO-TEST = Radio test mode
    	DEBUG = Debug mode

endef

define fw_load_err=

Usage make FW_LOAD=<RAM|ROM|PATCH|ROM_PATCH|NONE|HEX>
    where:
	RAM  = FW RAM load support
	ROM  = FW ROM load support only
	PATCH  = FW patch load support only
	ROM_PATCH  = FW ROM and patch load support
	HEX = FW hex load support

endef

WLAN_SUPPORT = 0
HAL_TB = 0
CONF_SUPPORT = 0
CMD_DEMO = 0
FW_LOAD_SUPPORT = 0
TWT_SUPPORT = 0
ifeq ($(CMD_DEMO), 1)
ccflags-y += -DCMD_DEMO
endif

ifeq ($(FUNC), WLAN)
WLAN_SUPPORT = 1
DRV_FUNC_NAME =  _wifi_fmac
else ifeq ($(FUNC), HAL_TB)
HAL_TB = 1
DRV_FUNC_NAME =  _hal_tb
endif

ifneq (, $(filter RAM ROM PATCH ROM_PATCH HEX, $(FW_LOAD)))
FW_LOAD_SUPPORT = 1
endif

ifneq (, $(filter RADIO-TEST DEBUG REG, $(MODE)))
CONF_SUPPORT = 1
endif

-include $(PWD)/Makefile.calder
-include $(PWD)/Makefile.rpi
-include $(PWD)/Makefile.wezen

# Compilation flags
ifeq ($(WLAN_SUPPORT), 1)
ccflags-y += -DWLAN_SUPPORT
ccflags-y += -DRPU_CONFIG_FMAC
# Disabling since CRC support is possibly not present in the FW due to which FW loading is failing
# ccflags-y += -DCRC_CHECKS
endif

ifeq ($(MODE), REG)
DRV_MODE_NAME = _reg
else ifeq ($(MODE), RADIO-TEST)
ccflags-y += -DCONFIG_NRF700X_RADIO_TEST
DRV_MODE_NAME = _radio_test
else ifeq ($(MODE), DEBUG)
ccflags-y += -DDEBUG_MODE_SUPPORT
DRV_MODE_NAME = _dbg
endif

ifeq ($(FW_LOAD_SUPPORT), 1)
ccflags-y += -DHOST_FW_LOAD_SUPPORT
ifeq ($(FW_LOAD), RAM)
ccflags-y += -DHOST_FW_RAM_LOAD_SUPPORT
else ifeq ($(FW_LOAD), ROM)
ccflags-y += -DHOST_FW_ROM_LOAD_SUPPORT
else ifeq ($(FW_LOAD), PATCH)
ccflags-y += -DHOST_FW_PATCH_LOAD_SUPPORT
else ifeq ($(FW_LOAD), ROM_PATCH)
ccflags-y += -DHOST_FW_ROM_LOAD_SUPPORT
ccflags-y += -DHOST_FW_PATCH_LOAD_SUPPORT
else ifeq ($(FW_LOAD), HEX)
ccflags-y += -DHOST_FW_HEX_LOAD_SUPPORT
endif
endif

ifneq ($(FW_LOAD), NONE)
ccflags-y += -DRPU_HARD_RESET_SUPPORT
endif

ifeq ($(CONF_SUPPORT), 1)
ccflags-y += -DCONF_SUPPORT
endif

ifeq ($(TWT_SUPPORT), 1)
ccflags-y += -DTWT_SUPPORT
endif

ifeq ($(LOW_POWER), 1)
ccflags-y += -DLOW_POWER
endif

ifeq ($(HAL_TB), 1)
ccflags-y += -DHAL_TB
endif

ifeq ($(CMD_DEMO), 1)
ccflags-y += -D CMD_DEMO
endif

# Common Includes
ccflags-y += -I$(LINUX_SHIM_INC_DIR)/inc
ccflags-y += -I$(ROOT_INC_DIR)/utils/inc
ccflags-y += -I$(ROOT_INC_DIR)/os_if/inc
ifeq ($(BUS_IF), PCIE)
ccflags-y += -I$(ROOT_INC_DIR)/bus_if/bus/pcie/inc
else ifeq ($(BUS_IF), SPI)
ccflags-y += -I$(ROOT_INC_DIR)/bus_if/bus/qspi/inc
endif

ccflags-y += -I$(ROOT_INC_DIR)/hw_if/hal/inc/fw
ccflags-y += -I$(ROOT_INC_DIR)/bus_if/bal/inc
ccflags-y += -I$(ROOT_INC_DIR)/hw_if/hal/inc
ccflags-y += -I$(ROOT_INC_DIR)/hw_if/hal/inc/fw

ifeq ($(WLAN_SUPPORT), 1)
ccflags-y += -I$(ROOT_INC_DIR)/fw_if/umac_if/inc
ccflags-y += -I$(ROOT_INC_DIR)/fw_if/umac_if/inc/default
ccflags-y += -I$(ROOT_INC_DIR)/fw_if/umac_if/inc/fw
endif


ifeq ($(INLINE_MODE), Y)
ifeq ($(BOUNCE_BUF), Y)
ccflags-y += -DINLINE_BB_MODE
else
ccflags-y += -DINLINE_MODE
endif
else
ccflags-y += -DOFFLINE_MODE
endif

ccflags-y += -DCONFIG_NRF_WIFI_BEAMFORMING=1

# Common Objects
OBJS += $(OSAL_DIR)/utils/src/list.o
OBJS += $(OSAL_DIR)/utils/src/queue.o
OBJS += $(OSAL_DIR)/utils/src/util.o
OBJS += $(OSAL_DIR)/os_if/src/osal.o
OBJS += $(OSAL_DIR)/bus_if/bal/src/bal.o
ifeq ($(BUS_IF), PCIE)
OBJS += $(OSAL_DIR)/bus_if/bus/pcie/src/pcie.o
else ifeq ($(BUS_IF), SPI)
OBJS += $(OSAL_DIR)/bus_if/bus/qspi/src/qspi.o
endif
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_mem.o
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_reg.o
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_api.o
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_interrupt.o
OBJS += $(OSAL_DIR)/hw_if/hal/src/pal.o

OBJS += $(LINUX_SHIM_DIR)/src/util.o
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_main.o
OBJS += $(LINUX_SHIM_DIR)/src/lnx_shim.o


ifeq ($(HAL_TB), 1)
OBJS += $(LINUX_SHIM_DIR)/src/hal_tb.o
else
OBJS += $(LINUX_SHIM_DIR)/src/main.o
endif

ifeq ($(WLAN_SUPPORT), 1)
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/cmd.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/event.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/default/fmac_api.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/fmac_api_common.o
endif

OBJS += $(LINUX_SHIM_DIR)/src/netdev.o
ifneq ($(MODE), RADIO-TEST)
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/rx.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/tx.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/fmac_vif.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/fmac_ap.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/fmac_peer.o
OBJS += $(OSAL_DIR)/fw_if/umac_if/src/fmac_util.o
endif
ifeq ($(HOST_CFG80211), Y)
OBJS += $(LINUX_SHIM_DIR)/src/cfg80211_if.o
else
OBJS += $(LINUX_SHIM_DIR)/src/netlink.o
OBJS += $(LINUX_SHIM_DIR)/src/wpa_supp_if.o
endif
OBJS += $(LINUX_SHIM_DIR)/src/wiphy.o
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_wlan_fmac_stats.o
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_wlan_fmac_ver.o
ifeq ($(CMD_DEMO), 1)
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_wlan_fmac_connect.o
endif
ifeq ($(TWT_SUPPORT), 1)
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_wlan_fmac_twt.o
endif
ifeq ($(CONF_SUPPORT), 1)
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_wlan_fmac_conf.o
endif

ifeq ($(FW_LOAD_SUPPORT), 1)
ifeq ($(FW_LOAD), $(filter $(FW_LOAD), ROM ROM_PATCH))
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_fw_rom_loader.o
endif
ifeq ($(FW_LOAD), $(filter $(FW_LOAD), PATCH ROM_PATCH))
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_fw_patch_loader.o
endif
ifeq ($(FW_LOAD), $(filter $(FW_LOAD), RAM))
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_fw_ram_loader.o
endif
ifeq ($(FW_LOAD), $(filter $(FW_LOAD), HEX))
OBJS += $(OSAL_DIR)/hw_if/hal/src/hal_fw_hex_loader.o
endif
endif


ifeq ($(HAL_TB), 1)
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_hal_tb_conf.o
OBJS += $(LINUX_SHIM_DIR)/src/dbgfs_hal_tb_stats.o
endif

ccflags-y += -DCONFIG_WIFI_NRF700X_LOG_LEVEL=3

NAME = nrf$(DRV_FUNC_NAME)$(DRV_MODE_NAME)

obj-m += $(NAME).o

$(NAME)-objs= $(OBJS)

all: validate_params
	@make -C $(KROOT) M=$(PWD) modules

clean:
	@make -C $(KROOT) M=$(PWD) clean
	@find $(OSAL_DIR) -name "*.o*" | xargs rm -rf

# Validate the compile time parameters
validate_params:
ifneq ($(FUNC), $(filter $(FUNC), WLAN PWR WLAN_PWR HAL_TB))
	$(error $(func_err))
endif

ifeq ($(FUNC), PWR)
ifneq ($(FW_LOAD),)
	$(error "FW_LOAD not supported in PWR")
endif
ifneq ($(MODE),)
	$(error "MODE not supported in PWR")
endif
else ifeq ($(FUNC), WLAN_PWR)
ifneq ($(MODE), $(filter $(MODE), REG DEBUG))
	$(error "Radio test MODE not supported in WLAN_PWR")
endif
else ifeq ($(FUNC), HAL_TB)
ifneq ($(FW_LOAD),)
	$(error "FW_LOAD not supported in HAL_TB")
endif
ifneq ($(MODE),)
	$(error "MODE not supported in HAL_TB")
endif
else
ifneq ($(FW_LOAD), $(filter $(FW_LOAD), RAM ROM PATCH ROM_PATCH NONE HEX))
	$(error $(fw_load_err))
endif
ifneq ($(MODE), $(filter $(MODE), REG RADIO-TEST DEBUG))
	$(error $(mode_err))
endif
endif

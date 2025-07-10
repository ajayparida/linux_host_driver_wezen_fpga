/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <linux/etherdevice.h>
#ifdef HOST_FW_LOAD_SUPPORT
#include <linux/firmware.h>
#endif /* HOST_FW_LOAD_SUPPORT */
#include <linux/rtnetlink.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#ifdef BUS_IF_PCIE
#include <pcie.h>
#endif
#include "lnx_main.h"
#include "lnx_fmac_dbgfs_if.h"
#include "pal.h"

#ifdef WLAN_SUPPORT
#include <util.h>
#include "lnx_util.h"
#include "fmac_util.h"
#include "fmac_api.h"
#include "lnx_fmac_main.h"
#include "lnx_net_stack.h"
#ifdef HOST_CFG80211_SUPPORT
#include "cfg80211_if.h"
#else
#include "wpa_supp_if.h"
#endif /* HOST_CFG80211_SUPPORT */

#ifndef CONFIG_NRF700X_RADIO_TEST
char *base_mac_addr = "0019F5331179";
module_param(base_mac_addr, charp, 0000);
MODULE_PARM_DESC(base_mac_addr, "Configure the WiFi base MAC address");

char *rf_params = NULL;
module_param(rf_params, charp, 0000);
MODULE_PARM_DESC(rf_params, "Configure the RF parameters");

#ifdef CONFIG_NRF_WIFI_LOW_POWER
char *sleep_type = NULL;
module_param(sleep_type, charp, 0000);
MODULE_PARM_DESC(sleep_type, "Configure the sleep type parameter");
#endif

#endif /* !CONFIG_NRF700X_RADIO_TEST */

unsigned int phy_calib = NRF_WIFI_DEF_PHY_CALIB;

module_param(phy_calib, uint, 0000);
MODULE_PARM_DESC(phy_calib, "Configure the bitmap of the PHY calibrations required");

/* 3 bytes for addreess, 3 bytes for length */
#define MAX_PKT_RAM_TX_ALIGN_OVERHEAD 6
#define MAX_RX_QUEUES 3

unsigned char aggregation = 1;
unsigned char wmm = 1;
unsigned char max_num_tx_agg_sessions = 4;
unsigned char max_num_rx_agg_sessions = 8;
//unsigned char max_num_rx_agg_sessions = 0;
unsigned char reorder_buf_size = 8;
unsigned char max_rxampdu_size = MAX_RX_AMPDU_SIZE_64KB;

#ifdef OFFLINE_MODE
unsigned char max_tx_aggregation = 13;

unsigned int rx1_num_bufs = 21;
unsigned int rx2_num_bufs = 21;
unsigned int rx3_num_bufs = 21;

unsigned int rx1_buf_sz = 1600;
unsigned int rx2_buf_sz = 1600;
unsigned int rx3_buf_sz = 1600;

#else
unsigned char max_tx_aggregation = MAX_TX_AGG_SIZE;

unsigned int rx1_num_bufs = 128;
unsigned int rx2_num_bufs = 64;
unsigned int rx3_num_bufs = 64;

unsigned int rx1_buf_sz = 2 * 1024;
unsigned int rx2_buf_sz = 2 * 1024;
unsigned int rx3_buf_sz = 2 * 1024;
#endif /* OFFLINE_MODE */

unsigned char rate_protection_type = 0;

module_param(aggregation, byte, 0000);
module_param(wmm, byte, 0000);
module_param(max_num_tx_agg_sessions, byte, 0000);
module_param(max_num_rx_agg_sessions, byte, 0000);
module_param(max_tx_aggregation, byte, 0000);
module_param(reorder_buf_size, byte, 0000);
module_param(max_rxampdu_size, byte, 0000);

module_param(rx1_num_bufs, int, 0000);
module_param(rx2_num_bufs, int, 0000);
module_param(rx3_num_bufs, int, 0000);

module_param(rx1_buf_sz, int, 0000);
module_param(rx2_buf_sz, int, 0000);
module_param(rx3_buf_sz, int, 0000);

module_param(rate_protection_type, byte, 0000);

MODULE_PARM_DESC(aggregation, "Enable (1) / disable (0) AMPDU aggregation");
MODULE_PARM_DESC(wmm, "Enable (1) / Disable (0) WMM");
MODULE_PARM_DESC(max_num_tx_agg_sessions, "Max Tx AMPDU sessions");
MODULE_PARM_DESC(max_num_rx_agg_sessions, "Max Rx AMPDU sessions");
MODULE_PARM_DESC(reorder_buf_size, "Reorder Buffer size (1 to 64)");
MODULE_PARM_DESC(max_rxampdu_size, "Rx AMPDU size (0 to 3 for 8K, 16K, 32K, 64K)");

MODULE_PARM_DESC(rx1_num_bufs, "Rx1 queue size");
MODULE_PARM_DESC(rx2_num_bufs, "Rx2 queue size");
MODULE_PARM_DESC(rx3_num_bufs, "Rx3 queue size");

MODULE_PARM_DESC(rx1_buf_sz, "Rx1 pkt size");
MODULE_PARM_DESC(rx2_buf_sz, "Rx2 pkt size");
MODULE_PARM_DESC(rx3_buf_sz, "Rx3 pkt size");

MODULE_PARM_DESC(rate_protection_type, "0 (NONE), 1 (RTS/CTS), 2 (CTS2SELF)");
#else
#include "pwr_api.h"
#define NRF_WIFI_PCIE_DRV_NAME "nrf_wifi_pwr_pcie"
#endif /* WLAN_SUPPORT */

struct nrf_wifi_drv_priv_lnx rpu_drv_priv;

#ifdef WLAN_SUPPORT
#ifndef CONFIG_NRF700X_RADIO_TEST
struct nrf_wifi_fmac_vif_ctx_lnx *nrf_wifi_lnx_wlan_fmac_add_vif(struct nrf_wifi_ctx_lnx *rpu_ctx_lnx,
								const char *name,
								char *mac_addr,
								enum nl80211_iftype if_type)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
#ifdef HOST_CFG80211_SUPPORT
	struct wireless_dev *wdev = NULL;	

	/* Create a cfg80211 VIF */
	wdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);

	if(!wdev) {
		pr_err("%s: Unable to allocate memory for wdev\n", __func__);
		goto out;
	}

	wdev->wiphy = rpu_ctx_lnx->wiphy;
	wdev->iftype = if_type;
#endif /* HOST_CFG80211_SUPPORT */

	/* Create the interface and register it to the netdev stack */
	vif_ctx_lnx = nrf_wifi_netdev_add_vif(rpu_ctx_lnx,
						name,
#ifdef HOST_CFG80211_SUPPORT
						wdev,
#endif /* HOST_CFG80211_SUPPORT */
						mac_addr);					      

	if (!vif_ctx_lnx) {
		pr_err("%s: Failed to add interface to netdev stack\n", __func__);
#ifdef HOST_CFG80211_SUPPORT		
		kfree(wdev);
#endif /* HOST_CFG80211_SUPPORT */		
		goto out;
	}

#ifdef HOST_CFG80211_SUPPORT
	vif_ctx_lnx->wdev = wdev;
#endif /* HOST_CFG80211_SUPPORT */		

#ifdef notyet
	netif_carrier_off(vif_ctx_lnx->netdev);
#endif /* notyet */

out:
	return vif_ctx_lnx;
}


void nrf_wifi_lnx_wlan_fmac_del_vif(struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx)
{
	struct net_device *netdev = NULL;
#ifdef HOST_CFG80211_SUPPORT	
	struct wireless_dev *wdev = NULL;
#endif /* HOST_CFG80211_SUPPORT */

	netdev = vif_ctx_lnx->netdev;

	/* Unregister the default interface from the netdev stack */
	nrf_wifi_netdev_del_vif(netdev);

#ifdef HOST_CFG80211_SUPPORT
	wdev = vif_ctx_lnx->wdev;
	kfree(wdev);
#endif /* HOST_CFG80211_SUPPORT */	
}
#endif /* !CONFIG_NRF700X_RADIO_TEST */


#ifdef HOST_FW_LOAD_SUPPORT
static char *nrf_wifi_fmac_fw_loc_get_lnx(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					    enum nrf_wifi_fw_type fw_type,
					    enum nrf_wifi_fw_subtype fw_subtype)
{
	char *fw_loc = NULL;

	fw_loc = pal_ops_get_fw_loc(fmac_dev_ctx->fpriv->opriv,
				    fw_type,
				    fw_subtype);

	return fw_loc;
}


enum nrf_wifi_status nrf_wifi_fmac_fw_get_lnx(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
						 struct device *dev,
						 enum nrf_wifi_fw_type fw_type,
						 enum nrf_wifi_fw_subtype fw_subtype,
						 struct nrf_wifi_fw_info *fw_info)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_SUCCESS;
	const struct firmware *fw = NULL;
	char *fw_loc = NULL;
	const unsigned char *fw_data = NULL;
	unsigned int fw_data_size = 0;	
	int ret = -1;
#ifdef FW_PATCH_INBUILT
#ifdef HOST_FW_PATCH_LOAD_SUPPORT
	if (fw_type == NRF_WIFI_FW_TYPE_LMAC_PATCH) {
		if (fw_subtype == NRF_WIFI_FW_SUBTYPE_PRI) {
			fw_data = nrf_wifi_lmac_patch_pri;
			fw_data_size = sizeof(nrf_wifi_lmac_patch_pri);
		}
		else if (fw_subtype == NRF_WIFI_FW_SUBTYPE_SEC) {
			fw_data = nrf_wifi_lmac_patch_sec;
			fw_data_size = sizeof(nrf_wifi_lmac_patch_sec);
		} else {
			pr_err("%s: Invalid fw_subtype (%d)\n",
			       __func__,
			       fw_subtype);
		}
	} 
	else if (fw_type == NRF_WIFI_FW_TYPE_UMAC_PATCH) {
		if (fw_subtype == NRF_WIFI_FW_SUBTYPE_PRI) {
			fw_data = nrf_wifi_umac_patch_pri;
			fw_data_size = sizeof(nrf_wifi_umac_patch_pri);
		}
		else if (fw_subtype == NRF_WIFI_FW_SUBTYPE_SEC) {
			fw_data = nrf_wifi_umac_patch_sec;
			fw_data_size = sizeof(nrf_wifi_umac_patch_sec);
		} else {
			pr_err("%s: Invalid fw_subtype (%d)\n",
			       __func__,
			       fw_subtype);
		}
	} else {
#endif /* HOST_FW_PATCH_LOAD_SUPPORT */
#endif /* FW_PATCH_INBUILT */
		fw_loc = nrf_wifi_fmac_fw_loc_get_lnx(fmac_dev_ctx,
							fw_type,
							fw_subtype);

		if (!fw_loc) {
			/* Nothing to be downloaded */
			goto out;
		}
		
		ret = request_firmware(&fw,
				       fw_loc,
				       dev);

		if (ret) {
			pr_err("Failed to get %s, Error = %d\n",
			       fw_loc,
			       ret);
			fw = NULL;
#ifdef HOST_FW_PATCH_LOAD_SUPPORT
			/* It is possible that FW patches are not present, so not being
			 * able to get them is not a failure case */
			if ((fw_type != NRF_WIFI_FW_TYPE_LMAC_PATCH) &&
			    (fw_type != NRF_WIFI_FW_TYPE_UMAC_PATCH))
				status = NRF_WIFI_STATUS_FAIL;
#else
			status = NRF_WIFI_STATUS_FAIL;
#endif /* HOST_FW_PATCH_LOAD_SUPPORT */
			goto out;
		}

		fw_data = fw->data;
		fw_data_size = fw->size;
#ifdef FW_PATCH_INBUILT
#ifdef HOST_FW_PATCH_LOAD_SUPPORT
	}
#endif /* HOST_FW_PATCH_LOAD_SUPPORT */
#endif /* FW_PATCH_INBUILT */

	//fw_info->data = kzalloc(fw_data_size, GFP_KERNEL);
	fw_info->data = vmalloc(fw_data_size);

	if(!fw_info->data) {
		pr_err("%s: Unable to allocate memory\n",
		       __func__);
		status = NRF_WIFI_STATUS_FAIL;
		goto out;
	}

	memcpy(fw_info->data,
	       fw_data,
	       fw_data_size);

	fw_info->size = fw_data_size;

out:
	if(fw)
		release_firmware(fw);

	return status;
}


void nrf_wifi_lnx_wlan_fw_rel(struct nrf_wifi_fw_info *fw_info)
{
	if (fw_info->data)
//		kfree(fw_info->data);
		vfree(fw_info->data);		

	fw_info->data = 0;
}


enum nrf_wifi_status nrf_wifi_lnx_wlan_fmac_fw_load(struct nrf_wifi_ctx_lnx *rpu_ctx_lnx,
						  struct device *dev)
{
	struct nrf_wifi_fmac_fw_info fw_info;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	memset(&fw_info, 0, sizeof(fw_info));

#ifdef HOST_FW_HEX_LOAD_SUPPORT
	/* Get the UMAC FW */
	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,			
					    NRF_WIFI_FW_TYPE_UMAC_HEX,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.umac_hex);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get LMAC FW\n", __func__);
		goto out;
	}
	
	/* Now get the LMAC FW */
	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_LMAC_HEX,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.lmac_hex);			

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get UMAC FW\n", __func__);
		goto out;
	}
#else
#ifdef HOST_FW_RAM_LOAD_SUPPORT
	/* Get the LMAC FW */
	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,			
					    NRF_WIFI_FW_TYPE_LMAC_RAM,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.lmac_ram);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get LMAC FW\n", __func__);
		goto out;
	}

	/* Now get the UMAC FW */
	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_UMAC_RAM,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.umac_ram);			

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get UMAC FW\n", __func__);
		goto out;
	}
#else /* HOST_FW_RAM_LOAD_SUPPORT */
#ifdef HOST_FW_ROM_LOAD_SUPPORT
	/* Get the LMAC ROM */
	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_LMAC_ROM,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.lmac_rom);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get LMAC ROM\n", __func__);
		goto out;
	}

	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_UMAC_ROM,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.umac_rom);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get UMAC ROM\n", __func__);
		goto out;
	}
#endif /* HOST_FW_ROM_LOAD_SUPPORT */

#ifdef HOST_FW_PATCH_LOAD_SUPPORT
	/* Get the LMAC patches */
	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_LMAC_PATCH,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.lmac_patch_pri);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get LMAC BIMG patch\n", __func__);
		goto out;
	}

	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_LMAC_PATCH,
					    NRF_WIFI_FW_SUBTYPE_SEC,
					    &fw_info.lmac_patch_sec);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get LMAC BIN patch\n", __func__);
		goto out;
	}

	/* Get the UMAC patches */
	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_UMAC_PATCH,
					    NRF_WIFI_FW_SUBTYPE_PRI,
					    &fw_info.umac_patch_pri);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get UMAC BIMG patch\n", __func__);
		goto out;
	}

	status = nrf_wifi_fmac_fw_get_lnx(rpu_ctx_lnx->rpu_ctx,
					    dev,
					    NRF_WIFI_FW_TYPE_UMAC_PATCH,
					    NRF_WIFI_FW_SUBTYPE_SEC,
					    &fw_info.umac_patch_sec);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Unable to get UMAC BIN patch\n", __func__);
		goto out;
	}
#endif /* HOST_FW_PATCH_LOAD_SUPPORT */
#endif /* !HOST_FW_RAM_LOAD_SUPPORT */
#endif /* HOST_FW_HEX_LOAD_SUPPORT */

	/* Load the FW's to the RPU */
	status = nrf_wifi_fmac_fw_load(rpu_ctx_lnx->rpu_ctx,
					 &fw_info);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: nrf_wifi_fmac_fw_load failed\n", __func__);
		goto out;
	}

	status = NRF_WIFI_STATUS_SUCCESS;

out:
#ifdef HOST_FW_HEX_LOAD_SUPPORT
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.umac_hex);
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.lmac_hex);
#else /* HOST_FW_RAM_LOAD_SUPPORT */
#ifdef HOST_FW_RAM_LOAD_SUPPORT
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.lmac_ram);
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.umac_ram);
#else /* HOST_FW_RAM_LOAD_SUPPORT */
#ifdef HOST_FW_ROM_LOAD_SUPPORT
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.lmac_rom);
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.umac_rom);
#endif /* HOST_FW_ROM_LOAD_SUPPORT */
#ifdef HOST_FW_PATCH_LOAD_SUPPORT
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.lmac_patch_pri);
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.lmac_patch_sec);
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.umac_patch_pri);
	nrf_wifi_lnx_wlan_fw_rel(&fw_info.umac_patch_sec);
#endif /* HOST_FW_PATCH_LOAD_SUPPORT */
#endif /* !HOST_FW_RAM_LOAD_SUPPORT */
#endif /* !HOST_FW_HEX_LOAD_SUPPORT */

	return status;
}
#endif /* HOST_FW_LOAD_SUPPORT */


struct nrf_wifi_ctx_lnx *nrf_wifi_fmac_dev_add_lnx(void)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;
	void *rpu_ctx = NULL;
	struct device *dev = NULL;
#ifdef HOST_CFG80211_SUPPORT
	struct wiphy *wiphy = NULL;
#else
	int err = 0;
#endif
	unsigned char i = 0;

	while ((!rpu_drv_priv.drv_init) && (i < 5)) {
		pr_err("%s: Driver not yet initialized\n", __func__);
		i++;
		msleep(10);
	}

	if (i == 5) {
		pr_err("%s: Driver init failed\n", __func__);
		goto out;
	}

#ifdef RPU_CONFIG_FMAC
#ifdef HOST_CFG80211_SUPPORT	
	/* Register the RPU to the cfg80211 stack */
	wiphy = cfg80211_if_init();

	if(!wiphy) {	
		pr_err("%s: cfg80211_if_init failed\n", __func__);
		goto out;
	}

	rpu_ctx_lnx = wiphy_priv(wiphy);

	rpu_ctx_lnx->wiphy = wiphy;
	dev = &wiphy->dev;
#else
	rpu_ctx_lnx = kzalloc(sizeof(*rpu_ctx_lnx), GFP_KERNEL);

	if(!rpu_ctx_lnx) {
		pr_err("%s: Unable to allocate rpu_ctx_lnx\n", __func__);
		goto out;
	}

	err = dev_set_name(&rpu_ctx_lnx->dev, "phy" "%d", 100);

	if (err < 0) {
		pr_err("%s: dev_set_name failed\n", __func__);
		kfree(rpu_ctx_lnx);
		rpu_ctx_lnx = NULL;
		goto out;
	}

	device_initialize(&rpu_ctx_lnx->dev);

	rtnl_lock();

	err = device_add(&rpu_ctx_lnx->dev);

	if (err) {
		rtnl_unlock();
		kfree(rpu_ctx_lnx);
		rpu_ctx_lnx = NULL;
		goto out;
	}

	rtnl_unlock();

	dev = &rpu_ctx_lnx->dev;
#endif /* HOST_CFG80211_SUPPORT */

	INIT_LIST_HEAD(&rpu_ctx_lnx->cookie_list);

	rpu_ctx = nrf_wifi_fmac_dev_add(rpu_drv_priv.fmac_priv,
					  rpu_ctx_lnx);

	if (!rpu_ctx) {
		pr_err("%s: nrf_wifi_fmac_dev_add failed\n", __func__);
#ifdef HOST_CFG80211_SUPPORT
		cfg80211_if_deinit(wiphy);
#else
		kfree(rpu_ctx_lnx);
#endif /* HOST_CFG80211_SUPPORT */		
		rpu_ctx_lnx = NULL;
		goto out;
	}

	rpu_ctx_lnx->rpu_ctx = rpu_ctx;

#if defined(HOST_FW_LOAD_SUPPORT) || defined(HOST_FW_HEX_LOAD_SUPPORT)
	/* Load the firmware to the RPU */
	status = nrf_wifi_lnx_wlan_fmac_fw_load(rpu_ctx_lnx,
				  		dev);
#else
	status = nrf_wifi_fmac_fw_chk_boot(rpu_ctx);
#endif /* HOST_FW_LOAD_SUPPORT */

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: FW is not booted up\n", __func__);
#ifdef HOST_CFG80211_SUPPORT
		cfg80211_if_deinit(wiphy);
#else
		kfree(rpu_ctx_lnx);
#endif /* HOST_CFG80211_SUPPORT */		
		rpu_ctx_lnx = NULL;
		goto out;
	}

#endif /* RPU_CONFIG_FMAC */
out:
#ifndef HOST_CFG80211_SUPPORT
	rpu_drv_priv.rpu_ctx_lnx[rpu_drv_priv.num_rpu] = rpu_ctx_lnx;
	rpu_drv_priv.num_rpu++;
#endif /* !HOST_CFG80211_SUPPORT */	

	return rpu_ctx_lnx;
}


void nrf_wifi_fmac_dev_rem_lnx(struct nrf_wifi_ctx_lnx *rpu_ctx_lnx)
{
#ifdef RPU_CONFIG_FMAC
#ifdef HOST_CFG80211_SUPPORT
	cfg80211_if_deinit(rpu_ctx_lnx->wiphy);
	nrf_wifi_fmac_dev_rem(rpu_ctx_lnx->rpu_ctx);
#else
	rtnl_lock();

	device_del(&rpu_ctx_lnx->dev);

	rtnl_unlock();

	nrf_wifi_fmac_dev_rem(rpu_ctx_lnx->rpu_ctx);

	kfree(rpu_ctx_lnx);

	rpu_drv_priv.num_rpu--;
	rpu_drv_priv.rpu_ctx_lnx[rpu_drv_priv.num_rpu] = NULL;

#endif /* HOST_CFG80211_SUPPORT */
#endif /* RPU_CONFIG_FMAC */
}


void configure_tx_pwr_settings(
        struct nrf_wifi_tx_pwr_ctrl_params *tx_pwr_ctrl_params,
        struct nrf_wifi_tx_pwr_ceil_params *tx_pwr_ceil_params)
{
	tx_pwr_ctrl_params->ant_gain_2g = CONFIG_NRF700X_ANT_GAIN_2G;
	tx_pwr_ctrl_params->ant_gain_5g_band1 = CONFIG_NRF700X_ANT_GAIN_5G_BAND1;
        tx_pwr_ctrl_params->ant_gain_5g_band2 = CONFIG_NRF700X_ANT_GAIN_5G_BAND2;
        tx_pwr_ctrl_params->ant_gain_5g_band3 = CONFIG_NRF700X_ANT_GAIN_5G_BAND3;
        tx_pwr_ctrl_params->band_edge_2g_lo_dss = CONFIG_NRF700X_BAND_2G_LOWER_EDGE_BACKOFF_DSSS;
        tx_pwr_ctrl_params->band_edge_2g_lo_ht = CONFIG_NRF700X_BAND_2G_LOWER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_2g_lo_he = CONFIG_NRF700X_BAND_2G_LOWER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_2g_hi_dsss = CONFIG_NRF700X_BAND_2G_UPPER_EDGE_BACKOFF_DSSS;
        tx_pwr_ctrl_params->band_edge_2g_hi_ht = CONFIG_NRF700X_BAND_2G_UPPER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_2g_hi_he = CONFIG_NRF700X_BAND_2G_UPPER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_1_lo_ht =
                CONFIG_NRF700X_BAND_UNII_1_LOWER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_1_lo_he =
                CONFIG_NRF700X_BAND_UNII_1_LOWER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_1_hi_ht =
                CONFIG_NRF700X_BAND_UNII_1_UPPER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_1_hi_he =
                CONFIG_NRF700X_BAND_UNII_1_UPPER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_2a_lo_ht =
                CONFIG_NRF700X_BAND_UNII_2A_LOWER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_2a_lo_he =
                CONFIG_NRF700X_BAND_UNII_2A_LOWER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_2a_hi_ht =
                CONFIG_NRF700X_BAND_UNII_2A_UPPER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_2a_hi_he =
                CONFIG_NRF700X_BAND_UNII_2A_UPPER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_2c_lo_ht =
                CONFIG_NRF700X_BAND_UNII_2C_LOWER_EDGE_BACKOFF_HT;

	tx_pwr_ctrl_params->band_edge_5g_unii_2c_lo_he =
                CONFIG_NRF700X_BAND_UNII_2C_LOWER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_2c_hi_ht =
                CONFIG_NRF700X_BAND_UNII_2C_UPPER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_2c_hi_he =
                CONFIG_NRF700X_BAND_UNII_2C_UPPER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_3_lo_ht =
                CONFIG_NRF700X_BAND_UNII_3_LOWER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_3_lo_he =
                CONFIG_NRF700X_BAND_UNII_3_LOWER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_3_hi_ht =
                CONFIG_NRF700X_BAND_UNII_3_UPPER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_3_hi_he =
                CONFIG_NRF700X_BAND_UNII_3_UPPER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_4_lo_ht =
                CONFIG_NRF700X_BAND_UNII_4_LOWER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_4_lo_he =
                CONFIG_NRF700X_BAND_UNII_4_LOWER_EDGE_BACKOFF_HE;
        tx_pwr_ctrl_params->band_edge_5g_unii_4_hi_ht =
                CONFIG_NRF700X_BAND_UNII_4_UPPER_EDGE_BACKOFF_HT;
        tx_pwr_ctrl_params->band_edge_5g_unii_4_hi_he =
                CONFIG_NRF700X_BAND_UNII_4_UPPER_EDGE_BACKOFF_HE;

        tx_pwr_ceil_params->max_pwr_2g_dsss = 0x54;
        /*       DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_dsss);*/

        tx_pwr_ceil_params->max_pwr_2g_mcs7 = 0x40;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_mcs7); */

        tx_pwr_ceil_params->max_pwr_2g_mcs0 = 0x40;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_mcs0); */
#ifndef CONFIG_NRF70_2_4G_ONLY
        tx_pwr_ceil_params->max_pwr_5g_low_mcs7 = 0x24;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_low_mcs7);*/

        tx_pwr_ceil_params->max_pwr_5g_mid_mcs7 = 0x2c;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_mid_mcs7);*/

        tx_pwr_ceil_params->max_pwr_5g_high_mcs7 = 0x34;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_high_mcs7); */

        tx_pwr_ceil_params->max_pwr_5g_low_mcs0 = 0x24;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_low_mcs0);*/

        tx_pwr_ceil_params->max_pwr_5g_mid_mcs0 = 0x2c;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_mid_mcs0); */

        tx_pwr_ceil_params->max_pwr_5g_high_mcs0 = 0x34;
        /*        DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_high_mcs0);*/
#endif /* CONFIG_NRF70_2_4G_ONLY */
#if 0
        tx_pwr_ctrl_params->ant_gain_2g = CONFIG_NRF700X_ANT_GAIN_2G;
        tx_pwr_ctrl_params->ant_gain_5g_band1 =
                CONFIG_NRF700X_ANT_GAIN_5G_BAND1;
        tx_pwr_ctrl_params->ant_gain_5g_band2 =
                CONFIG_NRF700X_ANT_GAIN_5G_BAND2;
        tx_pwr_ctrl_params->ant_gain_5g_band3 =
                CONFIG_NRF700X_ANT_GAIN_5G_BAND3;
        tx_pwr_ctrl_params->band_edge_2g_lo =
                CONFIG_NRF700X_BAND_2G_LOWER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_2g_hi =
                CONFIG_NRF700X_BAND_2G_UPPER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_1_lo =
                CONFIG_NRF700X_BAND_UNII_1_LOWER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_1_hi =
                CONFIG_NRF700X_BAND_UNII_1_UPPER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_2a_lo =
                CONFIG_NRF700X_BAND_UNII_2A_LOWER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_2a_hi =
                CONFIG_NRF700X_BAND_UNII_2A_UPPER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_2c_lo =
                CONFIG_NRF700X_BAND_UNII_2C_LOWER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_2c_hi =
                CONFIG_NRF700X_BAND_UNII_2C_UPPER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_3_lo =
                CONFIG_NRF700X_BAND_UNII_3_LOWER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_3_hi =
                CONFIG_NRF700X_BAND_UNII_3_UPPER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_4_lo =
                CONFIG_NRF700X_BAND_UNII_4_LOWER_EDGE_BACKOFF;
        tx_pwr_ctrl_params->band_edge_5g_unii_4_hi =
                CONFIG_NRF700X_BAND_UNII_4_UPPER_EDGE_BACKOFF;

#if defined(CONFIG_BOARD_NRF7002DK_NRF7001_NRF5340_CPUAPP) || \
        defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP) ||     \
        defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)
        set_tx_pwr_ceil_default(tx_pwr_ceil_params);
#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_dsss)
        tx_pwr_ceil_params->max_pwr_2g_dsss =
                DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_dsss);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_mcs7)
        tx_pwr_ceil_params->max_pwr_2g_mcs7 =
                DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_mcs7);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_mcs0)
        tx_pwr_ceil_params->max_pwr_2g_mcs0 =
                DT_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_2g_mcs0);
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(nrf70_tx_power_ceiling))
        tx_pwr_ceil_params->rf_tx_pwr_ceil_params_override = 1;
#else
        tx_pwr_ceil_params->rf_tx_pwr_ceil_params_override = 0;
#endif
#endif
#if defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP) || \
        defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_low_mcs7)
        tx_pwr_ceil_params->max_pwr_5g_low_mcs7 = DT_PROP(
                DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_low_mcs7);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_mid_mcs7)
        tx_pwr_ceil_params->max_pwr_5g_mid_mcs7 = DT_PROP(
                DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_mid_mcs7);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_high_mcs7)
        tx_pwr_ceil_params->max_pwr_5g_high_mcs7 = DT_PROP(
                DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_high_mcs7);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_low_mcs0)
        tx_pwr_ceil_params->max_pwr_5g_low_mcs0 = DT_PROP(
                DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_low_mcs0);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_mid_mcs0)
        tx_pwr_ceil_params->max_pwr_5g_mid_mcs0 = DT_PROP(
                DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_mid_mcs0);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_high_mcs0)
        tx_pwr_ceil_params->max_pwr_5g_high_mcs0 = DT_PROP(
                DT_NODELABEL(nrf70_tx_power_ceiling), max_pwr_5g_high_mcs0);
#endif
#endif
#endif
#endif

}

enum nrf_wifi_status nrf_wifi_fmac_dev_init_lnx(struct nrf_wifi_ctx_lnx *rpu_ctx_lnx)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
#ifndef CONFIG_NRF700X_RADIO_TEST

#ifdef SOC_WEZEN
	char default_mac_addr[NRF_WIFI_ETH_ADDR_LEN];
#else
	unsigned char base_mac_addr[NRF_WIFI_ETH_ADDR_LEN];
#endif /* SOC_WEZEN */
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
	struct nrf_wifi_umac_add_vif_info add_vif_info;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
#endif /* !CONFIG_NRF700X_RADIO_TEST */

#if defined(CONFIG_BOARD_NRF7001)
        enum op_band op_band = BAND_24G;
#else /* CONFIG_BOARD_NRF7001 */
        enum op_band op_band = BAND_ALL;
#endif /* CONFIG_BOARD_NRF7001 */

#ifdef CONFIG_NRF_WIFI_LOW_POWER
        int sleep_type = -1;
#ifndef CONFIG_NRF700X_RADIO_TEST
        sleep_type = HW_SLEEP_ENABLE;
#else
        sleep_type = SLEEP_DISABLE;
#endif /* CONFIG_NRF700X_RADIO_TEST */
#endif /* CONFIG_NRF_WIFI_LOW_POWER */

	unsigned int fw_ver = 0;
	struct nrf_wifi_tx_pwr_ctrl_params tx_pwr_ctrl_params;
	struct nrf_wifi_tx_pwr_ceil_params tx_pwr_ceil_params;

#ifndef CONFIG_NRF700X_RADIO_TEST

#ifndef SOC_WEZEN
	status = nrf_wifi_fmac_otp_mac_addr_get(rpu_ctx_lnx->rpu_ctx, 0,

                                                base_mac_addr);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                pr_err("%s: Fetching of MAC address from OTP failed\n",
                       __func__);
                goto out;
        }

#endif /* SOC_WEZEN */
        fmac_dev_ctx = rpu_ctx_lnx->rpu_ctx;

	if (nrf_wifi_utils_hex_str_to_val(fmac_dev_ctx->fpriv->opriv,
#ifdef SOC_WEZEN
					  default_mac_addr,
#else
					  base_mac_addr,
#endif /* SOC_WEZEN */
                                          sizeof(default_mac_addr),
                                          base_mac_addr) == -1) {
                pr_err("%s: hex_str_to_val failed\n", __func__);
                goto out;
        }

	/* Create a default interface and register it to the netdev stack.
	 * We need to take a rtnl_lock since the netdev stack expects it. In the
	 * regular case this lock is taken by the cfg80211, but in the case of
	 * the default interface we need to take it since we are initiating the
	 * creation of the interface */
	rtnl_lock();

	vif_ctx_lnx = nrf_wifi_lnx_wlan_fmac_add_vif(rpu_ctx_lnx,
#ifdef HOST_CFG80211_SUPPORT
						  "nrf_wifi",
#else
						  "wlan0",
#endif
#ifdef SOC_WEZEN
						  default_mac_addr,
#else
						  base_mac_addr,
#endif /* SOC_WEZEN */
						  NL80211_IFTYPE_STATION);

	rtnl_unlock();

	if (!vif_ctx_lnx) {
		pr_err("%s: Unable to register default interface to stack\n",
		       __func__);
		goto out;
	}

	memset(&add_vif_info,
	       0,
	       sizeof(add_vif_info));

	add_vif_info.iftype = NL80211_IFTYPE_STATION;

	memcpy(add_vif_info.ifacename,
	       "wlan0",
	       strlen("wlan0"));

	ether_addr_copy(add_vif_info.mac_addr,
#ifdef SOC_WEZEN
			default_mac_addr
#else
			base_mac_addr
#endif /* SOC_WEZEN */
			);

	vif_ctx_lnx->if_idx = nrf_wifi_fmac_add_vif(rpu_ctx_lnx->rpu_ctx,
						    vif_ctx_lnx,
						    &add_vif_info);

	if (vif_ctx_lnx->if_idx != 0) {
		pr_err("%s: FMAC returned non 0 index for default interface\n", __func__);
		goto out;
	}

	rpu_ctx_lnx->def_vif_ctx = vif_ctx_lnx;

#ifdef CONFIG_NRF_WIFI_LOW_POWER
	if (!sleep_type) {
		sleep_type = SLEEP_DISABLE;
	} else if (memcmp(sleep_type, "SW", 2) == 0) {
		sleep_type = SW_SLEEP_ENABLE;
	} else if (memcmp(sleep_type, "HW", 2) == 0) {
		sleep_type = HW_SLEEP_ENABLE;
	}
#endif
#endif /* !CONFIG_NRF700X_RADIO_TEST */


#ifdef CONF_SUPPORT
	nrf_wifi_lnx_wlan_fmac_conf_init(&rpu_ctx_lnx->conf_params);
#endif /* CONF_SUPPORT */

	status = nrf_wifi_lnx_wlan_fmac_dbgfs_init(rpu_ctx_lnx);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Failed to create wlan entry in DebugFS\n",
		       __func__);
		goto out;
	}

	status = nrf_wifi_fmac_ver_get(rpu_ctx_lnx->rpu_ctx,
                                       &fw_ver);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
                pr_err("%s: nrf_wifi_fmac_ver_get failed\n", __func__);
                goto out;
        }

	pr_info("Firmware (v%d.%d.%d.%d) booted successfully\n",
                NRF_WIFI_UMAC_VER(fw_ver),
                NRF_WIFI_UMAC_VER_MAJ(fw_ver),
                NRF_WIFI_UMAC_VER_MIN(fw_ver),
                NRF_WIFI_UMAC_VER_EXTRA(fw_ver));

	configure_tx_pwr_settings(&tx_pwr_ctrl_params, &tx_pwr_ceil_params);

	status = nrf_wifi_fmac_dev_init(rpu_ctx_lnx->rpu_ctx,
					/*NULL,*/NRF_WIFI_DEF_RF_PARAMS,
#ifdef CONFIG_NRF_WIFI_LOW_POWER
                                        sleep_type,
#endif /* CONFIG_NRF_WIFI_LOW_POWER */
                                        NRF_WIFI_DEF_PHY_CALIB,
                                        op_band,
					CONFIG_NRF_WIFI_BEAMFORMING,
                                        &tx_pwr_ctrl_params,
					&tx_pwr_ceil_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: nrf_wifi_fmac_dev_init failed\n",
		       __func__);
		goto out;
	}

#ifndef CONFIG_NRF700X_RADIO_TEST
	status = nrf_wifi_fmac_set_vif_macaddr(
		rpu_ctx_lnx->rpu_ctx, vif_ctx_lnx->if_idx,
#ifdef SOC_WEZEN
		default_mac_addr
#else
		base_mac_addr
#endif /* SOC_WEZEN */
		);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: MAC address change failed\n", __func__);
		goto out;
	}
#endif /* !CONFIG_NRF700X_RADIO_TEST */
out:
#ifndef CONFIG_NRF700X_RADIO_TEST
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		if (vif_ctx_lnx) {
			/* Remove the default interface and unregister it from the netdev stack.
			 * We need to take a rtnl_lock since the netdev stack expects it. In the
			 * regular case this lock is taken by the cfg80211, but in the case of
			 * the default interface we need to take it since we are initiating the
			 * deletion of the interface */
			rtnl_lock();

			nrf_wifi_lnx_wlan_fmac_del_vif(vif_ctx_lnx);

			rtnl_unlock();
		}
	}
#endif /* !CONFIG_NRF700X_RADIO_TEST */

	return status;
}


void nrf_wifi_fmac_dev_deinit_lnx(struct nrf_wifi_ctx_lnx *rpu_ctx_lnx)
{
#ifndef CONFIG_NRF700X_RADIO_TEST
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct nrf_wifi_fmac_dev_ctx_def *def_dev_ctx = NULL;
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
	unsigned char if_idx = MAX_NUM_VIFS;
	unsigned char i = 0;
	
	fmac_dev_ctx = rpu_ctx_lnx->rpu_ctx;
	def_dev_ctx = wifi_dev_priv(fmac_dev_ctx);

	/* Delete all other interfaces */
	for (i = 1; i < MAX_NUM_VIFS; i++) {
		if (def_dev_ctx->vif_ctx[i]) {
			vif_ctx_lnx = (struct nrf_wifi_fmac_vif_ctx_lnx *)
				(def_dev_ctx->vif_ctx[i]->os_vif_ctx);
			if_idx = vif_ctx_lnx->if_idx;
			rtnl_lock();
			nrf_wifi_lnx_wlan_fmac_del_vif(vif_ctx_lnx);
			rtnl_unlock();
			nrf_wifi_fmac_del_vif(fmac_dev_ctx,
						if_idx);
			def_dev_ctx->vif_ctx[i] = NULL;
		}
	}

	/* Delete the default interface*/
	vif_ctx_lnx = rpu_ctx_lnx->def_vif_ctx;
	fmac_dev_ctx = rpu_ctx_lnx->rpu_ctx;
	if_idx = vif_ctx_lnx->if_idx;
#endif /* !CONFIG_NRF700X_RADIO_TEST */

	nrf_wifi_fmac_dev_deinit(rpu_ctx_lnx->rpu_ctx);

	nrf_wifi_lnx_wlan_fmac_dbgfs_deinit(rpu_ctx_lnx);

#ifndef CONFIG_NRF700X_RADIO_TEST
	/* Remove the default interface and unregister it from the netdev stack.
	 * We need to take a rtnl_lock since the netdev stack expects it. In the
	 * regular case this lock is taken by the cfg80211, but in the case of
	 * the default interface we need to take it since we are initiating the
	 * deletion of the interface */
	rtnl_lock();

	nrf_wifi_lnx_wlan_fmac_del_vif(vif_ctx_lnx);

	rtnl_unlock();

	nrf_wifi_fmac_del_vif(fmac_dev_ctx,
				if_idx);


	rpu_ctx_lnx->def_vif_ctx = NULL;
#endif /* !CONFIG_NRF700X_RADIO_TEST */
}


void nrf_wifi_chnl_get_callbk_fn(void *os_vif_ctx,
				   struct nrf_wifi_umac_event_get_channel *info,
				   unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	pr_err("%s: Processing\n",
	       __func__);

	vif_ctx_lnx = os_vif_ctx;

	vif_ctx_lnx->chan_def = kmalloc(sizeof(struct nrf_wifi_chan_definition),
				       	GFP_KERNEL);

	memcpy(vif_ctx_lnx->chan_def,
	       &info->chan_def, 
	       sizeof(struct nrf_wifi_chan_definition));
}


void nrf_wifi_tx_pwr_get_callbk_fn(void *os_vif_ctx,
				     struct nrf_wifi_umac_event_get_tx_power *info,
				     unsigned int event_len)
{

	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	pr_err("%s: Processing\n",
	       __func__);

	vif_ctx_lnx = os_vif_ctx;

	vif_ctx_lnx->tx_power = info->txpwr_level;
	vif_ctx_lnx->event_tx_power = 1;
}


void nrf_wifi_get_station_callbk_fn(void *os_vif_ctx,
				  struct nrf_wifi_umac_event_new_station *info,
				  unsigned int event_len)
{

	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	pr_err("%s: Processing\n",
	       __func__);

	vif_ctx_lnx = os_vif_ctx;

	vif_ctx_lnx->station_info = kmalloc(sizeof(struct nrf_wifi_sta_info),
		       			    GFP_KERNEL);

	memcpy(vif_ctx_lnx->station_info,
	       &info->sta_info, 
	       sizeof(struct nrf_wifi_sta_info));
}


void nrf_wifi_disp_scan_res_callbk_fn(void *os_vif_ctx,
					struct nrf_wifi_umac_event_new_scan_display_results *scan_res,
					unsigned int event_len,
					bool more_res)
{






}


#ifndef CONFIG_NRF700X_RADIO_TEST
void nrf_wifi_cookie_rsp_callbk_fn(void *os_vif_ctx,
				     struct nrf_wifi_umac_event_cookie_rsp *cookie_rsp,
				     unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;
	struct cookie_info *cookie_info = NULL;

	vif_ctx_lnx = os_vif_ctx;
	rpu_ctx_lnx = vif_ctx_lnx->rpu_ctx;

	cookie_info = kzalloc(sizeof(*cookie_info),
			      GFP_KERNEL);

	if (!cookie_info) {
		pr_err("%s: Unable to allocate memory for cookie_info\n",
		       __func__);
		return;
	}

	cookie_info->host_cookie = cookie_rsp->host_cookie;
	cookie_info->rpu_cookie	= cookie_rsp->cookie;

	list_add(&cookie_info->list, &rpu_ctx_lnx->cookie_list);	
}
#endif /* !CONFIG_NRF700X_RADIO_TEST */

#ifdef CONFIG_NRF700X_STA_MODE
static void nrf_wifi_process_rssi_from_rx(void *os_vif_ctx, signed short signal)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	vif_ctx_lnx = os_vif_ctx;
	if (!vif_ctx_lnx) {
		pr_err("%s: vif_ctx_lnx is NULL\n", __func__);
		return;
	}
	rpu_ctx_lnx = vif_ctx_lnx->rpu_ctx;
	if (!rpu_ctx_lnx) {
		pr_err("%s: rpu_ctx_lnx is NULL\n", __func__);
		return;
	}
	fmac_dev_ctx = rpu_ctx_lnx->rpu_ctx;
	vif_ctx_lnx->rssi = MBM_TO_DBM(signal);
	vif_ctx_lnx->rssi_record_timestamp_us =
		nrf_wifi_osal_time_get_curr_us(fmac_dev_ctx->fpriv->opriv);
}
#endif /* CONFIG_NRF700X_STA_MODE */

void nrf_wifi_set_if_callbk_fn(void *os_vif_ctx,
				 struct nrf_wifi_umac_event_set_interface *set_if_event,
				 unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	vif_ctx_lnx->event_set_if = 1;
	vif_ctx_lnx->status_set_if = set_if_event->return_value;
}

#ifdef TWT_SUPPORT
void nrf_wifi_twt_cfg_callbk_fn(void *os_vif_ctx,
				  struct nrf_wifi_umac_cmd_config_twt *twt_cfg_event,
				  unsigned int event_len)
{
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;
	rpu_ctx_lnx = vif_ctx_lnx->rpu_ctx;

	rpu_ctx_lnx->twt_params.twt_event.twt_flow_id =
		twt_cfg_event->info.twt_flow_id;
	rpu_ctx_lnx->twt_params.twt_event.neg_type =
		twt_cfg_event->info.neg_type;
	rpu_ctx_lnx->twt_params.twt_event.setup_cmd =
		twt_cfg_event->info.setup_cmd;
	rpu_ctx_lnx->twt_params.twt_event.ap_trigger_frame =
		twt_cfg_event->info.ap_trigger_frame;
	rpu_ctx_lnx->twt_params.twt_event.is_implicit =
		twt_cfg_event->info.is_implicit;
	rpu_ctx_lnx->twt_params.twt_event.twt_flow_type =
		twt_cfg_event->info.twt_flow_type;
	rpu_ctx_lnx->twt_params.twt_event.twt_target_wake_interval_exponent =
		twt_cfg_event->info.twt_target_wake_interval_exponent;
	rpu_ctx_lnx->twt_params.twt_event.twt_target_wake_interval_mantissa =
		twt_cfg_event->info.twt_target_wake_interval_mantissa;
	//TODO
	//twt_setup_cmd->info.target_wake_time =
	//    rpu_ctx_lnx->twt_params.target_wake_time;
	rpu_ctx_lnx->twt_params.twt_event.nominal_min_twt_wake_duration =
		twt_cfg_event->info.nominal_min_twt_wake_duration;
	rpu_ctx_lnx->twt_params.twt_event_info_avail = 1;
}


void nrf_wifi_twt_teardown_callbk_fn(void *os_vif_ctx,
				       struct nrf_wifi_umac_cmd_teardown_twt *twt_teardown_event,
				       unsigned int event_len)
{
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;
	rpu_ctx_lnx = vif_ctx_lnx->rpu_ctx;

	rpu_ctx_lnx->twt_params.twt_event.twt_flow_id =
		twt_teardown_event->info.twt_flow_id;

	rpu_ctx_lnx->twt_params.teardown_reason =
		twt_teardown_event->info.reason_code;
}
#endif /*TWT_SUPPORT */

#ifdef LOW_POWER
void nrf_wifi_twt_sleep_callbk_fn(void *os_vif_ctx,
			  struct nrf_wifi_umac_event_twt_sleep *twt_sleep_evnt,
			  unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
	
	vif_ctx_lnx = os_vif_ctx;
	
	switch(twt_sleep_evnt->info.type) {
	case TWT_BLOCK_TX:
		netif_stop_queue(vif_ctx_lnx->netdev);
	break;
	case TWT_UNBLOCK_TX:
		netif_wake_queue(vif_ctx_lnx->netdev);
	break;
	default:
	break;
    	}
}
#endif /* LOW_POWER */

#ifndef CONFIG_NRF700X_RADIO_TEST
#ifdef notyet
void nrf_wifi_coalesing_callbk_fn(void *os_fmac_ctx,
			struct nrf_wifi_umac_event_coalescing *coales_evnt,
			unsigned int event_len)
{
	int ac;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;

	fmac_dev_ctx = os_fmac_ctx;

	ac = nrf_wifi_util_get_ac(coales_evnt->tid,
				  coales_evnt->sta_addr);

	if (coales_evnt->coalescing) {
		if (fmac_dev_ctx->no_of_active_aggr_session[ac] == 0)
			fmac_dev_ctx->no_of_active_aggr_session[ac]++;
	} else if (fmac_dev_ctx->no_of_active_aggr_session[ac] > 0)
		fmac_dev_ctx->no_of_active_aggr_session[ac]--;
}
#endif
#else/* !CONFIG_NRF700X_RADIO_TEST */
void *nrf_wifi_pwr_dev_add_lnx(void *callbk_data,
			    void *rpu_ctx)
{
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;

	if ((!callbk_data) || (!rpu_ctx)) {
		pr_err("%s: Invalid params\n", __func__);
		goto out;
	}

	rpu_ctx_lnx = kzalloc(sizeof(*rpu_ctx_lnx), GFP_KERNEL);

	if (!rpu_ctx_lnx) {
		pr_err("%s: Unable to allocate context for RPU\n",
		       __func__);
		goto out;
	}

	rpu_ctx_lnx->rpu_ctx = rpu_ctx;
out:
	return rpu_ctx_lnx;
}


void nrf_wifi_pwr_dev_rem_lnx(void *rpu_ctx_lnx)
{
	kfree(rpu_ctx_lnx);
}


enum nrf_wifi_status nrf_wifi_pwr_dev_init_lnx(void *rpu_ctx)
{
	enum nrf_wifi_status status = NVLSI_PWR_STATUS_FAIL;
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;

	if (!rpu_ctx) {
		pr_err("%s: Invalid params\n", __func__);
		goto out;
	}

	rpu_ctx_lnx = (struct nrf_wifi_ctx_lnx *)rpu_ctx;

	status = nrf_wifi_lnx_pwr_dbgfs_init(rpu_ctx_lnx);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: Failed to create pwr entry in DebugFS\n",
		       __func__);
		goto out;
	}
out:
	return status;
}


void nrf_wifi_pwr_dev_deinit_lnx(void *rpu_ctx)
{
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;

	if (!rpu_ctx) {
		pr_err("%s: Invalid params\n", __func__);
		return;
	}

	rpu_ctx_lnx = (struct nrf_wifi_ctx_lnx *)rpu_ctx;

	nrf_wifi_lnx_pwr_dbgfs_deinit(rpu_ctx_lnx);
}
#endif /* WLAN_SUPPORT */


int __init nrf_wifi_init_lnx(void)
{
	int ret = -ENOMEM;
#ifdef WLAN_SUPPORT
#ifndef CONFIG_NRF700X_RADIO_TEST
	struct nrf_wifi_fmac_callbk_fns callbk_fns;
	struct nrf_wifi_data_config_params data_config;
	struct rx_buf_pool_params rx_buf_pools[MAX_NUM_OF_RX_QUEUES];
#endif /* !CONFIG_NRF700X_RADIO_TEST */
#endif /* WLAN_SUPPORT */
	ret = nrf_wifi_lnx_dbgfs_init();

	if (ret) {
		pr_err("%s: Failed to create root entry in DebugFS\n",
		       __func__);
		goto out;
	}

#ifdef WLAN_SUPPORT
#ifndef CONFIG_NRF700X_RADIO_TEST
	if(rf_params) {
		if (strlen(rf_params) != (NRF_WIFI_RF_PARAMS_SIZE * 2)) {
			pr_err("%s: Invalid length of rf_params. Should consist of %d hex characters\n",
			       __func__,
			       (NRF_WIFI_RF_PARAMS_SIZE * 2));

			goto out;
		}
	}

	data_config.aggregation = aggregation;
	data_config.wmm = wmm;
	data_config.max_num_tx_agg_sessions = max_num_tx_agg_sessions;
	data_config.max_num_rx_agg_sessions = max_num_rx_agg_sessions;
	data_config.max_tx_aggregation = max_tx_aggregation;
	data_config.reorder_buf_size = reorder_buf_size;
	data_config.max_rxampdu_size = max_rxampdu_size;
	data_config.rate_protection_type = rate_protection_type;

	rx_buf_pools[0].num_bufs = rx1_num_bufs;
	rx_buf_pools[1].num_bufs = rx2_num_bufs;
	rx_buf_pools[2].num_bufs = rx3_num_bufs;

	rx_buf_pools[0].buf_sz = rx1_buf_sz;
	rx_buf_pools[1].buf_sz = rx2_buf_sz;
	rx_buf_pools[2].buf_sz = rx3_buf_sz;
#endif /* !CONFIG_NRF700X_RADIO_TEST */

#ifdef RPU_CONFIG_FMAC
	rpu_drv_priv.drv_init = false;

#ifndef CONFIG_NRF700X_RADIO_TEST
	memset(&callbk_fns, 0, sizeof(callbk_fns));

	callbk_fns.if_carr_state_chg_callbk_fn = &nrf_wifi_netdev_if_state_chg_callbk_fn;
	callbk_fns.rx_frm_callbk_fn = &nrf_wifi_netdev_frame_rx_callbk_fn;
	callbk_fns.disp_scan_res_callbk_fn = &nrf_wifi_disp_scan_res_callbk_fn;
#ifdef CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS
	callbk_fns.rx_bcn_prb_resp_callbk_fn =
		&nrf_wifi_cfg80211_rx_bcn_prb_rsp_callbk_fn;
#endif /* CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS */
	callbk_fns.set_if_callbk_fn = &nrf_wifi_set_if_callbk_fn;
	callbk_fns.process_rssi_from_rx = &nrf_wifi_process_rssi_from_rx;
#ifdef HOST_CFG80211_SUPPORT	
	callbk_fns.get_station_callbk_fn = &nrf_wifi_get_station_callbk_fn;
	callbk_fns.chnl_get_callbk_fn = &nrf_wifi_chnl_get_callbk_fn;
	callbk_fns.tx_pwr_get_callbk_fn = &nrf_wifi_tx_pwr_get_callbk_fn;
	callbk_fns.cookie_rsp_callbk_fn = &nrf_wifi_cookie_rsp_callbk_fn;
	callbk_fns.scan_start_callbk_fn =
		&nrf_wifi_cfg80211_scan_start_callbk_fn;
	callbk_fns.scan_res_callbk_fn = &nrf_wifi_cfg80211_scan_res_callbk_fn;
	callbk_fns.scan_done_callbk_fn = &nrf_wifi_cfg80211_scan_done_callbk_fn;
	callbk_fns.scan_abort_callbk_fn = &nrf_wifi_cfg80211_scan_abort_callbk_fn;
	callbk_fns.auth_resp_callbk_fn = &nrf_wifi_cfg80211_auth_resp_callbk_fn;
	callbk_fns.assoc_resp_callbk_fn = &nrf_wifi_cfg80211_assoc_resp_callbk_fn;
	callbk_fns.deauth_callbk_fn = &nrf_wifi_cfg80211_deauth_callbk_fn;
	callbk_fns.disassoc_callbk_fn = &nrf_wifi_cfg80211_disassoc_callbk_fn;
	callbk_fns.mgmt_rx_callbk_fn = &nrf_wifi_cfg80211_mgmt_rx_callbk_fn;
	callbk_fns.unprot_mlme_mgmt_rx_callbk_fn = &nrf_wifi_cfg80211_unprot_mlme_mgmt_rx_callbk_fn;
	callbk_fns.roc_callbk_fn = &nrf_wifi_cfg80211_roc_callbk_fn;
	callbk_fns.roc_cancel_callbk_fn = &nrf_wifi_cfg80211_roc_cancel_callbk_fn;
	callbk_fns.tx_status_callbk_fn = &nrf_wifi_cfg80211_tx_status_callbk_fn;
	callbk_fns.mgmt_tx_status = &nrf_wifi_cfg80211_tx_status_callbk_fn;
#else
	callbk_fns.get_station_callbk_fn = &nrf_wifi_wpa_supp_get_station_callbk_fn;
	callbk_fns.cookie_rsp_callbk_fn = &nrf_wifi_wpa_supp_cookie_rsp_callbk_fn;
	callbk_fns.scan_start_callbk_fn = &nrf_wifi_wpa_supp_scan_start_callbk_fn;
	callbk_fns.scan_res_callbk_fn = &nrf_wifi_wpa_supp_scan_res_callbk_fn;
	callbk_fns.scan_done_callbk_fn = &nrf_wifi_wpa_supp_scan_done_callbk_fn;
	callbk_fns.scan_abort_callbk_fn = &nrf_wifi_wpa_supp_scan_abort_callbk_fn;
	callbk_fns.auth_resp_callbk_fn = &nrf_wifi_wpa_supp_auth_resp_callbk_fn;
	callbk_fns.assoc_resp_callbk_fn = &nrf_wifi_wpa_supp_assoc_resp_callbk_fn;
	callbk_fns.deauth_callbk_fn = &nrf_wifi_wpa_supp_deauth_callbk_fn;
	callbk_fns.disassoc_callbk_fn = &nrf_wifi_wpa_supp_disassoc_callbk_fn;
	callbk_fns.mgmt_rx_callbk_fn = &nrf_wifi_wpa_supp_mgmt_rx_callbk_fn;
	callbk_fns.unprot_mlme_mgmt_rx_callbk_fn = &nrf_wifi_wpa_supp_unprot_mlme_mgmt_rx_callbk_fn;
	callbk_fns.roc_callbk_fn = &nrf_wifi_wpa_supp_roc_callbk_fn;
	callbk_fns.roc_cancel_callbk_fn = &nrf_wifi_wpa_supp_roc_cancel_callbk_fn;
	callbk_fns.tx_status_callbk_fn = &nrf_wifi_wpa_supp_tx_status_callbk_fn;
	callbk_fns.get_interface_callbk_fn = &nrf_wifi_wpa_supp_new_if_callbk_fn;
	callbk_fns.event_get_wiphy = &nrf_wifi_wpa_supp_get_wiphy_callbk_fn;
	callbk_fns.event_get_reg = &nrf_wifi_wpa_supp_get_reg_callbk_fn;
	callbk_fns.set_if_callbk_fn = &nrf_wifi_wpa_supp_set_if_callbk_fn;
	callbk_fns.disp_scan_res_callbk_fn = &nrf_wifi_wpa_supp_disp_scan_res_callbk_fn;
	callbk_fns.mgmt_tx_status = &nrf_wifi_wpa_supp_tx_status_callbk_fn;
#endif /* HOST_CFG80211_SUPPORT */


#ifdef TWT_SUPPORT
	callbk_fns.twt_config_callbk_fn = &nrf_wifi_twt_cfg_callbk_fn;
	callbk_fns.twt_teardown_callbk_fn = &nrf_wifi_twt_teardown_callbk_fn;
#endif /* TWT_SUPPORT */	
#ifdef LOW_POWER
	callbk_fns.twt_sleep_callbk_fn = &nrf_wifi_twt_sleep_callbk_fn;
#endif
#ifdef notyet
	callbk_fns.coalescing_callbk_fn = &nrf_wifi_coalesing_callbk_fn;
#endif
#endif /* !CONFIG_NRF700X_RADIO_TEST */

	rpu_drv_priv.fmac_priv = nrf_wifi_fmac_init(
#ifndef CONFIG_NRF700X_RADIO_TEST
						      &data_config,
						      rx_buf_pools,
						      &callbk_fns
#endif /* !CONFIG_NRF700X_RADIO_TEST */
						     );

	if (rpu_drv_priv.fmac_priv == NULL) {
		pr_err("%s: nrf_wifi_fmac_init failed\n", __func__);
		goto out;
	}


	rpu_drv_priv.drv_init = true;
#ifndef CONFIG_NRF700X_RADIO_TEST
#ifndef HOST_CFG80211_SUPPORT
#ifndef CMD_DEMO
	ret = wpa_supp_if_init();

	if (ret) {
		pr_err("%s: wpa_supp_if_init failed\n", __func__);
		goto out;
	}
#endif
#endif /* HOST_CFG80211_SUPPORT */
#endif /* !CONFIG_NRF700X_RADIO_TEST */
#endif /* RPU_CONFIG_FMAC */
#else /* WLAN_SUPPORT */
	rpu_drv_priv.pwr_priv = nrf_wifi_pwr_init(&rpu_drv_priv,
					       &nrf_wifi_pwr_dev_add_lnx,
					       &nrf_wifi_pwr_dev_rem_lnx,
					       &nrf_wifi_pwr_dev_init_lnx,
					       &nrf_wifi_pwr_dev_deinit_lnx,
					       NRF_WIFI_PCIE_DRV_NAME,
					       DEFAULT_NRF_WIFI_PCI_VENDOR_ID,
					       PCI_ANY_ID,
					       DEFAULT_NRF_WIFI_PCI_DEVICE_ID,
					       PCI_ANY_ID);			

	if (rpu_drv_priv.pwr_priv == NULL) {
		pr_err("%s: nrf_wifi_pwr_init failed\n", __func__);
		goto out;
	}
#endif /* WLAN_SUPPORT */

#ifdef CONFIG_NRF700X_DATA_TX
	{
		struct nrf_wifi_fmac_priv_def *def_priv = NULL;

		def_priv = wifi_fmac_priv(rpu_drv_priv.fmac_priv);
		def_priv->max_ampdu_len_per_token =
#ifdef SOC_WEZEN
			(RPU_DATA_RAM_SIZE - (CONFIG_NRF700X_RX_NUM_BUFS *
#else
			(RPU_PKTRAM_SIZE - (CONFIG_NRF700X_RX_NUM_BUFS *
#endif
					    CONFIG_NRF700X_RX_MAX_DATA_SIZE)) /
			CONFIG_NRF700X_MAX_TX_TOKENS;
		/* Align to 4-byte */
		def_priv->max_ampdu_len_per_token &= ~0x3;

		/* Alignment overhead for size based coalesce */
		def_priv->avail_ampdu_len_per_token =
			def_priv->max_ampdu_len_per_token -
			(MAX_PKT_RAM_TX_ALIGN_OVERHEAD * max_tx_aggregation);
	}
#endif /* CONFIG_NRF700X_DATA_TX */

	ret = 0;
out:
	return ret;
}


void __exit nrf_wifi_deinit_lnx(void)
{
#ifdef WLAN_SUPPORT
#ifdef RPU_CONFIG_FMAC
#ifndef CONFIG_NRF700X_RADIO_TEST
#ifndef HOST_CFG80211_SUPPORT
#ifndef CMD_DEMO
	wpa_supp_if_deinit();
#endif
#endif /* HOST_CFG80211_SUPPORT */	
#endif /* !CONFIG_NRF700X_RADIO_TEST */
	nrf_wifi_fmac_deinit(rpu_drv_priv.fmac_priv);
#endif /* RPU_CONFIG_UMAC */
#else
	nrf_wifi_pwr_deinit(rpu_drv_priv.pwr_priv);
#endif /* WLAN_SUPPORT */
	nrf_wifi_lnx_dbgfs_deinit();
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nordic Semiconductor");
#ifdef WLAN_SUPPORT
MODULE_DESCRIPTION("FullMAC Driver for nRF WLAN Solution");
MODULE_VERSION(NRF_WIFI_FMAC_DRV_VER);
#else 
MODULE_DESCRIPTION("Power Monitor Driver for nRF Power IP");
MODULE_VERSION(NVLSI_PWR_DRV_VER);
#endif /* WLAN_SUPPORT */

module_init(nrf_wifi_init_lnx);
module_exit(nrf_wifi_deinit_lnx);

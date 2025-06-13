#ifndef HOST_CFG80211_SUPPORT
/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "host_rpu_umac_if.h"
#include "lnx_main.h"
#include "lnx_fmac_main.h"
#include "fmac_cmd.h"
#include "fmac_vif.h"
#include "netlink.h"
#ifdef SOC_WEZEN
#ifdef SOC_WEZEN_SECURE_DOMAIN
#include "fmac_api.h"
#endif
#endif

#ifdef TWT_SUPPORT
int twt_setup_event;
#endif
extern struct nrf_wifi_drv_priv_lnx rpu_drv_priv;

#ifndef CONFIG_NRF700X_RADIO_TEST
#ifdef CMD_DEMO
void umac_event_scan_done(void *vif_ctx)
{
	struct nrf_wifi_umac_cmd_get_scan_results *scan_results = NULL;
	int len = 0;
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = vif_ctx;
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = vif_ctx_lnx->rpu_ctx;

	if (rpu_ctx_lnx->connect_params.scan_type == CONNECT_SCAN) {
		len = sizeof(struct nrf_wifi_umac_cmd_get_scan_results);

		scan_results = kzalloc(len, GFP_KERNEL);

		scan_results->umac_hdr.cmd_evnt = NRF_WIFI_UMAC_CMD_GET_SCAN_RESULTS;
		scan_results->umac_hdr.ids.wdev_id = vif_ctx_lnx->if_idx;
		scan_results->umac_hdr.ids.valid_fields |= NRF_WIFI_INDEX_IDS_WDEV_ID_VALID;
		scan_results->scan_reason = SCAN_CONNECT;
		umac_cmd_cfg(rpu_ctx_lnx->rpu_ctx,
			     scan_results,
			     len);
		pr_err("Scan done received...sending get_scan_results\n");
		rpu_ctx_lnx->connect_params.status = SCAN_COMPLETE;
	} else {
		pr_err("Scan done received...\n");
		rpu_ctx_lnx->connect_params.status = SCAN_COMPLETE;
	}
}

void umac_cmd_sta_connect(void *vif_ctx,
			  unsigned int center_freq,
			  unsigned char *bssid)
{
	struct nrf_wifi_umac_cmd_win_sta_connect *sta_connect = NULL;
	int len = 0;
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = vif_ctx;
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = vif_ctx_lnx->rpu_ctx;

	int status = -1;

	if (rpu_ctx_lnx->connect_params.status == SCAN_RESULTS_COMPLETE) {

		len = sizeof(struct nrf_wifi_umac_cmd_win_sta_connect);

		sta_connect = kzalloc(len, GFP_KERNEL);
		sta_connect->center_frequency = center_freq;
		sta_connect->auth_type = NRF_WIFI_AUTHTYPE_OPEN_SYSTEM;

		memcpy(&sta_connect->nrf_wifi_bssid, bssid, ETH_ALEN);

		if (rpu_ctx_lnx->connect_params.ssid_len) {
			memcpy(&sta_connect->ssid.nrf_wifi_ssid,
			       &rpu_ctx_lnx->connect_params.ssid,
			       rpu_ctx_lnx->connect_params.ssid_len);
			sta_connect->ssid.nrf_wifi_ssid_len =
				rpu_ctx_lnx->connect_params.ssid_len;
		}
		sta_connect->umac_hdr.cmd_evnt = NRF_WIFI_UMAC_CMD_WIN_STA_CONNECT;
		sta_connect->umac_hdr.ids.wdev_id =  vif_ctx_lnx->if_idx;
		sta_connect->umac_hdr.ids.valid_fields |= NRF_WIFI_INDEX_IDS_WDEV_ID_VALID;

		pr_err("Scan results received...sending sta connect ssid=%s bssid=%pM\n",
		       sta_connect->ssid.nrf_wifi_ssid,sta_connect->nrf_wifi_bssid);

		status = umac_cmd_cfg(rpu_ctx_lnx->rpu_ctx,
				      sta_connect,
				      len);
		if (!status) {
			/*save the bssid*/
			memcpy(&rpu_ctx_lnx->connect_params.bssid, bssid, ETH_ALEN);
			rpu_ctx_lnx->connect_params.status = CONNECTING;
		}
	}

}

#endif

void nrf_wifi_wpa_supp_scan_start_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_trigger_scan *scan_start_event,
					      unsigned int event_len)
{
	pr_err("%s: Processing\n", __func__);

	netlink_send((unsigned char *)scan_start_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);

	return;
}

					 
void nrf_wifi_wpa_supp_scan_res_callbk_fn(void *os_vif_ctx,
					    struct nrf_wifi_umac_event_new_scan_results *scan_res,
					    unsigned int event_len,
					    bool more_res)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;
	rpu_ctx_lnx = vif_ctx_lnx->rpu_ctx;

#ifdef CMD_DEMO
	if (!more_res) {
		rpu_ctx_lnx->connect_params.status = SCAN_RESULTS_COMPLETE;

		umac_cmd_sta_connect(vif_ctx_lnx->netdev,
				     scan_res->frequency,
				     scan_res->mac_addr);
	}
#else
	netlink_send((unsigned char *)scan_res,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
#endif		
}


void nrf_wifi_wpa_supp_scan_done_callbk_fn(void *os_vif_ctx,
					     struct nrf_wifi_umac_event_trigger_scan *scan_done_event,
					     unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

#ifdef CMD_DEMO
	umac_event_scan_done(vif_ctx_lnx);
#else
	netlink_send((unsigned char *)scan_done_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
#endif
}


void nrf_wifi_wpa_supp_scan_abort_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_trigger_scan *scan_done_event,
					      unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	pr_info("%s: NOT processing event from UMAC\n", __func__);
}


void nrf_wifi_wpa_supp_auth_resp_callbk_fn(void *os_vif_ctx,
					     struct nrf_wifi_umac_event_mlme *auth_resp_event,
					     unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

#ifndef CMD_DEMO
	netlink_send((unsigned char *)auth_resp_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
#endif
}


void nrf_wifi_wpa_supp_assoc_resp_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_mlme *assoc_resp_event,
					      unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;
#ifdef CMD_DEMO
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)assoc_resp_event->frame.frame;
	int status = (int)le16_to_cpu(mgmt->u.assoc_resp.status_code);
#endif

	vif_ctx_lnx = os_vif_ctx;

#ifdef CMD_DEMO
	if (!status) {
		vif_ctx_lnx->rpu_ctx_lnx->connect_params.status = CONNECTED;
		pr_err("Association Response received...status connected\n");
	} else 
		pr_err("Association Response received...status not connected\n");
#else
	netlink_send((unsigned char *)assoc_resp_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
#endif
}


void nrf_wifi_wpa_supp_deauth_callbk_fn(void *os_vif_ctx,
					  struct nrf_wifi_umac_event_mlme *deauth_event,
					  unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)deauth_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}


void nrf_wifi_wpa_supp_disassoc_callbk_fn(void *os_vif_ctx,
					    struct nrf_wifi_umac_event_mlme *disassoc_event,
					    unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)disassoc_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}


void nrf_wifi_wpa_supp_mgmt_rx_callbk_fn(void *os_vif_ctx,
					   struct nrf_wifi_umac_event_mlme *mgmt_rx_event,
					   unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)mgmt_rx_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}


void nrf_wifi_wpa_supp_tx_status_callbk_fn(void *os_vif_ctx,
					   struct nrf_wifi_umac_event_mlme *tx_status_event,
					   unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)tx_status_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}


void nrf_wifi_wpa_supp_unprot_mlme_mgmt_rx_callbk_fn(void *os_vif_ctx,
						       struct nrf_wifi_umac_event_mlme *unprot_mlme_event,
						       unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)unprot_mlme_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}

void nrf_wifi_wpa_supp_new_if_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_interface_info *new_intf_evnt,
			unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)new_intf_evnt,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

void nrf_wifi_wpa_supp_get_wiphy_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_event_get_wiphy *get_wiphy_event,
			unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)get_wiphy_event,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

void nrf_wifi_wpa_supp_get_reg_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_reg *get_reg_event,
			unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)get_reg_event,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

void nrf_wifi_wpa_supp_set_reg_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_reg *set_reg_event,
			unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)set_reg_event,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

void nrf_wifi_wpa_supp_set_if_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_umac_event_set_interface *set_if_event,
			unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)set_if_event,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

void nrf_wifi_wpa_supp_disp_scan_res_callbk_fn(void *os_vif_ctx,
		struct nrf_wifi_umac_event_new_scan_display_results *scan_res,
		unsigned int event_len,
		bool more_res)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)scan_res,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

void nrf_wifi_wpa_supp_roc_callbk_fn(void *os_vif_ctx,
				       struct nrf_wifi_event_remain_on_channel *roc_event,
				       unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)roc_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}


void nrf_wifi_wpa_supp_roc_cancel_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_event_remain_on_channel *roc_cancel_event,
					      unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)roc_cancel_event,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}


void nrf_wifi_wpa_supp_cookie_rsp_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_cookie_rsp *cookie_rsp,
					      unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)cookie_rsp,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}


void nrf_wifi_wpa_supp_get_key_callbk_fn(void *os_vif_ctx,
			  struct nrf_wifi_umac_event_get_key *get_key_resp,
			  unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)get_key_resp,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

					  
void nrf_wifi_wpa_supp_reg_change_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_event_regulatory_change *reg_chng,
			unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)reg_chng,
		     event_len,
		     NL_SOCK_TYPE_ASYNC);
}

void nrf_wifi_wpa_supp_get_station_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_umac_event_new_station *station_info_event,
			unsigned int event_len)
{
	struct nrf_wifi_fmac_vif_ctx_lnx *vif_ctx_lnx = NULL;

	vif_ctx_lnx = os_vif_ctx;

	netlink_send((unsigned char *)station_info_event,
		     event_len,
		     NL_SOCK_TYPE_SYNC);
}

static void wpa_supp_if_process_cmd(void *cmd,
				    unsigned int cmd_len)
{
	struct nrf_wifi_ctx_lnx *rpu_ctx_lnx = NULL;
	struct nrf_wifi_umac_hdr *umac_hdr = NULL;
	struct nrf_wifi_umac_cmd_chg_vif_attr *chg_vif_cmd = NULL;
	struct nrf_wifi_umac_cmd_add_vif *add_vif_cmd = NULL;
#ifdef SOC_WEZEN
#ifdef SOC_WEZEN_SECURE_DOMAIN
	struct nrf_wifi_umac_cmd_key *key_cmd = NULL;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
#endif
#endif

	umac_hdr = cmd;
	/* TODO: Need mechanism in place to get the RPU index from wpa_supplicant */
	rpu_ctx_lnx = rpu_drv_priv.rpu_ctx_lnx[0];

	if (!rpu_ctx_lnx) {
		pr_err("Received netlink message for invalid RPU\n");
		return;
	}
#ifdef SOC_WEZEN
#ifdef SOC_WEZEN_SECURE_DOMAIN
	if (umac_hdr->cmd_evnt == NRF_WIFI_UMAC_CMD_NEW_KEY) {
		key_cmd = (struct nrf_wifi_umac_cmd_key *)cmd;

		if (is_zero_ether_addr(key_cmd->mac_addr)) {
			status = nrf_wifi_fmac_add_key(rpu_ctx_lnx->rpu_ctx,
						       key_cmd->umac_hdr.ids.wdev_id,
						       &key_cmd->key_info,
						       NULL);
		} else {
			status = nrf_wifi_fmac_add_key(rpu_ctx_lnx->rpu_ctx,
						       key_cmd->umac_hdr.ids.wdev_id,
						       &key_cmd->key_info,
						       key_cmd->mac_addr);
		}
		if (status == NRF_WIFI_STATUS_FAIL)
			pr_err("%s: nrf_wifi_fmac_add_key failed\n", __func__);
	} else if (umac_hdr->cmd_evnt == NRF_WIFI_UMAC_CMD_DEL_KEY) {
		key_cmd = (struct nrf_wifi_umac_cmd_key *)cmd;

		if (is_zero_ether_addr(key_cmd->mac_addr)) {
			status = nrf_wifi_fmac_del_key(rpu_ctx_lnx->rpu_ctx,
						       key_cmd->umac_hdr.ids.wdev_id,
						       &key_cmd->key_info,
						       NULL);
		} else {
			status = nrf_wifi_fmac_del_key(rpu_ctx_lnx->rpu_ctx,
						       key_cmd->umac_hdr.ids.wdev_id,
						       &key_cmd->key_info,
						       key_cmd->mac_addr);
		}

		if (status == NRF_WIFI_STATUS_FAIL)
			pr_err("%s: nrf_wifi_fmac_del_key failed\n", __func__);
	} else if (umac_hdr->cmd_evnt == NRF_WIFI_UMAC_CMD_SET_KEY) {
		key_cmd = (struct nrf_wifi_umac_cmd_key *)cmd;

		status = nrf_wifi_fmac_set_key(rpu_ctx_lnx->rpu_ctx,
					       key_cmd->umac_hdr.ids.wdev_id,
					       &key_cmd->key_info);

		if (status == NRF_WIFI_STATUS_FAIL)
			pr_err("%s: nrf_wifi_fmac_set_key failed\n", __func__);
	}
#endif
#endif
	switch(umac_hdr->cmd_evnt) {
#ifdef only_for_logging
	case NRF_WIFI_UMAC_CMD_TRIGGER_SCAN:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_TRIGGER_SCAN: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_SCAN_RESULTS:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_SCAN_RESULTS: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_AUTHENTICATE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_AUTHENTICATE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_ASSOCIATE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_ASSOCIATE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_DEAUTHENTICATE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_DEAUTHENTICATE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_WIPHY:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_WIPHY: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_NEW_KEY:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_NEW_KEY: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_DEL_KEY:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_DEL_KEY: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_KEY:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_KEY: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_KEY:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_KEY: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_NEW_BEACON:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_NEW_BEACON: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_BEACON:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_BEACON: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_BSS:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_BSS: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_START_AP:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_START_AP: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_STOP_AP:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_STOP_AP: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_DEL_INTERFACE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_DEL_INTERFACE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_IFFLAGS:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_IFFLAGS: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_NEW_STATION:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_NEW_STATION: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_DEL_STATION:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_DEL_STATION: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_STATION:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_STATION: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_STATION:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_STATION: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_START_P2P_DEVICE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_START_P2P_DEVICE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_STOP_P2P_DEVICE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_STOP_P2P_DEVICE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_REMAIN_ON_CHANNEL:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_REMAIN_ON_CHANNEL: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_CANCEL_REMAIN_ON_CHANNEL:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_CANCEL_REMAIN_ON_CHANNEL: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_CHANNEL:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_CHANNEL: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_RADAR_DETECT:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_RADAR_DETECT: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_REGISTER_FRAME:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_REGISTER_FRAME: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_FRAME:
			printk(KERN_ERR "NRF_WIFI_UMAC_CMD_FRAME: %s \n", __func__);
		{
#define WLAN_FC_GET_STYPE(fc)   (((fc) & 0x00f0) >> 4)
#define WLAN_FC_STYPE_ASSOC_REQ         0
#define WLAN_FC_STYPE_ASSOC_RESP        1
#define WLAN_FC_STYPE_REASSOC_REQ       2
#define WLAN_FC_STYPE_REASSOC_RESP      3
#define WLAN_FC_STYPE_PROBE_REQ         4
#define WLAN_FC_STYPE_PROBE_RESP        5
#define WLAN_FC_STYPE_BEACON            8
#define WLAN_FC_STYPE_ATIM              9
#define WLAN_FC_STYPE_DISASSOC          10
#define WLAN_FC_STYPE_AUTH              11
#define WLAN_FC_STYPE_DEAUTH            12
#define WLAN_FC_STYPE_ACTION            13

			struct nrf_wifi_umac_mgmt_tx_info *mgmt_tx_info = NULL;
			struct nrf_wifi_umac_cmd_mgmt_tx *mgmt_tx_cmd = NULL;
			struct ieee80211_mgmt *mgmt;
			unsigned short fc;
			mgmt_tx_cmd = (struct nrf_wifi_umac_cmd_mgmt_tx *)nlmsg_data(nlh);
			mgmt_tx_info = (struct nrf_wifi_umac_mgmt_tx_info *)&mgmt_tx_cmd->info;
			mgmt = (struct ieee80211_mgmt *)&mgmt_tx_info->frame.frame;
			fc = mgmt->frame_control;
			switch(WLAN_FC_GET_STYPE(fc)) {
				case WLAN_FC_STYPE_PROBE_RESP:
					pr_err("Sending WLAN_FC_STYPE_PROBE_RESP\n");
					break;
				case WLAN_FC_STYPE_ASSOC_RESP:
					pr_err("Sending WLAN_FC_STYPE_ASSOC_RESP\n");
					break;
				case WLAN_FC_STYPE_AUTH:
					pr_err("Sending WLAN_FC_STYPE_AUTH\n");
					break;
				case WLAN_FC_STYPE_DEAUTH:
					pr_err("Sending WLAN_FC_STYPE_DEAUTH\n");
					break;
				case WLAN_FC_STYPE_BEACON:
					pr_err("Sending WLAN_FC_STYPE_BEACON\n");
					break;
				case WLAN_FC_STYPE_PROBE_REQ:
					pr_err("Sending WLAN_FC_STYPE_PROBE_REQ\n");
					break;
				default:
					pr_err("Sending frame with fc=%x\n", fc);
					break;
			}
		}
		break;
	case NRF_WIFI_UMAC_CMD_JOIN_IBSS:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_JOIN_IBSS: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_WIN_STA_CONNECT:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_WIN_STA_CONNECT: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_POWER_SAVE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_POWER_SAVE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_WOWLAN:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_WOWLAN: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SUSPEND:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SUSPEND: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_RESUME:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_RESUME: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_QOS_MAP:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_QOS_MAP: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_CHANNEL:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_CHANNEL: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_TX_POWER:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_TX_POWER: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_INTERFACE:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_INTERFACE: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_WIPHY:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_WIPHY: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_IFHWADDR:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_IFHWADDR: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_IFHWADDR:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_IFHWADDR: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_GET_REG:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_GET_REG: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_SET_REG:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_REG: %s \n", __func__);
		break;
	case NRF_WIFI_UMAC_CMD_REQ_SET_REG:
		printk(KERN_ERR "NRF_WIFI_UMAC_CMD_REQ_SET_REG: %s \n", __func__);
		break;
#endif
	case NRF_WIFI_UMAC_CMD_NEW_INTERFACE:
		//printk(KERN_ERR "NRF_WIFI_UMAC_CMD_NEW_INTERFACE: %s \n", __func__);
		add_vif_cmd = cmd;
		nrf_wifi_fmac_vif_update_if_type(rpu_ctx_lnx->rpu_ctx,
						   add_vif_cmd->umac_hdr.ids.wdev_id,
						   add_vif_cmd->info.iftype);
		break;
	case NRF_WIFI_UMAC_CMD_SET_INTERFACE:
		//printk(KERN_ERR "NRF_WIFI_UMAC_CMD_SET_INTERFACE: %s \n", __func__);
		chg_vif_cmd = cmd;
		nrf_wifi_fmac_vif_update_if_type(rpu_ctx_lnx->rpu_ctx,
						   chg_vif_cmd->umac_hdr.ids.wdev_id,
						   chg_vif_cmd->info.iftype);
		break;
	default:
		//printk(KERN_ERR " CMD: %s %d \n", __func__, umac_hdr->cmd_evnt);
		break;
	}

#ifdef SOC_WEZEN
#ifdef SOC_WEZEN_SECURE_DOMAIN
	if ((umac_hdr->cmd_evnt != NRF_WIFI_UMAC_CMD_NEW_KEY) &&
	    (umac_hdr->cmd_evnt != NRF_WIFI_UMAC_CMD_DEL_KEY) &&
	    (umac_hdr->cmd_evnt != NRF_WIFI_UMAC_CMD_SET_KEY)) {
		umac_cmd_cfg(rpu_ctx_lnx->rpu_ctx,
			     cmd,
			     cmd_len);
	}
#else
	umac_cmd_cfg(rpu_ctx_lnx->rpu_ctx,
		     cmd,
		     cmd_len);
#endif /* SOC_WEZEN_SECURE_DOMAIN */
#else
	umac_cmd_cfg(rpu_ctx_lnx->rpu_ctx,
		     cmd,
		     cmd_len);
#endif /* SOC_WEZEN */
}

int wpa_supp_if_init(void)
{
	int ret = -1;

	ret = netlink_init(&wpa_supp_if_process_cmd);

	if (ret)
		pr_err("%s: netlink_init failed\n", __func__);

	return ret;
}

void wpa_supp_if_deinit(void)
{
	netlink_deinit();
}
#endif /* !CONFIG_NRF700X_RADIO_TEST */
#endif /* !HOST_CFG80211_SUPPORT */

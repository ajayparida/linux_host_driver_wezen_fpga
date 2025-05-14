/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __WPA_SUPP_IF_H__
#define __WPA_SUPP_IF_H__

int wpa_supp_if_init(void);

void wpa_supp_if_deinit(void);

int wpa_supp_if_process_event(void *rpu_ctx,
			      void *vif_ctx,
			      unsigned int event_num,
			      void *event_data,
			      unsigned int even_len);

void nrf_wifi_wpa_supp_scan_start_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_trigger_scan *scan_start_event,
					      unsigned int event_len);
void nrf_wifi_wpa_supp_scan_res_callbk_fn(void *os_vif_ctx,
					    struct nrf_wifi_umac_event_new_scan_results *scan_res,
					    unsigned int event_len,
					    bool more_res);
void nrf_wifi_wpa_supp_scan_done_callbk_fn(void *os_vif_ctx,
					     struct nrf_wifi_umac_event_trigger_scan *scan_done_event,
					     unsigned int event_len);
void nrf_wifi_wpa_supp_scan_abort_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_trigger_scan *scan_done_event,
					      unsigned int event_len);
void nrf_wifi_wpa_supp_auth_resp_callbk_fn(void *os_vif_ctx,
					     struct nrf_wifi_umac_event_mlme *auth_resp_event,
					     unsigned int event_len);
void nrf_wifi_wpa_supp_assoc_resp_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_mlme *assoc_resp_event,
					      unsigned int event_len);
void nrf_wifi_wpa_supp_deauth_callbk_fn(void *os_vif_ctx,
					  struct nrf_wifi_umac_event_mlme *deauth_event,
					  unsigned int event_len);
void nrf_wifi_wpa_supp_disassoc_callbk_fn(void *os_vif_ctx,
					    struct nrf_wifi_umac_event_mlme *disassoc_event,
					    unsigned int event_len);
void nrf_wifi_wpa_supp_mgmt_rx_callbk_fn(void *os_vif_ctx,
					   struct nrf_wifi_umac_event_mlme *mgmt_rx_event,
					   unsigned int event_len);
void nrf_wifi_wpa_supp_tx_status_callbk_fn(void *os_vif_ctx,
					   struct nrf_wifi_umac_event_mlme *mgmt_rx_event,
					   unsigned int event_len);
void nrf_wifi_wpa_supp_unprot_mlme_mgmt_rx_callbk_fn(void *os_vif_ctx,
						       struct nrf_wifi_umac_event_mlme *unprot_mlme_event,
						       unsigned int event_len);
void nrf_wifi_wpa_supp_new_if_callbk_fn(void *os_vif_ctx,
				struct nrf_wifi_interface_info *new_intf_event,
				unsigned int event_len);
void nrf_wifi_wpa_supp_get_wiphy_callbk_fn(void *os_vif_ctx,
			struct nrf_wifi_event_get_wiphy *get_wiphy_event,
			unsigned int event_len);
void nrf_wifi_wpa_supp_get_reg_callbk_fn(void *os_vif_ctx,
				struct nrf_wifi_reg *get_reg_event,
				unsigned int event_len);
void nrf_wifi_wpa_supp_set_reg_callbk_fn(void *os_vif_ctx,
				struct nrf_wifi_reg *set_reg_event,
				unsigned int event_len);
void nrf_wifi_wpa_supp_set_if_callbk_fn(void *os_dev_ctx,
			struct nrf_wifi_umac_event_set_interface *set_if_event,
			unsigned int event_len);
void nrf_wifi_wpa_supp_roc_callbk_fn(void *os_vif_ctx,
				       struct nrf_wifi_event_remain_on_channel *roc_event,
				       unsigned int event_len);
void nrf_wifi_wpa_supp_roc_cancel_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_event_remain_on_channel *roc_cancel_event,
					      unsigned int event_len);
void nrf_wifi_wpa_supp_cookie_rsp_callbk_fn(void *os_vif_ctx,
					      struct nrf_wifi_umac_event_cookie_rsp *cookie_rsp,
					      unsigned int event_len);
void nrf_wifi_wpa_supp_disp_scan_res_callbk_fn(void *os_vif_ctx,
		struct nrf_wifi_umac_event_new_scan_display_results *scan_res,
		unsigned int event_len,
		bool more_res);

void nrf_wifi_wpa_supp_get_key_callbk_fn(void *os_vif_ctx,
		struct nrf_wifi_umac_event_get_key *get_key_resp,
		unsigned int event_len);

void nrf_wifi_wpa_supp_reg_change_callbk_fn(void *os_vif_ctx,
		struct nrf_wifi_event_regulatory_change *reg_chng,
		unsigned int event_len);
#endif /* __WPA_SUPP_IF_H__ */

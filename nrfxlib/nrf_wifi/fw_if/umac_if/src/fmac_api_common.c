/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @brief File containing API definitions for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#include "host_rpu_umac_if.h"
#include "fmac_api.h"
#include "hal_api.h"
#include "fmac_structs.h"
#include "fmac_api.h"
#include "fmac_util.h"
#include "fmac_peer.h"
#include "fmac_vif.h"
#include "fmac_tx.h"
#include "fmac_rx.h"
#include "fmac_cmd.h"
#include "fmac_event.h"
#include "fmac_bb.h"
#include "util.h"

#include <patch_info.h>

static int nrf_wifi_patch_version_compat(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				const unsigned int version)
{
	unsigned int family, major, minor, patch;

	family = (version >> 24) & 0xff;
	major = (version >> 16) & 0xff;
	minor = (version >> 8) & 0xff;
	patch = (version >> 0) & 0xff;

	if (family != RPU_FAMILY) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Incompatible RPU version: %d", family);
		return -1;
	}

	if (major != RPU_MAJOR_VERSION) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Incompatible RPU major version: %d", major);
		return -1;
	}

	/* TODO: Allow minor version to be different */
	if (minor != RPU_MINOR_VERSION) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Incompatible RPU minor version: %d", minor);
		return -1;
	}

	/* TODO: Allow patch version to be different */
	if (patch != RPU_PATCH_VERSION) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Incompatible RPU patch version: %d", patch);
		return -1;
	}

	return 0;
}

static int nrf_wifi_patch_feature_flags_compat(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				const unsigned int feature_flags)
{
#ifdef CONFIG_NRF700X_RADIO_TEST
	if (!(feature_flags & NRF70_FEAT_RADIO_TEST)) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Radio test feature flag not set");
		return -1;
	}
#elif defined(CONFIG_NRF700X_SCAN_ONLY)
	if (!(feature_flags & NRF70_FEAT_SCAN_ONLY)) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Scan only feature flag not set");
		return -1;
	}
#elif defined(CONFIG_NRF700X_SYSTEM_MODE)
	if (!(feature_flags & NRF70_FEAT_SYSTEM_MODE)) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"System mode feature flag not set");
		return -1;
	}
#else
	nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
		"Invalid feature flags: 0x%x or build configuration",
		feature_flags);
#endif

	return 0;
}

#ifndef SOC_WEZEN

enum nrf_wifi_status nrf_wifi_fmac_fw_parse(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					   const void *fw_data,
					   unsigned int fw_size,
					   struct nrf_wifi_fmac_fw_info *fw_info)
{
	struct nrf70_fw_image_info *info = (struct nrf70_fw_image_info *)fw_data;
	unsigned int offset;
	unsigned int image_id;

	if (!fw_data || !fw_size || !fw_info) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Invalid parameters");
		return NRF_WIFI_STATUS_FAIL;
	}

	if (fw_size < sizeof(struct nrf70_fw_image_info)) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Invalid fw_size: %d, minimum size: %d",
			fw_size, sizeof(struct nrf70_fw_image_info));
		return NRF_WIFI_STATUS_FAIL;
	}

	nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
		"num_images: %d", info->num_images);
	nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
		"version: 0x%x", info->version);
	nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
		"feature_flags: %d", info->feature_flags);

	if (info->num_images != NRF_WIFI_PATCH_NUM_IMAGES) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Invalid number of images, expected %d, got %d",
			NRF_WIFI_PATCH_NUM_IMAGES, info->num_images);
		return NRF_WIFI_STATUS_FAIL;
	}

	if (nrf_wifi_patch_version_compat(fmac_dev_ctx, info->version) != 0) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Incompatible patch version");
		return NRF_WIFI_STATUS_FAIL;
	}

	if (nrf_wifi_patch_feature_flags_compat(fmac_dev_ctx, info->feature_flags) != 0) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Incompatible feature flags");
		return NRF_WIFI_STATUS_FAIL;
	}

	offset = sizeof(struct nrf70_fw_image_info);

	nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv, "====");
	for (image_id = 0; image_id < info->num_images; image_id++) {
		struct nrf70_fw_image *image =
			(struct nrf70_fw_image *)((char *)fw_data + offset);
		const void *data = (char *)fw_data + offset + sizeof(struct nrf70_fw_image);

		if (offset + sizeof(struct nrf70_fw_image) + image->len > fw_size) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				"Invalid fw_size: %d for image[%d] len: %d",
				fw_size, image_id, image->len);
			return NRF_WIFI_STATUS_FAIL;
		}

		nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
			"image[%d] type: %d", image_id, image->type);
		nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
			"image[%d] len: %d", image_id, image->len);
		nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
			"====");

		switch (image_id) {
		case NRF70_IMAGE_LMAC_PRI:
			fw_info->lmac_patch_pri.data = data;
			fw_info->lmac_patch_pri.size = image->len;
			break;
		case NRF70_IMAGE_LMAC_SEC:
			fw_info->lmac_patch_sec.data = data;
			fw_info->lmac_patch_sec.size = image->len;
			break;
		case NRF70_IMAGE_UMAC_PRI:
			fw_info->umac_patch_pri.data = data;
			fw_info->umac_patch_pri.size = image->len;
			break;
		case NRF70_IMAGE_UMAC_SEC:
			fw_info->umac_patch_sec.data = data;
			fw_info->umac_patch_sec.size = image->len;
			break;
		default:
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				"Invalid image id: %d", image_id);
			break;
		}

		offset += sizeof(struct nrf70_fw_image) + image->len;
	}

	return NRF_WIFI_STATUS_SUCCESS;
}
#endif /*!SOC_WEZEN*/

enum nrf_wifi_status nrf_wifi_fmac_fw_load(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					   struct nrf_wifi_fmac_fw_info *fmac_fw)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
#ifdef HOST_FW_HEX_LOAD_SUPPORT
	long vpr0_patch_addr = -1, vpr1_patch_addr = -1;
	if (!(fmac_fw->umac_hex.data)) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC HEX image not available\n",
                                      __func__);
                goto out;
        }

	if (!(fmac_fw->lmac_hex.data)) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC HEX image not available\n",
                                      __func__);
                goto out;
        }
	
	status = nrf_wifi_hal_set_grtc(fmac_dev_ctx->hal_dev_ctx,
				       GRTC_CLKCFG_ADDR,
				       GRTC_CLKCFG_VAL);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: GRTC settings failed @0x%x\n",
                                      __func__,
				       GRTC_CLKCFG_ADDR);
                goto out;
        }

	status = nrf_wifi_hal_set_grtc(fmac_dev_ctx->hal_dev_ctx,
				       GRTC_MODE_ADDR,
				       GRTC_MODE_VAL);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: GRTC settings failed @0x%x\n",
                                      __func__,
				      GRTC_MODE_ADDR);
                goto out;
        }

	status = nrf_wifi_hal_set_grtc(fmac_dev_ctx->hal_dev_ctx,
				       GRTC_TASKS_START_ADDR,
				       GRTC_TASKS_START_VAL);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: GRTC settings failed @0x%x\n",
                                      __func__,
				      GRTC_TASKS_START_ADDR);
                goto out;
        }

	vpr0_patch_addr =  nrf_wifi_hal_get_fw_hex_patch_addr(fmac_dev_ctx->hal_dev_ctx,
							      RPU_PROC_TYPE_MCU_LMAC,
							      fmac_fw->lmac_hex.data,
							      fmac_fw->lmac_hex.size);
	if (vpr0_patch_addr == -1) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Gettings vpr0 patch address failed\n",
                                      __func__);
                goto out;
	}
	status = nrf_wifi_hal_set_wicr(fmac_dev_ctx->hal_dev_ctx,
				       RPU_REG_WICR_ADDR_VPR0_PATCH_ADDR,
				       vpr0_patch_addr);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: WICR settings failed for vpr0 patch address @0x%x\n",
                                      __func__,
				      vpr0_patch_addr);
                goto out;
        } else {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "vpr0 patch address %x wrote to %x\n",
				      vpr0_patch_addr,
				      RPU_REG_WICR_ADDR_VPR0_PATCH_ADDR);
	}

	vpr1_patch_addr =  nrf_wifi_hal_get_fw_hex_patch_addr(fmac_dev_ctx->hal_dev_ctx,
							 RPU_PROC_TYPE_MCU_UMAC,
							 fmac_fw->umac_hex.data,
							 fmac_fw->umac_hex.size);
	if (vpr1_patch_addr == -1) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Gettings vpr1 patch address failed\n",
                                      __func__);
                goto out;
	}

	status = nrf_wifi_hal_set_wicr(fmac_dev_ctx->hal_dev_ctx,
				       RPU_REG_WICR_ADDR_VPR1_PATCH_ADDR,
				       vpr1_patch_addr);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: WICR settings failed for vpr1 patch address @0x%x\n",
                                      __func__,
				      vpr1_patch_addr);
                goto out;
        } else {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "vpr1 patch address %x wrote to %x\n",
				      vpr1_patch_addr,
				      RPU_REG_WICR_ADDR_VPR1_PATCH_ADDR);
	}

        status = nrf_wifi_hal_fw_hex_load(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_UMAC,
                                          fmac_fw->umac_hex.data,
                                          fmac_fw->umac_hex.size);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Failed to load UMAC HEX\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                       "%s: UMAC HEX loaded\n",
                                       __func__);
        }

	status = nrf_wifi_hal_fw_hex_load(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_LMAC,
                                          fmac_fw->lmac_hex.data,
                                          fmac_fw->lmac_hex.size);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Failed to load LMAC HEX\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                       "%s: LMAC HEX loaded\n",
                                       __func__);
        }

	status = nrf_wifi_hal_set_initpc(fmac_dev_ctx->hal_dev_ctx,
					 VPR0_INITPC_ADDR,
					 RPU_ADDR_ROM0_START);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Setting INITPC failed\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Setting INITPC to %x succeeded\n",
                                      __func__,
				      RPU_ADDR_ROM0_START);
        }

	status = nrf_wifi_hal_cpu_run(fmac_dev_ctx->hal_dev_ctx,
				      VPR0_CPURUN_ADDR,
				      VPR0_CPURUN_VAL);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: CPU RUN failed\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                      "%s: CPURUN succeded; LMAC start given 0x%x. \n",
                                      __func__,
				      VPR0_CPURUN_ADDR) ;
        }
	status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_LMAC);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC HEX boot check failed\n",
                                      __func__);
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC HEX boot check passed\n",
                                      __func__);
        }

	status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_UMAC);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC HEX boot check failed\n",
                                      __func__);
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC HEX boot check passed\n",
                                      __func__);
        }
#else
#ifdef HOST_FW_RAM_LOAD_SUPPORT
        if (!(fmac_fw->lmac_ram.data)) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC RAM image not available\n",
                                      __func__);
                goto out;
        }

        if (!(fmac_fw->umac_ram.data)) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC RAM image not available\n",
                                      __func__);
                goto out;
        }

	pr_err("calling nrf_wifi_hal_proc_reset\n");
        status = nrf_wifi_hal_proc_reset(fmac_dev_ctx->hal_dev_ctx,
                                         RPU_PROC_TYPE_MCU_LMAC);

        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC processor reset failed\n",
                                      __func__);

                goto out;
        }

	pr_err("calling nrf_wifi_hal_fw_ram_load\n");
        status = nrf_wifi_hal_fw_ram_load(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_LMAC,
                                          fmac_fw->lmac_ram.data,
                                          fmac_fw->lmac_ram.size);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Failed to load LMAC RAM\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                       "%s: LMAC RAM loaded\n",
                                       __func__);
        }

	status = nrf_wifi_hal_fw_ram_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_LMAC);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC RAM boot failed\n",
                                      __func__);
                goto out;
        }

        status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_LMAC);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC RAM boot check failed\n",
                                      __func__);
        } else {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC RAM boot check passed\n",
                                      __func__);
        }
        status = nrf_wifi_hal_proc_reset(fmac_dev_ctx->hal_dev_ctx,
                                         RPU_PROC_TYPE_MCU_UMAC);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC processor reset failed\n",
                                      __func__);
                goto out;
        }
        status = nrf_wifi_hal_fw_ram_load(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_UMAC,
                                          fmac_fw->umac_ram.data,
                                          fmac_fw->umac_ram.size);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: Failed to load UMAC RAM\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                       "%s: UMAC RAM loaded\n",
                                       __func__);
        }
        status = nrf_wifi_hal_fw_ram_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_UMAC);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC RAM boot failed\n",
                                      __func__);
                goto out;
        }

	status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_UMAC);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC RAM boot check failed\n",
                                      __func__);
        } else {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC RAM boot check passed\n",
                                      __func__);
        }

#else /* HOST_FW_RAM_LOAD_SUPPORT */
#ifdef HOST_FW_ROM_LOAD_SUPPORT
#ifdef HOST_FW_PATCH_LOAD_SUPPORT
	/* Load the LMAC patches if available */
	if ((fmac_fw->lmac_patch_pri.data) && (fmac_fw->lmac_patch_pri.size)) {
		status = nrf_wifi_hal_proc_reset(fmac_dev_ctx->hal_dev_ctx,
						 RPU_PROC_TYPE_MCU_LMAC);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC processor reset failed\n",
					      __func__);

			goto out;
		}

		/* Load the LMAC patches */
		status = nrf_wifi_hal_fw_patch_load(fmac_dev_ctx->hal_dev_ctx,
						    RPU_PROC_TYPE_MCU_LMAC,
						    fmac_fw->lmac_patch_pri.data,
						    fmac_fw->lmac_patch_pri.size,
						    fmac_fw->lmac_patch_sec.data,
						    fmac_fw->lmac_patch_sec.size);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC patch load failed\n",
					      __func__);
			goto out;
		} else {
			nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC patches loaded\n",
					      __func__);
		}

		status = nrf_wifi_hal_fw_patch_boot(fmac_dev_ctx->hal_dev_ctx,
						    RPU_PROC_TYPE_MCU_LMAC,
						    true);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: Failed to boot LMAC with patch\n",
					      __func__);
			goto out;
		}

		status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
						  RPU_PROC_TYPE_MCU_LMAC);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC ROM boot check failed\n",
					      __func__);
			goto out;
		} else {
			nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC boot check passed\n",
					      __func__);
		}
	} else {
		status = nrf_wifi_hal_fw_patch_boot(fmac_dev_ctx->hal_dev_ctx,
						    RPU_PROC_TYPE_MCU_LMAC,
						    false);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC ROM boot failed\n",
					      __func__);
			goto out;
		}

		status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
						  RPU_PROC_TYPE_MCU_LMAC);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC ROM boot check failed\n",
					      __func__);
			goto out;
		} else {
			nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
					      "%s: LMAC boot check passed\n",
					      __func__);
		}
	}

	/* Load the UMAC patches if available */
	if ((fmac_fw->umac_patch_pri.data) && (fmac_fw->umac_patch_pri.size)) {
		status = nrf_wifi_hal_proc_reset(fmac_dev_ctx->hal_dev_ctx,
						 RPU_PROC_TYPE_MCU_UMAC);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC processor reset failed\n",
					      __func__);

			goto out;
		}

		/* Load the UMAC patches */
		status = nrf_wifi_hal_fw_patch_load(fmac_dev_ctx->hal_dev_ctx,
						    RPU_PROC_TYPE_MCU_UMAC,
						    fmac_fw->umac_patch_pri.data,
						    fmac_fw->umac_patch_pri.size,
						    fmac_fw->umac_patch_sec.data,
						    fmac_fw->umac_patch_sec.size);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC patch load failed\n",
					      __func__);
			goto out;
		} else {
			nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC patches loaded\n",
					      __func__);
		}

		status = nrf_wifi_hal_fw_patch_boot(fmac_dev_ctx->hal_dev_ctx,
						    RPU_PROC_TYPE_MCU_UMAC,
						    true);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: Failed to boot UMAC with patch\n",
					      __func__);
			goto out;
		}

		status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
						  RPU_PROC_TYPE_MCU_UMAC);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC ROM boot check failed\n",
					      __func__);
			goto out;
		} else {
			nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC boot check passed\n",
					      __func__);
		}
	} else {
		status = nrf_wifi_hal_fw_patch_boot(fmac_dev_ctx->hal_dev_ctx,
						    RPU_PROC_TYPE_MCU_UMAC,
						    false);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC ROM boot failed\n",
					      __func__);
			goto out;
		}

		status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
						  RPU_PROC_TYPE_MCU_UMAC);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC ROM boot check failed\n",
					      __func__);
			goto out;
		} else {
			nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
					      "%s: UMAC boot check passed\n",
					      __func__);
		}
	}
#endif /* HOST_FW_PATCH_LOAD_SUPPORT*/
#endif /* HOST_FW_ROM_LOAD_SUPPORT */
#endif /* !HOST_FW_RAM_LOAD_SUPPORT */
#endif /* !HOST_FW_HEX_LOAD_SUPPORT */
	fmac_dev_ctx->fw_boot_done = true;

out:
	return status;
}

enum nrf_wifi_status nrf_wifi_fmac_fw_chk_boot(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx)
{
        enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

        status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_LMAC);

        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: LMAC boot check failed\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                       "%s: LMAC boot check passed\n",
                                       __func__);
        }

        status = nrf_wifi_hal_fw_chk_boot(fmac_dev_ctx->hal_dev_ctx,
                                          RPU_PROC_TYPE_MCU_UMAC);

        if (status != NRF_WIFI_STATUS_SUCCESS) {
                nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
                                      "%s: UMAC boot check failed\n",
                                      __func__);
                goto out;
        } else {
                nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
                                       "%s: UMAC boot check passed\n",
                                       __func__);
        }
out:
        return status;
}


struct nrf_wifi_fmac_dev_ctx *nrf_wifi_fmac_dev_add(struct nrf_wifi_fmac_priv *fpriv,
						    void *os_dev_ctx)
{
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
#ifndef CONFIG_NRF700X_RADIO_TEST
	struct nrf_wifi_fmac_dev_ctx_def *fmac_dev_priv = NULL;
#else
	struct nrf_wifi_fmac_dev_ctx_rt *fmac_dev_priv = NULL;
#endif
#ifdef CONFIG_NRF700X_DATA_TX
	struct nrf_wifi_fmac_priv_def *def_priv = NULL;
#endif /* CONFIG_NRF700X_DATA_TX */

	if (!fpriv || !os_dev_ctx) {
		return NULL;
	}

	fmac_dev_ctx = nrf_wifi_osal_mem_zalloc(fpriv->opriv,
						sizeof(*fmac_dev_ctx) + sizeof(*fmac_dev_priv));

	if (!fmac_dev_ctx) {
		nrf_wifi_osal_log_err(fpriv->opriv,
				      "%s: Unable to allocate fmac_dev_ctx\n",
				      __func__);
		goto out;
	}

	fmac_dev_ctx->fpriv = fpriv;
	fmac_dev_ctx->os_dev_ctx = os_dev_ctx;

	fmac_dev_ctx->hal_dev_ctx = nrf_wifi_hal_dev_add(fpriv->hpriv,
							 fmac_dev_ctx);

	if (!fmac_dev_ctx->hal_dev_ctx) {
		nrf_wifi_osal_log_err(fpriv->opriv,
				      "%s: nrf_wifi_hal_dev_add failed\n",
				      __func__);

		nrf_wifi_osal_mem_free(fpriv->opriv,
				       fmac_dev_ctx);
		fmac_dev_ctx = NULL;
		goto out;
	}
#ifdef CONFIG_NRF700X_DATA_TX

	def_priv = wifi_fmac_priv(fpriv);
	fpriv->hpriv->cfg_params.max_ampdu_len_per_token = def_priv->max_ampdu_len_per_token;
#endif /* CONFIG_NRF700X_DATA_TX */
out:
	return fmac_dev_ctx;
}

enum nrf_wifi_status nrf_wifi_fmac_stats_get(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					     enum rpu_op_mode op_mode,
					     struct rpu_op_stats *stats)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned char count = 0;
	int stats_type;
#ifndef CONFIG_NRF700X_RADIO_TEST
	struct nrf_wifi_fmac_dev_ctx_def *def_dev_ctx = NULL;
#endif /* CONFIG_NRF700X_RADIO_TEST */

	#ifdef CONFIG_NRF700X_RADIO_TEST
		stats_type = RPU_STATS_TYPE_PHY;
	#else
		stats_type = RPU_STATS_TYPE_ALL;
	#endif /* CONFIG_NRF700X_RADIO_TEST */


	if ((stats_type == RPU_STATS_TYPE_ALL) ||
	    (stats_type == RPU_STATS_TYPE_UMAC) ||
	    (stats_type == RPU_STATS_TYPE_LMAC) ||
	    (stats_type == RPU_STATS_TYPE_PHY)) {
		if (fmac_dev_ctx->stats_req == true) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: Stats request already pending\n",
					      __func__);
			goto out;
		}

		fmac_dev_ctx->stats_req = true;
		fmac_dev_ctx->fw_stats = &stats->fw;

		status = umac_cmd_prog_stats_get(fmac_dev_ctx,
		#ifdef CONFIG_NRF700X_RADIO_TEST
						 op_mode,
		#endif /* CONFIG_NRF700X_RADIO_TEST */
						 stats_type);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			goto out;
		}

		do {
			nrf_wifi_osal_sleep_ms(fmac_dev_ctx->fpriv->opriv,
					       1);
			count++;
		} while ((fmac_dev_ctx->stats_req == true) &&
			 (count < NRF_WIFI_FMAC_STATS_RECV_TIMEOUT));

		if (count == NRF_WIFI_FMAC_STATS_RECV_TIMEOUT) {
			nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: Timed out\n",
					      __func__);
			goto out;
		}
	}


#ifndef CONFIG_NRF700X_RADIO_TEST
	def_dev_ctx = wifi_dev_priv(fmac_dev_ctx);
	if ((stats_type == RPU_STATS_TYPE_ALL) ||
	    (stats_type == RPU_STATS_TYPE_HOST)) {
		nrf_wifi_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				      &stats->host,
				      &def_dev_ctx->host_stats,
				      sizeof(def_dev_ctx->host_stats));
	}
#endif

	status = NRF_WIFI_STATUS_SUCCESS;
out:
	return status;
}

enum nrf_wifi_status nrf_wifi_fmac_ver_get(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					  unsigned int *fw_ver)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	status = hal_rpu_mem_read(fmac_dev_ctx->hal_dev_ctx,
				  fw_ver,
				  RPU_MEM_UMAC_VER,
				  sizeof(*fw_ver));

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Unable to read UMAC ver\n",
				      __func__);
		goto out;
	}

out:
	return status;
}

enum nrf_wifi_status nrf_wifi_fmac_conf_ltf_gi(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					       unsigned char he_ltf,
					       unsigned char he_gi,
					       unsigned char enabled)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	status = umac_cmd_he_ltf_gi(fmac_dev_ctx, he_ltf, he_gi, enabled);

	return status;
}

enum nrf_wifi_status nrf_wifi_fmac_conf_btcoex(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					       void *cmd, unsigned int cmd_len)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	status = umac_cmd_btcoex(fmac_dev_ctx, cmd, cmd_len);

	return status;
}


enum nrf_wifi_status nrf_wifi_fmac_otp_mac_addr_get(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
						    unsigned char vif_idx,
						    unsigned char *mac_addr)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_otp_info otp_info;
	unsigned char *otp_mac_addr = NULL;
	unsigned int otp_mac_addr_flag_mask = 0;

	if (!fmac_dev_ctx || !mac_addr || (vif_idx >= MAX_NUM_VIFS)) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto out;
	}

	nrf_wifi_osal_mem_set(fmac_dev_ctx->fpriv->opriv,
			      &otp_info,
			      0xFF,
			      sizeof(otp_info));

	status = nrf_wifi_hal_otp_info_get(fmac_dev_ctx->hal_dev_ctx,
					   &otp_info.info,
					   &otp_info.flags);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Fetching of RPU OTP information failed\n",
				      __func__);
		goto out;
	}

	if (vif_idx == 0) {
		otp_mac_addr = (unsigned char *)otp_info.info.mac_address0;
		otp_mac_addr_flag_mask = (~MAC0_ADDR_FLAG_MASK);

	} else if (vif_idx == 1) {
		otp_mac_addr = (unsigned char *)otp_info.info.mac_address1;
		otp_mac_addr_flag_mask = (~MAC1_ADDR_FLAG_MASK);
	}

	/* Check if a valid MAC address has been programmed in the OTP */

	if (otp_info.flags & otp_mac_addr_flag_mask) {
		nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
				       "%s: MAC addr not programmed in OTP\n",
				       __func__);

	} else {
		nrf_wifi_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				      mac_addr,
				      otp_mac_addr,
				      NRF_WIFI_ETH_ADDR_LEN);


		if (!nrf_wifi_utils_is_mac_addr_valid(fmac_dev_ctx->fpriv->opriv,
						      (const char *)mac_addr)) {
			nrf_wifi_osal_log_info(fmac_dev_ctx->fpriv->opriv,
					       "%s:  Invalid OTP MAC addr: %02X%02X%02X%02X%02X%02X\n",
					       __func__,
					       (*(mac_addr + 0)),
					       (*(mac_addr + 1)),
					       (*(mac_addr + 2)),
					       (*(mac_addr + 3)),
					       (*(mac_addr + 4)),
					       (*(mac_addr + 5)));

		}
	}
out:
	return status;
}


enum nrf_wifi_status nrf_wifi_fmac_rf_params_get(
		struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
		unsigned char *rf_params,
		struct nrf_wifi_tx_pwr_ceil_params *tx_pwr_ceil_params)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_otp_info otp_info;
	unsigned int ft_prog_ver;
	int ret = -1;
#if defined(CONFIG_BOARD_NRF7002DK_NRF7001_NRF5340_CPUAPP) || \
	defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP) || \
	defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)
	unsigned char backoff_2g_dsss = 0, backoff_2g_ofdm = 0;
	unsigned char backoff_5g_lowband = 0, backoff_5g_midband = 0, backoff_5g_highband = 0;
#endif

	if (!fmac_dev_ctx || !rf_params) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto out;
	}

	nrf_wifi_osal_mem_set(fmac_dev_ctx->fpriv->opriv,
			      &otp_info,
			      0xFF,
			      sizeof(otp_info));

	status = nrf_wifi_hal_otp_info_get(fmac_dev_ctx->hal_dev_ctx,
					   &otp_info.info,
					   &otp_info.flags);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Fetching of RPU OTP information failed\n",
				      __func__);
		goto out;
	}

	status = nrf_wifi_hal_otp_ft_prog_ver_get(fmac_dev_ctx->hal_dev_ctx,
						  &ft_prog_ver);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Fetching of FT program version failed\n",
				      __func__);
		goto out;
	}

	nrf_wifi_osal_mem_set(fmac_dev_ctx->fpriv->opriv,
			      rf_params,
			      0xFF,
			      NRF_WIFI_RF_PARAMS_SIZE);

	ret = nrf_wifi_utils_hex_str_to_val(fmac_dev_ctx->fpriv->opriv,
					    rf_params,
					    NRF_WIFI_RF_PARAMS_SIZE,
					    NRF_WIFI_DEF_RF_PARAMS);

	if (ret == -1) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Initialization of RF params with default values failed\n",
				      __func__);
		status = NRF_WIFI_STATUS_FAIL;
		goto out;
	}

	if (!(otp_info.flags & (~CALIB_XO_FLAG_MASK))) {
		nrf_wifi_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				      &rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_X0],
				      (char *)otp_info.info.calib + OTP_OFF_CALIB_XO,
				      OTP_SZ_CALIB_XO);

	}

	ft_prog_ver = (ft_prog_ver & FT_PROG_VER_MASK) >> 16;

#if defined(CONFIG_BOARD_NRF7002DK_NRF7001_NRF5340_CPUAPP) || \
	defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP) || \
	defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)
	if (tx_pwr_ceil_params->rf_tx_pwr_ceil_params_override) {
		if (ft_prog_ver == FT_PROG_VER1) {
			backoff_2g_dsss = FT_PROG_VER1_2G_DSSS_TXCEIL_BKOFF;
			backoff_2g_ofdm = FT_PROG_VER1_2G_OFDM_TXCEIL_BKOFF;
			backoff_5g_lowband = FT_PROG_VER1_5G_LOW_OFDM_TXCEIL_BKOFF;
			backoff_5g_midband = FT_PROG_VER1_5G_MID_OFDM_TXCEIL_BKOFF;
			backoff_5g_highband = FT_PROG_VER1_5G_HIGH_OFDM_TXCEIL_BKOFF;
		} else if (ft_prog_ver == FT_PROG_VER2) {
			backoff_2g_dsss = FT_PROG_VER2_2G_DSSS_TXCEIL_BKOFF;
			backoff_2g_ofdm = FT_PROG_VER2_2G_OFDM_TXCEIL_BKOFF;
			backoff_5g_lowband = FT_PROG_VER2_5G_LOW_OFDM_TXCEIL_BKOFF;
			backoff_5g_midband = FT_PROG_VER2_5G_MID_OFDM_TXCEIL_BKOFF;
			backoff_5g_highband = FT_PROG_VER2_5G_HIGH_OFDM_TXCEIL_BKOFF;
		} else if (ft_prog_ver == FT_PROG_VER3) {
			backoff_2g_dsss = FT_PROG_VER3_2G_DSSS_TXCEIL_BKOFF;
			backoff_2g_ofdm = FT_PROG_VER3_2G_OFDM_TXCEIL_BKOFF;
			backoff_5g_lowband = FT_PROG_VER3_5G_LOW_OFDM_TXCEIL_BKOFF;
			backoff_5g_midband = FT_PROG_VER3_5G_MID_OFDM_TXCEIL_BKOFF;
			backoff_5g_highband = FT_PROG_VER3_5G_HIGH_OFDM_TXCEIL_BKOFF;
		}
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR2G] =
			tx_pwr_ceil_params->max_pwr_2g_dsss-backoff_2g_dsss;
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR2GM0M7] =
			tx_pwr_ceil_params->max_pwr_2g_mcs7-backoff_2g_ofdm;
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR2GM0M7 + 1] =
			tx_pwr_ceil_params->max_pwr_2g_mcs0-backoff_2g_ofdm;

		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR5GM7] =
			tx_pwr_ceil_params->max_pwr_5g_low_mcs7-backoff_5g_lowband;
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR5GM7 + 1] =
			tx_pwr_ceil_params->max_pwr_5g_mid_mcs7-backoff_5g_midband;
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR5GM7 + 2] =
			tx_pwr_ceil_params->max_pwr_5g_high_mcs7-backoff_5g_highband;
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR5GM0] =
			tx_pwr_ceil_params->max_pwr_5g_low_mcs0-backoff_5g_lowband;
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR5GM0 + 1] =
			tx_pwr_ceil_params->max_pwr_5g_mid_mcs0-backoff_5g_midband;
		rf_params[NRF_WIFI_RF_PARAMS_OFF_CALIB_PWR5GM0 + 2] =
			tx_pwr_ceil_params->max_pwr_5g_high_mcs0-backoff_5g_highband;
	}
#endif

	status = NRF_WIFI_STATUS_SUCCESS;
out:
	return status;
}


enum nrf_wifi_status nrf_wifi_fmac_set_reg(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					   struct nrf_wifi_fmac_reg_info *reg_info)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_cmd_req_set_reg *set_reg_cmd = NULL;

	if (!fmac_dev_ctx || !reg_info) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto out;
	}

	set_reg_cmd = nrf_wifi_osal_mem_zalloc(fmac_dev_ctx->fpriv->opriv,
					       sizeof(*set_reg_cmd));

	if (!set_reg_cmd) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Unable to allocate memory\n",
				      __func__);
		goto out;
	}

	set_reg_cmd->umac_hdr.cmd_evnt = NRF_WIFI_UMAC_CMD_REQ_SET_REG;
	set_reg_cmd->umac_hdr.ids.valid_fields = 0;

	nrf_wifi_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			      set_reg_cmd->nrf_wifi_alpha2,
			      reg_info->alpha2,
			      NRF_WIFI_COUNTRY_CODE_LEN);

	set_reg_cmd->valid_fields = NRF_WIFI_CMD_REQ_SET_REG_ALPHA2_VALID;

	/* New feature in rev B patch */
	if (reg_info->force) {
		set_reg_cmd->valid_fields |= NRF_WIFI_CMD_REQ_SET_REG_USER_REG_FORCE;
	}

	status = umac_cmd_cfg(fmac_dev_ctx,
			      set_reg_cmd,
			      sizeof(*set_reg_cmd));
out:
	if (set_reg_cmd) {
		nrf_wifi_osal_mem_free(fmac_dev_ctx->fpriv->opriv,
				       set_reg_cmd);
	}

	return status;
}

enum nrf_wifi_status nrf_wifi_fmac_get_reg(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					   struct nrf_wifi_fmac_reg_info *reg_info)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_umac_cmd_get_reg *get_reg_cmd = NULL;
	unsigned int count = 0;

	if (!fmac_dev_ctx || !reg_info) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto err;
	}

	get_reg_cmd = nrf_wifi_osal_mem_zalloc(fmac_dev_ctx->fpriv->opriv,
					       sizeof(*get_reg_cmd));

	if (!get_reg_cmd) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Unable to allocate memory\n",
				      __func__);
		goto err;
	}

	get_reg_cmd->umac_hdr.cmd_evnt = NRF_WIFI_UMAC_CMD_GET_REG;
	get_reg_cmd->umac_hdr.ids.valid_fields = 0;

	status = umac_cmd_cfg(fmac_dev_ctx,
			      get_reg_cmd,
			      sizeof(*get_reg_cmd));

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Failed to get regulatory information\n",	__func__);
		goto err;
	}

	fmac_dev_ctx->alpha2_valid = false;

	do {
		nrf_wifi_osal_sleep_ms(fmac_dev_ctx->fpriv->opriv,
				       100);
	} while (count++ < 100 && !fmac_dev_ctx->alpha2_valid);

	if (!fmac_dev_ctx->alpha2_valid) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Failed to get regulatory information\n",
				      __func__);
		goto err;
	}

	nrf_wifi_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
		   reg_info->alpha2,
	       fmac_dev_ctx->alpha2,
	       sizeof(reg_info->alpha2));

	return NRF_WIFI_STATUS_SUCCESS;
err:
	return NRF_WIFI_STATUS_FAIL;
}

#ifdef CONFIG_NRF700X_UTIL
enum nrf_wifi_status nrf_wifi_fmac_set_tx_rate(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					       unsigned char rate_flag,
					       int data_rate)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_fix_tx_rate *umac_cmd_data = NULL;
	int len = 0;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: umac_cmd_alloc failed\n",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_fix_tx_rate *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_TX_FIX_DATA_RATE;
	umac_cmd_data->sys_head.len = len;

	umac_cmd_data->rate_flags = rate_flag;
	umac_cmd_data->fixed_rate = data_rate;

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));
out:
	return status;
}

#ifdef CONFIG_NRF_WIFI_LOW_POWER
enum nrf_wifi_status nrf_wifi_fmac_get_host_rpu_ps_ctrl_state(void *dev_ctx,
							      int *rpu_ps_ctrl_state)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;

	fmac_dev_ctx = dev_ctx;

	if (!fmac_dev_ctx || !rpu_ps_ctrl_state) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto out;
	}


	status = nrf_wifi_hal_get_rpu_ps_state(fmac_dev_ctx->hal_dev_ctx,
					       rpu_ps_ctrl_state);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Fetching of RPU PS state failed\n",
				      __func__);
		goto out;
	}
out:
	return status;
}
#endif /* CONFIG_NRF_WIFI_LOW_POWER */
#endif /* CONFIG_NRF700X_UTIL */

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing OS specific definitions for the
 * Linux OS layer of the Wi-Fi driver.
 */

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/printk.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#ifdef BUS_IF_PCIE
#include <linux/pci.h>
#endif
#include <linux/netdevice.h>
#include <linux/bug.h>
#include <net/cfg80211.h>
#include "osal_api.h"
#include "osal_ops.h"
#include "fmac_api.h"
#include "lnx_main.h"
#include "lnx_shim.h"
#include "pal.h"
#ifdef RPU_MODE_EXPLORER
#ifdef HACK_SERDES_CONFIG
#include "vcu118.h"
#endif
#endif

static void *lnx_shim_mem_alloc(size_t size)
{
	return kmalloc(size, GFP_ATOMIC);
}


static void *lnx_shim_mem_zalloc(size_t size)
{
	return kzalloc(size, GFP_ATOMIC);
}


static void lnx_shim_mem_free(void *addr)
{
	kfree((const void*)addr);
}


static void *lnx_shim_mem_cpy(void *dest,
			      const void *src,
			      size_t count)
{
	return memcpy(dest, src, count);
}


static void * lnx_shim_mem_set(void *start,
			       int val,
			       size_t size)
{
	return memset(start, val, size);
}


static void * lnx_shim_iomem_mmap(unsigned long addr, unsigned long size)
{
	return ioremap(addr, size);	
}


static void lnx_shim_iomem_unmap(volatile void *addr)
{
	iounmap(addr);
}

#ifdef SOC_WEZEN
#ifdef INLINE_RX
static void * lnx_shim_iomem_mmap_inline_rx(unsigned long addr, unsigned long size)
{
        if (request_mem_region(HOST_PKTRAM_BB_START, HOST_PKTRAM_BB_LEN,  "host_rpu_ram")) {
		return ioremap_nocache(HOST_PKTRAM_BB_START, HOST_PKTRAM_BB_LEN);
        }
}
static void lnx_shim_iomem_unmap_inline_rx(volatile void *addr)
{
	iounmap(addr);
	release_mem_region(HOST_PKTRAM_BB_START, HOST_PKTRAM_BB_LEN);
}
#endif
#endif

static unsigned int lnx_shim_iomem_read_reg32(const volatile void *addr)
{
	return readl(addr);
}


static void lnx_shim_iomem_write_reg32(volatile void *addr, unsigned int val)
{
	writel(val, addr);
}


static void lnx_shim_iomem_cpy_from(void *dest, const volatile void *src, size_t count)
{
	memcpy_fromio(dest, src, count);
}


static void lnx_shim_iomem_cpy_to(volatile void *dest, const void *src, size_t count)
{
	memcpy_toio(dest, src, count);
}


static void *lnx_shim_spinlock_alloc(void)
{
	spinlock_t *lock = NULL;

	lock = kmalloc(sizeof(*lock), GFP_ATOMIC);

	if (!lock)
		pr_err("%s: Unable to allocate memory for spinlock\n", __func__);

	return lock;
}


static void lnx_shim_spinlock_free(void *lock)
{
	kfree(lock);
}


static void lnx_shim_spinlock_init(void *lock)
{
	spin_lock_init((spinlock_t *)lock);
}


static void lnx_shim_spinlock_take(void *lock)
{
	spin_lock_bh((spinlock_t *)lock);
}


static void lnx_shim_spinlock_rel(void *lock)
{
	spin_unlock_bh((spinlock_t *)lock);
}

static void lnx_shim_spinlock_irq_take(void *lock, unsigned long *flags)
{
	spin_lock_irqsave((spinlock_t *)lock, *flags);
}


static void lnx_shim_spinlock_irq_rel(void *lock, unsigned long *flags)
{
	spin_unlock_irqrestore((spinlock_t *)lock, *flags);
}


static int lnx_shim_pr_dbg(const char *fmt, va_list args)
{
	char *mod_fmt = NULL;
	int ret = -1;

	mod_fmt = kmalloc(strlen(fmt) + 1 + 3, GFP_ATOMIC);

	if (!mod_fmt) {
		pr_err("%s: Unable to allocate memory for mod_fmt\n",
		       __func__);
		return -1;
	}

	strcpy(mod_fmt, KERN_DEBUG);
	strcat(mod_fmt, fmt);

	ret = vprintk(mod_fmt, args);

	return ret;
}


static int lnx_shim_pr_info(const char *fmt, va_list args)
{
	char *mod_fmt = NULL;
	int ret = -1;

	mod_fmt = kmalloc(strlen(fmt) + 1 + 3, GFP_ATOMIC);

	if (!mod_fmt) {
		pr_err("%s: Unable to allocate memory for mod_fmt\n",
		       __func__);
		return -1;
	}

	strcpy(mod_fmt, KERN_INFO);
	strcat(mod_fmt, fmt);

	ret = vprintk(mod_fmt, args);

	return ret;
}


static int lnx_shim_pr_err(const char *fmt, va_list args)
{
	char *mod_fmt = NULL;
	int ret = -1;

	mod_fmt = kmalloc(strlen(fmt) + 1 + 3, GFP_ATOMIC);

	if (!mod_fmt) {
		pr_err("%s: Unable to allocate memory for mod_fmt\n",
		       __func__);
		return -1;
	}

	strcpy(mod_fmt, KERN_ERR);
	strcat(mod_fmt, fmt);

	ret = vprintk(mod_fmt, args);

	return ret;
}


static void *lnx_shim_nbuf_alloc(unsigned int size)
{
	struct sk_buff *nbuf = NULL;

	nbuf = alloc_skb(size, GFP_ATOMIC);

	if (!nbuf)
		pr_err("%s: Unable to allocate memory for network buffer\n", __func__);

	return nbuf;
}


static void lnx_shim_nbuf_free(void *nbuf)
{
	kfree_skb(nbuf);
}


static void lnx_shim_nbuf_headroom_res(void *nbuf,
				       unsigned int size)
{
	skb_reserve(nbuf, size);
}


static unsigned int lnx_shim_nbuf_headroom_get(void *nbuf)
{
	struct sk_buff *skb = (struct sk_buff *)nbuf;

	return (skb->data - skb->head);
}

static unsigned int lnx_shim_nbuf_data_size(void *nbuf)
{
	struct sk_buff *skb = (struct sk_buff *)nbuf;

	return skb->len;
}


static void *lnx_shim_nbuf_data_get(void *nbuf)
{
	struct sk_buff *skb = (struct sk_buff *)nbuf;

	return skb->data;
}


static void *lnx_shim_nbuf_data_put(void *nbuf,
				    unsigned int size)
{
	return skb_put(nbuf, size);
}


static void *lnx_shim_nbuf_data_push(void *nbuf,
				     unsigned int size)
{
	return skb_push(nbuf, size);
}


static void *lnx_shim_nbuf_data_pull(void *nbuf,
				     unsigned int size)
{
	return skb_pull(nbuf, size);
}

static unsigned char lnx_shim_nbuf_get_priority(void *nbuf)
{
	struct sk_buff *skb = (struct sk_buff *)nbuf;

        return skb->priority;
}

static void *lnx_shim_llist_node_alloc(void)
{
	struct lnx_shim_llist_node *llist_node = NULL;

	llist_node = kzalloc(sizeof(*llist_node), GFP_ATOMIC);

	if (!llist_node)
		pr_err("%s: Unable to allocate memory for linked list node\n", __func__);

	return llist_node;
}


static void lnx_shim_llist_node_free(void *llist_node)
{
	kfree(llist_node);
}


static void *lnx_shim_llist_node_data_get(void *llist_node)
{
	struct lnx_shim_llist_node *lnx_llist_node = NULL;

	lnx_llist_node = (struct lnx_shim_llist_node *)llist_node;

	return lnx_llist_node->data;
}


static void lnx_shim_llist_node_data_set(void *llist_node,
					 void *data)
{
	struct lnx_shim_llist_node *lnx_llist_node = NULL;

	lnx_llist_node = (struct lnx_shim_llist_node *)llist_node;

	lnx_llist_node->data = data;
}


static void *lnx_shim_llist_alloc(void)
{
	struct lnx_shim_llist *llist = NULL;

	llist = kzalloc(sizeof(*llist), GFP_ATOMIC);

	if (!llist)
		pr_err("%s: Unable to allocate memory for linked list\n", __func__);

	return llist;
}


static void lnx_shim_llist_free(void *llist)
{
	kfree(llist);
}


static void lnx_shim_llist_init(void *llist)
{
	struct lnx_shim_llist *lnx_llist = NULL;

	lnx_llist = (struct lnx_shim_llist *)llist;

	INIT_LIST_HEAD(&lnx_llist->head);
}


static void lnx_shim_llist_add_node_tail(void *llist,
					 void *llist_node)
{
	struct lnx_shim_llist *lnx_llist = NULL;

	lnx_llist = (struct lnx_shim_llist *)llist;

	list_add_tail(llist_node, &lnx_llist->head);

	lnx_llist->len += 1;
}


static void *lnx_shim_llist_get_node_head(void *llist)
{
	struct lnx_shim_llist_node *lnx_head_node = NULL;
	struct lnx_shim_llist *lnx_llist = NULL;

	lnx_llist = (struct lnx_shim_llist *)llist;

	if (!lnx_llist->len)
		return NULL;

	lnx_head_node = list_first_entry(&lnx_llist->head, struct lnx_shim_llist_node, head);

	return lnx_head_node;
}


static void *lnx_shim_llist_get_node_nxt(void *llist, void *llist_node)
{
	struct lnx_shim_llist_node *lnx_node = NULL;
	struct lnx_shim_llist_node *lnx_nxt_node = NULL;
	struct lnx_shim_llist *lnx_llist = NULL;

	lnx_llist = (struct lnx_shim_llist *)llist;
	lnx_node = (struct lnx_shim_llist_node *)llist_node;

	if (lnx_node->head.next == &lnx_llist->head)
		return NULL;

	lnx_nxt_node = list_next_entry(lnx_node, head);

	return lnx_nxt_node;
}


static void lnx_shim_llist_del_node(void *llist,
				    void *llist_node)
{
	struct lnx_shim_llist_node *lnx_node = NULL;
	struct lnx_shim_llist *lnx_llist = NULL;

	lnx_llist = (struct lnx_shim_llist *)llist;
	lnx_node = (struct lnx_shim_llist_node *)llist_node;

	list_del(&lnx_node->head);

	lnx_llist->len -= 1;
}


static unsigned int lnx_shim_llist_len(void *llist)
{
	struct lnx_shim_llist *lnx_llist = NULL;

	lnx_llist = (struct lnx_shim_llist *)llist;

	return lnx_llist->len;
}


static void *lnx_shim_tasklet_alloc(int type)
{
	struct tasklet_struct *tasklet = NULL;

	tasklet = kmalloc(sizeof(*tasklet), GFP_ATOMIC);

	if (!tasklet)
		pr_err("%s: Unable to allocate memory for tasklet\n", __func__);

	return tasklet;
}


static void lnx_shim_tasklet_free(void *tasklet)
{
	kfree(tasklet);
}


static void lnx_shim_tasklet_init(void *tasklet,
				  void (*callback)(unsigned long),
				  unsigned long data)
{
	tasklet_init(tasklet,
		     callback,
		     data);
}


static void lnx_shim_tasklet_schedule(void *tasklet)
{
	tasklet_schedule(tasklet);
}


static void lnx_shim_tasklet_kill(void *tasklet)
{
	tasklet_kill(tasklet);
}


static int lnx_shim_msleep(int msecs)
{
	if (msecs > 20)
		msleep((unsigned int)msecs);
	else
		usleep_range((msecs * 1000),
			     (msecs * 1000 + 500));

	return 0;
}


static int lnx_shim_udelay(int usecs)
{
	udelay((unsigned long)usecs);

	return 0;
}

static unsigned long lnx_shim_time_get_curr_us(void)
{
	struct timespec64 curr_time;
	unsigned long curr_time_us = 0;
	ktime_get_real_ts64(&curr_time);
	curr_time_us = (curr_time.tv_sec * 1000 * 1000) + (curr_time.tv_nsec / 1000);

	return (curr_time_us);
}


static unsigned int lnx_shim_time_elapsed_us(unsigned long start_time_us)
{
	struct timespec64 curr_time;
	unsigned long curr_time_us = 0;
	ktime_get_real_ts64(&curr_time);
	curr_time_us = (curr_time.tv_sec * 1000 * 1000) + (curr_time.tv_nsec / 1000);

	return ((curr_time_us - start_time_us));
}

#ifdef BUS_IF_PCIE
static irqreturn_t lnx_shim_irq_handler(int irq, void *p)
{
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	int ret = 0;

	lnx_pcie_dev_ctx = (struct lnx_shim_bus_pcie_dev_ctx *)p;

	ret = lnx_pcie_dev_ctx->intr_callbk_fn(lnx_pcie_dev_ctx->intr_callbk_data);

	if (ret) {
		pr_err("%s: Interrupt callback failed\n", __func__);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}


static void lnx_shim_bus_pcie_dev_host_map_get(void *os_pcie_dev_ctx,
					       struct nrf_wifi_osal_host_map *host_map)
{
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;

	if (!os_pcie_dev_ctx || !host_map) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;

	host_map->addr = lnx_pcie_dev_ctx->host_map.addr;
	host_map->size = lnx_pcie_dev_ctx->host_map.size;
}


static void *lnx_shim_bus_pcie_dev_dma_map(void *os_pcie_dev_ctx,
					   void *virt_addr,
					   size_t size,
					   enum nrf_wifi_osal_dma_dir dir)
{
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	struct pci_dev *pdev = NULL;
	enum dma_data_direction dma_dir = DMA_NONE;
	dma_addr_t dma_buf = 0;

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;
	pdev = lnx_pcie_dev_ctx->pdev;

	if (dir == NRF_WIFI_OSAL_DMA_DIR_TO_DEV)
		dma_dir = DMA_TO_DEVICE;
	else if (dir == NRF_WIFI_OSAL_DMA_DIR_FROM_DEV)
		dma_dir = DMA_FROM_DEVICE;
	else if (dir == NRF_WIFI_OSAL_DMA_DIR_BIDI)
		dma_dir = DMA_BIDIRECTIONAL;
	else
		pr_err("%s: Invalid DMA direction (%d)\n", __func__, dir);

	dma_buf = dma_map_single(&pdev->dev,
				 virt_addr,
				 size,
				 dma_dir);

	if (unlikely(dma_mapping_error(&pdev->dev,
				       dma_buf))) {
		pr_err("%s Unable to map DMA on TX\n", __func__);
		return NULL;
	}

	return (void *)dma_buf;
}

static void lnx_shim_bus_pcie_dev_dma_unmap(void *os_pcie_dev_ctx,
					    void *dma_addr,
					    size_t size,
					    enum nrf_wifi_osal_dma_dir dir)
{
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	struct pci_dev *pdev = NULL;
	enum dma_data_direction dma_dir = DMA_NONE;

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;
	pdev = lnx_pcie_dev_ctx->pdev;

	if (dir == NRF_WIFI_OSAL_DMA_DIR_TO_DEV)
		dma_dir = DMA_TO_DEVICE;
	else if (dir == NRF_WIFI_OSAL_DMA_DIR_FROM_DEV)
		dma_dir = DMA_FROM_DEVICE;
	else if (dir == NRF_WIFI_OSAL_DMA_DIR_BIDI)
		dma_dir = DMA_BIDIRECTIONAL;
	else
		pr_err("%s: Invalid DMA direction (%d)\n", __func__, dir);

	dma_unmap_single(&pdev->dev,
			 (dma_addr_t)dma_addr,
			 size,
			 dma_dir);
}


enum nrf_wifi_status lnx_shim_bus_pcie_dev_intr_reg(void *os_pcie_dev_ctx,
						     void *callbk_data,
						     int (*callbk_fn)(void *callbk_data))
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	int ret = -1;
	unsigned int irq_flags = 0;

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;

	lnx_pcie_dev_ctx->intr_callbk_data = callbk_data;
	lnx_pcie_dev_ctx->intr_callbk_fn = callbk_fn;

#ifdef notyet
	/* TODO: See why this flag is needed (is it necessary for WoWLAN?) */
	if (lnx_pcie_dev_ctx->is_wakeup_src)
		irq_flags = IRQF_NO_SUSPEND;
#endif /* notyet */

	if(lnx_pcie_dev_ctx->is_msi == 0)
		irq_flags |= IRQF_SHARED;

	/* TODO: Replace wlan0 with a per RPU device name */
	ret = request_irq(lnx_pcie_dev_ctx->pdev->irq,
			  lnx_shim_irq_handler,
			  irq_flags,
			  "wlan0",
			  lnx_pcie_dev_ctx);

	if (ret) {
		pr_err("%s: request_irq failed\n", __func__);
		goto out;
	}

	status = NRF_WIFI_STATUS_SUCCESS;

out:
	return status;
}


void lnx_shim_bus_pcie_dev_intr_unreg(void *os_pcie_dev_ctx)
{
	struct pci_dev *pdev = NULL;
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;

	pdev = lnx_pcie_dev_ctx->pdev;

	free_irq(lnx_pcie_dev_ctx->pdev->irq, lnx_pcie_dev_ctx);

	if (lnx_pcie_dev_ctx->is_msi)
		pci_disable_msi(pdev);
}


static enum nrf_wifi_status lnx_shim_bus_pcie_dev_init(void *os_pcie_dev_ctx)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;

	lnx_pcie_dev_ctx->dev_init = true;

	status = NRF_WIFI_STATUS_SUCCESS;

	return status;
}


static void lnx_shim_bus_pcie_dev_deinit(void *os_pcie_dev_ctx)
{
	struct lnx_shim_bus_pcie_priv *lnx_pcie_priv = NULL;
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	struct pci_dev *pdev = NULL;

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;

	lnx_pcie_priv = lnx_pcie_dev_ctx->lnx_pcie_priv;
	pdev = lnx_pcie_dev_ctx->pdev;

#ifdef notyet
	interrupts_deinit(pdev);
	nrf_wifi_driver_deinit(fpriv);

	if (fpriv->rpu_mmap_addr_base)
		iounmap(fpriv->rpu_mmap_addr_base);

	/* Ideally we should call pci_clear_master api,
	 * which is in sync with pci_set_master. But while
	 * doing so, we got some issue at codescope. It is
	 * not able to access host memory, while device is
	 * not loaded and causing rpu hang. Need to test it
	 * further before enabling this.
	 */

	/*

	   pci_clear_master(pdev);

*/

#endif /* not yet */

	pci_disable_device(pdev);
}


static void *lnx_shim_bus_pcie_dev_add(void *os_pcie_priv,
				       void *osal_pcie_dev_ctx)
{
	struct lnx_shim_bus_pcie_priv *lnx_pcie_priv = NULL;
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	struct pci_dev *pdev = NULL;
	int ret = -1;

	lnx_pcie_priv = os_pcie_priv;

	pdev = lnx_pcie_priv->pdev;

	lnx_pcie_dev_ctx = kzalloc(sizeof(*lnx_pcie_dev_ctx), GFP_ATOMIC);

	if (!lnx_pcie_dev_ctx) {
		pr_err("%s: Unable to allocate memory for lnx_pcie_dev_ctx\n", __func__);
		goto out;
	}

	lnx_pcie_dev_ctx->pdev = pdev;
	lnx_pcie_dev_ctx->lnx_pcie_priv = lnx_pcie_priv;
	lnx_pcie_dev_ctx->osal_pcie_dev_ctx = osal_pcie_dev_ctx;

	/* Enable the PCIe device */
	ret = pci_enable_device(pdev);

	if (ret) {
		pr_err("%s, pci_enable_device failed\n", __func__);
		goto out;
	}

	/* Enable bus-mastering on the device */
	pci_set_master(pdev);

	/* Mark PCI regions associated with pdev as reserved by our driver */
	ret = pci_request_regions(pdev,
				  pdev->driver->name);

	if (ret) {
		pr_err("%s, pci_request_regions failed\n", __func__);
		goto out;
	}

	/* Get the I/O bus address associated with the WLAN function in the
	 * PCIe device */
	/* TODO : Get PCIE_BAR_OFFSET_WLAN_RPU using PAL ops */
	lnx_pcie_dev_ctx->host_map.addr = pci_resource_start(pdev, PCIE_BAR_OFFSET_WLAN_RPU);
	lnx_pcie_dev_ctx->host_map.size = pci_resource_len(pdev, PCIE_BAR_OFFSET_WLAN_RPU);

	/* Ideally the WLAN should not need to map more than 16MB of memory to
	 * make all the regions (Sysbus, GRAM, PBUS) accessible. We still
	 * are allowing upto 256MB to be mapped. If it exceeds this however
	 * something is not right and it is better to check with the RTL team
	 * to check if this much memory is really needed
	 */
	if (lnx_pcie_dev_ctx->host_map.size > (256 * 1024 * 1024)) {
		pr_err("%s: pci_resource_len = %lu, too large\n",
		       __func__,
		       lnx_pcie_dev_ctx->host_map.size);
		ret = -1;
		goto out;
	}
#if 0
	if (pci_enable_msi(pdev))
		lnx_pcie_dev_ctx->is_msi = 0;
#endif
	lnx_pcie_dev_ctx->dev_added = true;
	pci_set_drvdata(pdev, lnx_pcie_dev_ctx);
out:
	return lnx_pcie_dev_ctx;
}


static void lnx_shim_bus_pcie_dev_rem(void *os_pcie_dev_ctx)
{
	struct lnx_shim_bus_pcie_priv *lnx_pcie_priv = NULL;
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	struct pci_dev *pdev = NULL;

	lnx_pcie_dev_ctx = os_pcie_dev_ctx;

	lnx_pcie_priv = lnx_pcie_dev_ctx->lnx_pcie_priv;
	pdev = lnx_pcie_dev_ctx->pdev;

	pci_set_drvdata(pdev, NULL);
	pci_release_regions(pdev);

	kfree(lnx_pcie_dev_ctx);
}


static int lnx_shim_bus_pcie_probe(struct pci_dev *pdev,
				   const struct pci_device_id *id)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct lnx_shim_bus_pcie_priv *lnx_pcie_priv = NULL;
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;
	struct nrf_wifi_ctx_lnx *lnx_rpu_ctx = NULL;
	int ret = -1;

	lnx_pcie_priv = (struct lnx_shim_bus_pcie_priv *)id->driver_data;

	if (lnx_pcie_priv->pdev) {
		pr_err("%s: Previous detected device still not added\n", __func__);
		goto out;
	}

	lnx_pcie_priv->pdev = pdev;

	lnx_rpu_ctx = nrf_wifi_fmac_dev_add_lnx();

	if (!lnx_rpu_ctx) {
		pr_err("%s: nrf_wifi_fmac_dev_add_lnx failed\n", __func__);
		goto out;
	}

	lnx_pcie_dev_ctx = pci_get_drvdata(pdev);

	lnx_pcie_dev_ctx->lnx_rpu_ctx = lnx_rpu_ctx;

	status = nrf_wifi_fmac_dev_init_lnx(lnx_rpu_ctx);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		pr_err("%s: nrf_wifi_fmac_dev_init_lnx failed\n", __func__);
		goto out;
	}

	lnx_pcie_priv->pdev = NULL;
	ret = 0;
out:
	return ret;
}


static void lnx_shim_bus_pcie_remove(struct pci_dev *pdev)
{
	struct lnx_shim_bus_pcie_dev_ctx *lnx_pcie_dev_ctx = NULL;

	lnx_pcie_dev_ctx = pci_get_drvdata(pdev);

	if (lnx_pcie_dev_ctx->dev_init)
		nrf_wifi_fmac_dev_deinit_lnx(lnx_pcie_dev_ctx->lnx_rpu_ctx);

	if (lnx_pcie_dev_ctx->dev_added)
		nrf_wifi_fmac_dev_rem_lnx(lnx_pcie_dev_ctx->lnx_rpu_ctx);
}


static void lnx_shim_bus_pcie_reg_drv(struct work_struct *drv_reg_work)
{
	struct lnx_shim_bus_pcie_priv *lnx_pcie_priv = NULL;

	lnx_pcie_priv = container_of(drv_reg_work, struct lnx_shim_bus_pcie_priv,  drv_reg);

	if (pci_register_driver(lnx_pcie_priv->pcie_drv)) {
		pr_err("%s: Registration of PCIe driver failed\n", __func__);
		kfree(lnx_pcie_priv->pdev_id);
		kfree(lnx_pcie_priv->pcie_drv);
		kfree(lnx_pcie_priv);
	}
}


void *lnx_shim_bus_pcie_init(const char *drv_name,
			     unsigned int vendor_id,
			     unsigned int sub_vendor_id,
			     unsigned int device_id,
			     unsigned int sub_device_id)
{
	struct pci_driver *lnx_pcie_drv = NULL;
	struct pci_device_id *lnx_pdev_id = NULL;
	struct lnx_shim_bus_pcie_priv *lnx_pcie_priv = NULL;

	lnx_pcie_priv = kzalloc(sizeof(*lnx_pcie_priv), GFP_ATOMIC);

	if (!lnx_pcie_priv) {
		pr_err("%s: Unable to allocate memory for lnx_pcie_priv\n", __func__);
		goto out;
	}

	lnx_pcie_drv = kzalloc(sizeof(*lnx_pcie_drv), GFP_ATOMIC);

	if (!lnx_pcie_drv) {
		pr_err("%s: Unable to allocate memory for pcie_drv\n", __func__);
		kfree(lnx_pcie_priv);
		lnx_pcie_priv = NULL;
		goto out;
	}

	lnx_pdev_id = kzalloc(sizeof(*lnx_pdev_id), GFP_ATOMIC);

	if (!lnx_pdev_id) {
		pr_err("%s: Unable to allocate memory for pdev_id\n", __func__);
		kfree(lnx_pcie_drv);
		lnx_pcie_drv = NULL;
		kfree(lnx_pcie_priv);
		lnx_pcie_priv = NULL;
		goto out;
	}

	lnx_pcie_priv->pcie_drv = lnx_pcie_drv;
	lnx_pcie_priv->pdev_id = lnx_pdev_id;

	lnx_pdev_id->vendor = vendor_id;
	lnx_pdev_id->subvendor = sub_vendor_id;
	lnx_pdev_id->device = device_id;
	lnx_pdev_id->subdevice = sub_device_id;
	lnx_pdev_id->driver_data = (kernel_ulong_t)lnx_pcie_priv;

	lnx_pcie_drv->name = drv_name;
	lnx_pcie_drv->id_table = lnx_pdev_id;
	lnx_pcie_drv->probe = lnx_shim_bus_pcie_probe;
	lnx_pcie_drv->remove = lnx_shim_bus_pcie_remove;

	INIT_WORK(&lnx_pcie_priv->drv_reg, lnx_shim_bus_pcie_reg_drv);

	schedule_work(&lnx_pcie_priv->drv_reg);
out:
	return lnx_pcie_priv;
}


void lnx_shim_bus_pcie_deinit(void *os_pcie_priv)
{
	struct lnx_shim_bus_pcie_priv *lnx_pcie_priv = NULL;

	lnx_pcie_priv = os_pcie_priv;

	pci_unregister_driver(lnx_pcie_priv->pcie_drv);

	kfree(lnx_pcie_priv->pcie_drv);
	lnx_pcie_priv->pcie_drv = NULL;

	kfree(lnx_pcie_priv->pdev_id);
	lnx_pcie_priv->pdev_id = NULL;

	kfree(lnx_pcie_priv);
}
#endif

#ifdef CONFIG_NRF_WIFI_LOW_POWER

struct timer_info {
	struct timer_list sleep_timer;
	void (*callback)(unsigned long);
	unsigned long ctx;
};

static void *lnx_shim_timer_alloc(void)
{
	struct timer_info *timer_info = NULL;

	timer_info = kmalloc(sizeof(*timer_info), GFP_ATOMIC);

	if (!timer_info)
		pr_err("%s: Unable to allocate memory for timer\n", __func__);

	return timer_info;
}

static void lnx_shim_timer_task(struct timer_list *t)
{
	struct timer_info *timer_info = from_timer(timer_info,
						   t,
						   sleep_timer);

	timer_info->callback(timer_info->ctx);
}

static void lnx_shim_timer_init(void *timer_info,
				void (*callback)(unsigned long),
				unsigned long data)
{
	struct timer_info *info = (struct timer_info *)timer_info;
	info->callback = callback;
	info->ctx = data;
	timer_setup(&info->sleep_timer,
		    lnx_shim_timer_task,
		    0);
}

static void lnx_shim_timer_free(void *timer)
{
	kfree(timer);
}

static void lnx_shim_timer_schedule(void *timer, unsigned long duration)
{
	mod_timer(timer, jiffies + msecs_to_jiffies(duration));
}


static void lnx_shim_timer_kill(void *timer)
{
	del_timer_sync(timer);
}
#endif

static void lnx_shim_assert(int test_val, int val, enum nrf_wifi_assert_op_type op,
                        char *msg)
{
	switch (op) {
        case NRF_WIFI_ASSERT_EQUAL_TO:
                WARN(test_val != val, "%s", msg);
                break;
        case NRF_WIFI_ASSERT_NOT_EQUAL_TO:
                WARN(test_val == val, "%s", msg);
                break;
        case NRF_WIFI_ASSERT_LESS_THAN:
                WARN(test_val >= val, "%s", msg);
                break;
        case NRF_WIFI_ASSERT_LESS_THAN_EQUAL_TO:
                WARN(test_val > val, "%s", msg);
                break;
        case NRF_WIFI_ASSERT_GREATER_THAN:
                WARN(test_val <= val, "%s", msg);
                break;
        case NRF_WIFI_ASSERT_GREATER_THAN_EQUAL_TO:
                WARN(test_val < val, "%s", msg);
                break;
        default:
                pr_err("%s: Invalid assertion operation\n", __func__);
        }
}


static int lnx_shim_mem_cmp(const void *addr1,
			    const void *addr2,
			    size_t size)
{
	return memcmp(addr1, addr2, size);
}

static unsigned int lnx_shim_str_len(const void *str)
{
	return strlen(str);
}


const struct nrf_wifi_osal_ops nrf_wifi_os_lnx_ops = {
	.mem_alloc = lnx_shim_mem_alloc,
	.mem_zalloc = lnx_shim_mem_zalloc,
	.mem_free = lnx_shim_mem_free,
	.mem_cpy = lnx_shim_mem_cpy,
	.mem_set = lnx_shim_mem_set,
#ifdef SOC_WEZEN
#ifdef INLINE_RX
        .iomem_mmap_inline_rx = lnx_shim_iomem_mmap_inline_rx,
        .iomem_unmap_inline_rx = lnx_shim_iomem_unmap_inline_rx,
#endif
#endif
	.iomem_mmap = lnx_shim_iomem_mmap,
	.iomem_unmap = lnx_shim_iomem_unmap,
	.iomem_read_reg32 = lnx_shim_iomem_read_reg32,
	.iomem_write_reg32 = lnx_shim_iomem_write_reg32,
	.iomem_cpy_from = lnx_shim_iomem_cpy_from,
	.iomem_cpy_to = lnx_shim_iomem_cpy_to,

	.spinlock_alloc = lnx_shim_spinlock_alloc,
	.spinlock_free  = lnx_shim_spinlock_free,
	.spinlock_init = lnx_shim_spinlock_init,
	.spinlock_take = lnx_shim_spinlock_take,
	.spinlock_rel = lnx_shim_spinlock_rel,

	.spinlock_irq_take = lnx_shim_spinlock_irq_take,
	.spinlock_irq_rel = lnx_shim_spinlock_irq_rel,

	.log_dbg = lnx_shim_pr_dbg,
	.log_info = lnx_shim_pr_info,
	.log_err  = lnx_shim_pr_err,

	.llist_node_alloc = lnx_shim_llist_node_alloc,
	.llist_node_free = lnx_shim_llist_node_free,
	.llist_node_data_get = lnx_shim_llist_node_data_get,
	.llist_node_data_set = lnx_shim_llist_node_data_set,

	.llist_alloc = lnx_shim_llist_alloc,
	.llist_free = lnx_shim_llist_free,
	.llist_init = lnx_shim_llist_init,
	.llist_add_node_tail = lnx_shim_llist_add_node_tail,
	.llist_get_node_head = lnx_shim_llist_get_node_head,
	.llist_get_node_nxt = lnx_shim_llist_get_node_nxt,
	.llist_del_node = lnx_shim_llist_del_node,
	.llist_len = lnx_shim_llist_len,

	.nbuf_alloc = lnx_shim_nbuf_alloc,
	.nbuf_free = lnx_shim_nbuf_free,
	.nbuf_headroom_res = lnx_shim_nbuf_headroom_res,
	.nbuf_headroom_get = lnx_shim_nbuf_headroom_get,
	.nbuf_data_size = lnx_shim_nbuf_data_size,
	.nbuf_data_get = lnx_shim_nbuf_data_get,
	.nbuf_data_put = lnx_shim_nbuf_data_put,
	.nbuf_data_push = lnx_shim_nbuf_data_push,
	.nbuf_data_pull = lnx_shim_nbuf_data_pull,
	.nbuf_get_priority = lnx_shim_nbuf_get_priority,


	.tasklet_alloc = lnx_shim_tasklet_alloc,
	.tasklet_free = lnx_shim_tasklet_free,
	.tasklet_init = lnx_shim_tasklet_init,
	.tasklet_schedule = lnx_shim_tasklet_schedule,
	.tasklet_kill = lnx_shim_tasklet_kill,

	.sleep_ms = lnx_shim_msleep,
	.delay_us = lnx_shim_udelay,
	.time_get_curr_us = lnx_shim_time_get_curr_us,
	.time_elapsed_us = lnx_shim_time_elapsed_us,
#ifdef BUS_IF_PCIE
	.bus_pcie_init = lnx_shim_bus_pcie_init,
	.bus_pcie_deinit = lnx_shim_bus_pcie_deinit,
	.bus_pcie_dev_add = lnx_shim_bus_pcie_dev_add,
	.bus_pcie_dev_rem = lnx_shim_bus_pcie_dev_rem,
	.bus_pcie_dev_init = lnx_shim_bus_pcie_dev_init,
	.bus_pcie_dev_deinit = lnx_shim_bus_pcie_dev_deinit,
	.bus_pcie_dev_intr_reg = lnx_shim_bus_pcie_dev_intr_reg,
	.bus_pcie_dev_intr_unreg = lnx_shim_bus_pcie_dev_intr_unreg,
	.bus_pcie_dev_dma_map = lnx_shim_bus_pcie_dev_dma_map,
	.bus_pcie_dev_dma_unmap = lnx_shim_bus_pcie_dev_dma_unmap,
	.bus_pcie_dev_host_map_get = lnx_shim_bus_pcie_dev_host_map_get,
#endif
#ifdef CONFIG_NRF_WIFI_LOW_POWER
	.timer_alloc = lnx_shim_timer_alloc,
	.timer_init = lnx_shim_timer_init,
	.timer_free = lnx_shim_timer_free,
	.timer_schedule = lnx_shim_timer_schedule,
	.timer_kill = lnx_shim_timer_kill,
#endif
	.assert = lnx_shim_assert,
	.mem_cmp = lnx_shim_mem_cmp,
	.strlen = lnx_shim_str_len,
};


const struct nrf_wifi_osal_ops *get_os_ops(void)
{
	return &nrf_wifi_os_lnx_ops;
}

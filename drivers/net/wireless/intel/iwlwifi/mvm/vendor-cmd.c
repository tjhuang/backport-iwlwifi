/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 - 2019 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 - 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include <net/netlink.h>
#include "mvm.h"
#include "iwl-vendor-cmd.h"

#ifdef CPTCFG_IWLWIFI_LTE_COEX
#include "lte-coex.h"
#endif

#include "iwl-io.h"
#include "iwl-prph.h"

static LIST_HEAD(device_list);
static DEFINE_SPINLOCK(device_list_lock);

static int iwl_mvm_netlink_notifier(struct notifier_block *nb,
				    unsigned long state,
				    void *_notify)
{
	struct netlink_notify *notify = _notify;
	struct iwl_mvm *mvm;

	if (state != NETLINK_URELEASE || notify->protocol != NETLINK_GENERIC)
		return NOTIFY_DONE;

	spin_lock_bh(&device_list_lock);
	list_for_each_entry(mvm, &device_list, list) {
		if (mvm->csi_portid == netlink_notify_portid(notify))
			mvm->csi_portid = 0;
	}
	spin_unlock_bh(&device_list_lock);

	return NOTIFY_OK;
}

static struct notifier_block iwl_mvm_netlink_notifier_block = {
	.notifier_call = iwl_mvm_netlink_notifier,
};

void iwl_mvm_vendor_cmd_init(void)
{
	WARN_ON(netlink_register_notifier(&iwl_mvm_netlink_notifier_block));
	spin_lock_init(&device_list_lock);
}

void iwl_mvm_vendor_cmd_exit(void)
{
	netlink_unregister_notifier(&iwl_mvm_netlink_notifier_block);
}

static const struct nla_policy
iwl_mvm_vendor_attr_policy[NUM_IWL_MVM_VENDOR_ATTR] = {
	[IWL_MVM_VENDOR_ATTR_LOW_LATENCY] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_COUNTRY] = { .type = NLA_STRING, .len = 2 },
	[IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_FILTER_GTK] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_ADDR] = { .len = ETH_ALEN },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_OPPPS_WA] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MAC_ADDR] = { .len = ETH_ALEN },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MAC_ADDR_MASK] = { .len = ETH_ALEN },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MAX_AP_PER_SCAN] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_REPORT_THRESHOLD] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_BUCKET_SPECS] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_GSCAN_LOST_AP_SAMPLE_SIZE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_AP_LIST] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_GSCAN_RSSI_SAMPLE_SIZE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MIN_BREACHING] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_RXFILTER] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_RXFILTER_OP] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER] = { .type = NLA_STRING },
	[IWL_MVM_VENDOR_ATTR_NAN_FAW_FREQ] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_NAN_FAW_SLOTS] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_REPORT_THRESHOLD_NUM] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_CCM] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_GCM] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_AES] = { .type = NLA_NESTED },
};

static struct nlattr **iwl_mvm_parse_vendor_data(const void *data, int data_len)
{
	struct nlattr **tb;
	int err;

	if (!data)
		return ERR_PTR(-EINVAL);

	tb = kcalloc(MAX_IWL_MVM_VENDOR_ATTR + 1, sizeof(*tb), GFP_KERNEL);
	if (!tb)
		return ERR_PTR(-ENOMEM);

	err = nla_parse(tb, MAX_IWL_MVM_VENDOR_ATTR, data, data_len,
			iwl_mvm_vendor_attr_policy, NULL);
	if (err) {
		kfree(tb);
		return ERR_PTR(err);
	}

	return tb;
}

static int iwl_mvm_set_low_latency(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	int err;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	bool low_latency;

	if (!vif)
		return -ENODEV;

	if (data) {
		tb = iwl_mvm_parse_vendor_data(data, data_len);
		if (IS_ERR(tb))
			return PTR_ERR(tb);
		low_latency = tb[IWL_MVM_VENDOR_ATTR_LOW_LATENCY];
		kfree(tb);
	} else {
		low_latency = false;
	}

	mutex_lock(&mvm->mutex);
	err = iwl_mvm_update_low_latency(mvm, vif, low_latency,
					 LOW_LATENCY_VCMD);
	mutex_unlock(&mvm->mutex);

	return err;
}

static int iwl_mvm_get_low_latency(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct iwl_mvm_vif *mvmvif;
	struct sk_buff *skb;

	if (!vif)
		return -ENODEV;
	mvmvif = iwl_mvm_vif_from_mac80211(vif);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (iwl_mvm_vif_low_latency(mvmvif) &&
	    nla_put_flag(skb, IWL_MVM_VENDOR_ATTR_LOW_LATENCY)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_set_country(struct wiphy *wiphy,
			       struct wireless_dev *wdev,
			       const void *data, int data_len)
{
	struct ieee80211_regdomain *regd;
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	int retval;

	if (!iwl_mvm_is_lar_supported(mvm))
		return -EOPNOTSUPP;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_COUNTRY]) {
		retval = -EINVAL;
		goto free;
	}

	mutex_lock(&mvm->mutex);

	/* set regdomain information to FW */
	regd = iwl_mvm_get_regdomain(wiphy,
				     nla_data(tb[IWL_MVM_VENDOR_ATTR_COUNTRY]),
				     iwl_mvm_is_wifi_mcc_supported(mvm) ?
				     MCC_SOURCE_3G_LTE_HOST :
				     MCC_SOURCE_OLD_FW, NULL);
	if (IS_ERR_OR_NULL(regd)) {
		retval = -EIO;
		goto unlock;
	}

	retval = regulatory_set_wiphy_regd(wiphy, regd);
	kfree(regd);
unlock:
	mutex_unlock(&mvm->mutex);
free:
	kfree(tb);
	return retval;
}

#ifdef CPTCFG_IWLWIFI_LTE_COEX
static int iwl_vendor_lte_coex_state_cmd(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_state_cmd *cmd = data;
	struct sk_buff *skb;
	int err = LTE_OK;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: state cmd:\n\tstate: %d\n",
		       cmd->lte_state);

	switch (cmd->lte_state) {
	case LTE_OFF:
		if (mvm->lte_state.has_config &&
		    mvm->lte_state.state != LTE_CONNECTED) {
			err = LTE_STATE_ERR;
			goto out;
		}
		mvm->lte_state.state = LTE_OFF;
		mvm->lte_state.has_config = 0;
		mvm->lte_state.has_rprtd_chan = 0;
		mvm->lte_state.has_sps = 0;
		mvm->lte_state.has_ft = 0;
		break;
	case LTE_IDLE:
		if (!mvm->lte_state.has_static ||
		    (mvm->lte_state.has_config &&
		     mvm->lte_state.state != LTE_CONNECTED)) {
			err = LTE_STATE_ERR;
			goto out;
		}
		mvm->lte_state.has_config = 0;
		mvm->lte_state.has_sps = 0;
		mvm->lte_state.state = LTE_IDLE;
		break;
	case LTE_CONNECTED:
		if (!(mvm->lte_state.has_config)) {
			err = LTE_STATE_ERR;
			goto out;
		}
		mvm->lte_state.state = LTE_CONNECTED;
		break;
	default:
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}

	mvm->lte_state.config.lte_state = cpu_to_le32(mvm->lte_state.state);

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_coex_config_cmd(mvm))
		err = LTE_OTHER_ERR;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_vendor_lte_coex_config_cmd(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_config_info_cmd *cmd = data;
	struct iwl_lte_coex_static_params_cmd *stat = &mvm->lte_state.stat;
	struct sk_buff *skb;
	int err = LTE_OK;
	int i, j;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: config cmd:\n");

	/* send static config only once in the FW life */
	if (mvm->lte_state.has_static)
		goto out;

	for (i = 0; i < LTE_MWS_CONF_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\tmws config data[%d]: %d\n", i,
			       cmd->mws_conf_data[i]);
		stat->mfu_config[i] = cpu_to_le32(cmd->mws_conf_data[i]);
	}

	if (cmd->safe_power_table[0] != LTE_SAFE_PT_FIRST ||
	    cmd->safe_power_table[LTE_SAFE_PT_LENGTH - 1] !=
							LTE_SAFE_PT_LAST) {
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}

	/* power table must be ascending ordered */
	j = LTE_SAFE_PT_FIRST;
	for (i = 0; i < LTE_SAFE_PT_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\tsafe power table[%d]: %d\n", i,
			       cmd->safe_power_table[i]);
		if (cmd->safe_power_table[i] < j) {
			err = LTE_ILLEGAL_PARAMS;
			goto out;
		}
		j = cmd->safe_power_table[i];
		stat->tx_power_in_dbm[i] = cmd->safe_power_table[i];
	}

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_coex_static_params_cmd(mvm))
		err = LTE_OTHER_ERR;
	else
		mvm->lte_state.has_static = 1;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int in_range(int val, int min, int max)
{
	return (val >= min) && (val <= max);
}

static bool is_valid_lte_range(u16 min, u16 max)
{
	return (min == 0 && max == 0) ||
	       (max >= min &&
		in_range(min, LTE_FRQ_MIN, LTE_FRQ_MAX) &&
		in_range(max, LTE_FRQ_MIN, LTE_FRQ_MAX));
}

static int iwl_vendor_lte_coex_dynamic_info_cmd(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_dynamic_info_cmd *cmd = data;
	struct iwl_lte_coex_config_cmd *config = &mvm->lte_state.config;
	struct sk_buff *skb;
	int err = LTE_OK;
	int i;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	if (!mvm->lte_state.has_static ||
	    (mvm->lte_state.has_config &&
	     mvm->lte_state.state != LTE_CONNECTED)) {
		err = LTE_STATE_ERR;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: dynamic cmd:\n"
		       "\tlte band[0]: %d, chan[0]: %d\n\ttx range: %d - %d\n"
		       "\trx range: %d - %d\n", cmd->lte_connected_bands[0],
		       cmd->lte_connected_bands[1], cmd->wifi_tx_safe_freq_min,
		       cmd->wifi_tx_safe_freq_max, cmd->wifi_rx_safe_freq_min,
		       cmd->wifi_rx_safe_freq_max);

	/* TODO: validate lte connected bands and channel, and frame struct */
	config->lte_band = cpu_to_le32(cmd->lte_connected_bands[0]);
	config->lte_chan = cpu_to_le32(cmd->lte_connected_bands[1]);
	for (i = 0; i < LTE_FRAME_STRUCT_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\tframe structure[%d]: %d\n", i,
			       cmd->lte_frame_structure[i]);
		config->lte_frame_structure[i] =
				cpu_to_le32(cmd->lte_frame_structure[i]);
	}
	if (!is_valid_lte_range(cmd->wifi_tx_safe_freq_min,
				cmd->wifi_tx_safe_freq_max) ||
	    !is_valid_lte_range(cmd->wifi_rx_safe_freq_min,
				cmd->wifi_rx_safe_freq_max)) {
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}
	config->tx_safe_freq_min = cpu_to_le32(cmd->wifi_tx_safe_freq_min);
	config->tx_safe_freq_max = cpu_to_le32(cmd->wifi_tx_safe_freq_max);
	config->rx_safe_freq_min = cpu_to_le32(cmd->wifi_rx_safe_freq_min);
	config->rx_safe_freq_max = cpu_to_le32(cmd->wifi_rx_safe_freq_max);
	for (i = 0; i < LTE_TX_POWER_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\twifi max tx power[%d]: %d\n", i,
			       cmd->wifi_max_tx_power[i]);
		if (!in_range(cmd->wifi_max_tx_power[i], LTE_MAX_TX_MIN,
			      LTE_MAX_TX_MAX)) {
			err = LTE_ILLEGAL_PARAMS;
			goto out;
		}
		config->max_tx_power[i] = cmd->wifi_max_tx_power[i];
	}

	mvm->lte_state.has_config = 1;

	if (mvm->lte_state.state == LTE_CONNECTED) {
		mutex_lock(&mvm->mutex);
		if (iwl_mvm_send_lte_coex_config_cmd(mvm))
			err = LTE_OTHER_ERR;
		mutex_unlock(&mvm->mutex);
	}
out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_vendor_lte_sps_cmd(struct wiphy *wiphy,
				  struct wireless_dev *wdev, const void *data,
				  int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_sps_info_cmd *cmd = data;
	struct iwl_lte_coex_sps_cmd *sps = &mvm->lte_state.sps;
	struct sk_buff *skb;
	int err = LTE_OK;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: sps cmd:\n\tsps info: %d\n",
		       cmd->sps_info);

	if (mvm->lte_state.state != LTE_CONNECTED) {
		err = LTE_STATE_ERR;
		goto out;
	}

	/* TODO: validate SPS */
	sps->lte_semi_persistent_info = cpu_to_le32(cmd->sps_info);

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_sps_cmd(mvm))
		err = LTE_OTHER_ERR;
	else
		mvm->lte_state.has_sps = 1;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int
iwl_vendor_lte_coex_wifi_reported_channel_cmd(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_wifi_reported_chan_cmd *cmd = data;
	struct iwl_lte_coex_wifi_reported_channel_cmd *rprtd_chan =
						&mvm->lte_state.rprtd_chan;
	struct sk_buff *skb;
	int err = LTE_OK;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: wifi reported channel cmd:\n"
		       "\tchannel: %d, bandwidth: %d\n", cmd->chan,
		       cmd->bandwidth);

	if (!in_range(cmd->chan, LTE_RC_CHAN_MIN, LTE_RC_CHAN_MAX) ||
	    !in_range(cmd->bandwidth, LTE_RC_BW_MIN, LTE_RC_BW_MAX)) {
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}

	rprtd_chan->channel = cpu_to_le32(cmd->chan);
	rprtd_chan->bandwidth = cpu_to_le32(cmd->bandwidth);

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_coex_wifi_reported_channel_cmd(mvm))
		err = LTE_OTHER_ERR;
	else
		mvm->lte_state.has_rprtd_chan = 1;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}
#endif /* CPTCFG_IWLWIFI_LTE_COEX */

static int iwl_vendor_frame_filter_cmd(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);

	if (!vif)
		return -EINVAL;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	vif->filter_grat_arp_unsol_na =
		tb[IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA];
	vif->filter_gtk = tb[IWL_MVM_VENDOR_ATTR_FILTER_GTK];

	kfree(tb);

	return 0;
}

#ifdef CPTCFG_IWLMVM_TDLS_PEER_CACHE
static int iwl_vendor_tdls_peer_cache_add(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	u8 *addr;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	int err;

	if (!vif)
		return -ENODEV;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (vif->type != NL80211_IFTYPE_STATION ||
	    !tb[IWL_MVM_VENDOR_ATTR_ADDR]) {
		err = -EINVAL;
		goto free;
	}

	mutex_lock(&mvm->mutex);
	if (mvm->tdls_peer_cache_cnt >= IWL_MVM_TDLS_CNT_MAX_PEERS) {
		err = -ENOSPC;
		goto out_unlock;
	}

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	rcu_read_unlock();
	if (cnt) {
		err = -EEXIST;
		goto out_unlock;
	}

	cnt = kzalloc(sizeof(*cnt) +
		      sizeof(cnt->rx[0]) * mvm->trans->num_rx_queues,
		      GFP_KERNEL);
	if (!cnt) {
		err = -ENOMEM;
		goto out_unlock;
	}

	IWL_DEBUG_TDLS(mvm, "Adding %pM to TDLS peer cache\n", addr);
	ether_addr_copy(cnt->mac.addr, addr);
	cnt->vif = vif;
	list_add_tail_rcu(&cnt->list, &mvm->tdls_peer_cache_list);
	mvm->tdls_peer_cache_cnt++;

	err = 0;

out_unlock:
	mutex_unlock(&mvm->mutex);
free:
	kfree(tb);
	return err;
}

static int iwl_vendor_tdls_peer_cache_del(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	u8 *addr;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR]) {
		err = -EINVAL;
		goto free;
	}

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	mutex_lock(&mvm->mutex);
	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	if (!cnt) {
		IWL_DEBUG_TDLS(mvm, "%pM not found in TDLS peer cache\n", addr);
		err = -ENOENT;
		goto out_unlock;
	}

	IWL_DEBUG_TDLS(mvm, "Removing %pM from TDLS peer cache\n", addr);
	mvm->tdls_peer_cache_cnt--;
	list_del_rcu(&cnt->list);
	kfree_rcu(cnt, rcu_head);
	err = 0;

out_unlock:
	rcu_read_unlock();
	mutex_unlock(&mvm->mutex);
free:
	kfree(tb);
	return err;
}

static int iwl_vendor_tdls_peer_cache_query(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	struct sk_buff *skb;
	u32 rx_bytes, tx_bytes;
	u8 *addr;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR]) {
		kfree(tb);
		return -EINVAL;
	}

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	/* we can free the tb, the addr pointer is still valid into the msg */
	kfree(tb);

	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	if (!cnt) {
		IWL_DEBUG_TDLS(mvm, "%pM not found in TDLS peer cache\n",
			       addr);
		err = -ENOENT;
	} else {
		int q;

		tx_bytes = cnt->tx_bytes;
		rx_bytes = 0;
		for (q = 0; q < mvm->trans->num_rx_queues; q++)
			rx_bytes += cnt->rx[q].bytes;
		err = 0;
	}
	rcu_read_unlock();
	if (err)
		return err;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_TX_BYTES, tx_bytes) ||
	    nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_RX_BYTES, rx_bytes)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}
#endif /* CPTCFG_IWLMVM_TDLS_PEER_CACHE */

static int iwl_vendor_set_nic_txpower_limit(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	union {
		struct iwl_dev_tx_power_cmd_v4 v4;
		struct iwl_dev_tx_power_cmd v5;
	} cmd = {
		.v5.v3.set_mode = cpu_to_le32(IWL_TX_POWER_MODE_SET_DEVICE),
		.v5.v3.dev_24 = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
		.v5.v3.dev_52_low = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
		.v5.v3.dev_52_high = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
	};
	struct nlattr **tb;
	int len = sizeof(cmd);
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER) {
			err = -EINVAL;
			goto free;
		}
		cmd.v5.v3.dev_24 = cpu_to_le16(txp);
	}

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER) {
			err = -EINVAL;
			goto free;
		}
		cmd.v5.v3.dev_52_low = cpu_to_le16(txp);
	}

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER) {
			err = -EINVAL;
			goto free;
		}
		cmd.v5.v3.dev_52_high = cpu_to_le16(txp);
	}

	mvm->txp_cmd.v5 = cmd.v5;

	if (fw_has_api(&mvm->fw->ucode_capa,
		       IWL_UCODE_TLV_API_REDUCE_TX_POWER))
		len = sizeof(mvm->txp_cmd.v5);
	else if (fw_has_capa(&mvm->fw->ucode_capa,
			     IWL_UCODE_TLV_CAPA_TX_POWER_ACK))
		len = sizeof(mvm->txp_cmd.v4);
	else
		len = sizeof(mvm->txp_cmd.v4.v3);

	mutex_lock(&mvm->mutex);
	err = iwl_mvm_send_cmd_pdu(mvm, REDUCE_TX_POWER_CMD, 0, len, &cmd);
	mutex_unlock(&mvm->mutex);

	if (err)
		IWL_ERR(mvm, "failed to update device TX power: %d\n", err);
	err = 0;
free:
	kfree(tb);
	return err;
}

#ifdef CPTCFG_IWLMVM_P2P_OPPPS_TEST_WA
static int iwl_mvm_oppps_wa_update_quota(struct iwl_mvm *mvm,
					 struct ieee80211_vif *vif,
					 bool enable)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	struct ieee80211_p2p_noa_attr *noa = &vif->bss_conf.p2p_noa_attr;
	bool force_update = true;

	if (enable && noa->oppps_ctwindow & IEEE80211_P2P_OPPPS_ENABLE_BIT)
		mvm->p2p_opps_test_wa_vif = mvmvif;
	else
		mvm->p2p_opps_test_wa_vif = NULL;

	if (fw_has_capa(&mvm->fw->ucode_capa,
			IWL_UCODE_TLV_CAPA_DYNAMIC_QUOTA)) {
		return -EOPNOTSUPP;
	}

	return iwl_mvm_update_quotas(mvm, force_update, NULL);
}

static int iwl_mvm_oppps_wa(struct wiphy *wiphy,
			    struct wireless_dev *wdev,
			    const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	int err;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);

	if (!vif)
		return -ENODEV;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	mutex_lock(&mvm->mutex);
	if (vif->type == NL80211_IFTYPE_STATION && vif->p2p) {
		bool enable = !!tb[IWL_MVM_VENDOR_ATTR_OPPPS_WA];

		err = iwl_mvm_oppps_wa_update_quota(mvm, vif, enable);
	}
	mutex_unlock(&mvm->mutex);

	kfree(tb);
	return err;
}
#endif

void iwl_mvm_active_rx_filters(struct iwl_mvm *mvm)
{
	int i, len, total = 0;
	struct iwl_mcast_filter_cmd *cmd;
	static const u8 ipv4mc[] = {0x01, 0x00, 0x5e};
	static const u8 ipv6mc[] = {0x33, 0x33};
	static const u8 ipv4_mdns[] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb};
	static const u8 ipv6_mdns[] = {0x33, 0x33, 0x00, 0x00, 0x00, 0xfb};

	lockdep_assert_held(&mvm->mutex);

	if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_EINVAL)
		return;

	for (i = 0; i < mvm->mcast_filter_cmd->count; i++) {
		if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST4 &&
		    memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
			   ipv4mc, sizeof(ipv4mc)) == 0)
			total++;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv4_mdns, sizeof(ipv4_mdns)) == 0)
			total++;
		else if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST6 &&
			 memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6mc, sizeof(ipv6mc)) == 0)
			total++;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6_mdns, sizeof(ipv6_mdns)) == 0)
			total++;
	}

	/* FW expects full words */
	len = roundup(sizeof(*cmd) + total * ETH_ALEN, 4);
	cmd = kzalloc(len, GFP_KERNEL);
	if (!cmd)
		return;

	memcpy(cmd, mvm->mcast_filter_cmd, sizeof(*cmd));
	cmd->count = 0;

	for (i = 0; i < mvm->mcast_filter_cmd->count; i++) {
		bool copy_filter = false;

		if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST4 &&
		    memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
			   ipv4mc, sizeof(ipv4mc)) == 0)
			copy_filter = true;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv4_mdns, sizeof(ipv4_mdns)) == 0)
			copy_filter = true;
		else if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST6 &&
			 memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6mc, sizeof(ipv6mc)) == 0)
			copy_filter = true;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6_mdns, sizeof(ipv6_mdns)) == 0)
			copy_filter = true;

		if (!copy_filter)
			continue;

		ether_addr_copy(&cmd->addr_list[cmd->count * ETH_ALEN],
				&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN]);
		cmd->count++;
	}

	kfree(mvm->mcast_active_filter_cmd);
	mvm->mcast_active_filter_cmd = cmd;
}

static int iwl_mvm_vendor_rxfilter(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	enum iwl_mvm_vendor_rxfilter_flags filter, rx_filters, old_rx_filters;
	enum iwl_mvm_vendor_rxfilter_op op;
	bool first_set;
	u32 mask;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_RXFILTER]) {
		err = -EINVAL;
		goto free;
	}

	if (!tb[IWL_MVM_VENDOR_ATTR_RXFILTER_OP]) {
		err = -EINVAL;
		goto free;
	}

	filter = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_RXFILTER]);
	op = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_RXFILTER_OP]);

	if (filter != IWL_MVM_VENDOR_RXFILTER_UNICAST &&
	    filter != IWL_MVM_VENDOR_RXFILTER_BCAST &&
	    filter != IWL_MVM_VENDOR_RXFILTER_MCAST4 &&
	    filter != IWL_MVM_VENDOR_RXFILTER_MCAST6) {
		err = -EINVAL;
		goto free;
	}

	rx_filters = mvm->rx_filters & ~IWL_MVM_VENDOR_RXFILTER_EINVAL;
	switch (op) {
	case IWL_MVM_VENDOR_RXFILTER_OP_DROP:
		rx_filters &= ~filter;
		break;
	case IWL_MVM_VENDOR_RXFILTER_OP_PASS:
		rx_filters |= filter;
		break;
	default:
		err = -EINVAL;
		goto free;
	}

	first_set = mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_EINVAL;

	/* If first time set - clear EINVAL value */
	mvm->rx_filters &= ~IWL_MVM_VENDOR_RXFILTER_EINVAL;

	err = 0;

	if (rx_filters == mvm->rx_filters && !first_set)
		goto free;

	mutex_lock(&mvm->mutex);

	old_rx_filters = mvm->rx_filters;
	mvm->rx_filters = rx_filters;

	mask = IWL_MVM_VENDOR_RXFILTER_MCAST4 | IWL_MVM_VENDOR_RXFILTER_MCAST6;
	if ((old_rx_filters & mask) != (rx_filters & mask) || first_set) {
		iwl_mvm_active_rx_filters(mvm);
		iwl_mvm_recalc_multicast(mvm);
	}

	mask = IWL_MVM_VENDOR_RXFILTER_BCAST;
	if ((old_rx_filters & mask) != (rx_filters & mask) || first_set)
		iwl_mvm_configure_bcast_filter(mvm);

	mutex_unlock(&mvm->mutex);

free:
	kfree(tb);
	return err;
}

static int iwl_mvm_vendor_dbg_collect(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	int err, len = 0;
	const char *trigger_desc;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER]) {
		err = -EINVAL;
		goto free;
	}

	trigger_desc = nla_data(tb[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER]);
	len = nla_len(tb[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER]);

	iwl_fw_dbg_collect(&mvm->fwrt, FW_DBG_TRIGGER_USER_EXTENDED,
			   trigger_desc, len);
	err = 0;

free:
	kfree(tb);
	return err;
}

static int iwl_mvm_vendor_nan_faw_conf(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct cfg80211_chan_def def = {};
	struct ieee80211_channel *chan;
	u32 freq;
	u8 slots;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_SLOTS]) {
		err = -EINVAL;
		goto free;
	}

	if (!tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_FREQ]) {
		err = -EINVAL;
		goto free;
	}

	freq = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_FREQ]);
	slots = nla_get_u8(tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_SLOTS]);

	chan = ieee80211_get_channel(wiphy, freq);
	if (!chan) {
		err = -EINVAL;
		goto free;
	}

	cfg80211_chandef_create(&def, chan, NL80211_CHAN_NO_HT);

	if (!cfg80211_chandef_usable(wiphy, &def, IEEE80211_CHAN_DISABLED)) {
		err = -EINVAL;
		goto free;
	}

	err = iwl_mvm_nan_config_nan_faw_cmd(mvm, &def, slots);
free:
	kfree(tb);
	return err;
}

#ifdef CONFIG_ACPI
static int iwl_mvm_vendor_set_dynamic_txp_profile(struct wiphy *wiphy,
						  struct wireless_dev *wdev,
						  const void *data,
						  int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	u8 chain_a, chain_b;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE] ||
	    !tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE]) {
		err = -EINVAL;
		goto free;
	}

	chain_a = nla_get_u8(tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE]);
	chain_b = nla_get_u8(tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE]);

	if (mvm->sar_chain_a_profile == chain_a &&
	    mvm->sar_chain_b_profile == chain_b) {
		err = 0;
		goto free;
	}

	mvm->sar_chain_a_profile = chain_a;
	mvm->sar_chain_b_profile = chain_b;

	err = iwl_mvm_sar_select_profile(mvm, chain_a, chain_b);
free:
	kfree(tb);
	return err;
}

static int iwl_mvm_vendor_get_sar_profile_info(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb;
	int i;
	u32 n_profiles = 0;

	for (i = 0; i < ACPI_SAR_PROFILE_NUM; i++) {
		if (mvm->sar_profiles[i].enabled)
			n_profiles++;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_SAR_ENABLED_PROFILE_NUM,
		       n_profiles) ||
	    nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE,
		       mvm->sar_chain_a_profile) ||
	    nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE,
		       mvm->sar_chain_b_profile)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

#define IWL_MVM_SAR_GEO_NUM_BANDS	2

static int iwl_mvm_vendor_get_geo_profile_info(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb;
	struct nlattr *nl_profile;
	int i, tbl_idx;

	tbl_idx = iwl_mvm_get_sar_geo_profile(mvm);
	if (tbl_idx < 0)
		return tbl_idx;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	nl_profile = nla_nest_start(skb, IWL_MVM_VENDOR_ATTR_SAR_GEO_PROFILE);
	if (!nl_profile) {
		kfree_skb(skb);
		return -ENOBUFS;
	}
	if (!tbl_idx)
		goto out;

	for (i = 0; i < IWL_MVM_SAR_GEO_NUM_BANDS; i++) {
		u8 *value;
		struct nlattr *nl_chain = nla_nest_start(skb, i + 1);
		int idx = i * ACPI_GEO_PER_CHAIN_SIZE;

		if (!nl_chain) {
			kfree_skb(skb);
			return -ENOBUFS;
		}

		value =  &mvm->geo_profiles[tbl_idx - 1].values[idx];

		nla_put_u8(skb, IWL_VENDOR_SAR_GEO_MAX_TXP, value[0]);
		nla_put_u8(skb, IWL_VENDOR_SAR_GEO_CHAIN_A_OFFSET, value[1]);
		nla_put_u8(skb, IWL_VENDOR_SAR_GEO_CHAIN_B_OFFSET, value[2]);
		nla_nest_end(skb, nl_chain);
	}
out:
	nla_nest_end(skb, nl_profile);

	return cfg80211_vendor_cmd_reply(skb);
}
#endif

static const struct nla_policy
iwl_mvm_vendor_fips_hw_policy[NUM_IWL_VENDOR_FIPS_TEST_VECTOR_HW] = {
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_FLAGS] = { .type = NLA_U8 },
};

static int iwl_mvm_vendor_validate_ccm_vector(struct nlattr **tb)
{
	if (!tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] ||
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	    FIPS_KEY_LEN_128 ||
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]) !=
	    FIPS_CCM_NONCE_LEN)
		return -EINVAL;

	return 0;
}

static int iwl_mvm_vendor_validate_gcm_vector(struct nlattr **tb)
{
	if (!tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] ||
	    (nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_128 &&
	     nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_256) ||
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]) !=
	    FIPS_GCM_NONCE_LEN)
		return -EINVAL;

	return 0;
}

static int iwl_mvm_vendor_validate_aes_vector(struct nlattr **tb)
{
	if (!tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] ||
	    (nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_128 &&
	     nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_256))
		return -EINVAL;

	return 0;
}

/**
 * iwl_mvm_vendor_build_vector - build FIPS test vector for AES/CCM/GCM tests
 *
 * @cmd_buf: the command buffer is returned by this pointer in case of success.
 * @vector: test vector attributes.
 * @flags: specifies which encryption algorithm to use. One of
 *	&IWL_FIPS_TEST_VECTOR_FLAGS_CCM, &IWL_FIPS_TEST_VECTOR_FLAGS_GCM and
 *	&IWL_FIPS_TEST_VECTOR_FLAGS_AES.
 *
 * This function returns the length of the command buffer (in bytes) in case of
 * success, or a negative error code on failure.
 */
static int iwl_mvm_vendor_build_vector(u8 **cmd_buf, struct nlattr *vector,
				       u8 flags)
{
	struct nlattr *tb[NUM_IWL_VENDOR_FIPS_TEST_VECTOR_HW];
	struct iwl_fips_test_cmd *cmd;
	int err;
	int payload_len = 0;
	u8 *buf;

	err = nla_parse_nested(tb, MAX_IWL_VENDOR_FIPS_TEST_VECTOR_HW,
			       vector, iwl_mvm_vendor_fips_hw_policy, NULL);
	if (err)
		return err;

	switch (flags) {
	case IWL_FIPS_TEST_VECTOR_FLAGS_CCM:
		err = iwl_mvm_vendor_validate_ccm_vector(tb);
		break;
	case IWL_FIPS_TEST_VECTOR_FLAGS_GCM:
		err = iwl_mvm_vendor_validate_gcm_vector(tb);
		break;
	case IWL_FIPS_TEST_VECTOR_FLAGS_AES:
		err = iwl_mvm_vendor_validate_aes_vector(tb);
		break;
	default:
		return -EINVAL;
	}

	if (err)
		return err;

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] &&
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]) > FIPS_MAX_AAD_LEN)
		return -EINVAL;

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD])
		payload_len =
			nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD]);

	buf = kzalloc(sizeof(*cmd) + payload_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	cmd = (void *)buf;

	cmd->flags = cpu_to_le32(flags);

	memcpy(cmd->key, nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]),
	       nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]));

	if (nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) == FIPS_KEY_LEN_256)
		cmd->flags |= cpu_to_le32(IWL_FIPS_TEST_VECTOR_FLAGS_KEY_256);

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE])
		memcpy(cmd->nonce,
		       nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]),
		       nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]));

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]) {
		memcpy(cmd->aad,
		       nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]),
		       nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]));
		cmd->aad_len =
			cpu_to_le32(nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]));
	}

	if (payload_len) {
		memcpy(cmd->payload,
		       nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD]),
		       payload_len);
		cmd->payload_len = cpu_to_le32(payload_len);
	}

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_FLAGS]) {
		u8 hw_flags =
			nla_get_u8(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_FLAGS]);

		if (hw_flags & IWL_VENDOR_FIPS_TEST_VECTOR_FLAGS_ENCRYPT)
			cmd->flags |=
				cpu_to_le32(IWL_FIPS_TEST_VECTOR_FLAGS_ENC);
	}

	*cmd_buf = buf;
	return sizeof(*cmd) + payload_len;
}

static int iwl_mvm_vendor_test_fips_send_resp(struct wiphy *wiphy,
					      struct iwl_fips_test_resp *resp)
{
	struct sk_buff *skb;
	u32 resp_len = le32_to_cpu(resp->len);
	u32 *status = (void *)(resp->payload + resp_len);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(*resp));
	if (!skb)
		return -ENOMEM;

	if ((*status) == IWL_FIPS_TEST_STATUS_SUCCESS &&
	    nla_put(skb, IWL_MVM_VENDOR_ATTR_FIPS_TEST_RESULT, resp_len,
		    resp->payload)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_vendor_test_fips(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_host_cmd hcmd = {
		.id = iwl_cmd_id(FIPS_TEST_VECTOR_CMD, LEGACY_GROUP, 0),
		.flags = CMD_WANT_SKB,
		.dataflags = { IWL_HCMD_DFL_NOCOPY },
	};
	struct iwl_rx_packet *pkt;
	struct iwl_fips_test_resp *resp;
	struct nlattr *vector;
	u8 flags;
	u8 *buf = NULL;
	int ret;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_CCM]) {
		vector = tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_CCM];
		flags = IWL_FIPS_TEST_VECTOR_FLAGS_CCM;
	} else if (tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_GCM]) {
		vector = tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_GCM];
		flags = IWL_FIPS_TEST_VECTOR_FLAGS_GCM;
	} else if (tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_AES]) {
		vector = tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_AES];
		flags = IWL_FIPS_TEST_VECTOR_FLAGS_AES;
	} else {
		ret = -EINVAL;
		goto free;
	}

	ret = iwl_mvm_vendor_build_vector(&buf, vector, flags);
	if (ret <= 0)
		goto free;

	hcmd.data[0] = buf;
	hcmd.len[0] = ret;

	mutex_lock(&mvm->mutex);
	ret = iwl_mvm_send_cmd(mvm, &hcmd);
	mutex_unlock(&mvm->mutex);

	if (ret)
		goto free;

	pkt = hcmd.resp_pkt;
	resp = (void *)pkt->data;

	iwl_mvm_vendor_test_fips_send_resp(wiphy, resp);
	iwl_free_resp(&hcmd);

free:
	kfree(buf);
	kfree(tb);
	return ret;
}

static int iwl_mvm_vendor_csi_register(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);

	mvm->csi_portid = cfg80211_vendor_cmd_get_sender(wiphy);

	return 0;
}

static const struct wiphy_vendor_command iwl_mvm_vendor_commands[] = {
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_LOW_LATENCY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_set_low_latency,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_LOW_LATENCY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_get_low_latency,
	},
#ifdef CPTCFG_IWLWIFI_LTE_COEX
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_STATE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_state_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_CONFIG_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_config_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_DYNAMIC_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_dynamic_info_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_SPS_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_sps_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_WIFI_RPRTD_CHAN,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_wifi_reported_channel_cmd,
	},
#endif /* CPTCFG_IWLWIFI_LTE_COEX */
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_COUNTRY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_set_country,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_PROXY_FRAME_FILTERING,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_frame_filter_cmd,
	},
#ifdef CPTCFG_IWLMVM_TDLS_PEER_CACHE
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_ADD,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_add,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_DEL,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_del,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_QUERY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_query,
	},
#endif /* CPTCFG_IWLMVM_TDLS_PEER_CACHE */
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_NIC_TXPOWER_LIMIT,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_set_nic_txpower_limit,
	},
#ifdef CPTCFG_IWLMVM_P2P_OPPPS_TEST_WA
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_OPPPS_WA,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_oppps_wa,
	},
#endif
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_RXFILTER,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_rxfilter,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_DBG_COLLECT,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_dbg_collect,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,

			.subcmd = IWL_MVM_VENDOR_CMD_NAN_FAW_CONF,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_nan_faw_conf,
	},
#ifdef CONFIG_ACPI
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_SAR_PROFILE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_set_dynamic_txp_profile,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_SAR_PROFILE_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_get_sar_profile_info,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_SAR_GEO_PROFILE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_get_geo_profile_info,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TEST_FIPS,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_test_fips,
	},
#endif
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_CSI_EVENT,
		},
		.doit = iwl_mvm_vendor_csi_register,
	},
};

enum iwl_mvm_vendor_events_idx {
	IWL_MVM_VENDOR_EVENT_IDX_TCM,
	IWL_MVM_VENDOR_EVENT_IDX_CSI,
	NUM_IWL_MVM_VENDOR_EVENT_IDX
};

static const struct nl80211_vendor_cmd_info
iwl_mvm_vendor_events[NUM_IWL_MVM_VENDOR_EVENT_IDX] = {
	[IWL_MVM_VENDOR_EVENT_IDX_TCM] = {
		.vendor_id = INTEL_OUI,
		.subcmd = IWL_MVM_VENDOR_CMD_TCM_EVENT,
	},
	[IWL_MVM_VENDOR_EVENT_IDX_CSI] = {
		.vendor_id = INTEL_OUI,
		.subcmd = IWL_MVM_VENDOR_CMD_CSI_EVENT,
	},
};

void iwl_mvm_vendor_cmds_register(struct iwl_mvm *mvm)
{
	mvm->hw->wiphy->vendor_commands = iwl_mvm_vendor_commands;
	mvm->hw->wiphy->n_vendor_commands = ARRAY_SIZE(iwl_mvm_vendor_commands);
	mvm->hw->wiphy->vendor_events = iwl_mvm_vendor_events;
	mvm->hw->wiphy->n_vendor_events = ARRAY_SIZE(iwl_mvm_vendor_events);

	spin_lock_bh(&device_list_lock);
	list_add_tail(&mvm->list, &device_list);
	spin_unlock_bh(&device_list_lock);
}

void iwl_mvm_vendor_cmds_unregister(struct iwl_mvm *mvm)
{
	spin_lock_bh(&device_list_lock);
	list_del(&mvm->list);
	spin_unlock_bh(&device_list_lock);
}

static enum iwl_mvm_vendor_load
iwl_mvm_get_vendor_load(enum iwl_mvm_traffic_load load)
{
	switch (load) {
	case IWL_MVM_TRAFFIC_HIGH:
		return IWL_MVM_VENDOR_LOAD_HIGH;
	case IWL_MVM_TRAFFIC_MEDIUM:
		return IWL_MVM_VENDOR_LOAD_MEDIUM;
	case IWL_MVM_TRAFFIC_LOW:
		return IWL_MVM_VENDOR_LOAD_LOW;
	default:
		break;
	}

	return IWL_MVM_VENDOR_LOAD_LOW;
}

void iwl_mvm_send_tcm_event(struct iwl_mvm *mvm, struct ieee80211_vif *vif)
{
	struct sk_buff *msg =
		cfg80211_vendor_event_alloc(mvm->hw->wiphy,
					    ieee80211_vif_to_wdev(vif),
					    200, IWL_MVM_VENDOR_EVENT_IDX_TCM,
					    GFP_ATOMIC);

	if (!msg)
		return;

	if (vif) {
		struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);

		if (nla_put(msg, IWL_MVM_VENDOR_ATTR_VIF_ADDR,
			    ETH_ALEN, vif->addr) ||
		    nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_VIF_LL,
			       iwl_mvm_vif_low_latency(mvmvif)) ||
		    nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_VIF_LOAD,
			       mvm->tcm.result.load[mvmvif->id]))
			goto nla_put_failure;
	}

	if (nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_LL, iwl_mvm_low_latency(mvm)) ||
	    nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_LOAD,
		       iwl_mvm_get_vendor_load(mvm->tcm.result.global_load)))
		goto nla_put_failure;

	cfg80211_vendor_event(msg, GFP_ATOMIC);
	return;

 nla_put_failure:
	kfree_skb(msg);
}

static void
iwl_mvm_send_csi_event(struct iwl_mvm *mvm,
		       void *hdr, unsigned int hdr_len,
		       void **data, unsigned int *len)
{
	unsigned int data_len = 0;
	struct sk_buff *msg;
	struct nlattr *dattr;
	u8 *pos;
	int i;

	if (!mvm->csi_portid)
		return;

	for (i = 0; len[i] && data[i]; i++)
		data_len += len[i];

	msg = cfg80211_vendor_event_alloc_ucast(mvm->hw->wiphy, NULL,
						mvm->csi_portid,
						100 + hdr_len + data_len,
						IWL_MVM_VENDOR_EVENT_IDX_CSI,
						GFP_KERNEL);

	if (!msg)
		return;

	if (nla_put(msg, IWL_MVM_VENDOR_ATTR_CSI_HDR, hdr_len, hdr))
		goto nla_put_failure;

	dattr = nla_reserve(msg, IWL_MVM_VENDOR_ATTR_CSI_DATA, data_len);
	if (!dattr)
		goto nla_put_failure;

	pos = nla_data(dattr);
	for (i = 0; len[i] && data[i]; i++) {
		memcpy(pos, data[i], len[i]);
		pos += len[i];
	}

	cfg80211_vendor_event(msg, GFP_KERNEL);
	return;

 nla_put_failure:
	kfree_skb(msg);
}

static void iwl_mvm_csi_free_pages(struct iwl_mvm *mvm)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(mvm->csi_data_entries); idx++) {
		if (mvm->csi_data_entries[idx].page) {
			__free_pages(mvm->csi_data_entries[idx].page,
				     mvm->csi_data_entries[idx].page_order);
			memset(&mvm->csi_data_entries[idx], 0,
			       sizeof(mvm->csi_data_entries[idx]));
		}
	}
}

static void iwl_mvm_csi_steal(struct iwl_mvm *mvm, unsigned int idx,
			      struct iwl_rx_cmd_buffer *rxb)
{
	/* firmware is ... confused */
	if (WARN_ON(mvm->csi_data_entries[idx].page)) {
		iwl_mvm_csi_free_pages(mvm);
		return;
	}

	mvm->csi_data_entries[idx].page = rxb_steal_page(rxb);
	mvm->csi_data_entries[idx].page_order = rxb->_rx_page_order;
	mvm->csi_data_entries[idx].offset = rxb->_offset;
}

void iwl_mvm_rx_csi_header(struct iwl_mvm *mvm, struct iwl_rx_cmd_buffer *rxb)
{
	iwl_mvm_csi_steal(mvm, 0, rxb);
}

static void iwl_mvm_csi_complete(struct iwl_mvm *mvm)
{
	struct iwl_rx_packet *hdr_pkt;
	struct iwl_csi_data_buffer *hdr_buf = &mvm->csi_data_entries[0];
	void *data[5] = {};
	unsigned int len[5] = {};
	unsigned int csi_hdr_len;
	void *csi_hdr;
	int i;

	/*
	 * Ensure we have the right # of entries, the local data/len
	 * variables include a terminating entry, csi_data_entries
	 * instead has one place for the header.
	 */
	BUILD_BUG_ON(ARRAY_SIZE(data) < ARRAY_SIZE(mvm->csi_data_entries));
	BUILD_BUG_ON(ARRAY_SIZE(len) < ARRAY_SIZE(mvm->csi_data_entries));

	/* need at least the header and first fragment */
	if (WARN_ON(!hdr_buf->page || !mvm->csi_data_entries[1].page)) {
		iwl_mvm_csi_free_pages(mvm);
		return;
	}

	hdr_pkt = (void *)((unsigned long)page_address(hdr_buf->page) +
			   hdr_buf->offset);
	csi_hdr = (void *)hdr_pkt->data;
	csi_hdr_len = iwl_rx_packet_payload_len(hdr_pkt);

	for (i = 1; i < ARRAY_SIZE(mvm->csi_data_entries); i++) {
		struct iwl_csi_data_buffer *buf = &mvm->csi_data_entries[i];
		struct iwl_rx_packet *pkt;
		struct iwl_csi_chunk_notification *chunk;
		unsigned int chunk_len;

		if (!buf->page)
			break;

		pkt = (void *)((unsigned long)page_address(buf->page) +
			       buf->offset);
		chunk = (void *)pkt->data;
		chunk_len = iwl_rx_packet_payload_len(pkt);

		if (sizeof(*chunk) + le32_to_cpu(chunk->size) > chunk_len)
			goto free;

		data[i - 1] = chunk->data;
		len[i - 1] = le32_to_cpu(chunk->size);
	}

	iwl_mvm_send_csi_event(mvm, csi_hdr, csi_hdr_len, data, len);

 free:
	iwl_mvm_csi_free_pages(mvm);
}

void iwl_mvm_rx_csi_chunk(struct iwl_mvm *mvm, struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_csi_chunk_notification *chunk = (void *)pkt->data;
	int num = le16_get_bits(chunk->ctl, IWL_CSI_CHUNK_CTL_NUM_MASK);
	int idx = le16_get_bits(chunk->ctl, IWL_CSI_CHUNK_CTL_IDX_MASK) + 1;

	/*
	 * +1 to get from mask to number
	 * -1 to account for the header we also store there
	 */
	BUILD_BUG_ON(IWL_CSI_CHUNK_CTL_NUM_MASK + 1 >
		     ARRAY_SIZE(mvm->csi_data_entries) - 1);
	BUILD_BUG_ON((IWL_CSI_CHUNK_CTL_IDX_MASK >> 2) + 1 >
		     ARRAY_SIZE(mvm->csi_data_entries) - 1);

	iwl_mvm_csi_steal(mvm, idx, rxb);

	if (num == idx)
		iwl_mvm_csi_complete(mvm);
}

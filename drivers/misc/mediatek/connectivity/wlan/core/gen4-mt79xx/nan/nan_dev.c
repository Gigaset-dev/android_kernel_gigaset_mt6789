// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#if CFG_SUPPORT_NAN
#include "precomp.h"
#include "nan/nan_sec.h"

uint8_t
nanDevInit(IN struct ADAPTER *prAdapter, uint8_t ucIdx) {
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
	struct BSS_INFO *prnanBssInfo = (struct BSS_INFO *)NULL;
	struct _GL_NAN_INFO_T *prNANInfo = (struct _GL_NAN_INFO_T *)NULL;
	enum ENUM_PARAM_NAN_MODE_T eNanMode;
	const struct NON_HT_PHY_ATTRIBUTE *prLegacyPhyAttr;
	const struct NON_HT_ADHOC_MODE_ATTRIBUTE *prLegacyModeAttr;
	uint8_t ucLegacyPhyTp;
	struct WIFI_VAR *prWifiVar;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR,
			"[%s] prAdapter is NULL\n", __func__);
		return MAX_BSS_INDEX;
	}

	prnanBssInfo = cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_NAN,
					    FALSE);
	if (prnanBssInfo == NULL) {
		DBGLOG(NAN, INFO, "No enough BSS INDEX\n");
		return MAX_BSS_INDEX;
	}

	prNANSpecInfo =
		prAdapter->rWifiVar.aprNanSpecificBssInfo[ucIdx];

	prNANInfo = prAdapter->prGlueInfo->aprNANDevInfo[NAN_BSS_INDEX_BAND0];
	if (prnanBssInfo != NULL) {
		DBGLOG(NAN, INFO, "NAN DEV BSSIFO INDEX %d %p\n",
		       prnanBssInfo->ucBssIndex, prnanBssInfo);
		COPY_MAC_ADDR(prnanBssInfo->aucOwnMacAddr,
			      prNANInfo->prDevHandler->dev_addr);
		prNANSpecInfo->ucBssIndex = prnanBssInfo->ucBssIndex;
		prNANSpecInfo->u4ModuleUsed = 0;
		prnanBssInfo->eCurrentOPMode = OP_MODE_NAN;

		eNanMode = NAN_MODE_MIXED_11BG;
		prLegacyModeAttr = &rNonHTNanModeAttr[eNanMode];
		ucLegacyPhyTp =
			(uint8_t)prLegacyModeAttr->ePhyTypeIndex;
		prLegacyPhyAttr = &rNonHTPhyAttributes[ucLegacyPhyTp];
		prWifiVar = &prAdapter->rWifiVar;

#if (CFG_SUPPORT_DBDC == 1)
		if (ucIdx == NAN_BSS_INDEX_BAND1)
			prnanBssInfo->eBand = BAND_5G;
		else
#endif
			prnanBssInfo->eBand = BAND_2G4;

		prnanBssInfo->u4PrivateData = 0;
		prnanBssInfo->ucSSIDLen = 0;
		prnanBssInfo->fgIsQBSS = 1;
		prnanBssInfo->eConnectionState = MEDIA_STATE_DISCONNECTED;
		prnanBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;
		prnanBssInfo->ucOpRxNss = wlanGetSupportNss(
			prAdapter, prnanBssInfo->ucBssIndex);

		/* let secIsProtectedFrame to decide protection or
		 * not by STA record
		 */
		prnanBssInfo->fgIsProtection = FALSE;

#if (CFG_HW_WMM_BY_BSS == 1)
		if (prnanBssInfo->fgIsWmmInited == FALSE)
			prnanBssInfo->ucWmmQueSet = cnmWmmIndexDecision(
				prAdapter, prnanBssInfo);
#else
		prnanBssInfo->ucWmmQueSet =
			(prAdapter->rWifiVar.ucDbdcMode ==
			DBDC_MODE_DISABLED) ? DBDC_5G_WMM_INDEX
			: DBDC_2G_WMM_INDEX;
#endif


#if (CFG_SUPPORT_DBDC == 1)
		if (ucIdx == NAN_BSS_INDEX_BAND1)
			prnanBssInfo->ucPhyTypeSet =
				prWifiVar->ucAvailablePhyTypeSet &
				PHY_TYPE_SET_802_11ANAC;
		else
#endif
			prnanBssInfo->ucPhyTypeSet =
				prWifiVar->ucAvailablePhyTypeSet &
				PHY_TYPE_SET_802_11BGN;

		prnanBssInfo->ucNonHTBasicPhyType = ucLegacyPhyTp;
		if (prLegacyPhyAttr
			    ->fgIsShortPreambleOptionImplemented &&
		    (prWifiVar->ePreambleType == PREAMBLE_TYPE_SHORT ||
		     prWifiVar->ePreambleType == PREAMBLE_TYPE_AUTO))
			prnanBssInfo->fgUseShortPreamble = TRUE;
		else
			prnanBssInfo->fgUseShortPreamble = FALSE;
		prnanBssInfo->fgUseShortSlotTime =
			prLegacyPhyAttr
				->fgIsShortSlotTimeOptionImplemented;

		prnanBssInfo->u2OperationalRateSet =
			prLegacyPhyAttr->u2SupportedRateSet;
		prnanBssInfo->u2BSSBasicRateSet =
			prLegacyModeAttr->u2BSSBasicRateSet;
		/* Mask CCK 1M For Sco scenario except FDD mode */
		if (prAdapter->u4FddMode == FALSE)
			prnanBssInfo->u2BSSBasicRateSet &=
				~RATE_SET_BIT_1M;
		prnanBssInfo->u2VhtBasicMcsSet = 0;

		prnanBssInfo->fgErpProtectMode = FALSE;
		prnanBssInfo->eHtProtectMode = HT_PROTECT_MODE_NONE;
		prnanBssInfo->eGfOperationMode = GF_MODE_DISALLOWED;
		prnanBssInfo->eRifsOperationMode = RIFS_MODE_DISALLOWED;

#if (CFG_SUPPORT_DBDC == 1)
		if (ucIdx == NAN_BSS_INDEX_BAND1)
			prnanBssInfo->ucPrimaryChannel = 149;
		else
#endif
			prnanBssInfo->ucPrimaryChannel = 6;

		prnanBssInfo->eBssSCO = CHNL_EXT_SCN;
		prnanBssInfo->ucHtOpInfo1 = 0;
		prnanBssInfo->u2HtOpInfo2 = 0;
		prnanBssInfo->u2HtOpInfo3 = 0;

#if (CFG_SUPPORT_DBDC == 1)
		if (ucIdx == NAN_BSS_INDEX_BAND1)
			prnanBssInfo->ucVhtChannelWidth = CW_80MHZ;
		else
#endif
			prnanBssInfo->ucVhtChannelWidth = CW_20_40MHZ;

		prnanBssInfo->ucVhtChannelFrequencyS1 = 0;
		prnanBssInfo->ucVhtChannelFrequencyS2 = 0;
		if (prWifiVar->ucNanBandwidth >= MAX_BW_40MHZ) {
			prnanBssInfo->eBssSCO = CHNL_EXT_SCA;
			prnanBssInfo->ucHtOpInfo1 |=
				HT_OP_INFO1_STA_CHNL_WIDTH;
			prnanBssInfo->fgAssoc40mBwAllowed = TRUE;
		}

		prnanBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
		rateGetDataRatesFromRateSet(
			prnanBssInfo->u2OperationalRateSet,
			prnanBssInfo->u2BSSBasicRateSet,
			prnanBssInfo->aucAllSupportedRates,
			&prnanBssInfo->ucAllSupportedRatesLen);

		/* Set DBRTS to 0x3FF as default */
		prnanBssInfo->ucHeOpParams[0] |=
			HE_OP_PARAM0_TXOP_DUR_RTS_THRESHOLD_MASK;
		prnanBssInfo->ucHeOpParams[1] |=
			HE_OP_PARAM1_TXOP_DUR_RTS_THRESHOLD_MASK;

		/* Activate NAN BSS */
		if (!IS_BSS_ACTIVE(
			    prAdapter->aprBssInfo
				    [prnanBssInfo->ucBssIndex])) {

			nicUpdateBss(prAdapter, prnanBssInfo->ucBssIndex);

#if (CFG_SUPPORT_DBDC == 1)
			/* Check if DBDC is required to be enabled first */
			cnmDbdcPreConnectionEnableDecision(
				prAdapter,
				prnanBssInfo->ucBssIndex,
				prnanBssInfo->eBand,
				prnanBssInfo->ucPrimaryChannel,
				prnanBssInfo->ucWmmQueSet);
#endif

			/* DBDC decsion may change OpNss */
			cnmOpModeGetTRxNss(prAdapter,
				prnanBssInfo->ucBssIndex,
				&prnanBssInfo->ucOpRxNss,
				&prnanBssInfo->ucOpTxNss);

			/* Set BSS to active for DBDC on*/
			SET_NET_ACTIVE(prAdapter,
				prnanBssInfo->ucBssIndex);
			prnanBssInfo->eConnectionState
				= MEDIA_STATE_CONNECTED;

			nanDevDumpBssStatus(prAdapter);
		}
	}

	if (prnanBssInfo)
		return prnanBssInfo->ucBssIndex;
	else
		return MAX_BSS_INDEX;
} /* p2pDevFsmInit */

void
nanDevFsmUninit(IN struct ADAPTER *prAdapter) {

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR,
			"prAdapter is NULL\n");
		return;
	}

	nanDevBssDeactivate(prAdapter, TRUE);
}

void
nanDevDumpBssStatus(struct ADAPTER *prAdapter)
{
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
	struct BSS_INFO *prnanBssInfo = (struct BSS_INFO *)NULL;
	uint8_t ucIdx = 0;

	for (ucIdx = 0; ucIdx < NAN_BSS_INDEX_NUM; ucIdx++) {
		prNANSpecInfo =
			prAdapter->rWifiVar.aprNanSpecificBssInfo[ucIdx];

		if (prNANSpecInfo == NULL) {
			DBGLOG(NAN, ERROR,
				"prNANSpecInfo is NULL\n");
			return;
		}

		prnanBssInfo = prAdapter->aprBssInfo[prNANSpecInfo->ucBssIndex];

		DBGLOG(NAN, INFO,
			"[NAN INIT] BSSIFO INDEX %u Act/Con:[%u/%u]\n",
			prnanBssInfo->ucBssIndex,
			prnanBssInfo->fgIsNetActive,
			prnanBssInfo->eConnectionState);
	}
}

void
nanDevBssActivate(struct ADAPTER *prAdapter)
{
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
	struct BSS_INFO *prnanBssInfo = (struct BSS_INFO *)NULL;
	uint8_t ucIdx = 0;
	struct AC_QUE_PARMS *prACQueParms = NULL;
	enum ENUM_WMM_ACI eAci = WMM_AC_BE_INDEX;
	uint8_t auCWmin[WMM_AC_INDEX_NUM] = { 4, 4, 3, 2 };
	uint8_t auCWmax[WMM_AC_INDEX_NUM] = { 10, 10, 4, 3 };
	uint8_t auAifs[WMM_AC_INDEX_NUM] = { 3, 7, 2, 2 };
	uint8_t auTxop[WMM_AC_INDEX_NUM] = { 0, 0, 94, 47 };

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR,
			"prAdapter is NULL\n");
		return;
	}

	for (ucIdx = 0; ucIdx < NAN_BSS_INDEX_NUM; ucIdx++) {
		prNANSpecInfo =
			prAdapter->rWifiVar.aprNanSpecificBssInfo[ucIdx];

		if (prNANSpecInfo == NULL) {
			DBGLOG(NAN, ERROR,
				"prNANSpecInfo is NULL\n");
			continue;
		}

		prnanBssInfo = prAdapter->aprBssInfo[prNANSpecInfo->ucBssIndex];

		if (!IS_BSS_ACTIVE(prnanBssInfo)) {
			SET_NET_ACTIVE(prAdapter, prnanBssInfo->ucBssIndex);
			nicActivateNetwork(prAdapter, prnanBssInfo->ucBssIndex);
		}

		DBGLOG(NAN, INFO,
				"[NAN INIT] ACTIVATE NAN DEV BSSIFO INDEX %u\n",
				prnanBssInfo->ucBssIndex);

		prnanBssInfo->eConnectionState = MEDIA_STATE_CONNECTED;

		nicUpdateBss(prAdapter, prnanBssInfo->ucBssIndex);

#if CFG_SUPPORT_DBDC
		cnmDbdcRuntimeCheckDecision(prAdapter,
						prnanBssInfo->ucBssIndex,
						FALSE);
#endif

		/** Update AC WMM Parm with correct BN info in BSSInfo */
		prACQueParms = prnanBssInfo->arACQueParms;

		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {

			prACQueParms[eAci].ucIsACMSet = FALSE;
			prACQueParms[eAci].u2Aifsn = auAifs[eAci];
			prACQueParms[eAci].u2CWmin = BIT(auCWmin[eAci]) - 1;
			prACQueParms[eAci].u2CWmax = BIT(auCWmax[eAci]) - 1;
			prACQueParms[eAci].u2TxopLimit = auTxop[eAci];
		}
		nicQmUpdateWmmParms(prAdapter,
			prnanBssInfo->ucBssIndex);
	}

	nanDevDumpBssStatus(prAdapter);
}

void
nanDevBssDeactivate(IN struct ADAPTER *prAdapter, bool fgFreeBss)
{
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
	struct BSS_INFO *prnanBssInfo = (struct BSS_INFO *)NULL;
	uint8_t ucIdx = 0;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR,
			"prAdapter is NULL\n");
		return;
	}

	for (ucIdx = 0; ucIdx < NAN_BSS_INDEX_NUM; ucIdx++) {
		prNANSpecInfo =
			prAdapter->rWifiVar.aprNanSpecificBssInfo[ucIdx];

		if (prNANSpecInfo == NULL) {
			DBGLOG(NAN, ERROR,
				"prNANSpecInfo is NULL\n");
			continue;
		}

		prnanBssInfo = prAdapter->aprBssInfo[prNANSpecInfo->ucBssIndex];

		if (!IS_BSS_ACTIVE(prnanBssInfo)) {
			DBGLOG(NAN, WARN,
				"[NAN INIT] NAN DEV BSS_IDX %u already deactivate\n",
				prnanBssInfo->ucBssIndex);
			goto free_bss;
		}

		DBGLOG(NAN, INFO,
			"[NAN INIT] DEACTIVATE NAN DEV BSS_IDX %u\n",
			prnanBssInfo->ucBssIndex);

		/* Clear CmdQue */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo,
					   prnanBssInfo->ucBssIndex);
		kalClearSecurityFramesByBssIdx(prAdapter->prGlueInfo,
					       prnanBssInfo->ucBssIndex);
		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter,
					      prnanBssInfo->ucBssIndex);
		/* Clear PendingTxMsdu */
		nicFreePendingTxMsduInfo(prAdapter, prnanBssInfo->ucBssIndex,
					 MSDU_REMOVE_BY_BSS_INDEX);

		nicPmIndicateBssAbort(prAdapter, prnanBssInfo->ucBssIndex);

		/* Deactivate BSS. */
		prnanBssInfo->eConnectionState = MEDIA_STATE_DISCONNECTED;
		UNSET_NET_ACTIVE(prAdapter, prnanBssInfo->ucBssIndex);
		nicDeactivateNetwork(prAdapter, prnanBssInfo->ucBssIndex);
		nicUpdateBss(prAdapter, prnanBssInfo->ucBssIndex);

free_bss:
		if (fgFreeBss) {
			DBGLOG(NAN, INFO,
				"[NAN INIT] FREE NAN DEV BSSIFO INDEX %u\n",
				prnanBssInfo->ucBssIndex);
			cnmFreeBssInfo(prAdapter, prnanBssInfo);
		}
	}

	nanDevDumpBssStatus(prAdapter);
}

struct _NAN_SPECIFIC_BSS_INFO_T *
nanGetSpecificBssInfo(IN struct ADAPTER *prAdapter,
		      enum NAN_BSS_ROLE_INDEX eIndex) {
	return prAdapter->rWifiVar.aprNanSpecificBssInfo[eIndex];
}

uint8_t
nanGetBssIdxbyBand(IN struct ADAPTER *prAdapter,
		      enum ENUM_BAND eBand) {
	uint8_t ucIdx = 0;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo;
	struct BSS_INFO *prBssInfo;

	for (ucIdx = 0; ucIdx < NAN_BSS_INDEX_NUM; ucIdx++)
	{
		prNANSpecInfo = nanGetSpecificBssInfo(prAdapter, ucIdx);
		prBssInfo = GET_BSS_INFO_BY_INDEX(
			prAdapter,
			prNANSpecInfo->ucBssIndex);

		if(prBssInfo->eBand == eBand)
			break;
	}

	return prNANSpecInfo->ucBssIndex;
}

void
nanDevCommonSetCb(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo,
		  IN uint8_t *pucEventBuf) {

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return;
	}

	if (prCmdInfo == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prCmdInfo is NULL\n", __func__);
		return;
	}
}

void
nanDevSetMasterPreference(IN struct ADAPTER *prAdapter,
			  uint8_t ucMasterPreference) {
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_CMD_MASTER_PREFERENCE_T *prCmdNanMasterPreference = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_CMD_MASTER_PREFERENCE_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus =
		nicAddNewTlvElement(NAN_CMD_MASTER_PREFERENCE,
				    sizeof(struct _NAN_CMD_MASTER_PREFERENCE_T),
				    u4CmdBufferLen, prCmdBuffer);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvElement = nicGetTargetTlvElement(1, prCmdBuffer);

	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prCmdNanMasterPreference =
		(struct _NAN_CMD_MASTER_PREFERENCE_T *)prTlvElement->aucbody;
	prCmdNanMasterPreference->ucMasterPreference = ucMasterPreference;

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, nanDevCommonSetCb,
				      nicCmdTimeoutCommon, u4CmdBufferLen,
				      (uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);
}

enum NanStatusType
nanDevSetDiscBcn(IN struct ADAPTER *prAdapter,
		 struct _NAN_CMD_EVENT_SET_DISC_BCN_T *prNanSetDiscBcn)
{
	uint32_t rStatus = 0;
	void *prCmdBuffer = NULL;
	uint32_t u4CmdBufferLen = 0;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_CMD_EVENT_SET_DISC_BCN_T *prCmdNanSetDiscBcn = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_CMD_EVENT_SET_DISC_BCN_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_INTERNAL_FAILURE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus =
		nicAddNewTlvElement(NAN_CMD_SET_DISC_BCN,
			sizeof(struct _NAN_CMD_EVENT_SET_DISC_BCN_T),
			u4CmdBufferLen, prCmdBuffer);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_INTERNAL_FAILURE;
	}

	prTlvElement = nicGetTargetTlvElement(1, prCmdBuffer);

	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_INTERNAL_FAILURE;
	}

	prCmdNanSetDiscBcn =
		(struct _NAN_CMD_EVENT_SET_DISC_BCN_T *)prTlvElement->aucbody;
	prCmdNanSetDiscBcn->ucDiscBcnType = prNanSetDiscBcn->ucDiscBcnType;
	prCmdNanSetDiscBcn->ucDiscBcnPeriod = prNanSetDiscBcn->ucDiscBcnPeriod;

	kalMemCopy(&prCmdNanSetDiscBcn->rDiscBcnTimeline[0],
		   &prNanSetDiscBcn->rDiscBcnTimeline[0],
		   sizeof(prNanSetDiscBcn->rDiscBcnTimeline));

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, nanDevCommonSetCb,
				      nicCmdTimeoutCommon, u4CmdBufferLen,
				      (uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return NAN_STATUS_SUCCESS;
	else
		return NAN_STATUS_INTERNAL_FAILURE;
}

void nanDevDiscBcnPeriodEvtHandler(IN struct ADAPTER *prAdapter,
	IN uint8_t *pcuEvtBuf)
{
	struct _NAN_CMD_EVENT_SET_DISC_BCN_T *prDiscBcnPeriodEvt = NULL;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "prAdapter is NULL\n");
		return;
	}

	prDiscBcnPeriodEvt = (struct _NAN_CMD_EVENT_SET_DISC_BCN_T *)
		pcuEvtBuf;

	DBGLOG(NAN, INFO, "Update Disc Bcn Period(%d): %d->%d\n",
		prDiscBcnPeriodEvt->ucDiscBcnType,
		prAdapter->rWifiVar.ucDiscBcnPeriod,
		prDiscBcnPeriodEvt->ucDiscBcnPeriod);

	prAdapter->rWifiVar.ucDiscBcnPeriod =
		prDiscBcnPeriodEvt->ucDiscBcnPeriod;
}

void
nanDevUpdateBss(IN struct ADAPTER *prAdapter, int u4Idx)
{
	struct BSS_INFO *prnanBssInfo;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo;

	prNANSpecInfo = nanGetSpecificBssInfo(prAdapter, u4Idx);
	prnanBssInfo = GET_BSS_INFO_BY_INDEX(
		prAdapter, prNANSpecInfo->ucBssIndex);

	prnanBssInfo->eConnectionState = MEDIA_STATE_DISCONNECTED;
	UNSET_NET_ACTIVE(prAdapter, prnanBssInfo->ucBssIndex);
	nicDeactivateNetwork(prAdapter, prnanBssInfo->ucBssIndex);
	nicUpdateBss(prAdapter, prnanBssInfo->ucBssIndex);

	if (!IS_BSS_ACTIVE(prnanBssInfo)) {
		SET_NET_ACTIVE(prAdapter, prnanBssInfo->ucBssIndex);
		nicActivateNetwork(prAdapter, prnanBssInfo->ucBssIndex);
	}
	prnanBssInfo->eConnectionState = MEDIA_STATE_CONNECTED;
	nicUpdateBss(prAdapter, prnanBssInfo->ucBssIndex);
}

enum NanStatusType nanDevSetNmiAddress(IN struct ADAPTER *prAdapter,
	uint8_t *macAddress)
{
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct NanMacAddressEvent *prCmdNanChangeAddress = NULL;
	struct BSS_INFO *prnanBssInfo = (struct BSS_INFO *)NULL;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo;
	enum NAN_BSS_ROLE_INDEX eRole = NAN_BSS_INDEX_BAND0;

	for (eRole = 0; eRole < NAN_BSS_INDEX_NUM; eRole++) {
		prNANSpecInfo = nanGetSpecificBssInfo(prAdapter, eRole);
		prnanBssInfo = GET_BSS_INFO_BY_INDEX(
			prAdapter, prNANSpecInfo->ucBssIndex);

		DBGLOG(NAN, INFO, "BSS Idx:%d, Update NMI [" MACSTR "]\n",
				prNANSpecInfo->ucBssIndex, MAC2STR(macAddress));
		COPY_MAC_ADDR(prnanBssInfo->aucOwnMacAddr, macAddress);
		nanDevUpdateBss(prAdapter, eRole);
	}
	COPY_MAC_ADDR(prAdapter->rDataPathInfo.aucLocalNMIAddr, macAddress);

	/* Driver CMD */
	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct NanMacAddressEvent);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicAddNewTlvElement(NAN_CMD_CHANGE_ADDRESS,
				      sizeof(struct NanMacAddressEvent),
				      u4CmdBufferLen, prCmdBuffer);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prTlvElement = nicGetTargetTlvElement(1, prCmdBuffer);

	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prCmdNanChangeAddress = (struct NanMacAddressEvent *)
		prTlvElement->aucbody;
	COPY_MAC_ADDR(prCmdNanChangeAddress, macAddress);

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, nanDevCommonSetCb,
				      nicCmdTimeoutCommon, u4CmdBufferLen,
				      (uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return NAN_STATUS_SUCCESS;
	else
		return NAN_STATUS_INTERNAL_FAILURE;
}

enum NanStatusType nanDevSetNdiAddress(IN struct ADAPTER *prAdapter,
	uint8_t *macAddress)
{
	struct _GL_NAN_INFO_T *prNANInfo = (struct _GL_NAN_INFO_T *)NULL;

	prNANInfo = prAdapter->prGlueInfo->aprNANDevInfo[NAN_BSS_INDEX_BAND0];

	DBGLOG(NAN, INFO, "MAC dev addr in netdev [" MACSTR "]\n",
		MAC2STR(prNANInfo->prDevHandler->dev_addr));
	DBGLOG(NAN, INFO, "MAC perm addr in netdev [" MACSTR "]\n",
		MAC2STR(prNANInfo->prDevHandler->perm_addr));

	COPY_MAC_ADDR(prNANInfo->prDevHandler->dev_addr, macAddress);
	COPY_MAC_ADDR(prNANInfo->prDevHandler->perm_addr, macAddress);
	COPY_MAC_ADDR(prAdapter->rDataPathInfo.aucLocalNDIAddr, macAddress);

	return NAN_STATUS_SUCCESS;
}

enum NanStatusType
nanDevEnableRequest(IN struct ADAPTER *prAdapter,
		    struct NanEnableRequest *prEnableReq) {
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct NanEnableRequest *prCmdNanEnableReq = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct NanEnableRequest);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicAddNewTlvElement(NAN_CMD_ENABLE_REQUEST,
				      sizeof(struct NanEnableRequest),
				      u4CmdBufferLen, prCmdBuffer);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prTlvElement = nicGetTargetTlvElement(1, prCmdBuffer);

	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prCmdNanEnableReq = (struct NanEnableRequest *)prTlvElement->aucbody;
	kalMemCopy(prCmdNanEnableReq, prEnableReq,
		   sizeof(struct NanEnableRequest));

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, nanDevCommonSetCb,
				      nicCmdTimeoutCommon, u4CmdBufferLen,
				      (uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return NAN_STATUS_SUCCESS;
	else
		return NAN_STATUS_INTERNAL_FAILURE;
}

enum NanStatusType
nanDevDisableRequest(IN struct ADAPTER *prAdapter) {
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus = nicAddNewTlvElement(NAN_CMD_DISABLE_REQUEST, 0,
				      u4CmdBufferLen, prCmdBuffer);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	prTlvElement = nicGetTargetTlvElement(1, prCmdBuffer);

	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return NAN_STATUS_NO_RESOURCE_AVAILABLE;
	}

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, nanDevCommonSetCb,
				      nicCmdTimeoutCommon, u4CmdBufferLen,
				      (uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return NAN_STATUS_SUCCESS;
	else
		return NAN_STATUS_INTERNAL_FAILURE;
}

void
nanDevMasterIndEvtHandler(IN struct ADAPTER *prAdapter, IN uint8_t *pcuEvtBuf) {
	struct _NAN_ATTR_MASTER_INDICATION_T *prMasterIndEvt;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;

	prMasterIndEvt = (struct _NAN_ATTR_MASTER_INDICATION_T *)pcuEvtBuf;
	/* dumpMemory8((PUINT_8) pcuEvtBuf, 32); */

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return;
	}

	prNANSpecInfo = nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_BAND0);

	kalMemCopy(&prNANSpecInfo->rMasterIndAttr, prMasterIndEvt,
		   sizeof(struct _NAN_ATTR_MASTER_INDICATION_T));
}

uint32_t
nanDevGetMasterIndAttr(IN struct ADAPTER *prAdapter,
		       uint8_t *pucMasterIndAttrBuf,
		       uint32_t *pu4MasterIndAttrLength) {
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	prNANSpecInfo = nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_BAND0);

	if (prNANSpecInfo == NULL)
		return WLAN_STATUS_FAILURE;

	kalMemCopy(pucMasterIndAttrBuf, &prNANSpecInfo->rMasterIndAttr,
		   sizeof(struct _NAN_ATTR_MASTER_INDICATION_T));
	*pu4MasterIndAttrLength = sizeof(struct _NAN_ATTR_MASTER_INDICATION_T);

	return WLAN_STATUS_SUCCESS;
}

void
nanDevClusterIdEvtHandler(IN struct ADAPTER *prAdapter, IN uint8_t *pcuEvtBuf) {
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return;
	}

	prNANSpecInfo = nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_BAND0);

	kalMemCopy(&prNANSpecInfo->aucClusterId, pcuEvtBuf, MAC_ADDR_LEN);
}

uint32_t
nanDevGetClusterId(IN struct ADAPTER *prAdapter, OUT uint8_t *pucClusterId) {
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	prNANSpecInfo = nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_BAND0);

	if (prNANSpecInfo == NULL)
		return WLAN_STATUS_FAILURE;

	kalMemCopy(pucClusterId, &prNANSpecInfo->aucClusterId, MAC_ADDR_LEN);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanDevSendEnableRequestToCnm(IN struct ADAPTER *prAdapter)
{
	struct MSG_CH_REQ *prMsgChReq = NULL;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
	(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
	struct BSS_INFO *prnanBssInfo = (struct BSS_INFO *)NULL;
	uint8_t ucIdx;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	prNANSpecInfo = prAdapter
		->rWifiVar.aprNanSpecificBssInfo[NAN_BSS_INDEX_BAND0];
	if (prNANSpecInfo == NULL) {
		DBGLOG(NAN, ERROR,
			"[%s] prNANSpecInfo is NULL\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	prnanBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter, prNANSpecInfo->ucBssIndex);
	if (prnanBssInfo == NULL) {
		DBGLOG(NAN, ERROR,
			"[%s] prnanBssInfo is NULL\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	prMsgChReq =
		(struct MSG_CH_REQ *)cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG,
		sizeof(struct MSG_CH_REQ));
	if (!prMsgChReq) {
		DBGLOG(NAN, ERROR, "Can't indicate CNM\n");
		return WLAN_STATUS_FAILURE;
	}

	prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
	prMsgChReq->ucBssIndex = prNANSpecInfo->ucBssIndex;
	prMsgChReq->ucTokenID = ++prAdapter->ucNanReqTokenId;
	prMsgChReq->ucPrimaryChannel = prnanBssInfo->ucPrimaryChannel;
	prMsgChReq->eRfSco = prnanBssInfo->eBssSCO;
	prMsgChReq->eRfBand = prnanBssInfo->eBand;
	prMsgChReq->eRfChannelWidth = prnanBssInfo->ucVhtChannelWidth;
	prMsgChReq->ucRfCenterFreqSeg1 = prnanBssInfo->ucVhtChannelFrequencyS1;
	prMsgChReq->ucRfCenterFreqSeg2 = prnanBssInfo->ucVhtChannelFrequencyS1;
	prMsgChReq->eReqType = CH_REQ_TYPE_NAN_ON;
	prMsgChReq->u4MaxInterval = 20;
	prMsgChReq->eDBDCBand = ENUM_BAND_AUTO;

	DBGLOG(NAN, INFO,
		"NAN req CH for N:%d,Tkn,%d\n",
		prMsgChReq->ucBssIndex,
		prMsgChReq->ucTokenID);

	/** Set BSS to inactive */
	for (ucIdx = 0; ucIdx < NAN_BSS_INDEX_NUM; ucIdx++) {
		prNANSpecInfo = prAdapter
			->rWifiVar.aprNanSpecificBssInfo[ucIdx];
		if (prNANSpecInfo == NULL)
			continue;
		prnanBssInfo =
			GET_BSS_INFO_BY_INDEX(prAdapter,
			prNANSpecInfo->ucBssIndex);
		if (prnanBssInfo == NULL)
			continue;
		UNSET_NET_ACTIVE(prAdapter, prnanBssInfo->ucBssIndex);
	}

	nanDevDumpBssStatus(prAdapter);

	mboxSendMsg(prAdapter, MBOX_ID_0,
		(struct MSG_HDR *)prMsgChReq,
		MSG_SEND_METHOD_BUF);

	prAdapter->fgIsNanSendRequestToCnm = TRUE;

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanDevSendAbortRequestToCnm(IN struct ADAPTER *prAdapter)
{
	struct MSG_CH_ABORT *prMsgChAbort = NULL;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	prNANSpecInfo = prAdapter
		->rWifiVar.aprNanSpecificBssInfo[NAN_BSS_INDEX_BAND0];
	if (prNANSpecInfo == NULL) {
		DBGLOG(NAN, ERROR,
			"[%s] prNANSpecInfo is NULL\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	prMsgChAbort =
	(struct MSG_CH_ABORT *)cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG,
		sizeof(struct MSG_CH_ABORT));
	if (!prMsgChAbort) {
		DBGLOG(NAN, ERROR, "Can't indicate CNM\n");
		return WLAN_STATUS_FAILURE;
	}

	prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
	prMsgChAbort->ucBssIndex = prNANSpecInfo->ucBssIndex;
	prMsgChAbort->ucTokenID = prAdapter->ucNanReqTokenId;
	prMsgChAbort->eDBDCBand = ENUM_BAND_AUTO;

	DBGLOG(NAN, INFO,
		"NAN abort CH for N:%d,Tkn,%d\n",
		prMsgChAbort->ucBssIndex,
		prMsgChAbort->ucTokenID);

	mboxSendMsg(prAdapter, MBOX_ID_0,
		(struct MSG_HDR *)prMsgChAbort,
		MSG_SEND_METHOD_BUF);

	prAdapter->fgIsNanSendRequestToCnm = FALSE;

	return WLAN_STATUS_SUCCESS;
}

void
nanDevSendEnableRequest(struct ADAPTER *prAdapter,
		     struct MSG_HDR *prMsgHdr)
{
	struct NanEnableRequest rEnableReq;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNANSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
	struct BSS_INFO *prnanBssInfo = (struct BSS_INFO *)NULL;
	uint8_t ucIdx;

	/** Update BSS Info and set BSS to connected state */
	for (ucIdx = 0; ucIdx < NAN_BSS_INDEX_NUM; ucIdx++) {

		prNANSpecInfo = prAdapter->rWifiVar
				.aprNanSpecificBssInfo[ucIdx];
		prnanBssInfo =
			prAdapter->aprBssInfo[prNANSpecInfo->ucBssIndex];

		if (!IS_BSS_ACTIVE(prnanBssInfo)) {
			SET_NET_ACTIVE(prAdapter, prnanBssInfo->ucBssIndex);
			nicActivateNetwork(prAdapter, prnanBssInfo->ucBssIndex);
		}

		prnanBssInfo->eConnectionState = MEDIA_STATE_CONNECTED;

		nicUpdateBss(prAdapter, prnanBssInfo->ucBssIndex);
	}

	/** Send NAN enable request to FW */
	kalMemZero(&rEnableReq, sizeof(struct NanEnableRequest));
	rEnableReq.master_pref = prAdapter->rWifiVar.ucMasterPref;
	rEnableReq.config_random_factor_force = 0;
	rEnableReq.random_factor_force_val = 0;
	rEnableReq.config_hop_count_force = 0;
	rEnableReq.hop_count_force_val = 0;
	rEnableReq.config_5g_channel = prAdapter->rWifiVar.ucConfig5gChannel;
	rEnableReq.channel_5g_val = prAdapter->rWifiVar.ucChannel5gVal;

	nanDevEnableRequest(prAdapter, &rEnableReq);

	nanDevSendAbortRequestToCnm(prAdapter);

	DBGLOG(NAN, INFO, "[NAN INIT] done\n");

	cnmMemFree(prAdapter, prMsgHdr);
}

void nanDevSetConfig(IN struct ADAPTER *prAdapter)
{
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_CMD_UPDATE_CONFIG *prCmdNanUpdateConfig = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_CMD_UPDATE_CONFIG);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;

	prTlvCommon->u2TotalElementNum = 0;

	rStatus =
		nicAddNewTlvElement(NAN_CMD_SET_NAN_CONFIG,
				    sizeof(struct _NAN_CMD_UPDATE_CONFIG),
				    u4CmdBufferLen, prCmdBuffer);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvElement = nicGetTargetTlvElement(1, prCmdBuffer);

	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prCmdNanUpdateConfig =
		(struct _NAN_CMD_UPDATE_CONFIG *)prTlvElement->aucbody;

	prCmdNanUpdateConfig->ucSupportVendorIoctl =
		prAdapter->rWifiVar.ucNanVendorIoctl;

	DBGLOG(NAN, INFO,
		"SET_NAN_CONFIG:0x%x\n",
		prCmdNanUpdateConfig->ucSupportVendorIoctl);

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_NAN_EXT_CMD, TRUE,
				      FALSE, FALSE, nanDevCommonSetCb,
				      nicCmdTimeoutCommon, u4CmdBufferLen,
				      (uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);
}
#endif

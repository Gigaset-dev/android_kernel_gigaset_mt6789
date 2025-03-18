#
# Makefile for all kinds of Fingerprint
#

obj-$(CONFIG_GOODIX_FINGERPRINT) += goodix/
obj-$(CONFIG_FPC_FINGERPRINT) += fpc/
# prize-add-liaoxingen-20230114-start
obj-$(CONFIG_GOODIX_GW9518_FINGERPRINT) += goodix-gw9518/
# prize-add-liaoxingen-20230114-end
# begin, prize-pengguangyi-20190119, add for goodix gf5118m fp
obj-$(CONFIG_GOODIX_GF5118M_FINGERPRINT) += goodix-gf5118m/
# end, prize-pengguangyi-20190119, add for goodix gf5118m fp
# prize-add-pengzhipeng-20191105 end
obj-$(CONFIG_MTK_FINGERPRINT_SUNWAVECORP) += sunwave_driver_all_in_one/
obj-$(CONFIG_FT9362_FINGERPRINT) += focaltech/
obj-$(CONFIG_FT9362_FINGERPRINT_TEE) += focaltech_tee/
# prize-add-pengzhipeng-20191105-end
# prize-add-durunshen-focaltech fingerprint-20230410 start
obj-$(CONFIG_FINGERPRINT_FOCALTECH)  += focaltech_fp/
obj-$(CONFIG_MTK_FINGERPRINT_FT9391_TEE)  += ft9391_tee/
# prize-add-durunshen-focaltech fingerprint-20230410-end
# prize-add-xuxiaoming-20200112-start
obj-$(CONFIG_SILEAD_FP)		+= silead/
obj-$(CONFIG_SILEAD_FP_PLATFORM)		+= silead/
# prize-add-xuxiaoming-20200112-end

#prize added by huarui, add fp driver, 20190327-start
obj-$(CONFIG_CHIPONE_FINGERPRINT_REE) += chipone_ree/
#prize added by huarui, add fp driver, 20190327-end


#prize add by lipengpeng 20200628 start
obj-$(CONFIG_CHIPONE_FINGERPRINT_TEE) += chipone_tee/
#prize add by lipengpeng 20200628 end
obj-$(CONFIG_SILEAD_FP)		+= silead/
#prize add by tangcong 20200715 start
obj-$(CONFIG_BETTERLIFE_FINGERPRINT) += betterlife_fp/
#prize add by tangcong 20200715 end
#prize add by wangyongsheng 20210204 start
obj-$(CONFIG_CDFINGER_REE) += cdfinger_ree/
#prize add by wangyongsheng 20210204 end
#prize add by wangyongsheng 20210327 start
obj-$(CONFIG_MICROARRAY_FP) += microarray_fp/
#prize add by wangyongsheng 20210327 end
#prize add by tangcong  20210827 start 
obj-$(CONFIG_MTK_FINGERPRINT_FORTSENSE) += fortsense_driver_all_in_one/
obj-$(CONFIG_FINGERPRINT_FOCALTECH_REE_ATA)	+= focaltech_ree_ata/
#prize add by tangcong  20210827 end 
#prize add by dengbinggui  20240927 start 
obj-$(CONFIG_PRI_FINGER_ATA_NODE) += pri_finger_ftm_node/
#prize add by dengbinggui  20240927 end
FINGERPRINT_LISTS := $(subst ",,$(CONFIG_MTK_FINGERPRINT_SUPPORT_LIST))
obj-y += $(foreach FINGERPRINT,$(FINGERPRINT_LISTS),$(FINGERPRINT)/)

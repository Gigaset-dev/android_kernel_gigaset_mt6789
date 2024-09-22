#ifndef PANEL_NT37700C_FHDP_DSI_VDO_120HZ
#define PANEL_NT37700C_FHDP_DSI_VDO_120HZ


#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2412

#define PHYSICAL_WIDTH              69984
#define PHYSICAL_HEIGHT             151632

#define DATA_RATE                   390*2
#define HSA                         8
#define HBP                         92
#define VSA                         4
#define VBP                         8

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
#define MODE_0_VFP                  72
#define MODE_0_HFP                  100
#define MODE_0_DATA_RATE            390*2
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  90
#define MODE_1_VFP                  72
#define MODE_1_HFP                  100
#define MODE_1_DATA_RATE            390*2
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  120
#define MODE_2_VFP                  72
#define MODE_2_HFP                  100
#define MODE_2_DATA_RATE            390*2
/*Parameter setting for mode 2 End*/
/*
VSA 4
VBP 8
VFP 72
HSA 8
HBP 92
HFP 100

static const struct drm_display_mode default_mode = {
	.clock = 227090,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
	.vrefresh = MODE_0_FPS,
};

*/
#define LFR_EN                      1
/* DSC RELATED */

#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
//drv-modify 10bit-pengzhipeng-20230429-start
#define DSC_DSC_CFG                 2088
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10
#define DSC_DSC_LINE_BUF_DEPTH      11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           160
//define DSC_PIC_HEIGHT
//define DSC_PIC_WIDTH
#define DSC_SLICE_HEIGHT            12
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              675
#define DSC_XMIT_DELAY              410
#define DSC_DEC_DELAY               472
#define DSC_SCALE_VALUE             25
#define DSC_INCREMENT_INTERVAL      259
#define DSC_DECREMENT_INTERVAL      10
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          2235
#define DSC_SLICE_BPG_OFFSET        2655
#define DSC_INITIAL_OFFSET          5632
#define DSC_FINAL_OFFSET            4332
#define DSC_FLATNESS_MINQP          7
#define DSC_FLATNESS_MAXQP          16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15
#define DSC_RC_QUANT_INCR_LIMIT1    15
//drv-modify 10bit-pengzhipeng-20230429-end
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3
bool drv_doze_state(void);//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516
bool panel_fod_is_enabled(void);

#endif //end of PANEL_NT36672E_FHDP_DSI_VDO_120HZ_TIANMA_HFP

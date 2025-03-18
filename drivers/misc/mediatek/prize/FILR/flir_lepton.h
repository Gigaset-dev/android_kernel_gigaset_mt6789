#define LEPTON_MODULE_NAME "lepton"
#define VERSION "1.0"
#define lepton_major 240
#define lepton_minor 0
//drv add by lipengpeng 20240227 start
#define SPI_FILR_NAME "lepton_drv" 
//drv add by lipengpeng 20240227 end
#define LEPTON_LINE_WIDTH 62  	// 16-bit values per line
#define LEPTON_SUBFRAME_HEIGHT 80  // lines per subframe
#define FRAME_WIDTH_LEPTON_2 LEPTON_LINE_WIDTH
#define FRAME_WIDTH_LEPTON_3 (LEPTON_LINE_WIDTH * 2)
#define FRAME_HEIGHT_LEPTON_2 LEPTON_SUBFRAME_HEIGHT
#define FRAME_HEIGHT_LEPTON_3 (LEPTON_SUBFRAME_HEIGHT*2)
/*
 * The lepton will choke on capturing the next frame if the SPI transfers
 * from the previous frame were still occurring during a short period
 * before the VSYNC gets sent. Tune this parameter for situations in
 * which SPI transfers complete in time, but are marginal (evidenced
 * by error packets arriving in all subsequent frames). This situation
 * can only be resolved by resetting the lepton.
 */
#define ONE_MS_IN_NS 1000000
// the last SPI transfer needs to complete this many ns before VSYNC
#define MINIMUM_SPI_TRANSFER_QUIET_TIME (1 * ONE_MS_IN_NS)
// how many discards in a row can be received before resyncing
#define MAX_CONSEC_DISCARD_COUNT 4
// how many subframes should be skipped to allow lepton to resync
#define MAX_RESYNC_SKIP_COUNT 5
//drv add by lipengpeng 20240227 start
//extern int spidev_probe(struct spi_device *spi);
extern void filr_power_enable(int status);
extern int filr_status;
#define MY_MACIG 'G'
#define START_SPI_IOCTL _IOR(MY_MACIG, 3, __u32)
#define STOP_SPI_IOCTL _IOR(MY_MACIG, 4, __u32)

//drv add by lipengpeng 20240227 start
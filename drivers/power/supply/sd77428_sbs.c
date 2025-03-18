#include "sd77428_2.h"

unsigned int sbsd_cmd_def[SBSD_CMD_MAX] = 
{
    (0x00UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x00000234UL, /* CONTROL */
    (0x02UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x02000235UL, /* Temperature */
    (0x04UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x04000215UL, /* Voltage */
    (0x06UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x06000215UL, /* Flags */
    // (0x08UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x08000215UL, /* NominalAvailableCapacity */
    // (0x0AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x0A000215UL, /* FullAvailableCapacity */
    (0x0CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x0C000215UL, /* RemainingCapacity */
    (0x0EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x0E000215UL, /* FullChargeCapacity */
    (0x10UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),       //0x10000215UL, /* AverageCurrent */
    // (0x18UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x18000215UL, /* AveragePower */
    (0x1CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x1C000215UL, /* StateOfCharge */
    (0x1EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x1E000214UL, /* InternalTemperature */
    (0x20UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x20000215UL, /* StateOfHealth */
    // (0x28UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x28000215UL, /* RemainingCapacityUnfiltered */
    // (0x2AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x2A000215UL, /* RemainingCapacityFiltered */
    // (0x2CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x2C000214UL, /* FullChargeCapacityUnfiltered */
    // (0x2EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x2E000214UL, /* FullChargeCapacityFiltered */
    // (0x30UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x30000214UL, /* StateOfChargeUnfiltered */

    (0x60UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x60000214UL, /* CC LSB */
    (0x61UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x61000214UL, /* CADC dither enable */

    // (0x62UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x62000214UL, /* Run time to empty */
    // (0x63UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x63000214UL, /* Average time to empty */
    // (0x64UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),        //0x64000214UL, /* Average time to full */
    (0x65UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x65000214UL, /* Cycle count, */  
    (0x66UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x66000215UL, /* Discharge Capacity */
    (0x67UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x67000215UL, /* Charge Capacity */
    (0x68UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x68000215UL, /* Fully charge end current */
    (0x69UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x69000215UL, /* Fully charge end timeout */
    (0x6AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x6a000215UL, /* Discharge end voltage */
    (0x6BUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x6b000215UL, /* Enable seal via sha256 */
    (0x6CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x6c000215UL, /* Charge threshold */
    (0x6DUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),       //0x6d000215UL, /* DisCharge threshold */
    (0x6EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x6e000215UL, /* Fast wkup threshold */
    (0x6FUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x6F000214UL, /* CADC DB threshold */
    (0x70UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x70000214UL, /* CADC wakeup threshold */
    (0x71UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x71000215UL, /* Rsence*/
    (0x72UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x72000215UL, /* Debug mode*/
    (0x73UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x73000215UL, /* Idle to sleep time */
    (0x74UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x74000215UL, /* Sleep time */
    (0x75UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x75000215UL, /* Deep Sleep time  */
    (0x76UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x76000215UL, /* Power of sleep */
    (0x77UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x77000215UL, /* Power of deep sleep */
    
    (0x78UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x78000215UL, /* Charge current slope*/
    (0x79UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x79000215UL, /* Charge current offset */
    (0x7AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x7A000215UL, /* Discharge current slope */
    (0x7BUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x7B000215UL, /* Discharge current offset  */
    (0x7CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x7C000215UL, /* Voltage offset */
    (0x7DUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x7D000215UL, /* External temperature offset */   

    (0x7EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x7E000215UL, /* SOC1 SET THRESHOLD*/
    (0x7FUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x7F000215UL, /* SOC1 CLEAR THRESHOLD */
    (0x80UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x80000215UL, /* SOCF SET THRESHOLD*/
    (0x81UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x81000215UL, /* SOCF CLEAR THRESHOLD */
    (0x82UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x82000215UL, /* SOC1 DELTA */
    (0x83UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x83000215UL, /* OPCONFIG */  
    (0x84UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),      //0x84000215UL, /* Historical CC */     
    (0x85UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x85000215UL, /* Change RCtable */        
    
    (0x86UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x86000215UL, /* Charge IR */ 
    (0x87UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),       //0x87000215UL, /* LOWTEMP */   
    (0x88UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x88000234UL, /* INITSOC */
    (0x89UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x89000234UL, /* SYSIM */
    (0x8AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x8A000234UL, /* EOC CDV */
    (0x8BUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x8B000234UL, /* EOD DDV */
    (0x8CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),   //0x8C000215UL, /* Temperature table index */

    (0x8DUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),       //0x8D000215UL, /* Cadc zero offset */  
    (0x8FUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x8F000215UL, /* SBS Send finished */ 
    (0x90UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x90000234UL, /* Coulomb Counting, */
    (0x91UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x91000234UL, /* External charger status */
    
    (0x92UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x92000234UL, /* Dynamic Remaining Capacity */
    (0x93UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x93000234UL, /* Dynamic FCC */
    (0x94UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x94000234UL, /* Debug ratio */
    (0x95UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x95000234UL, /* Debug CC PREV */
    (0x96UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x96000234UL, /* Debug SOC NOW */
    (0x97UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0x97000234UL, /* Debug SOC END */
    (0x98UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),       //0x97000234UL, /* Debug AVERAGE CURRENT */
    // cablirateion-
    (0xA0UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),          /* SET chg/dis thresh  */
    (0xA1UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),          /* SET chg/dis thresh  */
    (0xA2UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    // filter
    (0xA3UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),          /* SET chg/dis thresh  */
    (0xA4UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),          /* SET chg/dis thresh  */
    (0xA5UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xA6UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    // libfg
    (0xA7UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xA8UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xA9UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xAAUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xABUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xACUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xADUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xAEUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xAFUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),

    (0xB0UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
    (0xB1UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),

    (0xD0UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xD0002018UL, /* Debug message */
    (0xD1UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xD1002018UL, /* Debug message */
    (0xD2UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xD2002018UL, /* Debug message */
    (0xD3UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xD3002018UL, /* Debug message */
    // table
    (0xDFUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0xDF000214UL, /* Table msg package index */
    (0xE0UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xE0002018UL, /* SBSE0_TABLE_OCV */
    (0xE1UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xE1002018UL, /* SBSE1_TABLE_VOLT */
    (0xE2UL << SBSD_CMD_Pos) | (8UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),     //0xE2002018UL, /* SBSE2_TABLE_CURR */
    (0xE3UL << SBSD_CMD_Pos) | (8UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),     //0xE3002018UL, /* SBSE3_TABLE_TEMP */
    (0xE4UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xE4002018UL, /* SBSE4_TABLE_RC */
    (0xE5UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xE4002018UL, /* SBSE5_TABLE_NTC_TEMP */
    
    (0xE6UL << SBSD_CMD_Pos) | (12UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xE2002018UL, /* SBSE6_TABLE_DFCC_SOC */
    (0xE7UL << SBSD_CMD_Pos) | (6UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),     //0xE3002018UL, /* SBSE7_TABLE_DFCC_CURR */
    (0xE8UL << SBSD_CMD_Pos) | (8UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),     //0xE3002018UL, /* SBSE8_TABLE_DFCC_TEMP */
    (0xE9UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),    //0xE4002018UL, /* SBSE9_TABLE_DFCC_RATIO */

    (0xF1UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),       //0xF1000217UL, /* Chip version */
    (0xF2UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),      //0xF2000417UL, /* Firmware version */
    (0xF9UL << SBSD_CMD_Pos) | (SBSF9_WR_LENGTH << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_PRM),      //0xF9000336UL, /* Parameter Read Write */
    //user defined SBS command
};

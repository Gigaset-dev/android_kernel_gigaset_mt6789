#ifndef _PRIZE_COMMON_NODE_H_
#define _PRIZE_COMMON_NODE_H_
void prize_common_node_register(char* name,void(*set)(unsigned char on_off));
#if defined(CONFIG_PRIZE_UNDERWATER_TOUCH_CONTROL)
extern bool underwater_report_status;
#endif
#endif 
/**
 * @file doca_ar_env.h
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief build base enviroment of DPDK and Doca-Flow
 * @version 1.0
 * @date 2024-01-07
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef DOCA_AR_ENV_H_
#define DOCA_AR_ENV_H_
#include <string.h>
#include <rte_byteorder.h>
#include <doca_log.h>
#include <doca_argp.h>
#include <dpdk_utils.h>
#include <doca_flow.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>

#define NB_PORTS 2                                 ///< we use 2 SF ports
extern int to_host_port;                           ///< port connected with host pf
extern int to_net_port;                            ///< port connected with uplink port
extern struct doca_flow_port *ports[NB_PORTS];     ///< pointer of doca-flow port
extern struct application_dpdk_config dpdk_config; ///< dpdk config
/**
 * @brief build doca-flow and dpdk env
 *
 * @param argc
 * @param argv
 * @return int
 */
int doca_ar_env_init(int argc, char **argv);
/**
 * @brief release all the resource of dpdk and doca-flow
 *
 */
void doca_ar_env_destroy();
#endif /* DOCA_AR_ENV_H_ */
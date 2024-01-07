/**
 * @file doca_ar_pipe.h
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief build needed doca-flow pipe
 * @version 1.0
 * @date 2024-01-07
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef DOCA_AR_PIPE_H_
#define DOCA_AR_PIPE_H_
#include "doca_ar_env.h"
#include "doca_ar_conntrack.h"
/**
 * @brief default time out for processing offloaded entry
 *
 */
#define DEFAULT_TIMEOUT_US (10000)
/**
 * @brief the max amount of aged entry per polling the aging api
 *
 */
#define MAX_AGED_CT_PER_POLL 16
/**
 * @brief build needed pipe
 *
 * @return int
 */
int doca_ar_pipe_init();
/**
 * @brief add entry to the vxlan pipe so that ar can come into effect
 *
 * @param conn
 * @return int
 */
int doca_ar_add_new_flow(struct doca_ar_conn *conn);
/**
 * @brief aged expired conns from doca-flow table(FDB) and del them from conntrack table
 *
 * @return int the amount of aged conns
 */
int doca_ar_flow_aging();

#endif /* DOCA_AR_PIPE_H_ */
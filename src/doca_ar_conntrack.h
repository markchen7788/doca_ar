/**
 * @file doca_ar_conntrack.h
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief simple connection tracking table module for recording flow info and modifying headers of packets forwarded by software
 * @version 1.0
 * @date 2024-01-07
 *
 * @copyright Copyright (c) 2024
 *
 */
#ifndef DOCA_AR_CONNTRACK_H_
#define DOCA_AR_CONNTRACK_H_
#include "doca_ar_env.h"
#include <cmdline.h>

#define MAX_CONNTRACK 1 << 14 ///< maximum connections can be stored in rte_mempool and offloaded into eSwitch

/**
 * @brief match of hash table and l4-connection
 *
 */
struct doca_ar_conn_match
{
    uint32_t sip;
    uint32_t dip;
    uint16_t sport;
    uint16_t dport;
    uint32_t rss_val; ///< used to store rss value precomputed by hardware
} __rte_cache_aligned;

typedef void (*ExpireCallback)(void *arg); ///< user-defined callback function when expiring connection

/**
 * @brief context of a connection
 *
 */
struct doca_ar_conn
{
    struct doca_ar_conn_match match;
    struct doca_flow_pipe_entry *entry; ///< used to store the pointer of doca-flow entry
    ExpireCallback expireCallback;
    void *expireCallbackArgs;
    uint64_t expireTime;
    uint16_t bestPath; ///< used to store the best path we probed by adptive routing algorithm
} __rte_cache_aligned;

/**
 * @brief init connection tracking table and mempool
 *
 * @param maxConntrack
 * @return int
 */
int doca_ar_conntrack_init_env(int maxConntrack);
/**
 * @brief pasrse conn match from rte_mbuf
 *
 * @param match
 * @param m
 * @return int
 */
int doca_ar_parse_conn(struct doca_ar_conn_match *match, struct rte_mbuf *m);
/**
 * @brief debug api for pring match info
 *
 * @param match
 */
void doca_ar_print_match(struct doca_ar_conn_match *match);

/**
 * @brief get conn from mempool and add conn into the conntrack table
 *
 * @param match
 * @param bestPath
 * @return struct doca_ar_conn*
 */
struct doca_ar_conn *doca_ar_add_conn(struct doca_ar_conn_match *match, uint16_t bestPath);
/**
 * @brief find the conn from conntrack table
 *
 * @param match
 * @return struct doca_ar_conn*
 */
struct doca_ar_conn *doca_ar_find_conn(struct doca_ar_conn_match *match);

/**
 * @brief del conn from the conntrack table and put back to mempool
 *
 * @param conn
 */
void doca_ar_del_conn(void *conn);

/**
 * @brief modify the sport of conn and offload cksum
 *
 * @param conn
 * @param m
 */
void doca_ar_modify_conn(struct doca_ar_conn *conn, struct rte_mbuf *m);
/**
 * @brief iterate the whole conntrack table and print all conns info onto cmdline
 *
 * @param cl
 */
void doca_ar_dump_conn(struct cmdline *cl);

#endif /* DOCA_AR_CONNTRACK_H_ */
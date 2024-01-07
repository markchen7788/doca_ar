
/**
 * @file doca_ar_conntrack.c
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief simple connection tracking table module for recording flow info and modifying headers of packets forwarded by software
 * @version 1.0
 * @date 2024-01-07
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "doca_ar_conntrack.h"
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
DOCA_LOG_REGISTER(DOCA_AR_CONNTRACK);
struct rte_hash *CT = NULL;
struct rte_mempool *CT_POOL = NULL;
int maxConntrack = 0;
/**
 * @brief user-defined hash function,here we directly use the rss val precomputed by hardware as the result of hash function so that we can save the cpu cosumption
 *
 * @param key
 * @param key_len
 * @param init_val
 * @return uint32_t
 */
uint32_t myHash(const void *key, uint32_t key_len, uint32_t init_val)
{
    struct doca_ar_conn_match *mt = (struct doca_ar_conn_match *)key;
    return mt->rss_val; // rss val is precomputed hash val by hw
}
int doca_ar_conntrack_init_env(int _maxConntrack)
{
    maxConntrack = _maxConntrack;
    CT_POOL = rte_mempool_create("CT_POOL", maxConntrack,
                                 sizeof(struct doca_ar_conn), maxConntrack / 4 > 256 ? 256 : maxConntrack / 4, 0,
                                 NULL, NULL, NULL, NULL,
                                 rte_socket_id(), 0);
    if (CT_POOL == NULL)
    {
        DOCA_LOG_ERR("Create CT_POOL Fail");
        return -1;
    }
    DOCA_LOG_INFO("Create CT_POOL Success");

    const struct rte_hash_parameters ConnectionTable =
        {
            .name = "CT",
            .entries = maxConntrack,
            .reserved = 0,
            .key_len = sizeof(struct doca_ar_conn_match),
            .hash_func = myHash, // rte_jhash,
            .hash_func_init_val = 0,
            .socket_id = rte_socket_id(),
            .extra_flag = RTE_HASH_EXTRA_FLAGS_EXT_TABLE, // 0,
        };

    CT = rte_hash_create(&ConnectionTable);
    if (!CT)
    {
        DOCA_LOG_ERR("Create ConnectionTable fail!");
        return -1;
    }
    DOCA_LOG_INFO("Create CT[%d] success", maxConntrack);

    return 0;
}

struct doca_ar_conn *doca_ar_add_conn(struct doca_ar_conn_match *match, uint16_t bestPath)
{
    /////////////////////////////////////////////////////////// 1.get a ctx from pool
    struct doca_ar_conn *newConn = NULL;
    if (CT_POOL == NULL)
    {
        DOCA_LOG_ERR("Pool is NULL.....");
        return NULL;
    }
    if (rte_mempool_get(CT_POOL, (void **)&newConn) != 0)
    {
        DOCA_LOG_ERR("Cannot get newConn from pool.....");
        return NULL;
    }
    // DOCA_LOG_INFO("CTX_POOL In Use:%d", rte_mempool_in_use_count(POOL));

    ///////////////////////////////////////////////////////////// 2.put match->ctx into CT
    int ret = rte_hash_add_key_data(CT, match, newConn);
    if (ret < 0)
    {
        if (ret == -EINVAL)
        {
            DOCA_LOG_ERR("Invalid params.....");
        }
        else
            DOCA_LOG_ERR("No space.....");

        rte_mempool_put(CT_POOL, (void *)newConn);
        return NULL;
    }
    ///////////////////////////////////////////////////////////// 3.memcpy conn info
    memset(newConn, 0, sizeof(struct doca_ar_conn));
    rte_memcpy(&(newConn->match), match, sizeof(struct doca_ar_conn_match));
    newConn->bestPath = bestPath;

    return newConn;
}

void doca_ar_del_conn(void *_conn)
{
    struct doca_ar_conn *conn = _conn;
    int ret = rte_hash_del_key(CT, &(conn->match));
    if (ret < 0)
    {
        DOCA_LOG_ERR("CT Del failed");
    }
    // DOCA_LOG_INFO("Aging Flow");
}
struct doca_ar_conn *doca_ar_find_conn(struct doca_ar_conn_match *match)
{
    struct doca_ar_conn *conn = NULL;
    int ret = rte_hash_lookup_data(CT, match, (void **)&conn);
    return ret >= 0 ? conn : NULL;
}

int doca_ar_parse_conn(struct doca_ar_conn_match *match, struct rte_mbuf *m)
{
    if (RTE_ETH_IS_IPV4_HDR(m->packet_type))
    {
        struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
        match->sip = ip->src_addr;
        match->dip = ip->dst_addr;
        match->rss_val = m->hash.rss;
        struct rte_udp_hdr *udp;
        if (ip->next_proto_id == 17)
        {
            udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
            if (udp->dst_port == rte_cpu_to_be_16(4789))
            {
                match->sport = udp->src_port;
                match->dport = udp->dst_port;
                return 1;
            }
        }
    }
    return 0;
}

void doca_ar_print_match(struct doca_ar_conn_match *match)
{
    char buf1[100] = {0}, buf2[100] = {0};
    void print_ipv4_addr(const rte_be32_t sip, const rte_be32_t dip)
    {
        sprintf(buf1, "(SIP=%d.%d.%d.%d,DIP=%d.%d.%d.%d,",
                (sip & 0xff000000) >> 24,
                (sip & 0x00ff0000) >> 16,
                (sip & 0x0000ff00) >> 8,
                (sip & 0x000000ff),
                (dip & 0xff000000) >> 24,
                (dip & 0x00ff0000) >> 16,
                (dip & 0x0000ff00) >> 8,
                (dip & 0x000000ff));
    }
    print_ipv4_addr(htonl(match->sip), htonl(match->dip));
    sprintf(buf2, "UDP,SPORT=%u,DPORT=%u,RSS=%u)",
            rte_be_to_cpu_16(match->sport),
            rte_be_to_cpu_16(match->dport),
            match->rss_val);
    DOCA_LOG_INFO("%s%s", buf1, buf2);
}

void doca_ar_modify_conn(struct doca_ar_conn *conn, struct rte_mbuf *m)
{
    struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
    struct rte_udp_hdr *udp;
    udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
    udp->src_port = conn->bestPath;

    // offload ip and udp cksum
    m->l2_len = sizeof(struct rte_ether_hdr);
    m->l3_len = sizeof(struct rte_ipv4_hdr);

    m->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM;
    ipv4_hdr->hdr_checksum = 0;

    m->ol_flags |= PKT_TX_UDP_CKSUM;
    udp->dgram_cksum = 0;
}

void doca_ar_dump_conn(struct cmdline *cl)
{
    struct doca_ar_conn_match *match;
    struct doca_ar_conn *conn;
    uint32_t iter = 0;
    while (1)
    {
        /* code */
        int ret = rte_hash_iterate(CT, (const void **)&match, (void **)&conn, (uint32_t *)&iter);
        if (ret < 0)
            break;

        char buf1[100] = {0}, buf2[100] = {0};
        void print_ipv4_addr(const rte_be32_t sip, const rte_be32_t dip)
        {
            sprintf(buf1, "(SIP=%d.%d.%d.%d,DIP=%d.%d.%d.%d,",
                    (sip & 0xff000000) >> 24,
                    (sip & 0x00ff0000) >> 16,
                    (sip & 0x0000ff00) >> 8,
                    (sip & 0x000000ff),
                    (dip & 0xff000000) >> 24,
                    (dip & 0x00ff0000) >> 16,
                    (dip & 0x0000ff00) >> 8,
                    (dip & 0x000000ff));
        }
        print_ipv4_addr(htonl(match->sip), htonl(match->dip));
        sprintf(buf2, "UDP,SPORT=%u,DPORT=%u,RSS=%u)",
                rte_be_to_cpu_16(match->sport),
                rte_be_to_cpu_16(match->dport),
                match->rss_val);
        cmdline_printf(cl, "%s%s===>BestPath:%d\n", buf1, buf2, rte_be_to_cpu_16(conn->bestPath));
    }
    cmdline_printf(cl, "Total Active Connections: %d\n", rte_hash_count(CT));
}
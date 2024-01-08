/**
 * @file doca_ar_core.c
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief the critical logic of doca-ar
 * @version 1.0
 * @date 2024-01-07
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "doca_ar_core.h"
#include "doca_ar_conntrack.h"
#include "doca_ar_pipe.h"

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>
#include <rte_string_fns.h>
#include <cmdline_socket.h>
#include <rte_byteorder.h>
#include <cmdline_parse_etheraddr.h>
#include <rte_ethdev.h>

DOCA_LOG_REGISTER(DOCA_AR_CORE);
#define PACKET_BURST 128    ///< num of tx_burst and rx_burst
#define EXPIRE_TIME 10      ///< default timeout of conn
#define PROBE_PATH_AMOUNT 4 ///< default probed paths amount and packets amount we sent
#define PROBE_TIMEOUT 50    ///< Probe Timeout[ms]

/**
 * @brief packets num the control plane recv and sent
 *
 */
struct PortStats
{
    uint64_t rx;
    uint64_t tx;
} __rte_cache_aligned;

/**
 * @brief the user-defined probe packets header
 *
 */
struct PROBE_HDR
{
    uint64_t timeStamp;
    uint64_t FlowID; ///< used to distinguish probe packets we sent just now, packets sent before will be discarded
};
/**
 * @brief Load balancing scheme we used
 *
 */
enum LB_SCHEME
{
    DOCA_AR, ///< use doca-ar
    ECMP     ///< use ecmp
};

volatile bool force_quit = false;           ///< flag of quit
unsigned int runing_lore_id = 0;            ///< id of lcore processing packets
struct PortStats portStats[NB_PORTS] = {0}; ///< packets num the control plane recv and sent
enum LB_SCHEME lb_scheme = ECMP;            ///< Load balancing scheme we used

/**
 * @brief print packets num the control plane recv and sent
 *
 * @param cl
 */
void printPortStats(struct cmdline *cl)
{
    for (int i = 0; i < NB_PORTS; i++)
    {
        cmdline_printf(cl, "Port %d: RX-Pkts:%16lu TX-Pkts:%16lu\n", i, portStats[i].rx, portStats[i].tx);
    }
}

/* OvS flow for sending back probe packets in receiver DPU
ovs-ofctl del-flows ovsbr1
ovs-ofctl add-flow ovsbr1 "priority=300,in_port=p0,udp,tp_dst=4789,nw_tos=0x20 actions=mod_dl_dst:08:c0:eb:bf:ef:9a,mod_tp_dst:4788,output:IN_PORT"
ovs-ofctl add-flow ovsbr1 "priority=100,in_port=p0 actions=output:pf0hpf"
ovs-ofctl add-flow ovsbr1 "priority=100,in_port=pf0hpf actions=output:p0"
*/

/**
 * @brief the ar algorithm we used in doca-ar, this is the most critical function the whole app
 *
 * @param pool packets mempool
 * @param match the match of new conn
 * @param m  packet of this new conn
 * @return uint16_t the best path we probed and the final src port of this new conn
 */
uint16_t AR(struct rte_mempool *pool, struct doca_ar_conn_match *match, struct rte_mbuf *m)
{
    struct rte_mbuf *mbufs[PROBE_PATH_AMOUNT];
    int count = rte_pktmbuf_alloc_bulk(pool, mbufs, PROBE_PATH_AMOUNT) == 0 ? PROBE_PATH_AMOUNT : 0;
    int port_id = to_net_port;
    uint64_t flowID = rte_rdtsc();

    struct rte_ether_hdr *this_ether_h = rte_pktmbuf_mtod_offset(m, struct rte_ether_hdr *, 0);
    struct rte_ipv4_hdr *this_ip = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
    struct rte_udp_hdr *this_udp_h = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));

    for (int p = 0; p < count; p++)
    {
        struct rte_ether_hdr *ether_h;
        struct rte_ipv4_hdr *ip;
        struct rte_udp_hdr *udp_h;
        struct PROBE_HDR *pay;
        /**Ether**/
        ether_h = (struct rte_ether_hdr *)rte_pktmbuf_append(mbufs[p], sizeof(struct rte_ether_hdr));
        rte_memcpy(ether_h, this_ether_h, sizeof(struct rte_ether_hdr));
        /**IP**/
        ip = (struct rte_ipv4_hdr *)rte_pktmbuf_append(mbufs[p], sizeof(struct rte_ipv4_hdr));
        rte_memcpy(ip, this_ip, sizeof(struct rte_ipv4_hdr));
        ip->version_ihl = 0x45;
        ip->type_of_service = 0x20;
        ip->total_length = htons(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + sizeof(struct PROBE_HDR));
        ip->packet_id = 0;
        ip->fragment_offset = 0;
        ip->time_to_live = 64; // ttl = 64
        ip->next_proto_id = IPPROTO_UDP;
        ip->hdr_checksum = 0;
        /**UDP**/
        udp_h = (struct rte_udp_hdr *)rte_pktmbuf_append(mbufs[p], sizeof(struct rte_udp_hdr));
        rte_memcpy(udp_h, this_udp_h, sizeof(struct rte_udp_hdr));
        udp_h->src_port = rte_cpu_to_be_16(rte_be_to_cpu_16(this_udp_h->src_port) + p);
        udp_h->dgram_cksum = 0;
        udp_h->dgram_len = rte_cpu_to_be_16(sizeof(struct PROBE_HDR));
        /**Payload**/
        pay = (struct PROBE_HDR *)rte_pktmbuf_append(mbufs[p], sizeof(struct PROBE_HDR));
        pay->timeStamp = rte_rdtsc();
        pay->FlowID = flowID;
        /**offload cksum**/
        mbufs[p]->l2_len = sizeof(struct rte_ether_hdr);
        mbufs[p]->l3_len = sizeof(struct rte_ipv4_hdr);
        mbufs[p]->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
    }
    int nb_tx = rte_eth_tx_burst(port_id, 0, mbufs, count);
    // DOCA_LOG_INFO("Sent %d Probe Packets", count);
    if (unlikely(nb_tx < count))
    {
        do
        {
            rte_pktmbuf_free(mbufs[nb_tx]);
        } while (++nb_tx < count);
    }

    uint64_t start = rte_rdtsc(), gap = PROBE_TIMEOUT * rte_get_tsc_hz() / 1000;
    while (rte_rdtsc() - start < gap)
    {
        int nb_rx = rte_eth_rx_burst(port_id, 0, mbufs, PROBE_PATH_AMOUNT);
        portStats[port_id].rx += nb_rx;
        for (int i = 0; i < nb_rx; i++)
        {
            struct rte_mbuf *m = mbufs[i];
            if (RTE_ETH_IS_IPV4_HDR(m->packet_type))
            {
                struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
                if (ip->next_proto_id == 17)
                {
                    struct rte_udp_hdr *udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
                    if (udp->dst_port == rte_cpu_to_be_16(4788))
                    {
                        struct PROBE_HDR *hdr = rte_pktmbuf_mtod_offset(m, struct PROBE_HDR *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr));
                        if (hdr->FlowID == flowID)
                        {
                            uint16_t res = udp->src_port;
                            if (this_udp_h->src_port != udp->src_port)
                                DOCA_LOG_INFO("FlowTD[%lu]:%d==>%d", flowID, rte_be_to_cpu_16(this_udp_h->src_port), rte_be_to_cpu_16(udp->src_port));
                            rte_pktmbuf_free(m);
                            return res;
                        }
                    }
                }
            }
            rte_pktmbuf_free(m);
        }
    }
    DOCA_LOG_ERR("Probe Timeout: Not Find Best Path for Not Received Probe Packets");
    doca_ar_print_match(match);
    return match->sport;
}

/**
 * @brief logic of processing control plane packets
 *
 * @param args
 * @return int
 */
int process_packets(void *args)
{
    int nb_rx = 0, nb_tx = 0;
    int ingress_port = to_host_port, egress_port = to_net_port, queue_index = 0;
    struct rte_mbuf *packets[PACKET_BURST];

    struct rte_mempool *pool = rte_mempool_lookup("MBUF_POOL");
    if (pool == NULL)
    {
        DOCA_LOG_ERR("Cannot find packet mempool ERR");
        return 0;
    }
    else
        DOCA_LOG_INFO("Find out packet mempool success and start DOCA_AR on core %d", rte_lcore_id());
    while (!force_quit)
    {
        /***********Ingress process**********************/
        nb_rx = rte_eth_rx_burst(ingress_port, queue_index, packets, PACKET_BURST);
        portStats[ingress_port].rx += nb_rx;
        for (int i = 0; i < nb_rx; i++)
        {
            struct doca_ar_conn_match match = {0};
            if (doca_ar_parse_conn(&match, packets[i]))
            {
                // doca_ar_print_match(&match);
                struct doca_ar_conn *thisConn = doca_ar_find_conn(&match);
                if (thisConn == NULL)
                {
                    uint16_t bestPath = lb_scheme == DOCA_AR ? AR(pool, &match, packets[i]) : match.sport;
                    thisConn = doca_ar_add_conn(&match, bestPath);
                    if (thisConn)
                    {
                        // DOCA_LOG_INFO("New Conn to best Path %d", rte_be_to_cpu_16(bestPath));
                    }
                    else
                    {
                        DOCA_LOG_ERR("Add conn fail");
                        continue;
                    }
                }
                else
                {
                    // DOCA_LOG_INFO("Old Conn to best Path %d", rte_be_to_cpu_16(thisConn->bestPath));
                }

                if (thisConn->entry == NULL)
                {
                    thisConn->expireTime = EXPIRE_TIME;
                    thisConn->expireCallback = doca_ar_del_conn;
                    thisConn->expireCallbackArgs = (void *)thisConn;
                    doca_ar_add_new_flow(thisConn);
                }

                doca_ar_modify_conn(thisConn, packets[i]);
            }
            else
            {
                DOCA_LOG_ERR("Recv Non-VXLAN Packtes ERR");
            }
        }

        /***********Egress process*********************/
        nb_tx = rte_eth_tx_burst(egress_port, queue_index, packets, nb_rx);
        portStats[egress_port].tx += nb_tx;
        if (unlikely(nb_tx < nb_rx))
        {
            do
            {
                rte_pktmbuf_free(packets[nb_tx]);
            } while (++nb_tx < nb_rx);
        }
        doca_ar_flow_aging();
        /*************Left Probe pkts Process******************/
        // this step cannot be ignored cause large accumulation of probed packets will easily cause timeout of probing
        nb_rx = rte_eth_rx_burst(egress_port, queue_index, packets, PACKET_BURST);
        portStats[egress_port].rx += nb_rx;
        for (int i = 0; i < nb_rx; i++)
        {
            rte_pktmbuf_free(packets[i]);
        }
    }
    DOCA_LOG_INFO("lcore %d quit from packet processing", rte_lcore_id());
    return 0;
}

/********************************dpdk cmdline***************************************/
struct cmd_simple_result
{
    cmdline_fixed_string_t simple;
};
static void cmd_simple_parsed(__rte_unused void *parsed_result,
                              struct cmdline *cl,
                              __rte_unused void *data)
{
    struct cmd_simple_result *res = parsed_result;
    if (strcmp(res->simple, "quit") == 0)
    {
        force_quit = true;
        rte_eal_wait_lcore(runing_lore_id);
        cmdline_printf(cl, "Quit from the app......\n");
        cmdline_quit(cl);
    }
    if (strcmp(res->simple, "dumpFDB") == 0)
    {
        for (int i = 0; i < NB_PORTS; i++)
            doca_flow_port_pipes_dump(ports[i], stdout);
    }
    if (strcmp(res->simple, "portStats") == 0)
    {
        printPortStats(cl);
    }
    if (strcmp(res->simple, "conntrack") == 0)
    {
        doca_ar_dump_conn(cl);
    }
}
cmdline_parse_token_string_t cmd_simple =
    TOKEN_STRING_INITIALIZER(struct cmd_simple_result, simple, "quit#dumpFDB#portStats#conntrack");
cmdline_parse_inst_t simple_cmdline = {
    .f = cmd_simple_parsed, /* function to call */
    .data = NULL,           /* 2nd arg of func */
    .help_str = "quit/dumpFDB/portStats/conntrack",
    .tokens = {
        /* token list, NULL terminated */
        (void *)&cmd_simple,
        NULL,
    },
};

cmdline_parse_ctx_t main_ctx[] = {
    &simple_cmdline,
    NULL};
/**************************************************************************/

/**
 * @brief enter into dpdk cmdline
 *
 */
void doca_ar_cmd()
{
    struct cmdline *cl = cmdline_stdin_new(main_ctx, "DOCA-AR-ENV@localhost:~$ ");
    if (cl == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create cmdline instance\n");
    cmdline_interact(cl);
    cmdline_stdin_exit(cl);
}

void doca_ar()
{
    if (rte_lcore_count() <= 1)
    {
        DOCA_LOG_ERR("Not Enough Core ERR ( should >=2 )");
        return;
    }
    if (rte_lcore_count() == 2)
    {
        lb_scheme = DOCA_AR;
        DOCA_LOG_INFO("Running DOCA-AR Load Balancing Scheme");
    }
    else
    {
        DOCA_LOG_INFO("Running ECMP Load Balancing Scheme");
    }
    if (doca_ar_conntrack_init_env(MAX_CONNTRACK))
    {
        return;
    }
    RTE_LCORE_FOREACH_WORKER(runing_lore_id)
    {
        break;
    }
    rte_eal_remote_launch(process_packets, NULL, runing_lore_id);
    rte_delay_ms(200);
    doca_ar_cmd();
}
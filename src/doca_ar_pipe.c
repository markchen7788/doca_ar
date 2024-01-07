/**
 * @file doca_ar_pipe.c
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief build needed doca-flow pipe
 * @version 1.0
 * @date 2024-01-07
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "doca_ar_pipe.h"
DOCA_LOG_REGISTER(DOCA_AR_PIPE);

struct doca_flow_pipe *upstream_vxlanPipe = NULL;     ///< critical doca-flow pipe used to fwd vxlan connection from host and routing them onto the best path
struct doca_flow_pipe *upstream_rssPipe = NULL;       ///< fwd the new flow from host onto the control plane (ARM)
struct doca_flow_pipe *downstream_rssPipe = NULL;     ///< fwd the probe packets from network onto the control plane
struct doca_flow_pipe *downstream_hairpinPipe = NULL; ///< fwd other traffic from network to host

/**
 * @brief build critical doca-flow pipe used to fwd vxlan connection from host and routing them onto the best path
 *
 * @return int
 */
int build_upstream_vxlanPipe()
{
    struct doca_flow_match match;
    struct doca_flow_monitor monitor;
    struct doca_flow_actions actions, *actions_arr[1];
    struct doca_flow_action_descs descs;
    struct doca_flow_action_descs *descs_arr[1];
    struct doca_flow_fwd fwd, miss_fwd;
    struct doca_flow_pipe_cfg pipe_cfg = {0};
    struct doca_flow_error error;
    int port_id = to_host_port;
    struct doca_flow_port *port = ports[port_id];

    memset(&monitor, 0, sizeof(monitor));
    memset(&match, 0, sizeof(match));
    memset(&actions, 0, sizeof(actions));
    memset(&fwd, 0, sizeof(fwd));
    memset(&miss_fwd, 0, sizeof(miss_fwd));
    memset(&pipe_cfg, 0, sizeof(pipe_cfg));
    memset(&descs, 0, sizeof(descs));

    pipe_cfg.attr.name = "upstream_vxlanPipe";
    pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;
    pipe_cfg.match = &match;
    actions_arr[0] = &actions;
    pipe_cfg.actions = actions_arr;
    descs_arr[0] = &descs;
    pipe_cfg.action_descs = descs_arr;
    pipe_cfg.attr.nb_actions = 1;
    pipe_cfg.attr.is_root = true;
    pipe_cfg.port = port;
    pipe_cfg.monitor = &monitor;
    pipe_cfg.attr.nb_flows = MAX_CONNTRACK;

    monitor.flags = DOCA_FLOW_MONITOR_AGING;

    // 5-tuple match (sip,dip,udp,sport,dport)
    match.out_l4_type = DOCA_PROTO_UDP;
    match.out_src_ip.type = DOCA_FLOW_IP4_ADDR;
    match.out_src_ip.ipv4_addr = 0xffffffff;
    match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
    match.out_dst_ip.ipv4_addr = 0xffffffff;
    match.out_src_port = 0xffff;
    match.out_dst_port = 0xffff;

    fwd.type = DOCA_FLOW_FWD_PORT;
    fwd.port_id = port_id ^ 1;

    miss_fwd.type = DOCA_FLOW_FWD_PIPE;
    miss_fwd.next_pipe = upstream_rssPipe;

    // only modify sport for Adaptive Routing
    actions.mod_src_port = 0xffff;

    upstream_vxlanPipe = doca_flow_pipe_create(&pipe_cfg, &fwd, &miss_fwd, &error);
    if (upstream_vxlanPipe == NULL)
    {
        DOCA_LOG_ERR("build_upstream_vxlanPipe ERR,  - %s (%u)", error.message, error.type);
        return -1;
    }
    DOCA_LOG_INFO("build_upstream_vxlanPipe success");
    return 0;
}
/**
 * @brief  fwd the new flow from host onto the control plane (ARM)
 *
 * @return int
 */
int build_upstream_rssPipe()
{
    struct doca_flow_match match, entryMatch;
    struct doca_flow_fwd fwd, miss_fwd;
    struct doca_flow_pipe_cfg pipe_cfg = {0};
    struct doca_flow_error error;
    struct doca_flow_pipe_entry *entry;
    int port_id = to_host_port, num_of_entries = 1;
    struct doca_flow_port *port = ports[port_id];

    memset(&match, 0, sizeof(match));
    memset(&entryMatch, 0, sizeof(entryMatch));
    memset(&fwd, 0, sizeof(fwd));
    memset(&miss_fwd, 0, sizeof(miss_fwd));
    memset(&pipe_cfg, 0, sizeof(pipe_cfg));

    pipe_cfg.attr.name = "upstream_rssPipe";
    pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;
    pipe_cfg.match = &match;
    pipe_cfg.port = port;

    match.out_l4_type = DOCA_PROTO_UDP;
    match.out_src_ip.type = DOCA_FLOW_IP4_ADDR;
    match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
    match.out_dst_port = 0xffff;

    uint16_t rss_queues[1] = {0};
    fwd.type = DOCA_FLOW_FWD_RSS;
    fwd.rss_queues = rss_queues;
    fwd.rss_flags = DOCA_FLOW_RSS_UDP;
    fwd.num_of_queues = 1;

    miss_fwd.type = DOCA_FLOW_FWD_DROP;

    upstream_rssPipe = doca_flow_pipe_create(&pipe_cfg, &fwd, &miss_fwd, &error);
    if (upstream_rssPipe == NULL)
    {
        DOCA_LOG_ERR("build_upstream_rssPipe ERR,  - %s (%u)", error.message, error.type);
        return -1;
    }
    DOCA_LOG_INFO("build_upstream_rssPipe success");

    entryMatch.out_dst_port = rte_cpu_to_be_16(4789);

    entry = doca_flow_pipe_add_entry(0, upstream_rssPipe, &entryMatch, NULL, NULL, NULL, 0, NULL, &error);
    if (entry == NULL)
    {
        DOCA_LOG_ERR("Entry is NULL - %s (%u)", error.message, error.type);
        return -1;
    }
    int result = doca_flow_entries_process(port, 0, DEFAULT_TIMEOUT_US, num_of_entries);
    if (result != num_of_entries || doca_flow_pipe_entry_get_status(entry) != DOCA_FLOW_ENTRY_STATUS_SUCCESS)
    {
        DOCA_LOG_ERR("add entry into upstream_rssPipe ERR,  - %s (%u)", error.message, error.type);
        return -1;
    }
    DOCA_LOG_INFO("add entry into upstream_rssPipe success");
    return 0;
}
/**
 * @brief fwd the probe packets from network onto the control plane
 *
 * @return int
 */
int build_downstream_rssPipe()
{
    struct doca_flow_match match, entryMatch;
    struct doca_flow_fwd fwd, miss_fwd;
    struct doca_flow_pipe_cfg pipe_cfg = {0};
    struct doca_flow_error error;
    struct doca_flow_pipe_entry *entry;
    int port_id = to_net_port, num_of_entries = 1;
    struct doca_flow_port *port = ports[port_id];

    memset(&match, 0, sizeof(match));
    memset(&fwd, 0, sizeof(fwd));
    memset(&miss_fwd, 0, sizeof(miss_fwd));
    memset(&pipe_cfg, 0, sizeof(pipe_cfg));

    pipe_cfg.attr.name = "downstream_rssPipe";
    pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;
    pipe_cfg.match = &match;
    pipe_cfg.attr.is_root = true;
    pipe_cfg.port = port;

    match.out_l4_type = DOCA_PROTO_UDP;
    match.out_src_ip.type = DOCA_FLOW_IP4_ADDR;
    match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
    match.out_dst_port = 0xffff;

    uint16_t rss_queues[1] = {0};
    fwd.type = DOCA_FLOW_FWD_RSS;
    fwd.rss_queues = rss_queues;
    fwd.rss_flags = DOCA_FLOW_RSS_UDP;
    fwd.num_of_queues = 1;

    miss_fwd.type = DOCA_FLOW_FWD_PIPE;
    miss_fwd.next_pipe = downstream_hairpinPipe;

    downstream_rssPipe = doca_flow_pipe_create(&pipe_cfg, &fwd, &miss_fwd, &error);
    if (downstream_rssPipe == NULL)
    {
        DOCA_LOG_ERR("build_downstream_rssPipe ERR,  - %s (%u)", error.message, error.type);
        return -1;
    }
    DOCA_LOG_INFO("build_downstream_rssPipe success");

    entryMatch.out_dst_port = rte_cpu_to_be_16(4788);

    entry = doca_flow_pipe_add_entry(0, downstream_rssPipe, &entryMatch, NULL, NULL, NULL, 0, NULL, &error);
    if (entry == NULL)
    {
        DOCA_LOG_ERR("Entry is NULL - %s (%u)", error.message, error.type);
        return -1;
    }
    int result = doca_flow_entries_process(port, 0, DEFAULT_TIMEOUT_US, num_of_entries);
    if (result != num_of_entries || doca_flow_pipe_entry_get_status(entry) != DOCA_FLOW_ENTRY_STATUS_SUCCESS)
    {
        DOCA_LOG_ERR("add entry into downstream_rssPipe ERR,  - %s (%u)", error.message, error.type);
        return -1;
    }
    DOCA_LOG_INFO("add entry into downstream_rssPipe success");
    return 0;
}
/**
 * @brief  fwd other traffic from network to host
 *
 * @return int
 */
int build_downstream_hairpinPipe()
{
    struct doca_flow_match match;
    struct doca_flow_fwd fwd;
    struct doca_flow_fwd fwd_miss;
    struct doca_flow_pipe_cfg pipe_cfg = {0};
    struct doca_flow_pipe_entry *entry;
    int port_id = to_net_port, num_of_entries = 1, result = 0;
    struct doca_flow_port *port = ports[port_id];
    struct doca_flow_error error;

    memset(&match, 0, sizeof(match));
    memset(&fwd, 0, sizeof(fwd));
    memset(&fwd_miss, 0, sizeof(fwd_miss));
    memset(&pipe_cfg, 0, sizeof(pipe_cfg));

    pipe_cfg.attr.name = "downstream_hairpinPipe";
    pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;
    pipe_cfg.match = &match;

    pipe_cfg.port = port;

    fwd.type = DOCA_FLOW_FWD_PORT;
    fwd.port_id = port_id ^ 1;
    fwd_miss.type = DOCA_FLOW_FWD_DROP;

    downstream_hairpinPipe = doca_flow_pipe_create(&pipe_cfg, &fwd, &fwd_miss, &error);
    if (downstream_hairpinPipe == NULL)
    {
        DOCA_LOG_ERR("build_downstream_hairpinPipe ERR,  - %s (%u)", error.message, error.type);
        return -1;
    }
    DOCA_LOG_INFO("build_downstream_hairpinPipe success");

    entry = doca_flow_pipe_add_entry(0, downstream_hairpinPipe, &match, NULL, NULL, NULL, 0, NULL, &error);
    if (entry == NULL)
    {
        DOCA_LOG_ERR("Entry is NULL - %s (%u)", error.message, error.type);
        return -1;
    }
    result = doca_flow_entries_process(port, 0, DEFAULT_TIMEOUT_US, num_of_entries);
    if (result != num_of_entries || doca_flow_pipe_entry_get_status(entry) != DOCA_FLOW_ENTRY_STATUS_SUCCESS)
    {
        DOCA_LOG_ERR("add entry into downstream_hairpinPipe ERR,  - %s (%u)", error.message, error.type);
        return -1;
    }
    DOCA_LOG_INFO("add entry into downstream_hairpinPipe success");

    return 0;
}
int doca_ar_pipe_init()
{
    if (build_upstream_rssPipe())
        return -1;
    if (build_upstream_vxlanPipe())
        return -1;
    if (build_downstream_hairpinPipe())
        return -1;
    if (build_downstream_rssPipe())
        return -1;
    return 0;
}
int doca_ar_add_new_flow(struct doca_ar_conn *conn)
{
    struct doca_flow_match match;
    struct doca_flow_actions actions;
    struct doca_flow_pipe_entry *entry;
    struct doca_flow_error error;
    struct doca_flow_monitor monitor;

    int result;
    int num_of_entries = 1;

    memset(&match, 0, sizeof(match));
    memset(&actions, 0, sizeof(actions));
    memset(&monitor, 0, sizeof(monitor));

    monitor.flags |= DOCA_FLOW_MONITOR_AGING;
    monitor.user_data = (uint64_t)(conn);
    monitor.aging = conn->expireTime;

    match.out_src_ip.ipv4_addr = conn->match.sip;
    match.out_dst_ip.ipv4_addr = conn->match.dip;
    match.out_src_port = conn->match.sport;
    match.out_dst_port = conn->match.dport;

    actions.action_idx = 0;
    /* modify destination mac address */
    actions.mod_src_port = conn->bestPath;

    entry = doca_flow_pipe_add_entry(0, upstream_vxlanPipe, &match, &actions, &monitor, NULL, 0, NULL, &error);
    if (entry == NULL)
    {
        DOCA_LOG_ERR("Entry is NULL - %s (%u)", error.message, error.type);
        return 0;
    }
    result = doca_flow_entries_process(ports[to_host_port], 0, DEFAULT_TIMEOUT_US, num_of_entries);
    if (result != num_of_entries || doca_flow_pipe_entry_get_status(entry) != DOCA_FLOW_ENTRY_STATUS_SUCCESS)
    {
        conn->entry = NULL;
        return 0;
    }
    conn->entry = entry;
    return 1;
}
int doca_ar_flow_aging()
{
    struct doca_flow_aged_query aged_entries[MAX_AGED_CT_PER_POLL];
    int num_of_aged_entries = doca_flow_aging_handle(ports[to_host_port], 0, 20 /*us*/,
                                                     aged_entries, MAX_AGED_CT_PER_POLL);
    /* call handle aging until full cycle complete */
    for (int i = 0; i < num_of_aged_entries; i++)
    {
        struct doca_ar_conn *conn = (struct doca_ar_conn *)aged_entries[i].user_data;
        if (doca_flow_pipe_rm_entry(0, NULL, conn->entry) < 0)
        {
            DOCA_LOG_INFO("failed to remove aged entry");
            continue;
        }
        else if (conn->expireCallback)
        {
            conn->expireCallback(conn->expireCallbackArgs); // Revoke user_defined callback
        }
    }
    return num_of_aged_entries > 0 ? num_of_aged_entries : 0;
}
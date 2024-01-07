/**
 * @file doca_ar_env.c
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief build base enviroment of DPDK and Doca-Flow
 * @version 1.0
 * @date 2024-01-07
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "doca_ar_env.h"

DOCA_LOG_REGISTER(DOCA_AR_ENV);

struct doca_flow_port *ports[NB_PORTS];
struct application_dpdk_config dpdk_config = {
	.port_config.nb_ports = 2,
	.port_config.nb_queues = 1,
	.port_config.nb_hairpin_q = 1,
	.sft_config = {0},
};

int to_host_port = 0;
int to_net_port = 1;

int init_doca_flow(int nb_queues, const char *mode, struct doca_flow_resources resource, uint32_t nr_shared_resources[], struct doca_flow_error *error)
{
	struct doca_flow_cfg flow_cfg;
	int shared_resource_idx;

	memset(&flow_cfg, 0, sizeof(flow_cfg));

	flow_cfg.queues = nb_queues;
	flow_cfg.mode_args = mode;
	flow_cfg.resource = resource;
	for (shared_resource_idx = 0; shared_resource_idx < DOCA_FLOW_SHARED_RESOURCE_MAX; shared_resource_idx++)
		flow_cfg.nr_shared_resources[shared_resource_idx] = nr_shared_resources[shared_resource_idx];
	return doca_flow_init(&flow_cfg, error);
}

/*
 * Create DOCA Flow port by port id
 *
 * @port_id [in]: port ID
 * @error [out]: output error
 * @return: port handler on success, NULL otherwise and error is set.
 */
static struct doca_flow_port *
create_doca_flow_port(int port_id, struct doca_flow_error *error)
{
	int max_port_str_len = 128;
	struct doca_flow_port_cfg port_cfg;
	char port_id_str[max_port_str_len];

	memset(&port_cfg, 0, sizeof(port_cfg));

	port_cfg.port_id = port_id;
	port_cfg.type = DOCA_FLOW_PORT_DPDK_BY_ID;
	snprintf(port_id_str, max_port_str_len, "%d", port_cfg.port_id);
	port_cfg.devargs = port_id_str;
	return doca_flow_port_start(&port_cfg, error);
}
/**
 * @brief destroy_doca_flow_ports function copied from doca-flow samples common directory
 *
 * @param nb_ports
 * @param ports
 */
void destroy_doca_flow_ports(int nb_ports, struct doca_flow_port *ports[])
{
	int portid;

	for (portid = 0; portid < nb_ports; portid++)
	{
		if (ports[portid] != NULL)
			doca_flow_port_destroy(ports[portid]);
	}
}
/**
 * @brief init_doca_flow_ports function copied from doca-flow samples common directory
 *
 * @param nb_ports
 * @param ports
 */
int init_doca_flow_ports(int nb_ports, struct doca_flow_port *ports[], bool is_hairpin)
{
	int portid, ret;
	struct doca_flow_error error;

	for (portid = 0; portid < nb_ports; portid++)
	{
		/* Create doca flow port */
		ports[portid] = create_doca_flow_port(portid, &error);
		if (ports[portid] == NULL)
		{
			DOCA_LOG_ERR("Failed to start port - %s (%u)", error.message, error.type);
			destroy_doca_flow_ports(portid + 1, ports);
			return -1;
		}
		/* Pair ports should be done in the following order: port0 with port1, port2 with port3 etc. */
		if (!is_hairpin || !portid || !(portid % 2))
			continue;
		/* pair odd port with previous port */
		ret = doca_flow_port_pair(ports[portid], ports[portid ^ 1]);
		if (ret < 0)
		{
			DOCA_LOG_ERR("Failed to pair ports %u - %u", portid, portid ^ 1);
			destroy_doca_flow_ports(portid + 1, ports);
			return -1;
		}
	}
	return 0;
}

int doca_ar_env_init(int argc, char **argv)
{
	doca_error_t result;
	struct doca_flow_resources resource = {0};
	uint32_t nr_shared_resources[DOCA_FLOW_SHARED_RESOURCE_MAX] = {0};
	struct doca_flow_error error;
	resource.nb_counters = 80;

	//////////////////////////////////////////////////////////////// Args Process
	result = doca_argp_init("FlowQoS", NULL);
	if (result != DOCA_SUCCESS)
	{
		DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}
	doca_argp_set_dpdk_program(dpdk_init);
	result = doca_argp_start(argc, argv);
	if (result != DOCA_SUCCESS)
	{
		DOCA_LOG_ERR("Failed to parse sample input: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	//////////////////////////////////////////////////////////////// DPDK Port Init
	/* update queues and ports */
	result = dpdk_queues_and_ports_init(&dpdk_config);
	if (result != DOCA_SUCCESS)
	{
		DOCA_LOG_ERR("Failed to update ports and queues");
		dpdk_fini(&dpdk_config);
		doca_argp_destroy();
		return EXIT_FAILURE;
	}
	DOCA_LOG_INFO("QueueNUM %d", dpdk_config.port_config.nb_queues);
	//////////////////////////////////////////////////////////////// DOCA Port Init

	/*hws mode has a conflict with adding entries into multiFlowQueues*/
	if (init_doca_flow(dpdk_config.port_config.nb_queues, "vnf", resource, nr_shared_resources, &error) < 0)
	{
		DOCA_LOG_ERR("Failed to init DOCA Flow - %s (%u)", error.message, error.type);
		return EXIT_FAILURE;
	}

	if (init_doca_flow_ports(NB_PORTS, ports, true))
	{
		DOCA_LOG_ERR("Failed to init DOCA ports");
		doca_flow_destroy();
		return EXIT_FAILURE;
	}
	DOCA_LOG_INFO("Init DOCA_AR_ENV Success");

	return DOCA_SUCCESS;
}
void doca_ar_env_destroy()
{
	destroy_doca_flow_ports(NB_PORTS, ports);
	doca_flow_destroy();

	/* cleanup resources */
	dpdk_queues_and_ports_fini(&dpdk_config);
	dpdk_fini();

	/* ARGP cleanup */
	doca_argp_destroy();
}

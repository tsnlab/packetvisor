#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "internal_config.h"
#include "internal_nic.h"	// for offload type list
#include <libfyaml.h>

static const char* DEFAULT_CONFIG_PATH = "./.tmp_config.yaml";

struct pv_offload_type rx_off_types[19] = {
	{"vlan_strip", DEV_RX_OFFLOAD_VLAN_STRIP},
	{"ipv4_cksum", DEV_RX_OFFLOAD_IPV4_CKSUM},
	{"udp_cksum", DEV_RX_OFFLOAD_UDP_CKSUM},
	{"tcp_cksum", DEV_RX_OFFLOAD_TCP_CKSUM},
	{"tcp_lro", DEV_RX_OFFLOAD_TCP_LRO},
	{"qinq_strip", DEV_RX_OFFLOAD_QINQ_STRIP},
	{"outer_ipv4_cksum", DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM},
	{"macsec_strip", DEV_RX_OFFLOAD_MACSEC_STRIP},
	{"header_split", DEV_RX_OFFLOAD_HEADER_SPLIT},
	{"vlan_filter", DEV_RX_OFFLOAD_VLAN_FILTER},
	{"vlan_extend", DEV_RX_OFFLOAD_VLAN_EXTEND},
	{"jumbo_frame", DEV_RX_OFFLOAD_JUMBO_FRAME},
	{"scatter", DEV_RX_OFFLOAD_SCATTER},
	{"timestamp", DEV_RX_OFFLOAD_TIMESTAMP},
	{"security", DEV_RX_OFFLOAD_SECURITY},
	{"keep_crc", DEV_RX_OFFLOAD_KEEP_CRC},
	{"sctp_cksum", DEV_RX_OFFLOAD_SCTP_CKSUM},
	{"outer_udp_cksum", DEV_RX_OFFLOAD_OUTER_UDP_CKSUM},
	{"rss_hash", DEV_RX_OFFLOAD_RSS_HASH}
};

struct pv_offload_type tx_off_types[22] = {
	{"vlan_insert", DEV_TX_OFFLOAD_VLAN_INSERT},
	{"ipv4_cksum", DEV_TX_OFFLOAD_IPV4_CKSUM},
	{"udp_cksum", DEV_TX_OFFLOAD_UDP_CKSUM},
	{"tcp_cksum", DEV_TX_OFFLOAD_TCP_CKSUM},
	{"sctp_cksum", DEV_TX_OFFLOAD_SCTP_CKSUM},
	{"tcp_tso", DEV_TX_OFFLOAD_TCP_TSO},
	{"udp_tso", DEV_TX_OFFLOAD_UDP_TSO},
	{"outer_ipv4_cksum", DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM},
	{"qinq_insert", DEV_TX_OFFLOAD_QINQ_INSERT},
	{"vxlan_tnl_tso", DEV_TX_OFFLOAD_VXLAN_TNL_TSO},
	{"gre_tnl_tso", DEV_TX_OFFLOAD_GRE_TNL_TSO},
	{"ipip_tnl_tso", DEV_TX_OFFLOAD_IPIP_TNL_TSO},
	{"geneve_tnl_tso", DEV_TX_OFFLOAD_GENEVE_TNL_TSO},
	{"macsec_insert", DEV_TX_OFFLOAD_MACSEC_INSERT},
	{"mt_lockfree", DEV_TX_OFFLOAD_MT_LOCKFREE},
	{"multi_segs", DEV_TX_OFFLOAD_MULTI_SEGS},
	{"mbuf_fast_free", DEV_TX_OFFLOAD_MBUF_FAST_FREE},
	{"security", DEV_TX_OFFLOAD_SECURITY},
	{"udp_tnl_tso", DEV_TX_OFFLOAD_UDP_TNL_TSO},
	{"ip_tnl_tso", DEV_TX_OFFLOAD_IP_TNL_TSO},
	{"outer_udp_cksum", DEV_TX_OFFLOAD_OUTER_UDP_CKSUM},
	{"send_on_timestamp", DEV_TX_OFFLOAD_SEND_ON_TIMESTAMP}
};

static bool pv_config_parse_nic(struct fy_node* nic_node, struct pv_config_nic* nic, int index/* only for err msg */) {
	char buf[1025] = {};

	// get dev name
	if(fy_node_scanf(nic_node, "dev %1024s", buf) <= 0) {
		printf("Failed to parse '/nic[%d]/dev'\n", index);
		return false;
	}

	nic->dev = strdup(buf);
	if(nic->dev == NULL) {
		printf("Failed to duplicate data for '/nic[%d]/dev'\n", index);
		return false;
	}

	// get mac addr
	if(fy_node_scanf(nic_node, "mac %1024s", buf) <= 0 || strlen(buf) < 17) {
		printf("Invalid key or value of '/nic[%d]/mac'. Use default value.\n", index);
		nic->is_mac_valid = false;
	} else {
		nic->is_mac_valid = true;
		nic->mac = 0;
		nic->mac |= ((uint64_t)strtoul(buf, NULL, 16) << 40);
		nic->mac |= ((uint64_t)strtoul(buf + 3, NULL, 16) << 32);
		nic->mac |= ((uint64_t)strtoul(buf + 6, NULL, 16) << 24);
		nic->mac |= ((uint64_t)strtoul(buf + 9, NULL, 16) << 16);
		nic->mac |= ((uint64_t)strtoul(buf + 12, NULL, 16) << 8);
		nic->mac |= (uint64_t)strtoul(buf + 15, NULL, 16);
	}

	// get ipv4 addr
	if(fy_node_scanf(nic_node, "ipv4 %1024s", buf) <= 0) {
		printf("Invalid key of '/nic[%d]/ipv4'. Use default value.\n", index);
		nic->is_ipv4_valid = false;
	} else {
		nic->is_ipv4_valid = true;
		nic->ipv4 = inet_network(buf);	// set ipv4 addr as host endian
	}


	// TODO ipv6
	nic->is_ipv6_valid = false;

	// get rx_queue
	if(fy_node_scanf(nic_node, "rx_queue %u", &nic->rx_queue) <= 0) {
		printf("Failed to parse '/nic[%d]/rx_queue'\n", index);
		return false;
	}

	// get tx_queue
	if(fy_node_scanf(nic_node, "tx_queue %u", &nic->tx_queue) <= 0) {
		printf("Failed to parse '/nic[%d]/tx_queue'\n", index);
		return false;
	}

	// get promisc
	if(fy_node_scanf(nic_node, "promisc %s", buf) <= 0) {
		printf("Invalid key or value of '/nic[%d]/promisc'. Use default value.\n", index);
		nic->promisc = false;
	} else {
		if(strncmp("true", buf, strlen(buf)) == 0) {
			nic->promisc = true;
		} if(strncmp("false", buf, strlen(buf)) == 0) {
			nic->promisc = false; 
		} else {
			printf("Invalid key or value of '/nic[%d]/promisc'. Use default value.\n", index);
			nic->promisc = false;
		}
	}

	// get rx_offload
	struct fy_node* rx_off_node = fy_node_by_path(nic_node, "rx_offloads", strlen("rx_offloads"), FYNWF_DONT_FOLLOW);
	if(rx_off_node == NULL || fy_node_sequence_item_count(rx_off_node) == -1) {
		printf("Invalid key or value of '/nic[%d]/rx_offloads'. Disable rx offload.\n", index);
	} else {
		void* iter = NULL;
		struct fy_node* item = NULL;
		while((item = fy_node_sequence_iterate(rx_off_node, &iter)) != NULL) {
			const char* off_str = fy_node_get_scalar0(item);

			for(int i = 0; i < sizeof(rx_off_types) / sizeof(rx_off_types[0]); i++) {
				if(strcmp(off_str, rx_off_types[i].name) == 0) {
					nic->rx_offloads |= rx_off_types[i].mask;
					break;
				}

				if(i == sizeof(rx_off_types) / sizeof(rx_off_types[0]) - 1)
					printf("Unsupported nic[%d]/rx_offload '%s'.\n", index, off_str);
			}
		}
	}


	// get tx_offload
	struct fy_node* tx_off_node = fy_node_by_path(nic_node, "tx_offloads", strlen("tx_offloads"), FYNWF_DONT_FOLLOW);
	if(tx_off_node == NULL || fy_node_sequence_item_count(tx_off_node) == -1) {
		printf("Invalid key or value of '/nic[%d]/tx_offloads'. Disable tx offload.\n", index);
	} else {
		void* iter = NULL;
		struct fy_node* item = NULL;
		while((item = fy_node_sequence_iterate(tx_off_node, &iter)) != NULL) {
			const char* off_str = fy_node_get_scalar0(item);

			for(int i = 0; i < sizeof(tx_off_types) / sizeof(tx_off_types[0]); i++) {
				if(strcmp(off_str, tx_off_types[i].name) == 0) {
					nic->tx_offloads |= tx_off_types[i].mask;
					break;
				}

				if(i == sizeof(tx_off_types) / sizeof(tx_off_types[0]) - 1)
					printf("Unsupported nic[%d]/tx_offload '%s'.\n", index, off_str);
			}
		}

	}

	return true;
}

struct pv_config* pv_config_create() {
	struct pv_config* config = NULL;

	struct fy_document* fyd = fy_document_build_from_file(NULL, DEFAULT_CONFIG_PATH);
	if(fyd == NULL) {
		printf("Failed to read config file '%s'\n", DEFAULT_CONFIG_PATH);
		return NULL;
	}

	config = calloc(1, sizeof(struct pv_config));
	if(config == NULL) {
		printf("Failed to alloc memory for config\n");
		return NULL;
	}

	// get cores
	struct fy_node* cores_node = fy_node_by_path(fy_document_root(fyd), "/cores", strlen("/cores"), FYNWF_DONT_FOLLOW);
	if(cores_node == NULL) {
		printf("Failed to parse '/cores'\n");
		goto fail;
	}

	config->cores_count = fy_node_sequence_item_count(cores_node);
	if(config->cores_count < 1) {
		printf("Invalid cores count: %d\n", config->cores_count);
		goto fail;
	}

	config->cores = calloc(config->cores_count, sizeof(uint16_t));
	if(config->cores == NULL) {
		printf("Failed to alloc memory for 'config->cores'\n");
		goto fail;
	}

	void* iter = NULL;
	for(int i = 0; i < config->cores_count; i++) {
		struct fy_node* item = fy_node_sequence_iterate(cores_node, &iter);
		config->cores[i] = atoi(fy_node_get_scalar0(item));
	}

	// get memory
	if(fy_document_scanf(fyd, "/memory/packet_pool %u", &config->memory.packet_pool) <= 0) {
		printf("Failed to parse '/memory/packet_pool'\n");
		goto fail;
	}

	if(fy_document_scanf(fyd, "/memory/shared_memory %u", &config->memory.shared_memory) <= 0) {
		printf("Failed to parse '/memory/shared_memory'\n");
		goto fail;
	}

	// get log_level
	char buf[1025] = {0, };
	if(fy_document_scanf(fyd, "/log_level %1024s", buf) <= 0) {
		printf("Failed to parse '/log_level'\n");
		goto fail;
	}

	config->log_level = strdup(buf);
	if(config->log_level == NULL) {
		printf("Failed to duplicate data for 'config->log_level'\n");
		goto fail;
	}

	// get nics
	struct fy_node* nics_node = fy_node_by_path(fy_document_root(fyd), "/nics", strlen("/nics"), FYNWF_DONT_FOLLOW);
	if(nics_node == NULL) {
		printf("Failed to parse 'nics'\n");
		goto fail;
	}
	
	config->nics_count = fy_node_sequence_item_count(nics_node);
	if(config->nics_count < 1) {
		printf("Invalid nics count: %d\n", config->nics_count);
		goto fail;
	}

	config->nics = calloc(config->nics_count, sizeof(struct pv_config_nic));
	if(config->nics == NULL) {
		printf("Failed to alloc memory for nics\n");
		goto fail;
	}

	iter = NULL;
	for(int i = 0; i < config->nics_count; i++) {
		struct fy_node* item = fy_node_sequence_iterate(nics_node, &iter);
		if(pv_config_parse_nic(item, &config->nics[i], i) == false)
			goto fail;
	}

	// get eal_params
	struct fy_node* eal_params_node = fy_node_by_path(fy_document_root(fyd), "/eal_params", strlen("/eal_params"), FYNWF_DONT_FOLLOW);
	if(eal_params_node == NULL) {
		printf("Failed to parse '/eal_params'\n");
		goto fail;
	}

	config->eal_params_count = fy_node_sequence_item_count(eal_params_node);
	if(config->eal_params_count < 1) {
		printf("Invalid eal_params count: %d\n", config->eal_params_count);
		goto fail;
	}

	config->eal_params = calloc(config->eal_params_count, sizeof(char*));
	if(config->eal_params == NULL) {
		printf("Failed to alloc memory for 'config->eal_params'\n");
		goto fail;
	}

	iter = NULL;
	for(int i = 0; i < config->eal_params_count; i++) {
		struct fy_node* item = fy_node_sequence_iterate(eal_params_node, &iter);
		config->eal_params[i] = strdup(fy_node_get_scalar0(item));
		if(config->eal_params[i] == NULL) {
			printf("Failed to alloc memory for 'config->eal_params[%d]: %s'\n", i, fy_node_get_scalar0(item));
			goto fail;
		}
	}

	fy_document_destroy(fyd);

	return config;

fail:
	if(fyd != NULL)
		fy_document_destroy(fyd);

	pv_config_destroy(config);

	return NULL;
}

void pv_config_destroy(struct pv_config* config) {
	if(config == NULL)
		return;

	if(config->cores != NULL)
		free(config->cores);

	if(config->log_level != NULL)
		free(config->log_level);
	
	if(config->nics != NULL)
		free(config->nics);

	if(config->eal_params != NULL) {
		for(int i = 0; i < config->eal_params_count; i++)
			free(config->eal_params[i]);
		free(config->eal_params);
	}

	free(config);
}

void pv_config_print(struct pv_config* config) {
	if(config == NULL) {
		printf("config is NULL\n");
		return;
	}

	// print cores
	printf("cores: %d", config->cores[0]);
	for(int i = 1; i < config->cores_count; i++) {
		printf(", %d", config->cores[i]);
	}
	printf("\n");

	// print memory
	printf("memory:\n  packet_pool: %u\n  shared_memory: %u\n", config->memory.packet_pool, config->memory.shared_memory);

	// print log_level
	printf("log_level: %s\n", config->log_level);

	//print nics
	printf("nics:\n");
	for(int nic_id = 0; nic_id < config->nics_count; nic_id++) {
		printf("  - dev: %s\n", config->nics[nic_id].dev);
		printf("    mac: 0x%012lx\n", config->nics[nic_id].mac);
		printf("    ipv4: 0x%08x\n", config->nics[nic_id].ipv4);
		printf("    rx_queue: %u\n", config->nics[nic_id].rx_queue);
		printf("    tx_queue: %u\n", config->nics[nic_id].tx_queue);
		if(config->nics[nic_id].promisc)
			printf("    promisc: true\n");
		else
			printf("    promisc: false\n");

		printf("    rx_offloads: [ ");
		for(int i = 0; i < sizeof(rx_off_types) / sizeof(rx_off_types[0]); i++) {
			if(config->nics[nic_id].rx_offloads & rx_off_types[i].mask)
				printf(" %s,", rx_off_types[i].name);
		}
		printf("\b  ]\n");

		printf("    tx_offloads: [ ");
		for(int i = 0; i < sizeof(tx_off_types) / sizeof(tx_off_types[0]); i++) {
			if(config->nics[nic_id].tx_offloads & tx_off_types[i].mask)
				printf(" %s,", tx_off_types[i].name);
		}
		printf("\b  ]\n");

		printf("    eal_params: [ ");
		for(int i = 0; i < config->eal_params_count; i++) {
			printf(" %s,", config->eal_params[i]);
		}
		printf("\b  ]\n");
	}
}

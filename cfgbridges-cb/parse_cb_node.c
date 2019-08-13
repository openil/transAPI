/*
 * SPDX-License-Identifier:	(GPL-2.0 OR MIT)
 *
 * Copyright 2019 NXP
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <libxml/tree.h>
#include <libnetconf_xml.h>
#include <pthread.h>
#include <semaphore.h>
#include <tsn/genl_tsn.h>
#include <linux/tsn.h>
#include "platform.h"
#include "bridges.h"
#include "xml_node_access.h"
#include "json_node_access.h"
#include "parse_cb_node.h"

void init_cb_memory(struct std_cb_conf *cb_conf)
{
	memset(cb_conf, 0, sizeof(struct std_cb_conf));
}

struct std_cb_stream_table *creat_new_stream_instance(void)
{
	struct std_cb_stream_table *stream_entry_table_ptr;
	struct std_cb_stream *stream_entry_ptr;

	nc_verb_verbose("%s is called", __func__);
	stream_entry_table_ptr = (struct std_cb_stream_table *)calloc(1, sizeof(struct std_cb_stream_table));
	if (!stream_entry_table_ptr)
		return stream_entry_table_ptr;
	stream_entry_ptr = (struct std_cb_stream *)calloc(1, sizeof(struct std_cb_stream));
	if (!stream_entry_ptr)
		free(stream_entry_table_ptr);

	stream_entry_table_ptr->stream_ptr = stream_entry_ptr;
	stream_entry_table_ptr->next = NULL;
	stream_entry_ptr->cbconf.handle = -1;
	//nc_verb_verbose("%p is calloced", stream_entry_table_ptr);
	//nc_verb_verbose("%p is calloced", stream_entry_ptr);
	return stream_entry_table_ptr;
}
void free_stream_memory(struct std_cb_stream_table *stream_table)
{
	nc_verb_verbose("%s is called", __func__);
	struct std_cb_stream_table *tmp_table = stream_table;
	struct std_cb_stream_table *next_table;

	while (tmp_table) {
		next_table = tmp_table->next;
		//nc_verb_verbose("%p is freed", tmp_table);
		//nc_verb_verbose("%p is freed", tmp_table->stream_ptr);
		if (tmp_table->stream_ptr)
			free(tmp_table->stream_ptr);
		free(tmp_table);
		tmp_table = next_table;
	}
}
void free_cb_memory(struct std_cb_conf *cb_conf)
{
	free_stream_memory(cb_conf->stream_table);
	cb_conf->stream_table = NULL;
}

int parse_mac_address(char *mac_str, uint64_t *mac,
	char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	char *temp;
	uint64_t ul = 0;
	int i = 0;
	uint64_t byte[6] = {0};

	nc_verb_verbose("%s is called", __func__);
	if (strlen(mac_str) != 17) {
		rc = EXIT_FAILURE;
		sprintf(err_msg, "length of '%s' in path '%s'should be 17!",
			mac_str, node_path);
		goto out;
	}
	temp = strtok(mac_str, "-");

	ul = strtoul(temp, NULL, 16);
	i = 0;
	byte[i++] = ul;
	while (1) {
		temp = strtok(NULL, "-");
		if (temp != NULL) {
			if (strlen(temp) != 2) {
				rc = EXIT_FAILURE;
				sprintf(err_msg,
					"'%s' in '%s' is in wrong format!",
					mac_str, node_path);
				goto out;
			}
			ul = strtoul(temp, NULL, 16);
			byte[i++] = (uint8_t)ul;
		} else {
			break;
		}
	}
	if (i != 6) {
		rc = EXIT_FAILURE;
		sprintf(err_msg, "'%s' in '%s' is in wrong format!",
			mac_str, node_path);
		goto out;
	}
	for (i = 0, ul = 0; i < 6; i++) {
		ul = (ul << 8) + byte[i];
	}
	*mac = ul;
out:
	return rc;
}

int get_ipv4_addr(char *ip_str, struct ip_addr *addr)
{
	return EXIT_FAILURE;
}

int get_ipv6_addr(char *ip_str, struct ip_addr *addr)
{
	return EXIT_FAILURE;
}

int parse_ip_address(xmlNode *node, struct ip_addr *addr,
	char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	xmlNode *tmp_node = NULL;
	char ele_val[MAX_ELEMENT_LENGTH];

	nc_verb_verbose("%s is called", __func__);
	/* get ip version */
	tmp_node = get_child_node(node, "ip-version");
	if (!tmp_node) {
		rc = EXIT_FAILURE;
		sprintf(err_msg, "can't get 'ip-version' in '%s'",
			node_path);
		goto out;
	}

	rc = xml_read_field(tmp_node, "ip-version", ele_val,
					    err_msg, node_path);
	if (rc != EXIT_SUCCESS)
		goto out;
	if (strcmp(ele_val, "ipv4") == 0) {
		addr->version = 1;
	} else if (strcmp(ele_val, "ipv6") == 0) {
		addr->version = 1;
	} else {
		rc = EXIT_FAILURE;
		sprintf(err_msg, "unknown 'ip-version' in '%s'",
			node_path);
		goto out;
	}
	/* get ip-address */
	switch (addr->version) {
	case 1:
		rc = xml_read_field(tmp_node, "ipv4-address", ele_val,
						    err_msg, node_path);
		if (rc != EXIT_SUCCESS)
			goto out;
		rc = get_ipv4_addr(ele_val, addr);
		break;
	case 2:
		rc = xml_read_field(tmp_node, "ipv6-address", ele_val,
						    err_msg, node_path);
		if (rc != EXIT_SUCCESS)
			goto out;
		rc = get_ipv6_addr(ele_val, addr);
		break;
	default:
		rc = EXIT_FAILURE;
		sprintf(err_msg, "unknown 'ip-version' in '%s'",
			node_path);
	}
out:
	return rc;
}

int parse_null_streamid_params(xmlNode *node,
	struct std_cb_stream *stream, char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	char *content;
	uint64_t tmp;
	xmlNode *tmp_node;
	char ele_val[MAX_ELEMENT_LENGTH];
	char path[MAX_PATH_LENGTH];
	char *para_name = "null-stream-identification-params";

	nc_verb_verbose("%s is called", __func__);
	/* get null-stream-identification-params node */
	tmp_node = get_child_node(node, para_name);
	if (!tmp_node) {
		sprintf(err_msg, "'%s' and identification-type doesn't match",
			para_name);
		rc = EXIT_FAILURE;
		goto out;
	}

	for (tmp_node = tmp_node->children;
	     tmp_node != NULL; tmp_node = tmp_node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;
		content = (char *)tmp_node->name;
		if (strcmp(content, "dest-address") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			strcpy(path, node_path);
			strcat(path, "/stream-gate-instance-table");
			rc = parse_mac_address(ele_val, &tmp,
					       err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;

			stream->cbconf.para.nid.dmac = tmp;
		} else if (strcmp(content, "vlan-tagged") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			if (strcmp(ele_val, "tagged") == 0) {
				stream->cbconf.para.nid.tagged = 1;
			} else if (strcmp(ele_val, "priority") == 0) {
				stream->cbconf.para.nid.tagged = 2;
			} else if (strcmp(ele_val, "all") == 0) {
				stream->cbconf.para.nid.tagged = 3;
			} else {
				sprintf(err_msg,
					"unknown '%s' in '%s'",
					content, node_path);
				rc = EXIT_FAILURE;
				goto out;
			}
		} else if (strcmp(content, "vlan-id") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.nid.vid = (uint16_t)tmp;
		}
	}
out:
	return rc;
}

int parse_smac_vlan_params(xmlNode *node,
	struct std_cb_stream *stream, char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	char *content;
	uint64_t tmp;
	xmlNode *tmp_node;
	char ele_val[MAX_ELEMENT_LENGTH];
	char path[MAX_PATH_LENGTH];
	char *para_name = "source-mac-and-vlan-identification-params";

	nc_verb_verbose("%s is called", __func__);
	/* get source-mac-and-vlan-identification-params node */
	tmp_node = get_child_node(node, para_name);
	if (!tmp_node) {
		sprintf(err_msg, "'%s' and identification-type doesn't match",
			para_name);
		rc = EXIT_FAILURE;
		goto out;
	}
	for (tmp_node = tmp_node->children;
	     tmp_node != NULL; tmp_node = tmp_node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		content = (char *)tmp_node->name;
		if (strcmp(content, "source-address") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			strcpy(path, node_path);
			strcat(path, "/source-address");
			rc = parse_mac_address(ele_val, &tmp,
					       err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;

			stream->cbconf.para.sid.smac = tmp;
		} else if (strcmp(content, "vlan-tagged") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			if (strcmp(ele_val, "tagged") == 0) {
				stream->cbconf.para.sid.tagged = 1;
			} else if (strcmp(ele_val, "priority") == 0) {
				stream->cbconf.para.sid.tagged = 2;
			} else if (strcmp(ele_val, "all") == 0) {
				stream->cbconf.para.sid.tagged = 3;
			} else {
				sprintf(err_msg,
					"unknown '%s' in '%s'",
					content, node_path);
				rc = EXIT_FAILURE;
				goto out;
			}
		} else if (strcmp(content, "vlan-id") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.sid.vid = (uint16_t)tmp;
		}
	}
out:
	return rc;
}

int parse_dmac_vlan_params(xmlNode *node,
	struct std_cb_stream *stream, char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	char *content;
	uint64_t tmp;
	xmlNode *tmp_node;
	char ele_val[MAX_ELEMENT_LENGTH];
	char path[MAX_PATH_LENGTH];
	char *para_name = "dest-mac-and-vlan-identification-params";

	nc_verb_verbose("%s is called", __func__);
	/* get null-stream-identification-params node */
	tmp_node = get_child_node(node, para_name);
	if (!tmp_node) {
		sprintf(err_msg, "'%s' and identification-type doesn't match",
			para_name);
		rc = EXIT_FAILURE;
		goto out;
	}

	for (tmp_node = tmp_node->children;
	     tmp_node != NULL; tmp_node = tmp_node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		content = (char *)tmp_node->name;
		if (strcmp(content, "down-dest-address") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			strcpy(path, node_path);
			strcat(path, "/down-dest-address");
			rc = parse_mac_address(ele_val, &tmp,
					       err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;

			stream->cbconf.para.did.down_dmac = tmp;
		} else if (strcmp(content, "down-vlan-tagged") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			if (strcmp(ele_val, "tagged") == 0) {
				stream->cbconf.para.did.down_tagged = 1;
			} else if (strcmp(ele_val, "priority") == 0) {
				stream->cbconf.para.did.down_tagged = 2;
			} else if (strcmp(ele_val, "all") == 0) {
				stream->cbconf.para.did.down_tagged = 3;
			} else {
				sprintf(err_msg,
					"unknown '%s' in '%s'",
					content, node_path);
				rc = EXIT_FAILURE;
				goto out;
			}
		} else if (strcmp(content, "down-vlan-id") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.did.down_vid = (uint16_t)tmp;
		} else if (strcmp(content, "down-priority") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.did.down_prio = (uint8_t)tmp;
		} else if (strcmp(content, "up-dest-address") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			strcpy(path, node_path);
			strcat(path, "/up-dest-address");
			rc = parse_mac_address(ele_val, &tmp,
					       err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;

			stream->cbconf.para.did.up_dmac = tmp;
		} else if (strcmp(content, "up-vlan-tagged") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			if (strcmp(ele_val, "tagged") == 0) {
				stream->cbconf.para.did.up_tagged = 1;
			} else if (strcmp(ele_val, "priority") == 0) {
				stream->cbconf.para.did.up_tagged = 2;
			} else if (strcmp(ele_val, "all") == 0) {
				stream->cbconf.para.did.up_tagged = 3;
			} else {
				sprintf(err_msg,
					"unknown '%s' in '%s'",
					content, node_path);
				rc = EXIT_FAILURE;
				goto out;
			}
		} else if (strcmp(content, "up-vlan-id") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.did.up_vid = (uint16_t)tmp;
		} else if (strcmp(content, "up-priority") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.did.up_prio = (uint8_t)tmp;
		}
	}
out:
	return rc;
}

int parse_stream_ip_params(xmlNode *node,
	struct std_cb_stream *stream, char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	char *content;
	uint64_t tmp;
	xmlNode *tmp_node;
	char ele_val[MAX_ELEMENT_LENGTH];
	char path[MAX_PATH_LENGTH];
	char *para_name = "ip-octuple-stream-identification-params";
	struct ip_addr address;

	nc_verb_verbose("%s is called", __func__);
	/* get ip-octuple-stream-identification-params node */
	tmp_node = get_child_node(node, para_name);
	if (!tmp_node) {
		sprintf(err_msg, "'%s' and identification-type doesn't match",
			para_name);
		rc = EXIT_FAILURE;
		goto out;
	}

	for (tmp_node = tmp_node->children;
	     tmp_node != NULL; tmp_node = tmp_node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		content = (char *)tmp_node->name;
		if (strcmp(content, "dest-address") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			strcpy(path, node_path);
			strcat(path, "/dest-address");
			rc = parse_mac_address(ele_val, &tmp,
					       err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;

			stream->cbconf.para.iid.dmac = tmp;
		} else if (strcmp(content, "down-vlan-tagged") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			if (strcmp(ele_val, "tagged") == 0) {
				stream->cbconf.para.iid.tagged = 1;
			} else if (strcmp(ele_val, "priority") == 0) {
				stream->cbconf.para.iid.tagged = 2;
			} else if (strcmp(ele_val, "all") == 0) {
				stream->cbconf.para.iid.tagged = 3;
			} else {
				sprintf(err_msg,
					"unknown '%s' in '%s'",
					content, node_path);
				rc = EXIT_FAILURE;
				goto out;
			}
		} else if (strcmp(content, "vlan-id") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.iid.vid = (uint16_t)tmp;
		} else if (strcmp(content, "source-ip-address") == 0) {
			strcpy(path, node_path);
			strcat(path, "/source-ip-address");
			rc = parse_ip_address(tmp_node, &address,
					       err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;

			stream->cbconf.para.iid.siph = address.iph;
			stream->cbconf.para.iid.sipl = address.ipl;
		} else if (strcmp(content, "dest-ip-address") == 0) {
			strcpy(path, node_path);
			strcat(path, "/dest-ip-address");
			rc = parse_ip_address(tmp_node, &address,
					       err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;

			stream->cbconf.para.iid.diph = address.iph;
			stream->cbconf.para.iid.dipl = address.ipl;
		} else if (strcmp(content, "dscp") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.iid.dscp = (uint16_t)tmp;
		} else if (strcmp(content, "next-protocol") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			if (strcmp(ele_val, "UDP") == 0) {
				stream->cbconf.para.iid.npt = 0;
			} else if (strcmp(ele_val, "TCP") == 0) {
				stream->cbconf.para.iid.npt = 1;
			} else if (strcmp(ele_val, "SCTP") == 0) {
				stream->cbconf.para.iid.npt = 2;
			} else if (strcmp(ele_val, "none") == 0) {
				stream->cbconf.para.iid.npt = 3;
			} else {
				sprintf(err_msg,
					"unknown '%s' in '%s'",
					content, node_path);
				rc = EXIT_FAILURE;
				goto out;
			}
		} else if (strcmp(content, "source-port") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.iid.sport = (uint16_t)tmp;
		} else if (strcmp(content, "dest-port") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			tmp = strtoul(ele_val, NULL, 0);
			stream->cbconf.para.iid.dport = (uint16_t)tmp;
		}
	}
out:
	return rc;
}

int parse_stream_id_table(xmlNode *node,
	struct std_cb_stream_table *stream_table,
	char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	char *content;
	uint64_t tmp;
	xmlNode *tmp_node;
	char ele_val[MAX_ELEMENT_LENGTH];
	char path[MAX_PATH_LENGTH];
	struct std_cb_stream *entry = stream_table->stream_ptr;

	nc_verb_verbose("%s is called", __func__);

	tmp_node = node;
	for (tmp_node = node->children; tmp_node != NULL;
	     tmp_node = tmp_node->next) {
		if (tmp_node->type != XML_ELEMENT_NODE)
			continue;

		content = (char *)tmp_node->name;
		if (strcmp(content, "index") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS) {
				goto out;
			} else {
				tmp = strtoul(ele_val, NULL, 0);
				entry->index = (uint32_t)tmp;
				entry->enable = TRUE;
			}
		} else if (strcmp(content, "stream-handle") == 0) {
			rc = xml_read_field(tmp_node, content, ele_val,
					    err_msg, node_path);
			if (rc != EXIT_SUCCESS) {
				goto out;
			} else {
				tmp = strtoul(ele_val, NULL, 0);
				entry->cbconf.handle = (uint32_t)(tmp);
			}
		} else if (strcmp(content,
				  "in-facing-output-port-list") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS) {
				goto out;
			} else {
				tmp = strtoul(ele_val, NULL, 0);
				entry->cbconf.ifac_oport = (uint32_t)(tmp);
			}
		} else if (strcmp(content,
				  "out-facing-output-port-list") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS) {
				goto out;
			} else {
				tmp = strtoul(ele_val, NULL, 0);
				entry->cbconf.ofac_oport = (uint32_t)(tmp);
			}
		} else if (strcmp(content,
				  "in-facing-input-port-list") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS) {
				goto out;
			} else {
				tmp = strtoul(ele_val, NULL, 0);
				entry->cbconf.ifac_iport = (uint32_t)(tmp);
			}
		} else if (strcmp(content,
				  "out-facing-input-port-list") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS) {
				goto out;
			} else {
				tmp = strtoul(ele_val, NULL, 0);
				entry->cbconf.ofac_iport = (uint32_t)(tmp);
			}
		} else if (strcmp(content, "identification-type") == 0) {
			rc = xml_read_field(tmp_node, content,
					    ele_val, err_msg, node_path);
			if (rc != EXIT_SUCCESS)
				goto out;

			if (strcmp(ele_val, "null") == 0) {
				entry->cbconf.type = STREAMID_NULL;
			} else if (strcmp(ele_val,
					  "source-mac-and-vlan") == 0) {
				entry->cbconf.type = STREAMID_SMAC_VLAN;
			} else if (strcmp(ele_val,
					  "dest-mac-and-vlan") == 0) {
				/* not supported in LS1028ARDB
				entry->cbconf.type = STREAMID_DMAC_VLAN;
				*/
				sprintf(err_msg,
					"unsupported stream identify type");
				rc = EXIT_FAILURE;
				goto out;
			} else if (strcmp(ele_val, "ip-octuple") == 0) {
				entry->cbconf.type = STREAMID_IP;
				sprintf(err_msg,
					"unsupported stream identify type");
				rc = EXIT_FAILURE;
				goto out;
			} else {
				sprintf(err_msg, "unknown '%s' in '%s'",
					ele_val, node_path);
				rc = EXIT_FAILURE;
				goto out;
			}
		} else if (strcmp(content, "lan-path-id") == 0) {

		} else if (strcmp(content, "parameters") == 0) {
			strcpy(path, node_path);
			strcat(path, "/parameters");
			switch (entry->cbconf.type) {
			case STREAMID_NULL:
				rc = parse_null_streamid_params(tmp_node,
								entry, err_msg,
								path);
				break;
			case STREAMID_SMAC_VLAN:
				rc = parse_smac_vlan_params(tmp_node, entry,
							    err_msg, path);
				break;
			case STREAMID_DMAC_VLAN:
				rc = parse_dmac_vlan_params(tmp_node,
							    entry, err_msg,
							    path);
				break;
			case STREAMID_IP:
				rc = parse_stream_ip_params(tmp_node,
							    entry, err_msg,
							    path);
				break;
			default:
				sprintf(err_msg,
					"Invalid streamid type");
				rc = EXIT_FAILURE;
				break;
			}
			if (rc != EXIT_SUCCESS)
				goto out;
		}
	}
out:
	return rc;
}

int parse_cb_node(xmlNode *node, struct std_cb_conf *cb_conf,
	char *err_msg, char *node_path)
{
	int rc = EXIT_SUCCESS;
	char *content;
	xmlNode *tmp_node;
	char path[MAX_PATH_LENGTH];
	struct std_cb_stream_table *cur_table = cb_conf->stream_table;

	nc_verb_verbose("%s is called", __func__);

	tmp_node = node;
	for (tmp_node = node->children; tmp_node != NULL;
	     tmp_node = tmp_node->next) {
		if (tmp_node->type != XML_ELEMENT_NODE)
			continue;

		content = (char *)tmp_node->name;
		if (strcmp(content, "stream-identity-table") == 0) {
			if (cb_conf->stream_table == NULL) {
				cb_conf->stream_table = creat_new_stream_instance();
				cur_table = cb_conf->stream_table;
			} else {
				cur_table->next = creat_new_stream_instance();
				cur_table = cur_table->next;
			}
			if (cur_table == NULL) {
				rc = EXIT_FAILURE;
				sprintf(err_msg,
					"allocate memory to '%s' failure!",
					content);
				goto out;
			}
			strcpy(path, node_path);
			strcat(path, "/stream-identity-table");
			rc = parse_stream_id_table(tmp_node, cur_table,
						   err_msg, path);
			if (rc != EXIT_SUCCESS)
				goto out;
		} else {
			sprintf(err_msg, "unknown node '%s' in path '%s'!",
				content, node_path);
			rc = EXIT_FAILURE;
			goto out;
		}
	}
out:
	return rc;
}
int cbstreamid_handle(char *portname, xmlNode *node,
	char *err_msg, char *node_path, int disable)
{
	int rc = EXIT_SUCCESS;
	struct std_cb_conf cb_conf;
	char path[MAX_PATH_LENGTH];
	struct std_cb_stream_table *table = NULL;

	nc_verb_verbose("%s is called", __func__);
	/* Init cb configuration data */
	init_cb_memory(&cb_conf);

	rc = parse_cb_node(node, &cb_conf, err_msg, path);

	if (rc != EXIT_SUCCESS)
		goto out;

	table = cb_conf.stream_table;
	while (table != NULL) {
		if (disable)
			table->stream_ptr->enable = FALSE;

		/* set new stream filters configuration */
		rc = tsn_cb_streamid_set(portname,
					 table->stream_ptr->index,
					 table->stream_ptr->enable,
					 &(table->stream_ptr->cbconf));
		if (rc != EXIT_SUCCESS) {
			sprintf(err_msg,
				"failed to set stream identification, %s!",
				strerror(-rc));
		       goto out;
		}
		if (table->next == NULL)
			break;
		table = table->next;
	}
out:
	free_cb_memory(&cb_conf);
	return rc;
}

int get_cb_status(char *port, xmlNodePtr node)
{
	int rc = EXIT_SUCCESS;
	return rc;
}

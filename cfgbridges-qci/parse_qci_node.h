/*
 * SPDX-License-Identifier:	(GPL-2.0 OR MIT)
 *
 * Copyright 2019 NXP
 */
#include <tsn/genl_tsn.h>
#include <linux/tsn.h>
#include <libxml/tree.h>
#include <libnetconf_xml.h>
#include <platform.h>

#ifndef __PARSE_QCI_NODE_H__
#define __PARSE_QCI_NODE_H__

struct std_qci_psfp_sfi {
	uint32_t stream_filter_instance_id;
	bool enable;
	struct tsn_qci_psfp_sfi_conf sficonf;
};

struct std_qci_psfp_sfi_table {
	struct std_qci_psfp_sfi *sfi_ptr;
	struct std_qci_psfp_sfi_table *next;
};

struct std_qci_psfp_sgi {
	uint32_t sgi_handle;
	bool enable;
	struct tsn_qci_psfp_sgi_conf sgiconf;
};

struct std_qci_psfp_sgi_table {
	struct std_qci_psfp_sgi *sgi_ptr;
	struct std_qci_psfp_sgi_table *next;
};

struct std_qci_psfp_fmi {
	uint32_t fmi_id;
	bool enable;
	struct tsn_qci_psfp_fmi fmiconf;
};

struct std_qci_psfp_fmi_table {
	struct std_qci_psfp_fmi *fmi_ptr;
	struct std_qci_psfp_fmi_table *next;
};

struct std_qci_conf {
	char device_name[MAX_IF_NAME_LENGTH];
	struct std_qci_psfp_sfi_table *sfi_table;
	struct std_qci_psfp_sgi_table *sgi_table;
	struct std_qci_psfp_fmi_table *fmi_table;
};

int get_qci_status(char *port, xmlNodePtr node);
int stream_filters_handle(char *portname, xmlNode *node,
	   char *err_msg, char *node_path);
int stream_gates_handle(char *portname, xmlNode *node,
	   char *err_msg, char *node_path);
int flowmeters_handle(char *portname, xmlNode *node,
	   char *err_msg, char *node_path);
int get_sfi_config(char *port, xmlNodePtr node, int mode, uint32_t index);
int get_sgi_config(char *port, xmlNodePtr node, int mode, uint32_t index);
int get_fmi_config(char *port, xmlNodePtr node, int mode, uint32_t index);
#endif

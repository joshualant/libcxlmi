#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <libcxlmi.h>

#define MiB (1024 * 1024)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

const uint8_t cel_uuid[0x10] = { 0x0d, 0xa9, 0xc0, 0xb5,
    0xbf, 0x41,
    0x4b, 0x78,
    0x8f, 0x79,
    0x96, 0xb1, 0x62, 0x3b, 0x3f, 0x17 };

const uint8_t ven_dbg[0x10] = { 0x5e, 0x18, 0x19, 0xd9,
       0x11, 0xa9,
       0x40, 0x0c,
       0x81, 0x1f,
       0xd6, 0x07, 0x19, 0x40, 0x3d, 0x86 };

const uint8_t c_s_dump[0x10] = { 0xb3, 0xfa, 0xb4, 0xcf,
    0x01, 0xb6,
    0x43, 0x32,
    0x94, 0x3e,
    0x5e, 0x99, 0x62, 0xf2, 0x35, 0x67 };

const int maxlogs = 10; /* Only 7 in CXL r3.1, but let us leave room */

typedef enum CxlExtentSelectionPolicy {
    CXL_EXTENT_SELECTION_POLICY_FREE,
    CXL_EXTENT_SELECTION_POLICY_CONTIGUOUS,
    CXL_EXTENT_SELECTION_POLICY_PRESCRIPTIVE,
    CXL_EXTENT_SELECTION_POLICY_ENABLE_SHARED_ACCESS,
    CXL_EXTENT_SELECTION_POLICY__MAX,
} CxlExtentSelectionPolicy;

typedef enum CxlExtentRemovalPolicy {
    CXL_EXTENT_REMOVAL_POLICY_TAG_BASED,
    CXL_EXTENT_REMOVAL_POLICY_PRESCRIPTIVE,
    CXL_EXTENT_REMOVAL_POLICY__MAX,
} CxlExtentRemovalPolicy;

typedef struct {
    uint64_t start_dpa;
    uint64_t len;
} extent;

typedef enum physical_port_control_opcode {
    ASSERT_PERST = 0x00,
    DEASSERT_PERST = 0x01,
    RESET_PPB = 0x02,
    MAX_PPC_OPCODE
} physical_port_control_opcode;

static int parse_supported_logs(struct cxlmi_cmd_get_supported_logs_rsp *pl,
				size_t *cel_size)
{
	int i, j;

	*cel_size = 0;
	printf("Get Supported Logs Response %d\n",
	       pl->num_supported_log_entries);

	for (i = 0; i < pl->num_supported_log_entries; i++) {
		for (j = 0; j < sizeof(pl->entries[i].uuid); j++) {
			if (pl->entries[i].uuid[j] != cel_uuid[j])
				break;
		}
		if (j == 0x10) {
			*cel_size = pl->entries[i].log_size;
			printf("\tCommand Effects Log (CEL) available\n");
		}
		for (j = 0; j < sizeof(pl->entries[i].uuid); j++) {
			if (pl->entries[i].uuid[j] != ven_dbg[j])
				break;
		}
		if (j == 0x10)
			printf("\tVendor Debug Log available\n");
		for (j = 0; j < sizeof(pl->entries[i].uuid); j++) {
			if (pl->entries[i].uuid[j] != c_s_dump[j])
				break;
		}
		if (j == 0x10)
			printf("\tComponent State Dump Log available\n");
	}
	if (*cel_size == 0) {
		return -1;
	}
	return 0;
}

static int support_opcode(struct cxlmi_endpoint *ep, int cel_size,
		uint16_t opcode, bool *supported)
{
	struct cxlmi_cmd_get_log_req in = {
		.offset = 0,
		.length = cel_size,
	};
	struct cxlmi_cmd_get_log_cel_rsp *ret;
	int i, rc;

	ret = calloc(1, sizeof(*ret) + cel_size);
	if (!ret)
		return -1;

	memcpy(in.uuid, cel_uuid, sizeof(in.uuid));
	rc = cxlmi_cmd_get_log_cel(ep, NULL, &in, ret);
	if (rc)
		goto done;

	for (i = 0; i < cel_size / sizeof(*ret); i++) {
		if (opcode == ret[i].opcode) {
			*supported = true;
			break;
		}
	}
done:
	free(ret);
	return rc;
}

static inline bool ep_supports_op(struct cxlmi_endpoint *ep, uint16_t opcode)
{
	int rc;
	size_t cel_size;
	struct cxlmi_cmd_get_supported_logs_rsp *gsl;
	bool op_support = false;

	gsl = calloc(1, sizeof(*gsl) + maxlogs * sizeof(*gsl->entries));
	if (!gsl)
		return op_support;

	rc = cxlmi_cmd_get_supported_logs(ep, NULL, gsl);
	if (rc) {
		free(gsl);
		return op_support;
	}

	rc = parse_supported_logs(gsl, &cel_size);
	if (rc) {
		free(gsl);
		return op_support;
	}

	/* we know there is a CEL */
	(void)support_opcode(ep, cel_size, opcode, &op_support);

	free(gsl);
	return op_support;
}

int show_cel(struct cxlmi_endpoint *ep, int cel_size)
{
	struct cxlmi_cmd_get_log_req in = {
		.offset = 0,
		.length = cel_size,
	};
	struct cxlmi_cmd_get_log_cel_rsp *ret;
	int i, rc;

	ret = calloc(1, sizeof(*ret) + cel_size);
	if (!ret)
		return -1;

	memcpy(in.uuid, cel_uuid, sizeof(in.uuid));
	rc = cxlmi_cmd_get_log_cel(ep, NULL, &in, ret);
	if (rc)
		goto done;

	for (i = 0; i < cel_size / sizeof(*ret); i++) {
		printf("\t[%04x] %s%s%s%s%s%s%s%s\n",
		       ret[i].opcode,
		       ret[i].command_effect & 0x1 ? "ColdReset " : "",
		       ret[i].command_effect & 0x2 ? "ImConf " : "",
		       ret[i].command_effect & 0x4 ? "ImData " : "",
		       ret[i].command_effect & 0x8 ? "ImPol " : "",
		       ret[i].command_effect & 0x10 ? "ImLog " : "",
		       ret[i].command_effect & 0x20 ? "ImSec" : "",
		       ret[i].command_effect & 0x40 ? "BgOp" : "",
		       ret[i].command_effect & 0x80 ? "SecSup" : "");
	}
done:
	free(ret);
	return rc;
}

void parse_cxlmi_cmd_fmapi_port_state_info_block(
        struct cxlmi_cmd_fmapi_get_phys_port_state_rsp *in) {

    for(int i = 0; i < in->num_ports; i++) {
        struct cxlmi_cmd_fmapi_port_state_info_block blk = in->ports[i];
        printf("port_id: %d\n"
            "config_state: %d\n"
            "conn_dev_cxl_ver: %d\n"
            "rsv1: %d\n"
            "conn_dev_type: %d\n"
            "port_cxl_ver_bitmask: %d\n"
            "max_link_width: %d\n"
            "negotiated_link_width: %d\n"
            "supported_link_speeds_vector: %d\n"
            "max_link_speed: %d\n"
            "current_link_speed: %d\n"
            "ltssm_state: %d\n"
            "first_lane_num: %d\n"
            "link_state: %d\n"
            "supported_ld_count: %d\n",
            blk.port_id,
            blk.config_state,
            blk.conn_dev_cxl_ver,
            blk.rsv1,
            blk.conn_dev_type,
            blk.port_cxl_ver_bitmask,
            blk.max_link_width,
            blk.negotiated_link_width,
            blk.supported_link_speeds_vector,
            blk.max_link_speed,
            blk.current_link_speed,
            blk.ltssm_state,
            blk.first_lane_num,
            blk.link_state,
            blk.supported_ld_count);
    }
}

int parse_vcs_info_rsp(struct cxlmi_cmd_fmapi_get_vcs_info_rsp *rsp) {
    const struct cxlmi_cmd_fmapi_vcs_info_block *vcs;
    const struct cxlmi_cmd_fmapi_vppb_info *vppb;
    uint8_t i, j, nbytes;

    printf("CXL FMAPI VCS Info:\n");
    printf("  num_vcs: %u\n", rsp->num_vcs);
    vcs = rsp->vcs_info_list;

    for (i = 0; i < rsp->num_vcs; i++) {
	    printf("  VCS[%u]:\n", i);
	    printf("    vcs_id     : %u\n", vcs->vcs_id);
	    printf("    vcs_state  : %u\n", vcs->vcs_state);
	    printf("    usp_id     : %u\n", vcs->usp_id);
	    printf("    num_vppbs  : %u\n", vcs->num_vppbs);
	    vppb = vcs->vppbs;

	    for (j = 0; j < vcs->num_vppbs; j++) {
		    printf("      VPPB[%u]:\n", j);
		    printf("        binding_status : %u\n", vppb->binding_status);
		    printf("        bound_port_id  : %u\n", vppb->bound_port_id);
		    printf("        bound_ld_id    : %u\n", vppb->bound_ld_id);
		    printf("        rsvd           : %u\n", vppb->rsv1);
		    vppb++;
	    }

            nbytes = sizeof(struct cxlmi_cmd_fmapi_vcs_info_block) +
                (vcs->num_vppbs*sizeof(struct cxlmi_cmd_fmapi_vppb_info));
            //vcs = ((uint8_t*)vcs)
            uint8_t *vcs_tmp = ((uint8_t*)vcs) + nbytes;
            vcs = (struct cxlmi_cmd_fmapi_vcs_info_block *)vcs_tmp;
	    /* advance to next vcs_info_block */
	    //vcs = (const struct cxlmi_cmd_fmapi_vcs_info_block *)vppb;
    }
    return 0;
}

void print_identify_switch_device_rsp(struct cxlmi_cmd_fmapi_identify_sw_device_rsp *rsp) {
    printf("Identify Switch Device Response:\n");
    printf("  Ingress Port ID: %u\n", rsp->ingress_port_id);
    printf("  Reserved: %u\n", rsp->rsv1);
    printf("  Number of Physical Ports: %u\n", rsp->num_physical_ports);
    printf("  Number of VCS: %u\n", rsp->num_vcs);

    printf("  Active Port Bitmask:\n    ");
    for (int i = 0; i < 32; i++) {
        printf("%02X ", rsp->active_port_bitmask[i]);
        if ((i+1) % 16 == 0) printf("\n    ");
    }

    printf("\n  Active VCS Bitmask:\n    ");
    for (int i = 0; i < 32; i++) {
        printf("%02X ", rsp->active_vcs_bitmask[i]);
        if ((i+1) % 16 == 0) printf("\n    ");
    }

    printf("\n  Total VPPBs: %u\n", rsp->num_total_vppb);
    printf("  Active VPPBs: %u\n", rsp->num_active_vppb);
    printf("  Number of HDM Decoders per USP: %u\n", rsp->num_hdm_decoder_per_usp);
    printf("\n");
}

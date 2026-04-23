// SPDX-License-Identifier: LGPL-2.1-or-later
//
// A test to perform configuration of a VCS switch.
//
#include "examples.h"
#include "cxlmi/api-types.h"

#define LOGN_MESSAGE_LIMIT 9
#define NUM_VCS 2
#define VPPB_LIST_LIMIT 8

/* Frees all endpoints for the given context and the context itself */
static void cleanup_ctx(struct cxlmi_ctx *ctx)
{
    struct cxlmi_endpoint *ep, *tmp;
    cxlmi_for_each_endpoint_safe(ctx, ep, tmp) {
        cxlmi_close(ep);
    }
    cxlmi_free_ctx(ctx);
}


int main(int argc, char **argv)
{
    // Setup and open the MCTP endpoint
    struct cxlmi_ctx *ctx;
    struct cxlmi_endpoint *ep;
    int rc = 0;
    unsigned int nid;
    uint8_t eid;

    ctx = cxlmi_new_ctx(stdout, DEFAULT_LOGLEVEL);
    if (!ctx) {
        fprintf(stderr, "cannot create new context object\n");
        return EXIT_FAILURE;
    }

    if(argc != 3) {
        fprintf(stderr, "argc=%d", argc);
        fprintf(stderr, "Input should be <nid> <eid> for MCTP endpoint\n");
        return EXIT_FAILURE;
    }

    nid = strtol(argv[1], NULL, 10);
    eid = strtol(argv[2], NULL, 10);
    printf("ep %d:%d\n", nid, eid);

    ep = cxlmi_open_mctp(ctx, nid, eid);
    if (!ep) {
        fprintf(stderr, "cannot open MCTP endpoint %d:%d\n", nid, eid);
        rc = -1;
        goto exit_free_ctx;
    }

    // FM issues get virtual CXL switch info (can create list of binding targets) 0x5200
    // Need to work on inputs... not necessarily correct...
    struct cxlmi_cmd_fmapi_get_vcs_info_req *vs_req;
    struct cxlmi_cmd_fmapi_get_vcs_info_rsp *vs_rsp;
    ssize_t max_rsp_sz, req_sz;

    req_sz = sizeof(struct cxlmi_cmd_fmapi_get_vcs_info_req);
    vs_req = calloc(1, req_sz + (NUM_VCS*sizeof(*(vs_req->vcs_id_list))));
    vs_req->start_vppb = 0;
    vs_req->vppb_list_limit = VPPB_LIST_LIMIT;
    vs_req->num_vcs = NUM_VCS;
    for(uint8_t i = 0; i < NUM_VCS; i++) {
        vs_req->vcs_id_list[i] = i;
    }

    max_rsp_sz = sizeof(*vs_rsp)
        + (NUM_VCS * (sizeof(struct cxlmi_cmd_fmapi_vcs_info_block) + (VPPB_LIST_LIMIT * sizeof(struct cxlmi_cmd_fmapi_vppb_info))));

    printf("size of the struct is? %lu %lu \n", sizeof(*(vs_rsp->vcs_info_list->vppbs)), sizeof(*vs_rsp));
    vs_rsp = calloc(1, max_rsp_sz);

    printf("**********\n");
    printf("Issuing get vcs info \n");
    printf("**********\n");
    rc = cxlmi_cmd_fmapi_get_vcs_info(ep, NULL, vs_req, vs_rsp);
    if(rc != 0) {
        printf("Error getting virtual switch info...\n");
        goto exit_free_ctx;
    }
    parse_vcs_info_rsp(vs_rsp);
    goto exit_free_ctx;


exit_free_ctx:
    cleanup_ctx(ctx);
    return rc;
}

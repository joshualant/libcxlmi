// SPDX-License-Identifier: LGPL-2.1-or-later
//
// A test to perform configuration of a VCS switch.
//
#include "examples.h"
#include "cxlmi/api-types.h"

#define LOGN_MESSAGE_LIMIT 9
#define NUM_VCS 1
#define VPPB_LIST_LIMIT 1

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
    uint8_t vcs, vppb, ppb;

    ctx = cxlmi_new_ctx(stdout, DEFAULT_LOGLEVEL);
    if (!ctx) {
        fprintf(stderr, "cannot create new context object\n");
        return EXIT_FAILURE;
    }

    if(argc != 6) {
        fprintf(stderr, "argc=%d", argc);
        fprintf(stderr, "Input should be <nid> <eid> <vcs> <vppb> <ppb> for unbind/bind\n");
        return EXIT_FAILURE;
    }

    nid = strtol(argv[1], NULL, 10);
    eid = strtol(argv[2], NULL, 10);
    vcs = strtol(argv[3], NULL, 10);
    vppb = strtol(argv[4], NULL, 10);
    ppb = strtol(argv[5], NULL, 10);
    printf("ep %d:%d\n", nid, eid);

    ep = cxlmi_open_mctp(ctx, nid, eid);
    if (!ep) {
        fprintf(stderr, "cannot open MCTP endpoint %d:%d\n", nid, eid);
        rc = -1;
        goto exit_free_ctx;
    }

    struct cxlmi_cmd_fmapi_bind_vppb_req vppb_req = {vcs, vppb, ppb, 0, 0xffff};
    printf("**********\n");
    printf("Issuing bind vppb\n");
    printf("**********\n");
    rc = cxlmi_cmd_fmapi_bind_vppb(ep, NULL, &vppb_req);
    if(rc != 0) {
        printf("Error binding virtual ppb...\n");
        goto exit_free_ctx;
    }
    goto exit_free_ctx;

exit_free_ctx:
    cleanup_ctx(ctx);
    return rc;
}

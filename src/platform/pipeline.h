#ifndef EFX_PIPELINE_H
#define EFX_PIPELINE_H

/* sokol-backed GPU sink for the render module (platform side, ADR 0003) */

void efx_pipeline_install(void);   /* after sg_setup */
void efx_pipeline_play(void);      /* inside an active pass */
void efx_pipeline_shutdown(void);  /* before sg_shutdown */

#endif

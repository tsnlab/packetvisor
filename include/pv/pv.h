#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Packetvisor. This function reads configuration and setup all the environments,
 * such as network interface controller, packet pool, ...
 * This function must be called only once before call any function related to Packetvisor.
 *
 * @return 0 when there is no error, error code is not specified yet.
 */
int pv_init();

/**
 * Clean up the resources which Packetvisor is allocated. This function must be called only
 * once before application termination.
 */
void pv_finalize();

#ifdef __cplusplus
}
#endif

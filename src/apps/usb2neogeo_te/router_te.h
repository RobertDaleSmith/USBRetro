#ifndef ROUTER_TE_H
#define ROUTER_TE_H

// TE build: use the core router header verbatim and add only the TE-specific
// entry point. Previously this file was a full copy of core/router/router.h
// that reused its ROUTER_H include guard, so whichever was included first
// silently shadowed the other.
#include "core/router/router.h"

void router_register_device(uint8_t dev_addr, uint8_t instance,
                            input_transport_t transport, const char* name);

#endif // ROUTER_TE_H

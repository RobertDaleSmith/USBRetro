// output.c
// Output registry - selects active output based on compile-time configuration

#include "output_interface.h"

// Include console-specific headers based on CONFIG_* defines
#ifdef CONFIG_NGC
#include "native/device/gamecube/gamecube_device.h"
extern const OutputInterface gamecube_output_interface;
#endif

#ifdef CONFIG_PCE
#include "native/device/pcengine/pcengine_device.h"
extern const OutputInterface pcengine_output_interface;
#endif

#ifdef CONFIG_NUON
#include "native/device/nuon/nuon_device.h"
extern const OutputInterface nuon_output_interface;
#endif

#ifdef CONFIG_LOOPY
#include "native/device/loopy/loopy_device.h"
extern const OutputInterface loopy_output_interface;
#endif

#ifdef CONFIG_XB1
#include "native/device/xboxone/xboxone_device.h"
extern const OutputInterface xboxone_output_interface;
#endif

#ifdef CONFIG_3DO
#include "native/device/3do/3do_device.h"
extern const OutputInterface threedooutput_interface;
#endif

// Select active output at compile-time
const OutputInterface* active_output =
#ifdef CONFIG_NGC
    &gamecube_output_interface;
#elif CONFIG_PCE
    &pcengine_output_interface;
#elif CONFIG_NUON
    &nuon_output_interface;
#elif CONFIG_LOOPY
    &loopy_output_interface;
#elif CONFIG_XB1
    &xboxone_output_interface;
#elif CONFIG_3DO
    &threedooutput_interface;
#else
    #error "No output configured - must define CONFIG_NGC, CONFIG_PCE, CONFIG_NUON, CONFIG_LOOPY, CONFIG_XB1, or CONFIG_3DO"
#endif

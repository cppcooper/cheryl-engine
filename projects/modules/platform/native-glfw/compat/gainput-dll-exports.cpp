#include <gainput/gainput.h>

namespace gainput {
    // Client-side device factories need this insertion operator exported by Gainput.
    template GAINPUT_LIBEXPORT InputDevice*& HashMap<DeviceId, InputDevice*>::operator[](const DeviceId&);
}

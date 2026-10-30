/*
 * AudioSystem entry points used by the qcril blob (libril-qc-qmi-1.so).
 *
 * Android 16 has no LD_SHIM_LIBS and vendor processes cannot load the system
 * libaudioclient, so the blob is patched to need this vendor library instead.
 * The calls only tune the audio HAL from the RIL; doing nothing is safe, but
 * the signatures must be the real ones: getParameters returns a String8 by
 * value and has to construct it.
 */

#include <stdint.h>
#include <utils/Errors.h>
#include <utils/String8.h>

namespace android {

typedef void (*audio_error_callback)(int);

class AudioSystem {
  public:
    static status_t setParameters(int ioHandle, const String8& keyValuePairs);
    static String8 getParameters(int ioHandle, const String8& keys);
    static void setErrorCallback(audio_error_callback cb);
    static uintptr_t addErrorCallback(audio_error_callback cb);
};

status_t AudioSystem::setParameters(int, const String8&) {
    return NO_ERROR;
}

String8 AudioSystem::getParameters(int, const String8&) {
    return String8();
}

void AudioSystem::setErrorCallback(audio_error_callback) {}

uintptr_t AudioSystem::addErrorCallback(audio_error_callback) {
    return 0;
}

}  // namespace android

/*
 * SensorManager without sensors for the angler mm-camera blobs
 * (liboemcamera.so, libmmcamera2_imglib_modules.so).
 *
 * On Android 16 vendor processes cannot load the system libsensor, and the
 * LineageOS libsensor_vendor lacks SensorEventQueue::getFd() (and hung the
 * camera daemon at session close on Android 13). The blobs are patched to
 * need this library instead: it reports an empty sensor list, so the camera
 * runs without gyro/accelerometer input (EIS and motion hints only).
 * Signatures match the libsensor ABI the 32-bit blobs link against.
 */

#include <stdint.h>
#include <sys/types.h>
struct ASensorEvent;
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>
#include <utils/Timers.h>

namespace android {

class Sensor {
  public:
    int32_t getHandle() const;
};

class SensorEventQueue : public RefBase {
  public:
    ssize_t read(ASensorEvent* events, size_t numEvents);
    status_t enableSensor(Sensor const* sensor) const;
    status_t disableSensor(Sensor const* sensor) const;
    status_t setEventRate(Sensor const* sensor, nsecs_t ns) const;
    int getFd() const;
};

class SensorManager {
  public:
    static SensorManager& getInstanceForPackage(const String16& packageName);
    ssize_t getSensorList(Sensor const* const** list);
    Sensor const* getDefaultSensor(int type);
    sp<SensorEventQueue> createEventQueue(String8 packageName, int mode);
};

int32_t Sensor::getHandle() const { return -1; }

ssize_t SensorEventQueue::read(ASensorEvent*, size_t) { return 0; }
status_t SensorEventQueue::enableSensor(Sensor const*) const { return NO_INIT; }
status_t SensorEventQueue::disableSensor(Sensor const*) const { return NO_ERROR; }
status_t SensorEventQueue::setEventRate(Sensor const*, nsecs_t) const { return NO_INIT; }
int SensorEventQueue::getFd() const { return -1; }

SensorManager& SensorManager::getInstanceForPackage(const String16&) {
    static SensorManager instance;
    return instance;
}

ssize_t SensorManager::getSensorList(Sensor const* const** list) {
    static Sensor const* const kNone[1] = {nullptr};
    if (list) *list = kNone;
    return 0;
}

Sensor const* SensorManager::getDefaultSensor(int) {
    return nullptr;
}

sp<SensorEventQueue> SensorManager::createEventQueue(String8, int) {
    return nullptr;
}

}  // namespace android

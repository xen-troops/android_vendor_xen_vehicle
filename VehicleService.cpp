/*
 * Copyright (C) 2016 The Android Open Source Project
 * Copyright (C) 2019 EPAM Systems Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "automotive.vehicle@2.0-service.xenvm"
#include <android/log.h>
#include <hidl/HidlTransportSupport.h>
#include <cutils/properties.h>

#include <iostream>

#include <vhal_v2_0/VehicleHalManager.h>
#include <vhal_v2_0/VisVehicleHal.h>
#include "EmulatedVehicleHal.h"

using namespace android;
using namespace android::hardware;
using namespace android::hardware::automotive::vehicle::V2_0;
using namespace android::hardware::automotive::vehicle::V2_0::xenvm;

int main(int /* argc */, char* /* argv */ []) {
    auto store = std::make_unique<VehiclePropertyStore>();
    std::unique_ptr<VehicleHal>  hal;
    // Used only for EmulatedVehicleHal
    std::unique_ptr<impl::VehicleEmulator> emulator;

    if (property_get_bool("persist.vehicle.use-vis-hal", true)) {
        hal = std::make_unique<VisVehicleHal>(store.get());
        ALOGI("Using VisVehicleHal ...");
    } else {
        auto hal_emu = std::make_unique<EmulatedVehicleHal>(store.get());
        emulator = std::make_unique<impl::VehicleEmulator>(hal_emu.get());
        hal.reset(hal_emu.release());
        ALOGI("Using EmulatedVehicleHal ...");
    }

    auto service = std::make_unique<VehicleHalManager>(hal.get());
    configureRpcThreadpool(4, true /* callerWillJoin */);
    ALOGI("Registering as service...");
    status_t status = service->registerAsService();

    if (status != OK) {
        ALOGE("Unable to register vehicle service (%d)", status);
        return 1;
    }

    ALOGI("Ready");
    joinRpcThreadpool();

    return 1;
}

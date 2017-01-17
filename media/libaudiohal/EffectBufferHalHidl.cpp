/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <atomic>

#define LOG_TAG "EffectBufferHalHidl"
//#define LOG_NDEBUG 0

#include <android/hidl/memory/1.0/IAllocator.h>
#include <hidlmemory/mapping.h>
#include <utils/Log.h>

#include "ConversionHelperHidl.h"
#include "EffectBufferHalHidl.h"

using ::android::hardware::Return;
using ::android::hardware::Status;
using ::android::hidl::memory::V1_0::IAllocator;

namespace android {

// static
uint64_t EffectBufferHalHidl::makeUniqueId() {
    static std::atomic<uint64_t> counter{1};
    return counter++;
}

// static
status_t EffectBufferHalInterface::allocate(
        size_t size, sp<EffectBufferHalInterface>* buffer) {
    return mirror(nullptr, size, buffer);
}

// static
status_t EffectBufferHalInterface::mirror(
        void* external, size_t size, sp<EffectBufferHalInterface>* buffer) {
    sp<EffectBufferHalInterface> tempBuffer = new EffectBufferHalHidl(size);
    status_t result = reinterpret_cast<EffectBufferHalHidl*>(tempBuffer.get())->init();
    if (result == OK) {
        tempBuffer->setExternalData(external);
        *buffer = tempBuffer;
    }
    return result;
}

EffectBufferHalHidl::EffectBufferHalHidl(size_t size)
        : mBufferSize(size), mExternalData(nullptr), mAudioBuffer{0, {nullptr}} {
    mHidlBuffer.id = makeUniqueId();
    mHidlBuffer.frameCount = 0;
}

EffectBufferHalHidl::~EffectBufferHalHidl() {
}

status_t EffectBufferHalHidl::init() {
    sp<IAllocator> ashmem = IAllocator::getService("ashmem");
    if (ashmem == 0) {
        ALOGE("Failed to retrieve ashmem allocator service");
        return NO_INIT;
    }
    status_t retval = NO_MEMORY;
    Return<void> result = ashmem->allocate(
            mBufferSize,
            [&](bool success, const hidl_memory& memory) {
                if (success) {
                    mHidlBuffer.data = memory;
                    retval = OK;
                }
            });
    if (retval == OK) {
        mMemory = hardware::mapMemory(mHidlBuffer.data);
        if (mMemory != 0) {
            mMemory->update();
            mAudioBuffer.raw = static_cast<void*>(mMemory->getPointer());
            memset(mAudioBuffer.raw, 0, mMemory->getSize());
            mMemory->commit();
        } else {
            ALOGE("Failed to map allocated ashmem");
            retval = NO_MEMORY;
        }
    }
    return retval;
}

audio_buffer_t* EffectBufferHalHidl::audioBuffer() {
    return &mAudioBuffer;
}

void* EffectBufferHalHidl::externalData() const {
    return mExternalData;
}

void EffectBufferHalHidl::setFrameCount(size_t frameCount) {
    mHidlBuffer.frameCount = frameCount;
    mAudioBuffer.frameCount = frameCount;
}

void EffectBufferHalHidl::setExternalData(void* external) {
    mExternalData = external;
}

void EffectBufferHalHidl::update() {
    if (mExternalData == nullptr) return;
    mMemory->update();
    memcpy(mAudioBuffer.raw, mExternalData, mBufferSize);
    mMemory->commit();
}

void EffectBufferHalHidl::commit() {
    if (mExternalData == nullptr) return;
    memcpy(mExternalData, mAudioBuffer.raw, mBufferSize);
}

} // namespace android
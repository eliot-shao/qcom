/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <cutils/memory.h>

#include <utils/Log.h>

#include <android/native_window.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

using namespace android;

//namespace android {

int main(int argc, char** argv)
{
    // set up the thread-pool
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    // create a client to surfaceflinger
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();
    
    sp<SurfaceControl> surfaceControl_a = client->createSurface(String8("resizea"),
            320, 240, PIXEL_FORMAT_RGB_565, 0);
    sp<SurfaceControl> surfaceControl_b = client->createSurface(String8("resizeb"),
            320, 240, PIXEL_FORMAT_RGB_565, 0);

    sp<Surface> surface_a = surfaceControl_a->getSurface();
	sp<Surface> surface_b = surfaceControl_b->getSurface();
	
    SurfaceComposerClient::openGlobalTransaction();
    surfaceControl_a->setLayer(100000);//blue
	surfaceControl_a->setPosition(100,100);
	surfaceControl_b->setLayer(100001);//red 覆盖blue，setLayer的值越大等级越高
    SurfaceComposerClient::closeGlobalTransaction();

    ANativeWindow_Buffer outBuffer_a;
    surface_a->lock(&outBuffer_a, NULL);//申请outBuffer_a
    ssize_t bpr = outBuffer_a.stride * bytesPerPixel(outBuffer_a.format);
    android_memset16((uint16_t*)outBuffer_a.bits, 0x07E0, bpr*outBuffer_a.height);
    surface_a->unlockAndPost();//发送outBuffer_a到flip
	
	ANativeWindow_Buffer outBuffer_b;
	surface_b->lock(&outBuffer_b, NULL);//申请outBuffer_b
    ssize_t bpr1 = outBuffer_b.stride * bytesPerPixel(outBuffer_b.format);
    android_memset16((uint16_t*)outBuffer_b.bits, 0xF800, bpr*outBuffer_b.height);
    surface_b->unlockAndPost();//发送outBuffer_b到flip

    
    IPCThreadState::self()->joinThreadPool();
    
    return 0;
}
//}

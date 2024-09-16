/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated January 1, 2020. Replaces all prior versions.
 *
 * Copyright (c) 2013-2020, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software
 * or otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THE SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#ifndef SPINE_AXMOL_H_
#define SPINE_AXMOL_H_

#include <spine/spine.h>
#include "axmol.h"
#include <spine/Cocos2dAttachmentLoader.h>
#include <spine/SkeletonRenderer.h>
#include <spine/SkeletonBatch.h>
#include <spine/SkeletonTwoColorBatch.h>

#include <spine/SkeletonAnimation.h>

#define AX_SPINE_VERSION 0x030600
#define AX_USE_SPINE_CPP 0

namespace spine {
    enum EventType
    {
        EventType_Start = spEventType::SP_ANIMATION_START,
        EventType_Interrupt = spEventType::SP_ANIMATION_INTERRUPT,
        EventType_End = spEventType::SP_ANIMATION_END,
        EventType_Complete = spEventType::SP_ANIMATION_COMPLETE,
        EventType_Dispose = spEventType::SP_ANIMATION_DISPOSE,
        EventType_Event = spEventType::SP_ANIMATION_EVENT
    };
    using Event = spEvent;
    using TrackEntry = spTrackEntry;
    using Animation = spAnimation;
    using Atlas = spAtlas;
    using Skeleton = spSkeleton;
    using SkeletonData = spSkeletonData;
	typedef ax::Texture2D* (*CustomTextureLoader)(const char* path);
	// set custom texture loader for _spAtlasPage_createTexture
	void spAtlasPage_setCustomTextureLoader(CustomTextureLoader texLoader);
}

#endif /* SPINE_COCOS2DX_H_ */

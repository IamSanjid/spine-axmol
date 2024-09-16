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

#include <spine/spine-axmol.h>
#include <spine/extension.h>
 //#include <spine/SkeletonBatch.h>
 //#include <spine/SkeletonTwoColorBatch.h>
#include <spine/AttachmentVertices.h>
#include <spine/Cocos2dAttachmentLoader.h>
#include <algorithm>

#define INITIAL_WORLD_VERTICES_LENGTH 1000
// Used for transforming attachments for bounding boxes & debug rendering
static float* worldVertices = nullptr;
static size_t worldVerticesLength = 0;

void ensureWorldVerticesCapacity(size_t capacity) {
    if (worldVerticesLength < capacity) {
        float* newWorldVertices = new float[capacity];
        memcpy(newWorldVertices, worldVertices, capacity * sizeof(float));
        delete[] worldVertices;
        worldVertices = newWorldVertices;
        worldVerticesLength = capacity;
    }
}
using namespace ax;

namespace spine {

    namespace {
        // Cocos2dTextureLoader textureLoader;

        int computeTotalCoordCount(spSkeleton& skeleton, int startSlotIndex, int endSlotIndex);
        ax::Rect computeBoundingRect(const float* coords, int vertexCount);
        void interleaveCoordinates(float* dst, const float* src, int vertexCount, int dstStride);
        BlendFunc makeBlendFunc(spBlendMode blendMode, bool premultipliedAlpha);
        void transformWorldVertices(float* dstCoord, int coordCount, spSkeleton& skeleton, int startSlotIndex, int endSlotIndex);
        bool cullRectangle(Renderer* renderer, const Mat4& transform, const ax::Rect& rect);
        Color4B ColorToColor4B(const spColor& color);
        bool slotIsOutRange(spSlot& slot, int startSlotIndex, int endSlotIndex);
        // bool nothingToDraw(spSlot& slot, int startSlotIndex, int endSlotIndex);
    }

    // C Variable length array
#ifdef _MSC_VER
// VLA not supported, use _malloca
#define VLA(type, arr, count) \
	type* arr = static_cast<type*>( _malloca(sizeof(type) * count) )
#define VLA_FREE(arr) do { _freea(arr); } while(false)
#else
#define VLA(type, arr, count) \
	type arr[count]
#define VLA_FREE(arr)
#endif

    SkeletonRenderer* SkeletonRenderer::createWithSkeleton(spSkeleton* skeleton, bool ownsSkeleton, bool ownsSkeletonData) {
        SkeletonRenderer* node = new SkeletonRenderer(skeleton, ownsSkeleton, ownsSkeletonData);
        node->autorelease();
        return node;
    }

    SkeletonRenderer* SkeletonRenderer::createWithData(spSkeletonData* skeletonData, bool ownsSkeletonData) {
        SkeletonRenderer* node = new SkeletonRenderer(skeletonData, ownsSkeletonData);
        node->autorelease();
        return node;
    }

    SkeletonRenderer* SkeletonRenderer::createWithFile(const std::string& skeletonDataFile, spAtlas* atlas, float scale) {
        SkeletonRenderer* node = new SkeletonRenderer(skeletonDataFile, atlas, scale);
        node->autorelease();
        return node;
    }

    SkeletonRenderer* SkeletonRenderer::createWithFile(const std::string& skeletonDataFile, const std::string& atlasFile, float scale) {
        SkeletonRenderer* node = new SkeletonRenderer(skeletonDataFile, atlasFile, scale);
        node->autorelease();
        return node;
    }

    void SkeletonRenderer::initialize() {
        _clipper = spSkeletonClipping_create();

        _blendFunc = BlendFunc::ALPHA_PREMULTIPLIED;
        setOpacityModifyRGB(true);

        setTwoColorTint(false);

        spSkeleton_setToSetupPose(_skeleton);
        spSkeleton_updateWorldTransform(_skeleton);
    }

    void SkeletonRenderer::setupGLProgramState(bool /*twoColorTintEnabled*/) {}

    void SkeletonRenderer::setSkeletonData(spSkeletonData* skeletonData, bool ownsSkeletonData) {
        _skeleton = spSkeleton_create(skeletonData);
        _ownsSkeletonData = ownsSkeletonData;
    }

    SkeletonRenderer::SkeletonRenderer()
        : _atlas(nullptr), _attachmentLoader(nullptr), _debugSlots(false), _debugBones(false), _debugMeshes(false), _timeScale(1), _effect(nullptr), _startSlotIndex(-1), _endSlotIndex(-1) {
    }

    SkeletonRenderer::SkeletonRenderer(spSkeleton* skeleton, bool ownsSkeleton, bool ownsSkeletonData)
        : _atlas(nullptr), _attachmentLoader(nullptr), _debugSlots(false), _debugBones(false), _debugMeshes(false), _timeScale(1), _effect(nullptr), _startSlotIndex(-1), _endSlotIndex(-1) {
        initWithSkeleton(skeleton, ownsSkeleton, ownsSkeletonData);
    }

    SkeletonRenderer::SkeletonRenderer(spSkeletonData* skeletonData, bool ownsSkeletonData)
        : _atlas(nullptr), _attachmentLoader(nullptr), _debugSlots(false), _debugBones(false), _debugMeshes(false), _timeScale(1), _effect(nullptr), _startSlotIndex(-1), _endSlotIndex(-1) {
        initWithData(skeletonData, ownsSkeletonData);
    }

    SkeletonRenderer::SkeletonRenderer(const std::string& skeletonDataFile, spAtlas* atlas, float scale)
        : _atlas(nullptr), _attachmentLoader(nullptr), _debugSlots(false), _debugBones(false), _debugMeshes(false), _timeScale(1), _effect(nullptr), _startSlotIndex(-1), _endSlotIndex(-1) {
        initWithJsonFile(skeletonDataFile, atlas, scale);
    }

    SkeletonRenderer::SkeletonRenderer(const std::string& skeletonDataFile, const std::string& atlasFile, float scale)
        : _atlas(nullptr), _attachmentLoader(nullptr), _debugSlots(false), _debugBones(false), _debugMeshes(false), _timeScale(1), _effect(nullptr), _startSlotIndex(-1), _endSlotIndex(-1) {
        initWithJsonFile(skeletonDataFile, atlasFile, scale);
    }

    SkeletonRenderer::~SkeletonRenderer() {
        if (_ownsSkeletonData) spSkeletonData_dispose(_skeleton->data);
        if (_ownsSkeleton) spSkeleton_dispose(_skeleton);
        if (_atlas) spAtlas_dispose(_atlas);
        if (_attachmentLoader) spAttachmentLoader_dispose(_attachmentLoader);
        spSkeletonClipping_dispose(_clipper);
    }

    void SkeletonRenderer::initWithSkeleton(spSkeleton* skeleton, bool ownsSkeleton, bool ownsSkeletonData) {
        _skeleton = skeleton;
        _ownsSkeleton = ownsSkeleton;
        _ownsSkeletonData = ownsSkeletonData;

        initialize();
    }

    void SkeletonRenderer::initWithData(spSkeletonData* skeletonData, bool ownsSkeletonData) {
        _ownsSkeleton = true;
        setSkeletonData(skeletonData, ownsSkeletonData);
        initialize();
    }

    void SkeletonRenderer::initWithJsonFile(const std::string& skeletonDataFile, spAtlas* atlas, float scale) {
        _atlas = atlas;
        _attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(_atlas));

        spSkeletonJson* json = spSkeletonJson_createWithLoader(_attachmentLoader);
        json->scale = scale;
        spSkeletonData* skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonDataFile.c_str());
        CCASSERT(skeletonData, json->error ? json->error : "Error reading skeleton data.");
        spSkeletonJson_dispose(json);

        _ownsSkeleton = true;
        setSkeletonData(skeletonData, true);

        initialize();
    }

    void SkeletonRenderer::initWithJsonFile(const std::string& skeletonDataFile, const std::string& atlasFile, float scale) {
        _atlas = spAtlas_createFromFile(atlasFile.c_str(), 0);
        CCASSERT(_atlas, "Error reading atlas file.");

        _attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(_atlas));

        spSkeletonJson* json = spSkeletonJson_createWithLoader(_attachmentLoader);
        json->scale = scale;
        spSkeletonData* skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonDataFile.c_str());
        CCASSERT(skeletonData, json->error ? json->error : "Error reading skeleton data file.");
        spSkeletonJson_dispose(json);

        _ownsSkeleton = true;
        setSkeletonData(skeletonData, true);

        initialize();
    }

    void SkeletonRenderer::initWithBinaryFile(const std::string& skeletonDataFile, spAtlas* atlas, float scale) {
        _atlas = atlas;
        _attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(_atlas));

        spSkeletonBinary* binary = spSkeletonBinary_createWithLoader(_attachmentLoader);
        binary->scale = scale;
        spSkeletonData* skeletonData = spSkeletonBinary_readSkeletonDataFile(binary, skeletonDataFile.c_str());
        CCASSERT(skeletonData, binary->error ? binary->error : "Error reading skeleton data file.");
        spSkeletonBinary_dispose(binary);
        _ownsSkeleton = true;
        setSkeletonData(skeletonData, true);

        initialize();
    }

    void SkeletonRenderer::initWithBinaryFile(const std::string& skeletonDataFile, const std::string& atlasFile, float scale) {
        _atlas = spAtlas_createFromFile(atlasFile.c_str(), 0);
        CCASSERT(_atlas, "Error reading atlas file.");

        _attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(_atlas));

        spSkeletonBinary* binary = spSkeletonBinary_createWithLoader(_attachmentLoader);
        binary->scale = scale;
        spSkeletonData* skeletonData = spSkeletonBinary_readSkeletonDataFile(binary, skeletonDataFile.c_str());
        CCASSERT(skeletonData, binary->error ? binary->error : "Error reading skeleton data file.");
        spSkeletonBinary_dispose(binary);
        _ownsSkeleton = true;
        setSkeletonData(skeletonData, true);

        initialize();
    }


    void SkeletonRenderer::update(float deltaTime) {
        Node::update(deltaTime);
        if (_ownsSkeleton) spSkeleton_update(_skeleton, deltaTime * _timeScale);
    }

    void SkeletonRenderer::draw(Renderer* renderer, const Mat4& transform, uint32_t transformFlags) {
        SkeletonBatch* batch = SkeletonBatch::getInstance();
        SkeletonTwoColorBatch* twoColorBatch = SkeletonTwoColorBatch::getInstance();
        bool isTwoColorTint = this->isTwoColorTint();

        // Early exit if the skeleton is invisible
        if (getDisplayedOpacity() == 0 || _skeleton->color.a == 0) {
            return;
        }

        if (_effect) _effect->begin(_effect, _skeleton);

        Color4F nodeColor;
        nodeColor.r = getDisplayedColor().r / (float)255;
        nodeColor.g = getDisplayedColor().g / (float)255;
        nodeColor.b = getDisplayedColor().b / (float)255;
        nodeColor.a = getDisplayedOpacity() / (float)255;

        Color4F color;
        Color4F darkColor;
        AttachmentVertices* attachmentVertices = nullptr;
        TwoColorTrianglesCommand* lastTwoColorTrianglesCommand = nullptr;
        bool inRange = _startSlotIndex != -1 || _endSlotIndex != -1 ? false : true;
        for (int i = 0, n = _skeleton->slotsCount; i < n; ++i) {
            spSlot* slot = _skeleton->drawOrder[i];

            if (_startSlotIndex >= 0 && _startSlotIndex == slot->data->index) {
                inRange = true;
            }

            if (!inRange) {
                spSkeletonClipping_clipEnd(_clipper, slot);
                continue;
            }

            if (_endSlotIndex >= 0 && _endSlotIndex == slot->data->index) {
                inRange = false;
            }

            if (!slot->attachment) {
                spSkeletonClipping_clipEnd(_clipper, slot);
                continue;
            }

            // Early exit if slot is invisible
            if (slot->color.a == 0) {
                spSkeletonClipping_clipEnd(_clipper, slot);
                continue;
            }

            ax::TrianglesCommand::Triangles triangles;
            TwoColorTriangles trianglesTwoColor;

            switch (slot->attachment->type) {
            case SP_ATTACHMENT_REGION: {
                spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
                attachmentVertices = getAttachmentVertices(attachment);

                // Early exit if attachment is invisible
                if (attachment->color.a == 0) {
                    spSkeletonClipping_clipEnd(_clipper, slot);
                    continue;
                }

                if (!isTwoColorTint) {
                    triangles.indices = attachmentVertices->_triangles->indices;
                    triangles.indexCount = attachmentVertices->_triangles->indexCount;
                    triangles.verts = batch->allocateVertices(attachmentVertices->_triangles->vertCount);
                    triangles.vertCount = attachmentVertices->_triangles->vertCount;
                    memcpy(triangles.verts, attachmentVertices->_triangles->verts, sizeof(ax::V3F_C4B_T2F) * attachmentVertices->_triangles->vertCount);
                    spRegionAttachment_computeWorldVertices(attachment, slot->bone, (float*)triangles.verts, 0, 6);
                }
                else {
                    trianglesTwoColor.indices = attachmentVertices->_triangles->indices;
                    trianglesTwoColor.indexCount = attachmentVertices->_triangles->indexCount;
                    trianglesTwoColor.verts = twoColorBatch->allocateVertices(attachmentVertices->_triangles->vertCount);
                    trianglesTwoColor.vertCount = attachmentVertices->_triangles->vertCount;
                    for (int i = 0; i < trianglesTwoColor.vertCount; i++) {
                        trianglesTwoColor.verts[i].texCoords = attachmentVertices->_triangles->verts[i].texCoords;
                    }
                    spRegionAttachment_computeWorldVertices(attachment, slot->bone, (float*)trianglesTwoColor.verts, 0, 7);
                }

                color.r = attachment->color.r;
                color.g = attachment->color.g;
                color.b = attachment->color.b;
                color.a = attachment->color.a;

                break;
            }
            case SP_ATTACHMENT_MESH: {
                spMeshAttachment* attachment = (spMeshAttachment*)slot->attachment;
                attachmentVertices = getAttachmentVertices(attachment);

                // Early exit if attachment is invisible
                if (attachment->color.a == 0) {
                    spSkeletonClipping_clipEnd(_clipper, slot);
                    continue;
                }

                if (!isTwoColorTint) {
                    triangles.indices = attachmentVertices->_triangles->indices;
                    triangles.indexCount = attachmentVertices->_triangles->indexCount;
                    triangles.verts = batch->allocateVertices(attachmentVertices->_triangles->vertCount);
                    triangles.vertCount = attachmentVertices->_triangles->vertCount;
                    memcpy(triangles.verts, attachmentVertices->_triangles->verts, sizeof(ax::V3F_C4B_T2F) * attachmentVertices->_triangles->vertCount);
                    int vertexSizeInFloats = sizeof(ax::V3F_C4B_T2F) / sizeof(float);
                    spVertexAttachment_computeWorldVertices(SUPER(attachment), slot, 0, attachment->super.worldVerticesLength, (float*)triangles.verts, 0, vertexSizeInFloats);
                }
                else {
                    trianglesTwoColor.indices = attachmentVertices->_triangles->indices;
                    trianglesTwoColor.indexCount = attachmentVertices->_triangles->indexCount;
                    trianglesTwoColor.verts = twoColorBatch->allocateVertices(attachmentVertices->_triangles->vertCount);
                    trianglesTwoColor.vertCount = attachmentVertices->_triangles->vertCount;
                    for (int i = 0; i < trianglesTwoColor.vertCount; i++) {
                        trianglesTwoColor.verts[i].texCoords = attachmentVertices->_triangles->verts[i].texCoords;
                    }
                    int vertexSizeInFloats = sizeof(V3F_C4B_C4B_T2F) / sizeof(float);
                    spVertexAttachment_computeWorldVertices(SUPER(attachment), slot, 0, attachment->super.worldVerticesLength, (float*)trianglesTwoColor.verts, 0, vertexSizeInFloats);
                }

                color.r = attachment->color.r;
                color.g = attachment->color.g;
                color.b = attachment->color.b;
                color.a = attachment->color.a;

                break;
            }
            case SP_ATTACHMENT_CLIPPING: {
                spClippingAttachment* clip = (spClippingAttachment*)slot->attachment;
                spSkeletonClipping_clipStart(_clipper, slot, clip);
                continue;
            }
            default:
                spSkeletonClipping_clipEnd(_clipper, slot);
                continue;
            }

            float alpha = nodeColor.a * _skeleton->color.a * slot->color.a * color.a * 255;
            // skip rendering if the color of this attachment is 0
            if (alpha == 0) {
                spSkeletonClipping_clipEnd(_clipper, slot);
                continue;
            }

            float multiplier = _premultipliedAlpha ? alpha : 255;

            float red = nodeColor.r * _skeleton->color.r * color.r * multiplier;
            float green = nodeColor.g * _skeleton->color.g * color.g * multiplier;
            float blue = nodeColor.b * _skeleton->color.b * color.b * multiplier;

            color.r = red * slot->color.r;
            color.g = green * slot->color.g;
            color.b = blue * slot->color.b;
            color.a = alpha;

            if (slot->darkColor) {
                darkColor.r = red * slot->darkColor->r;
                darkColor.g = green * slot->darkColor->g;
                darkColor.b = blue * slot->darkColor->b;
            }
            else {
                darkColor.r = 0;
                darkColor.g = 0;
                darkColor.b = 0;
            }
            darkColor.a = _premultipliedAlpha ? 255 : 0;

            BlendFunc blendFunc = makeBlendFunc(slot->data->blendMode, _premultipliedAlpha);
            if (!isTwoColorTint) {
                if (spSkeletonClipping_isClipping(_clipper)) {
                    spSkeletonClipping_clipTriangles(_clipper, (float*)&triangles.verts[0].vertices, triangles.vertCount * sizeof(ax::V3F_C4B_T2F) / 4, triangles.indices, triangles.indexCount, (float*)&triangles.verts[0].texCoords, 6);
                    batch->deallocateVertices(triangles.vertCount);

                    if (_clipper->clippedTriangles->size == 0) {
                        spSkeletonClipping_clipEnd(_clipper, slot);
                        continue;
                    }

                    triangles.vertCount = _clipper->clippedVertices->size >> 1;
                    triangles.verts = batch->allocateVertices(triangles.vertCount);
                    triangles.indexCount = _clipper->clippedTriangles->size;
                    triangles.indices = batch->allocateIndices(triangles.indexCount);
                    memcpy(triangles.indices, _clipper->clippedTriangles->items, sizeof(unsigned short) * _clipper->clippedTriangles->size);

                    float* verts = _clipper->clippedVertices->items;
                    float* uvs = _clipper->clippedUVs->items;
                    if (_effect) {
                        spColor light;
                        spColor dark;
                        light.r = color.r / 255.0f;
                        light.g = color.g / 255.0f;
                        light.b = color.b / 255.0f;
                        light.a = color.a / 255.0f;
                        dark.r = dark.g = dark.b = dark.a = 0;
                        for (int v = 0, vn = triangles.vertCount, vv = 0; v < vn; ++v, vv += 2) {
                            V3F_C4B_T2F* vertex = triangles.verts + v;
                            spColor lightCopy = light;
                            spColor darkCopy = dark;
                            vertex->vertices.x = verts[vv];
                            vertex->vertices.y = verts[vv + 1];
                            vertex->texCoords.u = uvs[vv];
                            vertex->texCoords.v = uvs[vv + 1];
                            _effect->transform(_effect, &vertex->vertices.x, &vertex->vertices.y, &vertex->texCoords.u, &vertex->texCoords.v, &lightCopy, &darkCopy);
                            vertex->colors.r = (uint8_t)(lightCopy.r * 255);
                            vertex->colors.g = (uint8_t)(lightCopy.g * 255);
                            vertex->colors.b = (uint8_t)(lightCopy.b * 255);
                            vertex->colors.a = (uint8_t)(lightCopy.a * 255);
                        }
                    }
                    else {
                        for (int v = 0, vn = triangles.vertCount, vv = 0; v < vn; ++v, vv += 2) {
                            V3F_C4B_T2F* vertex = triangles.verts + v;
                            vertex->vertices.x = verts[vv];
                            vertex->vertices.y = verts[vv + 1];
                            vertex->texCoords.u = uvs[vv];
                            vertex->texCoords.v = uvs[vv + 1];
                            vertex->colors.r = (uint8_t)color.r;
                            vertex->colors.g = (uint8_t)color.g;
                            vertex->colors.b = (uint8_t)color.b;
                            vertex->colors.a = (uint8_t)color.a;
                        }
                    }
                    batch->addCommand(renderer, _globalZOrder, attachmentVertices->_texture, _programState, blendFunc, triangles, transform, transformFlags);
                }
                else {

                    if (_effect) {
                        spColor light;
                        spColor dark;
                        light.r = color.r / 255.0f;
                        light.g = color.g / 255.0f;
                        light.b = color.b / 255.0f;
                        light.a = color.a / 255.0f;
                        dark.r = dark.g = dark.b = dark.a = 0;
                        for (int v = 0, vn = triangles.vertCount; v < vn; ++v) {
                            V3F_C4B_T2F* vertex = triangles.verts + v;
                            spColor lightCopy = light;
                            spColor darkCopy = dark;
                            _effect->transform(_effect, &vertex->vertices.x, &vertex->vertices.y, &vertex->texCoords.u, &vertex->texCoords.v, &lightCopy, &darkCopy);
                            vertex->colors.r = (uint8_t)(lightCopy.r * 255);
                            vertex->colors.g = (uint8_t)(lightCopy.g * 255);
                            vertex->colors.b = (uint8_t)(lightCopy.b * 255);
                            vertex->colors.a = (uint8_t)(lightCopy.a * 255);
                        }
                    }
                    else {
                        for (int v = 0, vn = triangles.vertCount; v < vn; ++v) {
                            V3F_C4B_T2F* vertex = triangles.verts + v;
                            vertex->colors.r = (uint8_t)color.r;
                            vertex->colors.g = (uint8_t)color.g;
                            vertex->colors.b = (uint8_t)color.b;
                            vertex->colors.a = (uint8_t)color.a;
                        }
                    }
                    batch->addCommand(renderer, _globalZOrder, attachmentVertices->_texture, _programState, blendFunc, triangles, transform, transformFlags);
                }
            }
            else {
                if (spSkeletonClipping_isClipping(_clipper)) {
                    spSkeletonClipping_clipTriangles(_clipper, (float*)&trianglesTwoColor.verts[0].position, trianglesTwoColor.vertCount * sizeof(V3F_C4B_C4B_T2F) / 4, trianglesTwoColor.indices, trianglesTwoColor.indexCount, (float*)&trianglesTwoColor.verts[0].texCoords, 7);
                    twoColorBatch->deallocateVertices(trianglesTwoColor.vertCount);

                    if (_clipper->clippedTriangles->size == 0) {
                        spSkeletonClipping_clipEnd(_clipper, slot);
                        continue;
                    }

                    trianglesTwoColor.vertCount = _clipper->clippedVertices->size >> 1;
                    trianglesTwoColor.verts = twoColorBatch->allocateVertices(trianglesTwoColor.vertCount);
                    trianglesTwoColor.indexCount = _clipper->clippedTriangles->size;
                    trianglesTwoColor.indices = twoColorBatch->allocateIndices(trianglesTwoColor.indexCount);
                    memcpy(trianglesTwoColor.indices, _clipper->clippedTriangles->items, sizeof(unsigned short) * _clipper->clippedTriangles->size);

                    float* verts = _clipper->clippedVertices->items;
                    float* uvs = _clipper->clippedUVs->items;

                    if (_effect) {
                        spColor light;
                        spColor dark;
                        light.r = color.r / 255.0f;
                        light.g = color.g / 255.0f;
                        light.b = color.b / 255.0f;
                        light.a = color.a / 255.0f;
                        dark.r = darkColor.r / 255.0f;
                        dark.g = darkColor.g / 255.0f;
                        dark.b = darkColor.b / 255.0f;
                        // dark.a = darkColor.a / 255.0f;
                        for (int v = 0, vn = trianglesTwoColor.vertCount, vv = 0; v < vn; ++v, vv += 2) {
                            V3F_C4B_C4B_T2F* vertex = trianglesTwoColor.verts + v;
                            spColor lightCopy = light;
                            spColor darkCopy = dark;
                            vertex->position.x = verts[vv];
                            vertex->position.y = verts[vv + 1];
                            vertex->texCoords.u = uvs[vv];
                            vertex->texCoords.v = uvs[vv + 1];
                            _effect->transform(_effect, &vertex->position.x, &vertex->position.y, &vertex->texCoords.u, &vertex->texCoords.v, &lightCopy, &darkCopy);
                            vertex->color.r = (uint8_t)(lightCopy.r * 255);
                            vertex->color.g = (uint8_t)(lightCopy.g * 255);
                            vertex->color.b = (uint8_t)(lightCopy.b * 255);
                            vertex->color.a = (uint8_t)(lightCopy.a * 255);
                            vertex->color2.r = (uint8_t)(darkCopy.r * 255);
                            vertex->color2.g = (uint8_t)(darkCopy.g * 255);
                            vertex->color2.b = (uint8_t)(darkCopy.b * 255);
                            vertex->color2.a = (uint8_t)darkColor.a;
                        }
                    }
                    else {
                        for (int v = 0, vn = trianglesTwoColor.vertCount, vv = 0; v < vn; ++v, vv += 2) {
                            V3F_C4B_C4B_T2F* vertex = trianglesTwoColor.verts + v;
                            vertex->position.x = verts[vv];
                            vertex->position.y = verts[vv + 1];
                            vertex->texCoords.u = uvs[vv];
                            vertex->texCoords.v = uvs[vv + 1];
                            vertex->color.r = (uint8_t)color.r;
                            vertex->color.g = (uint8_t)color.g;
                            vertex->color.b = (uint8_t)color.b;
                            vertex->color.a = (uint8_t)color.a;
                            vertex->color2.r = (uint8_t)darkColor.r;
                            vertex->color2.g = (uint8_t)darkColor.g;
                            vertex->color2.b = (uint8_t)darkColor.b;
                            vertex->color2.a = (uint8_t)darkColor.a;
                        }
                    }
                    lastTwoColorTrianglesCommand = twoColorBatch->addCommand(renderer, _globalZOrder, attachmentVertices->_texture, _programState, blendFunc, trianglesTwoColor, transform, transformFlags);
                }
                else {

                    if (_effect) {
                        spColor light;
                        spColor dark;
                        light.r = color.r / 255.0f;
                        light.g = color.g / 255.0f;
                        light.b = color.b / 255.0f;
                        light.a = color.a / 255.0f;
                        dark.r = darkColor.r / 255.0f;
                        dark.g = darkColor.g / 255.0f;
                        dark.b = darkColor.b / 255.0f;
                        dark.a = darkColor.a / 255.0f;

                        for (int v = 0, vn = trianglesTwoColor.vertCount; v < vn; ++v) {
                            V3F_C4B_C4B_T2F* vertex = trianglesTwoColor.verts + v;
                            spColor lightCopy = light;
                            spColor darkCopy = dark;
                            _effect->transform(_effect, &vertex->position.x, &vertex->position.y, &vertex->texCoords.u, &vertex->texCoords.v, &lightCopy, &darkCopy);
                            vertex->color.r = (uint8_t)(lightCopy.r * 255);
                            vertex->color.g = (uint8_t)(lightCopy.g * 255);
                            vertex->color.b = (uint8_t)(lightCopy.b * 255);
                            vertex->color.a = (uint8_t)(lightCopy.a * 255);
                            vertex->color2.r = (uint8_t)(darkCopy.r * 255);
                            vertex->color2.g = (uint8_t)(darkCopy.g * 255);
                            vertex->color2.b = (uint8_t)(darkCopy.b * 255);
                            // vertex->color2.a = (uint8_t)darkColor.a;
                        }
                    }
                    else {
                        for (int v = 0, vn = trianglesTwoColor.vertCount; v < vn; ++v) {
                            V3F_C4B_C4B_T2F* vertex = trianglesTwoColor.verts + v;
                            vertex->color.r = (uint8_t)color.r;
                            vertex->color.g = (uint8_t)color.g;
                            vertex->color.b = (uint8_t)color.b;
                            vertex->color.a = (uint8_t)color.a;
                            vertex->color2.r = (uint8_t)darkColor.r;
                            vertex->color2.g = (uint8_t)darkColor.g;
                            vertex->color2.b = (uint8_t)darkColor.b;
                            vertex->color2.a = (uint8_t)darkColor.a;
                        }
                    }
                    lastTwoColorTrianglesCommand = twoColorBatch->addCommand(renderer, _globalZOrder, attachmentVertices->_texture, _programState, blendFunc, trianglesTwoColor, transform, transformFlags);
                }
            }
            spSkeletonClipping_clipEnd(_clipper, slot);
        }
        spSkeletonClipping_clipEnd2(_clipper);

        if (lastTwoColorTrianglesCommand) {
            Node* parent = this->getParent();

            // We need to decide if we can postpone flushing the current
            // batch. We can postpone if the next sibling node is a
            // two color tinted skeleton with the same global-z.
            // The parent->getChildrenCount() > 100 check is a hack
            // as checking for a sibling is an O(n) operation, and if
            // all children of this nodes parent are skeletons, we
            // are in O(n2) territory.
            if (!parent || parent->getChildrenCount() > 100 || getChildrenCount() != 0) {
                lastTwoColorTrianglesCommand->setForceFlush(true);
            }
            else {
                Vector<Node*>& children = parent->getChildren();
                Node* sibling = nullptr;
                for (ssize_t i = 0; i < children.size(); i++) {
                    if (children.at(i) == this) {
                        if (i < children.size() - 1) {
                            sibling = children.at(i + 1);
                            break;
                        }
                    }
                }
                if (!sibling) {
                    lastTwoColorTrianglesCommand->setForceFlush(true);
                }
                else {
                    SkeletonRenderer* siblingSkeleton = dynamic_cast<SkeletonRenderer*>(sibling);
                    if (!siblingSkeleton || // flush is next sibling isn't a SkeletonRenderer
                        !siblingSkeleton->isTwoColorTint() || // flush if next sibling isn't two color tinted
                        !siblingSkeleton->isVisible() || // flush if next sibling is two color tinted but not visible
                        (siblingSkeleton->getGlobalZOrder() != this->getGlobalZOrder())) { // flush if next sibling is two color tinted but z-order differs
                        lastTwoColorTrianglesCommand->setForceFlush(true);
                    }
                }
            }
        }

        if (_effect) _effect->end(_effect);

        if (_debugSlots || _debugBones || _debugMeshes) {
            drawDebug(renderer, transform, transformFlags);
        }
    }


    void SkeletonRenderer::drawDebug(Renderer* renderer, const Mat4& transform, uint32_t transformFlags) {

#if !defined(USE_MATRIX_STACK_PROJECTION_ONLY)
        Director* director = Director::getInstance();
        director->pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
        director->loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW, transform);
#endif

        DrawNode* drawNode = DrawNode::create();
        drawNode->setGlobalZOrder(getGlobalZOrder());

        // Draw bounding rectangle
        if (_debugBoundingRect) {
#if COCOS2D_VERSION < 0x00040000
            glLineWidth(2);
#else
            drawNode->setLineWidth(2.0f);
#endif
            const ax::Rect brect = getBoundingBox();
            const Vec2 points[4] =
            {
                brect.origin,
                { brect.origin.x + brect.size.width, brect.origin.y },
                { brect.origin.x + brect.size.width, brect.origin.y + brect.size.height },
                { brect.origin.x, brect.origin.y + brect.size.height }
            };
            drawNode->drawPoly(points, 4, true, Color4F::GREEN);
        }

        if (_debugSlots) {
            // Slots.
            // DrawPrimitives::setDrawColor4B(0, 0, 255, 255);
#if COCOS2D_VERSION < 0x00040000
            glLineWidth(2);
#else
            drawNode->setLineWidth(2.0f);
#endif
            Vec2 points[4];
            V3F_C4B_T2F_Quad quad;
            for (int i = 0, n = _skeleton->slotsCount; i < n; i++) {
                spSlot* slot = _skeleton->drawOrder[i];
                if (!slot->attachment || slot->attachment->type != SP_ATTACHMENT_REGION) continue;
                spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
                spRegionAttachment_computeWorldVertices(attachment, slot->bone, worldVertices, 0, 2);
                points[0] = Vec2(worldVertices[0], worldVertices[1]);
                points[1] = Vec2(worldVertices[2], worldVertices[3]);
                points[2] = Vec2(worldVertices[4], worldVertices[5]);
                points[3] = Vec2(worldVertices[6], worldVertices[7]);
                drawNode->drawPoly(points, 4, true, Color4F::BLUE);
            }
        }

        if (_debugBones) {
            // Bone lengths.
#if COCOS2D_VERSION < 0x00040000
            glLineWidth(2);
#else
            drawNode->setLineWidth(2.0f);
#endif
            for (int i = 0, n = _skeleton->bonesCount; i < n; i++) {
                spBone* bone = _skeleton->bones[i];
                float x = bone->data->length * bone->a + bone->worldX;
                float y = bone->data->length * bone->c + bone->worldY;
                drawNode->drawLine(Vec2(bone->worldX, bone->worldY), Vec2(x, y), Color4F::RED);
            }
            // Bone origins.
            auto color = Color4F::BLUE; // Root bone is blue.
            for (int i = 0, n = _skeleton->bonesCount; i < n; i++) {
                spBone* bone = _skeleton->bones[i];
                drawNode->drawPoint(Vec2(bone->worldX, bone->worldY), 4, color);
                if (i == 0) color = Color4F::GREEN;
            }
        }

        if (_debugMeshes) {
            // Meshes.
#if COCOS2D_VERSION < 0x00040000
            glLineWidth(2);
#else
            drawNode->setLineWidth(2.0f);
#endif
            for (int i = 0, n = _skeleton->slotsCount; i < n; ++i) {
                spSlot* slot = _skeleton->drawOrder[i];
                if (!slot->attachment || slot->attachment->type != SP_ATTACHMENT_MESH) continue;
                spMeshAttachment* attachment = (spMeshAttachment*)slot->attachment;
                ensureWorldVerticesCapacity(attachment->super.worldVerticesLength);
                spVertexAttachment_computeWorldVertices(SUPER(attachment), slot, 0, attachment->super.worldVerticesLength, worldVertices, 0, 2);
                for (int ii = 0; ii < attachment->trianglesCount;) {
                    Vec2 v1(worldVertices + (attachment->triangles[ii++] * 2));
                    Vec2 v2(worldVertices + (attachment->triangles[ii++] * 2));
                    Vec2 v3(worldVertices + (attachment->triangles[ii++] * 2));
                    drawNode->drawLine(v1, v2, Color4F::YELLOW);
                    drawNode->drawLine(v2, v3, Color4F::YELLOW);
                    drawNode->drawLine(v3, v1, Color4F::YELLOW);
                }
            }
        }

        drawNode->draw(renderer, transform, transformFlags);
#if !defined(USE_MATRIX_STACK_PROJECTION_ONLY)
        director->popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
#endif
    }

    AttachmentVertices* SkeletonRenderer::getAttachmentVertices(spRegionAttachment* attachment) const {
        return (AttachmentVertices*)attachment->rendererObject;
    }

    AttachmentVertices* SkeletonRenderer::getAttachmentVertices(spMeshAttachment* attachment) const {
        return (AttachmentVertices*)attachment->rendererObject;
    }

    ax::Rect SkeletonRenderer::getBoundingBox() const {
        float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
        float scaleX = getScaleX(), scaleY = getScaleY();
        for (int i = 0; i < _skeleton->slotsCount; ++i) {
            spSlot* slot = _skeleton->slots[i];
            if (!slot->attachment) continue;
            int verticesCount;
            if (slot->attachment->type == SP_ATTACHMENT_REGION) {
                spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
                spRegionAttachment_computeWorldVertices(attachment, slot->bone, worldVertices, 0, 2);
                verticesCount = 8;
            }
            else if (slot->attachment->type == SP_ATTACHMENT_MESH) {
                spMeshAttachment* mesh = (spMeshAttachment*)slot->attachment;
                ensureWorldVerticesCapacity(mesh->super.worldVerticesLength);
                spVertexAttachment_computeWorldVertices(SUPER(mesh), slot, 0, mesh->super.worldVerticesLength, worldVertices, 0, 2);
                verticesCount = mesh->super.worldVerticesLength;
            }
            else
                continue;
            for (int ii = 0; ii < verticesCount; ii += 2) {
                float x = worldVertices[ii] * scaleX, y = worldVertices[ii + 1] * scaleY;
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
        Vec2 position = getPosition();
        if (minX == FLT_MAX) minX = minY = maxX = maxY = 0;
        return Rect(position.x + minX, position.y + minY, maxX - minX, maxY - minY);
    }

    // --- Convenience methods for Skeleton_* functions.

    void SkeletonRenderer::updateWorldTransform() {
        spSkeleton_updateWorldTransform(_skeleton);
    }

    void SkeletonRenderer::setToSetupPose() {
        spSkeleton_setToSetupPose(_skeleton);
    }
    void SkeletonRenderer::setBonesToSetupPose() {
        spSkeleton_setBonesToSetupPose(_skeleton);
    }
    void SkeletonRenderer::setSlotsToSetupPose() {
        spSkeleton_setSlotsToSetupPose(_skeleton);
    }

    spBone* SkeletonRenderer::findBone(const std::string& boneName) const {
        return spSkeleton_findBone(_skeleton, boneName.c_str());
    }

    spSlot* SkeletonRenderer::findSlot(const std::string& slotName) const {
        return spSkeleton_findSlot(_skeleton, slotName.c_str());
    }

    bool SkeletonRenderer::setSkin(const std::string& skinName) {
        return spSkeleton_setSkinByName(_skeleton, skinName.empty() ? 0 : skinName.c_str()) ? true : false;
    }
    bool SkeletonRenderer::setSkin(const char* skinName) {
        return spSkeleton_setSkinByName(_skeleton, skinName) ? true : false;
    }

    spAttachment* SkeletonRenderer::getAttachment(const std::string& slotName, const std::string& attachmentName) const {
        return spSkeleton_getAttachmentForSlotName(_skeleton, slotName.c_str(), attachmentName.c_str());
    }
    bool SkeletonRenderer::setAttachment(const std::string& slotName, const std::string& attachmentName) {
        return spSkeleton_setAttachment(_skeleton, slotName.c_str(), attachmentName.empty() ? 0 : attachmentName.c_str()) ? true : false;
    }
    bool SkeletonRenderer::setAttachment(const std::string& slotName, const char* attachmentName) {
        return spSkeleton_setAttachment(_skeleton, slotName.c_str(), attachmentName) ? true : false;
    }

    void SkeletonRenderer::setTwoColorTint(bool enabled) {
        setupGLProgramState(enabled);
    }

    bool SkeletonRenderer::isTwoColorTint() {
        return false;
    }

    void SkeletonRenderer::setVertexEffect(spVertexEffect* effect) {
        this->_effect = effect;
    }

    void SkeletonRenderer::setSlotsRange(int startSlotIndex, int endSlotIndex) {
        this->_startSlotIndex = startSlotIndex;
        this->_endSlotIndex = endSlotIndex;
    }

    spSkeleton* SkeletonRenderer::getSkeleton() const {
        return _skeleton;
    }

    void SkeletonRenderer::setTimeScale(float scale) {
        _timeScale = scale;
    }
    float SkeletonRenderer::getTimeScale() const {
        return _timeScale;
    }

    void SkeletonRenderer::setDebugSlotsEnabled(bool enabled) {
        _debugSlots = enabled;
    }
    bool SkeletonRenderer::getDebugSlotsEnabled() const {
        return _debugSlots;
    }

    void SkeletonRenderer::setDebugBonesEnabled(bool enabled) {
        _debugBones = enabled;
    }
    bool SkeletonRenderer::getDebugBonesEnabled() const {
        return _debugBones;
    }

    void SkeletonRenderer::setDebugMeshesEnabled(bool enabled) {
        _debugMeshes = enabled;
    }
    bool SkeletonRenderer::getDebugMeshesEnabled() const {
        return _debugMeshes;
    }


    void SkeletonRenderer::onEnter() {
#if CC_ENABLE_SCRIPT_BINDING && COCOS2D_VERSION < 0x00040000
        if (_scriptType == kScriptTypeJavascript && ScriptEngineManager::sendNodeEventToJSExtended(this, kNodeOnEnter)) return;
#endif
        Node::onEnter();
        scheduleUpdate();
    }

    void SkeletonRenderer::onExit() {
#if CC_ENABLE_SCRIPT_BINDING && COCOS2D_VERSION < 0x00040000
        if (_scriptType == kScriptTypeJavascript && ScriptEngineManager::sendNodeEventToJSExtended(this, kNodeOnExit)) return;
#endif
        Node::onExit();
        unscheduleUpdate();
    }

    // --- CCBlendProtocol

    const BlendFunc& SkeletonRenderer::getBlendFunc() const {
        return _blendFunc;
    }

    void SkeletonRenderer::setBlendFunc(const BlendFunc& blendFunc) {
        _blendFunc = blendFunc;
    }

    void SkeletonRenderer::setOpacityModifyRGB(bool value) {
        _premultipliedAlpha = value;
    }

    bool SkeletonRenderer::isOpacityModifyRGB() const {
        return _premultipliedAlpha;
    }

    namespace {
        ax::Rect computeBoundingRect(const float* coords, int vertexCount) {
            assert(coords);
            assert(vertexCount > 0);

            const float* v = coords;
            float minX = v[0];
            float minY = v[1];
            float maxX = minX;
            float maxY = minY;
            for (int i = 1; i < vertexCount; ++i) {
                v += 2;
                float x = v[0];
                float y = v[1];
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
            return { minX, minY, maxX - minX, maxY - minY };
        }

        bool slotIsOutRange(spSlot& slot, int startSlotIndex, int endSlotIndex) {
            const int index = slot.data->index;
            return startSlotIndex > index || endSlotIndex < index;
        }

#if 0
        bool nothingToDraw(spSlot& slot, int startSlotIndex, int endSlotIndex) {
            Attachment* attachment = slot.getAttachment();
            if (!attachment ||
                slotIsOutRange(slot, startSlotIndex, endSlotIndex) ||
                !slot.getBone().isActive() ||
                slot.getColor().a == 0)
                return true;
            if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
                if (static_cast<RegionAttachment*>(attachment)->getColor().a == 0)
                    return true;
            }
            else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
                if (static_cast<MeshAttachment*>(attachment)->getColor().a == 0)
                    return true;
            }
            return false;
        }

        int computeTotalCoordCount(spSkeleton& skeleton, int startSlotIndex, int endSlotIndex) {
            int coordCount = 0;
            for (size_t i = 0; i < skeleton.getSlots().size(); ++i) {
                Slot& slot = *skeleton.getSlots()[i];
                if (nothingToDraw(slot, startSlotIndex, endSlotIndex)) {
                    continue;
                }
                Attachment* const attachment = slot.getAttachment();
                if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
                    coordCount += 8;
                }
                else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
                    MeshAttachment* const mesh = static_cast<MeshAttachment*>(attachment);
                    coordCount += mesh->getWorldVerticesLength();
                }
            }
            return coordCount;
        }


        void transformWorldVertices(float* dstCoord, int coordCount, spSkeleton& skeleton, int startSlotIndex, int endSlotIndex) {
            float* dstPtr = dstCoord;
#ifndef NDEBUG
            float* const dstEnd = dstCoord + coordCount;
#endif
            for (size_t i = 0; i < skeleton.getSlots().size(); ++i) {
                /*const*/ Slot& slot = *skeleton.getDrawOrder()[i]; // match the draw order of SkeletonRenderer::Draw
                if (nothingToDraw(slot, startSlotIndex, endSlotIndex)) {
                    continue;
                }
                Attachment* const attachment = slot.getAttachment();
                if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
                    RegionAttachment* const regionAttachment = static_cast<RegionAttachment*>(attachment);
                    assert(dstPtr + 8 <= dstEnd);
                    regionAttachment->computeWorldVertices(slot.getBone(), dstPtr, 0, 2);
                    dstPtr += 8;
                }
                else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
                    MeshAttachment* const mesh = static_cast<MeshAttachment*>(attachment);
                    assert(dstPtr + mesh->getWorldVerticesLength() <= dstEnd);
                    mesh->computeWorldVertices(slot, 0, mesh->getWorldVerticesLength(), dstPtr, 0, 2);
                    dstPtr += mesh->getWorldVerticesLength();
                }
            }
            assert(dstPtr == dstEnd);
        }

        void interleaveCoordinates(float* dst, const float* src, int count, int dstStride) {
            if (dstStride == 2) {
                memcpy(dst, src, sizeof(float) * count * 2);
            }
            else {
                for (int i = 0; i < count; ++i) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst += dstStride;
                    src += 2;
                }
            }

        }
#endif

        BlendFunc makeBlendFunc(spBlendMode blendMode, bool premultipliedAlpha) {
            BlendFunc blendFunc;

#if COCOS2D_VERSION < 0x00040000
            switch (blendMode) {
            case SP_BLEND_MODE_ADDITIVE:
                blendFunc.src = premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA;
                blendFunc.dst = GL_ONE;
                break;
            case SP_BLEND_MODE_MULTIPLY:
                blendFunc.src = GL_DST_COLOR;
                blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
                break;
            case SP_BLEND_MODE_SCREEN:
                blendFunc.src = premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA;
                blendFunc.dst = GL_ONE_MINUS_SRC_COLOR;
                break;
            default:
                blendFunc.src = premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA;
                blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
                break;
            }
#else
            switch (blendMode) {
            case SP_BLEND_MODE_ADDITIVE:
                blendFunc.src = premultipliedAlpha ? backend::BlendFactor::ONE : backend::BlendFactor::SRC_ALPHA;
                blendFunc.dst = backend::BlendFactor::ONE;
                break;
            case SP_BLEND_MODE_MULTIPLY:
                blendFunc.src = backend::BlendFactor::DST_COLOR;
                blendFunc.dst = backend::BlendFactor::ONE_MINUS_SRC_ALPHA;
                break;
            case SP_BLEND_MODE_SCREEN:
                blendFunc.src = backend::BlendFactor::ONE;
                blendFunc.dst = backend::BlendFactor::ONE_MINUS_SRC_COLOR;
                break;
            default:
                blendFunc.src = premultipliedAlpha ? backend::BlendFactor::ONE : backend::BlendFactor::SRC_ALPHA;
                blendFunc.dst = backend::BlendFactor::ONE_MINUS_SRC_ALPHA;
            }
#endif
            return blendFunc;
        }


        bool cullRectangle(Renderer* renderer, const Mat4& transform, const ax::Rect& rect) {
            if (Camera::getVisitingCamera() == nullptr)
                return false;

            auto director = Director::getInstance();
            auto scene = director->getRunningScene();

            if (!scene || (scene && Camera::getDefaultCamera() != Camera::getVisitingCamera()))
                return false;

            Rect visibleRect(director->getVisibleOrigin(), director->getVisibleSize());

            // transform center point to screen space
            float hSizeX = rect.size.width / 2;
            float hSizeY = rect.size.height / 2;
            Vec3 v3p(rect.origin.x + hSizeX, rect.origin.y + hSizeY, 0);
            transform.transformPoint(&v3p);
            Vec2 v2p = Camera::getVisitingCamera()->projectGL(v3p);

            // convert content size to world coordinates
            float wshw = std::max(fabsf(hSizeX * transform.m[0] + hSizeY * transform.m[4]), fabsf(hSizeX * transform.m[0] - hSizeY * transform.m[4]));
            float wshh = std::max(fabsf(hSizeX * transform.m[1] + hSizeY * transform.m[5]), fabsf(hSizeX * transform.m[1] - hSizeY * transform.m[5]));

            // enlarge visible rect half size in screen coord
            visibleRect.origin.x -= wshw;
            visibleRect.origin.y -= wshh;
            visibleRect.size.width += wshw * 2;
            visibleRect.size.height += wshh * 2;
            return !visibleRect.containsPoint(v2p);
        }


        Color4B ColorToColor4B(const spColor& color) {
            return { (uint8_t)(color.r * 255.f), (uint8_t)(color.g * 255.f), (uint8_t)(color.b * 255.f), (uint8_t)(color.a * 255.f) };
        }
    }

}

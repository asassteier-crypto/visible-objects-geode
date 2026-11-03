#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/cocos/CCCamera.h>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
bool s_enabled = true;
float s_margin = 96.f;

struct ViewCache {
    CCNode* objectLayer = nullptr;
    unsigned int frame = 0;
    CCRect bounds;
    bool valid = false;
};

ViewCache s_viewCache;

bool pointIsFinite(CCPoint point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

CCRect visibleRectInLayer(PlayLayer* playLayer, float margin) {
    auto director = CCDirector::get();
    auto objectLayer = playLayer ? playLayer->m_objectLayer : nullptr;
    if (!director || !objectLayer)
        return {};

    auto origin = director->getVisibleOrigin();
    auto size = director->getVisibleSize();
    if (size.width <= 0.f || size.height <= 0.f)
        return {};

    // Geometry Dash can move its Cocos camera independently of the node
    // transforms. In that case, use the camera target and the game's current
    // camera dimensions instead of assuming screen coordinates equal world
    // coordinates.
    if (auto camera = objectLayer->getCamera()) {
        float centerX = 0.f;
        float centerY = 0.f;
        float centerZ = 0.f;
        camera->getCenterXYZ(&centerX, &centerY, &centerZ);

        auto contentSize = objectLayer->getContentSize();
        const bool cameraMoved = std::abs(centerX - contentSize.width * 0.5f) > 1.f ||
                                 std::abs(centerY - contentSize.height * 0.5f) > 1.f;
        if (cameraMoved && std::isfinite(centerX) && std::isfinite(centerY)) {
            float width = playLayer->m_cameraWidth;
            float height = playLayer->m_cameraHeight;
            if (!std::isfinite(width) || width <= 0.f)
                width = size.width;
            if (!std::isfinite(height) || height <= 0.f)
                height = size.height;

            const float marginX = margin * width / size.width;
            const float marginY = margin * height / size.height;
            return CCRect(centerX - width * 0.5f - marginX,
                          centerY - height * 0.5f - marginY,
                          width + marginX * 2.f,
                          height + marginY * 2.f);
        }
    }

    const CCPoint screenCorners[] = {
        origin,
        { origin.x + size.width, origin.y },
        { origin.x, origin.y + size.height },
        { origin.x + size.width, origin.y + size.height },
    };

    float minX = INFINITY;
    float minY = INFINITY;
    float maxX = -INFINITY;
    float maxY = -INFINITY;
    for (auto corner : screenCorners) {
        auto point = objectLayer->convertToNodeSpace(corner);
        if (!pointIsFinite(point))
            return {};

        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }

    // Scale the user-facing point value into object-layer coordinates. This
    // keeps the border feeling the same at different game zoom levels.
    const float marginX = margin * (maxX - minX) / size.width;
    const float marginY = margin * (maxY - minY) / size.height;
    return CCRect(minX - marginX, minY - marginY,
                  (maxX - minX) + marginX * 2.f,
                  (maxY - minY) + marginY * 2.f);
}

CCRect getVisibleBounds(PlayLayer* playLayer) {
    auto director = CCDirector::get();
    auto objectLayer = playLayer ? playLayer->m_objectLayer : nullptr;
    if (!director || !objectLayer)
        return {};

    const auto frame = director->getTotalFrames();
    if (!s_viewCache.valid || s_viewCache.objectLayer != objectLayer || s_viewCache.frame != frame) {
        s_viewCache.objectLayer = objectLayer;
        s_viewCache.frame = frame;
        s_viewCache.bounds = visibleRectInLayer(playLayer, s_margin);
        s_viewCache.valid = true;
    }
    return s_viewCache.bounds;
}

bool rectsIntersect(CCRect const& a, CCRect const& b) {
    return a.origin.x <= b.origin.x + b.size.width &&
           a.origin.x + a.size.width >= b.origin.x &&
           a.origin.y <= b.origin.y + b.size.height &&
           a.origin.y + a.size.height >= b.origin.y;
}

bool shouldSkipVisit(GameObject* object, PlayLayer* playLayer) {
    if (!s_enabled || !playLayer || !playLayer->m_objectLayer)
        return false;

    auto objectLayer = playLayer->m_objectLayer;

    // Only cull objects that belong to the active level's object layer. This
    // leaves menus, editor objects, and unrelated GameObject nodes alone.
    bool belongsToLevel = false;
    for (auto node = static_cast<CCNode*>(object); node; node = node->getParent()) {
        if (node == objectLayer) {
            belongsToLevel = true;
            break;
        }
    }
    if (!belongsToLevel || object->isTrigger())
        return false;

    auto parent = object->getParent();
    if (!parent)
        return false;

    auto box = object->boundingBox();
    if (!std::isfinite(box.origin.x) || !std::isfinite(box.origin.y) ||
        !std::isfinite(box.size.width) || !std::isfinite(box.size.height) ||
        box.size.width <= 0.f || box.size.height <= 0.f ||
        box.size.width > 10000.f || box.size.height > 10000.f)
        return false;

    CCRect levelBounds = box;
    if (parent != objectLayer) {
        const CCPoint corners[] = {
            { box.origin.x, box.origin.y },
            { box.origin.x + box.size.width, box.origin.y },
            { box.origin.x, box.origin.y + box.size.height },
            { box.origin.x + box.size.width, box.origin.y + box.size.height },
        };

        float minX = INFINITY;
        float minY = INFINITY;
        float maxX = -INFINITY;
        float maxY = -INFINITY;
        for (auto corner : corners) {
            auto worldPoint = parent->convertToWorldSpace(corner);
            auto layerPoint = objectLayer->convertToNodeSpace(worldPoint);
            if (!pointIsFinite(layerPoint))
                return false;
            minX = std::min(minX, layerPoint.x);
            minY = std::min(minY, layerPoint.y);
            maxX = std::max(maxX, layerPoint.x);
            maxY = std::max(maxY, layerPoint.y);
        }
        levelBounds = CCRect(minX, minY, maxX - minX, maxY - minY);
    }

    auto view = getVisibleBounds(playLayer);
    if (view.size.width <= 0.f || view.size.height <= 0.f)
        return false;

    return !rectsIntersect(levelBounds, view);
}
}

$on_mod(Loaded) {
    auto mod = Mod::get();
    s_enabled = mod->getSettingValue<bool>("enabled");
    s_margin = static_cast<float>(mod->getSettingValue<int>("margin"));

    listenForSettingChanges<bool>("enabled", [](bool enabled) {
        s_enabled = enabled;
    });
    listenForSettingChanges<int>("margin", [](int margin) {
        s_margin = static_cast<float>(margin);
        s_viewCache.valid = false;
    });
}

class $modify(VisibleObjectsGameObject, GameObject) {
    struct Fields {
        bool visibilityKnown = false;
        bool desiredVisible = true;
        bool hiddenByCulling = false;
        bool applyingCulling = false;
    };

    void setVisible(bool visible) {
        auto& fields = m_fields;
        if (!fields->applyingCulling) {
            fields->visibilityKnown = true;
            fields->desiredVisible = visible;
        }
        GameObject::setVisible(visible && !fields->hiddenByCulling);
    }

    void updateTransform() {
        auto& fields = m_fields;
        if (!fields->visibilityKnown) {
            fields->visibilityKnown = true;
            fields->desiredVisible = this->isVisible();
        }

        const bool shouldCull = shouldSkipVisit(this, PlayLayer::get());
        if (fields->hiddenByCulling != shouldCull) {
            fields->hiddenByCulling = shouldCull;
            fields->applyingCulling = true;
            GameObject::setVisible(fields->desiredVisible && !shouldCull);
            fields->applyingCulling = false;
        }

        GameObject::updateTransform();
    }

    void visit() {
        if (auto playLayer = PlayLayer::get(); playLayer && shouldSkipVisit(this, playLayer))
            return;

        GameObject::visit();
    }
};

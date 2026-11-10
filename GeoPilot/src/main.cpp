// GIGA DASH BOT v1.1 — Geometry Dash 2.2081 / Geode 5.11.2
// Lightweight predictive controller. Runs in PlayLayer::postUpdate (game-frame cadence).
// It uses native object types and collision rectangles, not screen-pixel guesses.

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <cmath>
#include <string>

using namespace geode::prelude;

namespace {
    constexpr float kMinClosingSpeed = 35.f;
    constexpr float kDefaultLeadSeconds = 0.095f;
    constexpr float kTapCooldown = 0.11f;
    constexpr float kRectSafety = 2.0f;
    constexpr float kDirectionalLookAhead = 0.42f;

    struct Candidate {
        GameObject* object = nullptr;
        cocos2d::CCRect rect{};
        float gap = 0.f;
        float timeToContact = 999.f;
        float predictedPlayerY = 0.f;
        bool projectedCollision = false;
    };

    enum class PlayerMode {
        Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing
    };

    PlayerMode detectMode(PlayerObject* p) {
        if (p->m_isShip) return PlayerMode::Ship;
        if (p->m_isBall) return PlayerMode::Ball;
        if (p->m_isBird) return PlayerMode::UFO;
        if (p->m_isDart) return PlayerMode::Wave;
        if (p->m_isRobot) return PlayerMode::Robot;
        if (p->m_isSpider) return PlayerMode::Spider;
        if (p->m_isSwing) return PlayerMode::Swing;
        return PlayerMode::Cube;
    }

    const char* modeName(PlayerMode mode, bool platformer) {
        if (platformer) return "Platformer";
        switch (mode) {
            case PlayerMode::Cube: return "Cube";
            case PlayerMode::Ship: return "Ship";
            case PlayerMode::Ball: return "Ball";
            case PlayerMode::UFO: return "UFO";
            case PlayerMode::Wave: return "Wave";
            case PlayerMode::Robot: return "Robot";
            case PlayerMode::Spider: return "Spider";
            case PlayerMode::Swing: return "Swing";
        }
        return "Unknown";
    }

    bool isDirectionalMode(PlayerMode mode) {
        return mode == PlayerMode::Ship || mode == PlayerMode::Wave || mode == PlayerMode::Swing;
    }

    // Geode exposes object roles. This excludes decorative spikes, fake spikes,
    // non-hazard text/props, portals and jump pads that the old ID list misread.
    bool isCollisionObject(GameObject* object) {
        if (!object) return false;
        const auto type = object->m_objectType;
        return type == GameObjectType::Hazard
            || type == GameObjectType::AnimatedHazard
            || type == GameObjectType::Solid;
    }

    bool verticalOverlap(const cocos2d::CCRect& a, const cocos2d::CCRect& b, float margin = 0.f) {
        return a.getMaxY() >= b.getMinY() - margin
            && b.getMaxY() >= a.getMinY() - margin;
    }

    bool horizontalOverlap(const cocos2d::CCRect& a, const cocos2d::CCRect& b, float margin = 0.f) {
        return a.getMaxX() >= b.getMinX() - margin
            && b.getMaxX() >= a.getMinX() - margin;
    }

    cocos2d::CCRect shiftedRect(cocos2d::CCRect rect, float dx, float dy) {
        rect.origin.x += dx;
        rect.origin.y += dy;
        return rect;
    }

    float rectCenterY(const cocos2d::CCRect& rect) {
        return rect.getMinY() + rect.size.height * 0.5f;
    }

    float forwardGap(const cocos2d::CCRect& player, const cocos2d::CCRect& object, float direction) {
        // Correct for reverse portals as well as normal left-to-right movement.
        if (direction >= 0.f) return object.getMinX() - player.getMaxX();
        return player.getMinX() - object.getMaxX();
    }
}

class $modify(GigaDashBotPlayLayer, PlayLayer) {
    struct Fields {
        float jumpCooldown = 0.f;
        float debugCooldown = 0.f;
        bool releaseJumpNextFrame = false;
        bool directionalButtonHeld = false;
        GameObject* lastTarget = nullptr;
        int lastTargetID = -1;
        float lastGap = 0.f;
        float lastTtc = 0.f;
        const char* lastDecision = "WAIT";
    };

    void releaseController(PlayerObject* player) {
        if (!player) return;
        player->releaseButton(PlayerButton::Jump);
        player->releaseButton(PlayerButton::Right);
        player->releaseButton(PlayerButton::Left);
        m_fields->releaseJumpNextFrame = false;
        m_fields->directionalButtonHeld = false;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto mod = Mod::get();
        auto* player = m_player1;

        if (!mod->getSettingValue<bool>("auto-play")) {
            if (player && (m_fields->releaseJumpNextFrame || m_fields->directionalButtonHeld)) {
                releaseController(player);
            }
            return;
        }

        if (!player || m_isPaused || player->m_isDead) return;

        // Release a tap on the next physics frame, so each press is a clean input edge.
        if (m_fields->releaseJumpNextFrame) {
            player->releaseButton(PlayerButton::Jump);
            m_fields->releaseJumpNextFrame = false;
        }

        m_fields->jumpCooldown = std::max(0.f, m_fields->jumpCooldown - dt);
        m_fields->debugCooldown = std::max(0.f, m_fields->debugCooldown - dt);

        const bool platformer = player->m_isPlatformer;
        const auto mode = detectMode(player);
        const bool directional = isDirectionalMode(mode);
        const float leadSeconds = static_cast<float>(
            mod->getSettingValue<int64_t>("jump-lead-ms")
        ) / 1000.f;

        // Platformer requires horizontal movement; hold Right while the bot is enabled.
        if (platformer && !player->buttonDown(PlayerButton::Right)) {
            player->pushButton(PlayerButton::Right);
        }

        const cocos2d::CCRect playerRect = player->getObjectRect();
        const float playerXVelocity = static_cast<float>(player->getCurrentXVelocity());
        const float direction = playerXVelocity < -1.f ? -1.f : 1.f;
        const float speed = std::max(std::abs(playerXVelocity), platformer ? 120.f : kMinClosingSpeed);
        const float playerYVelocity = static_cast<float>(player->getYVelocity());
        const float playerCenterY = rectCenterY(playerRect);

        Candidate best{};
        float bestTtc = 999.f;

        // This is a cheap native-object scan performed each game update (usually ~60 Hz),
        // so decisions are refreshed at least every 0.1 seconds without a separate AI thread.
        geode::cocos::CCArrayExt<GameObject*> objects = m_objects;
        for (auto* object : objects) {
            if (!isCollisionObject(object) || object == player) continue;

            const cocos2d::CCRect objectRect = object->getObjectRect();
            float gap = forwardGap(playerRect, objectRect, direction);

            // Ignore objects already behind the player's rear edge.
            if (gap < -playerRect.size.width * 0.85f) continue;

            const float relativeSpeed = std::max(speed, kMinClosingSpeed);
            const float ttc = std::max(0.f, gap) / relativeSpeed;
            if (ttc > (directional ? kDirectionalLookAhead : leadSeconds + 0.16f)) continue;

            // For directional modes, predict the current vertical path to the object's X.
            // For jump modes, require intersection with the player's current standing lane.
            const float projectedY = playerCenterY + playerYVelocity * ttc;
            const float dy = directional ? projectedY - playerCenterY : 0.f;
            const cocos2d::CCRect projectedPlayer = shiftedRect(playerRect, 0.f, dy);

            // A block/spike clearly above the head (or below the feet) is not a jump target.
            // In directional flight modes, only react when the projected path intersects it.
            const bool collisionPath = directional
                ? verticalOverlap(projectedPlayer, objectRect, kRectSafety)
                : verticalOverlap(playerRect, objectRect, kRectSafety);

            if (!collisionPath) continue;

            const bool xPath = directional
                ? (gap <= speed * kDirectionalLookAhead + objectRect.size.width)
                : (gap <= speed * (leadSeconds + 0.05f) + objectRect.size.width);
            if (!xPath) continue;

            if (ttc < bestTtc) {
                bestTtc = ttc;
                best.object = object;
                best.rect = objectRect;
                best.gap = gap;
                best.timeToContact = ttc;
                best.predictedPlayerY = projectedY;
                best.projectedCollision = true;
            }
        }

        m_fields->lastTarget = best.object;
        m_fields->lastTargetID = best.object ? best.object->m_objectID : -1;
        m_fields->lastGap = best.object ? best.gap : 0.f;
        m_fields->lastTtc = best.object ? best.timeToContact : 0.f;
        m_fields->lastDecision = best.object ? "TARGET" : "CLEAR";

        if (!best.object) {
            if (directional && m_fields->directionalButtonHeld) {
                // No near collision on the projected path: release thrust and coast.
                player->releaseButton(PlayerButton::Jump);
                m_fields->directionalButtonHeld = false;
            }
            return;
        }

        if (directional) {
            // Ship/Wave/Swing: choose the vertical direction opposite the obstacle.
            // This remains an approximate controller; each of these modes has different physics.
            const float obstacleY = rectCenterY(best.rect);
            const bool needMoveUp = obstacleY > best.predictedPlayerY;
            const bool pressUp = player->m_isUpsideDown ? !needMoveUp : needMoveUp;
            if (pressUp && !m_fields->directionalButtonHeld) {
                player->pushButton(PlayerButton::Jump);
                m_fields->directionalButtonHeld = true;
                m_fields->lastDecision = "STEER UP";
            }
            else if (!pressUp && m_fields->directionalButtonHeld) {
                player->releaseButton(PlayerButton::Jump);
                m_fields->directionalButtonHeld = false;
                m_fields->lastDecision = "STEER DOWN";
            }
        }
        else {
            // Ground-based modes: only jump if the collision rectangle intersects
            // the player's standing lane and the object is inside the short launch window.
            const bool canJumpFromGround =
                player->m_isOnGround && !player->m_isOnSlope;

            // UFO may tap again in the air, but only if its projected path intersects a target.
            const bool canAirTap = mode == PlayerMode::UFO;
            const bool inLaunchWindow =
                best.timeToContact <= leadSeconds
                && best.gap <= speed * leadSeconds + 1.f;

            if ((canJumpFromGround || canAirTap) && inLaunchWindow && m_fields->jumpCooldown <= 0.f) {
                player->pushButton(PlayerButton::Jump);
                m_fields->releaseJumpNextFrame = true;
                m_fields->jumpCooldown = kTapCooldown;
                m_fields->lastDecision = "JUMP";
            }
        }

        if (mod->getSettingValue<bool>("show-debug-log") && m_fields->debugCooldown <= 0.f) {
            log::info(
                "GIGA BOT: mode={}, object={}, gap={:.1f}, ttc={:.3f}s, decision={}",
                modeName(mode, platformer),
                m_fields->lastTargetID,
                m_fields->lastGap,
                m_fields->lastTtc,
                m_fields->lastDecision
            );
            m_fields->debugCooldown = 0.25f;
        }
    }

    void onQuit() {
        releaseController(m_player1);
        PlayLayer::onQuit();
    }
};

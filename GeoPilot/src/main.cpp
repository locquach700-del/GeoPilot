// GIGA DASH BOT v1.2 — Geometry Dash 2.2081 / Geode 5.11.2
// Predictive native-object controller. Experimental heuristic, not a perfect solver.

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
    constexpr float kMinSpeed = 35.f;
    constexpr float kTapCooldown = 0.16f;
    constexpr float kMaxLookAhead = 0.72f;
    constexpr float kDirectionalLookAhead = 0.48f;
    constexpr float kGroundTolerance = 3.5f;
    constexpr float kHazardCluster = 34.f;

    enum class Mode { Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing };

    struct Target {
        GameObject* object = nullptr;
        cocos2d::CCRect rect{};
        float gap = 999999.f;
        float ttc = 999999.f;
        float predictedY = 0.f;
    };

    Mode detectMode(PlayerObject* p) {
        if (p->m_isShip) return Mode::Ship;
        if (p->m_isBall) return Mode::Ball;
        if (p->m_isBird) return Mode::UFO;
        if (p->m_isDart) return Mode::Wave;
        if (p->m_isRobot) return Mode::Robot;
        if (p->m_isSpider) return Mode::Spider;
        if (p->m_isSwing) return Mode::Swing;
        return Mode::Cube;
    }

    const char* modeName(Mode m, bool platformer) {
        if (platformer) return "Platformer";
        switch (m) {
            case Mode::Cube: return "Cube";
            case Mode::Ship: return "Ship";
            case Mode::Ball: return "Ball";
            case Mode::UFO: return "UFO";
            case Mode::Wave: return "Wave";
            case Mode::Robot: return "Robot";
            case Mode::Spider: return "Spider";
            case Mode::Swing: return "Swing";
        }
        return "Unknown";
    }

    bool directionalMode(Mode m) {
        return m == Mode::Ship || m == Mode::Wave || m == Mode::Swing;
    }

    bool hazardObject(GameObject* o) {
        return o && (o->m_objectType == GameObjectType::Hazard
            || o->m_objectType == GameObjectType::AnimatedHazard);
    }

    // GameObjectType::Solid includes the ground. Only accept a solid when its
    // rectangle rises into the player's body; a floor whose top touches the
    // player's feet is explicitly ignored.
    bool raisedSolid(GameObject* o, const cocos2d::CCRect& r,
                     const cocos2d::CCRect& p) {
        if (!o || o->m_objectType != GameObjectType::Solid || o->m_isDisabled2)
            return false;

        const float h = std::max(p.size.height, 1.f);
        const float riseAboveFeet = r.getMaxY() - p.getMinY();

        if (riseAboveFeet <= std::max(kGroundTolerance, h * 0.22f))
            return false; // floor/platform below the player's feet

        if (r.getMinY() >= p.getMaxY() - kGroundTolerance)
            return false; // ceiling well above the player's head

        return riseAboveFeet > h * 0.28f;
    }

    bool verticalOverlap(const cocos2d::CCRect& a,
                         const cocos2d::CCRect& b, float margin = 0.f) {
        return a.getMaxY() >= b.getMinY() - margin
            && b.getMaxY() >= a.getMinY() - margin;
    }

    cocos2d::CCRect offsetRect(cocos2d::CCRect r, float dx, float dy) {
        r.origin.x += dx;
        r.origin.y += dy;
        return r;
    }

    float centerY(const cocos2d::CCRect& r) {
        return r.getMinY() + r.size.height * 0.5f;
    }

    float centerX(const cocos2d::CCRect& r) {
        return r.getMinX() + r.size.width * 0.5f;
    }

    float gapTo(const cocos2d::CCRect& p,
                const cocos2d::CCRect& o, float dir) {
        return dir >= 0.f ? o.getMinX() - p.getMaxX()
                          : p.getMinX() - o.getMaxX();
    }

    bool grounded(PlayerObject* p) {
        return p && (p->m_isOnGround || p->m_isOnGround2);
    }
}

class $modify(GigaDashBotPlayLayer, PlayLayer) {
    struct Fields {
        float cooldown = 0.f;
        float debugCooldown = 0.f;
        float lastJumpedX = -100000.f;
        bool releaseJumpNextFrame = false;
        bool directionalHeld = false;
        bool platformerRightHeld = false;
        int targetID = -1;
        float targetGap = 0.f;
        float targetTtc = 0.f;
        const char* decision = "WAIT";
    };

    void releaseBotInputs(PlayerObject* p) {
        if (!p) return;
        p->releaseButton(PlayerButton::Jump);
        p->releaseButton(PlayerButton::Right);
        p->releaseButton(PlayerButton::Left);
        m_fields->releaseJumpNextFrame = false;
        m_fields->directionalHeld = false;
        m_fields->platformerRightHeld = false;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto mod = Mod::get();
        auto* p = m_player1;

        if (!mod->getSettingValue<bool>("auto-play")) {
            if (p && (m_fields->releaseJumpNextFrame
                || m_fields->directionalHeld
                || m_fields->platformerRightHeld))
                releaseBotInputs(p);
            return;
        }

        if (!p) return;
        if (m_isPaused || p->m_isDead) {
            releaseBotInputs(p);
            return;
        }

        // Convert one-frame tap into a clean press/release edge.
        if (m_fields->releaseJumpNextFrame) {
            p->releaseButton(PlayerButton::Jump);
            m_fields->releaseJumpNextFrame = false;
        }

        m_fields->cooldown = std::max(0.f, m_fields->cooldown - dt);
        m_fields->debugCooldown = std::max(0.f, m_fields->debugCooldown - dt);

        const bool platformer = p->m_isPlatformer;
        const Mode mode = detectMode(p);
        const bool directional = !platformer && directionalMode(mode);
        const float lead = static_cast<float>(
            mod->getSettingValue<int64_t>("jump-lead-ms")) / 1000.f;

        if (platformer && !m_fields->platformerRightHeld) {
            p->pushButton(PlayerButton::Right);
            m_fields->platformerRightHeld = true;
        }

        const cocos2d::CCRect playerRect = p->getObjectRect();
        const float vx = static_cast<float>(p->getCurrentXVelocity());
        const float dir = vx < -1.f ? -1.f : 1.f;
        const float speed = std::max(std::abs(vx), platformer ? 120.f : kMinSpeed);
        const float vy = static_cast<float>(p->getYVelocity());
        const float playerY = centerY(playerRect);

        // Re-arm only after the player has actually passed the previous spike cluster.
        if (dir > 0.f && playerRect.getMinX() > m_fields->lastJumpedX + kHazardCluster)
            m_fields->lastJumpedX = -100000.f;
        else if (dir < 0.f && playerRect.getMaxX() < m_fields->lastJumpedX - kHazardCluster)
            m_fields->lastJumpedX = -100000.f;

        Target best{};
        const float lookAhead = directional
            ? kDirectionalLookAhead
            : std::min(kMaxLookAhead, lead + 0.36f);

        // Fast native object scan runs once per game update (usually ~60 Hz).
        geode::cocos::CCArrayExt<GameObject*> objects = m_objects;
        for (auto* object : objects) {
            if (!object || object == p || object->m_isDisabled2) continue;

            const bool isHazard = hazardObject(object);
            const cocos2d::CCRect objectRect = object->getObjectRect();

            if (!isHazard && !raisedSolid(object, objectRect, playerRect))
                continue;

            const float gap = gapTo(playerRect, objectRect, dir);
            if (gap < -playerRect.size.width * 0.7f) continue;

            const float ttc = std::max(0.f, gap) / speed;
            if (ttc > lookAhead) continue;

            const float predictedY = playerY + vy * ttc;
            const auto predictedPlayer = offsetRect(
                playerRect, dir * speed * ttc,
                directional ? predictedY - playerY : 0.f
            );

            // Ground modes must intersect the player's current lane. Flight modes
            // use the predicted vertical lane at the estimated crossing time.
            const bool intersectsPath = directional
                ? verticalOverlap(predictedPlayer, objectRect, 1.f)
                : verticalOverlap(playerRect, objectRect, 0.f);
            if (!intersectsPath) continue;

            if (!best.object || ttc < best.ttc) {
                best.object = object;
                best.rect = objectRect;
                best.gap = gap;
                best.ttc = ttc;
                best.predictedY = predictedY;
            }
        }

        m_fields->targetID = best.object ? best.object->m_objectID : -1;
        m_fields->targetGap = best.object ? best.gap : 0.f;
        m_fields->targetTtc = best.object ? best.ttc : 0.f;
        m_fields->decision = best.object ? "TRACK" : "CLEAR";

        if (!best.object) {
            if (directional && m_fields->directionalHeld) {
                p->releaseButton(PlayerButton::Jump);
                m_fields->directionalHeld = false;
            }
            return;
        }

        if (directional) {
            // Steer away from the obstacle's vertical side; this is intentionally
            // conservative rather than toggling the button for every nearby object.
            const bool obstacleAbove = centerY(best.rect) >= best.predictedY;
            bool pressUp = !obstacleAbove;
            if (p->m_isUpsideDown) pressUp = !pressUp;

            if (pressUp && !m_fields->directionalHeld) {
                p->pushButton(PlayerButton::Jump);
                m_fields->directionalHeld = true;
                m_fields->decision = "STEER UP";
            } else if (!pressUp && m_fields->directionalHeld) {
                p->releaseButton(PlayerButton::Jump);
                m_fields->directionalHeld = false;
                m_fields->decision = "STEER DOWN";
            }
        } else {
            const bool canJumpNow = grounded(p) && !p->m_isOnSlope;
            const bool canUfoAirTap = mode == Mode::UFO && !platformer;
            const bool inJumpWindow = best.ttc <= lead;

            const float targetX = centerX(best.rect);
            const bool sameCluster =
                std::abs(targetX - m_fields->lastJumpedX) < kHazardCluster;

            if (inJumpWindow && (canJumpNow || canUfoAirTap)
                && !sameCluster && m_fields->cooldown <= 0.f) {
                p->pushButton(PlayerButton::Jump);
                m_fields->releaseJumpNextFrame = true;
                m_fields->cooldown = kTapCooldown;
                m_fields->lastJumpedX = targetX;
                m_fields->decision = "JUMP";
            }
        }

        if (mod->getSettingValue<bool>("show-debug-log") && m_fields->debugCooldown <= 0.f) {
            log::info("GIGA BOT: mode={}, object={}, gap={:.1f}, ttc={:.3f}s, grounded={}, action={}",
                modeName(mode, platformer), m_fields->targetID, m_fields->targetGap,
                m_fields->targetTtc, grounded(p), m_fields->decision);
            m_fields->debugCooldown = 0.25f;
        }
    }

    void onQuit() {
        releaseBotInputs(m_player1);
        PlayLayer::onQuit();
    }
};

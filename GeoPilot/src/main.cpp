// lang: C++, file: src/main.cpp, target: Geometry Dash 2.2081, Geode v5.10.1
// GeoPilot is an experimental, offline-first obstacle heuristic. It is not a full physics solver.

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

using namespace geode::prelude;

namespace {
    enum class Mode : int {
        Cube = 0,
        Ship = 1,
        Ball = 2,
        UFO = 3,
        Wave = 4,
        Robot = 5,
        Spider = 6,
        Swing = 7
    };

    Mode parseMode(std::string const& value, PlayerObject* player) {
        if (value == "Cube") return Mode::Cube;
        if (value == "Ship") return Mode::Ship;
        if (value == "Ball") return Mode::Ball;
        if (value == "UFO") return Mode::UFO;
        if (value == "Wave") return Mode::Wave;
        if (value == "Robot") return Mode::Robot;
        if (value == "Spider") return Mode::Spider;
        if (value == "Swing") return Mode::Swing;

        // GameObjectType mode values in GD 2.2 map to Cube=0 through Swing=7.
        auto raw = static_cast<int>(player->getActiveMode());
        raw = std::clamp(raw, 0, 7);
        return static_cast<Mode>(raw);
    }

    bool isKnownHazard(int id) {
        // Common spike, saw, and related hazard object IDs. Custom objects and
        // non-visual triggers still require a real physics/pathfinding solver.
        switch (id) {
            case 8: case 39: case 88: case 89: case 90: case 91:
            case 92: case 93: case 94: case 95: case 96: case 97: case 98:
            case 103: case 105: case 133: case 134: case 135: case 136:
            case 137: case 138: case 139: case 140:
                return true;
            default:
                return false;
        }
    }

    bool isHoldMode(Mode mode) {
        return mode == Mode::Ship || mode == Mode::Wave;
    }
}

class $modify(GeoPilotPlayLayer, PlayLayer) {
    struct Fields {
        float cooldown = 0.f;
        bool releaseNextFrame = false;
        bool lastHoldState = false;
        float lastHazardX = -100000.f;
        int lastHazardID = -1;
        float logCooldown = 0.f;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("auto-play")) {
            if (m_fields->lastHoldState && m_player1) {
                m_player1->releaseButton(PlayerButton::Jump);
                m_fields->lastHoldState = false;
            }
            return;
        }

        if (!m_player1 || m_isPaused || m_player1->m_isDead) return;

        auto* player = m_player1;
        auto modeSetting = mod->getSettingValue<std::string>("control-mode");
        const Mode mode = parseMode(modeSetting, player);
        const bool holdMode = isHoldMode(mode);
        const float playerX = player->getPositionX();
        const float playerY = player->getPositionY();
        const float reactionDistance = static_cast<float>(
            mod->getSettingValue<int64_t>("reaction-distance")
        );

        m_fields->cooldown = std::max(0.f, m_fields->cooldown - dt);
        m_fields->logCooldown = std::max(0.f, m_fields->logCooldown - dt);

        // Taps must be released on the following frame to form a clean input edge.
        if (m_fields->releaseNextFrame) {
            player->releaseButton(PlayerButton::Jump);
            m_fields->releaseNextFrame = false;
        }

        GameObject* nearestHazard = nullptr;
        float nearestDx = reactionDistance * 2.f;
        geode::cocos::CCArrayExt<GameObject*> objects = m_objects;

        for (auto* object : objects) {
            if (!object || !isKnownHazard(object->m_objectID)) continue;
            const float dx = object->getPositionX() - playerX;
            if (dx < -24.f || dx > reactionDistance * 2.f) continue;
            if (std::abs(object->getPositionY() - playerY) > 260.f) continue;
            if (dx < nearestDx) {
                nearestDx = dx;
                nearestHazard = object;
            }
        }

        if (!nearestHazard) {
            if (holdMode && m_fields->lastHoldState) {
                player->releaseButton(PlayerButton::Jump);
                m_fields->lastHoldState = false;
            }
            return;
        }

        bool shouldAct = nearestDx <= reactionDistance;
        if (holdMode) {
            // Simple lane heuristic: steer away from the nearest recognized hazard.
            // Ship/Wave physics and adjacent corridors make this intentionally approximate.
            const float hazardY = nearestHazard->getPositionY();
            const float verticalGap = 22.f;
            bool wantHold = m_fields->lastHoldState;
            if (hazardY < playerY - verticalGap) wantHold = true;
            else if (hazardY > playerY + verticalGap) wantHold = false;

            if (shouldAct && wantHold != m_fields->lastHoldState) {
                if (wantHold) player->pushButton(PlayerButton::Jump);
                else player->releaseButton(PlayerButton::Jump);
                m_fields->lastHoldState = wantHold;
            }
        } else if (shouldAct && m_fields->cooldown <= 0.f) {
            const bool isSameHazard = nearestHazard->m_objectID == m_fields->lastHazardID
                && std::abs(nearestHazard->getPositionX() - m_fields->lastHazardX) < 10.f;
            if (!isSameHazard || nearestDx > reactionDistance * 0.55f) {
                player->pushButton(PlayerButton::Jump);
                m_fields->releaseNextFrame = true;
                m_fields->cooldown = 0.13f;
                m_fields->lastHazardID = nearestHazard->m_objectID;
                m_fields->lastHazardX = nearestHazard->getPositionX();
            }
        }

        if (mod->getSettingValue<bool>("show-debug-log") && m_fields->logCooldown <= 0.f) {
            log::info(
                "GeoPilot: mode={}, dx={:.1f}, y={:.1f}, hazard-id={}",
                static_cast<int>(mode), nearestDx, playerY, nearestHazard->m_objectID
            );
            m_fields->logCooldown = 1.f;
        }
    }

    void onQuit() {
        if (m_player1) m_player1->releaseButton(PlayerButton::Jump);
        PlayLayer::onQuit();
    }
};

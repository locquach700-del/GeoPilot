// Detail Boost - lag reduction for effect-heavy Geometry Dash levels.
//
// Nothing here deletes, hides or edits level objects. It only:
//   1. caps the particle budget of big particle effects, and
//   2. optionally ignores camera-shake triggers.

#include <Geode/Geode.hpp>
#include <Geode/modify/CCParticleSystem.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include <algorithm>
#include <atomic>

using namespace geode::prelude;

namespace {
    // Settings are cached here so the hot per-frame hook never has to look
    // them up by name.
    std::atomic_bool g_reduceParticles{true};
    std::atomic_int  g_particlePercent{35};
    std::atomic_bool g_onlyInLevel{true};
    std::atomic_bool g_disableShake{true};

    // Tiny particle systems (coin pickups, orb clicks, ...) cost almost
    // nothing, so they are left exactly as the game made them.
    constexpr unsigned int kSmallSystem = 16;
}

$on_mod(Loaded) {
    auto* mod = Mod::get();

    g_reduceParticles = mod->getSettingValue<bool>("reduce-particles");
    g_particlePercent = static_cast<int>(mod->getSettingValue<int64_t>("particle-percent"));
    g_onlyInLevel     = mod->getSettingValue<bool>("only-in-level");
    g_disableShake    = mod->getSettingValue<bool>("disable-shake");

    listenForSettingChanges<bool>("reduce-particles", [](bool v) {
        g_reduceParticles = v;
    });
    listenForSettingChanges<int64_t>("particle-percent", [](int64_t v) {
        g_particlePercent = static_cast<int>(v);
    });
    listenForSettingChanges<bool>("only-in-level", [](bool v) {
        g_onlyInLevel = v;
    });
    listenForSettingChanges<bool>("disable-shake", [](bool v) {
        g_disableShake = v;
    });
}

// ---------------------------------------------------------------------------
// Particle budget
//
// cocos2d's CCParticleSystem::update() only spawns a new particle while
// (particleCount < totalParticles). We lower "totalParticles" for the duration
// of that one call and put the real value back afterwards. The allocated
// particle array is never touched, so this cannot overflow anything; existing
// particles simply live out their lifetime while fewer new ones are born.
// ---------------------------------------------------------------------------
class $modify(DBParticleSystem, CCParticleSystem) {
    void update(float dt) {
        if (!g_reduceParticles.load(std::memory_order_relaxed) ||
            (g_onlyInLevel.load(std::memory_order_relaxed) && !PlayLayer::get())) {
            CCParticleSystem::update(dt);
            return;
        }

        unsigned int const original = m_uTotalParticles;
        if (original <= kSmallSystem) {
            CCParticleSystem::update(dt);
            return;
        }

        unsigned int const percent = static_cast<unsigned int>(
            std::clamp(g_particlePercent.load(std::memory_order_relaxed), 0, 100)
        );

        m_uTotalParticles = original * percent / 100;
        CCParticleSystem::update(dt);
        m_uTotalParticles = original;
    }
};

// ---------------------------------------------------------------------------
// Camera shake
// ---------------------------------------------------------------------------
class $modify(DBBaseGameLayer, GJBaseGameLayer) {
    void shakeCamera(float duration, float strength, float interval) {
        if (g_disableShake.load(std::memory_order_relaxed)) {
            return;
        }
        GJBaseGameLayer::shakeCamera(duration, strength, interval);
    }
};

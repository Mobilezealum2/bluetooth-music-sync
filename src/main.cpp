#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <cmath>
#include <string>
#include <algorithm>

using namespace geode::prelude;

// ---------- Сглаживание времени кадра ----------
class $modify(SmoothScheduler, CCScheduler) {
    void update(float dt) {
        if (!Mod::get()->getSettingValue<bool>("smooth_dt")) {
            return CCScheduler::update(dt);
        }
        static float err = 0.f;
        float target = (float)CCDirector::sharedDirector()->getAnimationInterval();
        float use = dt;

        if (target > 0.f && std::abs(dt - target) < target * 0.3f) {
            // обычный кадр с дрожанием: подаём ровное время
            err += dt - target;
            use = target;
            if (std::abs(err) > target * 0.5f) {
                use += err; // возвращаем накопленную разницу
                err = 0.f;
            }
        } else {
            err = 0.f; // реальный фриз не трогаем
        }
        CCScheduler::update(use);
    }
};

// ---------- Синхронизация музыки ----------
class $modify(SyncPlayLayer, PlayLayer) {
    struct Fields {
        bool calibrated = false;
        int frames = 0;
        double offsetMs = 0;
        float cooldown = 0.f;
    };

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->calibrated = false;
        m_fields->frames = 0;
    }

    void resume() {
        PlayLayer::resume();
        m_fields->calibrated = false;
        m_fields->frames = 0;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (m_isPaused || m_hasCompletedLevel || m_player1->m_isDead) return;

        auto audio = FMODAudioEngine::sharedEngine();
        if (!audio->isMusicPlaying(0)) return;

        auto f = m_fields.self();
        f->cooldown -= dt;

        // ждём ~1 сек после старта, пока музыка нормально запустится
        if (!f->calibrated) {
            if (++f->frames < 60) return;
            f->offsetMs = (double)audio->getMusicTimeMS(0) - m_gameState.m_levelTime * 1000.0;
            f->calibrated = true;
            return;
        }

        int btMs = 0;
        if (Mod::get()->getSettingValue<bool>("bt_enabled")) {
            btMs = (int)Mod::get()->getSettingValue<int64_t>("bt_delay");
        }

        double expected = m_gameState.m_levelTime * 1000.0 + f->offsetMs + btMs;
        double actual = (double)audio->getMusicTimeMS(0);
        double drift = actual - expected; // >0 музыка убежала вперёд

        // огромный дрейф = сменилась песня/триггер, калибруем заново
        if (std::abs(drift) > 2000.0) {
            f->calibrated = false;
            f->frames = 0;
            return;
        }

        int threshold = (int)Mod::get()->getSettingValue<int64_t>("threshold");
        if (std::abs(drift) > threshold && f->cooldown <= 0.f && expected > 0) {
            audio->setMusicTimeMS((unsigned int)expected, false, 0);
            f->cooldown = 0.25f;
        }
    }
};

// ---------- Кнопки в меню паузы ----------
class $modify(SyncPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        bool on = Mod::get()->getSettingValue<bool>("bt_enabled");

        auto menu = CCMenu::create();
        menu->setID("bt-menu");
        menu->setPosition({55.f, 110.f});

        auto title = CCLabelBMFont::create("Bluetooth", "bigFont.fnt");
        title->setScale(0.45f);
        title->setPosition({0.f, 62.f});
        menu->addChild(title);

        auto toggler = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(SyncPauseLayer::onBtToggle), 0.8f
        );
        toggler->toggle(on);
        toggler->setPosition({0.f, 35.f});
        menu->addChild(toggler);

        auto minusSpr = ButtonSprite::create("-");
        minusSpr->setScale(0.7f);
        auto minusBtn = CCMenuItemSpriteExtra::create(
            minusSpr, this, menu_selector(SyncPauseLayer::onBtMinus)
        );
        minusBtn->setPosition({-32.f, 2.f});
        menu->addChild(minusBtn);

        auto plusSpr = ButtonSprite::create("+");
        plusSpr->setScale(0.7f);
        auto plusBtn = CCMenuItemSpriteExtra::create(
            plusSpr, this, menu_selector(SyncPauseLayer::onBtPlus)
        );
        plusBtn->setPosition({32.f, 2.f});
        menu->addChild(plusBtn);

        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setID("bt-label");
        label->setScale(0.45f);
        label->setPosition({0.f, -24.f});
        menu->addChild(label);

        this->addChild(menu);
        this->updateBtLabel();
    }

    void updateBtLabel() {
        auto menu = this->getChildByID("bt-menu");
        if (!menu) return;
        auto label = static_cast<CCLabelBMFont*>(menu->getChildByID("bt-label"));
        if (!label) return;

        int ms = (int)Mod::get()->getSettingValue<int64_t>("bt_delay");
        label->setString((std::to_string(ms) + " ms").c_str());
    }

    void changeDelay(int diff) {
        int ms = (int)Mod::get()->getSettingValue<int64_t>("bt_delay");
        ms = std::max(0, std::min(500, ms + diff));
        Mod::get()->setSettingValue<int64_t>("bt_delay", ms);
        this->updateBtLabel();
    }

    void onBtToggle(CCObject* sender) {
        // в момент вызова состояние ещё не переключено, поэтому инвертируем
        auto t = static_cast<CCMenuItemToggler*>(sender);
        Mod::get()->setSettingValue<bool>("bt_enabled", !t->isToggled());
    }

    void onBtMinus(CCObject*) { this->changeDelay(-10); }
    void onBtPlus(CCObject*) { this->changeDelay(10); }
};

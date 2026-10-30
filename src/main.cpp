#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <string>
#include <algorithm>

using namespace geode::prelude;

// ---------- Один сдвиг музыки вперёд ----------
class $modify(BtPlayLayer, PlayLayer) {
    struct Fields {
        bool pending = true;     // нужно ли применить сдвиг
        int frames = 0;          // кадров после старта попытки
        int appliedMs = 0;       // какой сдвиг сейчас применён
    };

    void resetLevel() {
        PlayLayer::resetLevel();
        // музыка запускается заново, прежний сдвиг пропал
        m_fields->appliedMs = 0;
        m_fields->frames = 0;
        m_fields->pending = true;
    }

    void resume() {
        PlayLayer::resume();
        // применим разницу, если задержку поменяли в паузе
        m_fields->frames = 0;
        m_fields->pending = true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto f = m_fields.self();
        if (!f->pending) return; // после сдвига мод больше ничего не делает

        if (m_isPaused || m_hasCompletedLevel || m_player1->m_isDead) return;

        // ждём немного, пока музыка реально запустится
        if (++f->frames < 15) return;

        auto audio = FMODAudioEngine::sharedEngine();
        if (!audio->isMusicPlaying(0)) return;

        f->pending = false;

        int target = 0;
        if (Mod::get()->getSettingValue<bool>("bt_enabled")) {
            target = (int)Mod::get()->getSettingValue<int64_t>("bt_delay");
        }

        int delta = target - f->appliedMs;
        if (delta == 0) return;

        int now = (int)audio->getMusicTimeMS(0);
        int pos = std::max(0, now + delta);
        audio->setMusicTimeMS((unsigned int)pos, false, 0);
        f->appliedMs = target;
    }
};

// ---------- Кнопки в меню паузы ----------
class $modify(BtPauseLayer, PauseLayer) {
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
            this, menu_selector(BtPauseLayer::onBtToggle), 0.8f
        );
        toggler->toggle(on);
        toggler->setPosition({0.f, 35.f});
        menu->addChild(toggler);

        auto minusSpr = ButtonSprite::create("-");
        minusSpr->setScale(0.7f);
        auto minusBtn = CCMenuItemSpriteExtra::create(
            minusSpr, this, menu_selector(BtPauseLayer::onBtMinus)
        );
        minusBtn->setPosition({-32.f, 2.f});
        menu->addChild(minusBtn);

        auto plusSpr = ButtonSprite::create("+");
        plusSpr->setScale(0.7f);
        auto plusBtn = CCMenuItemSpriteExtra::create(
            plusSpr, this, menu_selector(BtPauseLayer::onBtPlus)
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

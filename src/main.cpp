#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/OptionsLayer.hpp>

using namespace geode::prelude;

static std::string getVolumeStr(float pVal) {
    return utils::numToString(static_cast<int>(std::round(pVal * 100)));
}

static void setupSlider(bool pIsMusic, CCNode* pLayer, geode::CopyableFunction<void(CCObject*)> pCallback, TextInput*& pInputPtr, Slider*& pSliderPtr) {
    auto slider = typeinfo_cast<Slider*>(pLayer->getChildByIDRecursive(pIsMusic ? "music-slider" : "sfx-slider"));

    if (!slider) {
        return;
    }

    pSliderPtr = slider;
    slider->setZOrder(1);

    auto label = typeinfo_cast<CCLabelBMFont*>(pLayer->getChildByIDRecursive(pIsMusic ? "music-label" : "sfx-label"));

    if (!label) {
        return;
    }

    label->setVisible(false);
    
    auto newLabel = CCLabelBMFont::create(Mod::get()->getSettingValue<std::string>(pIsMusic ? "music-text" : "sfx-text").c_str(), label->getFntFile());
    newLabel->setID(pIsMusic ? "music-label"_spr : "sfx-label"_spr);
    newLabel->setScale(label->getScale());

    auto texture = CCTextureCache::sharedTextureCache()->addImage("sliderBar2.png", true);

    if (texture && Mod::get()->getSettingValue<bool>("colored-bars")) {
        slider->m_sliderBar->setTexture(texture);
        slider->m_sliderBar->setColor(Mod::get()->getSettingValue<ccColor3B>(pIsMusic ? "music-color" : "sfx-color"));
    }

    pInputPtr = TextInput::create(40.0f, "0");
    pInputPtr->setID(pIsMusic ? "music-input"_spr : "sfx-input"_spr);
    pInputPtr->setScale(0.65f);
    pInputPtr->setFilter(".1234567890");
    pInputPtr->setString(getVolumeStr(slider->getValue()));
    pInputPtr->setCallback([=] (const std::string& pStr) {
        if (pStr.empty() || pStr.ends_with('.')) {
            return;
        }
        slider->setValue(std::clamp(utils::numFromString<float>(pStr).unwrapOrDefault(), 0.0f, 100.0f) / 100);
        slider->updateBar();
        pCallback(slider->m_touchLogic->m_thumb);
    });

    auto parent = slider->getParent();

    if (!parent) {
        return;
    }

    auto percentLabel = CCLabelBMFont::create("%", label->getFntFile());
    percentLabel->setScale(label->getScale());

    auto labelMenu = CCMenu::create();
    labelMenu->setID(pIsMusic ? "music-label-menu"_spr : "sfx-label-menu"_spr);
    labelMenu->setPosition(label->getPosition());
    labelMenu->setContentSize(CCSizeZero);
    parent->addChild(labelMenu);

    labelMenu->addChild(newLabel);
    labelMenu->addChild(pInputPtr);
    labelMenu->addChild(percentLabel);
    labelMenu->setLayout(RowLayout::create()
        ->setAxisAlignment(AxisAlignment::Center)
        ->setAutoGrowAxis(0.0f)
        ->setGrowCrossAxis(false)
        ->setAutoScale(false)
        ->setGap(5.0f)
    );

    if (!Mod::get()->getSettingValue<bool>("mute-button")) {
        return;
    }
    
    auto offSprite = CCSprite::create("muteoff.png"_spr);
    auto onSprite = CCSprite::create("muteon.png"_spr);

    if (!offSprite || !onSprite) {
        return;
    }

    auto muteToggle = CCMenuItemExt::createToggler(
        offSprite,
        onSprite,
        [=] (CCMenuItemToggler* pSender) {
            const auto muted = !Mod::get()->getSavedValue<bool>(pIsMusic ? "music-muted" : "sfx-muted");

            if (muted) {
                Mod::get()->setSavedValue<float>(pIsMusic ? "music-volume-ret" : "sfx-volume-ret", slider->getValue());
                slider->setValue(0.0f);
                slider->updateBar();
                pCallback(slider->m_touchLogic->m_thumb);
            }
            else {
                slider->setValue(
                    Mod::get()->getSavedValue<float>(pIsMusic ? "music-volume-ret" : "sfx-volume-ret")
                );
                slider->updateBar();
                pCallback(slider->m_touchLogic->m_thumb);
                pSender->toggle(true);
            }
            Mod::get()->setSavedValue<bool>(pIsMusic ? "music-muted" : "sfx-muted", muted);
        }
    );

    muteToggle->setID(pIsMusic ? "music-mute-toggle"_spr : "sfx-mute-toggle"_spr);
    muteToggle->toggle(Mod::get()->getSavedValue<bool>(pIsMusic ? "music-muted" : "sfx-muted"));
    muteToggle->setPosition(CCPointZero);

    auto muteButtonMenu = CCMenu::create();
    muteButtonMenu->setID(pIsMusic ? "music-mute-menu"_spr : "sfx-mute-menu"_spr);
    muteButtonMenu->setContentSize(muteToggle->getScaledContentSize());
    muteButtonMenu->addChild(muteToggle);
    parent->addChild(muteButtonMenu);

    if (Mod::get()->getSettingValue<bool>("mute-button-on-right")) {
        muteButtonMenu->setPosition(
            slider->getPositionX() + ((slider->m_groove->getScaledContentWidth() * slider->getScale()) + 10.0f) / 2,
            slider->getPositionY()
        );
        slider->setPosition(
            slider->getPositionX() - (muteButtonMenu->getScaledContentWidth() + 10.0f) / 2,
            slider->getPositionY()
        );
    }
    else {
        muteButtonMenu->setPosition(
            labelMenu->getPositionX() + (slider->m_groove->getScaledContentWidth() * slider->getScale()) / 2,
            labelMenu->getPositionY()
        );
    }
}

static void tryUpdateMuteButton(CCLayer* pLayer, bool pIsMusic) {
    if (!Mod::get()->getSavedValue<bool>(pIsMusic ? "music-muted" : "sfx-muted")) {
        return;
    }
    auto toggler = typeinfo_cast<CCMenuItemToggler*>(pLayer->getChildByIDRecursive(pIsMusic ? "music-mute-toggle"_spr : "sfx-mute-toggle"_spr));
    if (!toggler) {
        return;
    }
    Mod::get()->setSavedValue<bool>(pIsMusic ? "music-muted" : "sfx-muted", false);
    toggler->toggle(false);
}

class $modify(PauseLayer) {
    struct Fields { 
        TextInput* m_musicInput = nullptr; 
        TextInput* m_sfxInput = nullptr; 
        Slider* m_musicSlider = nullptr; 
        Slider* m_sfxSlider = nullptr; 
    };

    void customSetup() {
        PauseLayer::customSetup();
        auto fields = m_fields.self();
        setupSlider(true, this, [this] (CCObject* pSender) { musicSliderChanged(pSender); }, fields->m_musicInput, fields->m_musicSlider);
        setupSlider(false, this, [this] (CCObject* pSender) { sfxSliderChanged(pSender); }, fields->m_sfxInput, fields->m_sfxSlider);
    }

    void musicSliderChanged(CCObject* pSender) {
        PauseLayer::musicSliderChanged(pSender);
        auto fields = m_fields.self();
        if (fields->m_musicInput && fields->m_musicSlider) {
            fields->m_musicInput->setString(getVolumeStr(fields->m_musicSlider->getValue()));
            tryUpdateMuteButton(this, true);
        }
    }

    void sfxSliderChanged(CCObject* pSender) {
        PauseLayer::sfxSliderChanged(pSender);
        auto fields = m_fields.self();
        if (fields->m_sfxInput && fields->m_sfxSlider) {
            fields->m_sfxInput->setString(getVolumeStr(fields->m_sfxSlider->getValue()));
            tryUpdateMuteButton(this, false);
        }
    }
};

class $modify(OptionsLayer) {
    struct Fields { 
        TextInput* m_musicInput = nullptr; 
        TextInput* m_sfxInput = nullptr; 
        Slider* m_musicSlider = nullptr; 
        Slider* m_sfxSlider = nullptr; 
    };

    void customSetup() {
        OptionsLayer::customSetup();
        auto fields = m_fields.self();
        setupSlider(true, this, [this] (CCObject* pSender) { musicSliderChanged(pSender); }, fields->m_musicInput, fields->m_musicSlider);
        setupSlider(false, this, [this] (CCObject* pSender) { sfxSliderChanged(pSender); }, fields->m_sfxInput, fields->m_sfxSlider);
    }

    void musicSliderChanged(CCObject* pSender) {
        OptionsLayer::musicSliderChanged(pSender);
        auto fields = m_fields.self();
        if (fields->m_musicInput && fields->m_musicSlider) {
            fields->m_musicInput->setString(getVolumeStr(fields->m_musicSlider->getValue()));
            tryUpdateMuteButton(this, true);
        }
    }

    void sfxSliderChanged(CCObject* pSender) {
        OptionsLayer::sfxSliderChanged(pSender);
        auto fields = m_fields.self();
        if (fields->m_sfxInput && fields->m_sfxSlider) {
            fields->m_sfxInput->setString(getVolumeStr(fields->m_sfxSlider->getValue()));
            tryUpdateMuteButton(this, false);
        }
    }
};
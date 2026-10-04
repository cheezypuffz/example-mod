#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

// 1. Define what happens when the button is clicked
void onMyButtonClick(CCObject* sender) {
    FLAlertLayer::create(
        "....", 
        "cheese", 
        "Awesome"
    )->show();
}

// 2. Modify the Main Menu to add the button
class $modify(MenuLayer) {
    bool init() {
        // Run the original game's Main Menu setup first
        if (!MenuLayer::init()) return false;

        // Create a standard sprite (image) for our button from the game files
        auto buttonSprite = CCSprite::createWithSpriteFrameName("GJ_infoBtn_001.png");

        // Create the clickable button object linked to our function above
        auto myButton = CCMenuItemSpriteExtra::create(
            buttonSprite,
            this,
            menu_selector(onMyButtonClick)
        );

        // Find the existing side menu (the layer that holds social buttons)
        auto sideMenu = this->getChildByID("side-menu");

        if (sideMenu) {
            // Add our new button into the side menu list
            sideMenu->addChild(myButton);
            
            // Tell the menu layout to refresh and neatly space out the buttons
            sideMenu->updateLayout();
        }

        return true;
    }
};

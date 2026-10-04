#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace geode::prelude;

const std::string GD_SERVER_URL = "http://boomlings.com";

// Helper function to trim whitespaces from input strings
std::string trimStr(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// Custom Popup to hold the Copy Button neatly next to Scratch's text
class CopyIDPopup : public FLAlertLayer {
    std::string m_levelID;

public:
    static CopyIDPopup* create(const std::string& levelID, const std::string& keyword) {
        auto ret = new CopyIDPopup();
        if (ret && ret->init(levelID, keyword)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init(const std::string& levelID, const std::string& keyword) {
        m_levelID = levelID;

        // Create the base popup box panel layout
        if (!FLAlertLayer::init(nullptr, "ID Found!", "", "Done", nullptr, 260.0f, false, 150.0f, 1.0f)) {
            return false;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Label displaying the found info inside the center panel box
        auto infoLabel = CCLabelBMFont::create(
            fmt::format("Keyword: {}\nID: {}", keyword, levelID).c_str(), 
            "chatFont.fnt"
        );
        infoLabel->setPosition(winSize / 2 + CCPoint{0, 15});
        infoLabel->setScale(0.85f);
        m_mainLayer->addChild(infoLabel);

        // Native Gold Button Sprite for Copy action
        auto copySprite = ButtonSprite::create("Copy ID", 60, true, "goldFont.fnt", "GJ_button_01.png", 30.0f, 0.6f);
        auto copyBtn = CCMenuItemSpriteExtra::create(
            copySprite,
            this,
            menu_selector(CopyIDPopup::onCopy)
        );
        
        copyBtn->setPosition({0, -25}); // Position relative to center
        m_buttonMenu->addChild(copyBtn);

        return true;
    }

    void onCopy(CCObject*) {
        // Native Geode tool to instantly write text straight to user clipboards
        geode::utils::clipboard::write(m_levelID);
        
        // Show a brief standard green check notification badge natively in GD
        Notification::create("Copied to clipboard!", NotificationIcon::Success)->show();
    }
};

class AISearchManager {
public:
    static void startSearch(std::string userInput) {
        userInput = trimStr(userInput);
        int maxPages = 1;
        std::string aiPrompt = userInput;

        // Normalize text to lowercase to run checking routines on command prefixes
        std::string lowerInput = userInput;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);

        if (lowerInput.rfind("<thinker>", 0) == 0) {
            maxPages = 10;
            aiPrompt = trimStr(userInput.substr(9));
        } else if (lowerInput.rfind("<extreme>", 0) == 0) {
            maxPages = 50;
            aiPrompt = trimStr(userInput.substr(9));
        }

        // Show dialogue text showing that Scratch has received the prompt
        showScratchDialog("Let me think about that for a second...");
        fetchAIAnswer(aiPrompt, maxPages);
    }

private:
    // Helper to invoke Scratch speaking animations via native typing engines
    static void showScratchDialog(const std::string& text) {
        // ID 1 maps texture layouts to Scratch (Purple Shopkeeper)
        auto dialogObj = DialogObject::create(text, 1, 1.0f, false, {255, 255, 255});
        auto dialogArray = CCArray::create();
        dialogArray->addObject(dialogObj);

        // Creates the native dialogue window interface layer overlays
        auto dialogLayer = DialogLayer::create(dialogArray, 2); 
        dialogLayer->animateInDialog();
        
        CCDirector::sharedDirector()->getRunningScene()->addChild(dialogLayer, 100);
    }

    static void fetchAIAnswer(const std::string& prompt, int maxPages) {
        // Securely fetch your API Key without exposing it inside your GitHub repo commits
        std::string apiKey = Mod::get()->getSettingValue<std::string>("groq-api-key");
        apiKey = trimStr(apiKey);

        if (apiKey.empty()) {
            showScratchDialog("Oops! Go to the mod settings and paste your Groq API Key first.");
            return;
        }

        matjson::Value payload = matjson::Object {
            {"model", "openai/gpt-oss-20b"},
            {"messages", matjson::Array {
                matjson::Object {
                    {"role", "system"},
                    {"content", "You are a strict assistant. You must answer every query using exactly one or two words. No punctuation, no explanations"}
                },
                matjson::Object {
                    {"role", "user"},
                    {"content", prompt}
                }
            }},
            {"temperature", 0.1}
        };

        web::AsyncWebRequest()
            .header("Authorization", "Bearer " + apiKey)
            .header("Content-Type", "application/json")
            .json(payload)
            .post("https://groq.com")
            .text()
            .then([maxPages](std::string const& responseText) {
                auto parseResult = matjson::parse(responseText);
                if (!parseResult.has_value()) {
                    showScratchDialog("Bleh! Something is broken with my brain's JSON data.");
                    return;
                }
                
                auto json = parseResult.value();
                if (json.contains("choices") && json["choices"].is_array() && !json["choices"].as_array().empty()) {
                    std::string aiAnswer = json["choices"]["message"]["content"].as_string();
                    aiAnswer = trimStr(aiAnswer);
                    
                    if (aiAnswer.length() > 20) aiAnswer = aiAnswer.substr(0, 20);
                    if (aiAnswer.empty()) {
                        showScratchDialog("I drew a blank. Try giving me something else.");
                        return;
                    }

                    queryGDServers(aiAnswer, 0, maxPages);
                } else {
                    showScratchDialog("That response format layout looks strange...");
                }
            })
            .expect([](std::string const& error) {
                showScratchDialog("I couldn't reach the interwebs to think up an answer.");
            });
    }

    static void queryGDServers(const std::string& searchQuery, int currentPage, int maxPages) {
        std::string rawBody = fmt::format(
            "gameVersion=21&binaryVersion=35&gdw=0&type=0&str={}&secret=Wmfd2893gb7&page={}",
            searchQuery, currentPage
        );

        web::AsyncWebRequest()
            .header("User-Agent", "")
            .body(rawBody)
            .post(GD_SERVER_URL)
            .text()
            .then([searchQuery, currentPage, maxPages](std::string const& responseText) {
                if (responseText == "-1" || responseText.empty()) {
                    showScratchDialog("I checked the level list... and found absolutely nothing!");
                    return;
                }

                std::stringstream ss(responseText);
                std::string levelListSegment;
                std::getline(ss, levelListSegment, '#');
                if (levelListSegment.empty()) return;

                std::stringstream levelStream(levelListSegment);
                std::string rawLevelData;
                std::vector<std::string> matchingIDs;

                while (std::getline(levelStream, rawLevelData, '|')) {
                    if (rawLevelData.empty()) continue;

                    std::stringstream partsStream(rawLevelData);
                    std::string item;
                    std::vector<std::string> parts;
                    while (std::getline(partsStream, item, ':')) {
                        parts.push_back(item);
                    }

                    std::string levelName = "";
                    std::string levelID = "";
                    for (size_t i = 0; i < parts.size() - 1; i += 2) {
                        if (parts[i] == "2") levelName = parts[i+1];
                        if (parts[i] == "1") levelID = parts[i+1];
                    }

                    std::string lowerName = levelName;
                    std::string lowerQuery = searchQuery;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

                    if (lowerName == lowerQuery && !levelID.empty()) {
                        matchingIDs.push_back(levelID);
                    }
                }

                if (!matchingIDs.empty()) {
                    std::string firstID = matchingIDs[0];
                    
                    // Let Scratch speak via character typing canvas
                    std::string scratchSpeech = fmt::format("The keyword is '{}'! I found the ID for you.", searchQuery);
                    showScratchDialog(scratchSpeech);

                    // Spawn the custom interactive popup layer above containing the clipboard utility
                    auto popup = CopyIDPopup::create(firstID, searchQuery);
                    if (popup) {
                        popup->show();
                    }
                } else if (currentPage + 1 < maxPages) {
                    queryGDServers(searchQuery, currentPage + 1, maxPages);
                } else {
                    showScratchDialog("I searched through everything, but couldn't find an exact title match."}
})
.expect([](std::string const& error) {
showScratchDialog("Ah! The Geometry Dash server connection dropped out.");
});
}
};
// Mod implementation UI hooks
#include <Geode/modify/LevelSearchLayer.hpp>
class $modify(MySearchLayer, LevelSearchLayer) {
struct Fields {
TextInput* m_aiInputBox = nullptr;
};
bool init(int p0) {
if (!LevelSearchLayer::init(p0)) return false;
auto winSize = CCDirector::sharedDirector()->getWinSize();
auto menu = this->getChildByID("main-menu");
if (!menu) return true;
// Create the text box field where players can type prompts
auto inputBox = TextInput::create(140.0f, "Ask AI for a level...", "chatFont.fnt");
inputBox->setID("ai-search-input"_spr);
inputBox->setPosition({winSize.width / 2 - 80.0f, winSize.height / 2 + 65.0f});
inputBox->setScale(0.75f);
this->addChild(inputBox);
m_fields->m_aiInputBox = inputBox;
// Create a green button sprite to trigger the AI execution sequence
auto searchBtnSprite = ButtonSprite::create("AI Ask", 40, true, "goldFont.fnt", "GJ_button_01.png", 25.0f, 0.6f);
auto searchBtn = CCMenuItemSpriteExtra::create(
searchBtnSprite,
this,
menu_selector(MySearchLayer::onAISearchTriggered)
);
searchBtn->setID("ai-search-btn"_spr);
searchBtn->setPosition({70.0f, 65.0f});
menu->addChild(searchBtn);
return true;
}
void onAISearchTriggered(CCObject* sender) {
if (!m_fields->m_aiInputBox) return;
std::string userPrompt = m_fields->m_aiInputBox->getString();
if (userPrompt.empty()) {
Notification::create("Please type a prompt first!", NotificationIcon::Warning)->show();
return;
}
AISearchManager::startSearch(userPrompt);
}
};

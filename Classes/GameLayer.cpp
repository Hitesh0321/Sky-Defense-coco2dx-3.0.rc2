#include "GameLayer.h"
#include "SimpleAudioEngine.h"


USING_NS_CC;

using namespace CocosDenshion;

Scene* GameLayer::createScene()
{
    auto scene = Scene::create();
    auto layer = GameLayer::create();

    scene->addChild(layer);

    return scene;
}

bool GameLayer::init()
{
    if ( !Layer::init() ) {
        return false;
    }

    _screenSize = Director::getInstance()->getWinSize();

    createGameScreen();
    createPools();
    createActions();

    SimpleAudioEngine::getInstance()->playBackgroundMusic("background.mp3");

    schedule(schedule_selector(GameLayer::update));

    return true;
}

void GameLayer::createGameScreen()
{
    auto bg = Sprite::create("bg.png");
    bg->setPosition(Point{
        _screenSize.width * 0.5f,
        _screenSize.height * 0.5f
    });
    addChild(bg);

    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("sprite_sheet.plist", "sprite_sheet.png");
    
    // create cityscape
    Sprite *sprite = nullptr;
    for (auto i = 0; i < 2; i++) {
        sprite = Sprite::createWithSpriteFrameName("city_dark.png");
        sprite->setPosition(Point{
            _screenSize.width * (0.25f + i * 0.5f),
            sprite->boundingBox().size.height * 0.5f
        });
        addChild(sprite, kForeground);

        sprite = Sprite::createWithSpriteFrameName("city_light.png");
        sprite->setPosition(Point{
            (0.25f + i * 0.5f),
            sprite->boundingBox().size.height * 0.9f
        });
        addChild(sprite, kBackground);
    }

    // add trees
    for (auto i = 0; i < 3; i++) {
        sprite = Sprite::createWithSpriteFrameName("trees.png");
        sprite->setPosition(Point{
            _screenSize.width * (0.2f + i * 0.3f),
            sprite->boundingBox().size.height * 0.5f
        });
        addChild(sprite, kForeground);
    }
    
    // create labels
    _scoreDisplay = Label::createWithBMFont("font.fnt", "0");
    _scoreDisplay->setWidth(_screenSize.width * 0.3f);
    _scoreDisplay->setAnchorPoint(Point{1, 0.5});
    _scoreDisplay->setPosition(Point{_screenSize.width * 0.8f, _screenSize.height * 0.94f});
    addChild(_scoreDisplay);

    _energyDisplay = Label::createWithBMFont("font.fnt", "100%");
    _energyDisplay->setWidth(_screenSize.width * 0.1f);
    _energyDisplay->setHorizontalAlignment(TextHAlignment::RIGHT);
    _energyDisplay->setPosition(Point{_screenSize.width * 0.3f, _screenSize.height * 0.94f});
    addChild(_energyDisplay);
    
    auto iconFrame = SpriteFrameCache::getInstance()->getSpriteFrameByName("health_icon.png");
    auto icon = Sprite::createWithSpriteFrame(iconFrame);
    icon->setPosition(Point{_screenSize.width * 0.15f, _screenSize.height * 0.94f});
    addChild(icon, kBackground);
    
    // create clouds
    float cloud_y;
    for (int i = 0; i < 4; i++) {
        cloud_y = i % 2 == 0 ? _screenSize.height * 0.4f : _screenSize.height * 0.5f;
        auto cloud = Sprite::createWithSpriteFrameName("cloud.png");
        cloud->setPosition(Point{_screenSize.width * 0.1f + i * _screenSize.width * 0.3f, cloud_y});
        addChild(cloud, kBackground);
        _clouds.pushBack(cloud);
    }
    
    // create bomb
    _bomb = Sprite::createWithSpriteFrameName("bomb.png");
    _bomb->getTexture()->generateMipmap();
    _bomb->setVisible(false);

    // add sparkle inside the bomb sprite
    Size size = _bomb->boundingBox().size;
    Sprite *sparkle = Sprite::createWithSpriteFrameName("sparkle.png");
    sparkle->setPosition(Point{
        size.width * 0.72f, size.height * 0.72f
    });
    _bomb->addChild(sparkle, kBackground, kSpriteSparkle);
    
    // add halo inside the bomb sprite
    Sprite *halo = Sprite::createWithSpriteFrameName("halo.png");
    halo->setPosition(Point{
        size.width * 0.4f, size.height * 0.4f
    });
    _bomb->addChild(halo, kMiddleground, kSpriteHalo);
    addChild(_bomb, kForeground);

    // create shock wave
    _shockWave = Sprite::createWithSpriteFrameName("shockwave.png");
    _shockWave->getTexture()->generateMipmap();
    _shockWave->setVisible(false);
    addChild(_shockWave);
    
    // create messages
    _introMessage = Sprite::createWithSpriteFrameName("logo.png");
    _introMessage->setPosition(Point{
        _screenSize.width * 0.5f, _screenSize.height * 0.6f
    });
    addChild(_introMessage, kForeground);
    
    _gameOverMessage = Sprite::createWithSpriteFrameName("gameover.png");
    _gameOverMessage->setPosition(Point{
        _screenSize.width * 0.5f, _screenSize.height * 0.65f
    });
    _gameOverMessage->setVisible(false);
    addChild(_gameOverMessage, kForeground);
}

void GameLayer::createPools() {
    for (int i = 0; i < 50; i++) {
        auto sprite = Sprite::createWithSpriteFrameName("meteor.png");
        sprite->setVisible(false);
        addChild(sprite, kMiddleground, kSpriteMeteor);
        _meteorPool.pushBack(sprite);
    }
    
    for (int i = 0; i < 20; i++) {
        auto sprite = Sprite::createWithSpriteFrameName("health.png");
        sprite->setVisible(false);
        addChild(sprite, kMiddleground, kSpriteHealth);
        _healthPool.pushBack(sprite);
    }
}

void GameLayer::createActions() {
    auto easeSwing = Sequence::create(
      EaseInOut::create(RotateTo::create(1.2f, -10), 2),
      EaseInOut::create(RotateTo::create(1.2f, 10), 2),
      NULL
    );
    _swingHealth = RepeatForever::create(easeSwing);

    _shockWaveSequence = Sequence::create(FadeOut::create(1.0f),
                                          CallFuncN::create(
                                            std::bind(&GameLayer::shockWaveDone, this, std::placeholders::_1)),
                                          nullptr);

    _growBomb = ScaleTo::create(6.0f, 1.0);
    
    // action to rotate sprite
    auto rotate = RotateBy::create(0.5f, -90);
    _rotateSprite = RepeatForever::create(rotate);
    
    Animation *animation = Animation::create();
    for (int i = 1; i <= 10; i++) {
        auto name = "boom" + std::to_string(i) + ".png";
        auto frame = SpriteFrameCache::getInstance()->getSpriteFrameByName(name);
        animation->addSpriteFrame(frame);
    }
    animation->setDelayPerUnit(1/10);
    animation->setRestoreOriginalFrame(true);
    _groundHit = Sequence::create(MoveBy::create(0, Point{0, _screenSize.height * 0.12f}),
                                  Animate::create(animation),
                                  CallFuncN::create(std::bind(&GameLayer::animationDone, this, std::placeholders::_1)),
                                  nullptr);
    
    animation = Animation::create();
    for (int i = 0; i <= 7; i++) {
        auto name  = "explosion_small" + std::to_string(i) + ".png";
        auto frame = SpriteFrameCache::getInstance()->getSpriteFrameByName(name);
        animation->addSpriteFrame(frame);
    }
    animation->setDelayPerUnit(0.5/7.0);
    animation->setRestoreOriginalFrame(true);
    _explosion = Sequence::create(Animate::create(animation),
                                  CallFuncN::create(std::bind(&GameLayer::animationDone, this, std::placeholders::_1)),
                                  nullptr);
}

void GameLayer::shockWaveDone(Node *sender) {
}

void GameLayer::animationDone(Node *sender) {
    sender->setVisible(false);
}

void GameLayer::onTouchesBegan(const std::vector<Touch *>& touches, Event *unused_event) {
    if (!_running) {
        if (_introMessage->isVisible()) {
            _introMessage->setVisible(false);
        } else if (_gameOverMessage->isVisible()) {
            SimpleAudioEngine::getInstance()->stopAllEffects();
            _gameOverMessage->setVisible(false);
        }

        resetGame();
        
        return;
    }
    
    Touch *touch = touches[0];
    
    if (touch) {
        if (_bomb->isVisible()) {
            // stop all actions in bomb, halo and sparkle
            _bomb->stopAllActions();
            Sprite *child = static_cast<Sprite *>(_bomb->getChildByTag(kSpriteHalo));
            child->stopAllActions();

            child = static_cast<Sprite *>(_bomb->getChildByTag(kSpriteSparkle));
            child->stopAllActions();

            // if bomb is the right scale, then create the shockwave
            if (_bomb->getScale() > 0.3f) {
                _shockWave->setScale(0.1f);
                _shockWave->setPosition(_bomb->getPosition());
                _shockWave->setVisible(true);
                _shockWave->runAction(ScaleTo::create(0.5f, _bomb->getScale() * 2.0f));
                _shockWave->runAction(_shockWaveSequence->clone());
                SimpleAudioEngine::getInstance()->playEffect("bombRelease.wav");
            } else {
                SimpleAudioEngine::getInstance()->playEffect("bombFail.wav");
            }
            _bomb->setVisible(false);
            _shockWaveHits = 0;
        } else {
            // no bomb currently on screen, let's create one
            Point tap = touch->getLocation();
            _bomb->stopAllActions();
            _bomb->setScale(0.1f);
            _bomb->setPosition(tap);
            _bomb->setVisible(true);
            _bomb->setOpacity(50);
            _bomb->runAction(_growBomb->clone());
            
            Sprite *child = static_cast<Sprite *>(_bomb->getChildByTag(kSpriteHalo));
            child->runAction(_rotateSprite->clone());

            child = static_cast<Sprite *>(_bomb->getChildByTag(kSpriteSparkle));
            child->runAction(_rotateSprite->clone());
        }
    }
}

void GameLayer::resetGame() {
    _score = 0;
    _energy = 100;
    
    
    // reset timers and speed
    _meteorInterval = 2.5f;
    _meteorTimer    = _meteorInterval * 0.99f;
    _meteorSpeed    = 10;
    
    _healthInterval = 20.0f;
    _healthTimer    = 0;
    _healthSpeed    = 15;
    
    _difficultyInterval = 60;
    _difficultyTimer    = 0;
    
    _running = true;
    
    auto value = std::to_string(_energy) + "%";
    _energyDisplay->setString(value);
    
    _scoreDisplay->setString(std::to_string(_score));
}

void GameLayer::stopGame() {
    _running = false;
    
    // stop all actions currently running
    if (_bomb->isVisible()) {
        _bomb->stopAllActions();
        _bomb->setVisible(false);
        Sprite *child = static_cast<Sprite *>(_bomb->getChildByTag(kSpriteHalo));
        child->stopAllActions();

        child = static_cast<Sprite *>(_bomb->getChildByTag(kSpriteSparkle));
        child->stopAllActions();
    }
    
    if (_shockWave->isVisible()) {
        _shockWave->stopAllActions();
        _shockWave->setVisible(false);
    }
}

void GameLayer::update(float delta) {
    // move the clouds
    Sprite *sprite;
    int count;
    
    count = _clouds.size();
    for (int i = 0; i < count; i++) {
        sprite = _clouds.at(i);
        sprite->setPositionX(sprite->getPositionX() + delta * 20);
        if (sprite->getPositionX() > _screenSize.width + sprite->boundingBox().size.width * 0.5f) {
            sprite->setPositionX(-sprite->boundingBox().size.width * 0.5f);
        }
    }
}

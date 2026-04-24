#include <windows.h>
#include "Asteroid.h"
#include "Asteroids.h"
#include "Animation.h"
#include "AnimationManager.h"
#include "GameUtil.h"
#include "GameWindow.h"
#include "GameWorld.h"
#include "GameDisplay.h"
#include "Spaceship.h"
#include "BoundingShape.h"
#include "BoundingSphere.h"
#include "GUILabel.h"
#include "Explosion.h"
#include "BlackHole.h"
#include "PowerUp.h"
#include <algorithm>
#include <functional>
#include <vector>
#include <fstream>

// PUBLIC INSTANCE CONSTRUCTORS ///////////////////////////////////////////////

Asteroids::Asteroids(int argc, char *argv[])
	: GameSession(argc, argv)
{
	mLevel = 0;
	mAsteroidCount = 0;
	mGameStarted = false;
	mMenuState = MENU_MAIN;
	mDifficulty = 0;
	mHasPowerUps   = true;
	mHasBlackHoles = false;
	mHasMilestones = true;
	mFinalScore = 0;
	mNextMilestone = 250;
	mControlLevel = 0;
	mBlackHoleInterval = 20000;
	mTitleAnimTimer = 0;
}

Asteroids::~Asteroids(void)
{
}

// PUBLIC INSTANCE METHODS ////////////////////////////////////////////////////

void Asteroids::Start()
{
	shared_ptr<Asteroids> thisPtr = shared_ptr<Asteroids>(this);

	mGameWorld->AddListener(thisPtr.get());
	mGameWindow->AddKeyboardListener(thisPtr);
	mGameWorld->AddListener(&mScoreKeeper);
	mScoreKeeper.AddListener(thisPtr);

	GLfloat ambient_light[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat diffuse_light[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient_light);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse_light);
	glEnable(GL_LIGHT0);

	AnimationManager::GetInstance().CreateAnimationFromFile("explosion", 64, 1024, 64, 64, "explosion_fs.png");
	AnimationManager::GetInstance().CreateAnimationFromFile("asteroid1", 128, 8192, 128, 128, "asteroid1_fs.png");
	AnimationManager::GetInstance().CreateAnimationFromFile("spaceship", 128, 128, 128, 128, "spaceship_fs.png");

	// Load persisted high scores from file
	{
		std::ifstream file("highscores.txt");
		if (file.is_open())
		{
			std::string line;
			while (std::getline(file, line))
			{
				size_t pos = line.rfind(':');
				if (pos != std::string::npos)
				{
					HighScoreEntry e;
					e.name  = line.substr(0, pos);
					e.score = std::stoi(line.substr(pos + 1));
					mHighScores.push_back(e);
				}
			}
			file.close();
			std::sort(mHighScores.begin(), mHighScores.end(),
				[](const HighScoreEntry& a, const HighScoreEntry& b) {
					return a.score > b.score;
				});
			if (mHighScores.size() > 10) mHighScores.resize(10);
		}
	}

	CreateGUI();

	mGameWorld->AddListener(&mPlayer);
	mPlayer.AddListener(thisPtr);

	CreateBackgroundAsteroids(8);
	ShowMenuState(MENU_MAIN);

	GameSession::Start();
}

void Asteroids::Stop()
{
	GameSession::Stop();
}

// KEYBOARD LISTENER //////////////////////////////////////////////////////////

void Asteroids::OnKeyPressed(uchar key, int x, int y)
{
	// Tab always returns to menu from in-game
	if (mGameStarted && key == 9)
	{
		ResetGame();
		return;
	}

	if (!mGameStarted)
	{
		switch (mMenuState)
		{
		case MENU_MAIN:
			if      (key == '1') StartGame();
			else if (key == '2') ShowMenuState(MENU_DIFFICULTY);
			else if (key == '3') ShowMenuState(MENU_INSTRUCTIONS);
			else if (key == '4') ShowMenuState(MENU_HIGHSCORES);
			break;

		case MENU_DIFFICULTY:
			if      (key == '1') { mDifficulty = 0; ShowMenuState(MENU_MAIN); }
			else if (key == '2') { mDifficulty = 1; ShowMenuState(MENU_MAIN); }
			else if (key == '3') { mDifficulty = 2; ShowMenuState(MENU_MAIN); }
			else                 { ShowMenuState(MENU_MAIN); }
			break;

		case MENU_INSTRUCTIONS:
			ShowMenuState(MENU_MAIN);
			break;

		case MENU_HIGHSCORES:
			ResetGame();
			break;

		case MENU_ENTER_TAG:
			if (key == 13)
			{
				if (mCurrentInput.empty()) mCurrentInput = "PLAYER";
				AddHighScore(mCurrentInput, mFinalScore);
				UpdateHighScoreLabels();
				ShowMenuState(MENU_HIGHSCORES);
			}
			else if (key == 8)
			{
				if (!mCurrentInput.empty())
				{
					mCurrentInput.pop_back();
					mEnterTagInputLabel->SetText("> " + mCurrentInput + "_");
				}
			}
			else if (mCurrentInput.length() < 12 && key >= 32 && key < 127)
			{
				mCurrentInput += (char)key;
				mEnterTagInputLabel->SetText("> " + mCurrentInput + "_");
			}
			break;

		default:
			break;
		}
		return;
	}

	// In-game controls
	switch (key)
	{
	case ' ':
		if (glutGetModifiers() & GLUT_ACTIVE_SHIFT)
			mSpaceship->ShootRing();
		else
			mSpaceship->Shoot();
		break;
	default:
		break;
	}
}

void Asteroids::OnKeyReleased(uchar key, int x, int y)
{
	if (!mGameStarted) return;
}

void Asteroids::OnSpecialKeyPressed(int key, int x, int y)
{
	if (!mGameStarted) return;

	if (glutGetModifiers() & GLUT_ACTIVE_SHIFT)
	{
		switch (key)
		{
		case GLUT_KEY_UP:    mSpaceship->Dash( 90.0f); break;
		case GLUT_KEY_LEFT:  mSpaceship->Dash(180.0f); break;
		case GLUT_KEY_RIGHT: mSpaceship->Dash(  0.0f); break;
		case GLUT_KEY_DOWN:  mSpaceship->Dash(270.0f); break;
		default: break;
		}
		return;
	}

	switch (key)
	{
	case GLUT_KEY_UP:    mSpaceship->Thrust(mSpaceship->GetThrustPower());    break;
	case GLUT_KEY_DOWN:  mSpaceship->Thrust(-mSpaceship->GetThrustPower());   break;
	case GLUT_KEY_LEFT:  mSpaceship->Rotate(mSpaceship->GetRotationSpeed());  break;
	case GLUT_KEY_RIGHT: mSpaceship->Rotate(-mSpaceship->GetRotationSpeed()); break;
	case 114: // GLUT_KEY_CTRL_L
	case 115: // GLUT_KEY_CTRL_R
		mSpaceship->SetBraking(true);
		break;
	default: break;
	}
}

void Asteroids::OnSpecialKeyReleased(int key, int x, int y)
{
	if (!mGameStarted) return;
	switch (key)
	{
	case GLUT_KEY_UP:
	case GLUT_KEY_DOWN:
		mSpaceship->Thrust(0);
		break;
	case GLUT_KEY_LEFT:  mSpaceship->Rotate(0); break;
	case GLUT_KEY_RIGHT: mSpaceship->Rotate(0); break;
	case 114: // GLUT_KEY_CTRL_L
	case 115: // GLUT_KEY_CTRL_R
		mSpaceship->SetBraking(false);
		break;
	default: break;
	}
}

// GAME WORLD LISTENER ////////////////////////////////////////////////////////

void Asteroids::OnObjectRemoved(GameWorld* world, shared_ptr<GameObject> object)
{
	if (!mGameStarted) return;

	if (object->GetType() == GameObjectType("Asteroid"))
	{
		shared_ptr<GameObject> explosion = CreateExplosion();
		explosion->SetPosition(object->GetPosition());
		explosion->SetRotation(object->GetRotation());
		mGameWorld->AddObject(explosion);
		mAsteroidCount--;
		if (mAsteroidCount <= 0)
			SetTimer(500, START_NEXT_LEVEL);

		// Check if this asteroid was consumed by a black hole (not destroyed by the player)
		bool eatenByBlackHole = false;
		{
			GLVector3f aPos = object->GetPosition();
			GameObjectList all = mGameWorld->GetObjects();
			for (auto& o : all)
			{
				if (o->GetType() != GameObjectType("BlackHole")) continue;
				GLVector3f bhPos = o->GetPosition();
				float dx = bhPos.x - aPos.x, dy = bhPos.y - aPos.y;
				if (dx * dx + dy * dy < 15.0f * 15.0f) { eatenByBlackHole = true; break; }
			}
		}

		// Only award points if the player destroyed it
		if (!eatenByBlackHole)
			mScoreKeeper.AddScore(10);

		if (mHasPowerUps && !eatenByBlackHole && rand() % 10 < 3)
		{
			std::vector<PowerUp::Type> enabled;
			if (mPlayer.GetLives() < 5) enabled.push_back(PowerUp::EXTRA_LIFE);
			enabled.push_back(PowerUp::SHOOT_SPREAD);
			enabled.push_back(PowerUp::RING_ATTACK);
			if (enabled.empty()) return;
			PowerUp::Type puType = enabled[rand() % enabled.size()];

			std::function<void()> cb;
			if (puType == PowerUp::EXTRA_LIFE)
			{
				cb = [this]() {
					if (mPlayer.GetLives() >= 5) return; // hard cap at collection time
					mPlayer.AddLife();
					std::ostringstream ss;
					ss << "Lives: " << mPlayer.GetLives();
					mLivesLabel->SetText(ss.str());
					ShowNotification("POWER-UP: Extra Life!");
				};
			}
			else if (puType == PowerUp::SHOOT_SPREAD)
			{
				cb = [this]() {
					if (mSpaceship) mSpaceship->SetShootMode(Spaceship::SHOOT_SPREAD, -1);
					ShowNotification("POWER-UP: Spread Shot!");
				};
			}
			else
			{
				cb = [this]() {
					if (mSpaceship) mSpaceship->SetHasRingAttack(true);
					ShowNotification("POWER-UP: Ring Attack!");
				};
			}

			auto pu = make_shared<PowerUp>(object->GetPosition(), puType, cb);
			pu->SetBoundingShape(make_shared<BoundingSphere>(pu->GetThisPtr(), 5.0f));

			const char* shapeName = (puType == PowerUp::EXTRA_LIFE)  ? "powerup_life.shape"
			                      : (puType == PowerUp::SHOOT_SPREAD) ? "powerup_spread.shape"
			                                                          : "powerup_ring.shape";
			pu->SetShape(make_shared<Shape>(shapeName));
			mGameWorld->AddObject(pu);
		}
	}
}

void Asteroids::OnWorldUpdated(GameWorld* world)
{
	// Animate the main menu title (rainbow cycle) regardless of game state
	mTitleAnimTimer += 16; // ~60 fps tick
	{
		float t  = mTitleAnimTimer / 1000.0f;
		float r  = 0.55f + 0.45f * sinf(t * 0.6f);
		float g  = 0.55f + 0.45f * sinf(t * 0.6f + 2.094f);
		float b  = 0.55f + 0.45f * sinf(t * 0.6f + 4.189f);
		mMenuTitleLabel->SetColor(GLVector3f(r, g, b));
		// Rainbow for #1 leaderboard entry when leaderboard is visible
		if (mMenuState == MENU_HIGHSCORES && !mHighScores.empty())
			mHSEntryLabels[0]->SetColor(GLVector3f(r, g, b));
	}

	if (!mGameStarted || !mSpaceship) return;

	// Poll Ctrl key directly
	bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
	mSpaceship->SetBraking(ctrlHeld);

	// Ring attack indicator
	if (mSpaceship->HasRingAttack())
	{
		int cd = mSpaceship->GetRingCooldown();
		if (cd > 0)
		{
			int secs = (cd + 999) / 1000;
			std::ostringstream ss;
			ss << "Ring: " << secs << "s";
			mRingStatusLabel->SetText(ss.str());
			mRingStatusLabel->SetColor(GLVector3f(0.9f, 0.4f, 0.1f));
		}
		else
		{
			mRingStatusLabel->SetText("Ring: READY");
			mRingStatusLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
		}
	}
	else
	{
		mRingStatusLabel->SetText("");
	}

	// Spread shot indicator
	if (mSpaceship->GetShootMode() == Spaceship::SHOOT_SPREAD)
		mSpreadStatusLabel->SetText("Spread Shot");
	else
		mSpreadStatusLabel->SetText("");

	// Speed display
	{
		GLVector3f vel = mSpaceship->GetVelocity();
		int speed = (int)sqrtf(vel.x * vel.x + vel.y * vel.y);
		std::ostringstream ss;
		ss << "Speed: " << speed;
		mSpeedLabel->SetText(ss.str());
	}

	// Dash indicator
	if (mSpaceship->CanDash())
	{
		mDashStatusLabel->SetText("Dash: READY");
		mDashStatusLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	}
	else
	{
		int cd = mSpaceship->GetDashCooldown();
		std::ostringstream ss;
		ss << "Dash: " << ((cd + 999) / 1000) << "s";
		mDashStatusLabel->SetText(ss.str());
		mDashStatusLabel->SetColor(GLVector3f(0.9f, 0.5f, 0.1f));
	}

	// Brake indicator
	mBrakeStatusLabel->SetText(mSpaceship->IsBraking() ? "BRAKING" : "");
}

// TIMER LISTENER /////////////////////////////////////////////////////////////

void Asteroids::OnTimer(int value)
{
	if (value == CREATE_NEW_PLAYER)
	{
		if (!mGameStarted) return;

		// If the player died near a black hole, spawn at the origin instead
		// of the death position to prevent instant re-kill (spawn camping)
		GLVector3f spawnPos = mDeathPosition;
		GameObjectList allObjs = mGameWorld->GetObjects();
		for (auto& obj : allObjs)
		{
			if (obj->GetType() != GameObjectType("BlackHole")) continue;
			GLVector3f bhPos = obj->GetPosition();
			float dx = bhPos.x - mDeathPosition.x;
			float dy = bhPos.y - mDeathPosition.y;
			if (dx * dx + dy * dy < 80.0f * 80.0f)
			{
				spawnPos = GLVector3f(0.0f, 0.0f, 0.0f);
				break;
			}
		}

		ClearNearbyAsteroids(spawnPos, 30.0f);
		mSpaceship->Reset();
		mSpaceship->SetPosition(spawnPos);
		mGameWorld->AddObject(mSpaceship);
		mSpaceship->StartInvulnerability(200);
	}

	if (value == START_NEXT_LEVEL)
	{
		if (!mGameStarted) return;
		mLevel++;
		int num_asteroids = 10 + 2 * mLevel;
		CreateAsteroids(num_asteroids);
	}

	if (value == HIDE_NOTIFICATION)
	{
		mNotificationLabel->SetVisible(false);
	}

	if (value == SPAWN_BLACK_HOLE)
	{
		if (!mGameStarted) return;
		float angle = (float)(rand() % 360);
		float dist  = 50.0f + (float)(rand() % 30);
		GLVector3f bhPos(
			cos(DEG2RAD * angle) * dist,
			sin(DEG2RAD * angle) * dist,
			0.0f);
		mGameWorld->AddObject(make_shared<BlackHole>(bhPos));
		ShowNotification("WARNING: BLACK HOLE!");
		SetTimer(mBlackHoleInterval, SPAWN_BLACK_HOLE);
	}

	if (value == SHOW_GAME_OVER)
	{
		mFinalScore = mScoreKeeper.GetScore();
		mCurrentInput = "";

		std::ostringstream ss;
		ss << "Final Score: " << mFinalScore;
		mEnterTagScoreLabel->SetText(ss.str());
		mEnterTagInputLabel->SetText("> _");

		mGameStarted = false;
		ShowMenuState(MENU_ENTER_TAG);
	}
}

// PRIVATE METHODS ////////////////////////////////////////////////////////////

void Asteroids::StartGame()
{
	// Remove background asteroids without triggering score/listener events
	mGameWorld->RemoveListener(&mScoreKeeper);
	mGameWorld->RemoveListener(&mPlayer);
	for (auto& obj : mBackgroundAsteroids)
		mGameWorld->RemoveObject(obj);
	mBackgroundAsteroids.clear();
	mGameWorld->AddListener(&mScoreKeeper);
	mGameWorld->AddListener(&mPlayer);

	mScoreKeeper.Reset();
	mPlayer.Reset();
	mLevel = 0;
	mAsteroidCount = 0;
	mNextMilestone = 250;
	mControlLevel = 0;
	mBlackHoleInterval = 20000;
	mScoreLabel->SetText("Score: 0");
	mLivesLabel->SetText("Lives: 3");

	ShowMenuState(STATE_PLAYING);

	mHasPowerUps   = (mDifficulty <= 1);
	mHasBlackHoles = (mDifficulty >= 1);
	mHasMilestones = (mDifficulty <= 1);

	mGameStarted = true;
	mGameWorld->AddObject(CreateSpaceship());
	mSpaceship->SetControlLevel(0);
	CreateAsteroids(10);
	if (mHasBlackHoles)
		SetTimer(15000, SPAWN_BLACK_HOLE);
}

void Asteroids::ResetGame()
{
	mGameStarted = false;
	mLevel = 0;
	mAsteroidCount = 0;
	mNextMilestone = 250;
	mControlLevel = 0;
	mBlackHoleInterval = 20000;

	mGameWorld->RemoveListener(&mScoreKeeper);
	mGameWorld->RemoveListener(&mPlayer);

	mGameWorld->Clear();

	mGameWorld->AddListener(&mScoreKeeper);
	mGameWorld->AddListener(&mPlayer);

	mScoreKeeper.Reset();
	mPlayer.Reset();

	mScoreLabel->SetText("Score: 0");
	mLivesLabel->SetText("Lives: 3");

	CreateBackgroundAsteroids(8);
	ShowMenuState(MENU_MAIN);
}

shared_ptr<GameObject> Asteroids::CreateSpaceship()
{
	mSpaceship = make_shared<Spaceship>();
	mSpaceship->SetBoundingShape(make_shared<BoundingSphere>(mSpaceship->GetThisPtr(), 4.0f));
	shared_ptr<Shape> bullet_shape = make_shared<Shape>("bullet.shape");
	mSpaceship->SetBulletShape(bullet_shape);
	Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("spaceship");
	shared_ptr<Sprite> spaceship_sprite =
		make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
	mSpaceship->SetSprite(spaceship_sprite);
	mSpaceship->SetScale(0.1f);
	mSpaceship->Reset();
	return mSpaceship;
}

void Asteroids::CreateAsteroids(const uint num_asteroids)
{
	mAsteroidCount = num_asteroids;
	for (uint i = 0; i < num_asteroids; i++)
	{
		Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("asteroid1");
		shared_ptr<Sprite> asteroid_sprite =
			make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
		asteroid_sprite->SetLoopAnimation(true);
		shared_ptr<GameObject> asteroid = make_shared<Asteroid>();
		asteroid->SetBoundingShape(make_shared<BoundingSphere>(asteroid->GetThisPtr(), 10.0f));
		asteroid->SetSprite(asteroid_sprite);
		asteroid->SetScale(0.2f);
		mGameWorld->AddObject(asteroid);
	}
}

void Asteroids::CreateBackgroundAsteroids(const uint num_asteroids)
{
	for (uint i = 0; i < num_asteroids; i++)
	{
		Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("asteroid1");
		shared_ptr<Sprite> asteroid_sprite =
			make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
		asteroid_sprite->SetLoopAnimation(true);
		shared_ptr<GameObject> asteroid = make_shared<Asteroid>();
		// No bounding shape — purely decorative, will not collide
		asteroid->SetSprite(asteroid_sprite);
		asteroid->SetScale(0.2f);
		mGameWorld->AddObject(asteroid);
		mBackgroundAsteroids.push_back(asteroid);
	}
}

void Asteroids::CreateGUI()
{
	mGameDisplay->GetContainer()->SetBorder(GLVector2i(10, 10));

	// ---- IN-GAME LABELS ----

	mScoreLabel = make_shared<GUILabel>("Score: 0");
	mScoreLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_TOP);
	mScoreLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mScoreLabel), GLVector2f(0.0f, 1.0f));

	mLivesLabel = make_shared<GUILabel>("Lives: 3");
	mLivesLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_BOTTOM);
	mLivesLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mLivesLabel), GLVector2f(0.0f, 0.0f));

	mGameOverLabel = make_shared<GUILabel>("GAME OVER");
	mGameOverLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mGameOverLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mGameOverLabel->SetColor(GLVector3f(1.0f, 0.2f, 0.2f));
	mGameOverLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mGameOverLabel), GLVector2f(0.5f, 0.5f));

	// ---- MAIN MENU LABELS ----

	mMenuTitleLabel = make_shared<GUILabel>("CRAZY ASTEROIDS");
	mMenuTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mMenuTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuTitleLabel), GLVector2f(0.5f, 0.87f));

	mMenuItem1Label = make_shared<GUILabel>("1 - Start Game");
	mMenuItem1Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem1Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mMenuItem1Label->SetColor(GLVector3f(1.0f, 1.0f, 1.0f));
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem1Label), GLVector2f(0.5f, 0.68f));

	mMenuItem2Label = make_shared<GUILabel>("2 - Difficulty  [EASY]");
	mMenuItem2Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem2Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mMenuItem2Label->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem2Label), GLVector2f(0.5f, 0.58f));

	mMenuItem3Label = make_shared<GUILabel>("3 - Instructions");
	mMenuItem3Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem3Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mMenuItem3Label->SetColor(GLVector3f(1.0f, 1.0f, 1.0f));
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem3Label), GLVector2f(0.5f, 0.48f));

	mMenuItem4Label = make_shared<GUILabel>("4 - High Scores");
	mMenuItem4Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem4Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mMenuItem4Label->SetColor(GLVector3f(1.0f, 1.0f, 1.0f));
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem4Label), GLVector2f(0.5f, 0.38f));

	// ---- DIFFICULTY MENU LABELS ----

	mDiffTitleLabel = make_shared<GUILabel>("DIFFICULTY SETTINGS");
	mDiffTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mDiffTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffTitleLabel), GLVector2f(0.5f, 0.85f));

	mDiffItem1Label = make_shared<GUILabel>("1 - Easy   (Power-Ups, No Black Holes)");
	mDiffItem1Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffItem1Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffItem1Label->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mDiffItem1Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffItem1Label), GLVector2f(0.5f, 0.68f));

	mDiffItem2Label = make_shared<GUILabel>("2 - Normal (Power-Ups + Black Holes)");
	mDiffItem2Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffItem2Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffItem2Label->SetColor(GLVector3f(1.0f, 1.0f, 0.3f));
	mDiffItem2Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffItem2Label), GLVector2f(0.5f, 0.52f));

	mDiffItem3Label = make_shared<GUILabel>("3 - Hard   (Black Holes, No Power-Ups)");
	mDiffItem3Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffItem3Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffItem3Label->SetColor(GLVector3f(1.0f, 0.3f, 0.3f));
	mDiffItem3Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffItem3Label), GLVector2f(0.5f, 0.36f));

	mDiffBackLabel = make_shared<GUILabel>("Press any key to return");
	mDiffBackLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffBackLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffBackLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mDiffBackLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffBackLabel), GLVector2f(0.5f, 0.14f));

	// ---- INSTRUCTIONS LABELS ----

	mInstrTitleLabel = make_shared<GUILabel>("INSTRUCTIONS");
	mInstrTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mInstrTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrTitleLabel), GLVector2f(0.5f, 0.88f));

	mInstrLine1Label = make_shared<GUILabel>("UP: Thrust | DOWN: Brake | L/R: Rotate");
	mInstrLine1Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine1Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine1Label->SetColor(GLVector3f(0.9f, 0.9f, 1.0f));
	mInstrLine1Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine1Label), GLVector2f(0.5f, 0.76f));

	mInstrLine2Label = make_shared<GUILabel>("CTRL: Hard Brake | SHIFT+Arrow: Dash");
	mInstrLine2Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine2Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine2Label->SetColor(GLVector3f(0.9f, 0.9f, 1.0f));
	mInstrLine2Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine2Label), GLVector2f(0.5f, 0.67f));

	mInstrLine3Label = make_shared<GUILabel>("SPACE: Shoot | SHIFT+SPACE: Ring Attack");
	mInstrLine3Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine3Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine3Label->SetColor(GLVector3f(0.9f, 0.9f, 1.0f));
	mInstrLine3Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine3Label), GLVector2f(0.5f, 0.58f));

	mInstrLine4Label = make_shared<GUILabel>("TAB: Return to Main Menu (works in-game)");
	mInstrLine4Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine4Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine4Label->SetColor(GLVector3f(0.9f, 0.9f, 1.0f));
	mInstrLine4Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine4Label), GLVector2f(0.5f, 0.49f));

	mInstrLine5Label = make_shared<GUILabel>("Power-ups: Extra Life, Spread Shot, Ring");
	mInstrLine5Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine5Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine5Label->SetColor(GLVector3f(0.3f, 1.0f, 0.6f));
	mInstrLine5Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine5Label), GLVector2f(0.5f, 0.39f));

	mInstrLine6Label = make_shared<GUILabel>("10 pts/asteroid | Black Holes mid-game");
	mInstrLine6Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine6Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine6Label->SetColor(GLVector3f(1.0f, 0.8f, 0.2f));
	mInstrLine6Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine6Label), GLVector2f(0.5f, 0.29f));

	mInstrLine7Label = make_shared<GUILabel>("Enter tag on Leaderboard after game over");
	mInstrLine7Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine7Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine7Label->SetColor(GLVector3f(1.0f, 0.8f, 0.2f));
	mInstrLine7Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine7Label), GLVector2f(0.5f, 0.19f));

	mInstrBackLabel = make_shared<GUILabel>("Press any key to return");
	mInstrBackLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrBackLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrBackLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mInstrBackLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrBackLabel), GLVector2f(0.5f, 0.07f));

	// ---- IN-GAME NOTIFICATION LABEL ----

	mNotificationLabel = make_shared<GUILabel>("");
	mNotificationLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mNotificationLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mNotificationLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mNotificationLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mNotificationLabel), GLVector2f(0.5f, 0.88f));

	// ---- POWER-UP HUD LABELS (bottom-right) ----

	mRingStatusLabel = make_shared<GUILabel>("");
	mRingStatusLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_RIGHT);
	mRingStatusLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_BOTTOM);
	mRingStatusLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mRingStatusLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mRingStatusLabel), GLVector2f(1.0f, 0.0f));

	mSpreadStatusLabel = make_shared<GUILabel>("");
	mSpreadStatusLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_RIGHT);
	mSpreadStatusLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_BOTTOM);
	mSpreadStatusLabel->SetColor(GLVector3f(0.0f, 0.9f, 1.0f));
	mSpreadStatusLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mSpreadStatusLabel), GLVector2f(1.0f, 0.06f));

	// Speed (bottom-center, above brake)
	mSpeedLabel = make_shared<GUILabel>("Speed: 0");
	mSpeedLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mSpeedLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_BOTTOM);
	mSpeedLabel->SetColor(GLVector3f(0.8f, 0.8f, 0.8f));
	mSpeedLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mSpeedLabel), GLVector2f(0.5f, 0.06f));

	// Dash (bottom-left)
	mDashStatusLabel = make_shared<GUILabel>("Dash: READY");
	mDashStatusLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_LEFT);
	mDashStatusLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_BOTTOM);
	mDashStatusLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mDashStatusLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDashStatusLabel), GLVector2f(0.0f, 0.06f));

	// Brake (bottom-center)
	mBrakeStatusLabel = make_shared<GUILabel>("");
	mBrakeStatusLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mBrakeStatusLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_BOTTOM);
	mBrakeStatusLabel->SetColor(GLVector3f(1.0f, 0.4f, 0.1f));
	mBrakeStatusLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mBrakeStatusLabel), GLVector2f(0.5f, 0.0f));

	// ---- HIGH SCORE LABELS ----

	mHSTitleLabel = make_shared<GUILabel>("HIGH SCORES");
	mHSTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mHSTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mHSTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mHSTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mHSTitleLabel), GLVector2f(0.5f, 0.92f));

	for (int i = 0; i < 10; i++)
	{
		mHSEntryLabels[i] = make_shared<GUILabel>("");
		mHSEntryLabels[i]->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
		mHSEntryLabels[i]->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
		mHSEntryLabels[i]->SetVisible(false);
		float y = 0.82f - i * 0.075f;
		mGameDisplay->GetContainer()->AddComponent(
			static_pointer_cast<GUIComponent>(mHSEntryLabels[i]), GLVector2f(0.5f, y));
	}

	mHSBackLabel = make_shared<GUILabel>("Press any key to return");
	mHSBackLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mHSBackLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mHSBackLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mHSBackLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mHSBackLabel), GLVector2f(0.5f, 0.05f));

	// ---- ENTER TAG LABELS ----

	mEnterTagTitleLabel = make_shared<GUILabel>("GAME OVER");
	mEnterTagTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagTitleLabel->SetColor(GLVector3f(1.0f, 0.2f, 0.2f));
	mEnterTagTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagTitleLabel), GLVector2f(0.5f, 0.82f));

	mEnterTagScoreLabel = make_shared<GUILabel>("Final Score: 0");
	mEnterTagScoreLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagScoreLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagScoreLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagScoreLabel), GLVector2f(0.5f, 0.70f));

	mEnterTagPromptLabel = make_shared<GUILabel>("Enter your gamer tag:");
	mEnterTagPromptLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagPromptLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagPromptLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagPromptLabel), GLVector2f(0.5f, 0.57f));

	mEnterTagInputLabel = make_shared<GUILabel>("> _");
	mEnterTagInputLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagInputLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagInputLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mEnterTagInputLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagInputLabel), GLVector2f(0.5f, 0.46f));

	mEnterTagHintLabel = make_shared<GUILabel>("Press ENTER to confirm");
	mEnterTagHintLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagHintLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagHintLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mEnterTagHintLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagHintLabel), GLVector2f(0.5f, 0.32f));
}

void Asteroids::HideAllLabels()
{
	mScoreLabel->SetVisible(false);
	mLivesLabel->SetVisible(false);
	mGameOverLabel->SetVisible(false);

	mMenuTitleLabel->SetVisible(false);
	mMenuItem1Label->SetVisible(false);
	mMenuItem2Label->SetVisible(false);
	mMenuItem3Label->SetVisible(false);
	mMenuItem4Label->SetVisible(false);

	mDiffTitleLabel->SetVisible(false);
	mDiffItem1Label->SetVisible(false);
	mDiffItem2Label->SetVisible(false);
	mDiffItem3Label->SetVisible(false);
	mDiffBackLabel->SetVisible(false);

	mInstrTitleLabel->SetVisible(false);
	mInstrLine1Label->SetVisible(false);
	mInstrLine2Label->SetVisible(false);
	mInstrLine3Label->SetVisible(false);
	mInstrLine4Label->SetVisible(false);
	mInstrLine5Label->SetVisible(false);
	mInstrLine6Label->SetVisible(false);
	mInstrLine7Label->SetVisible(false);
	mInstrBackLabel->SetVisible(false);

	mNotificationLabel->SetVisible(false);
	mRingStatusLabel->SetVisible(false);
	mSpreadStatusLabel->SetVisible(false);
	mSpeedLabel->SetVisible(false);
	mDashStatusLabel->SetVisible(false);
	mBrakeStatusLabel->SetVisible(false);

	mHSTitleLabel->SetVisible(false);
	for (int i = 0; i < 10; i++) mHSEntryLabels[i]->SetVisible(false);
	mHSBackLabel->SetVisible(false);

	mEnterTagTitleLabel->SetVisible(false);
	mEnterTagScoreLabel->SetVisible(false);
	mEnterTagPromptLabel->SetVisible(false);
	mEnterTagInputLabel->SetVisible(false);
	mEnterTagHintLabel->SetVisible(false);
}

void Asteroids::ShowMenuState(MenuState state)
{
	mMenuState = state;
	HideAllLabels();

	switch (state)
	{
	case MENU_MAIN:
		UpdateDifficultyLabel();
		mMenuTitleLabel->SetVisible(true);
		mMenuItem1Label->SetVisible(true);
		mMenuItem2Label->SetVisible(true);
		mMenuItem3Label->SetVisible(true);
		mMenuItem4Label->SetVisible(true);
		break;

	case MENU_DIFFICULTY:
		mDiffTitleLabel->SetVisible(true);
		mDiffItem1Label->SetVisible(true);
		mDiffItem2Label->SetVisible(true);
		mDiffItem3Label->SetVisible(true);
		mDiffBackLabel->SetVisible(true);
		break;

	case MENU_INSTRUCTIONS:
		mInstrTitleLabel->SetVisible(true);
		mInstrLine1Label->SetVisible(true);
		mInstrLine2Label->SetVisible(true);
		mInstrLine3Label->SetVisible(true);
		mInstrLine4Label->SetVisible(true);
		mInstrLine5Label->SetVisible(true);
		mInstrLine6Label->SetVisible(true);
		mInstrLine7Label->SetVisible(true);
		mInstrBackLabel->SetVisible(true);
		break;

	case MENU_HIGHSCORES:
		UpdateHighScoreLabels();
		mHSTitleLabel->SetVisible(true);
		for (int i = 0; i < (int)mHighScores.size() && i < 10; i++)
			mHSEntryLabels[i]->SetVisible(true);
		if (mHighScores.empty())
			mHSEntryLabels[0]->SetVisible(true);
		mHSBackLabel->SetVisible(true);
		break;

	case MENU_ENTER_TAG:
		mEnterTagTitleLabel->SetVisible(true);
		mEnterTagScoreLabel->SetVisible(true);
		mEnterTagPromptLabel->SetVisible(true);
		mEnterTagInputLabel->SetVisible(true);
		mEnterTagHintLabel->SetVisible(true);
		break;

	case STATE_PLAYING:
		mScoreLabel->SetVisible(true);
		mLivesLabel->SetVisible(true);
		mRingStatusLabel->SetVisible(true);
		mSpreadStatusLabel->SetVisible(true);
		mSpeedLabel->SetVisible(true);
		mDashStatusLabel->SetVisible(true);
		mBrakeStatusLabel->SetVisible(true);
		break;
	}
}

void Asteroids::UpdateDifficultyLabel()
{
	const char* names[] = { "EASY", "NORMAL", "HARD" };
	std::string text = std::string("2 - Difficulty  [") + names[mDifficulty] + "]";
	mMenuItem2Label->SetText(text);
	if      (mDifficulty == 0) mMenuItem2Label->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	else if (mDifficulty == 1) mMenuItem2Label->SetColor(GLVector3f(1.0f, 1.0f, 0.3f));
	else                       mMenuItem2Label->SetColor(GLVector3f(1.0f, 0.3f, 0.3f));
}

void Asteroids::UpdateHighScoreLabels()
{
	if (mHighScores.empty())
	{
		mHSEntryLabels[0]->SetText("  (no entries yet)");
		return;
	}
	for (int i = 0; i < 10; i++)
	{
		if (i < (int)mHighScores.size())
		{
			std::ostringstream ss;
			ss << (i + 1) << ".  ";
			std::string entry = ss.str() + mHighScores[i].name;
			while ((int)entry.length() < 22) entry += " ";
			entry += std::to_string(mHighScores[i].score);
			mHSEntryLabels[i]->SetText(entry);
		}
		else
		{
			mHSEntryLabels[i]->SetText("");
		}
	}
}

void Asteroids::AddHighScore(const std::string& name, int score)
{
	HighScoreEntry entry;
	entry.name = name;
	entry.score = score;
	mHighScores.push_back(entry);
	std::sort(mHighScores.begin(), mHighScores.end(),
		[](const HighScoreEntry& a, const HighScoreEntry& b) {
			return a.score > b.score;
		});
	if (mHighScores.size() > 10)
		mHighScores.resize(10);

	// Save to file so scores persist between sessions
	std::ofstream file("highscores.txt");
	if (file.is_open())
	{
		for (auto& e : mHighScores)
			file << e.name << ":" << e.score << "\n";
		file.close();
	}
}

void Asteroids::ClearNearbyAsteroids(GLVector3f pos, float radius)
{
	GameObjectList objects = mGameWorld->GetObjects();
	for (GameObjectList::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		shared_ptr<GameObject> obj = *it;
		if (obj->GetType() != GameObjectType("Asteroid")) continue;
		GLVector3f apos = obj->GetPosition();
		float dx = apos.x - pos.x;
		float dy = apos.y - pos.y;
		if (sqrtf(dx * dx + dy * dy) < radius)
			mGameWorld->RemoveObject(obj);
	}
}

void Asteroids::OnScoreChanged(int score)
{
	std::ostringstream msg_stream;
	msg_stream << "Score: " << score;
	mScoreLabel->SetText(msg_stream.str());

	if (mHasMilestones && score >= mNextMilestone)
	{
		mNextMilestone += 250;

		if (mPlayer.GetLives() < 5)
		{
			mPlayer.AddLife();
			std::ostringstream lives_stream;
			lives_stream << "Lives: " << mPlayer.GetLives();
			mLivesLabel->SetText(lives_stream.str());
		}

		mControlLevel++;
		if (mSpaceship)
			mSpaceship->SetControlLevel(mControlLevel);

		if (mBlackHoleInterval > 5000)
			mBlackHoleInterval -= 2000;

		std::ostringstream note;
		note << "MILESTONE! +1 Life  |  Controls Lv." << mControlLevel;
		ShowNotification(note.str());
	}
}

void Asteroids::ShowNotification(const std::string& msg)
{
	mNotificationLabel->SetText(msg);
	mNotificationLabel->SetVisible(true);
	SetTimer(2500, HIDE_NOTIFICATION);
}

void Asteroids::OnPlayerKilled(int lives_left)
{
	mDeathPosition = mSpaceship->GetPosition();

	shared_ptr<GameObject> explosion = CreateExplosion();
	explosion->SetPosition(mDeathPosition);
	explosion->SetRotation(mSpaceship->GetRotation());
	mGameWorld->AddObject(explosion);

	std::ostringstream msg_stream;
	msg_stream << "Lives: " << lives_left;
	mLivesLabel->SetText(msg_stream.str());

	if (lives_left > 0)
		SetTimer(1000, CREATE_NEW_PLAYER);
	else
		SetTimer(1500, SHOW_GAME_OVER);
}

shared_ptr<GameObject> Asteroids::CreateExplosion()
{
	Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("explosion");
	shared_ptr<Sprite> explosion_sprite =
		make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
	explosion_sprite->SetLoopAnimation(false);
	shared_ptr<GameObject> explosion = make_shared<Explosion>();
	explosion->SetSprite(explosion_sprite);
	explosion->Reset();
	return explosion;
}

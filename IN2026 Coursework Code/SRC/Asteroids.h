#ifndef __ASTEROIDS_H__
#define __ASTEROIDS_H__

#include "GameUtil.h"
#include "GameSession.h"
#include "IKeyboardListener.h"
#include "IGameWorldListener.h"
#include "IScoreListener.h"
#include "ScoreKeeper.h"
#include "Player.h"
#include "IPlayerListener.h"
#include <vector>
#include <algorithm>

class GameObject;
class Spaceship;
class GUILabel;

class Asteroids : public GameSession, public IKeyboardListener, public IGameWorldListener, public IScoreListener, public IPlayerListener
{
public:
	Asteroids(int argc, char *argv[]);
	virtual ~Asteroids(void);

	virtual void Start(void);
	virtual void Stop(void);

	void OnKeyPressed(uchar key, int x, int y);
	void OnKeyReleased(uchar key, int x, int y);
	void OnSpecialKeyPressed(int key, int x, int y);
	void OnSpecialKeyReleased(int key, int x, int y);

	void OnScoreChanged(int score);
	void OnPlayerKilled(int lives_left);

	void OnWorldUpdated(GameWorld* world);
	void OnObjectAdded(GameWorld* world, shared_ptr<GameObject> object) {}
	void OnObjectRemoved(GameWorld* world, shared_ptr<GameObject> object);

	void OnTimer(int value);

private:
	enum MenuState {
		MENU_MAIN,
		MENU_DIFFICULTY,
		MENU_INSTRUCTIONS,
		MENU_HIGHSCORES,
		MENU_ENTER_TAG,
		STATE_PLAYING
	};

	struct HighScoreEntry {
		std::string name;
		int score;
	};

	// Game state
	bool mGameStarted;
	MenuState mMenuState;
	int  mDifficulty;        // 0=Easy, 1=Normal, 2=Hard
	bool mHasPowerUps;       // computed at StartGame
	bool mHasBlackHoles;
	bool mHasMilestones;
	std::string mCurrentInput;
	int mFinalScore;
	std::vector<HighScoreEntry> mHighScores;

	// Game objects
	shared_ptr<Spaceship> mSpaceship;
	list<shared_ptr<GameObject>> mBackgroundAsteroids;

	// In-game GUI labels
	shared_ptr<GUILabel> mScoreLabel;
	shared_ptr<GUILabel> mLivesLabel;
	shared_ptr<GUILabel> mGameOverLabel;

	// Main menu labels
	shared_ptr<GUILabel> mMenuTitleLabel;
	shared_ptr<GUILabel> mMenuItem1Label;
	shared_ptr<GUILabel> mMenuItem2Label;
	shared_ptr<GUILabel> mMenuItem3Label;
	shared_ptr<GUILabel> mMenuItem4Label;

	// Difficulty menu labels
	shared_ptr<GUILabel> mDiffTitleLabel;
	shared_ptr<GUILabel> mDiffItem1Label;
	shared_ptr<GUILabel> mDiffItem2Label;
	shared_ptr<GUILabel> mDiffItem3Label;
	shared_ptr<GUILabel> mDiffBackLabel;

	// Instructions labels
	shared_ptr<GUILabel> mInstrTitleLabel;
	shared_ptr<GUILabel> mInstrLine1Label;
	shared_ptr<GUILabel> mInstrLine2Label;
	shared_ptr<GUILabel> mInstrLine3Label;
	shared_ptr<GUILabel> mInstrLine4Label;
	shared_ptr<GUILabel> mInstrLine5Label;
	shared_ptr<GUILabel> mInstrLine6Label;
	shared_ptr<GUILabel> mInstrLine7Label;
	shared_ptr<GUILabel> mInstrBackLabel;

	// In-game notification label
	shared_ptr<GUILabel> mNotificationLabel;

	// Power-up HUD indicators (bottom-right)
	shared_ptr<GUILabel> mRingStatusLabel;
	shared_ptr<GUILabel> mSpreadStatusLabel;

	// Speed / brake / dash HUD
	shared_ptr<GUILabel> mSpeedLabel;
	shared_ptr<GUILabel> mBrakeStatusLabel;
	shared_ptr<GUILabel> mDashStatusLabel;

	// High score labels
	shared_ptr<GUILabel> mHSTitleLabel;
	shared_ptr<GUILabel> mHSEntryLabels[10];
	shared_ptr<GUILabel> mHSBackLabel;

	// Enter tag labels
	shared_ptr<GUILabel> mEnterTagTitleLabel;
	shared_ptr<GUILabel> mEnterTagScoreLabel;
	shared_ptr<GUILabel> mEnterTagPromptLabel;
	shared_ptr<GUILabel> mEnterTagInputLabel;
	shared_ptr<GUILabel> mEnterTagHintLabel;

	uint mLevel;
	uint mAsteroidCount;
	int  mNextMilestone;
	int  mControlLevel;
	int  mBlackHoleInterval;
	int  mTitleAnimTimer;
	GLVector3f mDeathPosition;

	void StartGame();
	void ResetGame();
	void ShowNotification(const std::string& msg);
	void ClearNearbyAsteroids(GLVector3f pos, float radius);
	shared_ptr<GameObject> CreateSpaceship();
	void CreateGUI();
	void CreateAsteroids(const uint num_asteroids);
	void CreateBackgroundAsteroids(const uint num_asteroids);
	shared_ptr<GameObject> CreateExplosion();
	void ShowMenuState(MenuState state);
	void HideAllLabels();
	void UpdateDifficultyLabel();
	void UpdateHighScoreLabels();
	void AddHighScore(const std::string& name, int score);

	const static uint SHOW_GAME_OVER      = 0;
	const static uint START_NEXT_LEVEL    = 1;
	const static uint CREATE_NEW_PLAYER   = 2;
	const static uint HIDE_NOTIFICATION   = 3;
	const static uint SPAWN_BLACK_HOLE    = 4;

	ScoreKeeper mScoreKeeper;
	Player mPlayer;
};

#endif

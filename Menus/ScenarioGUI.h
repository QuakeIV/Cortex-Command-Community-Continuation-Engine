#ifndef _RTESCENARIOGUI_
#define _RTESCENARIOGUI_

#include "Activity.h"
#include "Timer.h"

namespace RTE {

	class GUIScreen;
	class GUIInput;
	class GUIControlManager;
	class GUICollectionBox;
	class GUIComboBox;
	class GUICheckbox;
	class GUIButton;
	class GUILabel;
	class GUISlider;
	class Scene;
	class Activity;
	class AllegroBitmap;

	/// <summary>
	/// A menu for setting up and launching scenario games.
	/// </summary>
	class ScenarioGUI {

	public:

		/// <summary>
		/// 
		/// </summary>
		enum ScenarioUpdateResult {
			NOEVENT = 0,
			BACKTOMAIN,
			ACTIVITYRESUMED,
			ACTIVITYRESTARTED
		};

#pragma region Creation
		/// <summary>
		/// Constructor method used to instantiate this ScenarioGUI object in system memory.
		/// </summary>
		explicit ScenarioGUI(Controller *controller);
#pragma endregion

#pragma region Concrete Methods
		/// <summary>
		/// Enables the menu by making the activity box visible and fetching the available activities and scenes.
		/// </summary>
		void SetEnabled();

		/// <summary>
		/// Sets where the planet is on the screen and its other data so the menu can overlay properly on it.
		/// </summary>
		/// <param name="center">The absolute screen coordinates of the planet's center.</param>
		/// <param name="radius">The radius, in screen pixel units, of the planet.</param>
		void SetPlanetInfo(const Vector &center, float radius);

		/// <summary>
		/// Updates the user input processing.
		/// </summary>
		ScenarioUpdateResult UpdateInput();

		/// <summary>
		/// Updates the state of this Menu each frame.
		/// </summary>
		void Update();

		/// <summary>
		/// Draws the menu.
		/// </summary>
		/// <param name="drawBitmap">The bitmap to draw on.</param>
		void Draw(BITMAP *drawBitmap) const;
#pragma endregion

	private:

		/// <summary>
		/// Enumeration for the GUICollectionBoxes of the Scenario GUI. Used to access elements in the CollectionBoxes array.
		/// </summary>
		enum ScenarioCollections {
			RootBox,
			ActivitySelectBox,
			SceneInfoBox,
			PlayerSetupBox,
			CollectionBoxCount
		};

		/// <summary>
		/// Enumeration for the GUIButtons of the Scenario GUI. Used to access elements in the Buttons array.
		/// </summary>
		enum ScenarioButtons {
			BackToMainButton,
			StartHereButton,
			StartGameButton,
			ResumeButton,
			ButtonCount
		};

		/// <summary>
		/// These add on the player and team max counts.
		/// </summary>
		enum PlayerColumns {
			PlayerCPU = Players::MaxPlayerCount,
			PlayerColumnCount
		};

		/// <summary>
		/// 
		/// </summary>
		enum TeamRows {
			DisabledTeam = Activity::Teams::MaxTeamCount,
			TeamRowCount
		};

		Controller *m_Controller; //!< The Controller which controls this menu. Not owned.

		std::unique_ptr<GUIScreen> m_GUIScreen; //!< GUI Screen for use by the in-game GUI.
		std::unique_ptr<GUIInput> m_GUIInput; //!< Input controller.
		std::unique_ptr<GUIControlManager> m_ScenarioGUIController; //!< The control manager which owns all the GUI elements.

		std::array<GUICollectionBox *, ScenarioCollections::CollectionBoxCount> m_ScenarioCollectionBoxes; //!< The different dialog/floating boxes.
		std::array<GUIButton *, ScenarioButtons::ButtonCount> m_ScenarioButtons; //!< The menu buttons we want to manipulate.

		Timer m_BlinkTimer; //!< Notification blink timer.

		GUICollectionBox *m_DraggedBox; //!< Currently dragged GUI box.
		Vector m_PrevMousePos; //!< Previous pos of mouse to calculate dragging.

		Vector m_PlanetCenter; //!< The absolute screen position of the planet center.
		float m_PlanetRadius = 240.0F; //!< The screen radius of the planet.

		//std::list<Scene *> *m_ScenarioScenes; //!< Pointer to the current set of Scenes being displayed. Not owned, and neither are the scenes.
		std::vector<Scene *> *m_ScenarioScenes;

		Scene *m_HoveredScene; //!< The scene preset currently hovered. Not owned.
		Scene *m_SelectedScene; //!< The scene preset currently selected. Not owned.
		GUILabel *m_SitePointLabel; //!< Hover name label over Scenes.

		std::vector<Vector> m_LineToSitePoints; //!< Collection of points that form lines from a screen point to the selected site point.

		// Activity selection.
		GUIComboBox *m_ActivitySelectComboBox;
		GUILabel *m_ActivityLabel;
		GUILabel *m_DifficultyLabel;
		GUISlider *m_DifficultySlider;

		//std::map<Activity *, std::list<Scene *>> m_ScenarioActivities; //!< The map of Activities and the Scenes compatible with each, neither of which are owned here.
		std::map<Activity *, std::vector<Scene *>> m_ScenarioActivities; //!< The map of Activities and the Scenes compatible with each, neither of which are owned here.
		const Activity *m_SelectedActivity; //!< The currently selected activity. Not owned.

		// Scene Info.
		GUIButton *m_SceneCloseButton;
		GUILabel *m_SceneNameLabel;
		GUILabel *m_SceneInfoLabel;
		GUICollectionBox *m_ScenePreviewBox;

		std::unique_ptr<AllegroBitmap> m_ScenePreviewBitmap = std::make_unique<AllegroBitmap>();
		std::unique_ptr<AllegroBitmap> m_DefaultPreviewBitmap = std::make_unique<AllegroBitmap>();

		// Player setup.
		std::array<std::array<GUICollectionBox *, TeamRows::TeamRowCount>, PlayerColumns::PlayerColumnCount> m_PlayerBoxes;
		std::array<GUICollectionBox *, TeamRows::TeamRowCount> m_TeamBoxes;
		std::array<GUILabel *, TeamRows::TeamRowCount> m_TeamNameLabels;
		GUILabel *m_StartErrorLabel;
		GUILabel *m_CPULockLabel;
		GUILabel *m_GoldLabel;
		GUISlider *m_GoldSlider;
		GUICheckbox *m_FogOfWarCheckbox;
		GUICheckbox *m_RequireClearPathToOrbitCheckbox;
		GUICheckbox *m_DeployUnitsCheckbox;
		std::array<GUIComboBox *, Activity::Teams::MaxTeamCount> m_TeamTechSelect; //!< Tech selection ComboBox array.
		std::array<GUISlider *, Activity::Teams::MaxTeamCount> m_TeamAISkillSlider; //!< AI skill slider array.
		std::array<GUILabel *, Activity::Teams::MaxTeamCount> m_TeamAISkillLabel; //!< AI skill label array.

		int m_LockedCPUTeam = Activity::Teams::NoTeam; //!< Which team the CPU is locked to, if any.

#pragma region
		/// <summary>
		/// Shows the Scene info box.
		/// </summary>
		void ShowScenesBox();

		/// <summary>
		/// Shows the player config box.
		/// </summary>
		void ShowPlayersBox();

		/// <summary>
		/// Hides all menu screens, so a single screen can be unhidden and shown alone.
		/// </summary>
		void HideAllScreens();

		/// <summary>
		/// Makes sure a specific box doesn't move off-screen.
		/// </summary>
		/// <param name="screenBox">The GUICollectionBox to adjust, if necessary.</param>
		void KeepBoxInScreenBounds(GUICollectionBox *screenBox) const;
#pragma endregion

#pragma region
		/// <summary>
		/// Gathers all the available Scenes and Activity presets there are.
		/// </summary>
		void GetScenesAndActivities(bool selectTutorial);

		/// <summary>
		/// Sets the currently selected scene.
		/// </summary>
		/// <param name="newSelectedScene">The new selected scene or nullptr to deselect the current selection.</param>
		void SetSelectedScene(Scene *newSelectedScene);
#pragma endregion

#pragma region
		/// <summary>
		/// 
		/// </summary>
		/// <param name="hoveredPlayer"></param>
		/// <param name="hoveredTeam"></param>
		void ClickInPlayerSetup(int hoveredPlayer, int hoveredTeam);

		/// <summary>
		/// Sets up and starts the currently selected Activity and settings.
		/// </summary>
		/// <returns>Whether the game was set up and started successfully.</returns>
		bool StartGame();
#pragma endregion

#pragma region Planet Site Handling
		/// <summary>
		/// Calculates how to draw lines from the scene info box to the selected site point on the planet.
		/// </summary>
		void CalculateLinesToSitePoint();

		/// <summary>
		/// Draws the site points on top of the planet.
		/// </summary>
		/// <param name="drawBitmap">The bitmap to draw on.</param>
		void DrawSitePoints(BITMAP *drawBitmap) const;

		/// <summary>
		/// Draws fancy thick flickering lines to point out the selected scene point on the planet, from the scene info box.
		/// </summary>
		/// <param name="drawBitmap">The bitmap to draw to.</param>
		void DrawLineToSitePoint(BITMAP *drawBitmap) const;
#pragma endregion

#pragma region Updates
		/// <summary>
		/// Updates the contents of the Activity selection box.
		/// </summary>
		void UpdateActivityBox();

		/// <summary>
		/// Updates the contents of the player config box.
		/// </summary>
		void UpdatePlayersBox();

		/// <summary>
		/// Displays the site's name label if the mouse is over a site point. Otherwise the label is hidden.
		/// </summary>
		void UpdateHoveredScene(int mouseX, int mouseY);
#pragma endregion

		// Disallow the use of some implicit methods.
		ScenarioGUI(const ScenarioGUI &reference) = delete;
		ScenarioGUI & operator=(const ScenarioGUI &rhs) = delete;
	};
}
#endif
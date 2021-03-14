#include "ObjectPickerGUI.h"

#include "FrameMan.h"
#include "PresetMan.h"
#include "ActivityMan.h"
#include "UInputMan.h"
#include "SettingsMan.h"

#include "GUI/GUI.h"
#include "GUI/AllegroBitmap.h"
#include "GUI/AllegroScreen.h"
#include "GUI/AllegroInput.h"
#include "GUI/GUIControlManager.h"
#include "GUI/GUICollectionBox.h"
#include "GUI/GUIListBox.h"
#include "GUI/GUILabel.h"

#include "DataModule.h"
#include "SceneObject.h"
#include "EditorActivity.h"
#include "BunkerAssembly.h"
#include "BunkerAssemblyScheme.h"

namespace RTE {

	BITMAP *ObjectPickerGUI::s_Cursor = nullptr;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::Clear() {
		m_CursorPos.Reset();
		m_GUIScreen = nullptr;
		m_GUIInput = nullptr;
		m_GUIControlManager = nullptr;
		m_ParentBox = nullptr;
		m_PopupBox = nullptr;
		m_PopupText = nullptr;
		m_GroupsList = nullptr;
		m_ObjectsList = nullptr;
		m_Controller = nullptr;
		m_PickerState = PickerState::Disabled;
		m_PickerFocus = PickerFocus::GroupList;
		m_OpenCloseSpeed = 0.3F;
		m_ModuleSpaceID = -1;
		m_ShowType.clear();
		m_NativeTechModule = 0;
		m_ForeignCostMult = 4.0F;
		m_SelectedGroupIndex = 0;
		m_SelectedObjectIndex = 0;
		m_PickedObject = nullptr;
		m_RepeatStartTimer.Reset();
		m_RepeatTimer.Reset();

		m_ExpandedModules.resize(g_PresetMan.GetTotalModuleCount());
		m_ExpandedModules.at(0) = true; // Base.rte is always expanded
		for (int moduleID = 1; moduleID < m_ExpandedModules.size(); ++moduleID) {
			m_ExpandedModules.at(moduleID) = false;
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int ObjectPickerGUI::Create(Controller *controller, int whichModuleSpace, const std::string_view &onlyOfType) {
		RTEAssert(controller, "No controller sent to ObjectPickerGUI on creation!");
		m_Controller = controller;

		if (!m_GUIScreen) { m_GUIScreen.reset(new AllegroScreen(g_FrameMan.GetBackBuffer8())); }
		if (!m_GUIInput) { m_GUIInput.reset(new AllegroInput(controller->GetPlayer())); }
		if (!m_GUIControlManager) { m_GUIControlManager.reset(new GUIControlManager()); }
		if (!m_GUIControlManager->Create(m_GUIScreen.get(), m_GUIInput.get(), "Base.rte/GUIs/Skins/Base")) { RTEAbort("Failed to create GUI Control Manager and load it from Base.rte/GUIs/Skins/Base"); }

		m_GUIControlManager->Load("Base.rte/GUIs/ObjectPickerGUI.ini");
		m_GUIControlManager->EnableMouse(controller->IsMouseControlled());

		if (!s_Cursor) {
			ContentFile cursorFile("Base.rte/GUIs/Skins/Cursor.png");
			s_Cursor = cursorFile.GetAsBitmap();
		}

		// Stretch the invisible root box to fill the screen
		if (g_FrameMan.IsInMultiplayerMode()) {
			dynamic_cast<GUICollectionBox *>(m_GUIControlManager->GetControl("base"))->SetSize(g_FrameMan.GetPlayerFrameBufferWidth(controller->GetPlayer()), g_FrameMan.GetPlayerFrameBufferHeight(controller->GetPlayer()));
		} else {
			dynamic_cast<GUICollectionBox *>(m_GUIControlManager->GetControl("base"))->SetSize(g_FrameMan.GetResX(), g_FrameMan.GetResY());
		}

		if (!m_ParentBox) { m_ParentBox = dynamic_cast<GUICollectionBox *>(m_GUIControlManager->GetControl("PickerGUIBox")); }
		m_ParentBox->SetPositionAbs(g_FrameMan.GetPlayerFrameBufferWidth(m_Controller->GetPlayer()), 0);
		m_ParentBox->SetEnabled(false);
		m_ParentBox->SetVisible(false);
		m_GroupsList = dynamic_cast<GUIListBox *>(m_GUIControlManager->GetControl("GroupsLB"));
		m_GroupsList->SetAlternateDrawMode(false);
		m_GroupsList->SetMultiSelect(false);
		m_ObjectsList = dynamic_cast<GUIListBox *>(m_GUIControlManager->GetControl("ObjectsLB"));
		m_ObjectsList->SetAlternateDrawMode(true);
		m_ObjectsList->SetMultiSelect(false);

		// Stretch out the layout for all the relevant controls
		int stretchAmount = g_FrameMan.IsInMultiplayerMode() ? (g_FrameMan.GetPlayerFrameBufferHeight(m_Controller->GetPlayer()) - m_ParentBox->GetHeight()) : (g_FrameMan.GetPlayerScreenHeight() - m_ParentBox->GetHeight());
		if (stretchAmount != 0) {
			m_ParentBox->SetSize(m_ParentBox->GetWidth(), m_ParentBox->GetHeight() + stretchAmount);
			m_GroupsList->SetSize(m_GroupsList->GetWidth(), m_GroupsList->GetHeight() + stretchAmount);
			m_ObjectsList->SetSize(m_ObjectsList->GetWidth(), m_ObjectsList->GetHeight() + stretchAmount);
		}

		if (!m_PopupBox) {
			m_PopupBox = dynamic_cast<GUICollectionBox *>(m_GUIControlManager->GetControl("BuyGUIPopup"));
			m_PopupText = dynamic_cast<GUILabel *>(m_GUIControlManager->GetControl("PopupText"));
			m_PopupText->SetFont(m_GUIControlManager->GetSkin()->GetFont("smallfont.png"));

			// Never enable the popup box because it steals focus and causes other windows to think the cursor left them
			m_PopupBox->SetEnabled(false);
			m_PopupBox->SetVisible(false);
		}

		// Set up the groups to show from the module space and only the objects of the specific type we are working within. This also updates the Objects list
		SetModuleSpace(whichModuleSpace);
		ShowOnlyType(onlyOfType);

		return 0;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::SetEnabled(bool enable) {
		if (enable && m_PickerState != PickerState::Enabled && m_PickerState != PickerState::Enabling) {
			m_PickerState = PickerState::Enabling;
			g_UInputMan.TrapMousePos(false, m_Controller->GetPlayer());
			// Move the mouse cursor to the middle of the player's screen
			m_CursorPos.SetXY(static_cast<float>(g_FrameMan.GetPlayerFrameBufferWidth(m_Controller->GetPlayer()) / 2), static_cast<float>(g_FrameMan.GetPlayerFrameBufferHeight(m_Controller->GetPlayer()) / 2));
			g_UInputMan.SetMousePos(m_CursorPos, m_Controller->GetPlayer());

			// Default focus to the groups list if the objects are empty
			SetListFocus(m_ObjectsList->GetItemList()->empty() ? PickerFocus::GroupList : PickerFocus::ObjectList);

			m_RepeatStartTimer.Reset();
			m_RepeatTimer.Reset();

			g_GUISound.EnterMenuSound()->Play(m_Controller->GetPlayer());
		} else if (!enable && m_PickerState != PickerState::Disabled && m_PickerState != PickerState::Disabling) {
			m_PickerState = PickerState::Disabling;
			g_UInputMan.TrapMousePos(true, m_Controller->GetPlayer());
			g_GUISound.ExitMenuSound()->Play(m_Controller->GetPlayer());
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::SetPosOnScreen(int newPosX, int newPosY) const {
		m_GUIControlManager->SetPosOnScreen(newPosX, newPosY);
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::SetModuleSpace(int newModuleSpaceID) {
		if (newModuleSpaceID != m_ModuleSpaceID) {
			m_ModuleSpaceID = newModuleSpaceID;
			UpdateGroupsList();
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool ObjectPickerGUI::ShowSpecificGroup(const std::string_view &groupName) {
		int index = 0;
		for (const GUIListPanel::Item *groupListItem : *m_GroupsList->GetItemList()) {
			if (groupListItem->m_Name == groupName) {
				m_SelectedGroupIndex = index;
				m_GroupsList->SetSelectedIndex(m_SelectedGroupIndex);
				UpdateObjectsList();
				SetListFocus(PickerFocus::ObjectList);
				return true;
			}
			index++;
		}
		return false;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::SetNativeTechModule(int whichModule) {
		if (whichModule >= 0 && whichModule < g_PresetMan.GetTotalModuleCount()) {
			// Set the multipliers and refresh everything that needs refreshing to reflect the change
			m_NativeTechModule = whichModule;
			// ObjectPicker is first created with NativeTechModule = 0, which expands all modules but does not display them as expanded
			// When visually collapsed module is first clicked, it internally turns itself into collapsed while the others display themselves as expanded
			// To avoid that, just don't expand anything if we pass Base.rte as a native tech as Base.rte is always expanded by default anyway
			if (whichModule > 0) { SetModuleExpanded(m_NativeTechModule); }
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::SetModuleExpanded(int whichModule, bool expanded) {
		int moduleCount = g_PresetMan.GetTotalModuleCount();
		if (whichModule > 0 && whichModule < moduleCount) {
			m_ExpandedModules.at(whichModule) = expanded;
			UpdateObjectsList(false);
		} else {
			// If base module (0), or out of range module, then affect all
			for (int moduleID = 0; moduleID < moduleCount; ++moduleID) {
				m_ExpandedModules.at(moduleID) = expanded;
			}
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool ObjectPickerGUI::SetListFocus(PickerFocus listToFocusOn) {
		if (listToFocusOn == m_PickerFocus) {
			return false;
		}
		if (listToFocusOn == PickerFocus::GroupList) {
			m_PickerFocus = PickerFocus::GroupList;
			m_GroupsList->SetFocus();
			if (!m_GroupsList->GetItemList()->empty() && m_GroupsList->GetSelectedIndex() < 0) {
				m_SelectedGroupIndex = 0;
				m_GroupsList->SetSelectedIndex(m_SelectedGroupIndex);
			} else {
				m_SelectedGroupIndex = m_GroupsList->GetSelectedIndex();
				m_GroupsList->ScrollToSelected();
			}
		} else if (listToFocusOn == PickerFocus::ObjectList) {
			m_PickerFocus = PickerFocus::ObjectList;
			m_ObjectsList->SetFocus();
			if (!m_ObjectsList->GetItemList()->empty() && m_ObjectsList->GetSelectedIndex() < 0) {
				m_SelectedObjectIndex = 0;
				m_ObjectsList->SetSelectedIndex(m_SelectedObjectIndex);
			} else {
				m_SelectedObjectIndex = m_ObjectsList->GetSelectedIndex();
				m_ObjectsList->ScrollToSelected();
			}
		}
		g_GUISound.FocusChangeSound()->Play(m_Controller->GetPlayer());
		return true;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const SceneObject * ObjectPickerGUI::GetNextOrPrevObject(bool getPrev) {
		if (getPrev) {
			m_SelectedObjectIndex--;
			if (m_SelectedObjectIndex < 0) { m_SelectedObjectIndex = m_ObjectsList->GetItemList()->size() - 1; }
		} else {
			m_SelectedObjectIndex++;
			if (m_SelectedObjectIndex >= m_ObjectsList->GetItemList()->size()) { m_SelectedObjectIndex = 0; }
		}
		m_ObjectsList->SetSelectedIndex(m_SelectedObjectIndex);
		// Report the newly selected item as being 'picked', but don't close the picker
		const GUIListPanel::Item *selectedItem = m_ObjectsList->GetSelected();
		if (selectedItem) {
			g_GUISound.SelectionChangeSound()->Play(m_Controller->GetPlayer());
			return dynamic_cast<const SceneObject *>(selectedItem->m_pEntity);
		}
		return nullptr;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::ShowDescriptionPopupBox() {
		std::string description = "";
		GUIListPanel::Item *objectListItem = m_ObjectsList->GetSelected();
		if (objectListItem && objectListItem->m_pEntity && !objectListItem->m_pEntity->GetDescription().empty()) {
			description = objectListItem->m_pEntity->GetDescription();
		} else if (objectListItem && objectListItem->m_ExtraIndex >= 0) {
			// Description for module group items
			const DataModule *dataModule = g_PresetMan.GetDataModule(objectListItem->m_ExtraIndex);
			if (dataModule && !dataModule->GetDescription().empty()) { description = dataModule->GetDescription(); }
		}
		if (!description.empty()) {
			m_PopupBox->SetEnabled(false);
			m_PopupBox->SetVisible(true);
			// Need to add an offset to make it look better and not have the cursor obscure text
			m_PopupBox->SetPositionAbs(m_ObjectsList->GetXPos() - m_PopupBox->GetWidth() + 4, m_ObjectsList->GetYPos() + m_ObjectsList->GetStackHeight(objectListItem) - m_ObjectsList->GetScrollVerticalValue());
			// Make sure the popup box doesn't drop out of sight
			if (m_PopupBox->GetYPos() + m_PopupBox->GetHeight() > m_ParentBox->GetHeight()) { m_PopupBox->SetPositionAbs(m_PopupBox->GetXPos(), m_ParentBox->GetHeight() - m_PopupBox->GetHeight()); }

			m_PopupText->SetHAlignment(GUIFont::Left);
			m_PopupText->SetText(description);
			m_PopupBox->Resize(m_PopupBox->GetWidth(), m_PopupText->ResizeHeightToFit() + 10);
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::UpdateGroupsList() {
		m_GroupsList->ClearList();
		bool showSchemes = dynamic_cast<EditorActivity *>(g_ActivityMan.GetActivity());

		// Get the registered groups of all official modules loaded before this + the specific module (official or not) one we're picking from
		std::list<std::string> groupList;
		g_PresetMan.GetModuleSpaceGroups(groupList, m_ModuleSpaceID, m_ShowType);

		for (const std::string &groupListEntry : groupList) {
			// Get the actual object list for each group so we can check if they're empty or not
			std::list<Entity *> objectList;
			g_PresetMan.GetAllOfGroupInModuleSpace(objectList, groupListEntry, m_ShowType, m_ModuleSpaceID);

			bool onlyAssembliesInGroup = true;
			bool onlySchemesInGroup = true;
			bool hasObjectsToShow = false;

			for (Entity *objectListEntry : objectList) {
				// Check if we have any other objects than assemblies to skip assembly groups
				if (!dynamic_cast<BunkerAssembly *>(objectListEntry)) { onlyAssembliesInGroup = false; }
				// Check if we have any other objects than schemes to skip schemes group
				if (!dynamic_cast<BunkerAssemblyScheme *>(objectListEntry)) { onlySchemesInGroup = false; }

				const SceneObject *sceneObject = dynamic_cast<SceneObject *>(objectListEntry);
				if (sceneObject && sceneObject->IsBuyable()) {
					hasObjectsToShow = true;
					break;
				}
			}
			// If we have this assembly group in the list of visible assembly groups, then show it no matter what
			if (onlyAssembliesInGroup) {
				for (const std::string &assemblyGroup : g_SettingsMan.GetVisibleAssemblyGroupsList()) {
					if (groupListEntry == assemblyGroup) {
						onlyAssembliesInGroup = false;
						break;
					}
				}
			}
			// Only add the group if it has something in it!
			if (!objectList.empty() && hasObjectsToShow && (!onlyAssembliesInGroup || groupListEntry == "Assemblies") && (!onlySchemesInGroup || showSchemes)) { m_GroupsList->AddItem(groupListEntry); }
		}

		// Select and load the first group
		if (const GUIListPanel::Item *listItem = m_GroupsList->GetItem(0)) {
			m_GroupsList->ScrollToTop();
			m_SelectedGroupIndex = 0;
			m_GroupsList->SetSelectedIndex(m_SelectedGroupIndex);
			UpdateObjectsList();
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::UpdateObjectsList(bool selectTop) {
		m_ObjectsList->ClearList();
		std::vector<std::list<Entity *>> moduleList(g_PresetMan.GetTotalModuleCount(), std::list<Entity *>());

		if (const GUIListPanel::Item *groupListItem = m_GroupsList->GetSelected()) {
			// Show objects from ALL modules
			if (m_ModuleSpaceID < 0) {
				if (g_SettingsMan.ShowForeignItems() || m_NativeTechModule <= 0) {
					for (int moduleID = 0; moduleID < moduleList.size(); ++moduleID) {
						g_PresetMan.GetAllOfGroup(moduleList.at(moduleID), groupListItem->m_Name, m_ShowType, moduleID);
					}
				} else {
					for (int moduleID = 0; moduleID < moduleList.size(); ++moduleID) {
						if (moduleID == 0 || moduleID == m_NativeTechModule) { g_PresetMan.GetAllOfGroup(moduleList.at(moduleID), groupListItem->m_Name, m_ShowType, moduleID); }
					}
				}
			} else {
				for (int moduleID = 0; moduleID < g_PresetMan.GetOfficialModuleCount() && moduleID < m_ModuleSpaceID; ++moduleID) {
					g_PresetMan.GetAllOfGroup(moduleList.at(moduleID), groupListItem->m_Name, m_ShowType, moduleID);
				}
				g_PresetMan.GetAllOfGroup(moduleList.at(m_ModuleSpaceID), groupListItem->m_Name, m_ShowType, m_ModuleSpaceID);
			}
		}

		for (int moduleID = 0; moduleID < moduleList.size(); ++moduleID) {
			if (moduleList.at(moduleID).empty()) {
				continue;
			}
			std::list<SceneObject *> objectList;
			for (Entity *moduleListEntryEntity : moduleList.at(moduleID)) {
				SceneObject *sceneObject = dynamic_cast<SceneObject *>(moduleListEntryEntity);
				if (sceneObject && sceneObject->IsBuyable()) { objectList.emplace_back(sceneObject); }
			}
			if (!objectList.empty()) {
				// Add the DataModule separator in the item list with appropriate name and icon. Don't add for base module
				const DataModule *dataModule = g_PresetMan.GetDataModule(moduleID);
				if (moduleID != 0) {
					std::string moduleName = dataModule->GetFriendlyName();
					std::transform(moduleName.begin(), moduleName.end(), moduleName.begin(), ::toupper);
					GUIBitmap *dataModuleIcon = dataModule->GetIcon() ? new AllegroBitmap(dataModule->GetIcon()) : nullptr;
					m_ObjectsList->AddItem(moduleName, m_ExpandedModules.at(moduleID) ? "-" : "+", dataModuleIcon, nullptr, moduleID);
				}
				// If the module is expanded add all the items within it to the actual object list
				if (moduleID == 0 || m_ExpandedModules.at(moduleID)) {
					for (SceneObject *objectListEntry : objectList) {
						GUIBitmap *objectIcon = new AllegroBitmap(objectListEntry->GetGraphicalIcon());
						m_ObjectsList->AddItem(objectListEntry->GetPresetName(), objectListEntry->GetGoldValueString(m_NativeTechModule, m_ForeignCostMult), objectIcon, objectListEntry);
					}
				}
			}
		}
		if (selectTop) {
			m_ObjectsList->ScrollToTop();
			m_SelectedObjectIndex = 0;
			m_ObjectsList->SetSelectedIndex(m_SelectedObjectIndex);
		} else {
			m_ObjectsList->SetSelectedIndex(m_SelectedObjectIndex);
			m_ObjectsList->ScrollToSelected();
		}
		const GUIListPanel::Item *selectedItem = m_ObjectsList->GetSelected();
		if (selectedItem) { m_PickedObject = dynamic_cast<const SceneObject *>(selectedItem->m_pEntity); }
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::Update() {
		// Popup box is hidden by default
		m_PopupBox->SetVisible(false);

		if (m_PickerState != PickerState::Enabled && m_PickerState != PickerState::Disabled) { AnimateOpenClose(); }
		if (m_PickerState == PickerState::Enabled || m_PickerState == PickerState::Enabling) { m_GUIControlManager->Update(); }
		if (m_PickerState == PickerState::Enabled) {
			m_PickedObject = nullptr;

			// Enable mouse input if the controller allows it
			m_GUIControlManager->EnableMouse(m_Controller->IsMouseControlled());
			if (HandleInput()) {
				return;
			}
			if (m_PickerFocus == PickerFocus::ObjectList) { ShowDescriptionPopupBox(); }
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool ObjectPickerGUI::HandleInput() {
		bool objectPickedOrPickerClosed = false;
		if (m_Controller->IsMouseControlled()) { objectPickedOrPickerClosed = HandleMouseEvents(); }

		if (!objectPickedOrPickerClosed && !m_Controller->IsState(ControlState::PRESS_SECONDARY)) {
			bool pressUp = m_Controller->IsState(ControlState::PRESS_UP) || m_Controller->IsState(ControlState::SCROLL_UP);
			bool pressDown = m_Controller->IsState(ControlState::PRESS_DOWN) || m_Controller->IsState(ControlState::SCROLL_DOWN);

			if (!(m_Controller->IsState(ControlState::MOVE_UP) || m_Controller->IsState(ControlState::MOVE_DOWN))) {
				m_RepeatStartTimer.Reset();
				m_RepeatTimer.Reset();
			}

			// Check if any direction has been held for the starting amount of time to get into repeat mode
			if (m_RepeatStartTimer.IsPastRealMS(200) && m_RepeatTimer.IsPastRealMS(70)) {
				if (m_Controller->IsState(ControlState::MOVE_UP)) {
					pressUp = true;
				} else if (m_Controller->IsState(ControlState::MOVE_DOWN)) {
					pressDown = true;
				}
				m_RepeatTimer.Reset();
			}

			if ((m_Controller->IsState(ControlState::PRESS_LEFT) && !SetListFocus(PickerFocus::GroupList)) || (m_Controller->IsState(ControlState::PRESS_RIGHT) && !SetListFocus(PickerFocus::ObjectList))) {
				g_GUISound.UserErrorSound()->Play(m_Controller->GetPlayer());
			}

			if (m_PickerFocus == PickerFocus::GroupList) {
				if (pressDown || pressUp) {
					int listSize = m_GroupsList->GetItemList()->size();
					if (pressDown) {
						m_SelectedGroupIndex++;
						if (m_SelectedGroupIndex >= listSize) { m_SelectedGroupIndex = 0; }
					} else if (pressUp) {
						m_SelectedGroupIndex--;
						if (m_SelectedGroupIndex < 0) { m_SelectedGroupIndex = listSize - 1; }
					}
					m_GroupsList->SetSelectedIndex(m_SelectedGroupIndex);
					if (m_GroupsList->GetSelected()) {
						UpdateObjectsList();
						g_GUISound.ItemChangeSound()->Play(m_Controller->GetPlayer());
					}
				} else if (m_Controller->IsState(ControlState::PRESS_FACEBUTTON) && m_GroupsList->GetItem(m_SelectedGroupIndex)) {
					UpdateObjectsList();
					SetListFocus(PickerFocus::ObjectList);
				}
			} else if (m_PickerFocus == PickerFocus::ObjectList) {
				if (pressDown || pressUp) {
					int listSize = m_ObjectsList->GetItemList()->size();
					if (pressDown) {
						m_SelectedObjectIndex++;
						if (m_SelectedObjectIndex >= listSize) { m_SelectedObjectIndex = 0; }
					} else if (pressUp) {
						m_SelectedObjectIndex--;
						if (m_SelectedObjectIndex < 0) { m_SelectedObjectIndex = listSize - 1; }
					}
					m_ObjectsList->SetSelectedIndex(m_SelectedObjectIndex);
					// Report the newly selected item as being 'picked', but don't close the picker
					const GUIListPanel::Item *objectListItem = m_ObjectsList->GetSelected();
					if (objectListItem) { m_PickedObject = dynamic_cast<const SceneObject *>(objectListItem->m_pEntity); }
					g_GUISound.SelectionChangeSound()->Play(m_Controller->GetPlayer());
				} else if (m_Controller->IsState(ControlState::PRESS_FACEBUTTON)) {
					if (const GUIListPanel::Item *objectListItem = m_ObjectsList->GetSelected()) {
						// User pressed on a module group item. Toggle its expansion and repopulate the item list with the new module expansion configuration
						if (objectListItem->m_ExtraIndex >= 0) {
							m_ExpandedModules.at(objectListItem->m_ExtraIndex) = !m_ExpandedModules.at(objectListItem->m_ExtraIndex);
							UpdateObjectsList(false);
							g_GUISound.ItemChangeSound()->Play(m_Controller->GetPlayer());
						} else {
							objectPickedOrPickerClosed = true;
						}
					}
				}
			}
		}
		if (objectPickedOrPickerClosed || m_Controller->IsState(ControlState::PRESS_SECONDARY)) {
			if (const GUIListPanel::Item *selectedItem = m_ObjectsList->GetSelected()) {
				m_PickedObject = dynamic_cast<const SceneObject *>(selectedItem->m_pEntity);
				if (m_PickedObject) {
					g_GUISound.ObjectPickedSound()->Play(m_Controller->GetPlayer());
					SetEnabled(false);
					return true;
				}
			}
		}
		return false;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool ObjectPickerGUI::HandleMouseEvents() {
		int mouseX;
		int mouseY;
		m_GUIInput->GetMousePosition(&mouseX, &mouseY);
		m_CursorPos.SetXY(static_cast<float>(mouseX), static_cast<float>(mouseY));

		GUIEvent guiEvent;
		while (m_GUIControlManager->GetEvent(&guiEvent)) {
			if (guiEvent.GetType() == GUIEvent::Notification) {
				if (guiEvent.GetControl() == m_GroupsList) {
					if (guiEvent.GetMsg() == GUIListBox::MouseDown) {
						if (m_GroupsList->GetSelected()) {
							m_SelectedGroupIndex = m_GroupsList->GetSelectedIndex();
							UpdateObjectsList();
							g_GUISound.ItemChangeSound()->Play(m_Controller->GetPlayer());
						} else {
							// Undo the click deselection if nothing was selected
							m_GroupsList->SetSelectedIndex(m_SelectedGroupIndex);
						}
					} else if (guiEvent.GetMsg() == GUIListBox::MouseEnter) {
						SetListFocus(PickerFocus::GroupList);
					}
				} else if (guiEvent.GetControl() == m_ObjectsList) {
					if (guiEvent.GetMsg() == GUIListBox::MouseDown) {
						m_ObjectsList->ScrollToSelected();
						if (const GUIListPanel::Item *objectListItem = m_ObjectsList->GetSelected()) {
							// User pressed on a module group item. Toggle its expansion and repopulate the item list with the new module expansion configuration
							if (objectListItem->m_ExtraIndex >= 0) {
								m_ExpandedModules.at(objectListItem->m_ExtraIndex) = !m_ExpandedModules.at(objectListItem->m_ExtraIndex);
								UpdateObjectsList(false);
								g_GUISound.ItemChangeSound()->Play(m_Controller->GetPlayer());
							} else if (objectListItem->m_pEntity) {
								m_SelectedObjectIndex = m_ObjectsList->GetSelectedIndex();
								return true;
							}
						}
					} else if (guiEvent.GetMsg() == GUIListBox::MouseMove) {
						const GUIListPanel::Item *objectListItem = m_ObjectsList->GetItem(m_CursorPos.GetFloorIntX(), m_CursorPos.GetFloorIntY());
						if (objectListItem && m_SelectedObjectIndex != objectListItem->m_ID) {
							m_SelectedObjectIndex = objectListItem->m_ID;
							m_ObjectsList->SetSelectedIndex(m_SelectedObjectIndex);
							g_GUISound.SelectionChangeSound()->Play(m_Controller->GetPlayer());
						}
					} else if (guiEvent.GetMsg() == GUIListBox::MouseEnter) {
						SetListFocus(PickerFocus::ObjectList);
					}
				} else {
					// If clicked outside the picker, then close the picker GUI
					if (guiEvent.GetMsg() == GUIListBox::Click && m_CursorPos.GetFloorIntX() < m_ParentBox->GetXPos()) {
						return true;
					}
				}
			}
		}
		return false;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::AnimateOpenClose() {
		if (m_PickerState == PickerState::Enabling) {
			m_ParentBox->SetVisible(true);

			int enabledPos = g_FrameMan.GetPlayerFrameBufferWidth(m_Controller->GetPlayer()) - m_ParentBox->GetWidth();
			float travelCompletionDistance = std::floor(static_cast<float>(enabledPos - m_ParentBox->GetXPos()) * m_OpenCloseSpeed);

			m_ParentBox->SetPositionAbs(m_ParentBox->GetXPos() + static_cast<int>(travelCompletionDistance), 0);
			g_SceneMan.SetScreenOcclusion(Vector(static_cast<float>(m_ParentBox->GetXPos() - g_FrameMan.GetPlayerFrameBufferWidth(m_Controller->GetPlayer())), 0), g_ActivityMan.GetActivity()->ScreenOfPlayer(m_Controller->GetPlayer()));

			if (m_ParentBox->GetXPos() <= enabledPos) {
				m_ParentBox->SetEnabled(true);
				m_PickerState = PickerState::Enabled;
			}
		} else if (m_PickerState == PickerState::Disabling) {
			m_ParentBox->SetEnabled(false);
			m_PopupBox->SetVisible(false);

			int disabledPos = g_FrameMan.GetPlayerFrameBufferWidth(m_Controller->GetPlayer());
			float travelCompletionDistance = std::ceil(static_cast<float>(disabledPos - m_ParentBox->GetXPos()) * m_OpenCloseSpeed);

			m_ParentBox->SetPositionAbs(m_ParentBox->GetXPos() + static_cast<int>(travelCompletionDistance), 0);
			g_SceneMan.SetScreenOcclusion(Vector(static_cast<float>(m_ParentBox->GetXPos() - g_FrameMan.GetPlayerFrameBufferWidth(m_Controller->GetPlayer())), 0), g_ActivityMan.GetActivity()->ScreenOfPlayer(m_Controller->GetPlayer()));

			if (m_ParentBox->GetXPos() >= g_FrameMan.GetPlayerFrameBufferWidth(m_Controller->GetPlayer())) {
				m_ParentBox->SetVisible(false);
				m_PickerState = PickerState::Disabled;
			}
		}
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::Draw(BITMAP *drawBitmap) const {
		m_GUIControlManager->Draw(&AllegroScreen(drawBitmap));

		// Draw the cursor on top of everything
		if (IsEnabled() && m_Controller->IsMouseControlled()) { draw_sprite(drawBitmap, s_Cursor, m_CursorPos.GetFloorIntX(), m_CursorPos.GetFloorIntY()); }
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::GUIScreenDeleter::operator()(GUIScreen *ptr) const { ptr->Destroy(); }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::GUIInputDeleter::operator()(GUIInput *ptr) const { ptr->Destroy(); }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void ObjectPickerGUI::GUIControlManagerDeleter::operator()(GUIControlManager *ptr) const { ptr->Destroy(); }
}
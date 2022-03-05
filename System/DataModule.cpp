#include "DataModule.h"
#include "PresetMan.h"
#include "SceneMan.h"
#include "LuaMan.h"

namespace RTE {

	const std::string DataModule::c_ClassName = "DataModule";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void DataModule::Clear() {
		m_FileName.clear();
		m_FriendlyName.clear();
		m_Author.clear();
		m_Description.clear();
		m_Version = 1;
		m_ModuleID = -1;
		m_IconFile.Reset();
		m_Icon = nullptr;
		m_PresetList.clear();
		m_EntityList.clear();
		m_TypeMap.clear();
		m_MaterialMappings.fill(0);
		m_ScanFolderContents = false;
		m_IgnoreMissingItems = false;
		m_CrabToHumanSpawnRatio = 0;
		m_ScriptPath.clear();
		m_IsFaction = false;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int DataModule::Create(const std::string &moduleName, const ProgressCallback &progressCallback) {
		m_FileName = std::filesystem::path(moduleName).generic_string();
		m_ModuleID = g_PresetMan.GetModuleID(moduleName);
		m_CrabToHumanSpawnRatio = 0;

		// Report that we're starting to read a new DataModule
		if (progressCallback) { progressCallback(m_FileName + " " + static_cast<char>(-43) + " loading:", true); }

		Reader reader;
		std::string indexPath(m_FileName + "/Index.ini");
		std::string mergedIndexPath(m_FileName + "/MergedIndex.ini");

		// NOTE: This looks for the MergedIndex.ini generated by the index merger tool. The tool is mostly superseded by disabling loading visuals, but still provides some benefit.
		if (std::filesystem::exists(mergedIndexPath)) { indexPath = mergedIndexPath; }

		if (reader.Create(indexPath, true, progressCallback) >= 0) {
			int result = Serializable::Create(reader);

			// Print an empty line to separate the end of a module from the beginning of the next one in the loading progress log.
			if (progressCallback) { progressCallback(" ", true); }

			if (m_ScanFolderContents) { result = FindAndRead(progressCallback); }

			return result;
		}
		return -1;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void DataModule::Destroy() {
		for (const PresetEntry &preset : m_PresetList){
			delete preset.m_EntityPreset;
		}
		Clear();
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int DataModule::ReadModuleProperties(const std::string &moduleName, const ProgressCallback &progressCallback) {
		m_FileName = moduleName;
		m_ModuleID = g_PresetMan.GetModuleID(moduleName);
		m_CrabToHumanSpawnRatio = 0;

		// Report that we're starting to read a new DataModule
		if (progressCallback) { progressCallback(m_FileName + " " + static_cast<char>(-43) + " reading properties:", true); }
		Reader reader;
		std::string indexPath(m_FileName + "/Index.ini");

		if (reader.Create(indexPath, true, progressCallback) >= 0) {
			reader.SetSkipIncludes(true);
			int result = Serializable::Create(reader);
			return result;
		}
		return -1;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int DataModule::ReadProperty(const std::string_view &propName, Reader &reader) {
		if (propName == "ModuleName") {
			reader >> m_FriendlyName;
		} else if (propName == "Author") {
			reader >> m_Author;
		} else if (propName == "Description") {
			std::string descriptionValue = reader.ReadPropValue();
			if (descriptionValue == "MultiLineText") {
				m_Description.clear();
				while (reader.NextProperty() && reader.ReadPropName() == "AddLine") {
					m_Description += reader.ReadPropValue() + "\n\n";
				}
				if (!m_Description.empty()) {
					m_Description.resize(m_Description.size() - 2);
				}
			} else {
				m_Description = descriptionValue;
			}
		} else if (propName == "IsFaction") {
			reader >> m_IsFaction;
		} else if (propName == "Version") {
			reader >> m_Version;
		} else if (propName == "ScanFolderContents") {
			reader >> m_ScanFolderContents;
		} else if (propName == "IgnoreMissingItems") {
			reader >> m_IgnoreMissingItems;
		} else if (propName == "CrabToHumanSpawnRatio") {
			reader >> m_CrabToHumanSpawnRatio;
		} else if (propName == "ScriptPath") {
			reader >> m_ScriptPath;
			LoadScripts();
		} else if (propName == "Require") {
			// Check for required dependencies if we're not load properties
			std::string requiredModule;
			reader >> requiredModule;
			if (!reader.GetSkipIncludes() && g_PresetMan.GetModuleID(requiredModule) == -1) {
				reader.ReportError("\"" + m_FileName + "\" requires \"" + requiredModule + "\" in order to load!\n");
			}
		} else if (propName == "IconFile") {
			reader >> m_IconFile;
			m_Icon = m_IconFile.GetAsBitmap();
		} else if (propName == "AddMaterial") {
			return g_SceneMan.ReadProperty(propName, reader);
		} else {
			// Try to read in the preset and add it to the PresetMan in one go, and report if fail
			if (!g_PresetMan.GetEntityPreset(reader)) { reader.ReportError("Could not understand Preset type!"); }
		}
		return 0;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int DataModule::Save(Writer &writer) const {
		Serializable::Save(writer);

		writer.NewPropertyWithValue("ModuleName", m_FriendlyName);
		writer.NewPropertyWithValue("Author", m_Author);
		writer.NewPropertyWithValue("Description", m_Description);
		writer.NewPropertyWithValue("IsFaction", m_IsFaction);
		writer.NewPropertyWithValue("Version", m_Version);
		writer.NewPropertyWithValue("IconFile", m_IconFile);

		// TODO: Write out all the different entity instances, each having their own relative location within the data module stored
		// Will need the writer to be able to open different files and append to them as needed, probably done in NewEntity()
		// writer.NewEntity()

		return 0;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::string DataModule::GetEntityDataLocation(const std::string &exactType, const std::string &instance) {
		const Entity *foundEntity = GetEntityPreset(exactType, instance);
		if (foundEntity == nullptr) {
			return nullptr;
		}

		// Search for entity in instanceList
		for (const PresetEntry &presetListEntry : m_PresetList) {
			if (presetListEntry.m_EntityPreset == foundEntity) {
				return presetListEntry.m_FileReadFrom;
			}
		}

		RTEAbort("Tried to find allegedly existing Entity Preset Entry: " + foundEntity->GetPresetName() + ", but couldn't!");
		return nullptr;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const Entity * DataModule::GetEntityPreset(const std::string &exactType, const std::string &instance) {
		if (exactType.empty() || instance == "None" || instance.empty()) {
			return nullptr;
		}

		std::map<std::string, std::list<std::pair<std::string, Entity *>>>::iterator classItr = m_TypeMap.find(exactType);
		if (classItr != m_TypeMap.end()) {
			// Find an instance of that EXACT type and name; derived types are not matched
			for (const std::pair<std::string, Entity *> &classItrEntry : classItr->second) {
				if (classItrEntry.first == instance && classItrEntry.second->GetClassName() == exactType) {
					return classItrEntry.second;
				}
			}
		}
		return nullptr;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool DataModule::AddEntityPreset(Entity *entityToAdd, bool overwriteSame, const std::string &readFromFile) {
		// Fail if the entity is unnamed or it's not the original preset.
		//TODO If we're overwriting, we may not want to fail if it's not the original preset, this needs to be investigated
		if (entityToAdd->GetPresetName() == "None" || entityToAdd->GetPresetName().empty() || !entityToAdd->IsOriginalPreset()) {
			return false;
		}
		bool entityAdded = false;
		Entity *existingEntity = GetEntityIfExactType(entityToAdd->GetClassName(), entityToAdd->GetPresetName());

		if (existingEntity) {
			// If we're commanded to overwrite any collisions, then do so by cloning over the existing instance in the list
			// This way we're not invalidating any instance references that would have been taken out and held by clients
			if (overwriteSame) {
				entityToAdd->SetModuleID(m_ModuleID); //TODO this is probably overwritten by Entity::Create(other), making it useless. Double-check this and remove this line if certain
				entityToAdd->Clone(existingEntity);
				// Make sure the existing one is still marked as the Original Preset
				existingEntity->m_IsOriginalPreset = true;
				// Alter the instance entry to reflect the data file location of the new definition
				if (readFromFile != "Same") {
					std::list<PresetEntry>::iterator itr = m_PresetList.begin();
					for (; itr != m_PresetList.end(); ++itr) {
						// When we find the correct entry, alter its data file location
						if ((*itr).m_EntityPreset == existingEntity) {
							(*itr).m_FileReadFrom = readFromFile;
							break;
						}
					}
					RTEAssert(itr != m_PresetList.end(), "Tried to alter allegedly existing Entity Preset Entry: " + entityToAdd->GetPresetName() + ", but couldn't find it in the list!");
				}
				return true;
			} else {
				return false;
			}
		} else {
			entityToAdd->SetModuleID(m_ModuleID);
			Entity *entityClone = entityToAdd->Clone();
			// Mark the one we are about to add to the list as the Original now - this is now the actual Original Preset instance
			entityClone->m_IsOriginalPreset = true;

			if (readFromFile == "Same" && m_PresetList.empty()) {
				RTEAbort("Tried to add first entity instance to data module " + m_FileName + " without specifying a data file!");
			}

			m_PresetList.push_back(PresetEntry(entityClone, readFromFile != "Same" ? readFromFile : m_PresetList.back().m_FileReadFrom));
			m_EntityList.push_back(entityClone);
			entityAdded = AddToTypeMap(entityClone);
			RTEAssert(entityAdded, "Unexpected problem while adding Entity instance \"" + entityToAdd->GetPresetName() + "\" to the type map of data module: " + m_FileName);
		}
		return entityAdded;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool DataModule::GetGroupsWithType(std::list<std::string> &groupList, const std::string &withType) {
		bool foundAny = false;

		if (withType == "All" || withType.empty()) {
			for (const std::string &groupRegisterEntry : m_GroupRegister) {
				groupList.push_back(groupRegisterEntry);
				// TODO: it seems weird that foundAny isn't set to true here, given that the list gets filled.
				// But I suppose no actual finding is done. Investigate this and see where it's called, maybe this should be changed
			}
		} else {
			std::map<std::string, std::list<std::pair<std::string, Entity *>>>::iterator classItr = m_TypeMap.find(withType);
			if (classItr != m_TypeMap.end()) {
				const std::list<std::string> *groupListPtr = nullptr;
				// Go through all the entities of that type, adding the groups they belong to
				for (const std::pair<std::string, Entity *> &instance : classItr->second) {
					groupListPtr = instance.second->GetGroupList();

					for (const std::string &groupListEntry : *groupListPtr) {
						groupList.push_back(groupListEntry); // Get the grouped entities, without transferring ownership
						foundAny = true;
					}
				}

				// Make sure there are no dupe groups in the list
				groupList.sort();
				groupList.unique();
			}
		}
		return foundAny;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool DataModule::GetAllOfGroup(std::list<Entity *> &entityList, const std::string &group, const std::string &type) {
		if (group.empty()) {
			return false;
		}

		bool foundAny = false;

		// Find either the Entity typelist that contains all entities in this DataModule, or the specific class' typelist (which will get all derived classes too)
		std::map<std::string, std::list<std::pair<std::string, Entity *>>>::iterator classItr = m_TypeMap.find((type.empty() || type == "All") ? "Entity" : type);

		if (classItr != m_TypeMap.end()) {
			RTEAssert(!classItr->second.empty(), "DataModule has class entry without instances in its map!?");

			for (const std::pair<std::string, Entity *> &instance : classItr->second) {
				if (instance.second->IsInGroup(group)) {
					entityList.push_back(instance.second); // Get the grouped entities, without transferring ownership
					foundAny = true;
				}
			}
		}
		return foundAny;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool DataModule::GetAllOfType(std::list<Entity *> &entityList, const std::string &type) {
		if (type.empty()) {
			return false;
		}

		std::map<std::string, std::list<std::pair<std::string, Entity *>>>::iterator classItr = m_TypeMap.find(type);
		if (classItr != m_TypeMap.end()) {
			RTEAssert(!classItr->second.empty(), "DataModule has class entry without instances in its map!?");

			for (const std::pair<std::string, Entity *> &instance : classItr->second) {
				entityList.push_back(instance.second); // Get the entities, without transferring ownership
			}
			return true;
		}
		return false;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool DataModule::AddMaterialMapping(unsigned char fromID, unsigned char toID) {
		RTEAssert(fromID > 0 && fromID < c_PaletteEntriesNumber && toID > 0 && toID < c_PaletteEntriesNumber, "Tried to make an out-of-bounds Material mapping");

		bool clear = m_MaterialMappings.at(fromID) == 0;
		m_MaterialMappings.at(fromID) = toID;

		return clear;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int DataModule::LoadScripts() const {
		return m_ScriptPath.empty() ? 0 : g_LuaMan.RunScriptFile(m_ScriptPath);
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void DataModule::ReloadAllScripts() const {
		for (const PresetEntry &presetListEntry : m_PresetList) {
			presetListEntry.m_EntityPreset->ReloadScripts();
		}
		LoadScripts();
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int DataModule::FindAndRead(const ProgressCallback &progressCallback) {
		int result = 0;
		for (const std::filesystem::directory_entry &directoryEntry : std::filesystem::directory_iterator(System::GetWorkingDirectory() + m_FileName)) {
			if (directoryEntry.path().extension() == ".ini" && directoryEntry.path().filename() != "Index.ini") {
				Reader iniReader;
				if (iniReader.Create(directoryEntry.path().generic_string(), false, progressCallback) >= 0) {
					result = Serializable::Create(iniReader, false, true);
					if (progressCallback) { progressCallback(" ", true); }
				}
			}
		}
		return result;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// TODO: This method is almost identical to GetEntityPreset, except it doesn't return a const Entity *.
	// Investigate if the latter needs to return const (based on what's using it) and if not, get rid of this and replace its uses. At the very least, consider renaming this
	// See https://github.com/Filipawn-Industries/Cortex-Command-Community-Continuation-Source/issues/87
	Entity * DataModule::GetEntityIfExactType(const std::string &exactType, const std::string &presetName) {
		if (exactType.empty() || presetName == "None" || presetName.empty()) {
			return nullptr;
		}

		std::map<std::string, std::list<std::pair<std::string, Entity *>>>::iterator classItr = m_TypeMap.find(exactType);
		if (classItr != m_TypeMap.end()) {
			// Find an instance of that EXACT type and name; derived types are not matched
			for (const std::pair<std::string, Entity *> &instance : classItr->second) {
				if (instance.first == presetName && instance.second->GetClassName() == exactType) {
					return instance.second;
				}
			}
		}
		return nullptr;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool DataModule::AddToTypeMap(Entity *entityToAdd) {
		if (!entityToAdd || entityToAdd->GetPresetName() == "None" || entityToAdd->GetPresetName().empty()) {
			return false;
		}

		// Walk up the class hierarchy till we reach the top, adding an entry of the passed in entity into each typelist as we go along
		for (const Entity::ClassInfo *pClass = &(entityToAdd->GetClass()); pClass != nullptr; pClass = pClass->GetParent()) {
			std::map<std::string, std::list<std::pair<std::string, Entity *>>>::iterator classItr = m_TypeMap.find(pClass->GetName());

			// No instances of this entity have been added yet so add a class category for it
			if (classItr == m_TypeMap.end()) {
				classItr = (m_TypeMap.insert(std::pair<std::string, std::list<std::pair<std::string, Entity *>>>(pClass->GetName(), std::list<std::pair<std::string, Entity *>>()))).first;
			}

			// NOTE We're adding the entity to the class category list but not transferring ownership. Also, we're not checking for collisions as they're assumed to have been checked for already
			(*classItr).second.push_back(std::pair<std::string, Entity *>(entityToAdd->GetPresetName(), entityToAdd));
		}
		return true;
	}
}

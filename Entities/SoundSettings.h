#ifndef _RTESOUNDSETTINGS_
#define _RTESOUNDSETTINGS_

#include "Entity.h"
#include "SoundSet.h"
#include "AudioMan.h"

namespace RTE {
	/**
	 * @brief Provides a container for various settings pertaining to audio
	 * playback. Used by SoundSets.
	 * 
	 */
	class SoundSettings
	{
	public:
		/**
		 * @brief Set the Priority for this SoundSetting instance
		 * 
		 * @param newPriority 
		 */
		void SetPriority(const int newPriority) { m_Priority = std::clamp(newPriority, 0, 256); }

		/**
		 * @brief Get the Priority for this SoundSetting instance
		 * 
		 * @return int 
		 */
		int GetPriority() const { return m_Priority; }

		/**
		 * @brief Set the Affected By Global Pitch state for this SoundSetting instance
		 * 
		 * @param state 
		 */
		void SetAffectedByGlobalPitch(const bool state) { m_AffectedByGlobalPitch = state; }

		/**
		 * @brief Get whether this SoundSetting instance is affected by the Affected By Global Pitch state
		 * 
		 * @return true 
		 * @return false 
		 */
		bool IsAffectedByGlobalPitch() const { return m_AffectedByGlobalPitch; }

		/**
		 * @brief Set the Pitch for this SoundSetting instance
		 * 
		 * @param newPitch 
		 */
		void SetPitch(const float newPitch) { m_Pitch = m_Pitch = std::clamp(newPitch, 0.125F, 8.0F); }

		/**
		 * @brief Get the Pitch for this SoundSetting instance
		 * 
		 * @return float 
		 */
		float GetPitch() const { return m_Pitch; }

		/**
		 * @brief Set the Pitch Variation for this SoundSetting instance
		 * 
		 * @param newPitchVariation 
		 */
		void SetPitchVariation(const float newPitchVariation) { m_PitchVariation = newPitchVariation; }

		/**
		 * @brief Get the Pitch Variation for this SoundSetting instance
		 * 
		 * @return float 
		 */
		float GetPitchVariation() const { return m_PitchVariation; }

		/**
		 * @brief Set the Volume for this SoundSetting instance
		 * 
		 * @param newVolume 
		 */
		void SetVolume(const float newVolume) { m_Volume = std::clamp(newVolume, 0.0F, 10.0F); }

		/**
		 * @brief Get the Volume for this SoundSetting instance
		 * 
		 * @return float 
		 */
		float GetVolume() const { return m_Volume; }

	private:
		/* Mixing priority of this SoundSetting. Higher values are more likely to be heard */
		int m_Priority;
		/* Is this SoundSetting affected by global pitch? */
		bool m_AffectedByGlobalPitch;
		/* Natural pitch of this SoundSetting instance */
		float m_Pitch;
		/*
			Randomized pitch variation of this SoundSetting instance. `1`
			means the sound will vary a full octavve both ways.
		*/
		float m_PitchVariation;
		/* The natural volume of this SoundSetting instance */
		float m_Volume;
	};
}

#endif


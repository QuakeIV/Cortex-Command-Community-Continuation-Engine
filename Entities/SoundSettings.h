#ifndef _RTESOUNDSETTINGS_
#define _RTESOUNDSETTINGS_

#include "Entity.h"
#include "SoundSet.h"
#include "AudioMan.h"

namespace RTE {
	class SoundSettings
	{
	public:
		void SetPriority(const int pri) { m_Priority = std::clamp(pri, 0, 256); }
		int GetPriority() const { return m_Priority; }

		void SetAffectedByGlobalPitch(const bool state) { m_AffectedByGlobalPitch = state; }
		bool IsAffectedByGlobalPitch() const { return m_AffectedByGlobalPitch; }

		void SetPitch(const float pitch) { m_Pitch = m_Pitch = std::clamp(pitch, 0.125F, 8.0F); }
		float GetPitch() const { return m_Pitch; }

		void SetPitchVariation(const float pitchvar) { m_PitchVariation = pitchvar; }
		float GetPitchVariation() const { return m_PitchVariation; }

		void SetVolume(const float vol) { m_Volume = std::clamp(vol, 0.0F, 10.0F); }
		float GetVolume() const { return m_Volume; }

	private:
		int m_Priority; //!< The mixing priority of this SoundData's sounds. Higher values are more likely to be heard.
		bool m_AffectedByGlobalPitch; //!< Whether this SoundData's sounds should be able to be altered by global pitch changes.

		float m_Pitch; //!< The current natural pitch of this SoundContainer's or SoundSet's sounds.
		float m_PitchVariation; //!< The randomized pitch variation of this SoundContainer's or SoundSet's sounds. 1 means the sound will vary a full octave both ways.
		float m_Volume; //!< The current natural volume of this SoundContainer's or SoundSet's sounds.
//		float MinimumAudibleDistance; Handled by sound data for rev compat
//		Vector Offset = Vector(); // Handled by sound data for rev compat
	};
}

#endif


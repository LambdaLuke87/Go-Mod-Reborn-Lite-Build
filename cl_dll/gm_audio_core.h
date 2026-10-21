#pragma once

// Core of the Go-Mod client audio engine. Plain C++: no SDK, no SDL.
// (so it can be tested alone, and the SDL/engine glue stays tiny)

#include <cstddef>
#include <memory>

#include "gm_audio_fx.h"
#include <string>
#include <utility>
#include <vector>

struct GMSoundData
{
	std::vector<float> samples; // mono, -1..1
	int rate = 22050;
};

// Decodes a .wav that is already in memory (PCM 8/16/24/32 bit or float32, any number of
// channels: they are mixed down to mono, because the position is what makes it stereo).
// Returns false and fills 'error' if the file is unsupported or corrupt.
bool GMAudio_DecodeWav(const unsigned char* data, size_t size, GMSoundData& out, std::string& error);

struct GMVec3
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

// Volume of the left/right ear for a source. Same model the engine uses (Quake/GoldSrc):
//   attenuation <= 0  -> heard everywhere at full volume, in the center (ATTN_NONE)
//   otherwise volume = 1 - distance * attenuation / 1000, and left/right comes from the listener's right vector
void GMAudio_Spatialize(const GMVec3& listener, const GMVec3& listenerRight, const GMVec3& source,
	float attenuation, float& left, float& right);

struct GMVoiceParams
{
	int entity = 0;
	int channel = 0;
	float volume = 1.0f;	  // 0..1
	float attenuation = 0.8f; // ATTN_NORM
	float pitch = 1.0f;		  // 1.0 = normal speed
	bool global = false;	  // full volume in the center (sounds of the listener's own entity)
	GMVec3 origin;
};

// One piece of a voice. A normal sound has one segment; a sentence (sentences.txt) has one per word.
struct GMSegment
{
	std::shared_ptr<const GMSoundData> sound;
	float volume = 1.0f; // multiplies the voice volume (sentence "v" modifier, 100 = 1.0)
	float pitch = 1.0f;	 // multiplies the voice pitch (sentence "p" modifier)
	float start = 0.0f;	 // 0..1, where to start inside the sound (sentence "s" modifier)
	float end = 1.0f;	 // 0..1, where to stop (sentence "e" modifier)
};

// Optional effects (all off = the plain mixer). Set by the game thread every frame.
struct GMEffects
{
	bool reverb = false;	   // room reverb
	int roomType = 0;		   // engine room_type 0-28 (env_sound), already resolved by the caller
	float reverbMix = 1.0f;	   // multiplies the room's reverb level
	bool spatial = false;	   // binaural approximation: time difference + head shadow + behind
	bool airAbsorption = false; // far sounds lose high frequencies
	bool underwater = false;   // the listener is underwater: muffle everything
	int interpolation = 0;	   // 0 linear, 1 cubic (better quality when the pitch changes)

	bool vibrato = false;	   // the pitch goes up and down by itself (YTP style)
	float vibratoRate = 6.0f;  // oscillations per second
	float vibratoDepth = 0.3f; // 0.3 = +-30% pitch
};

// what the game thread needs to compute occlusion / doppler for a voice
struct GMVoiceSource
{
	unsigned long long serial = 0;
	int entity = 0;
	GMVec3 origin;
	float attenuation = 0.0f;
	bool global = false;
};

// Mixes many voices into interleaved stereo float. It has NO locking: whoever owns the audio
// device must not call anything here while Mix() is running (the SDL glue locks the device).
class GMMixer
{
public:
	static const int kMaxVoices = 64;

	void SetOutputRate(int rate);

	// Same rule as the engine: a new sound replaces the previous one of the same entity + channel,
	// except CHAN_AUTO (0) and CHAN_STATIC (6), which never replace anything.
	void Play(std::shared_ptr<const GMSoundData> sound, const GMVoiceParams& params);
	void Play(const std::vector<GMSegment>& segments, const GMVoiceParams& params); // played one after the other
	void Stop(int entity, int channel);
	void StopAll();

	void SetListener(const GMVec3& origin, const GMVec3& right, float masterVolume);
	void SetListenerForward(const GMVec3& forward); // needed by the "spatial" effect (front / behind)
	void SetEffects(const GMEffects& fx);

	// occlusion (0 = clear .. 1 = fully blocked) and doppler (pitch multiplier) of one voice.
	// The voice moves smoothly towards these values.
	void GetSources(std::vector<GMVoiceSource>& out) const;
	void SetVoiceFx(unsigned long long serial, float occlusion, float doppler);
	void SetEntityOrigin(int entity, const GMVec3& origin);
	void GetEntities(std::vector<int>& out) const; // entities (> 0) that have a sound playing

	// loudness (0..1) of what each entity says on CHAN_VOICE right now, to move the mouth
	void GetVoiceLevels(std::vector<std::pair<int, float>>& out) const;

	void Mix(float* out, int frames); // overwrites 'out' (frames * 2 floats)

	int ActiveVoices() const { return (int)m_Voices.size(); }
	int OutputRate() const { return m_OutputRate; }

private:
	struct Voice
	{
		std::vector<GMSegment> segments;
		size_t segment = 0; // the one playing now
		GMVoiceParams params;
		double pos = 0.0;	// sample position inside the current segment's sound
		float level = 0.0f; // smoothed loudness, for the mouth
		float curL = 0.0f, curR = 0.0f;
		bool fresh = true;
		unsigned long long serial = 0;

		// effects
		float occlusion = 0.0f, occlusionCur = 0.0f;
		float doppler = 1.0f, dopplerCur = 1.0f;
		GMLowpass lowpass;			  // occlusion + air absorption
		GMLowpass shadowL, shadowR;	  // head shadow / behind (spatial)
		float delay[64] = {};		  // interaural delay line (spatial)
		int delayPos = 0;
		float itdL = 0.0f, itdR = 0.0f; // current delay of each ear, in samples
		double vibratoPhase = 0.0;		 // 0..1
	};

	void MixVoice(Voice& v, float* out, float* send, int frames);

	std::vector<Voice> m_Voices;
	GMVec3 m_Listener, m_ListenerRight, m_ListenerForward;
	GMEffects m_Fx;
	GMReverb m_Reverb;
	std::vector<float> m_Send; // mono reverb send, one block
	GMLowpass m_WaterL, m_WaterR;
	float m_WaterMix = 0.0f; // 0..1 fade of the underwater filter
	float m_Master = 1.0f;
	int m_OutputRate = 44100;
	unsigned long long m_NextSerial = 1;
};

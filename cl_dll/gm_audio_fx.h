#pragma once

// Go-Mod audio engine: effects (pure C++, no SDK, no SDL, so it can be tested alone).
//
//  - reverb: Schroeder/Moorer reverb with the public domain "Freeverb" tuning (Jezar at Dreampoint),
//    with presets that follow the engine's room types (env_sound / room_type 0-28)
//  - one-pole lowpass filters (occlusion, air absorption, head shadow, underwater)
//
// Everything written from scratch for Go-Mod: no code from other projects.

#include <vector>

// one-pole lowpass: y += a * (x - y)
struct GMLowpass
{
	float y = 0.0f;

	void Reset() { y = 0.0f; }
	float Process(float x, float a)
	{
		y += a * (x - y);
		return y;
	}
};

// coefficient for GMLowpass. Returns 1 (no filtering) when the cutoff is above ~18 kHz
float GMAudio_LowpassCoef(float cutoffHz, float sampleRate);

struct GMRoomPreset
{
	const char* name;
	float room;		// 0..1, size of the room (how long the tail is)
	float damp;		// 0..1, how fast the high frequencies die
	float wet;		// 0..1, how loud the reverb is
	float lowpass;	// Hz applied to the dry sound too (0 = none). Used by the water rooms
};

// room_type of the engine (0 = none ... 28). Out of range = 0
const GMRoomPreset& GMAudio_RoomPreset(int roomType);
int GMAudio_RoomCount();

class GMReverb
{
public:
	GMReverb();

	void SetRate(int rate);
	void SetTarget(float room, float damp, float wet); // the current values move towards these (no clicks)
	void Clear();

	// in: mono send (frames). out: interleaved stereo, the reverb is ADDED to it
	void Process(const float* in, float* out, int frames);

	// still has something to say (target on, or the tail is still fading)
	bool Active() const;

private:
	struct Comb
	{
		std::vector<float> buf;
		size_t idx = 0;
		float store = 0.0f;
	};

	struct Allpass
	{
		std::vector<float> buf;
		size_t idx = 0;
	};

	static const int kCombs = 8;
	static const int kAllpasses = 4;

	Comb m_CombL[kCombs], m_CombR[kCombs];
	Allpass m_AllL[kAllpasses], m_AllR[kAllpasses];

	int m_Rate = 44100;
	float m_Room = 0.5f, m_Damp = 0.5f, m_Wet = 0.0f;			  // current
	float m_TargetRoom = 0.5f, m_TargetDamp = 0.5f, m_TargetWet = 0.0f; // wanted
	bool m_Idle = true;
};

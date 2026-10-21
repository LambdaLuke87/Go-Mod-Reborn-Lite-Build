#include "gm_audio_fx.h"

#include <algorithm>
#include <cmath>

float GMAudio_LowpassCoef(float cutoffHz, float sampleRate)
{
	if (sampleRate <= 0.0f || cutoffHz >= 18000.0f || cutoffHz >= sampleRate * 0.45f)
		return 1.0f;

	cutoffHz = std::max(cutoffHz, 20.0f);
	return 1.0f - std::exp(-2.0f * 3.14159265f * cutoffHz / sampleRate);
}

// ---------------------------------------------------------------------------------------
//  Rooms. Same numbers as the engine's room_type (what env_sound sets), with values chosen
//  to sound close to it: small/medium/large = longer tails, "bright" = little damping, etc.
// ---------------------------------------------------------------------------------------

static const GMRoomPreset kRooms[] = {
	{"off", 0.00f, 0.50f, 0.00f, 0.0f},
	{"generic", 0.50f, 0.50f, 0.15f, 0.0f},
	{"metal small", 0.55f, 0.15f, 0.20f, 0.0f},
	{"metal medium", 0.70f, 0.15f, 0.25f, 0.0f},
	{"metal large", 0.85f, 0.15f, 0.30f, 0.0f},
	{"tunnel small", 0.60f, 0.30f, 0.25f, 0.0f},
	{"tunnel medium", 0.75f, 0.30f, 0.30f, 0.0f},
	{"tunnel large", 0.88f, 0.30f, 0.35f, 0.0f},
	{"chamber small", 0.50f, 0.40f, 0.20f, 0.0f},
	{"chamber medium", 0.65f, 0.40f, 0.25f, 0.0f},
	{"chamber large", 0.80f, 0.40f, 0.30f, 0.0f},
	{"bright small", 0.50f, 0.05f, 0.20f, 0.0f},
	{"bright medium", 0.65f, 0.05f, 0.25f, 0.0f},
	{"bright large", 0.80f, 0.05f, 0.30f, 0.0f},
	{"water 1", 0.50f, 0.85f, 0.20f, 1500.0f},
	{"water 2", 0.65f, 0.88f, 0.25f, 1000.0f},
	{"water 3", 0.80f, 0.90f, 0.30f, 700.0f},
	{"concrete small", 0.55f, 0.35f, 0.20f, 0.0f},
	{"concrete medium", 0.70f, 0.35f, 0.25f, 0.0f},
	{"concrete large", 0.85f, 0.35f, 0.30f, 0.0f},
	{"big 1", 0.80f, 0.50f, 0.25f, 0.0f},
	{"big 2", 0.88f, 0.50f, 0.30f, 0.0f},
	{"big 3", 0.93f, 0.50f, 0.35f, 0.0f},
	{"cavern small", 0.90f, 0.40f, 0.30f, 0.0f},
	{"cavern medium", 0.94f, 0.40f, 0.35f, 0.0f},
	{"cavern large", 0.97f, 0.40f, 0.40f, 0.0f},
	{"weirdo 1", 0.95f, 0.10f, 0.35f, 0.0f},
	{"weirdo 2", 0.97f, 0.05f, 0.40f, 0.0f},
	{"weirdo 3", 0.98f, 0.00f, 0.45f, 0.0f},
};

int GMAudio_RoomCount()
{
	return (int)(sizeof(kRooms) / sizeof(kRooms[0]));
}

const GMRoomPreset& GMAudio_RoomPreset(int roomType)
{
	if (roomType < 0 || roomType >= GMAudio_RoomCount())
		return kRooms[0];
	return kRooms[roomType];
}

// ---------------------------------------------------------------------------------------
//  Reverb: 8 parallel lowpass-feedback combs + 4 series allpasses per ear
//  (Freeverb tuning, public domain). Lengths are for 44.1 kHz and scaled to the real rate.
// ---------------------------------------------------------------------------------------

static const int kCombTuning[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static const int kAllpassTuning[4] = {556, 441, 341, 225};
static const int kStereoSpread = 23;

GMReverb::GMReverb()
{
	SetRate(44100);
}

void GMReverb::SetRate(int rate)
{
	m_Rate = std::max(8000, rate);
	const double scale = (double)m_Rate / 44100.0;

	for (int i = 0; i < kCombs; ++i)
	{
		m_CombL[i].buf.assign(std::max<size_t>(1, (size_t)(kCombTuning[i] * scale)), 0.0f);
		m_CombR[i].buf.assign(std::max<size_t>(1, (size_t)((kCombTuning[i] + kStereoSpread) * scale)), 0.0f);
	}

	for (int i = 0; i < kAllpasses; ++i)
	{
		m_AllL[i].buf.assign(std::max<size_t>(1, (size_t)(kAllpassTuning[i] * scale)), 0.0f);
		m_AllR[i].buf.assign(std::max<size_t>(1, (size_t)((kAllpassTuning[i] + kStereoSpread) * scale)), 0.0f);
	}

	Clear();
}

void GMReverb::Clear()
{
	for (int i = 0; i < kCombs; ++i)
	{
		std::fill(m_CombL[i].buf.begin(), m_CombL[i].buf.end(), 0.0f);
		std::fill(m_CombR[i].buf.begin(), m_CombR[i].buf.end(), 0.0f);
		m_CombL[i].idx = m_CombR[i].idx = 0;
		m_CombL[i].store = m_CombR[i].store = 0.0f;
	}

	for (int i = 0; i < kAllpasses; ++i)
	{
		std::fill(m_AllL[i].buf.begin(), m_AllL[i].buf.end(), 0.0f);
		std::fill(m_AllR[i].buf.begin(), m_AllR[i].buf.end(), 0.0f);
		m_AllL[i].idx = m_AllR[i].idx = 0;
	}

}

void GMReverb::SetTarget(float room, float damp, float wet)
{
	m_TargetRoom = std::min(0.99f, std::max(0.0f, room));
	m_TargetDamp = std::min(1.0f, std::max(0.0f, damp));
	m_TargetWet = std::min(1.0f, std::max(0.0f, wet));
}

bool GMReverb::Active() const
{
	// when it is turned off the wet level fades out first, then it goes idle
	return m_TargetWet > 0.0f || m_Wet > 0.0f;
}

static inline float CombProcess(std::vector<float>& buf, size_t& idx, float& store, float input, float feedback, float damp1, float damp2)
{
	const float output = buf[idx];
	store = output * damp2 + store * damp1;
	buf[idx] = input + store * feedback;
	if (++idx >= buf.size())
		idx = 0;
	return output;
}

static inline float AllpassProcess(std::vector<float>& buf, size_t& idx, float input)
{
	const float bufout = buf[idx];
	const float output = -input + bufout;
	buf[idx] = input + bufout * 0.5f;
	if (++idx >= buf.size())
		idx = 0;
	return output;
}

void GMReverb::Process(const float* in, float* out, int frames)
{
	if (frames <= 0)
		return;

	// fade between rooms / on / off (about 0.3 s with 1024 frame blocks)
	const float k = 0.15f;
	m_Room += (m_TargetRoom - m_Room) * k;
	m_Damp += (m_TargetDamp - m_Damp) * k;
	m_Wet += (m_TargetWet - m_Wet) * k;
	if (m_TargetWet == 0.0f && m_Wet < 0.0001f)
		m_Wet = 0.0f;

	if (m_Wet == 0.0f && m_TargetWet == 0.0f)
	{
		if (!m_Idle)
		{
			Clear(); // so turning it on again doesn't play an old tail
			m_Idle = true;
		}
		return;
	}

	m_Idle = false;

	const float feedback = m_Room * 0.28f + 0.7f;
	const float damp1 = m_Damp * 0.4f;
	const float damp2 = 1.0f - damp1;
	const float wet = m_Wet * 3.0f;

	for (int f = 0; f < frames; ++f)
	{
		const float input = in[f] * 0.03f;
		float l = 0.0f, r = 0.0f;

		for (int i = 0; i < kCombs; ++i)
		{
			l += CombProcess(m_CombL[i].buf, m_CombL[i].idx, m_CombL[i].store, input, feedback, damp1, damp2);
			r += CombProcess(m_CombR[i].buf, m_CombR[i].idx, m_CombR[i].store, input, feedback, damp1, damp2);
		}

		for (int i = 0; i < kAllpasses; ++i)
		{
			l = AllpassProcess(m_AllL[i].buf, m_AllL[i].idx, l);
			r = AllpassProcess(m_AllR[i].buf, m_AllR[i].idx, r);
		}

		out[2 * f] += l * wet;
		out[2 * f + 1] += r * wet;
	}
}

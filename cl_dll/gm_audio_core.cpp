#include "gm_audio_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------------------
//  WAV decoder
// ---------------------------------------------------------------------------------------

static uint16_t Rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t Rd32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool GMAudio_DecodeWav(const unsigned char* data, size_t size, GMSoundData& out, std::string& error)
{
	out.samples.clear();

	if (!data || size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
	{
		error = "not a RIFF/WAVE file";
		return false;
	}

	bool haveFmt = false;
	int format = 0, channels = 0, bits = 0;
	uint32_t rate = 0;
	const unsigned char* pcm = nullptr;
	size_t pcmSize = 0;

	size_t pos = 12;
	while (pos + 8 <= size)
	{
		const unsigned char* id = data + pos;
		const uint32_t chunkSize = Rd32(data + pos + 4);
		pos += 8;
		const size_t avail = size - pos;

		if (memcmp(id, "fmt ", 4) == 0)
		{
			if (chunkSize < 16 || avail < 16)
			{
				error = "truncated fmt chunk";
				return false;
			}

			format = Rd16(data + pos);
			channels = Rd16(data + pos + 2);
			rate = Rd32(data + pos + 4);
			bits = Rd16(data + pos + 14);

			// WAVE_FORMAT_EXTENSIBLE: the real format is the first 2 bytes of the sub-format GUID
			if (format == 0xFFFE && chunkSize >= 40 && avail >= 40)
				format = Rd16(data + pos + 24);

			haveFmt = true;
		}
		else if (memcmp(id, "data", 4) == 0)
		{
			pcm = data + pos;
			// Some writers leave 0 or 0xFFFFFFFF when they don't know the size: use what is there.
			pcmSize = (chunkSize == 0 || chunkSize > avail) ? avail : chunkSize;
			break;
		}

		if (chunkSize > avail)
			break;

		pos += (size_t)chunkSize + (chunkSize & 1); // chunks are padded to an even size
	}

	if (!haveFmt)
	{
		error = "missing fmt chunk";
		return false;
	}

	if (!pcm)
	{
		error = "missing data chunk";
		return false;
	}

	const bool isPcm = (format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32));
	const bool isFloat = (format == 3 && bits == 32);

	if (!isPcm && !isFloat)
	{
		error = "unsupported format (only PCM 8/16/24/32 bit and float32)";
		return false;
	}

	if (channels < 1 || channels > 8)
	{
		error = "unsupported number of channels";
		return false;
	}

	if (rate < 1000 || rate > 192000)
	{
		error = "unsupported sample rate";
		return false;
	}

	const size_t bytesPerSample = (size_t)bits / 8;
	const size_t frameBytes = bytesPerSample * (size_t)channels;
	const size_t frames = pcmSize / frameBytes;

	if (frames == 0)
	{
		error = "no audio data";
		return false;
	}

	if (frames > 16u * 1024u * 1024u)
	{
		error = "sound too long (use it as music, not as a sound effect)";
		return false;
	}

	out.rate = (int)rate;
	out.samples.resize(frames);

	for (size_t f = 0; f < frames; ++f)
	{
		const unsigned char* p = pcm + f * frameBytes;
		float sum = 0.0f;

		for (int c = 0; c < channels; ++c, p += bytesPerSample)
		{
			float v;

			if (isFloat)
			{
				uint32_t u = Rd32(p);
				memcpy(&v, &u, sizeof(v));
			}
			else if (bits == 8)
			{
				v = ((int)p[0] - 128) / 128.0f; // 8 bit wav is unsigned
			}
			else if (bits == 16)
			{
				v = (int16_t)Rd16(p) / 32768.0f;
			}
			else if (bits == 24)
			{
				int32_t s = (int32_t)(((uint32_t)p[0] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 24)) >> 8; // sign extend
				v = s / 8388608.0f;
			}
			else
			{
				v = (int32_t)Rd32(p) / 2147483648.0f;
			}

			sum += v;
		}

		out.samples[f] = sum / (float)channels;
	}

	return true;
}

// ---------------------------------------------------------------------------------------
//  Spatialization
// ---------------------------------------------------------------------------------------

void GMAudio_Spatialize(const GMVec3& listener, const GMVec3& right, const GMVec3& source,
	float attenuation, float& left, float& rightOut)
{
	if (attenuation <= 0.0f)
	{
		left = rightOut = 1.0f;
		return;
	}

	const float dx = source.x - listener.x;
	const float dy = source.y - listener.y;
	const float dz = source.z - listener.z;
	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	const float gain = 1.0f - dist * (attenuation / 1000.0f); // SOUND_NOMINAL_CLIP_DIST = 1000

	if (gain <= 0.0f)
	{
		left = rightOut = 0.0f;
		return;
	}

	float dot = 0.0f;
	if (dist > 0.001f)
		dot = (dx * right.x + dy * right.y + dz * right.z) / dist;

	// Like the engine: at the center both ears get 'gain'; to one side the near ear gets up to twice that (clipped at 1)
	left = std::min(1.0f, gain * (1.0f - dot));
	rightOut = std::min(1.0f, gain * (1.0f + dot));
}

// ---------------------------------------------------------------------------------------
//  Mixer
// ---------------------------------------------------------------------------------------

void GMMixer::SetOutputRate(int rate)
{
	if (rate >= 8000 && rate <= 192000)
		m_OutputRate = rate;
}

static bool ChannelReplaces(int channel)
{
	return channel != 0 && channel != 6; // CHAN_AUTO and CHAN_STATIC never replace
}

void GMMixer::Play(std::shared_ptr<const GMSoundData> sound, const GMVoiceParams& params)
{
	if (!sound || sound->samples.empty())
		return;

	if (ChannelReplaces(params.channel))
		Stop(params.entity, params.channel);

	if ((int)m_Voices.size() >= kMaxVoices)
	{
		// Full: drop the least audible one (the oldest if they tie)
		size_t victim = 0;
		float victimLevel = 1e9f;

		for (size_t i = 0; i < m_Voices.size(); ++i)
		{
			const Voice& v = m_Voices[i];
			const float level = v.fresh ? 1.0f : std::max(v.curL, v.curR);

			if (level < victimLevel || (level == victimLevel && v.serial < m_Voices[victim].serial))
			{
				victimLevel = level;
				victim = i;
			}
		}

		m_Voices.erase(m_Voices.begin() + (std::ptrdiff_t)victim);
	}

	Voice v;
	v.sound = std::move(sound);
	v.params = params;
	v.params.volume = std::min(1.0f, std::max(0.0f, v.params.volume));
	v.params.pitch = std::min(4.0f, std::max(0.05f, v.params.pitch));
	v.serial = m_NextSerial++;
	m_Voices.push_back(std::move(v));
}

void GMMixer::Stop(int entity, int channel)
{
	m_Voices.erase(std::remove_if(m_Voices.begin(), m_Voices.end(),
					   [&](const Voice& v) { return v.params.entity == entity && v.params.channel == channel; }),
		m_Voices.end());
}

void GMMixer::StopAll()
{
	m_Voices.clear();
}

void GMMixer::SetListener(const GMVec3& origin, const GMVec3& right, float masterVolume)
{
	m_Listener = origin;
	m_ListenerRight = right;
	m_Master = std::min(1.0f, std::max(0.0f, masterVolume));
}

void GMMixer::SetEntityOrigin(int entity, const GMVec3& origin)
{
	for (Voice& v : m_Voices)
	{
		if (v.params.entity == entity)
			v.params.origin = origin;
	}
}

void GMMixer::GetEntities(std::vector<int>& out) const
{
	out.clear();

	for (const Voice& v : m_Voices)
	{
		if (v.params.entity > 0 && std::find(out.begin(), out.end(), v.params.entity) == out.end())
			out.push_back(v.params.entity);
	}
}

void GMMixer::Mix(float* out, int frames)
{
	if (frames <= 0)
		return;

	std::fill(out, out + (size_t)frames * 2, 0.0f);

	for (size_t i = 0; i < m_Voices.size();)
	{
		Voice& v = m_Voices[i];

		float tgtL = 1.0f, tgtR = 1.0f;
		if (!v.params.global)
			GMAudio_Spatialize(m_Listener, m_ListenerRight, v.params.origin, v.params.attenuation, tgtL, tgtR);

		tgtL *= v.params.volume * m_Master;
		tgtR *= v.params.volume * m_Master;

		if (v.fresh) // first block: no fade-in from silence
		{
			v.curL = tgtL;
			v.curR = tgtR;
			v.fresh = false;
		}

		// the volume moves smoothly across the block, so a source that moves doesn't click
		const float dL = (tgtL - v.curL) / (float)frames;
		const float dR = (tgtR - v.curR) / (float)frames;
		float gL = v.curL, gR = v.curR;

		const float* s = v.sound->samples.data();
		const size_t count = v.sound->samples.size();
		const double step = ((double)v.sound->rate / (double)m_OutputRate) * (double)v.params.pitch;
		double pos = v.pos;

		int f = 0;
		for (; f < frames; ++f)
		{
			const size_t idx = (size_t)pos;
			if (idx >= count)
				break;

			const float s0 = s[idx];
			const float s1 = (idx + 1 < count) ? s[idx + 1] : s0;
			const float smp = s0 + (s1 - s0) * (float)(pos - (double)idx);

			out[2 * f] += smp * gL;
			out[2 * f + 1] += smp * gR;

			gL += dL;
			gR += dR;
			pos += step;
		}

		v.pos = pos;
		v.curL = tgtL;
		v.curR = tgtR;

		if (f < frames || (size_t)pos >= count)
			m_Voices.erase(m_Voices.begin() + (std::ptrdiff_t)i); // finished
		else
			++i;
	}

	for (int k = 0; k < frames * 2; ++k)
		out[k] = std::min(1.0f, std::max(-1.0f, out[k]));
}

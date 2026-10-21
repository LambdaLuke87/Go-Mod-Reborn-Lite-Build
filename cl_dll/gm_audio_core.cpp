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

	m_Reverb.SetRate(m_OutputRate);
}

static bool ChannelReplaces(int channel)
{
	return channel != 0 && channel != 6; // CHAN_AUTO and CHAN_STATIC never replace
}

void GMMixer::Play(std::shared_ptr<const GMSoundData> sound, const GMVoiceParams& params)
{
	GMSegment seg;
	seg.sound = std::move(sound);

	std::vector<GMSegment> segments;
	segments.push_back(std::move(seg));
	Play(segments, params);
}

static size_t SegmentStart(const GMSegment& seg)
{
	const size_t count = seg.sound->samples.size();
	return std::min(count, (size_t)((double)count * std::min(1.0f, std::max(0.0f, seg.start))));
}

static size_t SegmentEnd(const GMSegment& seg)
{
	const size_t count = seg.sound->samples.size();
	return std::min(count, (size_t)((double)count * std::min(1.0f, std::max(0.0f, seg.end))));
}

void GMMixer::Play(const std::vector<GMSegment>& segments, const GMVoiceParams& params)
{
	std::vector<GMSegment> valid;
	for (const GMSegment& seg : segments)
	{
		if (seg.sound && !seg.sound->samples.empty() && SegmentEnd(seg) > SegmentStart(seg))
			valid.push_back(seg);
	}

	if (valid.empty())
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
	v.segments = std::move(valid);
	v.segment = 0;
	v.pos = (double)SegmentStart(v.segments[0]);
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

void GMMixer::SetListenerForward(const GMVec3& forward)
{
	m_ListenerForward = forward;
}

void GMMixer::SetEffects(const GMEffects& fx)
{
	m_Fx = fx;
}

void GMMixer::GetSources(std::vector<GMVoiceSource>& out) const
{
	out.clear();
	out.reserve(m_Voices.size());

	for (const Voice& v : m_Voices)
	{
		GMVoiceSource src;
		src.serial = v.serial;
		src.entity = v.params.entity;
		src.origin = v.params.origin;
		src.attenuation = v.params.attenuation;
		src.global = v.params.global;
		out.push_back(src);
	}
}

void GMMixer::SetVoiceFx(unsigned long long serial, float occlusion, float doppler)
{
	for (Voice& v : m_Voices)
	{
		if (v.serial == serial)
		{
			v.occlusion = std::min(1.0f, std::max(0.0f, occlusion));
			v.doppler = std::min(2.0f, std::max(0.5f, doppler));
			return;
		}
	}
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

void GMMixer::GetVoiceLevels(std::vector<std::pair<int, float>>& out) const
{
	out.clear();

	for (const Voice& v : m_Voices)
	{
		if (v.params.entity <= 0 || v.params.channel != 2) // CHAN_VOICE
			continue;

		bool found = false;
		for (auto& e : out)
		{
			if (e.first == v.params.entity)
			{
				e.second = std::max(e.second, v.level);
				found = true;
				break;
			}
		}

		if (!found)
			out.emplace_back(v.params.entity, v.level);
	}
}

static inline float Lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

// mixes one voice into 'out' (stereo) and its reverb send into 'send' (mono).
// Leaves v.segment == v.segments.size() when the voice is over.
void GMMixer::MixVoice(Voice& v, float* out, float* send, int frames)
{
	const float rate = (float)m_OutputRate;
	const bool global = v.params.global;

	// ------------------------------------------------ geometry
	float tgtL = 1.0f, tgtR = 1.0f;
	float dist = 0.0f, lateral = 0.0f, front = 1.0f, distGain = 1.0f;

	if (!global)
	{
		GMAudio_Spatialize(m_Listener, m_ListenerRight, v.params.origin, v.params.attenuation, tgtL, tgtR);

		const float dx = v.params.origin.x - m_Listener.x;
		const float dy = v.params.origin.y - m_Listener.y;
		const float dz = v.params.origin.z - m_Listener.z;
		dist = std::sqrt(dx * dx + dy * dy + dz * dz);

		if (dist > 0.001f)
		{
			lateral = (dx * m_ListenerRight.x + dy * m_ListenerRight.y + dz * m_ListenerRight.z) / dist;
			front = (dx * m_ListenerForward.x + dy * m_ListenerForward.y + dz * m_ListenerForward.z) / dist;
		}

		distGain = v.params.attenuation > 0.0f ? std::max(0.0f, 1.0f - dist * (v.params.attenuation / 1000.0f)) : 1.0f;

		if (m_Fx.spatial && dist > 1.0f)
		{
			// the time difference and the head shadow do most of the work: softer level difference
			tgtL = std::min(1.0f, distGain * (1.0f - 0.35f * lateral));
			tgtR = std::min(1.0f, distGain * (1.0f + 0.35f * lateral));
		}
	}

	// ------------------------------------------------ occlusion / doppler (smooth)
	v.occlusionCur += (v.occlusion - v.occlusionCur) * 0.2f;
	v.dopplerCur += (v.doppler - v.dopplerCur) * 0.3f;

	const float occ = global ? 0.0f : v.occlusionCur;
	const float occGain = 1.0f - 0.6f * occ;

	tgtL *= v.params.volume * m_Master * occGain;
	tgtR *= v.params.volume * m_Master * occGain;

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

	// ------------------------------------------------ filters
	float cutoff = 22000.0f;
	if (m_Fx.airAbsorption && !global)
		cutoff = std::min(cutoff, std::max(3000.0f, 20000.0f * std::exp(-dist / 3000.0f)));
	if (occ > 0.001f)
		cutoff = std::min(cutoff, Lerp(20000.0f, 800.0f, occ));
	const float lpA = GMAudio_LowpassCoef(cutoff, rate);
	const bool useLowpass = lpA < 1.0f;

	const bool spatial = m_Fx.spatial && !global && dist > 1.0f;
	float shA_L = 1.0f, shA_R = 1.0f;
	float itdTargetL = 0.0f, itdTargetR = 0.0f;

	if (spatial)
	{
		// ~0.66 ms max interaural delay; the far ear also loses high frequencies
		const float maxItd = std::min(60.0f, 0.00066f * rate);
		const float side = std::fabs(lateral);
		const float farCut = Lerp(20000.0f, 3500.0f, side);
		const float behindCut = front < 0.0f ? Lerp(20000.0f, 5000.0f, -front) : 20000.0f;

		if (lateral > 0.0f) // source on the right: the left ear is the far one
		{
			itdTargetL = side * maxItd;
			shA_L = GMAudio_LowpassCoef(std::min(farCut, behindCut), rate);
			shA_R = GMAudio_LowpassCoef(behindCut, rate);
		}
		else
		{
			itdTargetR = side * maxItd;
			shA_R = GMAudio_LowpassCoef(std::min(farCut, behindCut), rate);
			shA_L = GMAudio_LowpassCoef(behindCut, rate);
		}
	}

	const float itdStepL = (itdTargetL - v.itdL) / (float)frames;
	const float itdStepR = (itdTargetR - v.itdR) / (float)frames;

	// ------------------------------------------------ reverb send
	float sendGain = 0.0f;
	if (send && m_Fx.reverb && distGain > 0.0f)
		sendGain = v.params.volume * m_Master * (global ? 0.3f : (0.25f + 0.45f * distGain)) * occGain;

	// ------------------------------------------------ samples
	float sumAbs = 0.0f;
	int mixed = 0;
	int f = 0;

	while (f < frames && v.segment < v.segments.size())
	{
		const GMSegment& seg = v.segments[v.segment];
		const float* s = seg.sound->samples.data();
		const size_t start = SegmentStart(seg);
		const size_t end = SegmentEnd(seg);
		const double step = ((double)seg.sound->rate / (double)m_OutputRate) * (double)v.params.pitch *
							(double)std::min(4.0f, std::max(0.05f, seg.pitch)) * (double)(global ? 1.0f : v.dopplerCur);
		const float segVolume = std::min(2.0f, std::max(0.0f, seg.volume));
		const bool cubic = m_Fx.interpolation == 1;
		const bool vibrato = m_Fx.vibrato;
		const double vibDepth = std::min(0.9, std::max(0.0, (double)m_Fx.vibratoDepth));
		const double vibStep = std::min(30.0, std::max(0.1, (double)m_Fx.vibratoRate)) / (double)m_OutputRate;
		double pos = v.pos;

		for (; f < frames; ++f)
		{
			const size_t idx = (size_t)pos;
			if (idx >= end)
				break;

			const float t = (float)(pos - (double)idx);
			const float x0 = s[idx];
			const float x1 = (idx + 1 < end) ? s[idx + 1] : x0;
			float smp;

			if (cubic)
			{
				const float xm1 = (idx > start) ? s[idx - 1] : x0;
				const float x2 = (idx + 2 < end) ? s[idx + 2] : x1;
				const float c1 = 0.5f * (x1 - xm1);
				const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
				const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
				smp = ((c3 * t + c2) * t + c1) * t + x0;
			}
			else
			{
				smp = x0 + (x1 - x0) * t;
			}

			smp *= segVolume;
			sumAbs += std::fabs(smp);
			mixed++;

			if (useLowpass)
				smp = v.lowpass.Process(smp, lpA);

			float l = smp, r = smp;

			if (spatial)
			{
				v.delay[v.delayPos] = smp;

				auto readDelay = [&](float d) -> float
				{
					const int di = (int)d;
					const float fr = d - (float)di;
					const float a = v.delay[(v.delayPos - di + 64) & 63];
					const float b = v.delay[(v.delayPos - di - 1 + 64) & 63];
					return a + (b - a) * fr;
				};

				l = v.shadowL.Process(readDelay(v.itdL), shA_L);
				r = v.shadowR.Process(readDelay(v.itdR), shA_R);

				v.delayPos = (v.delayPos + 1) & 63;
				v.itdL += itdStepL;
				v.itdR += itdStepR;
			}

			out[2 * f] += l * gL;
			out[2 * f + 1] += r * gR;

			if (sendGain > 0.0f)
				send[f] += smp * sendGain;

			gL += dL;
			gR += dR;

			if (vibrato)
			{
				pos += step * (1.0 + vibDepth * std::sin(6.283185307179586 * v.vibratoPhase));
				v.vibratoPhase += vibStep;
				if (v.vibratoPhase >= 1.0)
					v.vibratoPhase -= 1.0;
			}
			else
			{
				pos += step;
			}
		}

		v.pos = pos;

		if ((size_t)pos >= end) // this segment is over: next word
		{
			v.segment++;
			if (v.segment < v.segments.size())
				v.pos = (double)SegmentStart(v.segments[v.segment]);
		}
	}

	if (spatial)
	{
		v.itdL = itdTargetL;
		v.itdR = itdTargetR;
	}

	const float blockLevel = mixed > 0 ? (sumAbs / (float)mixed) * v.params.volume : 0.0f;
	v.level = v.level * 0.5f + blockLevel * 0.5f;

	v.curL = tgtL;
	v.curR = tgtR;
}

void GMMixer::Mix(float* out, int frames)
{
	if (frames <= 0)
		return;

	std::fill(out, out + (size_t)frames * 2, 0.0f);

	if ((int)m_Send.size() < frames)
		m_Send.resize((size_t)frames);
	std::fill(m_Send.begin(), m_Send.begin() + frames, 0.0f);

	for (size_t i = 0; i < m_Voices.size();)
	{
		Voice& v = m_Voices[i];
		MixVoice(v, out, m_Send.data(), frames);

		if (v.segment >= v.segments.size())
			m_Voices.erase(m_Voices.begin() + (std::ptrdiff_t)i); // finished
		else
			++i;
	}

	// ------------------------------------------------ room reverb
	const GMRoomPreset& room = GMAudio_RoomPreset(m_Fx.reverb ? m_Fx.roomType : 0);
	m_Reverb.SetTarget(room.room, room.damp, m_Fx.reverb ? room.wet * std::max(0.0f, m_Fx.reverbMix) : 0.0f);
	if (m_Reverb.Active())
		m_Reverb.Process(m_Send.data(), out, frames);

	// ------------------------------------------------ underwater / water rooms: muffle everything
	float waterCut = 0.0f;
	if (m_Fx.underwater)
		waterCut = 700.0f;
	else if (m_Fx.reverb && room.lowpass > 0.0f)
		waterCut = room.lowpass;

	const float waterTarget = waterCut > 0.0f ? 1.0f : 0.0f;
	m_WaterMix += (waterTarget - m_WaterMix) * 0.2f;
	if (waterTarget == 0.0f && m_WaterMix < 0.001f)
		m_WaterMix = 0.0f;

	if (m_WaterMix > 0.0f)
	{
		const float a = GMAudio_LowpassCoef(waterCut > 0.0f ? waterCut : 700.0f, (float)m_OutputRate);
		const float vol = m_Fx.underwater ? Lerp(1.0f, 0.8f, m_WaterMix) : 1.0f;

		for (int k = 0; k < frames; ++k)
		{
			const float l = out[2 * k], r = out[2 * k + 1];
			out[2 * k] = Lerp(l, m_WaterL.Process(l, a), m_WaterMix) * vol;
			out[2 * k + 1] = Lerp(r, m_WaterR.Process(r, a), m_WaterMix) * vol;
		}
	}
	else
	{
		m_WaterL.Reset();
		m_WaterR.Reset();
	}

	for (int k = 0; k < frames * 2; ++k)
		out[k] = std::min(1.0f, std::max(-1.0f, out[k]));
}

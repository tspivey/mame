// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    speaker.cpp

    Speaker output sound device.

***************************************************************************/

#include "emu.h"
#include "emuopts.h"
#include "speaker.h"



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
DEFINE_DEVICE_TYPE(SPEAKER, speaker_device, "speaker", "Speaker")

const speaker_device::position_name_mapping speaker_device::position_name_mappings[] = {
	{  0.0,  0.0,  1.0, "Front center" },
	{ -0.2,  0.0,  1.0, "Front left"   },
	{  0.0, -0.5,  1.0, "Front floor" },
	{  0.2,  0.0,  1.0, "Front right" },
	{  0.0,  0.0, -0.5, "Rear center" },
	{ -0.2,  0.0, -0.5, "Rear left" },
	{  0.2,  0.0, -0.5, "Read right" },
	{  0.0,  0.0, -0.1, "Headrest center" },
	{ -0.1,  0.0, -0.1, "Headrest left" },
	{  0.1,  0.0, -0.1, "Headrest right" },
	{  0.0, -0.5,  0.0, "Seat" },
	{  0.0, -0.2,  0.1, "Backrest" },
	{ }
};

std::string speaker_device::get_position_name(u32 channel) const
{
	for(unsigned int i = 0; position_name_mappings[i].m_name; i++)
		if(m_positions[channel][0] == position_name_mappings[i].m_x && m_positions[channel][1] == position_name_mappings[i].m_y && m_positions[channel][2] == position_name_mappings[i].m_z)
			return position_name_mappings[i].m_name;
	return util::string_format("#%d", channel);
}

//**************************************************************************
//  LIVE SPEAKER DEVICE
//**************************************************************************

//-------------------------------------------------
//  speaker_device - constructor
//-------------------------------------------------

speaker_device::speaker_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 channels)
	: device_t(mconfig, SPEAKER, tag, owner, 0)
	, device_mixer_interface(mconfig, *this, channels ? channels : 1)
	, m_positions(channels ? channels : 1)
	, m_current_max(0)
	, m_samples_this_bucket(0)
{
}


//-------------------------------------------------
//  ~speaker_device - destructor
//-------------------------------------------------

speaker_device::~speaker_device()
{
}


//-------------------------------------------------
//  set_position - set speaker position
//-------------------------------------------------

speaker_device &speaker_device::set_position(u32 channel, double x, double y, double z)
{
	m_positions[channel][0] = x;
	m_positions[channel][1] = y;
	m_positions[channel][2] = z;
	return *this;
}


//-------------------------------------------------
//  update - output the samples from the speaker's stream
//-------------------------------------------------

void speaker_device::update(u32 channel, stream_buffer::sample_t *output, attotime start, attotime end, int expected_samples)
{
	// skip if no stream
	if (m_mixer_stream == nullptr)
		return;

	// skip if invalid range
	if (start > end)
		return;

	// get a view on the desired range
	read_stream_view view = m_mixer_stream->update_view(start, end, channel);
	sound_assert(view.samples() >= expected_samples);

	// track maximum sample value for each 0.1s bucket
	if (machine().options().speaker_report() != 0)
	{
		u32 samples_per_bucket = m_mixer_stream->sample_rate() / BUCKETS_PER_SECOND;
		for (int sample = 0; sample < expected_samples; sample++)
		{
			m_current_max = std::max(m_current_max, fabsf(view.get(sample)));
			if (++m_samples_this_bucket >= samples_per_bucket)
			{
				m_max_sample.push_back(m_current_max);
				m_current_max = 0.0f;
				m_samples_this_bucket = 0;
			}
		}
	}

	for (int sample = 0; sample < expected_samples; sample++)
		output[sample] = view.get(sample);
}


//-------------------------------------------------
//  device_start - handle device startup
//-------------------------------------------------

void speaker_device::device_start()
{
}


//-------------------------------------------------
//  device_stop - cleanup and report
//-------------------------------------------------

void speaker_device::device_stop()
{
	// level 1: just report if there was any clipping
	// level 2: report the overall maximum, even if no clipping
	// level 3: print a detailed list of all the times there was clipping
	// level 4: print a detailed list of every bucket
	int report = machine().options().speaker_report();
	if (report != 0)
	{
		m_max_sample.push_back(m_current_max);

		// determine overall maximum and number of clipped buckets
		stream_buffer::sample_t overallmax = 0;
		u32 clipped = 0;
		for (auto &curmax : m_max_sample)
		{
			overallmax = std::max(overallmax, curmax);
			if (curmax > stream_buffer::sample_t(1.0))
				clipped++;
		}

		// levels 1 and 2 just get a summary
		if (clipped != 0 || report == 2 || report == 4)
			osd_printf_info("Speaker \"%s\" - max = %.5f (gain *= %.3f) - clipped in %d/%d (%d%%) buckets\n", tag(), overallmax, 1 / (overallmax ? overallmax : 1), clipped, m_max_sample.size(), clipped * 100 / m_max_sample.size());

		// levels 3 and 4 get a full dump
		if (report >= 3)
		{
			static char const * const s_stars  = "************************************************************";
			static char const * const s_spaces = "                                                            ";
			int totalstars = strlen(s_stars);
			double t = 0;
			if (overallmax < 1.0)
				overallmax = 1.0;
			int leftstars = totalstars / overallmax;
			for (auto &curmax : m_max_sample)
			{
				if (curmax > stream_buffer::sample_t(1.0) || report == 4)
				{
					osd_printf_info("%6.1f: %9.5f |", t, curmax);
					if (curmax == 0)
						osd_printf_info("%.*s|\n", leftstars, s_spaces);
					else if (curmax <= 1.0)
					{
						int stars = std::max(1, std::min(leftstars, int(curmax * totalstars / overallmax)));
						osd_printf_info("%.*s", stars, s_stars);
						int spaces = leftstars - stars;
						if (spaces != 0)
							osd_printf_info("%.*s", spaces, s_spaces);
						osd_printf_info("|\n");
					}
					else
					{
						int rightstars = std::max(1, std::min(totalstars, int(curmax * totalstars / overallmax)) - leftstars);
						osd_printf_info("%.*s|%.*s\n", leftstars, s_stars, rightstars, s_stars);
					}
				}
				t += 1.0 / double(BUCKETS_PER_SECOND);
			}
		}
	}
}

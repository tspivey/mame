// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    speaker.h

    Speaker output sound device.

    Speakers have (x, y, z) coordinates in 3D space:
    * Observer is at position (0, 0, 0)
    * Positive x is to the right of the observer
    * Negative x is to the left of the observer
    * Positive y is above the observer
    * Negative y is below the observer
    * Positive z is in front of the observer
    * Negative z is behind the observer

***************************************************************************/

#ifndef MAME_EMU_SPEAKER_H
#define MAME_EMU_SPEAKER_H

#pragma once


//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
DECLARE_DEVICE_TYPE(SPEAKER, speaker_device)



//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class speaker_device : public device_t, public device_mixer_interface
{
public:
	// construction/destruction
	speaker_device(const machine_config &mconfig, const char *tag, device_t *owner, double x, double y, double z)
		: speaker_device(mconfig, tag, owner, 0)
	{
		set_position(0, x, y, z);
	}
	speaker_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 channels = 1); // Collides with clock, but not important
	virtual ~speaker_device();

	// configuration helpers
	speaker_device &set_position(u32 channel, double x, double y, double z);
	speaker_device &front_center(u32 channel = 0)      { return set_position(channel,  0.0,  0.0,  1.0); }
	speaker_device &front_left(u32 channel = 0)        { return set_position(channel, -0.2,  0.0,  1.0); }
	speaker_device &front_floor(u32 channel = 0)       { return set_position(channel,  0.0, -0.5,  1.0); }
	speaker_device &front_right(u32 channel = 0)       { return set_position(channel,  0.2,  0.0,  1.0); }
	speaker_device &rear_center(u32 channel = 0)       { return set_position(channel,  0.0,  0.0, -0.5); }
	speaker_device &rear_left(u32 channel = 0)         { return set_position(channel, -0.2,  0.0, -0.5); }
	speaker_device &rear_right(u32 channel = 0)        { return set_position(channel,  0.2,  0.0, -0.5); }
	speaker_device &headrest_center(u32 channel = 0)   { return set_position(channel,  0.0,  0.0, -0.1); }
	speaker_device &headrest_left(u32 channel = 0)     { return set_position(channel, -0.1,  0.0, -0.1); }
	speaker_device &headrest_right(u32 channel = 0)    { return set_position(channel,  0.1,  0.0, -0.1); }
	speaker_device &seat(u32 channel = 0)              { return set_position(channel,  0.0, -0.5,  0.0); }
	speaker_device &backrest(u32 channel = 0)          { return set_position(channel,  0.0, -0.2,  0.1); }
	speaker_device &front()                            { return front_left(0).front_right(1); }
	speaker_device &rear()                             { return rear_left(0).rear_right(1); }
	speaker_device &corners()                          { return front_left(0).front_right(1).rear_left(2).rear_right(3); }
	std::array<double, 3> get_position(u32 channel) const { return m_positions[channel]; }
	std::string get_position_name(u32 channel) const;

	// internally for use by the sound system
	void update(u32 channel, stream_buffer::sample_t *output, attotime start, attotime end, int expected_samples);

protected:
	struct position_name_mapping {
		double m_x, m_y, m_z;
		const char *m_name;
	};

	static const position_name_mapping position_name_mappings[];

	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

	// configuration state
	std::vector<std::array<double, 3>> m_positions;

	// internal state
	static constexpr int BUCKETS_PER_SECOND = 10;
	std::vector<stream_buffer::sample_t> m_max_sample;
	stream_buffer::sample_t m_current_max;
	u32 m_samples_this_bucket;
};


// speaker device iterator
using speaker_device_enumerator = device_type_enumerator<speaker_device>;


#endif // MAME_EMU_SPEAKER_H

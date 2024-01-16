// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    ui/audiomix.h

    Audio mixing/mapping control

***************************************************************************/

#ifndef MAME_FRONTEND_UI_AUDIOMIX_H
#define MAME_FRONTEND_UI_AUDIOMIX_H

#pragma once

#include "ui/menu.h"


namespace ui {

class menu_audio_mixer : public menu
{
public:
	menu_audio_mixer(mame_ui_manager &mui, render_container &container);
	virtual ~menu_audio_mixer() override;

protected:
	virtual void recompute_metrics(uint32_t width, uint32_t height, float aspect) override;
	virtual void custom_render(void *selectedref, float top, float bottom, float x, float y, float x2, float y2) override;
	virtual void menu_activated() override;
	virtual void menu_deactivated() override;

private:
	enum {
		MT_UNDEFINED, // At startup
		MT_NONE,      // [no mapping]
		MT_FULL,      // Full mapping to node
		MT_CHANNEL,   // Channel-to-channel mapping
		MT_INTERNAL   // Go back to previous menu or other non-mapping entry
	};

	enum {
		GRP_GUEST_CHANNEL,
		GRP_NODE,
		GRP_NODE_CHANNEL,
		GRP_DB
	};

	struct select_entry {
		u32 m_maptype;
		speaker_device *m_speaker;
		u32 m_guest_channel;
		u32 m_node;
		u32 m_node_channel;
		float m_db;

		inline bool operator ==(const select_entry &sel) {
			return sel.m_maptype == m_maptype && sel.m_speaker == m_speaker && sel.m_guest_channel == m_guest_channel && sel.m_node == m_node && sel.m_node_channel == m_node_channel;
		}
	};

	uint32_t m_generation;
	select_entry m_current_selection;
	uint32_t m_current_group;
	std::vector<select_entry> m_selections;
	
	virtual void populate() override;
	virtual bool handle(event const *ev) override;

	uint32_t find_node_index(uint32_t node) const;
	std::string find_node_name(uint32_t node) const;
	uint32_t find_node_channel_count(uint32_t node) const;
	uint32_t find_next_target_node_index(uint32_t index) const;
	uint32_t find_previous_target_node_index(uint32_t index) const;
	uint32_t find_first_target_node_index() const;
	uint32_t find_last_target_node_index() const;
	bool full_mapping_available(speaker_device *speaker, uint32_t node) const;
	bool channel_mapping_available(speaker_device *speaker, uint32_t guest_channel, uint32_t node, uint32_t node_channel) const;
	uint32_t find_next_available_target_node(speaker_device *speaker, uint32_t node) const;
	uint32_t find_previous_available_target_node(speaker_device *speaker, uint32_t node) const;
	uint32_t find_next_available_channel_target_node(speaker_device *speaker, uint32_t guest_channel, uint32_t node, uint32_t node_channel) const;
	uint32_t find_previous_available_channel_target_node(speaker_device *speaker, uint32_t guest_channel, uint32_t node, uint32_t node_channel) const;

	static float quantize_db(float db);
	static float inc_db(float db);
	static float dec_db(float db);
};

} // namespace ui

#endif // MAME_FRONTEND_UI_AUDIOMIX_H

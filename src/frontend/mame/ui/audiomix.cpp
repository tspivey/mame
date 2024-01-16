// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/*********************************************************************

    ui/audio_mixer.cpp

    Audio mixing/mapping control

*********************************************************************/

#include "emu.h"
#include "ui/audiomix.h"

#include "ui/ui.h"

#include "osdepend.h"
#include "speaker.h"

namespace ui {

menu_audio_mixer::menu_audio_mixer(mame_ui_manager &mui, render_container &container)
	: menu(mui, container)
{
	set_heading(_("Audio Mixer"));
	m_generation = 0;
	m_current_selection.m_maptype = MT_UNDEFINED;
	m_current_selection.m_speaker = nullptr;
	m_current_selection.m_guest_channel = 0;
	m_current_selection.m_node = 0;
	m_current_selection.m_node_channel = 0;
	m_current_group = GRP_NODE;

	set_process_flags(PROCESS_LR_ALWAYS);
}

menu_audio_mixer::~menu_audio_mixer()
{
}

bool menu_audio_mixer::handle(event const *ev)
{
	if(!ev) {
		if(m_generation != machine().sound().get_osd_info().m_generation) {
			reset(reset_options::REMEMBER_POSITION);
			return true;
		}
		return false;
	}

	switch(ev->iptkey) {
	case IPT_UI_MIXER_ADD_FULL:
		if(m_current_selection.m_maptype == MT_INTERNAL)
			return false;

		if(full_mapping_available(m_current_selection.m_speaker, 0)) {
			m_current_selection.m_node = 0;
			machine().sound().config_add_speaker_target_default(m_current_selection.m_speaker, 0.0);

		} else {
			uint32_t node = find_next_available_target_node(m_current_selection.m_speaker, 0);
			if(node == 0xffffffff)
				return false;
			m_current_selection.m_node = node;
			machine().sound().config_add_speaker_target_node(m_current_selection.m_speaker, find_node_name(node), 0.0);
		}

		m_current_selection.m_maptype = MT_FULL;
		m_current_selection.m_guest_channel = 0;
		m_current_selection.m_node_channel = 0;
		m_current_selection.m_db = 0.0;
		reset(reset_options::REMEMBER_POSITION);
		return true;

	case IPT_UI_MIXER_ADD_CHANNEL: {
		if(m_current_selection.m_maptype == MT_INTERNAL)
			return false;

		// Find a possible triplet, any triplet
		const auto &info = machine().sound().get_osd_info();
		u32 guest_channel;
		u32 node_index, node_id;
		u32 node_channel;
		for(node_index = info.m_default_sink == 0 ? 0 : 0xffffffff; node_index != info.m_nodes.size(); node_index++) {
			node_id = node_index == 0xffffffff ? 0 : info.m_nodes[node_index].m_id;
			u32 guest_channel_count = m_current_selection.m_speaker->outputs();
			u32 node_channel_count = 0;
			if(node_index == 0xffffffff) {
				for(u32 i = 0; i != info.m_nodes.size(); i++)
					if(info.m_nodes[i].m_id == info.m_default_sink) {
						node_channel_count = info.m_nodes[i].m_sinks.size();
						break;
					}
			} else
				node_channel_count = info.m_nodes[node_index].m_sinks.size();

			for(guest_channel = 0; guest_channel != guest_channel_count; guest_channel ++)
				for(node_channel = 0; node_channel != node_channel_count; node_channel ++)
					if(channel_mapping_available(m_current_selection.m_speaker, guest_channel, node_id, node_channel))
						goto found;
		}
		return false;

	found:
		if(node_id)
			machine().sound().config_add_speaker_channel_target_node(m_current_selection.m_speaker, guest_channel, info.m_nodes[node_index].m_name, node_channel, 0.0);
		else
			machine().sound().config_add_speaker_channel_target_default(m_current_selection.m_speaker, guest_channel, node_channel, 0.0);
		m_current_selection.m_maptype = MT_CHANNEL;
		m_current_selection.m_guest_channel = guest_channel;
		m_current_selection.m_node = node_id;
		m_current_selection.m_node_channel = node_channel;
		m_current_selection.m_db = 0.0;
		reset(reset_options::REMEMBER_POSITION);
		return true;
	}

	case IPT_UI_CLEAR: {
		if(m_current_selection.m_maptype == MT_NONE || m_current_selection.m_maptype == MT_INTERNAL)
			return false;

		if(m_current_selection.m_maptype == MT_FULL) {
			if(m_current_selection.m_node == 0)
				machine().sound().config_remove_speaker_target_default(m_current_selection.m_speaker);
			else
				machine().sound().config_remove_speaker_target_node(m_current_selection.m_speaker, find_node_name(m_current_selection.m_node));
		} else {
			if(m_current_selection.m_node == 0)
				machine().sound().config_remove_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel);
			else
				machine().sound().config_remove_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, find_node_name(m_current_selection.m_node), m_current_selection.m_node_channel);
		}

		// Find where the selection was
		uint32_t cursel_index = 0;
		for(uint32_t i = 0; i != m_selections.size(); i++)
			if(m_selections[i] == m_current_selection) {
				cursel_index = i;
				break;
			}

		// If the next item exists and is the same speaker, go there (visually, the cursor stays on the same line)
		// Otherwise if the previous item exists and is the same speaker, go there (visually, the cursor goes up once)
		// Otherwise create a MT_NONE, because one is going to appear at the same place

		if(cursel_index + 1 < m_selections.size() && m_selections[cursel_index+1].m_speaker == m_current_selection.m_speaker)
			m_current_selection = m_selections[cursel_index+1];
		else if(cursel_index != 0 && m_selections[cursel_index-1].m_speaker == m_current_selection.m_speaker)
			m_current_selection = m_selections[cursel_index-1];
		else {
			m_current_selection.m_maptype = MT_NONE;
			m_current_selection.m_guest_channel = 0;
			m_current_selection.m_node = 0;
			m_current_selection.m_node_channel = 0;
			m_current_selection.m_db = 0.0;
		}

		reset(reset_options::REMEMBER_POSITION);
		return true;
	}

	case IPT_UI_UP:
	case IPT_UI_DOWN:
		if(!ev->itemref) {
			m_current_selection.m_maptype = MT_INTERNAL;
			reset(reset_options::REMEMBER_POSITION);
			return true;
		}

		m_current_selection = *(select_entry *)(ev->itemref);
		if(m_current_selection.m_maptype == MT_FULL) {
			if(m_current_group == GRP_GUEST_CHANNEL || m_current_group == GRP_NODE_CHANNEL)
				m_current_group = GRP_NODE;
		}
		reset(reset_options::REMEMBER_POSITION);
		return true;

	case IPT_UI_NEXT_GROUP:
		if(m_current_selection.m_maptype == MT_NONE || m_current_selection.m_maptype == MT_INTERNAL)
			return false;

		if(m_current_selection.m_maptype == MT_FULL) {
			if(m_current_group == GRP_NODE)
				m_current_group = GRP_DB;
			else
				m_current_group = GRP_NODE;

		} else if(m_current_selection.m_maptype == MT_CHANNEL) {
			if(m_current_group == GRP_NODE)
				m_current_group = GRP_NODE_CHANNEL;
			else if(m_current_group == GRP_NODE_CHANNEL)
				m_current_group = GRP_DB;
			else if(m_current_group == GRP_DB)
				m_current_group = GRP_GUEST_CHANNEL;
			else
				m_current_group = GRP_NODE;
		}
		reset(reset_options::REMEMBER_POSITION);
		return true;

	case IPT_UI_PREV_GROUP:
		if(m_current_selection.m_maptype == MT_NONE || m_current_selection.m_maptype == MT_INTERNAL)
			return false;

		if(m_current_selection.m_maptype == MT_FULL) {
			if(m_current_group == GRP_NODE)
				m_current_group = GRP_DB;
			else
				m_current_group = GRP_NODE;

		} else if(m_current_selection.m_maptype == MT_CHANNEL) {
			if(m_current_group == GRP_NODE)
				m_current_group = GRP_GUEST_CHANNEL;
			else if(m_current_group == GRP_GUEST_CHANNEL)
				m_current_group = GRP_DB;
			else if(m_current_group == GRP_DB)
				m_current_group = GRP_NODE_CHANNEL;
			else
				m_current_group = GRP_NODE;
		}
		reset(reset_options::REMEMBER_POSITION);
		return true;

	case IPT_UI_LEFT: {
		if(m_current_selection.m_maptype == MT_NONE || m_current_selection.m_maptype == MT_INTERNAL)
			return false;

		switch(m_current_group) {
		case GRP_NODE: {
			if(m_current_selection.m_maptype == MT_FULL) {
				uint32_t prev_node = m_current_selection.m_node;
				uint32_t next_node = find_previous_available_target_node(m_current_selection.m_speaker, prev_node);
				if(next_node != 0xffffffff) {
					m_current_selection.m_node = next_node;
					if(prev_node)
						machine().sound().config_remove_speaker_target_node(m_current_selection.m_speaker, find_node_name(prev_node));
					else
						machine().sound().config_remove_speaker_target_default(m_current_selection.m_speaker);
					if(next_node)
						machine().sound().config_add_speaker_target_node(m_current_selection.m_speaker, find_node_name(next_node), m_current_selection.m_db);
					else
						machine().sound().config_add_speaker_target_default(m_current_selection.m_speaker, m_current_selection.m_db);
					reset(reset_options::REMEMBER_POSITION);
					return true;			
				}
			} else if(m_current_selection.m_maptype == MT_CHANNEL) {
				uint32_t prev_node = m_current_selection.m_node;
				uint32_t next_node = find_previous_available_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, prev_node, m_current_selection.m_node_channel);
				if(next_node != 0xffffffff) {
					m_current_selection.m_node = next_node;
					if(prev_node)
						machine().sound().config_remove_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, find_node_name(prev_node), m_current_selection.m_node_channel);
					else
						machine().sound().config_remove_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel);
					if(next_node)
						machine().sound().config_add_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, find_node_name(next_node), m_current_selection.m_node_channel, m_current_selection.m_db);
					else
						machine().sound().config_add_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel, m_current_selection.m_db);
					reset(reset_options::REMEMBER_POSITION);
					return true;			
				}
			}
			break;
		}

		case GRP_DB: {
			double db = dec_db(m_current_selection.m_db);
			m_current_selection.m_db = db;
			if(m_current_selection.m_maptype == MT_FULL) {
				if(m_current_selection.m_node == 0)
					machine().sound().config_set_volume_speaker_target_default(m_current_selection.m_speaker, db);
				else
					machine().sound().config_set_volume_speaker_target_node(m_current_selection.m_speaker, find_node_name(m_current_selection.m_node), db);
			} else {
				if(m_current_selection.m_node == 0)
					machine().sound().config_set_volume_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel, db);
				else
					machine().sound().config_set_volume_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, find_node_name(m_current_selection.m_node), m_current_selection.m_node_channel, db);
			}
			reset(reset_options::REMEMBER_POSITION);
			return true;
		}

		case GRP_GUEST_CHANNEL: {
			if(m_current_selection.m_maptype != MT_CHANNEL)
				return false;

			u32 guest_channel_count = m_current_selection.m_speaker->outputs();
			if(guest_channel_count == 1)
				return false;
			u32 guest_channel = m_current_selection.m_guest_channel;
			for(;;) {
				if(guest_channel == 0)
					guest_channel = guest_channel_count - 1;
				else
					guest_channel --;
				if(guest_channel == m_current_selection.m_guest_channel)
					return false;
				if(channel_mapping_available(m_current_selection.m_speaker, guest_channel, m_current_selection.m_node, m_current_selection.m_node_channel)) {
					if(m_current_selection.m_node) {
						std::string node = find_node_name(m_current_selection.m_node);
						machine().sound().config_remove_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_node(m_current_selection.m_speaker, guest_channel, node, m_current_selection.m_node_channel, m_current_selection.m_db);
					} else {
						machine().sound().config_remove_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_default(m_current_selection.m_speaker, guest_channel, m_current_selection.m_node_channel, m_current_selection.m_db);
					}
					m_current_selection.m_guest_channel = guest_channel;
					reset(reset_options::REMEMBER_POSITION);
					return true;
				}
			}
			break;
		}

		case GRP_NODE_CHANNEL: {
			if(m_current_selection.m_maptype != MT_CHANNEL)
				return false;

			u32 node_channel_count = find_node_channel_count(m_current_selection.m_node);
			if(node_channel_count == 1)
				return false;
			u32 node_channel = m_current_selection.m_node_channel;
			for(;;) {
				if(node_channel == 0)
					node_channel = node_channel_count - 1;
				else
					node_channel --;
				if(node_channel == m_current_selection.m_node_channel)
					return false;
				if(channel_mapping_available(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node, node_channel)) {
					if(m_current_selection.m_node) {
						std::string node = find_node_name(m_current_selection.m_node);
						machine().sound().config_remove_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node, node_channel, m_current_selection.m_db);
					} else {
						machine().sound().config_remove_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node_channel, m_current_selection.m_db);
					}
					m_current_selection.m_node_channel = node_channel;
					reset(reset_options::REMEMBER_POSITION);
					return true;
				}
			}
			break;
		}
		}
		break;
	}

	case IPT_UI_RIGHT: {
		if(m_current_selection.m_maptype == MT_NONE || m_current_selection.m_maptype == MT_INTERNAL)
			return false;

		switch(m_current_group) {
		case GRP_NODE: {
			if(m_current_selection.m_maptype == MT_FULL) {
				uint32_t prev_node = m_current_selection.m_node;
				uint32_t next_node = find_next_available_target_node(m_current_selection.m_speaker, prev_node);
				if(next_node != 0xffffffff) {
					m_current_selection.m_node = next_node;
					if(prev_node)
						machine().sound().config_remove_speaker_target_node(m_current_selection.m_speaker, find_node_name(prev_node));
					else
						machine().sound().config_remove_speaker_target_default(m_current_selection.m_speaker);
					if(next_node)
						machine().sound().config_add_speaker_target_node(m_current_selection.m_speaker, find_node_name(next_node), m_current_selection.m_db);
					else
						machine().sound().config_add_speaker_target_default(m_current_selection.m_speaker, m_current_selection.m_db);
					reset(reset_options::REMEMBER_POSITION);
					return true;			
				}
			} else if(m_current_selection.m_maptype == MT_CHANNEL) {
				uint32_t prev_node = m_current_selection.m_node;
				uint32_t next_node = find_next_available_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, prev_node, m_current_selection.m_node_channel);
				if(next_node != 0xffffffff) {
					m_current_selection.m_node = next_node;
					if(prev_node)
						machine().sound().config_remove_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, find_node_name(prev_node), m_current_selection.m_node_channel);
					else
						machine().sound().config_remove_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel);
					if(next_node)
						machine().sound().config_add_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, find_node_name(next_node), m_current_selection.m_node_channel, m_current_selection.m_db);
					else
						machine().sound().config_add_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel, m_current_selection.m_db);
					reset(reset_options::REMEMBER_POSITION);
					return true;			
				}
			}
			break;
		}

		case GRP_DB: {
			double db = inc_db(m_current_selection.m_db);
			m_current_selection.m_db = db;
			if(m_current_selection.m_maptype == MT_FULL) {
				if(m_current_selection.m_node == 0)
					machine().sound().config_set_volume_speaker_target_default(m_current_selection.m_speaker, db);
				else
					machine().sound().config_set_volume_speaker_target_node(m_current_selection.m_speaker, find_node_name(m_current_selection.m_node), db);
			} else {
				if(m_current_selection.m_node == 0)
					machine().sound().config_set_volume_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel, db);
				else
					machine().sound().config_set_volume_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, find_node_name(m_current_selection.m_node), m_current_selection.m_node_channel, db);
			}
			reset(reset_options::REMEMBER_POSITION);
			return true;
		}

		case GRP_GUEST_CHANNEL: {
			if(m_current_selection.m_maptype != MT_CHANNEL)
				return false;

			u32 guest_channel_count = m_current_selection.m_speaker->outputs();
			if(guest_channel_count == 1)
				return false;
			u32 guest_channel = m_current_selection.m_guest_channel;
			for(;;) {
				guest_channel ++;
				if(guest_channel == guest_channel_count)
					guest_channel = 0;
				if(guest_channel == m_current_selection.m_guest_channel)
					return false;
				if(channel_mapping_available(m_current_selection.m_speaker, guest_channel, m_current_selection.m_node, m_current_selection.m_node_channel)) {
					if(m_current_selection.m_node) {
						std::string node = find_node_name(m_current_selection.m_node);
						machine().sound().config_remove_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_node(m_current_selection.m_speaker, guest_channel, node, m_current_selection.m_node_channel, m_current_selection.m_db);
					} else {
						machine().sound().config_remove_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_default(m_current_selection.m_speaker, guest_channel, m_current_selection.m_node_channel, m_current_selection.m_db);
					}
					m_current_selection.m_guest_channel = guest_channel;
					reset(reset_options::REMEMBER_POSITION);
					return true;
				}
			}
			break;
		}

		case GRP_NODE_CHANNEL: {
			if(m_current_selection.m_maptype != MT_CHANNEL)
				return false;

			u32 node_channel_count = find_node_channel_count(m_current_selection.m_node);
			if(node_channel_count == 1)
				return false;
			u32 node_channel = m_current_selection.m_node_channel;
			for(;;) {
				node_channel ++;
				if(node_channel == node_channel_count)
					node_channel = 0;
				if(node_channel == m_current_selection.m_node_channel)
					return false;
				if(channel_mapping_available(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node, node_channel)) {
					if(m_current_selection.m_node) {
						std::string node = find_node_name(m_current_selection.m_node);
						machine().sound().config_remove_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_node(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node, node_channel, m_current_selection.m_db);
					} else {
						machine().sound().config_remove_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, m_current_selection.m_node_channel);
						machine().sound().config_add_speaker_channel_target_default(m_current_selection.m_speaker, m_current_selection.m_guest_channel, node_channel, m_current_selection.m_db);
					}
					m_current_selection.m_node_channel = node_channel;
					reset(reset_options::REMEMBER_POSITION);
					return true;
				}
			}
			break;
		}
		}
		break;		
	}
	}

	return false;
}


//-------------------------------------------------
//  menu_audio_mixer_populate - populate the audio_mixer
//  menu
//-------------------------------------------------

void menu_audio_mixer::populate()
{
	const auto &mapping = machine().sound().get_mappings();
	const auto &info = machine().sound().get_osd_info();
	m_generation = info.m_generation;

	auto find_node = [&info](u32 node_id) -> const osd::audio_info::node_info * {
		for(const auto &node : info.m_nodes)
			if(node.m_id == node_id)
				return &node;
		// Never happens
		return nullptr;
	};

	// Rebuild the selections list
	m_selections.clear();
	for(const auto &omap : mapping) {
		for(const auto &nmap : omap.m_node_mappings)
			m_selections.emplace_back(select_entry { MT_FULL, omap.m_speaker, 0, nmap.m_is_system_default ? 0 : nmap.m_node, 0, nmap.m_db });
		for(const auto &cmap : omap.m_channel_mappings)
			m_selections.emplace_back(select_entry { MT_CHANNEL, omap.m_speaker, cmap.m_guest_channel, cmap.m_is_system_default ? 0 : cmap.m_node, cmap.m_node_channel, cmap.m_db });
		if(omap.m_node_mappings.empty() && omap.m_channel_mappings.empty())
			m_selections.emplace_back(select_entry { MT_NONE, omap.m_speaker, 0, 0, 0, 0 });
	}

	// If there's nothing, get out of there
	if(m_selections.empty())
		return;

	// Find the line of the current selection, if any.
	// Otherwise default to the first line

	u32 cursel_line = 0;
	for(u32 i = 0; i != m_selections.size(); i++)
		if(m_current_selection == m_selections[i]) {
			cursel_line = i;
			break;
		}

	if(m_current_selection.m_maptype == MT_INTERNAL)
		cursel_line = 0xffffffff;
	else
		m_current_selection = m_selections[cursel_line];

	if(m_current_selection.m_maptype == MT_FULL) {
		if(m_current_group == GRP_GUEST_CHANNEL || m_current_group == GRP_NODE_CHANNEL)
			m_current_group = GRP_NODE;
	}

	// (Re)build the menu
	u32 curline = 0;
	for(const auto &omap : mapping) {
		item_append(omap.m_speaker->tag(), FLAG_UI_HEADING | FLAG_DISABLE, nullptr);
		for(const auto &nmap : omap.m_node_mappings) {
			const auto &node = find_node(nmap.m_node);
			std::string lnode = nmap.m_is_system_default ? "[default]" : node->m_name;
			if(curline == cursel_line && m_current_group == GRP_NODE)
				lnode = "\xe2\x86\x90" + lnode + "\xe2\x86\x92";

			std::string line = "> " + lnode;

			std::string db = util::string_format("%g dB", nmap.m_db);
			if(curline == cursel_line && m_current_group == GRP_DB)
				db = "\xe2\x86\x90" + db + "\xe2\x86\x92";
			
			item_append(line, db, 0, m_selections.data() + curline);
			curline ++;
		}
		for(const auto &cmap : omap.m_channel_mappings) {
			const auto &node = find_node(cmap.m_node);
			std::string guest_channel = omap.m_speaker->get_position_name(cmap.m_guest_channel);
			if(curline == cursel_line && m_current_group == GRP_GUEST_CHANNEL)
				guest_channel = "\xe2\x86\x90" + guest_channel + "\xe2\x86\x92";

			std::string lnode = cmap.m_is_system_default ? "[default]" : node->m_name;
			if(curline == cursel_line && m_current_group == GRP_NODE)
				lnode = "\xe2\x86\x90" + lnode + "\xe2\x86\x92";

			std::string lnode_channel = node->m_sinks[cmap.m_node_channel].m_name;
			if(curline == cursel_line && m_current_group == GRP_NODE_CHANNEL)
				lnode_channel = "\xe2\x86\x90" + lnode_channel + "\xe2\x86\x92";

			std::string line = guest_channel + " > " + lnode + ":" + lnode_channel;

			std::string db = util::string_format("%g dB", cmap.m_db);
			if(curline == cursel_line && m_current_group == GRP_DB)
				db = "\xe2\x86\x90" + db + "\xe2\x86\x92";
			
			item_append(line, db, 0, m_selections.data() + curline);
			curline ++;
		}
		if(omap.m_node_mappings.empty() && omap.m_channel_mappings.empty()) {
			item_append("[no mapping]", 0, m_selections.data() + curline);
			curline ++;
		}
	}
	item_append(menu_item_type::SEPARATOR);
	item_append(util::string_format("%s: add a full mapping", ui().get_general_input_setting(IPT_UI_MIXER_ADD_FULL)), FLAG_DISABLE, nullptr);
	item_append(util::string_format("%s: add a channel mapping", ui().get_general_input_setting(IPT_UI_MIXER_ADD_CHANNEL)), FLAG_DISABLE, nullptr);
	item_append(util::string_format("%s: remove a mapping", ui().get_general_input_setting(IPT_UI_CLEAR)), FLAG_DISABLE, nullptr);
	item_append(menu_item_type::SEPARATOR);

	if(cursel_line != 0xffffffff)
		set_selection(m_selections.data() + cursel_line);
}


//-------------------------------------------------
//  recompute_metrics - recompute metrics
//-------------------------------------------------

void menu_audio_mixer::recompute_metrics(uint32_t width, uint32_t height, float aspect)
{
	menu::recompute_metrics(width, height, aspect);

	//	set_custom_space(0.0f, 2.0f * line_height() + 2.0f * tb_border());
}


//-------------------------------------------------
//  menu_audio_mixer_custom_render - perform our special
//  rendering
//-------------------------------------------------

void menu_audio_mixer::custom_render(void *selectedref, float top, float bottom, float x1, float y1, float x2, float y2)
{
}


//-------------------------------------------------
//  menu_activated - handle menu gaining focus
//-------------------------------------------------

void menu_audio_mixer::menu_activated()
{
	// scripts or the other form of the menu could have changed something in the mean time
	reset(reset_options::REMEMBER_POSITION);
}


//-------------------------------------------------
//  menu_deactivated - handle menu losing focus
//-------------------------------------------------

void menu_audio_mixer::menu_deactivated()
{
}

uint32_t menu_audio_mixer::find_node_index(uint32_t node) const
{
	const auto &info = machine().sound().get_osd_info();
	for(uint32_t i = 0; i != info.m_nodes.size(); i++)
		if(info.m_nodes[i].m_id == node)
			return i;
	// Can't happen in theory
	return 0xffffffff;
}

std::string menu_audio_mixer::find_node_name(uint32_t node) const
{
	const auto &info = machine().sound().get_osd_info();
	for(uint32_t i = 0; i != info.m_nodes.size(); i++)
		if(info.m_nodes[i].m_id == node)
			return info.m_nodes[i].m_name;
	// Can't happen in theory
	return "";
}

uint32_t menu_audio_mixer::find_node_channel_count(uint32_t node) const
{
	const auto &info = machine().sound().get_osd_info();
	if(!node)
		node = info.m_default_sink;
	for(uint32_t i = 0; i != info.m_nodes.size(); i++)
		if(info.m_nodes[i].m_id == node)
			return info.m_nodes[i].m_sinks.size();
	// Can't happen in theory
	return 0;
}


uint32_t menu_audio_mixer::find_next_target_node_index(uint32_t index) const
{
	if(index == 0xffffffff)
		return index;

	const auto &info = machine().sound().get_osd_info();
	for(uint32_t idx = index + 1; idx != info.m_nodes.size(); idx++)
		if(!info.m_nodes[idx].m_sinks.empty())
			return idx;
	return 0xffffffff;
}

uint32_t menu_audio_mixer::find_previous_target_node_index(uint32_t index) const
{
	if(index == 0xffffffff)
		return index;

	const auto &info = machine().sound().get_osd_info();
	for(uint32_t idx = index - 1; idx != 0xffffffff; idx--)
		if(!info.m_nodes[idx].m_sinks.empty())
			return idx;
	return 0xffffffff;
}

uint32_t menu_audio_mixer::find_first_target_node_index() const
{
	const auto &info = machine().sound().get_osd_info();
	for(uint32_t index = 0; index != info.m_nodes.size(); index ++)
		if(!info.m_nodes[index].m_sinks.empty())
			return index;
	return 0xffffffff;
}

uint32_t menu_audio_mixer::find_last_target_node_index() const
{
	const auto &info = machine().sound().get_osd_info();
	for(uint32_t index = info.m_nodes.size() - 1; index != 0xffffffff; index --)
		if(!info.m_nodes[index].m_sinks.empty())
			return index;
	return 0xffffffff;
}

bool menu_audio_mixer::full_mapping_available(speaker_device *speaker, uint32_t node) const
{
	if(!node && machine().sound().get_osd_info().m_default_sink == 0)
		return false;

	const auto &mapping = machine().sound().get_mappings();
	for(const auto &omap : mapping)
		if(omap.m_speaker == speaker) {
			for(const auto &nmap : omap.m_node_mappings)
				if((node != 0 && nmap.m_node == node && !nmap.m_is_system_default) || (node == 0 && nmap.m_is_system_default))
					return false;
			return true;
		}
	return true;
}

bool menu_audio_mixer::channel_mapping_available(speaker_device *speaker, uint32_t guest_channel, uint32_t node, uint32_t node_channel) const
{
	if(!node && machine().sound().get_osd_info().m_default_sink == 0)
		return false;

	const auto &mapping = machine().sound().get_mappings();
	for(const auto &omap : mapping)
		if(omap.m_speaker == speaker) {
			for(const auto &cmap : omap.m_channel_mappings)
				if(cmap.m_guest_channel == guest_channel &&
				   ((node != 0 && cmap.m_node == node && !cmap.m_is_system_default) || (node == 0 && cmap.m_is_system_default))
				   && cmap.m_node_channel == node_channel)
					return false;
			return true;
		}
	return true;
}

uint32_t menu_audio_mixer::find_next_available_target_node(speaker_device *speaker, uint32_t node) const
{
	const auto &info = machine().sound().get_osd_info();

	if(node == 0) {
		uint32_t index = find_first_target_node_index();
		while(index != 0xffffffff && !full_mapping_available(speaker, info.m_nodes[index].m_id))
			index = find_next_target_node_index(index);
		return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
	}

	uint32_t index = find_node_index(node);
	while(index != 0xffffffff) {
		index = find_next_target_node_index(index);
		if(index != 0xffffffff && full_mapping_available(speaker, info.m_nodes[index].m_id))
			return info.m_nodes[index].m_id;
	}

	if(info.m_default_sink != 0 && full_mapping_available(speaker, 0))
		return 0;

	index = find_first_target_node_index();
	while(index != 0xffffffff && !full_mapping_available(speaker, info.m_nodes[index].m_id))
		index = find_next_target_node_index(index);
	return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
}

uint32_t menu_audio_mixer::find_previous_available_target_node(speaker_device *speaker, uint32_t node) const
{
	const auto &info = machine().sound().get_osd_info();

	if(node == 0) {
		uint32_t index = find_last_target_node_index();
		while(index != 0xffffffff && !full_mapping_available(speaker, info.m_nodes[index].m_id))
			index = find_previous_target_node_index(index);
		return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
	}

	uint32_t index = find_node_index(node);
	while(index != 0xffffffff) {
		index = find_previous_target_node_index(index);
		if(index != 0xffffffff && full_mapping_available(speaker, info.m_nodes[index].m_id))
			return info.m_nodes[index].m_id;
	}

	if(info.m_default_sink != 0 && full_mapping_available(speaker, 0))
		return 0;

	index = find_last_target_node_index();
	while(index != 0xffffffff && !full_mapping_available(speaker, info.m_nodes[index].m_id))
		index = find_previous_target_node_index(index);
	return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
}

uint32_t menu_audio_mixer::find_next_available_channel_target_node(speaker_device *speaker, uint32_t guest_channel, uint32_t node, uint32_t node_channel) const
{
	const auto &info = machine().sound().get_osd_info();

	if(node == 0) {
		uint32_t index = find_first_target_node_index();
		while(index != 0xffffffff && !channel_mapping_available(speaker, guest_channel, info.m_nodes[index].m_id, node_channel))
			index = find_next_target_node_index(index);
		return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
	}

	uint32_t index = find_node_index(node);
	while(index != 0xffffffff) {
		index = find_next_target_node_index(index);
		if(index != 0xffffffff && channel_mapping_available(speaker, guest_channel, info.m_nodes[index].m_id, node_channel))
			return info.m_nodes[index].m_id;
	}

	if(info.m_default_sink != 0 && channel_mapping_available(speaker, guest_channel, 0, node_channel))
		return 0;

	index = find_first_target_node_index();
	while(index != 0xffffffff && !channel_mapping_available(speaker, guest_channel, info.m_nodes[index].m_id, node_channel))
		index = find_next_target_node_index(index);
	return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
}

uint32_t menu_audio_mixer::find_previous_available_channel_target_node(speaker_device *speaker, uint32_t guest_channel, uint32_t node, uint32_t node_channel) const
{
	const auto &info = machine().sound().get_osd_info();

	if(node == 0) {
		uint32_t index = find_last_target_node_index();
		while(index != 0xffffffff && !channel_mapping_available(speaker, guest_channel, info.m_nodes[index].m_id, node_channel))
			index = find_previous_target_node_index(index);
		return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
	}

	uint32_t index = find_node_index(node);
	while(index != 0xffffffff) {
		index = find_previous_target_node_index(index);
		if(index != 0xffffffff && channel_mapping_available(speaker, guest_channel, info.m_nodes[index].m_id, node_channel))
			return info.m_nodes[index].m_id;
	}

	if(info.m_default_sink != 0 && channel_mapping_available(speaker, guest_channel, 0, node_channel))
		return 0;

	index = find_last_target_node_index();
	while(index != 0xffffffff && !channel_mapping_available(speaker, guest_channel, info.m_nodes[index].m_id, node_channel))
		index = find_previous_target_node_index(index);
	return index == 0xffffffff ? 0xffffffff : info.m_nodes[index].m_id;
}

float menu_audio_mixer::quantize_db(float db)
{
	if(db >= 12.0)
		return 12.0;
	if(db >= -12.0)
		return floor(db*2 + 0.5) / 2;
	if(db >= -24.0)
		return floor(db + 0.5);
	if(db >= -48.0)
		return floor(db/2 + 0.5) * 2;
	if(db >= -96.0)
		return floor(db/4 + 0.5) * 4;
	return -96.0;
}

float menu_audio_mixer::inc_db(float db)
{
	db = quantize_db(db);
	if(db >= 12)
		return 12.0;
	if(db >= -12.0)
		return db + 0.5;
	if(db >= -24.0)
		return db + 1;
	if(db >= -48.0)
		return db + 2;
	if(db >= -96.0 + 4)
		return db + 4;
	return -96.0 + 4;
	
}

float menu_audio_mixer::dec_db(float db)
{
	db = quantize_db(db);
	if(db >= 12.5)
		return 11.5;
	if(db > -12.0)
		return db - 0.5;
	if(db > -24.0)
		return db - 1;
	if(db > -48.0)
		return db - 2;
	if(db >= -92.0)
		return db - 4;
	return -96.0;	
}

} // namespace ui


// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    piepewire_sound.c

    PipeWire interface.

***************************************************************************/

#include "sound_module.h"
#include "modules/osdmodule.h"

#ifndef NO_USE_PIPEWIRE

#define GNU_SOURCE
#include "modules/lib/osdobj_common.h"

#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <spa/debug/pod.h>
#include <spa/debug/dict.h>
#include <spa/pod/builder.h>
#include <spa/param/audio/raw-utils.h>
#include <rapidjson/document.h>

#include <map>

class sound_pipewire : public osd_module, public sound_module
{
public:
	sound_pipewire()
		: osd_module(OSD_SOUND_PROVIDER, "pipewire"), sound_module()
	{
	}
	virtual ~sound_pipewire() { }

	virtual int init(osd_interface &osd, osd_options const &options) override;
	virtual void exit() override;

	virtual bool external_per_channel_volume() override { return true; }
	virtual bool split_streams_per_source() override { return true; }

	virtual uint32_t get_generation() override;
	virtual osd::audio_info get_information() override;
	virtual uint32_t stream_sink_open(uint32_t node, std::string name, uint32_t rate) override;
	virtual void stream_set_volumes(uint32_t id, const std::vector<float> &db) override;
	virtual void stream_close(uint32_t id) override;
	virtual void stream_update(uint32_t id, const int16_t *buffer, int samples_this_frame) override;

private:
	struct position_info {
		uint32_t m_position;
		std::array<double, 3> m_coords;
	};

	struct abuffer {
		uint32_t m_cpos;
		std::vector<int16_t> m_data;
	};

	static const position_info position_infos[];

	static const char *const typenames[];
	enum { AREC, APLAY };

	struct node_info {
		sound_pipewire *m_wire;
		uint32_t m_id, m_osdid;
		int m_type;
		std::string m_serial;
		std::string m_name;
		std::string m_text_id;

		// Audio node info
		std::vector<uint32_t> m_position_codes;
		std::vector<std::array<double, 3>> m_positions;
		std::vector<std::string> m_sink_port_names;
		std::vector<std::string> m_source_port_names;

		osd::audio_rate_range m_rate;
		bool m_has_s16;
		bool m_has_iec958;

		pw_node *m_node;
		spa_hook m_node_listener;

		node_info(sound_pipewire *wire, uint32_t id, uint32_t osdid, int type, std::string serial, std::string name, std::string text_id) : m_wire(wire), m_id(id), m_osdid(osdid), m_type(type), m_serial(serial), m_name(name), m_text_id(text_id), m_rate{0, 0, 0}, m_has_s16(false), m_has_iec958(false), m_node(nullptr) {
			spa_zero(m_node_listener);
		}
	};
	
	struct stream_info {
		sound_pipewire *m_wire;
		uint32_t m_osdid;
		uint32_t m_source_node_id;
		node_info *m_target_node;
		uint32_t m_channels;
		pw_stream *m_stream;
		std::vector<float> m_volumes;
		std::vector<abuffer> m_buffers;
		std::vector<int16_t> m_last_sample;

		stream_info(sound_pipewire *wire, uint32_t osdid) : m_wire(wire), m_osdid(osdid), m_stream(nullptr) {}
	};
	
	static const pw_core_events     core_events;
	static const pw_registry_events registry_events;
	static const pw_node_events     node_events;
	static const pw_metadata_events default_events;
	static const pw_stream_events   stream_events;

	std::map<uint32_t, node_info> m_nodes;
	std::map<uint32_t, uint32_t> m_node_osdid_to_id;

	std::map<uint32_t, stream_info> m_streams;

	pw_thread_loop *m_loop;
	pw_context *m_context;
	pw_core *m_core;
	spa_hook m_core_listener;
	pw_registry *m_registry;
	spa_hook m_registry_listener;
	pw_metadata *m_default;
	spa_hook m_default_listener;

	std::string m_default_audio_sink;
	std::string m_default_audio_source;

	uint32_t m_node_current_id, m_stream_current_id;
	uint32_t m_generation;
	bool m_wait_sync, m_wait_stream;

	void sync();

	void core_event_done(uint32_t id, int seq);
	static void s_core_event_done(void *data, uint32_t id, int seq);

	void register_node(uint32_t id, const spa_dict *props);
	void register_port(uint32_t id, const spa_dict *props);
	void register_link(uint32_t id, const spa_dict *props);
	void register_default_metadata(uint32_t id);
	void register_metadata(uint32_t id, const spa_dict *props);
	void registry_event_global(uint32_t id, uint32_t permissions, const char *type, uint32_t version, const spa_dict *props);
	static void s_registry_event_global(void *data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const spa_dict *props);

	void registry_event_global_remove(uint32_t id);
	static void s_registry_event_global_remove(void *data, uint32_t id);

	void node_event_param(node_info *node, int seq, uint32_t id, uint32_t index, uint32_t next, const spa_pod *param);
	static void s_node_event_param(void *data, int seq, uint32_t id, uint32_t index, uint32_t next, const spa_pod *param);

	int default_event_property(uint32_t subject, const char *key, const char *type, const char *value);
	static int s_default_event_property(void *data, uint32_t subject, const char *key, const char *type, const char *value);

	void stream_event_process(stream_info *stream);
	static void s_stream_event_process(void *data);

	void stream_event_param_changed(stream_info *stream, uint32_t id, const spa_pod *param);
	static void s_stream_event_param_changed(void *data, uint32_t id, const spa_pod *param);
};

// Try to more or less map to speaker.h positions

const sound_pipewire::position_info sound_pipewire::position_infos[] = {
	{ SPA_AUDIO_CHANNEL_MONO,    {  0.0,  0.0,  1.0 } },
	{ SPA_AUDIO_CHANNEL_FL,      { -0.2,  0.0,  1.0 } },
	{ SPA_AUDIO_CHANNEL_FR,      {  0.2,  0.0,  1.0 } },
	{ SPA_AUDIO_CHANNEL_FC,      {  0.0,  0.0,  1.0 } },
	{ SPA_AUDIO_CHANNEL_LFE,     {  0.0, -0.5,  1.0 } },
	{ SPA_AUDIO_CHANNEL_RL,      { -0.2,  0.0, -0.5 } },
	{ SPA_AUDIO_CHANNEL_RR,      {  0.2,  0.0, -0.5 } },
	{ SPA_AUDIO_CHANNEL_RC,      {  0.0,  0.0, -0.5 } },
	{ SPA_AUDIO_CHANNEL_UNKNOWN, {  0.0,  0.0,  0.0 } }
};


const char *const sound_pipewire::typenames[] = {
	"Audio recorder", "Speaker"
};

const pw_core_events sound_pipewire::core_events = {
	PW_VERSION_CORE_EVENTS,
	nullptr, // info
	s_core_event_done,
	nullptr, // ping
	nullptr, // error
	nullptr, // remove_id
	nullptr, // bound_id
	nullptr, // add_mem
	nullptr, // remove_mem
	nullptr  // bound_props
};

const pw_registry_events sound_pipewire::registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	s_registry_event_global,
	s_registry_event_global_remove
};

const pw_node_events sound_pipewire::node_events = {
	PW_VERSION_NODE_EVENTS,
	nullptr, // info
	s_node_event_param
};

const pw_metadata_events sound_pipewire::default_events = {
	PW_VERSION_METADATA_EVENTS,
	s_default_event_property
};

const pw_stream_events sound_pipewire::stream_events = {
	PW_VERSION_STREAM_EVENTS,
	nullptr, // destroy
	nullptr, // state changed
	nullptr, // control info
	nullptr, // io changed
	s_stream_event_param_changed,
	nullptr, // add buffer
	nullptr, // remove buffer
	s_stream_event_process,
	nullptr, // drained
	nullptr, // command
	nullptr  // trigger done
};

void sound_pipewire::register_node(uint32_t id, const spa_dict *props)
{
	const spa_dict_item *cls = spa_dict_lookup_item(props, PW_KEY_MEDIA_CLASS);
	const spa_dict_item *desc = spa_dict_lookup_item(props, PW_KEY_NODE_DESCRIPTION);
	const spa_dict_item *name = spa_dict_lookup_item(props, PW_KEY_NODE_NAME);
	const spa_dict_item *serial = spa_dict_lookup_item(props, PW_KEY_OBJECT_SERIAL);
	if(!cls)
		return;
	int type;
	if(!strcmp(cls->value, "Audio/Source"))
		type = AREC;
	else if(!strcmp(cls->value, "Audio/Sink"))
		type = APLAY;
	else
		return;

	m_node_osdid_to_id[m_node_current_id] = id;
	auto &node = m_nodes.emplace(id, node_info(this, id, m_node_current_id++, type, serial->value, desc ? desc->value : "?", name ? name->value : "?")).first->second;

	//	printf("node %03x: %s %s %s | %s\n", node.m_id, serial->value, typenames[node.m_type], node.m_name.c_str(), node.m_text_id.c_str());

	node.m_node = (pw_node *)pw_registry_bind(m_registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0);
	pw_node_add_listener(node.m_node, &node.m_node_listener, &node_events, &node);
	pw_node_enum_params(node.m_node, 0, 3, 0, 0xffffffff, nullptr);
	m_generation++;
}

void sound_pipewire::register_port(uint32_t id, const spa_dict *props)
{
	uint32_t node = strtol(spa_dict_lookup_item(props, PW_KEY_NODE_ID)->value, nullptr, 10);
	auto ind = m_nodes.find(node);
	if(ind == m_nodes.end())
		return;

	const spa_dict_item *channel = spa_dict_lookup_item(props, PW_KEY_AUDIO_CHANNEL);
	const spa_dict_item *dir = spa_dict_lookup_item(props, PW_KEY_PORT_DIRECTION);
	uint32_t index = strtol(spa_dict_lookup_item(props, PW_KEY_PORT_ID)->value, nullptr, 10);

	auto &port_names = dir && !strcmp(dir->value, "in") ? ind->second.m_sink_port_names : ind->second.m_source_port_names;
	if(port_names.size() <= index)
		port_names.resize(index+1);

	port_names[index] = channel ? channel->value : "?";

	m_generation++;
	//	printf("port %03x.%d %03x: %s\n", node, index, id, port_names[index].c_str());
}

void sound_pipewire::register_link(uint32_t id, const spa_dict *props)
{
	const spa_dict_item *input  = spa_dict_lookup_item(props, PW_KEY_LINK_INPUT_NODE);
	const spa_dict_item *output = spa_dict_lookup_item(props, PW_KEY_LINK_OUTPUT_NODE);
	if(!input || !output)
		return;

	uint32_t input_id = strtol(input->value, nullptr, 10);
	uint32_t output_id = strtol(output->value, nullptr, 10);

	for(auto &si : m_streams) {
		stream_info &stream = si.second;
		if(stream.m_source_node_id == output_id && (stream.m_target_node && stream.m_target_node->m_id != input_id)) {
			auto ni = m_nodes.find(input_id);
			if(ni != m_nodes.end()) {
				stream.m_target_node = &ni->second;
				m_generation ++;
			}
		}
	}
}

void sound_pipewire::register_default_metadata(uint32_t id)
{
	m_default = (pw_metadata *)pw_registry_bind(m_registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0);
	pw_metadata_add_listener(m_default, &m_default_listener, &default_events, this);	
}

void sound_pipewire::register_metadata(uint32_t id, const spa_dict *props)
{
	const spa_dict_item *mn = spa_dict_lookup_item(props, PW_KEY_METADATA_NAME);
	if(mn && !strcmp(mn->value, "default"))
		register_default_metadata(id);
}

void sound_pipewire::registry_event_global(uint32_t id,
									 uint32_t permissions, const char *type, uint32_t version,
									 const spa_dict *props)
{
	if(!strcmp(type, PW_TYPE_INTERFACE_Node))
		register_node(id, props);
	else if(!strcmp(type, PW_TYPE_INTERFACE_Port))
		register_port(id, props);
	else if(!strcmp(type, PW_TYPE_INTERFACE_Metadata))
		register_metadata(id, props);
	else if(!strcmp(type, PW_TYPE_INTERFACE_Link))
		register_link(id, props);
	else {
		//		printf("type %03x %s\n", id, type);
	}
}

void sound_pipewire::s_registry_event_global(void *data, uint32_t id,
											uint32_t permissions, const char *type, uint32_t version,
											const spa_dict *props)
{
	((sound_pipewire *)data)->registry_event_global(id, permissions, type, version, props);
}

void sound_pipewire::registry_event_global_remove(uint32_t id)
{
	auto ind = m_nodes.find(id);
	if(ind == m_nodes.end())
		return;

	for(auto &istream : m_streams)
		if(istream.second.m_target_node == &ind->second)
			istream.second.m_target_node = nullptr;
	m_nodes.erase(ind);
	m_generation++;
}

void sound_pipewire::s_registry_event_global_remove(void *data, uint32_t id)
{
	((sound_pipewire *)data)->registry_event_global_remove(id);
}

void sound_pipewire::node_event_param(node_info *node, int seq, uint32_t id, uint32_t index, uint32_t next, const spa_pod *param)
{
	if(id == SPA_PARAM_EnumFormat) {
		const spa_pod_prop *subtype  = spa_pod_find_prop(param, nullptr, SPA_FORMAT_mediaSubtype);
		if(subtype) {
			uint32_t sval;
			if(!spa_pod_get_id(&subtype->value, &sval)) {
				if(sval == SPA_MEDIA_SUBTYPE_raw) {
					const spa_pod_prop *format   = spa_pod_find_prop(param, nullptr, SPA_FORMAT_AUDIO_format);
					const spa_pod_prop *rate     = spa_pod_find_prop(param, nullptr, SPA_FORMAT_AUDIO_rate);
					const spa_pod_prop *position = spa_pod_find_prop(param, nullptr, SPA_FORMAT_AUDIO_position);

					if(format) {
						uint32_t *entry;
						SPA_POD_CHOICE_FOREACH((spa_pod_choice *)(&format->value), entry) {
							if(*entry == SPA_AUDIO_FORMAT_S16)
								node->m_has_s16 = true;
						}
					}
					if(rate) {
						if(rate->value.type == SPA_TYPE_Choice) {
							struct spa_pod_choice_body *b = &((spa_pod_choice *)(&rate->value))->body;
							uint32_t *choices = (uint32_t *)(b+1);
							node->m_rate.m_default_rate = choices[0];
							if(b->type == SPA_CHOICE_Range) {
								node->m_rate.m_min_rate = choices[1];
								node->m_rate.m_max_rate = choices[2];
							} else {
								node->m_rate.m_min_rate = node->m_rate.m_default_rate;
								node->m_rate.m_max_rate = node->m_rate.m_default_rate;
							}
						}
					}

					if(position) {
						uint32_t *entry;
						node->m_position_codes.clear();
						node->m_positions.clear();
						SPA_POD_ARRAY_FOREACH((spa_pod_array *)(&position->value), entry) {
							node->m_position_codes.push_back(*entry);
							for(uint32_t i = 0;; i++) {
								if((position_infos[i].m_position == *entry) || (position_infos[i].m_position == SPA_AUDIO_CHANNEL_UNKNOWN)) {
									node->m_positions.push_back(position_infos[i].m_coords);
									break;
								}
							}
						}
					}
				} else if(sval == SPA_MEDIA_SUBTYPE_iec958)
					node->m_has_iec958 = true;
			}
		}
		m_generation++;
		
	} else
		spa_debug_pod(2, nullptr, param);
}

void sound_pipewire::s_node_event_param(void *data, int seq, uint32_t id, uint32_t index, uint32_t next, const spa_pod *param)
{
	node_info *n = (node_info *)data;
	n->m_wire->node_event_param(n, seq, id, index, next, param);
}

int sound_pipewire::default_event_property(uint32_t subject, const char *key, const char *type, const char *value)
{
	if(!value)
		return 0;
	std::string val = value;
	if(!strcmp(type, "Spa:String:JSON")) {
		rapidjson::Document json;
		json.Parse(value);
		if(json.IsObject() && json.HasMember("name") && json["name"].IsString())
			val = json["name"].GetString();
	} else
		val = value;
	   
	if(!strcmp(key, "default.audio.sink"))
		m_default_audio_sink = val;

	else if(!strcmp(key, "default.audio.source"))
		m_default_audio_source = val;

	return 0;
}

int sound_pipewire::s_default_event_property(void *data, uint32_t subject, const char *key, const char *type, const char *value)
{
	return ((sound_pipewire *)data)->default_event_property(subject, key, type, value);
}

int sound_pipewire::init(osd_interface &osd, osd_options const &options)
{
	spa_zero(m_core_listener);
	spa_zero(m_registry_listener);
	spa_zero(m_default_listener);

	m_node_current_id = 1;
	m_stream_current_id = 1;
	m_generation = 1;

	m_wait_sync = false;
	m_wait_stream = false;

	pw_init(nullptr, nullptr);
 	m_loop = pw_thread_loop_new(nullptr, nullptr);
	m_context = pw_context_new(pw_thread_loop_get_loop(m_loop), nullptr, 0);
	m_core = pw_context_connect(m_context, nullptr, 0);

	if(!m_core)
		return 1;

	pw_core_add_listener(m_core, &m_core_listener, &core_events, this);

	m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
	pw_registry_add_listener(m_registry, &m_registry_listener, &registry_events, this);

	pw_thread_loop_start(m_loop);

	// The first sync ensures that the initial information request is
	// completed, the second that the subsequent ones (parameters
	// retrieval) are completed too.
	sync();
	sync();

	return 0;
}

void sound_pipewire::core_event_done(uint32_t id, int seq)
{
	m_wait_sync = false;
	pw_thread_loop_signal(m_loop, false);
}

void sound_pipewire::s_core_event_done(void *data, uint32_t id, int seq)
{
	((sound_pipewire *)data)->core_event_done(id, seq);
}

void sound_pipewire::sync()
{
	pw_thread_loop_lock(m_loop);
	m_wait_sync = true;
	pw_core_sync(m_core, PW_ID_CORE, 0);	
	while(m_wait_sync)
		pw_thread_loop_wait(m_loop);
	pw_thread_loop_unlock(m_loop);
}

void sound_pipewire::exit()
{
	pw_thread_loop_lock(m_loop);
	for(const auto &si : m_streams)
		pw_stream_destroy(si.second.m_stream);
	for(const auto &ni : m_nodes)
		pw_proxy_destroy((pw_proxy *)ni.second.m_node);
	pw_proxy_destroy((pw_proxy *)m_default);
	pw_proxy_destroy((pw_proxy *)m_registry);
	pw_thread_loop_unlock(m_loop);
	pw_core_disconnect(m_core);
	pw_context_destroy(m_context);
	pw_thread_loop_stop(m_loop);
	pw_thread_loop_destroy(m_loop);
	pw_deinit();
}

uint32_t sound_pipewire::get_generation()
{
	pw_thread_loop_lock(m_loop);
	uint32_t result = m_generation;
	pw_thread_loop_unlock(m_loop);
	return result;
}

osd::audio_info sound_pipewire::get_information()
{
	osd::audio_info result;
	pw_thread_loop_lock(m_loop);
	result.m_nodes.resize(m_nodes.size());
	result.m_default_sink = 0;
	result.m_default_source = 0;
	result.m_generation = m_generation;
	uint32_t node = 0;
	for(auto &inode : m_nodes) {
		result.m_nodes[node].m_name = inode.second.m_name;
		result.m_nodes[node].m_id = inode.second.m_osdid;
		result.m_nodes[node].m_rate = inode.second.m_rate;
		result.m_nodes[node].m_sinks.resize(inode.second.m_sink_port_names.size());

		for(uint32_t port = 0; port != inode.second.m_sink_port_names.size(); port++) {
			result.m_nodes[node].m_sinks[port].m_name = inode.second.m_sink_port_names[port];
			result.m_nodes[node].m_sinks[port].m_position = inode.second.m_positions[port];
		}

		result.m_nodes[node].m_sources.resize(inode.second.m_source_port_names.size());

		for(uint32_t port = 0; port != inode.second.m_source_port_names.size(); port++) {
			result.m_nodes[node].m_sources[port].m_name = inode.second.m_source_port_names[port];
			result.m_nodes[node].m_sources[port].m_position = inode.second.m_positions[port];
		}

		if(inode.second.m_text_id == m_default_audio_sink)
			result.m_default_sink = inode.second.m_osdid;
		if(inode.second.m_text_id == m_default_audio_source)
			result.m_default_source = inode.second.m_osdid;
		node ++;
	}

	for(auto &istream : m_streams)
		if(istream.second.m_target_node)
			result.m_streams.emplace_back(osd::audio_info::stream_info { istream.second.m_osdid, istream.second.m_target_node->m_osdid, istream.second.m_volumes });

	pw_thread_loop_unlock(m_loop);
	return result;
}

void sound_pipewire::stream_event_process(stream_info *stream)
{
	pw_buffer *buffer = pw_stream_dequeue_buffer(stream->m_stream);
	if(!buffer)
		return;

	spa_buffer *sbuf = buffer->buffer;
	uint32_t request = buffer->requested;
	int16_t *data = (int16_t *)(sbuf->datas[0].data);
	uint32_t pos = 0;
	while(pos != request) {
		if(stream->m_buffers.empty()) {
			while(pos != request) {
				memcpy(data, stream->m_last_sample.data(), stream->m_channels*2);
				data += stream->m_channels;
				pos ++;				
			}
			break;
		}

		auto &buf = stream->m_buffers.front();
		uint32_t avail = buf.m_data.size() / stream->m_channels - buf.m_cpos;
		if(avail > request - pos) {
			avail = request - pos;
			memcpy(data, buf.m_data.data() + buf.m_cpos * stream->m_channels, avail*2*stream->m_channels);
			buf.m_cpos += avail;
			break;
		}

		memcpy(data, buf.m_data.data() + buf.m_cpos * stream->m_channels, avail*2*stream->m_channels);
		stream->m_buffers.erase(stream->m_buffers.begin());
		pos += avail;
		data += avail * stream->m_channels;
	}			

	sbuf->datas[0].chunk->offset = 0;
	sbuf->datas[0].chunk->stride = stream->m_channels*2;
	sbuf->datas[0].chunk->size = request*stream->m_channels*2;

	pw_stream_queue_buffer(stream->m_stream, buffer);
}

void sound_pipewire::s_stream_event_process(void *data)
{
	stream_info *info = (stream_info *)(data);
	info->m_wire->stream_event_process(info);
}

void sound_pipewire::stream_event_param_changed(stream_info *stream, uint32_t id, const spa_pod *param)
{
	if(id == SPA_PARAM_Props) {
		const spa_pod_prop *vols = spa_pod_find_prop(param, nullptr, SPA_PROP_channelVolumes);
		if(vols) {
			bool initial = stream->m_volumes.empty();
			stream->m_volumes.clear();
			float *entry;
			SPA_POD_ARRAY_FOREACH((spa_pod_array *)(&vols->value), entry) {
				stream->m_volumes.push_back(osd::linear_to_db(*entry));
			}
			if(!stream->m_volumes.empty()) {
				if(initial) {
					m_wait_stream = false;
					pw_thread_loop_signal(m_loop, false);
				} else
					m_generation++;
			}
		}
	}
}

void sound_pipewire::s_stream_event_param_changed(void *data, uint32_t id, const spa_pod *param)
{
	stream_info *info = (stream_info *)(data);
	info->m_wire->stream_event_param_changed(info, id, param);
}

uint32_t sound_pipewire::stream_sink_open(uint32_t node, std::string name, uint32_t rate)
{
	pw_thread_loop_lock(m_loop);
	auto ni = m_node_osdid_to_id.find(node);
	if(ni == m_node_osdid_to_id.end()) {
		pw_thread_loop_unlock(m_loop);
		return 0;
	}
	node_info &snode = m_nodes.find(ni->second)->second;

	uint32_t id = m_stream_current_id++;
	auto &stream = m_streams.emplace(id, stream_info(this, id)).first->second;

	stream.m_stream = pw_stream_new_simple(pw_thread_loop_get_loop(m_loop),
										   name.c_str(), 
										   pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
															 PW_KEY_MEDIA_CATEGORY, "Playback",
															 PW_KEY_MEDIA_ROLE, "Game",
															 PW_KEY_TARGET_OBJECT, snode.m_serial.c_str(),
															 nullptr),
										   &stream_events,
										   &stream);
	stream.m_channels = snode.m_sink_port_names.size();
	stream.m_last_sample.resize(stream.m_channels);
	stream.m_target_node = &snode;

	const spa_pod *params;
	spa_audio_info_raw format = {
		SPA_AUDIO_FORMAT_S16,
		0,
		rate,
		stream.m_channels
	};
	for(uint32_t i=0; i != snode.m_sink_port_names.size(); i++)
		format.position[i] = snode.m_position_codes[i];

	uint8_t buffer[1024];
	spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &format);

	pw_stream_connect(stream.m_stream,
					  PW_DIRECTION_OUTPUT,
					  PW_ID_ANY,
					  pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
					  &params, 1);

	m_wait_stream = true;
	while(m_wait_stream)
		pw_thread_loop_wait(m_loop);
	pw_thread_loop_unlock(m_loop);

	stream.m_source_node_id = pw_stream_get_node_id(stream.m_stream);

	return id;
}

void sound_pipewire::stream_set_volumes(uint32_t id, const std::vector<float> &db)
{
	pw_thread_loop_lock(m_loop);
	auto si = m_streams.find(id);
	if(si == m_streams.end()) {
		pw_thread_loop_unlock(m_loop);
		return;
	}
	stream_info &stream = si->second;
	stream.m_volumes = db;
	std::vector<float> linear;
	for(float db1 : db)
		linear.push_back(osd::db_to_linear(db1));
	pw_stream_set_control(stream.m_stream, SPA_PROP_channelVolumes, linear.size(), linear.data(), 0);
	pw_thread_loop_unlock(m_loop);
}

void sound_pipewire::stream_close(uint32_t id)
{
	pw_thread_loop_lock(m_loop);
	auto si = m_streams.find(id);
	if(si == m_streams.end()) {
		pw_thread_loop_unlock(m_loop);
		return;
	}
	stream_info &stream = si->second;
	pw_stream_destroy(stream.m_stream);
	m_streams.erase(si);
	pw_thread_loop_unlock(m_loop);
}

void sound_pipewire::stream_update(uint32_t id, const int16_t *buffer, int samples_this_frame)
{
	pw_thread_loop_lock(m_loop);
	auto si = m_streams.find(id);
	if(si == m_streams.end()) {
		pw_thread_loop_unlock(m_loop);
		return;
	}
	stream_info &stream = si->second;

	stream.m_buffers.resize(stream.m_buffers.size() + 1);
	auto &buf = stream.m_buffers.back();
	buf.m_cpos = 0;
	buf.m_data.resize(samples_this_frame*stream.m_channels);
	memcpy(buf.m_data.data(), buffer, samples_this_frame*2*stream.m_channels);
	memcpy(stream.m_last_sample.data(), buffer + (samples_this_frame-1)*stream.m_channels, 2*stream.m_channels);

	if(stream.m_buffers.size() > 10)
		// If there way too many buffers, drop some so only 10 are left (roughly 0.2s)
		stream.m_buffers.erase(stream.m_buffers.begin(), stream.m_buffers.begin() + stream.m_buffers.size() - 10);

	else if(stream.m_buffers.size() >= 5)
		// If there are too many buffers, remove five sample per buffer
		// to slowly resync to reduce latency (4 seconds to
		// compensate one buffer roughly)
		buf.m_cpos = 5;

	pw_thread_loop_unlock(m_loop);
}

#else
	MODULE_NOT_SUPPORTED(sound_pipewire, OSD_SOUND_PROVIDER, "pipewire")
#endif

MODULE_DEFINITION(SOUND_PIPEWIRE, sound_pipewire)

#ifdef _WIN32
#  define WINVER 0x0500
#  define _WIN32_WINNT 0x0500
#  include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>
#include <thread>

//Boost for POSIX Timestamps
#include <boost/date_time/posix_time/posix_time_types.hpp>
boost::posix_time::ptime const time_epoch(boost::gregorian::date(1970, 1, 1));

//Discord
#include "discord.h"
Discord discord;
RichPresence rich;

//Discord Update Thread
std::thread discordThread;
long long lastUpdated;
bool stopUpdating = false;

// SDK
#include <scssdk_telemetry.h>
#include <eurotrucks2/scssdk_eut2.h>
#include <eurotrucks2/scssdk_telemetry_eut2.h>
#include <amtrucks/scssdk_ats.h>
#include <amtrucks/scssdk_telemetry_ats.h>
#define registerChannel(name, type, to) version_params->register_for_channel(SCS_TELEMETRY_##name, SCS_U32_NIL, SCS_VALUE_TYPE_##type, SCS_TELEMETRY_CHANNEL_FLAG_no_value, telemetry_store_##type, &( to ));
#define registerSpecificChannel(name, type, handler, to) version_params->register_for_channel(SCS_TELEMETRY_##name, SCS_U32_NIL, SCS_VALUE_TYPE_##type, SCS_TELEMETRY_CHANNEL_FLAG_no_value, handler, &( to ));
#define UNUSED(x)



long long getEpoch()
{
	return (boost::posix_time::microsec_clock::local_time() - time_epoch).total_microseconds() / 1000;
}


/**
 * @brief Last timestamp we received.
 */
scs_timestamp_t last_timestamp = static_cast<scs_timestamp_t>(-1);

/**
 * @brief Combined telemetry data.
 */
struct telemetry_state_t
{
	scs_timestamp_t timestamp;
	scs_timestamp_t raw_rendering_timestamp;
	scs_timestamp_t raw_simulation_timestamp;
	scs_timestamp_t raw_paused_simulation_timestamp;



	float	speed;
	bool onJob;
	bool paused;
	bool lowBeamsOn;
	bool wipersOn;
	bool hasTruck;
	int income;
	std::string makeID;
	std::string model;
	float odometer;
	std::string make;
	std::string source;
	std::string destination;


} telemetry;


void updateDiscordPresence()
{
	while (!stopUpdating)
	{
		if (stopUpdating)
			return;
		if (getEpoch() - lastUpdated >= 15)
		{
			if (telemetry.onJob)
			{
				rich.state = std::string(telemetry.source + " to " + telemetry.destination);
				rich.largeText = std::string("Estimated Income: " + std::to_string(telemetry.income) + " EUR");
			}
			else
			{
				rich.state = "Freeroaming";
				rich.largeText = "";
			}

			if (telemetry.hasTruck)
			{
				rich.details = std::string("Driving at " + std::to_string((int)(telemetry.speed * 2.23694)) + " mph");
				rich.smallKey = std::string("brand_" + telemetry.makeID);
				rich.smallText = std::string(telemetry.make + " " + telemetry.model + " - At " + std::to_string((int)(telemetry.odometer*0.621371)) + " Miles");

				//night time, not raining
				if (telemetry.lowBeamsOn && !telemetry.wipersOn)
				{
					//rich.largeKey = "ets2rpc_night";
				}
				//night time, raining
				else if (telemetry.lowBeamsOn && telemetry.wipersOn)
				{
					//rich.largeKey = "ets2rpc_rain";
				}
				//Day time, not raining
				else if (!telemetry.lowBeamsOn && !telemetry.wipersOn)
				{
					//rich.largeKey = "ets2rpc_day";
				}
				//Day time, raining
				else if (!telemetry.lowBeamsOn && telemetry.wipersOn)
				{
					//rich.largeKey = "ets2rpc_rain";
				}
			}

			discord.UpdatePresence(rich);
			lastUpdated = getEpoch();
		}
		Sleep(1000);
	}
}

/**
 * @brief Function writting message to the game internal log.
 */
scs_log_t game_log = NULL;


SCSAPI_VOID telemetry_store_float(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context)
{
	// The SCS_TELEMETRY_CHANNEL_FLAG_no_value flag was not provided during registration
	// so this callback is only called when a valid value is available.

	assert(value);
	assert(value->type == SCS_VALUE_TYPE_float);
	assert(context);
	*static_cast<float *>(context) = value->value_float.value;
}

SCSAPI_VOID telemetry_store_s32(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context)
{
	// The SCS_TELEMETRY_CHANNEL_FLAG_no_value flag was not provided during registration
	// so this callback is only called when a valid value is available.

	assert(value);
	assert(value->type == SCS_VALUE_TYPE_s32);
	assert(context);
	*static_cast<int *>(context) = value->value_s32.value;
}

#pragma region SCS_VALUE_TYPE_u32 To int
SCSAPI_VOID telemetry_store_u32(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context)
{
	if (!value) return;
	assert(value);
	assert(value->type == SCS_VALUE_TYPE_u32);
	assert(context);
	*static_cast<unsigned int *>(context) = value->value_u32.value;
}
#pragma endregion

#pragma region SCS_VALUE_TYPE_u64 To int
SCSAPI_VOID telemetry_store_u64(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context)
{
	if (!value) return;
	assert(value);
	assert(value->type == SCS_VALUE_TYPE_u64);
	assert(context);
	*static_cast<unsigned int *>(context) = (unsigned int)value->value_u64.value;
}
#pragma endregion

#pragma region SCS_VALUE_TYPE_bool To Bool
SCSAPI_VOID telemetry_store_bool(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context)
{
	if (!context) return;
	if (value)
	{
		if (value->value_bool.value == 0)
			*static_cast<bool*>(context) = false;
		else
			*static_cast<bool*>(context) = true;
	}
	else
		*static_cast<bool*>(context) = false;
}

#pragma endregion

#pragma region SCS_VALUE_TYPE_string To std::string
SCSAPI_VOID telemetry_store_string(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context)
{
	if (!context) return;
	if (value)
	{
		*static_cast<std::string*>(context) = value->value_string.value;
	}
	else
		*static_cast<std::string*>(context) = "";
}
#pragma endregion


// Handling of individual events.

SCSAPI_VOID telemetry_frame_start(const scs_event_t UNUSED(event), const void *const event_info, const scs_context_t UNUSED(context))
{
	const struct scs_telemetry_frame_start_t *const info = static_cast<const scs_telemetry_frame_start_t *>(event_info);

	// The following processing of the timestamps is done so the output
	// from this plugin has continuous time, it is not necessary otherwise.

	// When we just initialized itself, assume that the time started
	// just now.

	if (last_timestamp == static_cast<scs_timestamp_t>(-1)) {
		last_timestamp = info->paused_simulation_time;
	}

	// The timer might be sometimes restarted (e.g. after load) while
	// we want to provide continuous time on our output.

	if (info->flags & SCS_TELEMETRY_FRAME_START_FLAG_timer_restart) {
		last_timestamp = 0;
	}

	// Advance the timestamp by delta since last frame.

	telemetry.timestamp += (info->paused_simulation_time - last_timestamp);
	last_timestamp = info->paused_simulation_time;

	// The raw values.

	telemetry.raw_rendering_timestamp = info->render_time;
	telemetry.raw_simulation_timestamp = info->simulation_time;
	telemetry.raw_paused_simulation_timestamp = info->paused_simulation_time;
}

SCSAPI_VOID telemetry_frame_end(const scs_event_t UNUSED(event), const void *const UNUSED(event_info), const scs_context_t UNUSED(context))
{

}

SCSAPI_VOID telemetry_pause(const scs_event_t event, const void *const UNUSED(event_info), const scs_context_t UNUSED(context))
{
	telemetry.paused = (event == SCS_TELEMETRY_EVENT_paused);
}

SCSAPI_VOID telemetry_configuration(const scs_event_t event, const void *const event_info, const scs_context_t UNUSED(context))
{
	const struct scs_telemetry_configuration_t *const info = static_cast<const scs_telemetry_configuration_t *>(event_info);

	#pragma region Truck Configuration Change
	if (strcmp(info->id, SCS_TELEMETRY_CONFIG_truck) == 0)
	{
		bool hasTruck = false;
		for (const scs_named_value_t *cur = info->attributes; cur->name; ++cur)
		{
			if (strcmp(cur->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand_id) == 0)
			{
				//telemetry_store_string(nullptr, 0, &cur->value, &telemetry.makeID);
				telemetry.makeID = std::string(cur->value.value_string.value);
			}
			else if (strcmp(cur->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand) == 0)
			{
				//telemetry_store_string(nullptr, 0, &cur->value, &telemetry.make);
				telemetry.make = std::string(cur->value.value_string.value);
			}
			else if (strcmp(cur->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_name) == 0)
			{
				//telemetry_store_string(nullptr, 0, &cur->value, &telemetry.model);
				telemetry.model = std::string(cur->value.value_string.value);
			}
			hasTruck = true;
		}
		telemetry.hasTruck = hasTruck;
	}
	#pragma endregion
	#pragma region Job Configuration Change
	else if (strcmp(info->id, SCS_TELEMETRY_CONFIG_job) == 0)
	{
		bool hasJob = false;
		for (const scs_named_value_t *cur = info->attributes; cur->name; ++cur)
		{
			if (strcmp(cur->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_destination_city) == 0)
			{
				//telemetry_store_string(nullptr, 0, &cur->value, &telemetry.destination);
				telemetry.destination = std::string(cur->value.value_string.value);
			}
			else if (strcmp(cur->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_source_city) == 0)
			{
				//telemetry_store_string(nullptr, 0, &cur->value, &telemetry.source);
				telemetry.source = std::string(cur->value.value_string.value);
			}
			else if (strcmp(cur->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_income) == 0)
			{
				telemetry_store_u64(nullptr, 0, &cur->value, &telemetry.income);
			}
			telemetry.onJob = true;
			hasJob = true;
		}
		if (!hasJob)
			telemetry.onJob = false;
	}
	#pragma endregion
}	




SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t *const params)
{

	if (version != SCS_TELEMETRY_VERSION_1_00) {
		return SCS_RESULT_unsupported;
	}

	const scs_telemetry_init_params_v100_t *const version_params = static_cast<const scs_telemetry_init_params_v100_t *>(params);


	const bool events_registered =
		(version_params->register_for_event(SCS_TELEMETRY_EVENT_frame_start, telemetry_frame_start, NULL) == SCS_RESULT_ok) &&
		(version_params->register_for_event(SCS_TELEMETRY_EVENT_frame_end, telemetry_frame_end, NULL) == SCS_RESULT_ok) &&
		(version_params->register_for_event(SCS_TELEMETRY_EVENT_paused, telemetry_pause, NULL) == SCS_RESULT_ok) &&
		(version_params->register_for_event(SCS_TELEMETRY_EVENT_started, telemetry_pause, NULL) == SCS_RESULT_ok)
	;
	if (! events_registered) {
		version_params->common.log(SCS_LOG_TYPE_error, "Unable to register event callbacks");
		return SCS_RESULT_generic_error;
	}
	version_params->register_for_event(SCS_TELEMETRY_EVENT_configuration, telemetry_configuration, NULL);

	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_store_float, &telemetry.speed);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_low_beam, SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_store_bool, &telemetry.lowBeamsOn);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wipers, SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_store_bool, &telemetry.wipersOn);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_odometer, SCS_U32_NIL, SCS_VALUE_TYPE_float, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_store_float, &telemetry.odometer);
	

	game_log = version_params->common.log;

	memset(&telemetry, 0, sizeof(telemetry));
	last_timestamp = static_cast<scs_timestamp_t>(-1);
	discord = Discord();
	discord.Initialise("426512878108016647");
	rich.largeKey = "large_image_1";
	rich.state = "Game Initialized";
	rich.details = "Game loading...";

	discord.UpdatePresence(rich);
	lastUpdated = getEpoch();
	discordThread = std::thread(updateDiscordPresence);

	return SCS_RESULT_ok;
}

SCSAPI_VOID scs_telemetry_shutdown(void)
{
	game_log = NULL;
	stopUpdating = true;
	discordThread.join();
	discord.~Discord();
	
}



#ifdef _WIN32
BOOL APIENTRY DllMain(
	HMODULE module,
	DWORD  reason_for_call,
	LPVOID reseved
)
{
	if (reason_for_call == DLL_PROCESS_DETACH)
	{
		
	}
	else if (reason_for_call == DLL_PROCESS_ATTACH)
	{
		
	}
	return TRUE;
}
#endif

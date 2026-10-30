#pragma once

#include <stdint.h>

#define MAX_STATES 10000

struct aircraft_state
{
	uint32_t ICAO; // ICAO is used as a hash
	float    Latitude;
	float    Longitude;
	float    Altitude;
	float    Velocity;
	float	 Heading;
};

class aircraft_state_hash_table
{
public:
	void            Init();
	void            Free();
	void            CopyTo(aircraft_state_hash_table Other);
	aircraft_state* GetState(int ICAO);
	void SetState(aircraft_state State);

	aircraft_state* States;
};
#include "AircraftState.h"

#include <stdlib.h>
#include <string.h>

void aircraft_state_hash_table::Init()
{
	States = (aircraft_state*)calloc(MAX_STATES, sizeof(aircraft_state));
}

void aircraft_state_hash_table::Free()
{
	free(States);
}

void aircraft_state_hash_table::CopyTo(aircraft_state_hash_table Other)
{
	memcpy(Other.States, States, MAX_STATES * sizeof(aircraft_state));
}

aircraft_state* aircraft_state_hash_table::GetState(int ICAO)
{
	aircraft_state* State = NULL;

	for (int i = 0; i < MAX_STATES; ++i)
	{
		int StateIdx = ICAO % MAX_STATES;
		State = States + StateIdx;
		if (State->ICAO)
		{
			if (State->ICAO == ICAO)
			{
				break;
			}
		}
		else
		{
			State->ICAO = ICAO;
			break;
		}

		++ICAO;
	}

	return State;
}

void aircraft_state_hash_table::SetState(aircraft_state State)
{
	aircraft_state* StateToSet = GetState(State.ICAO);
	*StateToSet = State;
}
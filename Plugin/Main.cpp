#include <stdio.h>
#include <string.h>
#include <float.h>

#include "Udp.h"

#include <windows.h>
#include <wininet.h>

#include "XPLMCamera.h"
#include "XPLMInstance.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMMenus.h"
#include "XPLMUtilities.h"
#include "XPLMPlugin.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"

#include "Common.h"
#include "ARMath.h"
#include "RequestContext.h"
#include "WorkQueue.h"
#include "AircraftState.h"

static request_context Ctx;

static udp Udp;

static const char* AircraftVirtualFilename = "lib/airport/aircraft/airliners/heavy_e.obj";
static XPLMObjectRef     AircraftObj;
static XPLMInstanceRef* AircraftInstances;
static int               AircraftInstancesCount;

static void FreeAircraftInstances()
{
	for (int i = 0; i < AircraftInstancesCount; ++i)
	{
		XPLMInstanceRef instance = AircraftInstances[i];
		if (instance)
		{
			XPLMDestroyInstance(instance);
		}
	}

	free(AircraftInstances);
}

static void LibraryEnumerator(const char* Filename, void* Ref)
{
	XPLMObjectRef* Dst = (XPLMObjectRef*)Ref;
	if (!(*Dst))
	{
		*Dst = XPLMLoadObject(Filename);
	}
}

static Vec3 UserCamPos;
static Vec3 CurrCamPos;

static float BoundingRadius = .1f;
double SentLat, SentLong, SentAlt;

Vec3 GetLocalPos(float Latitude, float Longitude, float Altitude)
{
	double x, y, z;
	XPLMWorldToLocal(Latitude, Longitude, Altitude, &x, &y, &z);

	Vec3 Result = { x, y, z };
	return Result;
}

static float FlightLoop(float ElapsedTimeSinceLastCall, float ElapsedTimeSinceLastFlightLoop, int Counter, void* Ref)
{
	float Result = -1.0f; // Call the flight loop ASAP

	XPLMDataRef LatRef = XPLMFindDataRef("sim/flightmodel/position/latitude");
	XPLMDataRef LongRef = XPLMFindDataRef("sim/flightmodel/position/longitude");
	XPLMDataRef AltRef = XPLMFindDataRef("sim/flightmodel/position/elevation");

	UserCamPos.x = XPLMGetDataf(LatRef);
	UserCamPos.y = XPLMGetDataf(LongRef);
	UserCamPos.z = XPLMGetDataf(AltRef);

	if (!AircraftObj)
	{
		XPLMLookupObjects(AircraftVirtualFilename, 0.0f, 0.0f, LibraryEnumerator, &AircraftObj);
	}

	if (!Ctx.Update(ElapsedTimeSinceLastCall))
	{
		Ctx.UpdateFromDatabase(ElapsedTimeSinceLastFlightLoop);
	}

	if (Ctx.Hashes)
	{
		if (Ctx.HashCount > AircraftInstancesCount)
		{
			if (AircraftInstances)
			{
				FreeAircraftInstances();
			}

			AircraftInstancesCount = Ctx.HashCount;
			AircraftInstances = (XPLMInstanceRef*)malloc(AircraftInstancesCount * sizeof(*AircraftInstances));
			for (int i = 0; i < AircraftInstancesCount; ++i)
			{
				if (AircraftObj)
				{
					AircraftInstances[i] = XPLMCreateInstance(AircraftObj, NULL);
				}
			}
		}

		for (int i = 0; i < Ctx.HashCount; ++i)
		{
			uint32_t ICAO = Ctx.Hashes[i];

			aircraft_state* Curr = Ctx.CurrHashTable.GetState(ICAO);
			aircraft_state* Prev = Ctx.PrevHashTable.GetState(ICAO);

			XPLMInstanceRef Instance = AircraftInstances[i];

			if (Instance)
			{
				Vec3 PrevLocalPos = GetLocalPos(Prev->Latitude, Prev->Longitude, Prev->Altitude);
				Vec3 CurrLocalPos = GetLocalPos(Curr->Latitude, Curr->Longitude, Curr->Altitude);

				Vec3 Direction = normalize_vec3(CurrLocalPos - PrevLocalPos);
				Vec3 XZDirection = normalize_vec3({ Direction.x, 0.0f, Direction.z });
				Vec3 YZDirection = normalize_vec3({ 0.0f, Direction.y, Direction.z });

				float Heading = rad_to_deg(atan2f(XZDirection.x, -XZDirection.z));
				float Pitch = rad_to_deg(atan2f(YZDirection.y, fabsf(YZDirection.z)));

				double Latitude = Lerp(Prev->Latitude, Curr->Latitude, Ctx.Timer / SEND_TIME_THRESHOLD);
				double Longitude = Lerp(Prev->Longitude, Curr->Longitude, Ctx.Timer / SEND_TIME_THRESHOLD);
				double Altitude = Lerp(Prev->Altitude, Curr->Altitude, Ctx.Timer / SEND_TIME_THRESHOLD);

				Vec3 LocalPos = GetLocalPos(Latitude, Longitude, Altitude);

				XPLMDrawInfo_t Info = {};
				Info.structSize = sizeof(Info);
				Info.x = LocalPos.x;
				Info.y = LocalPos.y;
				Info.z = LocalPos.z;
				Info.pitch = Pitch;
				Info.heading = Heading;
				Info.roll = 0.0f;

				XPLMInstanceSetPosition(Instance, &Info, NULL);

				if ((Latitude < UserCamPos.x + BoundingRadius && Latitude > UserCamPos.x - BoundingRadius) && (Longitude < UserCamPos.y + BoundingRadius && Longitude > UserCamPos.y - BoundingRadius))
				{
					char Buf[512] = {};
					snprintf(Buf, sizeof(Buf), "n%un127.0.0.1n%fn%fn%f", ICAO, Latitude, Longitude, Altitude);
					Udp.Send(Buf);
				}
			}
		}
	}

	return Result;
}

int DummyMouseHandler(XPLMWindowID Window, int x, int y, int IsDown, void* Refcon)
{
	return 0;
}

XPLMCursorStatus DummyCursorStatusHandler(XPLMWindowID Window, int x, int y, void* Refcon)
{
	return xplm_CursorDefault;
}

int DummyWheelHandler(XPLMWindowID Window, int x, int y, int Wheel, int Clicks, void* Refcon)
{
	return 0;
}

void DummyKeyHandler(XPLMWindowID Window, char Key, XPLMKeyFlags Flags, char VK, void* Refcon, int LosingFocus)
{
	return;
}

PLUGIN_API int XPluginStart(char* Name, char* Sig, char* Desc)
{
	strcpy(Name, "AR Aircraft");
	strcpy(Sig, "jlaffan.plugins.ar_aircraft");
	strcpy(Desc, "Renders realtime aircrafts.");

	Udp.Init();

	if (!Ctx.Init())
	{
		return 0;
	}

	XPLMRegisterFlightLoopCallback(FlightLoop, -1.0f, NULL);

	XPLMDataRef LatitudeRef = XPLMFindDataRef("sim/flightmodel/position/local_x");
	XPLMDataRef LongitudeRef = XPLMFindDataRef("sim/flightmodel/position/local_y");
	XPLMDataRef AltitudeRef = XPLMFindDataRef("sim/flightmodel/position/local_z");

	CurrCamPos.x = XPLMGetDataf(LatitudeRef);
	CurrCamPos.y = XPLMGetDataf(LongitudeRef);
	CurrCamPos.z = XPLMGetDataf(AltitudeRef);;

	return 1;
}

PLUGIN_API void	XPluginStop(void)
{
	Ctx.Free();

	FreeAircraftInstances();

	Udp.Cleanup();

	if (AircraftObj)
	{
		XPLMUnloadObject(AircraftObj);
	}
}

PLUGIN_API int XPluginEnable(void)
{
	return 1;
}

PLUGIN_API void XPluginDisable(void)
{
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID From, int Msg, void* Refcon)
{
}
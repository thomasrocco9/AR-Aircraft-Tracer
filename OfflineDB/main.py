import requests
import struct
import time
from datetime import datetime, timedelta

USERNAME = 'btenbergen'
PASSWORD = 'FHpJ5GtMdXjcVDz'
URL = "https://opensky-network.org/api/states/all"

runtime = 24  # runtime duration in hours


def fetch_data():
    try:
        response = requests.get(URL, auth=(USERNAME, PASSWORD), timeout=10)
        response.raise_for_status()
        data = response.json()
        timestamp = data.get('time', 0)
        states = data.get('states', [])
        planes = [
            (
                state[0] or "",
                state[5] or 0.0,
                state[6] or 0.0,
                state[7] or 0.0,
                state[9] or 0.0,
                state[10] or 0.0
            )
            for state in states
        ]
        num_aircraft = len(planes)
        return planes, timestamp, num_aircraft
    except requests.exceptions.RequestException as e:
        print(f"Error fetching data: {e}")
        return [], 0, 0


def binary_write(filename, planes, timestamp, num_aircraft):
    with open(filename, 'ab') as file:
        header = struct.pack('>QI', timestamp, num_aircraft)
        file.write(header)

        for plane in planes:
            icao24 = plane[0].encode('utf-8')[:7]
            latitude = plane[1]
            longitude = plane[2]
            altitude = plane[3]
            velocity = plane[4]
            heading = plane[5]

            icao24 = icao24.ljust(7, b'\0')  # Pad or truncate to 7 bytes
            packed_data = struct.pack('>7sddfff', icao24, latitude, longitude, altitude, velocity, heading)
            file.write(packed_data)

        print(f"Wrote {num_aircraft} planes to {filename} at timestamp {timestamp}")


def wait_until_midnight():
    now = datetime.now()
    midnight = datetime(now.year, now.month, now.day) + timedelta(days=1)
    time_until_midnight = (midnight - now).total_seconds()
    print(f"Waiting for {time_until_midnight} seconds until midnight...")
    time.sleep(time_until_midnight)


def write_data():
    wait_until_midnight()  # Wait until midnight
    end_time = datetime.now() + timedelta(hours=runtime)

    while datetime.now() < end_time:
        planes, timestamp, num_aircraft = fetch_data()
        if planes:  # Write only if data was fetched
            binary_write('offlineDB.bin', planes, timestamp, num_aircraft)
            time.sleep(10)

if __name__ == "__main__":
    print("Starting data collection...")
    write_data()
    print("Data collection completed.")

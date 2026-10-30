const fs = require('fs');

console.log("Initializing map...");

var map = L.map('map', {
    center: [43.4559, -76.5105],  // Oswego
    zoom: 11,
    minZoom: 2,
    maxZoom: 19
});
console.log("Map initialized");

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
}).addTo(map);

console.log("Tile layer added");

const buf = fs.readFileSync('FilteredDatabase.bin');

let flightPaths = {};

let offset = 0;
while (offset < buf.length) {
	const timestamp = buf.readBigUInt64BE(offset);
	const aircraftCount = buf.readUInt32BE(offset + 8);
	offset += 12;

	for (let i = 0; i < aircraftCount; ++i) {
		const icao = buf.toString('ascii', offset, offset + 7)
		offset += 7;

		const longitude = buf.readDoubleBE(offset);
		const latitude = buf.readDoubleBE(offset + 8);
		offset += 16;

		offset += 12;

		if (!flightPaths[icao]) {
			flightPaths[icao] = [];
		}

		if (longitude != 0.0 && latitude != 0.0) {
			flightPaths[icao].push([latitude, longitude]);
		}
	}
}

for (const icao in flightPaths) {
	let polyline = L.polyline(flightPaths[icao], {color: 'red'}).addTo(map);

	polyline.on('click', function() {
		console.log(icao);
		console.log(flightPaths[icao]);

        const path = flightPaths[icao];
        const start = path[0];
        const end = path[path.length - 1];

		var popupElement = document.getElementById('custom-popup');

		popupElement.innerHTML = `
            <strong>ICAO:</strong> ${icao}<br>
            <strong>Starting Location:</strong> ${start[0]}, ${start[1]}<br>
            <strong>Ending Location:</strong> ${end[0]}, ${end[1]}
        `;

		popupElement.style.display = 'block';
	});
}

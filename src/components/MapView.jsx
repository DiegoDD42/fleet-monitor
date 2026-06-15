import {
  MapContainer,
  TileLayer,
  Marker,
  Popup,
  Polyline,
} from "react-leaflet";
import { Fragment } from "react";

function MapView({ vehicles }) {
  return (
    <MapContainer
      center={[-18.9186, -48.2772]}
      zoom={13}
      style={{ height: "500px" }}
    >
      <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png" />

      {Object.entries(vehicles).map(([id, vehicle]) => (
        <Fragment key={id}>
        
          <Marker key={`marker-${id}`} position={[vehicle.lat, vehicle.lng]}>
            <Popup>
              <strong>{id}</strong>
              <br />
              Velocidade: {vehicle.velocidade}
              {" "}
              km/h
              <br />
              Bateria: {" "} {vehicle.bateria}%
              <br />
              Status: {" "} {vehicle.status}
            </Popup>
          </Marker>

          <Polyline key={`line-${id}`} positions={vehicle.route} />
        </Fragment>
      ))}
    </MapContainer>
  );
}

export default MapView;

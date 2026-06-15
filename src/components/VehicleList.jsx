import ListGroup from "react-bootstrap/ListGroup";
import Badge from "react-bootstrap/Badge";

function VehicleList({ vehicles }) {
  return (
    <>
      <h4>Veículos</h4>

      <ListGroup>
        {Object.entries(vehicles).map(([id, v]) => {
          const online = Date.now() - (v.updatedAt || 0) < 10000;

          return (
            <ListGroup.Item key={id}>
              <strong>{id}</strong>
              <br />
              <Badge bg={online ? "success" : "danger"}>
                {online ? "ONLINE" : "OFFLINE"}
              </Badge>
              <br />
              Status: {v.status}
              <br />
              Velocidade: {v.velocidade || 0} km/h
              <br />
              Bateria: {v.bateria || 0}%
            </ListGroup.Item>
          );
        })}
      </ListGroup>
    </>
  );
}

export default VehicleList;

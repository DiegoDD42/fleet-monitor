import Container from "react-bootstrap/Container";
import Row from "react-bootstrap/Row";
import Col from "react-bootstrap/Col";
import Card from "react-bootstrap/Card";

import useMqtt from "./hooks/useMqtt";

import MapView from "./components/MapView";
import VehicleList from "./components/VehicleList";
import CommandPanel from "./components/CommandPanel";
import SpeedChart from "./components/SpeedChart";

function App() {
  const { vehicles, client } = useMqtt();

  return (
    <Container fluid className="mt-3">
      <Row>
        <Col md={3}>
          <VehicleList vehicles={vehicles} />
        </Col>

        <Col md={9}>
          <MapView vehicles={vehicles} />
        </Col>
      </Row>

      <Row className="mt-4">
        <Col md={4}>
          <CommandPanel client={client} vehicles={vehicles} />
        </Col>

        <Col md={8}>
          <SpeedChart vehicles={vehicles} />
        </Col>
      </Row>

      <Row className="mb-3">
        <Col>
          <Card>
            <Card.Body>
              Veículos conectados: {Object.keys(vehicles).length}
            </Card.Body>
          </Card>
        </Col>
      </Row>
    </Container>
  );
}

export default App;

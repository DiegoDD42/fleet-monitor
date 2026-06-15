import { useState } from "react";
import Button from "react-bootstrap/Button";
import Form from "react-bootstrap/Form";

function CommandPanel({ client, vehicles }) {
  const [selected, setSelected] = useState("");

  const sendCommand = (command) => {
    if (!selected) return;

    client.publish(`frota/${import.meta.env.VITE_MQTT_COMPANY}/${selected}/comando`, command, {
      qos: 2,
    });
  };

  return (
    <>
      <h4>Comandos</h4>

      <Form.Select onChange={(e) => setSelected(e.target.value)}>
        <option>Selecione</option>

        {Object.keys(vehicles).map((id) => (
          <option key={id}>{id}</option>
        ))}
      </Form.Select>

      <div className="mt-3">
        <Button className="me-2" onClick={() => sendCommand("PARAR")}>
          PARAR
        </Button>

        <Button className="me-2" onClick={() => sendCommand("RETOMAR")}>
          RETOMAR
        </Button>

        <Button onClick={() => sendCommand("BUZINAR")}>BUZINAR</Button>
      </div>
    </>
  );
}

export default CommandPanel;

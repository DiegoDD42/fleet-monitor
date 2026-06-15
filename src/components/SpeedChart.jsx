import { useState } from "react";
import Form from "react-bootstrap/Form";
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
  ResponsiveContainer,
} from "recharts";

function SpeedChart({ vehicles }) {
  const ids = Object.keys(vehicles);
  const [selected, setSelected] = useState("");

  const vehicleId = selected || ids[0];
  const data = vehicles[vehicleId]?.speedHistory || [];

  if (ids.length === 0) return null;

  return (
    <>
      <h4>Velocidade — {vehicleId}</h4>

      <Form.Select
        className="mb-3"
        value={vehicleId}
        onChange={(e) => setSelected(e.target.value)}
      >
        {ids.map((id) => (
          <option key={id} value={id}>{id}</option>
        ))}
      </Form.Select>

      <ResponsiveContainer width="100%" height={300}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis dataKey="time" />
          <YAxis unit=" km/h" />
          <Tooltip />
          <Line type="monotone" dataKey="velocidade" stroke="#0d6efd" dot={false} />
        </LineChart>
      </ResponsiveContainer>
    </>
  );
}

export default SpeedChart;
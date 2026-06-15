import { useEffect, useState } from "react";
import mqtt from "mqtt";

export default function useMqtt() {
    const [vehicles, setVehicles] = useState({});
    const [client, setClient] = useState(null);

    function isOnline(vehicle) {
        if (!vehicle.ts) return false;

        return Date.now() - vehicle.updatedAt < 10000;
    }

    useEffect(() => {
        const mqttClient = mqtt.connect(
            `wss://${import.meta.env.VITE_MQTT_HOST}:${import.meta.env.VITE_MQTT_PORT}/mqtt`,
            {
                username: import.meta.env.VITE_MQTT_USER,
                password: import.meta.env.VITE_MQTT_PASS,
                reconnectPeriod: 3000,
                clean: true,
            }
        );

        mqttClient.on("connect", () => {
            console.log("MQTT conectado");

            mqttClient.subscribe(`frota/${import.meta.env.VITE_MQTT_COMPANY}/+/posicao`, {
                qos: 1,
            });

            mqttClient.subscribe(`frota/${import.meta.env.VITE_MQTT_COMPANY}/+/status`, {
                qos: 1,
            });
        });

        mqttClient.on("message", (topic, message) => {
            try {
                const payload = JSON.parse(message.toString());

                const parts = topic.split("/");
                const vehicleId = parts[2];
                const type = parts[3];

                setVehicles((prev) => {
                    const currentVehicle = prev[vehicleId] || {
                        speedHistory: [],
                        route: [],
                    };

                    let updatedVehicle = {
                        ...currentVehicle,
                    };

                    if (type === "posicao") {
                        const velocidadeReal = payload.parado ? 0 : payload.velocidade;
                        updatedVehicle = {
                            ...updatedVehicle,
                            lat: Number(payload.lat),
                            lng: Number(payload.lng),
                            velocidade: velocidadeReal,
                            bateria: payload.bateria,
                            parado: payload.parado,
                            ts: payload.ts,
                            updatedAt: Date.now(),
                        };

                        updatedVehicle.speedHistory = [
                            ...(currentVehicle.speedHistory || []),
                            {
                                time: new Date().toLocaleTimeString(),
                                velocidade: velocidadeReal,
                            },
                        ].slice(-30);

                        updatedVehicle.route = [
                            ...(currentVehicle.route || []),
                            [
                                Number(payload.lat),
                                Number(payload.lng),
                            ],
                        ].slice(-100);
                    }

                    if (type === "status") {
                        updatedVehicle = {
                            ...updatedVehicle,
                            status: payload.status,
                            bateria: payload.bateria,
                        };
                    }

                    return {
                        ...prev,
                        [vehicleId]: updatedVehicle,
                    };
                });
            } catch (err) {
                console.error(err);
            }
        });

        setClient(mqttClient);

        return () => mqttClient.end();
    }, []);

    return { vehicles, client };
}
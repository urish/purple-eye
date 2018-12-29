const DEVICE_NAME = 'PurpleEye';

const SERVOS_PINS = [D29, D30, D2, D28];
const BATTERY_PIN = D4;
let servoValues = [0, 0, 0, 0];
let servoOffsets = [0, 0, 0, 0];

// Simple Task management
const tasks = {};

function startTask(name, callback, interval) {
    if (tasks[name] == null) {
        tasks[name] = setInterval(callback, interval);
    }
}

function stopTask(name) {
    if (tasks[name] != null) {
        const interval = tasks[name];
        tasks[name] = null;
        clearInterval(interval);
    }
}

// Battery
function getBatteryVoltage() {
    return analogRead(BATTERY_PIN) * NRF.getBattery() * 2;
}

function updateBattery() {
    const percent = Math.max(0, Math.min(100, ((getBatteryVoltage() - 3.6) / 0.6) * 100)) | 0;
    NRF.updateServices({
        0x180f: {
            0x2a19: {
                value: new Uint8Array([percent]),
                notify: true,
            },
        },
    });
}

function servoAngleToPulse(angle) {
    return 0.5 + (angle / 180) * 2;
}

// Servo
function updateServos() {
    for (const i = 0; i < servoValues.length; i++) {
        if (servoValues[i]) {
            digitalPulse(SERVOS_PINS[i], HIGH, servoAngleToPulse(servoOffsets[i] + servoValues[i]));
        }
    }
}

function setServoValues(values) {
    for (const i = 0; i < values.length; i++) {
        servoValues[i] = values[i];
    }
    if (servoValues.find((v) => v > 0)) {
        startTask('servo', updateServos, 50);
    } else {
        stopTask('servo');
    }
}

function setServoOffsets(values) {
    for (const i = 0; i < values.length; i++) {
        servoOffsets[i] = values[i];
    }
}

function onInit() {
    const eirEntry = (type, data) => [data.length + 1, type].concat(data);
    NRF.setAdvertising([
        eirEntry(0x9, DEVICE_NAME),
    ], { name: DEVICE_NAME });
    NRF.setServices(
        {
            // Battery Level
            0x180f: {
                0x2a19: {
                    value: new Uint8Array([0]),
                    description: 'Battery Level',
                    readable: true,
                    notify: true,
                },
            },

            // Servo Control
            0x5100: {
                0x5200: {
                    value: new Uint8Array(SERVOS_PINS.length),
                    description: 'Servo positions',
                    readable: true,
                    writable: true,
                    onWrite: (evt) => setServoValues(new Uint8Array(evt.data)),
                },
                0x5201: {
                    description: 'Servo offsets',
                    value: new Uint8Array(SERVOS_PINS.length * 4),
                    readable: true,
                    writable: true,
                    onWrite: (evt) => setServoOffsets(new Float32Array(evt.data)),
                },
            },
        },
        {
            advertise: ['180f', '5100'],
        },
    );

    NRF.on('connect', () => {
        updateBattery();
        startTask('battery', updateBattery, 10000);
    });
    NRF.on('disconnect', () => {
        stopTask('battery');
    });
}

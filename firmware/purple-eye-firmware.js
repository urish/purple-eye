const DEVICE_NAME = 'PurpleEye';

const SERVOS = [D2, D28, D29, D30];
const BATTERY_PIN = D4;

function getBatteryVoltage() {
    return analogRead(BATTERY_PIN) * NRF.getBattery() * 2;
}

function updateServos(values) {
    // TODO: calculate pulse length
    for (const i = 0; i < values.length; i++) {
        analogWrite(SERVOS[i], 0.025, { freq: 20 });
    }
}

function onInit() {
    const eirEntry = (type, data) => [data.length + 1, type].concat(data);
    NRF.setAdvertising(
        [
            eirEntry(0x9, DEVICE_NAME),
            // Physical-Web beacon
            [].concat(eirEntry(0x3, [0xaa, 0xfe]), [
                ,
                0xaa,
                0xfe, // Eddystone Service Id
                0x10, // Frame type: URL
                0xf8, // Power
                0x03, // https://
                'bit.do/prpl',
            ]),
        ],
        {
            name: DEVICE_NAME,
        },
    );
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
                    value: new Uint8Array(SERVOS.length),
                    description: 'Servo positions',
                    readable: true,
                    writable: true,
                    onWrite: (evt) => updateServos(new Uint8Array(evt.data)),
                },
                0x5201: {
                    description: 'Servo offsets',
                    value: new Uint8Array(SERVOS.length),
                    readable: true,
                    writable: true,
                },
            },
        },
        {
            advertise: ['180f', '5100'],
        },
    );

    // TODO sample battery periodically and update battery level service
}

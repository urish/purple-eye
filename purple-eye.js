'use strict';

let gattServer = null;
let servoCharacteristic = null;
let batteryCharacteristic = null;

function indicateBatteryLevel(value) {
    document.querySelector('.battery-level-text').textContent = value + '%';
    const batteryLevelIcon = document.querySelector('.battery-level > .fa');
    if (value > 85) {
        batteryLevelIcon.className = 'fa fa-battery-full';
    } else if (value > 65) {
        batteryLevelIcon.className = 'fa fa-battery-three-quarters';
    } else if (value > 40) {
        batteryLevelIcon.className = 'fa fa-battery-half';
    } else if (value > 20) {
        batteryLevelIcon.className = 'fa fa-battery-quarter';
    } else {
        batteryLevelIcon.className = 'fa fa-battery-empty';
    }
}

function connect() {
    console.log('Requesting Bluetooth Device...');
    navigator.bluetooth.requestDevice(
        { filters: [{ services: [0x5100] }], optionalServices: ['battery_service'] })
        .then(device => {
            console.log('> Found ' + device.name);
            console.log('Connecting to GATT Server...');
            return device.gatt.connect();
        })
        .then(server => {
            gattServer = server;
            console.log('Getting Service 0x5100 - Robot Control...');
            return server.getPrimaryService(0x5100);
        })
        .then(service => {
            console.log('Getting Characteristic 0x5200 - Servo Angles...');
            return service.getCharacteristic(0x5200);
        })
        .then(characteristic => {
            console.log('All ready!');
            servoCharacteristic = characteristic;
        })
        .then(() => {
            return gattServer.getPrimaryService('battery_service')
        })
        .then(service => {
            return service.getCharacteristic('battery_level');
        })
        .then(characteristic => {
            batteryCharacteristic = characteristic;
            return batteryCharacteristic.readValue();
        }).then(value => {
            indicateBatteryLevel(value.getUint8(0));
            return batteryCharacteristic.startNotifications();
        }).then(_ => {
            batteryCharacteristic.addEventListener('characteristicvaluechanged', e => {
                const batteryLevel = e.target.value.getUint8(0);
                indicateBatteryLevel(batteryLevel);
            });
            console.log('> Notifications started');
        })
        .catch(error => {
            console.log('Argh! ' + error);
        });
}

function writeServos(rightLegValue, rightFootValue, leftFootValue, leftLegValue) {
    var buffer = new ArrayBuffer(4);
    var view = new Int8Array(buffer);
    view[0] = rightLegValue;
    view[1] = rightFootValue;
    view[2] = leftFootValue;
    view[3] = leftLegValue;
    return servoCharacteristic.writeValue(buffer)
        .catch(err => console.log('Error when writing value! ', err));
}

function spread() {
    return writeServos(110, 94, 86, 70)
        .then(() => console.log('Spread successful'));
}

function stand() {
    return writeServos(90, 90, 90, 90)
        .then(() => console.log('Stand successful'));
}

function rest() {
    stopMoving();
    return writeServos(0, 0, 0, 0)
        .then(() => console.log('Rest successful'));
}

let dancing = false,
    shimming = false;

function shimmy() {
    var standing = true;
    stopMoving();
    shimming = true;

    function step() {
        var promise = standing ? stand() : spread();
        standing = !standing;
        promise.then(() => {
            if (shimming) {
                setTimeout(step, 150);
            }
        })
    }

    step();
}

function dance() {
    let delta = 0, direction = 1;
    stopMoving();
    dancing = true;

    function danceStep() {
        delta += direction * 2;
        if (delta > 20 || delta < -20) {
            direction = -direction;
        }
        writeServos(90 + delta, 90 + delta, 90 + delta, 90 + delta)
            .then(() => {
                if (dancing) {
                    setTimeout(danceStep, 10);
                }
            });
    }

    danceStep();
}

function stopMoving() {
    dancing = false;
    shimming = false;
}

'use strict';

let gattServer = null;
let servoCharacteristic = null;
let batteryCharacteristic = null;

var robotModel, arrow;

(function () {
    var scene = new THREE.Scene();

    var camera = new THREE.PerspectiveCamera(45, 1, 0.1, 20000);
    camera.position.set(60, 50, 0);
    camera.lookAt(scene.position);
    scene.add(camera);

    var light = new THREE.AmbientLight(0xffffff);
    light.position.set(50, 50, 0);
    scene.add(light);

    var loader = new THREE.ColladaLoader();
    loader.options.convertUpAxis = true;
    loader.load('./purple-eye.dae', function (collada) {
        robotModel = collada.scene;
        robotModel.scale.set(10, 10, 10);
        robotModel.getObjectByName('Eyeball').children[0].geometry.computeVertexNormals();
        scene.add(robotModel);
    });

    var plane = new THREE.Mesh(
        new THREE.PlaneBufferGeometry(100, 100),
        new THREE.MeshPhongMaterial({ color: 0x404040, specular: 0x101010 })
    );
    plane.rotation.x = -Math.PI / 2;
    plane.receiveShadow = true;
    scene.add(plane);

    var renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(300, 300);
    document.body.appendChild(renderer.domElement);

    function animate() {
        requestAnimationFrame(animate);
        renderer.render(scene, camera);
    }

    animate();
})();

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
        { filters: [{ services: [0x5100] }], optionalServices: ['battery_service', 0xff08] })
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
        .then(() => {
            return gattServer.getPrimaryService(0xff08);
        })
        .then(service => {
            return service.getCharacteristic(0xff09);
        })
        .then(imuCharacteristic => {
            imuCharacteristic.startNotifications();
            imuCharacteristic.addEventListener('characteristicvaluechanged', e => {
                var ax = e.target.value.getInt16(0, true);
                var az = e.target.value.getInt16(2, true);
                var ay = -e.target.value.getInt16(4, true);
                var mx = e.target.value.getInt16(6, true);
                var my = e.target.value.getInt16(8, true);
                var mz = e.target.value.getInt16(10, true);

                var gVector = new THREE.Vector3(az, ay, -ax);
                gVector.applyAxisAngle(new THREE.Vector3(0, 0, 1), -Math.PI);
                var yAxis = new THREE.Vector3(0, 1, 0);
                robotModel.quaternion.setFromUnitVectors(yAxis, gVector.clone().normalize());

                console.log('imu:', ax.toString(16), ay.toString(16), az.toString(16), mx, my, mz);
            });
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

var b = require('bonescript');
var inputPin = 'P8_19';
var pulses = 0;
b.pinMode(inputPin, b.INPUT);
b.attachInterrupt(inputPin, true, b.RISING, interruptCallback);
setTimeout(detach, 12000);

function interruptCallback(x) {
    console.log(JSON.stringify(x));
    pulses += 1;
    console.log('Got a pulse, pulses = ' + pulses);
}

function detach() {
    b.detachInterrupt(inputPin);
    console.log('Interrupt detached');
}


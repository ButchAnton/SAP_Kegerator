var b = require('bonescript');
var inputPin = 'P8_19';
var pulses = 0;
var pouring = false;
var timeNow = 0;
var timeOfLastPour = 0;

b.pinMode(inputPin, b.INPUT);
b.attachInterrupt(inputPin, true, b.RISING, interruptCallback);
// setTimeout(detach, 12000);

function interruptCallback(x) {
    
  pouring = true;
  pulses += 1;
  timeOfLastPour = new Date().getTime();
  console.log('Got a pulse, pulses = ' + pulses);
  console.log("Time of tick: " + timeOfLastPour);
}

function detach() {
  b.detachInterrupt(inputPin);
  console.log('Interrupt detached');
}

console.log(pulses);
console.log(timeOfLastPour);


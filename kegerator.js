var b = require('bonescript');
var ds18b20 = require('ds18b20');

const POUR_TIMEOUT = 250; // 250 miliseconds after a pour ends to call it a pour
const MIN_POUR_VOLUME = 1.0;  // 1oz is the minimum pour volume.
var inputPin = 'P8_19';
var pulses = 0;
var pouring = false;
var timeNow = 0;
var timePourIsOver = 0;

// Set up the flow meter

b.pinMode(inputPin, b.INPUT);
b.attachInterrupt(inputPin, true, b.RISING, interruptCallback);
// setTimeout(detach, 12000);
setInterval(checkAndReportPour, 500);
setInterval(heartbeat, 1000);
setInterval(postTemperature, 1000);
setInterval(postDoorSwitch, 1000);


function heartbeat() {
  console.log("I'm alive!  pulses = " + pulses + ".");
}

function postTemperature() {
  ds18b20.sensors(function (err, id) {
    console.log('Current temperature is ' + ds18b20.temperatureSync(id));
  });
}

function postDoorSwitch() {
}

function checkAndReportPour() {

  var pourVolumeInOunces = 0.0;

  if (true == pouring) {

    if ((new Date().getTime()) > timePourIsOver) {

      // Calculate the volume of the pour
      // There are 10200 pulses per gallon, and 128 ounces
      // per gallon.

      pourVolumeInOunces = (pulses / 10200.0) * 128.0;

      // Check the pour volume to see if it's a valid pour.

      if (pourVolumeInOunces > MIN_POUR_VOLUME) {

        console.log("Valid pour of " + pourVolumeInOunces + " ounces.");
        pouring = false;
        pulses = 0;
        pourVolumeInOunces = 0.0;

        // POST the data to HANA.
      }
    }
  }
}

function interruptCallback(x) {

  pouring = true;
  pulses += 1;
  timePourIsOver = new Date().getTime() + POUR_TIMEOUT;
  console.log('Got a pulse, pulses = ' + pulses);
}

function detach() {
  b.detachInterrupt(inputPin);
  console.log('Interrupt detached');
}


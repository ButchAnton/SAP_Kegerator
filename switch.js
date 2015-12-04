var b = require('bonescript');
var switchPin = 'P8_15';
var doorState = 1; // start with the door open

b.pinMode('P8_15', b.INPUT);
setInterval(readDoorSwitch,1000);

function readDoorSwitch(){
  b.digitalRead('P8_15', postDoorSwitch);
}

function postDoorSwitch(x) {
  if(x.value != doorState) {
    doorState = x.value;
    console.log("Door is now " + (doorState == 0) ? "closed." : "open.");
  }
}

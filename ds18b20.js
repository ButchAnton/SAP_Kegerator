// var io = require('/mnt/sd/apache/node_modules/socket.io').listen(3000);
var ds18b20 = require('ds18b20');
var interval = 1000; //enter the time between sensor queries here (in milliseconds)
setInterval(readTemperature, interval);

function readTemperature() {
  ds18b20.sensors(function (err, id) {
    console.log('Current temperature is ' + ds18b20.temperatureSync(id));
  });
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var url = 'https://api.open-meteo.com/v1/forecast?latitude=' +
        pos.coords.latitude + '&longitude=' + pos.coords.longitude +
        '&current=temperature_2m&temperature_unit=fahrenheit';

      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        var json = JSON.parse(this.responseText);
        var temperature = Math.round(json.current.temperature_2m);
        Pebble.sendAppMessage({'TEMPERATURE': temperature},
          function(e) { console.log('Weather sent successfully!'); },
          function(e) { console.log('Error sending weather!'); }
        );
      };
      xhr.open('GET', url);
      xhr.send();
    },
    function(err) { console.log('Error requesting location!'); },
    {timeout: 15000, maximumAge: 60000}
  );
}

Pebble.addEventListener('ready', function(e) {
  getWeather();
});

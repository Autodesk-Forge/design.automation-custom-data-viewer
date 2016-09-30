var express = require('express');

var app = express();
app.use(function(req, res, next) {
  res.header("Access-Control-Allow-Origin", "*");
  res.header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  next();
});


app.use('/', express.static(__dirname + '/../src/www'));
app.set('port', 8080);

var server = app.listen(app.get('port'), function() {
    console.log('Server listening on port ' + server.address().port);
});


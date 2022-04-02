var express = require('express');
var app = express();
multer = require('multer');

app.use(express.urlencoded({extended: false}));
//handle static files
app.use(express.static('contents')); //Serves resources from public folder
app.post('/data/:tname',multer().none(), function(req,res,next){ data=JSON.stringify(req.body);res.send(data);console.log(req.params.tname);console.log(data); console.log(req); });

var server = app.listen(8080);


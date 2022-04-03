const {
  parse,
  stringify,
  assign
} = require('comment-json');
var fs = require('fs');
var express = require('express');
var app = express();
multer = require('multer');

app.use(express.urlencoded({extended: false}));
//handle static files
app.use(express.static('contents')); //Serves resources from public folder
app.use('/data',express.static('data'));
app.post('/data/:tname',multer().none(), function(req,res,next){
         console.log(req.body);
         data=stringify(req.body);
         data=parse(data);
         save(data,"./data/" + req.params.tname);
         res.sendFile(__dirname + "/data/" + req.params.tname);
         });

var server = app.listen(8080);


function save(item, path = './collection.json'){
    console.log("save: " + path + " - " + item);
    if (!fs.existsSync(path)) {
        fs.writeFile(path, "[" + item + "]");
    } else {
        var data = fs.readFileSync(path, 'utf8');  
        console.log(data);
        var list = parse(data);
        for(var i = 0; i < list.length; i++){
          console.log(list[i]);
          console.log(item.id);
          if (item.id == list[i].id){
            console.log("Updating item");
            list[i] = item;
            updated = true;
            }
          }
       result =stringify(list,null,2);
       console.log(result);
       fs.writeFileSync(path, result);
       } 
}


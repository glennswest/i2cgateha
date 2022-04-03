var fs = require('fs');
var express = require('express');
var app = express();
multer = require('multer');

app.use(express.urlencoded({extended: false}));
//handle static files
app.use(express.static('contents')); //Serves resources from public folder
app.use(express.static('data'));
app.post('/data/:tname',multer().none(), function(req,res,next){
         data=JSON.stringify(req.body);
         save(data,req."data/"+params.tname);
         res.sendFIle(params.tname);
         });

var server = app.listen(8080);


function save(item, path = './collection.json'){
    if (!fs.existsSync(path)) {
        fs.writeFile(path, JSON.stringify([item]));
    } else {
        var data = fs.readFileSync(path, 'utf8');  
        var list = (data.length) ? JSON.parse(data): [];
        if (list instanceof Array) list.push(item)
        else list = [item]  
        fs.writeFileSync(path, JSON.stringify(list));
    }
}

function save(item, path = './collection.json'){
    console.log("save: " + path + " - " + item);
    if (!fs.existsSync(path)) {
        fs.writeFile(path, "[" + JSON.stringify([item]) + "]");
    } else {
        var data = fs.readFileSync(path, 'utf8');  
        var list = JSON.parse(data);
        for(var i = 0; i < list.length; i++){
          if (item.id = list[i].id){
            console.log("Updating item");
            list[i] = item;
            updated = true;
            }
          }
       fs.writeFileSync(path, JSON.stringify(list));
       } 
}


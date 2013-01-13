var html2xhtml = require('./lib/html2xhtml.js');

//read some data
var data = require('fs').readFileSync('index.html');

//make api call
html2xhtml.convert(data,function(fixed){

	//write converted data
    require('fs').writeFileSync('index2.html',fixed);
    
});
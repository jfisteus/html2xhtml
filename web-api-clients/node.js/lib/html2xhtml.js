/*
html2xhtml.js
*/
exports.convert = function(data,callback){
	var options = {
		host: 'www.it.uc3m.es',
		port: 80,
		path: '/jaf/cgi-bin/html2xhtml.cgi',
		method: 'POST'
	};
	
	options.headers = {
		'Content-Type': 'text/html',
		'Content-Length': data.length
	};
	
	var req = require('http').request(options, function(res) {
		res.setEncoding('utf8');
		var body = '';
		res.on('data', function (chunk) {
			body = body + chunk;
		});
		res.on('error',function(err){
			console.log(err);
		});
		res.on('end',function(){
			if(callback){
				callback(body);
			}
		});
	});
	req.on('error',function(err){
		console.log(err);
	});
	req.write(data);
	req.end();
}

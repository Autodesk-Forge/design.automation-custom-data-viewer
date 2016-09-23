var app = require("./app.js");

////////////////////////////////
// test code
////////////////////////////////
var event0 = 
{
    "output": ""
};
var context = 
 {
    "callbackWaitsForEmptyEventLoop" : false
};
app.handler(event0, context, function (err) {
});

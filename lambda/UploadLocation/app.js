
var AWS = require("aws-sdk");
var uuid = require('uuid');

// local files
var config = require("./config.js");

exports.handler = handleEvent;

function handleEvent(event, context, callback) {
    
    // The drawing name is passed as part of the input request
    var drawingName = event.dwgname;
    if (drawingName) {
        drawingName = decodeURIComponent(drawingName);
    } else {
        drawingName = "input.dwg";
    }

    // Create a unique key
    var key = "drawings/" + uuid.v4() + "/" + drawingName;
    
    var s3 = new AWS.S3({ accessKeyId: config.aws_key, secretAccessKey: config.aws_secret });
    var params = { Bucket: config.aws_s3_bucket, Key: key };
    var url = s3.getSignedUrl('putObject', params);

    // Return the presigned url
    if (url) {
        context.callbackWaitsForEmptyEventLoop = false;
        var param = { Result : url };
        callback(null, param);
    } else {
        context.callbackWaitsForEmptyEventLoop = false;
        callback("Could not process the request", "Error message");
    }
};

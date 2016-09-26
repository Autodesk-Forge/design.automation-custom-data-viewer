
var AWS = require("aws-sdk");
var request = require('request');
var uuid = require('uuid');
var unzip = require('unzip');
var stream = require('stream');
var path = require('path');

// local files
var config = require("./config.js");


exports.handler = handleEvent;

function handleEvent(event, context, callback) {       
        
    var url = event.output;
    if (!url) {
        callback("Error: Invalid url");
        return;
    }

    var folderName = "files/" + uuid.v4() + "/";
   
    // The function gets the result output by the Design Automation API, 
    // which is a compressed form. It uncompresses the result, uploads 
    // the files to a unique location on S3, and returns the location 
    // of SVF/F2D files, along with the location of the xdata.
    //
    uploadWorkItemResult(url, folderName, function (status, resultlocation, xdatalocation) {
        if (status) {
            context.callbackWaitsForEmptyEventLoop = false;
            var param = { Result : resultlocation, CustomData : xdatalocation };
            callback(null, param);                       
        }
        else {
            context.callbackWaitsForEmptyEventLoop = false;
            var errormsg = "Could not process the request";
            if (resultlocation)
                errormsg = resultlocation;
            callback(errormsg, "Error message");            
        }
    }); 
}

// Get the result and process it. 
// 
function uploadWorkItemResult(url, folderName, callback) {
    if (!url) {
        callback(false);
    }
    
    getBinaryData(url, function (status, body) {
        if (status == 200) {
            uploadResultToS3(body, folderName, function (status, resultlocation, xdatalocation) {
                if (status) {
                    callback(true, resultlocation, xdatalocation);
                } else {
                    console.log("Error: failed to upload the results to S3");
                    callback(false);
                }
            });
        }
        else {
            callback(false);
        }
    });
}

// The function uncompresses the data, and upload the result to S3.
// In the process it also stores the location of SVF/F2D and xdata, 
// and returns it to the caller.
//
function uploadResultToS3(body, folderName, callback) {
    
    var s3 = new AWS.S3({ accessKeyId: config.aws_key, secretAccessKey: config.aws_secret });
    
    var areEqual = false;
    var resultLocation;
    var location;
    var customdatalocation;
    var count = 0;
    var bufferStream = new stream.PassThrough();
    bufferStream.end(body);
    bufferStream.pipe(unzip.Parse())
    .on('entry', function (entry) {
        ++count;
        var fileName = entry.path;
        
        var index = fileName.indexOf("\\");
        if (index != -1) {
            fileName = fileName.replace("\\", "/");
        }

        var key = folderName + fileName;
        
        var params = { Bucket: config.aws_s3_bucket, Key: key, ACL: 'public-read', Body: entry };
        s3.upload(params, function (err, data) {
            if (err) {
                console.log('Error: uploading' + filename + ' to S3' + ': ' + err);
                entry.autodrain();
                callback(false);
                return;
            }
            else {
                resultLocation = path.parse(data.Location);
                if (resultLocation) {
                    if (resultLocation.ext.toUpperCase() === '.SVF') {
                        location = data.Location;
                    } else if (resultLocation.ext.toUpperCase() === '.F2D') {
                        location = data.Location;
                    } else {
                        if (resultLocation.base.toUpperCase() === 'XDATA.JSON') {
                            customdatalocation = data.Location;
                        }
                    }
                }
                count--;
                console.log('Successfully uploaded ' + count);
                if (count == 0)
                    callback(true, location, customdatalocation);
            }
        });
    });
}

// Helper function to get the binary data
//
function getBinaryData(uri, callback) {
    request({
        url: uri,
        method: 'GET',
        encoding: null
    }, function (error, response, body) {
        if (callback)
            callback(error || response.statusCode, body);
        else {
            if (error) {
                console.log(error);
            } else {
                console.log(response.statusCode, body);
            }
        }
    });
}

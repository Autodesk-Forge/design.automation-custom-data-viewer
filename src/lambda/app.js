
var AWS = require("aws-sdk");
var request = require('request');
var uuid = require('uuid');
var unzip = require('unzip');
var async = require('async');
var fs = require('fs');
var stream = require('stream');
var path = require('path');

// local files
var config = require("./config.js");

// The function returns a pre-signed url location that the client can upload the drawing
//
module.exports.uploadlocation = (event, context, callback) => {
    
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
        var param = { Result: url };
        callback(null, param);
    } else {
        context.callbackWaitsForEmptyEventLoop = false;
        callback("Could not process the request", "Error message");
    }
};

// The function takes the activity name, and the location of the drawing file as input. 
// It checks if the custom package and the activity are present. 
// And then the workitem is then submitted using the DesignAutomation API, configured to run the 
// activity that was passed.
//
module.exports.submitworkitem = (event, context, callback) => {

    // Get the activity name from the request
    //
    var activityname = event.activityname;

    if (!activityname) {
        callback("Error: Invalid activity name", "Error message");
        return;
    }

    if (!event.dwglocation) {
        callback("Error: Invalid drawing location");
        return;
    }

    var key = event.dwglocation;
    var strtoken = "amazonaws.com/";
    key = key.substring(key.indexOf(strtoken) + strtoken.length);
    key = key.substring(0, key.indexOf("?"));
    if (!key) {
        callback("Invalid pre-signed url");
        return;
    }

    key = decodeURIComponent(key);

    // Create a presigned url for GET that needs to be passed to DesignAutomation API
    //
    var s3 = new AWS.S3({ accessKeyId: config.aws_key, secretAccessKey: config.aws_secret });
    var params = { Bucket: config.aws_s3_bucket, Key: key };
    var url = s3.getSignedUrl('getObject', params);

    if (!url) {
        context.callbackWaitsForEmptyEventLoop = false;
        callback("Could not process the request", "Error message");
        return;
    }

    getToken(config.auth_endpoint, config.developer_key, config.developer_secret, function (token) {
        if (!token) {
            callback("Error: Failed to obtain the access token");
            return;
        }

        submitItem(token, url, activityname, function (ret, workitemId) {
            if (ret) {
                context.callbackWaitsForEmptyEventLoop = false;
                var param = { Result: workitemId };
                callback(null, param);
            } else {
                context.callbackWaitsForEmptyEventLoop = false;
                callback("Could not process the request", "Error message");
            }
        });
    });
}

// The function takes the workitem identifier as the input, and checks the 
// status of the respective workitem. The function uses the DesignAutomation 
// API to poll the status. It returns immediately on success or failure of the 
// workitem. If the workitem status is pending or in-progress, it loops through, 
// polling the status every two seconds for up to twenty seconds and then returns. 
// The reason it returns after twenty seconds is due to a thirty second timeout 
// limit set by the AWS API gateway. If the API does not respond within thirty 
// seconds, then the request is terminated and an error status of 504 is returned 
// by the AWS API gateway.
//
module.exports.workitemstatus = (event, context, callback) => {

    // Get the workitem id from the request
    //
    var workitemId = event.workitemid;
    if (!workitemId) {
        callback("Error: Invalid workitem id");
        return;
    }
    workitemId = decodeURIComponent(workitemId);
    var url = config.workitem_endpoint + "(\'" + workitemId + "\')";

    getToken(config.auth_endpoint, config.developer_key, config.developer_secret, function (token) {

        getWorkItemStatus(url, token, function (status, statustext, body) {
            if (status) {
                context.callbackWaitsForEmptyEventLoop = false;
                var param = { StatusText: statustext, Result: body };
                callback(null, param);
            } else {
                context.callbackWaitsForEmptyEventLoop = false;
                var errormsg = "Error getting workitem status";
                if (body)
                    errormsg = body;
                callback(errormsg, "Error message");
            }
        });
    });
}

// The function takes the location of the result posted by DesignAutomation API 
// for the workitem that was submitted. The result which is a compressed file 
// containing the output is uncompressed by the function and uploaded to a unique 
// location on S3. The location of the SVF/F2D and the XData files are returned.
//
module.exports.processresult = (event, context, callback) => {

    var url = event.output;
    if (!url) {
        console.log("Error: Invalid url");
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
            var param = { Result: resultlocation, CustomData: xdatalocation };
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


// Helper functions


// The function creates the app package and the custom activity if not available and then submits the 
// workitem for the given drawing.
//
function submitItem(token, dwgLocation, activityname, callback) {

    isPackageAvailable(config.package_endpoint, token, config.package_name, function (status) {
        if (status) {
            isActivityAvailable(config.activity_endpoint, token, activityname, function (status) {
                if (status) {
                    submitWorkItem(config.workitem_endpoint, token, activityname, dwgLocation, function (ret, workitemId) {
                        if (ret) {
                            console.log("Sucessfully submitted the workitem");
                            callback(true, workitemId);
                        } else {
                            console.log("Error: submitting the workitem");
                            callback(false);
                        }
                    });
                } else {
                    console.log("Error: Activity " + activityname + " not available ");
                    callback(false);
                    return;
                }
            });
        } else {
            console.log("Error: AppPackage " + config.package_name + " not available ");
            callback(false);
            return;
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

// The function gets the status of the workitem using the DesignAutomation API,
// loops through to a count of ten if the status is pending with a time interval 
// of two seconds. The AWS API gateway timesout at 30s, and so it keeps the loop 
// below thirty seconds after which it returns the pending status to the client.
// It returns the status immediately on success or failure.
//
function getWorkItemStatus(url, token, callback) {
    var count = 0;
    async.forever(function (next) {
        setTimeout(sendAuthData(url, 'GET', token, null, function (status, body) {
            if (status == 200) {
                var result = JSON.parse(body);
                if (result.Status == "Pending" || result.Status == "InProgress") {
                    if (count > 10) {
                        // Exit at count 10, we do not want to run over 20s                        
                        next({ Status: true, StatusText: "Pending" });
                        return;
                    }

                    next();
                    ++count;
                } else if (result.Status == "Succeeded") {
                    next({ Status: true, StatusText: "Succeeded", Result: { Output: result.Arguments.OutputArguments[0].Resource, Report: result.StatusDetails.Report } });
                }
                else {
                    next({ Status: true, StatusText: "Failed", Result: { Report: result.StatusDetails.Report } });
                }
            }
            else {
                next({ Status: false, StatusText: "Failed", Result: body });
            }
        }), 2000)
    }, function (obj) {
        if (obj.Status) {
            callback(true, obj.StatusText, obj.Result);
        } else {
            callback(false, obj.StatusText, obj.Result);
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
                console.log('Error: uploading to S3' + ': ' + err);
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

// Helper function to check if the given app package is available
//
function isPackageAvailable(url, token, packagename, callback) {
    var uri = url + "(\'" + packagename + "\')";
    sendAuthData(uri, 'GET', token, null, function (status) {
        if (status == 200)
            callback(true);
        else
            callback(false);
    });
}

// The function checks if the activity with the given name is available
//
function isActivityAvailable(url, token, activityname, callback) {
    var uri = url + "(\'" + activityname + "\')";
    sendAuthData(uri, 'GET', token, null, function (status) {
        if (status == 200)
            callback(true);
        else
            callback(false);
    });
}

// The functions submits a workitem using the Design Automation API.
//
function submitWorkItem(url, token, activityname, resource, callback) {
    var workitembody = {
        "@odata.type": "#ACES.Models.WorkItem",
        "ActivityId": activityname,
        "Arguments": {
            "@odata.type": "#ACES.Models.Arguments",
            "InputArguments@odata.type": "#Collection(ACES.Models.Argument)",
            "InputArguments": [
                {
                    "@odata.type": "#ACES.Models.Argument",
                    "Headers@odata.type": "#Collection(ACES.Models.Header)",
                    "Headers": [],
                    "HttpVerb@odata.type": "#ACES.Models.HttpVerbType",
                    "HttpVerb": null,
                    "Name": "HostDwg",
                    "Resource": resource,
                    "ResourceKind@odata.type": "#ACES.Models.ResourceKind",
                    "ResourceKind": null,
                    "StorageProvider@odata.type": "#ACES.Models.StorageProvider",
                    "StorageProvider": "Generic"
                },
                {
                    "@odata.type": "#ACES.Models.Argument",
                    "Headers@odata.type": "#Collection(ACES.Models.Header)",
                    "Headers": [],
                    "HttpVerb@odata.type": "#ACES.Models.HttpVerbType",
                    "HttpVerb": null,
                    "Name": "Params",
                    "Resource": "data:application/json, ",
                    "ResourceKind@odata.type": "#ACES.Models.ResourceKind",
                    "ResourceKind": "Embedded",
                    "StorageProvider@odata.type": "#ACES.Models.StorageProvider",
                    "StorageProvider": "Generic"
                }
            ],
            "OutputArguments@odata.type": "#Collection(ACES.Models.Argument)",
            "OutputArguments": [
                {
                    "@odata.type": "#ACES.Models.Argument",
                    "Headers@odata.type": "#Collection(ACES.Models.Header)",
                    "Headers": [],
                    "HttpVerb@odata.type": "#ACES.Models.HttpVerbType",
                    "HttpVerb": "POST",
                    "Name": "Results",
                    "Resource": null,
                    "ResourceKind@odata.type": "#ACES.Models.ResourceKind",
                    "ResourceKind": "ZipPackage",
                    "StorageProvider@odata.type": "#ACES.Models.StorageProvider",
                    "StorageProvider": "Generic"
                }
            ]
        },
        "AvailabilityZone@odata.type": "#ACES.Models.DataAffinity",
        "AvailabilityZone": null,
        "BytesTranferredIn": null,
        "BytesTranferredOut": null,
        "Id": "",
        "Status@odata.type": "#ACES.Models.ExecutionStatus",
        "Status": null,
        "StatusDetails": null,
        "TimeInputTransferStarted": null,
        "TimeOutputTransferEnded": null,
        "TimeQueued": null,
        "TimeScriptEnded": null,
        "TimeScriptStarted": null,
        "Timestamp": "0001-01-01T00:00:00Z"
    };

    sendAuthData(url, 'POST', token, workitembody, function (status, param) {
        if (status == 200 || status == 201) {
            console.log("Submitted work item");
            var paramObj = JSON.parse(param);
            callback(true, paramObj.Id);
        } else {
            console.log("Error: submitting workitem");
            callback(false);
        }
    });
}

// Sends a request with the authorization token
//
function sendAuthData(uri, httpmethod, token, data, callback) {

    var requestbody = "";
    if (data)
        requestbody = JSON.stringify(data);

    request({
        url: uri,
        method: httpmethod,
        headers: {
            'Content-Type': 'application/json',
            'Authorization': token
        },
        body: requestbody
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

// Sends a normal request
//
function sendData(uri, httpmethod, content, callback) {
    request({
        url: uri,
        method: httpmethod,
        body: content
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

// Helper function to download binary data
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

// Helper function to get the access token
//
function requestToken(authurl, key, secret, callback) {
    request({
        url: authurl,
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: "client_id=" + key + "&client_secret=" + secret + "&grant_type=client_credentials" + "&scope=code:all"
    }, function (error, response, body) {
        if (callback) {
            var token = '';
            var bodyObj = {};
            if (body)
                bodyObj = JSON.parse(body);
            if (bodyObj.token_type && bodyObj.access_token)
                token = bodyObj.token_type + ' ' + bodyObj.access_token;
            callback(error, token);
        }
        else {
            if (error) {
                console.log(error);
            } else {
                console.log(response.statusCode, body);
            }
        }
    });
}

function getToken(url, key, secret, callback) {
    requestToken(url, key, secret, function (err, token) {
        if (err) {
            console.log("Error: " + err.toString());
        }
        callback(token);
    });
}

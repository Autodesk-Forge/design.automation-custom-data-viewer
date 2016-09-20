
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


exports.handler = handleEvent;

function handleEvent(event, context, callback) {       
    
    var actlength = config.activities.length;
    var activityname;
    var script;
    for (var i = 0; i < actlength; i++) {
        if (config.activities[i].name === event.activityname) {
            activityname = config.activities[i].name;
            script = config.activities[i].script;
            break;
        }
    }
    
    if (!activityname) {
        console.log("Error: Invalid activity name");
        return;
    }
    
    var dwgLocation = event.dwglocation;
    var folderName = "Test/" + uuid.v4() + "/";
    
    var key = dwgLocation;
    var strtoken = "amazonaws.com/";
    key = key.substring(key.indexOf(strtoken) + strtoken.length);
    key = key.substring(0, key.indexOf("?"));
    key = decodeURIComponent(key);
    
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
            console.log("Error: Failed to obtain the access token");
            return;
        }

        start(token, url, activityname, script, folderName, function (ret, resultlocation, xdatalocation) {
            if (ret) {
                context.callbackWaitsForEmptyEventLoop = false;
                var param = { Result : resultlocation, CustomData : xdatalocation };
                callback(null, param);
            } else {
                context.callbackWaitsForEmptyEventLoop = false;
                callback("Could not process the request", "Error message");
            }
        });

    });
    
}


function start(token, dwgLocation, activityname, script, folderName, callback) {
            
    createPackage(token, config.package_name, config.package_endpoint, config.package_source_endpoint, config.package_upload_endpoint, function (ret) {
        if (ret) {
            console.log("Successfully created the app package");
            createActivity(config.activity_endpoint, token, config.package_name, config.refpackage_name, activityname, script, function (ret) {
                if (ret) {
                    submitWorkItem(config.workitem_endpoint, token, activityname, dwgLocation, folderName, function (ret, resultlocation, xdatalocation) {
                        if (ret) {
                            console.log("Sucessfully submitted the workitem");
                            callback(true, resultlocation, xdatalocation);
                        } else {
                            console.log("Error: submitting the workitem");
                            callback(false);
                        }
                    });

                } else {
                    callback(false);
                    return;
                }
            });
        } else {
            console.log("Error: failed creating the app package");
            callback(false);
        }
    });


}

function isPackageAvailable(url, token, packagename, callback) {
    var uri = url + "(\'" + packagename + "\')";
    sendAuthData(uri, 'GET', token, null, function (status) {
        if (status == 200)
            callback(true);
        else
            callback(false);
    });
}

function deletePackage(url, token, packagename, callback) {
    var uri = url + "(\'" + packagename + "\')";
    sendAuthData(uri, 'DELETE', token, null, function (status) {
        if (status == 200 || status == 204)
            callback(true);
        else
            callback(false);
    });
}

function createPackage(token, packagename, packageurl, packagesourceurl, packageuploadurl, callback) {
    
    // Check if the package already exists, and if does exist, return.
    isPackageAvailable(packageurl, token, packagename, function (status) {
        if (status) {
            callback(true);
            return;
        } else {

            getpackageuploadurl(packageuploadurl, token, function (value) {
                if (value) {
                    uploadPackage(packagesourceurl, value, token, function (status) {
                        if (status) {
                            var uploadbody = {
                                "@odata.type": "#ACES.Models.AppPackage",
                                "Description": null,
                                "Id": packagename,
                                "IsObjectEnabler": false,
                                "IsPublic": false,
                                "References@odata.type": "#Collection(String)",
                                "References": [],
                                "RequiredEngineVersion": config.required_engineversion,
                                "Resource": value,
                                "Timestamp": "0001-01-01T00:00:00Z",
                                "Version": 0
                            };
                            
                            sendAuthData(packageurl, 'POST', token, uploadbody, function (status, body) {
                                if (status == 200 || status == 201) {
                                    callback(true);
                                }
                                else {
                                    callback(false);
                                }
                            });
                        }
                        else {
                            console.log("Error: Could not upload the package data");
                            callback(false);
                        }
                    });
                }
                else {
                    console.log("Error: Could not obtain the upload url");
                    callback(false);
                }
            });
        }
    });
    
}


function getpackageuploadurl(url, token, callback) {
    sendAuthData(url, 'GET', token, null, function (status, body) {
        if (status == 200) {
            var bodyObj = JSON.parse(body);
            callback(bodyObj.value);
        }
        else {
            callback(null);
        }
    });
}

function uploadPackage(packagessourceurl, url, token, callback) {
    getBinaryData(packagessourceurl, function (status, body) {
        if (status == 200) {
            sendData(url, 'PUT', body, function (status, body) {
                if (status == 200) {
                    callback(true);
                } else {
                    callback(false);
                }
            });
        }
        else {
            callback(false);
        }
    });
}


function isActivityAvailable(url, token, activityname, callback) {
    var uri = url + "(\'" + activityname + "\')";
    sendAuthData(uri, 'GET', token, null, function (status) {
        if (status == 200)
            callback(true);
        else
            callback(false);
    });
}

function deleteActivity(url, token, activityname, callback) {
    var uri = url + "(\'" + activityname + "\')";
    sendAuthData(uri, 'DELETE', token, null, function (status) {
        if (status == 200 || status == 204)
            callback(true);
        else
            callback(false);
    });
}

function createActivity(url, token, packagename, refpackagename, activityname, script, callback) {
    
    // Check if the activity already exists, return if it does exist.
    isActivityAvailable(url, token, activityname, function (ret) {
        if (ret) {
            callback(true);
            return;
        } else {

            var activitybody = {
                "@odata.type": "#ACES.Models.Activity",
                "AllowedChildProcesses@odata.type": "#Collection(ACES.Models.AllowedChildProcess)",
                "AllowedChildProcesses": [],
                "AppPackages@odata.type": "#Collection(String)",
                "AppPackages": [],
                "Description": null,
                "HostApplication": null,
                "Id": "",
                "Instruction": {
                    "@odata.type": "#ACES.Models.Instruction",
                    "CommandLineParameters": null,
                    "Script": ""
                },
                "IsPublic": false,
                "Parameters": {
                    "@odata.type": "#ACES.Models.Parameters",
                    "InputParameters@odata.type": "#Collection(ACES.Models.Parameter)",
                    "InputParameters": [
                        {
                            "@odata.type": "#ACES.Models.Parameter",
                            "LocalFileName": "$(HostDwg)",
                            "Name": "HostDwg",
                            "Optional": null
                        },
                        {
                            "@odata.type": "#ACES.Models.Parameter",
                            "LocalFileName": "params.json",
                            "Name": "Params",
                            "Optional": null
                        }
                    ],
                    "OutputParameters@odata.type": "#Collection(ACES.Models.Parameter)",
                    "OutputParameters": [
                        {
                            "@odata.type": "#ACES.Models.Parameter",
                            "LocalFileName": "result",
                            "Name": "Results",
                            "Optional": null
                        }
                    ]
                },
                "RequiredEngineVersion": config.required_engineversion,
                "Timestamp": "0001-01-01T00:00:00Z",
                "Version": 0
            };
            
            activitybody.Id = activityname;
            activitybody.AppPackages.push(packagename);
            activitybody.AppPackages.push(refpackagename);
            activitybody.Instruction.Script = script;
            
            sendAuthData(url, 'POST', token, activitybody, function (status, body) {
                if (status == 200 || status == 201) {
                    console.log("Created activity");
                    callback(true);
                } else {
                    console.log("Error: Creating activity");
                    callback(false);
                }
            });
        }
    });

}


function submitWorkItem(url, token, activityname, resource, folderName, callback) {
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
            getWorkItemStatus(url, paramObj.Id, token, function (status, param) {
                if (status) {
                    var resultObj = JSON.parse(param);
                    uploadWorkItemResult(resultObj.Arguments.OutputArguments[0].Resource, folderName, function (status, resultlocation, xdatalocation) {
                        if (status) {
                            console.log("successfully uploaded the results");
                            callback(true, resultlocation, xdatalocation);
                        }
                        else {
                            callback(false);
                        }
                    });

                } else {
                    callback(false);
                }
            });
        } else {
            console.log("Error: submitting workitem");
            callback(false);
        }
    });
}

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
        // Remove any folders and upload them to a single folder, if not the Viewer will balk.
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


function getWorkItemStatus(workitemurl, id, token, callback) {
    var url = workitemurl + "(\'" + id + "\')";
    
    async.forever(
        function (next) {
            setTimeout(sendAuthData(url, 'GET', token, null, function (status, body) {
                if (status == 200) {
                    var result = JSON.parse(body);
                    if (result.Status == "Pending" || result.Status == "InProgress") {
                        next();
                    } else if (result.Status == "Succeeded") {
                        next({ Status: true, Result: body });
                    }
                    else {
                        next({ Status: false, Result: body });
                    }
                }
                else {
                    next({ Status: false, Result: body });
                }
            }), 2000)
        }, function (obj) {
            if (obj.Status) {
                callback(true, obj.Result);
            } else {
                callback(false, obj.Result);
            }
        });
    
}


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

// expect to get a callback(err, data)
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

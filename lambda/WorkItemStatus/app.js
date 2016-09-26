
var AWS = require("aws-sdk");
var request = require('request');
var async = require('async');

// local files
var config = require("./config.js");

exports.handler = handleEvent;

function handleEvent(event, context, callback) {   
    
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
                var param = { StatusText : statustext, Result : body };
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
                        next({ Status: true, StatusText: "Succeeded", Result: { Output: result.Arguments.OutputArguments[0].Resource, Report : result.StatusDetails.Report } });
                    }
                    else {
                        next({ Status: true, StatusText: "Failed", Result: { Report : result.StatusDetails.Report } });
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

// Helper function to send authenticated request
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

// Helper function to get the access token.
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
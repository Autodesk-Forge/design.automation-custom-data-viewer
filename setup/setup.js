var AWS = require("aws-sdk");
var uuid = require('uuid');
var process = require('process');
var fs = require('fs');
var readline = require('readline');
const exec = require('child_process').exec;

var configfile = '..\\src\\lambda\\config.js';
var packagefiledir = '..\\src\\bin';
var packagefile = 'package.zip';
var deployfile = '..\\src\\lambda\\deploy.bat';
var indexfile = '..\\src\\www\\index.html';
var startmarker = 'endpoints:';
var endmarker = 'functions:';

var variables = ['AWS_ACCESS_KEY_ID', 'AWS_SECRET_ACCESS_KEY', 'AWS_DEFAULT_REGION',
                 'DA_VIEWER_DWG_BUCKET', 'ADSK_DEVELOPER_KEY', 'ADSK_DEVELOPER_SECRET'];

(function()
{
    console.log("Staring installation of the service...");
    var initialized = true;
    variables.forEach(function (val) {
        if (!process.env[val]) {
            console.log(val + ' environment variable not set');
            initialized = false;
        }
    });

    if (!initialized) {
        console.log('\n');
        console.log('Please set the value of environment variables in init.bat, and run the batch file');
        return;
    }

    var packagepath = packagefiledir + '\\' + packagefile;
    if (!fs.existsSync(packagepath)) {
        console.error('Error: missing file ' + packagepath);
        return;
    }

    console.log("Upload the App package...");
    // Upload the package.zip file to S3
    var packagebucketname = process.env['DA_VIEWER_DWG_BUCKET'] + '-package';
    uploadAppPackage(packagepath, packagefile, packagebucketname, function (status, signedurl) {
        if (!status) {
            return;
        }

        console.log("Update the config file...");
        // Update the config file
        //
        updateConfigFile(signedurl, function (status) {
            if (!status) {
                return;
            }

            console.log("Deploying the service, this will take sometime");
            // Start the deployment
            //
            execbatchfile(deployfile, function (status, text) {
                if (!status) {
                    console.log("Error deploying to AWS");
                    return;
                }

                // Update the html file
                //
                var output = text.toString();
                console.log(output);
                console.log("Update the html file...");
                updateUrl(output, function (status) {
                    if (status) {
                        console.log('Successfully updated index.html');
                    }
                });
            });
        });       
    });
    
})();

// Creates an S3 bucket for a package and upload the package
//
function uploadAppPackage(packagepath, packagefile, bucketname, callback) {

    AWS.config.region = 'us-west-2';
    var s3 = new AWS.S3({ params: { Bucket: bucketname } });

    s3.createBucket(function (err, data) {        
        if (err) {
            if (err.code !== 'BucketAlreadyExists' && err.code !== 'BucketAlreadyOwnedByYou') {
                callback(false);
                return;
            }
        }

        var data = fs.readFileSync(packagepath);
        if (!data) {
            console.log("Error reading file " + packagepath);
            callback(false);
            return;
        }

        var key = "package/" + packagefile;
        var params = { Bucket: bucketname, Key: key, Body: data };

        s3.upload(params, function (err, data) {
            if (err) {
                console.log('Error: uploading to S3' + ': ' + err);
                callback(false);
                return;
            }

            params = { Bucket: bucketname, Key: key };
            var url = s3.getSignedUrl('getObject', params);

            if (!url) {
                console.log('Error: getting pre-signed url ' + ': ' + err);
                callback(false);
                return;
            }

            callback(true, url);
        });
    });
}


// Updates the config file with actual values
//
function updateConfigFile(signedurl, callback) {

    var file = fs.createReadStream(configfile, 'utf8');
    var newdata = '';    
    file.on('data', function (data) {
        var updatedata = data;
        variables.forEach(function (val) {
            updatedata = updatedata.toString().replace(val, process.env[val]);
        });
        
        updatedata = updatedata.toString().replace('package_source_endpoint_value', signedurl);
        newdata += updatedata;
    });

    file.on('end', function () {
        fs.writeFile(configfile, newdata, function (err) {
            if (err) {
                console.log(err);
                callback(false);
                return;
            }

            callback(true);
        });
    });
}

// Helper function to execute the batch file
//
function execbatchfile(filename, callback) {

    exec(filename, (err, stdout, stderr) => {
        if (err) {
            console.error(err);
            callback(false, null);
            return;
        }

        callback(true, stdout);
    });
}

// Updates the webpage with the AWS API Gateway endpoints
//
function updateUrl(value, callback) {

    var outputtext = value.toString();
    var start = outputtext.indexOf(startmarker) + startmarker.length;
    var end = outputtext.indexOf(endmarker);

    outputtext = outputtext.slice(start, end);
    outputtext = outputtext.trim();

    var endpoints = outputtext.split('\n');
    var uploadurl = endpoints[0].trim();
    var workitemurl = endpoints[1].trim();
    var workitemstatusurl = endpoints[2].trim();
    var processurl = endpoints[3].trim();

    uploadurl = uploadurl.slice(uploadurl.indexOf('-') + 1);
    uploadurl = uploadurl.trim();
    workitemurl = workitemurl.slice(workitemurl.indexOf('-') + 1);
    workitemurl = workitemurl.trim();
    workitemstatusurl = workitemstatusurl.slice(workitemstatusurl.indexOf('-') + 1);
    workitemstatusurl = workitemstatusurl.trim();
    processurl = processurl.slice(processurl.indexOf('-') + 1);
    processurl = processurl.trim();

    if (!uploadurl || !workitemurl || !workitemstatusurl || !processurl) {
        console.log("The endpoints could not be extracted");
        callback(false);
        return;
    }

    var file = fs.createReadStream(indexfile, 'utf8');
    var newdata = '';
    file.on('data', function (data) {
        var updatedata = data;
        updatedata = updatedata.toString().replace('UPLOAD_URL', uploadurl);
        updatedata = updatedata.toString().replace('WORKITEM_URL', workitemurl);
        updatedata = updatedata.toString().replace('WORKITEMSTATUS_URL', workitemstatusurl);
        updatedata = updatedata.toString().replace('PROCESS_URL', processurl);

        newdata += updatedata;
    });

    file.on('end', function () {
        fs.writeFile(indexfile, newdata, function (err) {
            if (err) {
                console.log('Error updating index.html');
                console.log(err);
                callback(false);
                return;
            }

            callback(true);
        });
    });
}

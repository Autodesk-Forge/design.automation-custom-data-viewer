# IOViewerSample
The sample code shows the combined usage of DesignAutomation API (https://developer.autodesk.com/en/docs/design-automation/v2) and the Viewer API (https://developer.autodesk.com/en/docs/viewer/v2/overview/) in a sample application (http://viewersample.autocad.io/). The sample displays the usage of custom objects, object enablers using DesignAutomation API.

## Pre-requisites
1. Visual Studio 2015 (https://msdn.microsoft.com/en-us/default.aspx)
2. Nodejs (https://nodejs.org/en/download/)
3. Amazon Web Service access key and secret (https://aws.amazon.com/).
4. Autodesk Developer access key and secret (https://developer.autodesk.com/).


## Setup
1. Verify that you have all the pre-requisites.
2. Open .\src\SampleApp.sln in VisualStudio 2015.
3. Build solution.
4. Open .\setup\init.bat in your favorite text editor and enter the values. All the values are needed to run the sample.
5. Open "Node.js command prompt" from the Windows Start Menu, change directory to .\setup\
6. Run setup.bat

## Setup Workflow
1. This will run ClientApp executable. This will create
   - Zip package .\src\bin\<Debug\Release>\package.zip.
   - AppPackage using the DesignAutomation API.
   - Custom activities using the DesignAutomation API.
2. The dependencies are downloaded for the various projects
3. The config.js file is updated with data needed to run the lambdas.
4. The services is deployed using serverless framework
   - The lambdas are uploaded and configured.
   - APIs are created, configured using AWS API gateway to expose the lambdas.
   - A storage bucket is created and configured for processing dwg file.
5. The html file is updated with the endpoints from the API gateway.
6. A node server is started and the page is loaded from localhost on port 8080.

The sample app has essentially three parts

##1. ARX Application
The ARX application is written in .NET, it has a command called ‘TEST’. The TEST command extracts any XData associated with the entities to a JSON file. 
There is also another command called ‘TESTPOLY’ that creates the polysamp custom object. The ARX along with the object enablers and managed wrappers for polysamp custom object is uploaded as part of a custom package using the DesignAutomation API, and run as part of a custom activity. The object enablers and the respective managed wrappers for the polysamp custom object have been pre-built. Please refer to the ObjectARX sdk for the source code.


##2. AWS Lambdas
The backend processing is implemented using server less paradigm using AWS lambdas. The lambdas are exposed to the client webpage using the AWS API gateway.

There are four lambda functions implemented.
- UploadLocation
The function returns a pre-signed url location that the client can upload the drawing.

- SubmitWorkItem
The function takes the activity name, and the location of the drawing file as input. It checks that the custom package and the activity are present, and creates workitem using the DesignAutomation API.

- WorkItemStatus
The function takes the workitem identifier as the input, and checks the status of the respective workitem. The function uses the DesignAutomation API to poll the status. It returns immediately on success or failure of the workitem. If the workitem status is pending or in-progress, it loops through, polling the status every two seconds for up to twenty seconds and then returns. The reason it returns after twenty seconds is due to a thirty second timeout limit set by the AWS API gateway. If the API does not respond within thirty seconds, then the request is terminated and an error status of 504 is returned by the AWS API gateway.

- ProcessResult
The function takes the location of the result posted by DesignAutomation API for the workitem that was submitted. The result which is a compressed file containing the output is uncompressed by the function and uploaded to a unique location on S3. The location of the SVF/F2D and the XData files are returned.

##3. Client Webpage
The webpage has JavaScript functionality to invoke the API exposed by the lambdas. The local drawing selected by the user is uploaded using the AWS API gateway, and the workitem referencing the uploaded drawing is submitted. The Viewer API is used to show the resultant SVF/F2D file that was processed. Selecting the object in the Viewer will display any XData associated with the object in a properties panel, implemented using the Viewer API.


##Workflow:

![Workflow of the sample](http://g.gravizo.com/g?
@startuml;
actor "Browser" as ua;
participant "UploadLocation lambda" as uploader;
participant "SubmitWorkItem lambda" as submitter;
participant "WorkitemStatus lambda" as statuschecker;
participant "ProcessResult lambda" as resultprocessor;
participant s3;
participant "Forge Design Automation" as acadio;
ua -> uploader: Get presigned url upload;
uploader -> ua: presigned url for S3;
ua -> s3 : upload dwg file;
ua -> submitter : request work;
submitter -> acadio : check apppackage;
submitter -> acadio : check activitiy;
submitter -> acadio : post workitem;
submitter -> ua : work submitted;
ua -> statuschecker : check workitem status;
statuschecker -> acadio : get workitem status;
acadio -> statuschecker : success;
statuschecker -> ua : success;
ua -> resultprocessor : process results;
@enduml
)

###Workflow1:
1.	The drawing is selected in the webpage.
2.	A pre-signed url is obtained from the UploadLocation lambda using the AWS gateway API for uploading the drawing, and it is uploaded.
3.	The SubmitWorkItem lambda is invoked passing the location of the uploaded drawing, and the activity name.
4.	The workitem referencing the custom activity is submitted for the drawing.
5.	The workitem status is queried by the client if the workitem has been successfully submitted. The status is queried using the WorkItemStatus lambda. The lambda uses the DesignAutomation API to get the status and return it to the client.
6.	On success, the workitem output location is returned to the client.
7.	The ProcessResult lambda is invoked with the location of the output, which is a zip file.
8.	The output file is uncompressed by the lambda, uploads it to S3. It returns the location of the SVF/F2D and XData files, which are among the uncompressed files.
9.	A new browser window is launched passing the location of the files when the Viewer button is clicked.
10.	The SVF/F2D files are displayed using the Viewer API in the Window. The Viewer API is used to implement the extension to display the XData of the object.
11.	When an object is selected, the XData information is searched if the object has any associated XData using the object handle. If it has, then the values are displayed in a properties panel created using the Viewer API.

###Workflow2:
1.	Create custom object is selected in the Webpage.
2.	The SubmitWorkItem lambda is invoked passing the location of the empty drawing.
3.	The workitem referencing the custom activity to create the custom object is submitted.
4.	The workitem status is queried by the client if the workitem has been successfully submitted. The status is queried using the WorkItemStatus lambda. The lambda uses the DesignAutomation API to get the status and return it to the client.
5.	On success, the workitem output location is returned to the client.
6.	The ProcessResult lambda is invoked with the location of the output, which is a zip file.
7.	The output file is uncompressed by the lambda, uploads it to S3. It returns the location of the SVF/F2D and XData files, which are among the uncompressed files.
8.	A new browser window is launched passing the location of the files when the Viewer button is clicked.
9.	The F2D files are displayed using the Viewer API in the Window. The Viewer API is used to implement the extension to display the XData of the object.
10.	When an object is selected, the XData information is searched if the object has any associated XData using the object handle. If it has, then the values are displayed in a properties panel created using the Viewer API.


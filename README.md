# IOViewerSample
The sample code shows the usage of DesignAutomation API (https://developer.autodesk.com/en/docs/design-automation/v2) and the Viewer API (https://developer.autodesk.com/en/docs/viewer/v2/overview/) in a sample application (http://viewersample.autocad.io/).

The sample app has essentially three parts

##1. ARX Application
The ARX application is written in .NET, it has a command called ‘TEST’. The TEST command extracts any XData associated with the entities to a JSON file. The ARX is uploaded as part of a custom package using the DesignAutomation API, and run as part of a custom activity.


##2. AWS Lambdas
The backend processing is implemented using server less paradigm using AWS lambdas. The lambdas are exposed to the client webpage using the AWS API gateway.

There are four lambda functions implemented.
- UploadLocation
The function returns a pre-signed url location that the client can upload the drawing.

- SubmitWorkItem
The function takes the activity name, and the location of the drawing file as input. It checks if the custom package and the activity are present. If it does not exist, it is created using the DesignAutomation API. The zip package that contains the ARX application is uploaded using the DesignAutomation API to create the custom package. The workitem is then submitted using the DesignAutomation API, configured to run the custom activity.

- WorkItemStatus
The function takes the workitem identifier as the input, and checks the status of the respective workitem. The function uses the DesignAutomation API to poll the status. It returns immediately on success or failure of the workitem. If the workitem status is pending or in-progress, it loops through, polling the status every two seconds for up to twenty seconds and then returns. The reason it returns after twenty seconds is due to a thirty second timeout limit set by the AWS API gateway. If the API does not respond within thirty seconds, then the request is terminated and an error status of 504 is returned by the AWS API gateway.

- ProcessResult
The function takes the location of the result posted by DesignAutomation API for the workitem that was submitted. The result which is a compressed file containing the output is uncompressed by the function and uploaded to a unique location on S3. The location of the SVF/F2D and the XData files are returned.

##3. Client Webpage
The webpage has JavaScript functionality to invoke the API exposed by the lambdas. The local drawing selected by the user is uploaded using the AWS API gateway, and the workitem referencing the uploaded drawing is submitted. The Viewer API is used to show the resultant SVF/F2D file that was processed. Selecting the object in the Viewer will display any XData associated with the object in a properties panel, implemented using the Viewer API.


##Setup:
1.	Build the Client Application, SampleApp.sln. This will create a package.zip
2.	Upload package.zip to a location.
3.	Reference the package.zip location in the config.js file of SubmitWorkItem lambda.
4.	Update config.js files with the Application API Key and Value.
5.	Update config.js files with AWS key, secret and the bucket.

##Workflow:
1.	The drawing is selected in the webpage.
2.	A pre-signed url is obtained from the UploadLocation lambda using the AWS gateway API for uploading the drawing, and it is uploaded.
3.	The SubmitWorkItem lambda is invoked passing the location of the uploaded drawing.
4.	The SubmitWorkItem lambda checks if the custom package exists using the DesignAutomation API. The package is created using the package.zip if it does not exist.
5.	The SubmitWorkItem lambda checks if the custom activity exists using the DesignAutomation API. The custom activity is created if it does not exist using the API.
6.	The workitem referencing the custom activity is submitted for the drawing.
7.	The workitem status is queried by the client if the workitem has been successfully submitted. The status is queried using the WorkItemStatus lambda. The lambda uses the DesignAutomation API to get the status and return it to the client.
8.	On success, the workitem output location is returned to the client.
9.	The ProcessResult lambda is invoked with the location of the output, which is a zip file.
10.	The output file is uncompressed by the lambda, uploads it to S3. It returns the location of the SVF/F2D and XData files, which are among the uncompressed files.
11.	A new browser window is launched passing the location of the files when the Viewer button is clicked.
12.	The SVF/F2D files are displayed using the Viewer API in the Window. The Viewer API is used to implement the extension to display the XData of the object.
13.	When an object is selected, the XData information is searched if the object has any associated XData using the object handle. If it has, then the values are displayed in a properties panel created using the Viewer API.


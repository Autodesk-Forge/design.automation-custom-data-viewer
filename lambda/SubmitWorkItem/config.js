module.exports = {
      
    auth_endpoint: "https://developer.api.autodesk.com/authentication/v1/authenticate",   
    package_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/AppPackages",
    package_upload_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/AppPackages/Operations.GetUploadUrl()",
    activity_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/Activities",
    workitem_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/WorkItems",
    
    package_name: "MyCustomPackage",
    refpackage_name: "Publish2View21",
    activities : [{ name: "MyPublishActivity3d", script: "_test ./result/xdata.json\r\n_prepareforpropertyextraction index.json\r\n_indexextractor index.json\r\n_publishtosvf ./output/result.svf\r\n_createbubblepackage ./output ./result \r\n\n" },
                  { name: "MyPublishActivity2d", script: "_test ./result/xdata.json\r\n_prepareforpropertyextraction index.json\r\n_indexextractor index.json\r\n_publishtof2d ./output\r\n_createbubblepackage ./output ./result \r\n\n" }],
    

    // Enter the URL where the package source has been posted
    package_source_endpoint: "",
                  
    // Enter the Autodesk API developer key
    developer_key: "",

    // Enter the Autodesk API developer key value
    developer_secret: "",    

    // Update the engine version to be used
    required_engineversion: "21.0",

    // Enter the AWS key
    aws_key:"",
    // Enter the AWS key value
    aws_secret:"",
    // Enter the region if applicable
    aws_region: "",
    // Enter the bucket name
    aws_s3_bucket:""
}

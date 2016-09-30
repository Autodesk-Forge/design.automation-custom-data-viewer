module.exports = {
    auth_endpoint: "https://developer.api.autodesk.com/authentication/v1/authenticate",

    package_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/AppPackages",
    package_upload_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/AppPackages/Operations.GetUploadUrl()",
    activity_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/Activities",
    workitem_endpoint: "https://developer.api.autodesk.com/autocad.io/us-east/v2/WorkItems",
    package_source_endpoint: "package_source_endpoint_value",

    package_name: "MyCustomPackage",
    refpackage_name: "Publish2View21",
    activities: [{ name: "MyPublishActivity3d", script: "_test ./result/xdata.json\r\n_prepareforpropertyextraction index.json\r\n_indexextractor index.json\r\n_publishtosvf ./output/result.svf\r\n_createbubblepackage ./output ./result \r\n\n" },
                  { name: "MyPublishActivity2d", script: "_test ./result/xdata.json\r\n_prepareforpropertyextraction index.json\r\n_indexextractor index.json\r\n_publishtof2d ./output\r\n_createbubblepackage ./output ./result \r\n\n" }],


    developer_key: "ADSK_DEVELOPER_KEY",
    developer_secret: "ADSK_DEVELOPER_SECRET",
    required_engineversion: "21.0",

    aws_key: "AWS_ACCESS_KEY_ID",
    aws_secret: "AWS_SECRET_ACCESS_KEY",
    aws_region: "",
    aws_s3_bucket: "DA_VIEWER_DWG_BUCKET"
}

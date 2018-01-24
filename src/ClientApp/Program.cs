using ClientApp.ACES.Models;
using ClientApp.Operations;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace ClientApp
{
    class Credentials
    {
        // Get your ConsumerKey/ConsumerSecret from https://developer.autodesk.com
        //
        public static string baseUrl = "https://developer.api.autodesk.com";
    }

    class Program
    {
        static readonly string PackageName = "MyCustomPackage";
        static readonly string zip = "package.zip";
        static readonly string ActivityName2d = "MyPublishActivity2d";
        static readonly string ActivityName3d = "MyPublishActivity3d";
        static readonly string ActivityNameCustom = "MyPublishActivityCustom";
        static readonly string Script2d = "_test ./result/xdata.json\r\n_prepareforpropertyextraction index.json\r\n_indexextractor index.json\r\n_publishtof2d ./output\r\n_createbubblepackage ./output ./result \r\n\n";
        static readonly string Script3d = "_test ./result/xdata.json\r\n_prepareforpropertyextraction index.json\r\n_indexextractor index.json\r\n_publishtosvf ./output/result.svf\r\n_createbubblepackage ./output ./result \r\n\n";
        static readonly string ScriptCustom = "_testpoly\r\n_save \r\n_test ./result/xdata.json\r\n_prepareforpropertyextraction index.json\r\n_indexextractor index.json\r\n_publishtof2d ./output\r\n_createbubblepackage ./output ./result \r\n\n";

        static readonly string RequiredEngineVersion = "22.0";

        static int Main(string[] args)
        {
            var consumerKey = Environment.GetEnvironmentVariable("ADSK_DEVELOPER_KEY");
            var consumerSecret = Environment.GetEnvironmentVariable("ADSK_DEVELOPER_SECRET");

            if (string.IsNullOrEmpty(consumerKey) || string.IsNullOrEmpty(consumerSecret))
            {
                Console.WriteLine("Please enter valid Autodesk consumer key secret");
                return 1;
            }

            var token = GetToken(consumerKey, consumerSecret);
            if (token == null)
            {
                Console.WriteLine("Error obtaining access token");
                return 1;
            }

            string url = Credentials.baseUrl + "/autocad.io/us-east/v2/";
            Container container = new Container(new Uri(url));
            container.SendingRequest2 += (sender, e) => e.RequestMessage.SetHeader("Authorization", token);

            // Create the app package if it does not exist
            AppPackage package = CreatePackage(container);            
            if (package == null)
            {
                Console.WriteLine("Error creating app package");
                return 1;
            }

            // Create the custom activity
            //
            Activity activity = CreateActivity(ActivityName2d, Script2d, container);
            if (activity == null)
            {
                Console.WriteLine("Error creating custom activity: " + ActivityName2d);
                return 1;
            }

            activity = CreateActivity(ActivityName3d, Script3d, container);
            if (activity == null)
            {
                Console.WriteLine("Error creating custom activity: " + ActivityName3d);
                return 1;
            }

            activity = CreateActivity(ActivityNameCustom, ScriptCustom, container);
            if (activity == null)
            {
                Console.WriteLine("Error creating custom activity: " + ActivityNameCustom);
                return 1;
            }

            // Save changes if any
            container.SaveChanges();

            return 0;
        }

        public static string GetToken(string consumerKey, string consumerSecret)
        {
            using (var client = new HttpClient())
            {
                var values = new List<KeyValuePair<string, string>>();
                values.Add(new KeyValuePair<string, string>("client_id", consumerKey));
                values.Add(new KeyValuePair<string, string>("client_secret", consumerSecret));
                values.Add(new KeyValuePair<string, string>("grant_type", "client_credentials"));
                values.Add(new KeyValuePair<string, string>("scope", "code:all"));
                
                var requestContent = new FormUrlEncodedContent(values);
                string url = Credentials.baseUrl + "/authentication/v1/authenticate";
                var response = client.PostAsync(url, requestContent).Result;
                var responseContent = response.Content.ReadAsStringAsync().Result;
                var resValues = JsonConvert.DeserializeObject<Dictionary<string, string>>(responseContent);
                return resValues["token_type"] + " " + resValues["access_token"];
            }
        }

        static AppPackage CreatePackage(Container container)
        {
            AppPackage package = null;
            try
            {
                package = container.AppPackages.ByKey(PackageName).GetValue();
            }
            catch
            {
            }
            if (package != null)
            {
                string res = null;
                res = Prompts.PromptForKeyword(string.Format("AppPackage '{0}' already exists. What do you want to do? [Recreate/Update/Leave]<Update>", PackageName));
                if (res == "Recreate")
                {
                    container.DeleteObject(package);
                    container.SaveChanges();
                    package = null;
                }
                if (res == "Leave")
                    return package;
            }

            var dir = Path.GetDirectoryName(System.Diagnostics.Process.GetCurrentProcess().MainModule.FileName);
            if (!File.Exists(Path.Combine(dir, "RxApp.dll")))
            {
                Console.WriteLine("Error building RxApp.dll");
                return null;
            }
            if (!File.Exists(Path.Combine(dir, "asdkmgPoly.dll")))
            {
                Console.WriteLine("Error building asdkmgPoly.dll");
                return null;
            }
            if (!File.Exists(Path.Combine(dir, "asdkpolyobj.dbx")))
            {
                Console.WriteLine("Error building asdkpolyobj.dbx");
                return null;
            }

            string zipfile = CreateZip();
            if (string.IsNullOrEmpty(zipfile))
            {
                Console.WriteLine("Error creating package.zip");
                return null;
            }
            Console.WriteLine("The package.zip file was successfully created");

            Console.WriteLine("Creating/Updating AppPackage...");
            // Query for the url to upload the AppPackage file
            var url = container.AppPackages.GetUploadUrl().GetValue();
            var client = new HttpClient();
            // Upload AppPackage file
            client.PutAsync(url, new StreamContent(File.OpenRead(zip))).Result.EnsureSuccessStatusCode();

            // third step -- after upload, create the AppPackage entity
            if (package == null)
            {
                package = new AppPackage()
                {
                    Id = PackageName,
                    RequiredEngineVersion = RequiredEngineVersion,
                    Resource = url
                };
                container.AddToAppPackages(package);
            }
            else
            {
                package.Resource = url;
                container.UpdateObject(package);
            }

            container.SaveChanges();
            return package;
        }


        static Activity CreateActivity(string activityname, string script, Container container)
        {
            Activity activity = null;
            try { activity = container.Activities.ByKey(activityname).GetValue(); }
            catch { }
            if (activity != null)
            {
                if (Prompts.PromptForKeyword(string.Format("Activity '{0}' already exists. Do you want to recreate it? [Yes/No]<No>", activityname)) == "Yes")
                {
                    container.DeleteObject(activity);
                    container.SaveChanges();
                    activity = null;
                }
                else
                {
                    return activity;
                }
            }

            Console.WriteLine("Creating/Updating Activity...");
            activity = new Activity()
            {
                Id = activityname,
                Instruction = new Instruction()
                {
                    Script = script
                },
                Parameters = new Parameters()
                {
                    InputParameters = {
                        new Parameter() { Name = "HostDwg", LocalFileName = "$(HostDwg)" }
                    },
                    OutputParameters = { new Parameter() { Name = "Results", LocalFileName = "result" } }
                },
                RequiredEngineVersion = RequiredEngineVersion
            };
            activity.AppPackages.Add(PackageName); 
            activity.AppPackages.Add("Publish2View22"); // reference the custom AppPackage
            container.AddToActivities(activity);
            container.SaveChanges();
            return activity;
        }


        static string CreateZip()
        {
            Console.WriteLine("Generating autoloader zip...");
            if (File.Exists(zip))
                File.Delete(zip);
            var dir = Path.GetDirectoryName(System.Diagnostics.Process.GetCurrentProcess().MainModule.FileName);
            using (var archive = ZipFile.Open(zip, ZipArchiveMode.Create))
            {
                string bundle = PackageName + ".bundle";
                string name = "PackageContents.xml";
                archive.CreateEntryFromFile(Path.Combine(dir,name), Path.Combine(bundle, name));
                name = "RxApp.dll";
                archive.CreateEntryFromFile(Path.Combine(dir, name), Path.Combine(bundle, "Contents", name));
                name = "asdkmgPoly.dll";
                archive.CreateEntryFromFile(Path.Combine(dir, name), Path.Combine(bundle, "Contents", name));
                name = "asdkpolyobj.dbx";
                archive.CreateEntryFromFile(Path.Combine(dir, name), Path.Combine(bundle, "Contents", name));
            }
            return zip;
        }
    }

    class Prompts
    {
        class KeywordPrompt
        {
            string[] m_keywords;
            string m_default;
            static Regex s_reg = new Regex(".*\\[(?<keywords>.*)\\]\\<(?<default>.*)\\>$");
            public KeywordPrompt(string prompt)
            {
                var match = s_reg.Match(prompt);
                if (!match.Success)
                    throw new ArgumentException();
                m_keywords = match.Groups["keywords"].Value.Split(new char[] { '/' }, StringSplitOptions.RemoveEmptyEntries);
                m_default = match.Groups["default"].Value;
                string dummy;
                if (string.IsNullOrEmpty(m_default) || !TryMatch(m_default, out dummy))
                    throw new ArgumentException();
            }
            public bool TryMatch(string str, out string match)
            {
                if (string.IsNullOrEmpty(str))
                {
                    match = m_default;
                    return true;
                }
                match = null;
                var matches = m_keywords.Where(s => s.StartsWith(str, StringComparison.CurrentCultureIgnoreCase));
                if (matches.Count() != 1)
                    return false;
                match = matches.First();
                return true;
            }
        }
        public static string PromptForKeyword(string promptString)
        {
            var prompt = new KeywordPrompt(promptString);
            while (true)
            {
                Console.Write("{0}:", promptString);
                var resp = Console.ReadLine();
                string ret;
                if (prompt.TryMatch(resp, out ret))
                    return ret;
            }

        }
    }

}

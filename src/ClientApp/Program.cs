using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientApp
{
    class Program
    {
        static readonly string PackageName = "MyCustomPackage";
        static readonly string zip = "package.zip";

        static void Main(string[] args)
        {
            if (!File.Exists("RxApp.dll"))
            {
                Console.WriteLine("Error building RxApp.dll");
                return;
            }
            string zipfile = CreateZip();
            if (string.IsNullOrEmpty(zipfile))
            {
                Console.WriteLine("Error creating package.zip");
                return;
            }

            File.Copy(zip, "..\\" + zip, true);
            Console.WriteLine("The package.zip was successfully created");
        }

        static string CreateZip()
        {
            Console.WriteLine("Generating autoloader zip...");
            if (File.Exists(zip))
                File.Delete(zip);
            using (var archive = ZipFile.Open(zip, ZipArchiveMode.Create))
            {
                string bundle = PackageName + ".bundle";
                string name = "PackageContents.xml";
                archive.CreateEntryFromFile(name, Path.Combine(bundle, name));
                name = "RxApp.dll";
                archive.CreateEntryFromFile(name, Path.Combine(bundle, "Contents", name));
            }
            return zip;
        }
    }
}

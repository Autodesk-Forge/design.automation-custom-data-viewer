using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Autodesk.AutoCAD.Runtime;
using Autodesk.AutoCAD.ApplicationServices.Core;
using Autodesk.AutoCAD.DatabaseServices;
using System.Collections.ObjectModel;
using Autodesk.AutoCAD.EditorInput;
using System.IO;
using System.Runtime.Serialization.Json;

[assembly: CommandClass(typeof(RxApp.Commands))]
[assembly: ExtensionApplication(null)]

namespace RxApp
{
    public class Commands
    {
        [CommandMethod("MyTestCommand", "test", CommandFlags.Modal)]
        static public void Test()
        {
            var doc = Application.DocumentManager.MdiActiveDocument;
            var ed = doc.Editor;

            PromptResult pr = ed.GetString("Specify output filename:");
            if (pr.Status == Autodesk.AutoCAD.EditorInput.PromptStatus.Cancel)
            {
                ed.WriteMessage("Command cancelled");
                return;
            }
            string outputFile = pr.StringResult;
            if (string.IsNullOrEmpty(outputFile))
            {
                ed.WriteMessage("Error: Invalid output file");
                return;
            }

            try
            {
                Collection<DBObjectInfo> xdata = new Collection<DBObjectInfo>();

                Database db = doc.Database;
                ObjectId modId = SymbolUtilityServices.GetBlockModelSpaceId(db);
                using (Transaction trans = doc.TransactionManager.StartTransaction())
                {
                    BlockTableRecord btr = trans.GetObject(modId, OpenMode.ForRead) as BlockTableRecord;
                    foreach (ObjectId entId in btr)
                    {
                        DBObject obj = trans.GetObject(entId, OpenMode.ForRead);
                        ResultBuffer rb = obj.XData;
                        if (rb == null)
                            continue;

                        DBObjectInfo objInfo = new DBObjectInfo();
                        objInfo.Id = obj.Handle.ToString();
                        string strValue;
                        foreach (TypedValue tvalue in rb)
                        {
                            strValue = "";
                            if (tvalue.TypeCode == 1004)
                            {
                                string hexvalue = BitConverter.ToString(tvalue.Value as byte[]);
                                hexvalue = hexvalue.Replace("-", "");
                                strValue = hexvalue;
                            }
                            else
                            {
                                strValue = string.Format("{0}", tvalue.Value);
                            }

                            DataValue dataVal = new DataValue();
                            dataVal.Type = tvalue.TypeCode;
                            dataVal.Value = strValue;

                            objInfo.XData.Add(dataVal);
                        }
                        rb.Dispose();

                        xdata.Add(objInfo);
                    }
                }

                if (xdata.Count <= 0)
                {
                    ed.WriteMessage("There are no XData associated with entities in this drawing.");
                    return;
                }

                Directory.CreateDirectory(Path.GetDirectoryName(outputFile));
                using (var stream = File.Create(outputFile))
                {
                    DataContractJsonSerializer ser = new DataContractJsonSerializer(xdata.GetType());
                    ser.WriteObject(stream, xdata);
                }

            }
            catch (System.Exception e)
            {
                ed.WriteMessage("Error: {0}", e);
            }
        }
    }
}

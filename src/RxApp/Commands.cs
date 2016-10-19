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
using Autodesk.AutoCAD.Geometry;
using Autodesk.AutoCAD.ApplicationServices;

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

        [CommandMethod("MyTestCommand", "testpoly", CommandFlags.Modal)]
        static public void Poly()
        {
            Document doc = Application.DocumentManager.MdiActiveDocument;
            using (DocumentLock l = doc.LockDocument())
            {
                using (Transaction t = doc.Database.TransactionManager.StartTransaction())
                {
                    BlockTable bt = (BlockTable)t.GetObject(doc.Database.BlockTableId, OpenMode.ForRead);
                    BlockTableRecord btr = (BlockTableRecord)t.GetObject(bt[BlockTableRecord.ModelSpace], OpenMode.ForWrite);
                    using (Autodesk.ObjectDbxSample.Poly poly = new Autodesk.ObjectDbxSample.Poly())
                    {
                        poly.Center = new Point2d(100, 100);
                        poly.StartPoint2d = new Point2d(300, 100);
                        poly.NumberOfSides = 5;
                        poly.Name = "Managed Poly";

                        btr.AppendEntity(poly);
                        t.AddNewlyCreatedDBObject(poly, true);

                        AddRegAppTableRecord("IOVIEWERSAMPLE");
                        ResultBuffer rb = new ResultBuffer(new TypedValue(1001, "IOVIEWERSAMPLE"),
                            new TypedValue(1000, "This is a sample string"));

                        poly.XData = rb;
                        rb.Dispose();
                    }
                    t.Commit();
                }
            }            
        }

        static void AddRegAppTableRecord(string regAppName)
        {
            Document doc = Application.DocumentManager.MdiActiveDocument;
            Editor ed = doc.Editor;
            Database db = doc.Database;
            using (Transaction tr = doc.TransactionManager.StartTransaction())
            {
                RegAppTable regAppTable = (RegAppTable)tr.GetObject(db.RegAppTableId, OpenMode.ForRead, false);
                if (!regAppTable.Has(regAppName))
                {
                    regAppTable.UpgradeOpen();
                    RegAppTableRecord regAppTableRecord = new RegAppTableRecord();
                    regAppTableRecord.Name = regAppName;
                    regAppTable.Add(regAppTableRecord);
                    tr.AddNewlyCreatedDBObject(regAppTableRecord, true);
                }
                tr.Commit();
            }
        }
    }
}

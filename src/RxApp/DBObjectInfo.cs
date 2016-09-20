using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace RxApp
{
    [DataContract]
    class DBObjectInfo
    {
        Collection<DataValue> coll = new Collection<DataValue>();
        public DBObjectInfo()
        {

        }

        [DataMember]
        public string Id { get; set; }

        [DataMember]
        public Collection<DataValue> XData
        {
            get
            {
                return coll;
            }
        }
    }

    [DataContract]
    class DataValue
    {
        public DataValue()
        {

        }

        [DataMember]
        public short Type { get; set; }

        [DataMember]
        public string Value { get; set; }
    }
}

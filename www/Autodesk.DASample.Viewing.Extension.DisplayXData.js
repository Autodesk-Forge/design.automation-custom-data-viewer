// The viewer extension to view Xdata associated with the object
//
AutodeskNamespace("Autodesk.DASample.Viewing.Extension");

// The property panel to view the xdata values
XDataPanel = function (parentContainer, id, x, y) {
    this.content = document.createElement('div');
    Autodesk.Viewing.UI.PropertyPanel.call(this, parentContainer, id, 'XData');

    this.container.style.height = "auto";
    this.container.style.width = "auto";
    this.container.style.resize = "none";
    this.container.style.left = x + "px";
    this.container.style.top = y + "px";
};

XDataPanel.prototype = Object.create(Autodesk.Viewing.UI.PropertyPanel.prototype);
XDataPanel.prototype.constructor = XDataPanel;
XDataPanel.prototype.initialize = function () {

    this.title = this.createTitleBar(this.titleLabel || this.container.id);
    this.container.appendChild(this.title);
    this.container.appendChild(this.content);
    this.initializeMoveHandlers(this.container);
};


Autodesk.DASample.Viewing.Extension.DisplayXData = function (viewer, options) {

    Autodesk.Viewing.Extension.call(this, viewer, options);

    var _self = this;
    var _panel = null;

    _self.load = function () {
        viewer.addEventListener(Autodesk.Viewing.SELECTION_CHANGED_EVENT, onSelectionChanged);
        _panel = new XDataPanel(viewer.container, guid(), 5, 5);
        _panel.setVisible(true);

        return true;
    };

    _self.unload = function () {

        viewer.removeEventListener(Autodesk.Viewing.SELECTION_CHANGED_EVENT, onSelectionChanged);
        return true;
    };

    function guid() {

        var d = new Date().getTime();

        var guid = 'xxxx-xxxx-xxxx-xxxx'.replace(
          /[xy]/g,
          function (c) {
              var r = (d + Math.random() * 16) % 16 | 0;
              d = Math.floor(d / 16);
              return (c == 'x' ? r : (r & 0x7 | 0x8)).toString(16);
          });

        return guid;
    }

    function onSelectionChanged(event) {
        _panel.removeAllProperties();

        var viewer = event.target;
        event.dbIdArray.forEach(function (dbId) {
            displayXData(dbId);
        });
    }

    // The function checks if the object that is passed has any xdata that has 
    // been cached. The object handle is used to check the availability 
    // in the cache.
    //
    function displayXData(dbId) {
        if (!xdatainfo)
            return;

        viewer.getProperties(dbId, function _cb(result) {
            if (result.properties) {
                var displayValue;
                var length = result.properties.length;
                for (var i = 0; i < length; i++) {
                    var prop = result.properties[i];
                    if (prop.displayName === "elementId") {
                        displayValue = prop.displayValue;
                        break;
                    }
                }

                if (displayValue) {
                    if (xdatainfo[displayValue]) {
                        var xdata = xdatainfo[displayValue].XData;
                        var length = xdata.length;
                        for (var i = 0; i < length; i++) {
                            _panel.addProperty(xdata[i].Type, xdata[i].Value, dbId);
                        }
                    }
                }
            }
        });
    }
};

Autodesk.DASample.Viewing.Extension.DisplayXData.prototype =
    Object.create(Autodesk.Viewing.Extension.prototype);

Autodesk.DASample.Viewing.Extension.DisplayXData.prototype.constructor =
    Autodesk.DASample.Viewing.Extension.DisplayXData;

Autodesk.Viewing.theExtensionManager.registerExtension(
    'Autodesk.DASample.Viewing.Extension.DisplayXData',
    Autodesk.DASample.Viewing.Extension.DisplayXData);

{
  "patcher": {
    "fileversion": 1,
    "appversion": { "major": 8, "minor": 5, "revision": 0, "architecture": "x64", "modernui": 1 },
    "classnamespace": "box",
    "rect": [80.0, 80.0, 900.0, 600.0],
    "boxes": [
      {
        "box": {
          "id": "obj-driver",
          "maxclass": "newobj",
          "text": "js avnd_max_driver.js",
          "varname": "driver",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": [""],
          "patching_rect": [40.0, 160.0, 180.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-lb",
          "maxclass": "newobj",
          "text": "loadbang",
          "numinlets": 1,
          "numoutlets": 1,
          "outlettype": ["bang"],
          "patching_rect": [40.0, 40.0, 70.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-del",
          "maxclass": "newobj",
          "text": "del 800",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": ["bang"],
          "patching_rect": [40.0, 80.0, 70.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-run",
          "maxclass": "message",
          "text": "run",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""],
          "patching_rect": [40.0, 120.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-dspstart",
          "maxclass": "message",
          "text": "; dsp start",
          "varname": "dspstart",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""],
          "patching_rect": [300.0, 40.0, 90.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-dspstop",
          "maxclass": "message",
          "text": "; dsp stop",
          "varname": "dspstop",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""],
          "patching_rect": [300.0, 80.0, 90.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-dspdrv",
          "maxclass": "message",
          "text": "; dsp driver NonRealTime",
          "varname": "dspdriver",
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""],
          "patching_rect": [300.0, 120.0, 200.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-avin0",
          "maxclass": "newobj",
          "text": "buffer~ avin0",
          "varname": "buf_avin0",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": ["float", "bang"],
          "patching_rect": [500.0, 40.0, 110.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-avin1",
          "maxclass": "newobj",
          "text": "buffer~ avin1",
          "varname": "buf_avin1",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": ["float", "bang"],
          "patching_rect": [500.0, 80.0, 110.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-avout0",
          "maxclass": "newobj",
          "text": "buffer~ avout0",
          "varname": "buf_avout0",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": ["float", "bang"],
          "patching_rect": [500.0, 120.0, 110.0, 22.0]
        }
      },
      {
        "box": {
          "id": "obj-avout1",
          "maxclass": "newobj",
          "text": "buffer~ avout1",
          "varname": "buf_avout1",
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": ["float", "bang"],
          "patching_rect": [500.0, 160.0, 110.0, 22.0]
        }
      }
    ],
    "lines": [
      { "patchline": { "source": ["obj-lb", 0], "destination": ["obj-del", 0] } },
      { "patchline": { "source": ["obj-del", 0], "destination": ["obj-run", 0] } },
      { "patchline": { "source": ["obj-run", 0], "destination": ["obj-driver", 0] } }
    ]
  }
}

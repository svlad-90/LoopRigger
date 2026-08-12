DEVICE_ID = "test.device"
DISPLAY_NAME = "Test Device"


def on_event(event):
    if event["widgetId"] == "record_t1" and event["type"] == "press":
        return [
            {
                "type": "toggle_track_recording",
                "index": 0,
                "value": event["value"],
            }
        ]
    return []

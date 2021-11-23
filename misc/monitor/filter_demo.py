from platformio.commands.device import DeviceMonitorFilter


class Demo(DeviceMonitorFilter):
    NAME = "demo"

    def __init__(self, *args, **kwargs):
        super(Demo, self).__init__(*args, **kwargs)
        print("Demo filter is loaded")

    def rx(self, text):
        return f"Received: {text}\n"

    def tx(self, text):
        print(f"Sent: {text}\n")
        return text
    
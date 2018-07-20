import pyudev
context = pyudev.Context()
for device in context.list_devices(subsystem='block'):
    print(device, device.get('ID_FS_TYPE', 'unlabeled partition'))


#include <ntifs.h>
#include <ntddk.h>
#include <aux_klib.h>

#include "IOCTLs.h"
#include "Common.h"
#include "WindowsVersions.h"
#include "Callbacks.h"

void DriverCleanup(PDRIVER_OBJECT DriverObject);
NTSTATUS CreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS DeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

WINDOWS_VERSION GetWindowsVersion();
LIST_ENTRY* GetListEntry(POBJECT_TYPE* object);
void SearchLoadedModules(OBJ_CALLBACK_INFORMATION* ModuleInfo);
POB_CALLBACK_ENTRY NextCallbackEntryItem(LIST_ENTRY* List);


UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\AgentKiller");
UNICODE_STRING symlink = RTL_CONSTANT_STRING(L"\\??\\AgentKiller");

extern "C"
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = DriverCleanup;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;

	PDEVICE_OBJECT deviceObject;
	NTSTATUS status = IoCreateDevice(
		DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&deviceObject
	);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("[!] Failed to create Device Object (0x%08X)\n", status));
		return status;
	}

	status = IoCreateSymbolicLink(&symlink, &deviceName);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("[!] Failed to create symlink (0x%08X)\n", status));
		IoDeleteDevice(deviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
DeviceControl(
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	NTSTATUS status = STATUS_SUCCESS;
	ULONG_PTR length = 0;

	// check Windows version
	WINDOWS_VERSION windowsVersion = GetWindowsVersion();

	if (windowsVersion == WINDOWS_UNSUPPORTED)
	{
		status = STATUS_NOT_SUPPORTED;
		KdPrint(("[!] Windows Version Unsupported\n"));

		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = length;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return status;
	}

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{



	case AGENT_KILLER_ENUM_OBJ_PROCESS: {
		ULONG szBuffer = sizeof(OBJ_CALLBACK_INFORMATION) * 64;

		if (stack->Parameters.DeviceIoControl.OutputBufferLength < szBuffer)
		{
			status = STATUS_BUFFER_TOO_SMALL;
			KdPrint(("[!] STATUS_BUFFER_TOO_SMALL\n"));
			break;
		}

		OBJ_CALLBACK_INFORMATION* userBuffer = (OBJ_CALLBACK_INFORMATION*)Irp->UserBuffer;

		if (userBuffer == nullptr)
		{
			status = STATUS_INVALID_PARAMETER;
			KdPrint(("[!] STATUS_INVALID_PARAMETER\n"));
			break;
		}

		LIST_ENTRY* list = GetListEntry(PsProcessType);



		for (ULONG i = 0; i < 64; i++)
		{

			POB_CALLBACK_ENTRY pCallbackEntryItem = NextCallbackEntryItem(list);

			list = &(pCallbackEntryItem->CallbackList);

			ULONG64 callback = (ULONG64)pCallbackEntryItem->PreOperation;  // + offset;
			BOOL enable = pCallbackEntryItem->Enabled;  // + offset;



			userBuffer[i].Pointer = callback;
			userBuffer[i].Operations = pCallbackEntryItem->Operations;
			userBuffer[i].Enable = enable;

			if (callback > 0)
			{
				SearchLoadedModules(&userBuffer[i]);
			}

			length += sizeof(OBJ_CALLBACK_INFORMATION);
		}

		break;

	}

	case AGENT_KILLER_DISABLE_OBJ_PROCESS: {
		if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(TargetCallback))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			KdPrint(("[!] STATUS_BUFFER_TOO_SMALL\n"));
			break;
		}

		TargetCallback* target = (TargetCallback*)stack->Parameters.DeviceIoControl.Type3InputBuffer;

		if (target == nullptr)
		{
			status = STATUS_INVALID_PARAMETER;
			KdPrint(("[!] STATUS_INVALID_PARAMETER\n"));
			break;
		}

		// sanity check value
		if (target->Index < 0 || target->Index > 64)
		{
			status = STATUS_INVALID_PARAMETER;
			KdPrint(("[!] STATUS_INVALID_PARAMETER\n"));
			break;
		}

		LIST_ENTRY* list = GetListEntry(PsProcessType);



		for (int i = 0; i < 64; i++)
		{

			POB_CALLBACK_ENTRY pCallbackEntryItem = NextCallbackEntryItem(list);

			list = &pCallbackEntryItem->CallbackList;

			if (i == target->Index)
			{
				pCallbackEntryItem->Enabled = 0;
				break;
			}

		}
		break;
	}

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		KdPrint(("[!] STATUS_INVALID_DEVICE_REQUEST\n"));
		break;
	}


	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = length;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}



POB_CALLBACK_ENTRY NextCallbackEntryItem(LIST_ENTRY* List) {

	return CONTAINING_RECORD(List, OB_CALLBACK_ENTRY, CallbackList);
}



LIST_ENTRY* GetListEntry(POBJECT_TYPE* object) {

	return (LIST_ENTRY*)(object + 0x8c);

}

void SearchLoadedModules(
	OBJ_CALLBACK_INFORMATION* ModuleInfo)
{
	NTSTATUS status = AuxKlibInitialize();

	if (!NT_SUCCESS(status))
	{
		KdPrint(("[!] AuxKlibInitialize failed (0x%08X)", status));
		return;
	}

	ULONG szBuffer = 0;

	// run once to get required buffer size
	status = AuxKlibQueryModuleInformation(
		&szBuffer,
		sizeof(AUX_MODULE_EXTENDED_INFO),
		NULL);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("[!] AuxKlibQueryModuleInformation failed (0x%08X)", status));
		return;
	}

	// allocate memory
	AUX_MODULE_EXTENDED_INFO* modules = (AUX_MODULE_EXTENDED_INFO*)ExAllocatePool2(
		POOL_FLAG_PAGED,
		szBuffer,
		AGENT_KILLER_TAG);

	if (modules == nullptr)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		return;
	}

	RtlZeroMemory(modules, szBuffer);

	// run again to get the info
	status = AuxKlibQueryModuleInformation(
		&szBuffer,
		sizeof(AUX_MODULE_EXTENDED_INFO),
		modules);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("[!] AuxKlibQueryModuleInformation failed (0x%08X)", status));
		ExFreePoolWithTag(modules, AGENT_KILLER_TAG);
		return;
	}

	// iterate over each module
	ULONG numberOfModules = szBuffer / sizeof(AUX_MODULE_EXTENDED_INFO);

	for (ULONG i = 0; i < numberOfModules; i++)
	{
		ULONG64 startAddress = (ULONG64)modules[i].BasicInfo.ImageBase;
		ULONG imageSize = modules[i].ImageSize;
		ULONG64 endAddress = (ULONG64)(startAddress + imageSize);

		ULONG64 rawPointer = *(PULONG64)(ModuleInfo->Pointer & 0xfffffffffffffff8);

		if (rawPointer > startAddress && rawPointer < endAddress)
		{
			strcpy(ModuleInfo->ModuleName, (CHAR*)(modules[i].FullPathName + modules[i].FileNameOffset));
			break;
		}
	}

	ExFreePoolWithTag(modules, AGENT_KILLER_TAG);
	return;
}

NTSTATUS
CreateClose(
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

void
DriverCleanup(
	PDRIVER_OBJECT DriverObject)
{
	IoDeleteSymbolicLink(&symlink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

WINDOWS_VERSION
GetWindowsVersion()
{
	RTL_OSVERSIONINFOW info;
	info.dwOSVersionInfoSize = sizeof(info);

	NTSTATUS status = RtlGetVersion(&info);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("[!] RtlGetVersion failed (0x%08X)\n", status));
		return WINDOWS_UNSUPPORTED;
	}

	KdPrint(("[+] Windows Version %d.%d\n", info.dwMajorVersion, info.dwBuildNumber));

	if (info.dwMajorVersion != 10)
	{
		return WINDOWS_UNSUPPORTED;
	}

	switch (info.dwBuildNumber)
	{
	case 17763:
		return WINDOWS_REDSTONE_5;

	default:
		return WINDOWS_UNSUPPORTED;
	}
}
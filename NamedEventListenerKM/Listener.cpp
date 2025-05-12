#include <ntddk.h>
#include "CustomData.h"

#define EVENTS_NUMBER 6

UNICODE_STRING g_NamedEvents[EVENTS_NUMBER];
HANDLE         g_hEvents[EVENTS_NUMBER];
PKEVENT        g_pEvents[EVENTS_NUMBER];
KWAIT_BLOCK    g_WaitBlockArray[EVENTS_NUMBER];

void StartListen(ULONG msec)
{
	LARGE_INTEGER Interval;
	Interval.QuadPart = -10000LL * msec;

	NTSTATUS Status = KeWaitForMultipleObjects(EVENTS_NUMBER, (PVOID*)g_pEvents, WaitAny, Executive, KernelMode, FALSE, &Interval, g_WaitBlockArray);

	if (Status > 0 && Status < EVENTS_NUMBER)
	{
		ULONG SignaledIndex = Status;

		DbgPrint("Event Name *%wZ* foi sinalizado!\n", g_NamedEvents[SignaledIndex]);
	}
	else if (Status == STATUS_TIMEOUT)
	{
		DbgPrint("Timeout!\n");
	}
}

NTSTATUS ListenerCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP pIrp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS ListenerWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP pIrp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	NTSTATUS status = STATUS_SUCCESS;
	ULONG information = 0;

	auto pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

	do
	{
		if (pIrpSp->Parameters.Write.Length < sizeof(CUSTOM_DATA))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto data = reinterpret_cast<PCUSTOM_DATA>(pIrp->UserBuffer);

		__try
		{
			StartListen(data->msec);
		}
		__except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			status = STATUS_ACCESS_VIOLATION;
			break;
		}

	} while (false);

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = information;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	for (ULONG i = 0; i < EVENTS_NUMBER; i++)
	{
		ZwClose(g_hEvents[i]);
	}

	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\Listener");

	IoDeleteSymbolicLink(&SymbolicLinkName);
	IoDeleteDevice(DriverObject->DeviceObject);

	DbgPrint("Byebye Driver\n");
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = DriverUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = ListenerCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = ListenerCreateClose;
	DriverObject->MajorFunction[IRP_MJ_WRITE]  = ListenerWrite;

	g_NamedEvents[0] = RTL_CONSTANT_STRING(L"\\KernelObjects\\MemoryErrors");
	g_NamedEvents[1] = RTL_CONSTANT_STRING(L"\\KernelObjects\\PhysicalMemoryChange");
	g_NamedEvents[2] = RTL_CONSTANT_STRING(L"\\KernelObjects\\HighCommitCondition");
	g_NamedEvents[3] = RTL_CONSTANT_STRING(L"\\KernelObjects\\MaximumCommitCondition");
	g_NamedEvents[4] = RTL_CONSTANT_STRING(L"\\KernelObjects\\LowMemoryCondition");
	g_NamedEvents[5] = RTL_CONSTANT_STRING(L"\\KernelObjects\\LowCommitCondition");

	for (ULONG i = 0; i < EVENTS_NUMBER; i++)
	{
		g_pEvents[i] = IoCreateNotificationEvent(&g_NamedEvents[i], &g_hEvents[i]);

		if (g_pEvents[i] == nullptr)
		{
			DbgPrint("Failed to open event: %wZ\n", &g_NamedEvents[i]);
			return STATUS_UNSUCCESSFUL;
		}
	}

	PDEVICE_OBJECT DeviceObject;

	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\Listener");

	NTSTATUS Status = IoCreateDevice(DriverObject, 0, &DeviceName, 0, FILE_DEVICE_UNKNOWN, FALSE, &DeviceObject);

	if (!NT_SUCCESS(Status))
	{
		DbgPrint("Falha ao criar o Device Object (%#08x)\n", Status);
		return Status;
	}

	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\Listener");

	Status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);

	if (!NT_SUCCESS(Status))
	{
		DbgPrint("Falha ao criar o Symbolic Link (%#08x)\n", Status);
		IoDeleteDevice(DeviceObject);
		return Status;
	}

	return STATUS_SUCCESS;
}
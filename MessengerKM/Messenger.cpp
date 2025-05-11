#include <ntddk.h>
#include "MsgType.h"

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\Messenger");

	IoDeleteSymbolicLink(&SymbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS MessengerCreateClose(_In_ PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS MessengerWrite(_In_ PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	NTSTATUS status = STATUS_SUCCESS;

	ULONG_PTR Information = 0;

	auto IrpSp = IoGetCurrentIrpStackLocation(Irp);

	do
	{
		if (IrpSp->Parameters.Write.Length < sizeof(MsgType))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto Msg = static_cast<MsgType*>(Irp->UserBuffer);

		Msg->Message[255] = 0;

		DbgPrint("[MESSENGER] ID=%d | Message: %s\n", Msg->MsgId, Msg->Message);

		Information = sizeof Msg;

	} while (false);

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = Information;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = DriverUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = MessengerCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = MessengerCreateClose;
	DriverObject->MajorFunction[IRP_MJ_WRITE]  = MessengerWrite;

	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\Messenger");

	PDEVICE_OBJECT DeviceObject;

	NTSTATUS status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("Erro ao criar o Device Object\n");
		return status;
	}

	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\Messenger");

	status = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("Erro ao criar um Symbolic Link\n");
		IoDeleteDevice(DeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}
#include "pch.h"

NTSTATUS CompleteIrp(_In_ PIRP Irp, _In_ NTSTATUS Status = STATUS_SUCCESS, _In_ ULONG Information = 0)
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}

NTSTATUS ZeroCreateClose(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp)
{
	return CompleteIrp(Irp);
}

NTSTATUS ZeroRead(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp)
{
	auto IrpSp = IoGetCurrentIrpStackLocation(Irp);

	auto Length = IrpSp->Parameters.Read.Length;

	if (Length == 0)
	{
		return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE);
	}

	NT_ASSERT(Irp->MdlAddress);

	auto Buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

	memset(&Buffer, 0, Length);

	return CompleteIrp(Irp, STATUS_SUCCESS, Length);
}

NTSTATUS ZeroWrite(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp)
{
	auto IrpSp = IoGetCurrentIrpStackLocation(Irp);

	auto Length = IrpSp->Parameters.Read.Length;

	return CompleteIrp(Irp, STATUS_SUCCESS, Length);
}

void ZeroUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\Zero");

	IoDeleteSymbolicLink(&SymbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING /*RegistryPath*/)
{
	DriverObject->DriverUnload = ZeroUnload;
	
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ]   = ZeroRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE]  = ZeroWrite;

	UNICODE_STRING DeviceName   = RTL_CONSTANT_STRING(L"\\Device\\Zero");
	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\Zero");

	PDEVICE_OBJECT DeviceObject = nullptr;

	NTSTATUS Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

	do
	{
		if (!NT_SUCCESS(Status))
		{
			DbgPrint("Erro ao criar o Device Object\n");
			break;
		}

		DeviceObject->Flags |= DO_DIRECT_IO;
	
		Status = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);

		if (!NT_SUCCESS(Status))
		{
			DbgPrint("Erro ao criar o Symbolic Link\n");
			IoDeleteDevice(DeviceObject);
			break;
		}

	} while (false);

	return Status;
}
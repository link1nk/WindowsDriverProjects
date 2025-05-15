#include "pch.h"
#include "ZeroCommon.h"

long long g_TotalRead;
long long g_TotalWritten;

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

	if (!Buffer)
	{
		return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);
	}

	memset(Buffer, 0, Length);

	InterlockedAdd64(&g_TotalRead, Length);

	return CompleteIrp(Irp, STATUS_SUCCESS, Length);
}

NTSTATUS ZeroWrite(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp)
{
	auto IrpSp = IoGetCurrentIrpStackLocation(Irp);
	auto Length = IrpSp->Parameters.Read.Length;

	InterlockedAdd64(&g_TotalWritten, Length);

	return CompleteIrp(Irp, STATUS_SUCCESS, Length);
}

NTSTATUS ZeroDeviceControl(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp)
{
	auto IrpSp = IoGetCurrentIrpStackLocation(Irp);
	auto& DIC = IrpSp->Parameters.DeviceIoControl;
	auto Status = STATUS_INVALID_DEVICE_REQUEST;
	
	ULONG Length = 0;

	switch (DIC.IoControlCode)
	{
	case IOCTL_ZERO_GET_STATS:
	{
		if (DIC.OutputBufferLength < sizeof(ZeroStats))
		{
			Status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto zStatus = reinterpret_cast<ZeroStats*>(Irp->AssociatedIrp.SystemBuffer);

		if (zStatus == nullptr)
		{
			Status = STATUS_INVALID_PARAMETER;
			break;
		}

		zStatus->TotalRead = g_TotalRead;
		zStatus->TotalWritten = g_TotalWritten;

		Length = sizeof (*zStatus);

		Status = STATUS_SUCCESS;

		break;
	}

	case IOCTL_ZERO_CLEAR_STATS:
		g_TotalRead = g_TotalWritten = 0;
		Status = STATUS_SUCCESS;
		break;
	}

	return CompleteIrp(Irp, Status, Length);
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
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ZeroDeviceControl;

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
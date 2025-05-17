#include <ntddk.h>
#include "CommonType.h"

typedef struct _DPC_CONTEXT_WRAPPER {
	PKDPC Dpc;
	PKEVENT Event;
	PPERSON Person;
} DPC_CONTEXT_WRAPPER, *PDPC_CONTEXT_WRAPPER;

CHAR g_LastWrittenName[64] = "None";
KSPIN_LOCK SpinLock;

KDEFERRED_ROUTINE DpcRoutine;

_Use_decl_annotations_ void DpcRoutine
(
	_In_ struct _KDPC* Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
)
{
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(DeferredContext);
	UNREFERENCED_PARAMETER(SystemArgument2);

	if (DeferredContext == nullptr)
	{
		DbgPrint("[DPC] Error: DeferredContext is NULL\n");
		return;
	}

	auto DpcContextWrapper = reinterpret_cast<PDPC_CONTEXT_WRAPPER>(SystemArgument1);

	do
	{
		if (DpcContextWrapper == nullptr)
		{
			break;
		}

		auto UserBuffer = DpcContextWrapper->Person;

		if (UserBuffer == nullptr)
		{
			DbgPrint("[DPC] Error: Person pointer is NULL\n");
			break;
		}

		UserBuffer->Name[63] = 0;

		DbgPrint("[DPC] -> Hello %s!\n", UserBuffer->Name);

		KeAcquireSpinLockAtDpcLevel(&SpinLock);
		DbgPrint("[DPC - SpinLock] -> Last Written Name: %s\n", g_LastWrittenName);
		strcpy_s(g_LastWrittenName, sizeof(g_LastWrittenName), UserBuffer->Name);
		KeReleaseSpinLockFromDpcLevel(&SpinLock);

	} while (false);

	if (DpcContextWrapper != nullptr)
	{
		KeSetEvent(DpcContextWrapper->Event, IO_NO_INCREMENT, FALSE);
	}
}

NTSTATUS CompleteRequest(_In_ PIRP Irp, _In_  NTSTATUS Status = STATUS_SUCCESS, _In_ ULONG Information = 0)
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}

NTSTATUS SayHelloDeviceControl(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp)
{
	auto IrpSp = IoGetCurrentIrpStackLocation(Irp);
	auto& DIC = IrpSp->Parameters.DeviceIoControl;
	ULONG Information = 0;

	NTSTATUS Status = STATUS_INVALID_DEVICE_REQUEST;

	auto DpcContextWrapper = reinterpret_cast<PDPC_CONTEXT_WRAPPER>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof (DPC_CONTEXT_WRAPPER), 'sayh'));

	if (DpcContextWrapper == nullptr)
	{
		Status = STATUS_INSUFFICIENT_RESOURCES;
		return CompleteRequest(Irp, STATUS_SUCCESS, Information);
	}

	RtlZeroMemory(DpcContextWrapper, sizeof(DPC_CONTEXT_WRAPPER));

	DpcContextWrapper->Dpc = reinterpret_cast<PKDPC>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KDPC), 'sayh'));

	if (DpcContextWrapper->Dpc == nullptr)
	{
		Status = STATUS_INSUFFICIENT_RESOURCES;
		ExFreePool(DpcContextWrapper);
		return CompleteRequest(Irp, STATUS_SUCCESS, Information);
	}

	DpcContextWrapper->Event = reinterpret_cast<PKEVENT>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KEVENT), 'sayh'));

	if (DpcContextWrapper->Event == nullptr)
	{
		Status = STATUS_INSUFFICIENT_RESOURCES;
		ExFreePool(DpcContextWrapper->Dpc);
		ExFreePool(DpcContextWrapper);
		return CompleteRequest(Irp, STATUS_SUCCESS, Information);
	}

	KeInitializeEvent(DpcContextWrapper->Event, NotificationEvent, FALSE);

	switch (DIC.IoControlCode)
	{
	case IOCTL_SAYHELLO_WRITE_NAME:
	{
		if (Irp->AssociatedIrp.SystemBuffer == nullptr)
		{
			Status = STATUS_INVALID_PARAMETER;
			break;
		}

		if (DIC.InputBufferLength < sizeof (PERSON))
		{
			Status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto Buffer = reinterpret_cast<PPERSON>(Irp->AssociatedIrp.SystemBuffer);

		if (Buffer == nullptr)
		{
			Status = STATUS_INVALID_PARAMETER;
			break;
		}

		auto CopiedPerson = reinterpret_cast<PPERSON>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(PERSON), 'sayh'));

		if (!CopiedPerson) {
			Status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		memcpy(CopiedPerson, Buffer, sizeof(PERSON));
		DpcContextWrapper->Person = CopiedPerson;

		KeInitializeDpc(DpcContextWrapper->Dpc, DpcRoutine, (PVOID)DpcContextWrapper);
		KeInsertQueueDpc(DpcContextWrapper->Dpc, DpcContextWrapper, nullptr);

		Information = static_cast<ULONG>(strlen(DpcContextWrapper->Person->Name));
		Status = STATUS_SUCCESS;
	
		KeWaitForSingleObject(DpcContextWrapper->Event, Executive, KernelMode, FALSE, nullptr);

		ExFreePool(DpcContextWrapper->Person);

		break;
	}
	}

	ExFreePool(DpcContextWrapper->Event);
	ExFreePool(DpcContextWrapper->Dpc);
	ExFreePool(DpcContextWrapper);

	return CompleteRequest(Irp, Status, Information);
}

NTSTATUS SayHelloCreateClose(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp)
{
	return CompleteRequest(Irp);
}

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\SayHello");

	IoDeleteSymbolicLink(&SymbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING /*RegistryPath*/)
{
	DriverObject->DriverUnload = DriverUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = SayHelloCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = SayHelloCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SayHelloDeviceControl;

	UNICODE_STRING DeviceName   = RTL_CONSTANT_STRING(L"\\Device\\SayHello");
	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\SayHello");

	PDEVICE_OBJECT DeviceObject = nullptr;

	NTSTATUS Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

	do
	{
		if (!NT_SUCCESS(Status))
		{
			DbgPrint("Falha ao criar o Device Object\n");
			break;
		}

		Status = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);

		if (!NT_SUCCESS(Status))
		{
			DbgPrint("Falha ao criar o Symbolic Link\n");
			IoDeleteDevice(DeviceObject);
			break;
		}

		KeInitializeSpinLock(&SpinLock);

	} while (false);

	return Status;
}
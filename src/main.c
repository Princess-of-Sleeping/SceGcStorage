/*
 * SceGcStorage
 * Copyright (C) 2021 Princess of Sleeping
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/sysmem.h>
#include <taihen.h>

#define HookImport(module_name, library_nid, func_nid, func_name) \
	taiHookFunctionImportForKernel(0x10005, &func_name ## _ref, module_name, library_nid, func_nid, func_name ## _patch)

typedef struct SceUIDIoMountEventClass { // size is 0x4C
	uint32_t sce_rsvd[2];
	int data_0x08;
	void *data_0x0C;
	void *data_0x10;
	void *data_0x14;
	int data_0x18;
	int data_0x1C;    // 1
	int data_0x20;    // 0x100
	int data_0x24;
	void *data_0x28;
	void *data_0x2C;
	int data_0x30;
	int data_0x34;
	SceUID data_0x38; // this obj uid
	int data_0x3C;    // 0x202
	int data_0x40;
	void *data_0x44;
	struct SceUIDIoMountEventClass *next;
} SceUIDIoMountEventClass;

typedef struct SceIoPartConfig {
	const char *device;
	const char *blockdev_fs;
	const char *device_block[2];
	int mount_id;
} SceIoPartConfig;

typedef struct SceIoPartEntry { // size is 0x38
	int mount_id;
	const char *dev_unix;
	int data_0x0C;
	int16_t dev_major[2];

	int8_t dev_minor[4];
	const char *dev_fs;
	struct {
		int unk;
		SceIoPartConfig *config;
	} ent[2];

	SceUIDIoMountEventClass *mount_event;
	int data_0x2C;
	int data_0x30;
	int data_0x34;
} SceIoPartEntry;

SceIoPartEntry *sceIoGetEmptyPartEntry(SceIoPartEntry *pIoPartEntry){

	for(int i=0;i<0x20;i++){
		if(pIoPartEntry[i].mount_id == 0)
			return &pIoPartEntry[i];
	}

	return NULL;
}

SceIoPartEntry *sceIoSearchPartEntryById(SceIoPartEntry *pIoPartEntry, SceUInt32 mntId){

	for(int i=0;i<0x20;i++){
		if(pIoPartEntry[i].mount_id == mntId)
			return &pIoPartEntry[i];
	}

	return NULL;
}

/*
 * Genuine SCE sd0: Add entry to enable gamecard sdcard
 */
int add_sd0_ent(void){

	SceKernelModuleInfo sce_info;
	SceUID moduleid;

	moduleid = ksceKernelSearchModuleByName("SceIofilemgr");

	memset(&sce_info, 0, sizeof(sce_info));
	sce_info.size = sizeof(sce_info);
	ksceKernelGetModuleInfo(0x10005, moduleid, &sce_info);

	SceIoPartEntry *pIoPartEntryList, *pIoNewPart;

	pIoPartEntryList = (SceIoPartEntry *)((uintptr_t)(sce_info.segments[1].vaddr) + 0x1A90);

	pIoNewPart = sceIoGetEmptyPartEntry(pIoPartEntryList);
	if(pIoNewPart == NULL)
		return -1;

	memset(pIoNewPart, 0, sizeof(*pIoNewPart));

	const void *pIoPartInfoBase = (const void *)((uintptr_t)sce_info.segments[0].vaddr + 0x1D964);

	pIoNewPart->mount_id = 0x100;
	memcpy(&pIoNewPart->dev_unix, pIoPartInfoBase, 0x20);

	pIoNewPart->ent[0].config = sce_info.segments[0].vaddr + 0x1D7CC;
	pIoNewPart->ent[1].config = sce_info.segments[0].vaddr + 0x1D7CC;

	return 0;
}

int swap_sd0_and_ux0_ent(void){

	SceKernelModuleInfo sce_info;
	SceUID moduleid;

	moduleid = ksceKernelSearchModuleByName("SceIofilemgr");

	memset(&sce_info, 0, sizeof(sce_info));
	sce_info.size = sizeof(sce_info);
	ksceKernelGetModuleInfo(0x10005, moduleid, &sce_info);

	SceIoPartEntry *pIoPartEntryList;

	pIoPartEntryList = (SceIoPartEntry *)((uintptr_t)(sce_info.segments[1].vaddr) + 0x1A90);

	SceIoPartEntry *pIoPartEntrySd0 = sceIoSearchPartEntryById(pIoPartEntryList, 0x100);
	SceIoPartEntry *pIoPartEntryUx0 = sceIoSearchPartEntryById(pIoPartEntryList, 0x800);

	SceIoPartConfig *pIoPartConfigUx0, *pIoPartConfigSd0;

	pIoPartConfigUx0 = (SceIoPartConfig *)ksceKernelAllocHeapMemory(0x1000B, sizeof(*pIoPartConfigUx0));
	pIoPartConfigSd0 = (SceIoPartConfig *)ksceKernelAllocHeapMemory(0x1000B, sizeof(*pIoPartConfigSd0));
	if(pIoPartConfigUx0 == NULL || pIoPartConfigSd0 == NULL)
		return -1;

	memcpy(pIoPartConfigUx0, pIoPartEntryUx0->ent[0].config, sizeof(*pIoPartConfigUx0));
	memcpy(pIoPartConfigSd0, pIoPartEntrySd0->ent[0].config, sizeof(*pIoPartConfigSd0));

	pIoPartConfigUx0->device = "sd0:";
	pIoPartConfigSd0->device = "ux0:";

	pIoPartEntrySd0->ent[0].config = pIoPartConfigSd0;
	pIoPartEntrySd0->ent[1].config = pIoPartConfigSd0;
	pIoPartEntryUx0->ent[0].config = pIoPartConfigUx0;
	pIoPartEntryUx0->ent[1].config = pIoPartConfigUx0;

	ksceIoUmount(0x800, 0, 0, 0);
	ksceIoUmount(0x100, 0, 0, 0);

	ksceIoMount(0x800, NULL, 0, 0, 0, 0);
	ksceIoMount(0x100, NULL, 0, 0, 0, 0);

	return 0;
}

/*
 * Patch sceSysrootUseExternalStorage to return 1 and enable gcsd
 */
tai_hook_ref_t sceSysrootUseExternalStorage_for_SceExfatfs_ref;
int sceSysrootUseExternalStorage_for_SceExfatfs_patch(void){

	TAI_CONTINUE(int, sceSysrootUseExternalStorage_for_SceExfatfs_ref);

	return 1;
}

tai_hook_ref_t sceSysrootUseExternalStorage_for_SceSdstor_ref;
int sceSysrootUseExternalStorage_for_SceSdstor_patch(void){

	TAI_CONTINUE(int, sceSysrootUseExternalStorage_for_SceSdstor_ref);

	return 1;
}

/*
 * Currently only 3.60 is supported
 */
int system_version_check(void){

	SceKblParam *pKblParam = ksceKernelSysrootGetKblParam();
	if(pKblParam == NULL)
		return -1;

	if(pKblParam->current_fw_version != 0x3600000)
		return -1;

	return 0;
}

SceUID hook_id[2];

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp){

	if(ksceSysrootUseExternalStorage() != 0)
		return SCE_KERNEL_START_NO_RESIDENT; // Already sd0: is available

	if(system_version_check() < 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	if(add_sd0_ent() < 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	hook_id[0] = HookImport("SceExfatfs", 0x2ED7F97A, 0x55392965, sceSysrootUseExternalStorage_for_SceExfatfs);
	hook_id[1] = HookImport("SceSdstor",  0x2ED7F97A, 0x55392965, sceSysrootUseExternalStorage_for_SceSdstor);

	// swap_sd0_and_ux0_ent();

	ksceIoMount(0x100, NULL, 0, 0, 0, 0);

	return SCE_KERNEL_START_SUCCESS;
}

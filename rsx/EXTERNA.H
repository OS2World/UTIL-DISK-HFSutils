#pragma aux TASM "_*"   \
	parm caller []	\
	value struct float struct routine [ax]	\
	modify [ax bx cx dx];

#pragma aux TASM2 "_*"   \
	parm caller []	\
	value struct float struct routine [ax]	\
	modify [ax bx cx dx es];

/* adosx32 */
#pragma aux (TASM) int21voff ;
#pragma aux (TASM) int21vsel ;
#pragma aux (TASM) doscall ;
#pragma aux (TASM) back_from_syscall ;
#pragma aux (TASM) debug_entry ;
#pragma aux (TASM) execute_dpmi_function ;

/* cdosx32 */
#pragma aux (TASM) excep0_386;
#pragma aux (TASM) excep1_386;
#pragma aux (TASM) excep2_386;
#pragma aux (TASM) excep3_386;
#pragma aux (TASM) excep4_386;
#pragma aux (TASM) excep5_386;
#pragma aux (TASM) excep6_386;
#pragma aux (TASM) excep7_386;
#pragma aux (TASM) excep8_386;
#pragma aux (TASM) excep9_386;
#pragma aux (TASM) excep10_386;
#pragma aux (TASM) excep11_386;
#pragma aux (TASM) excep12_386;
#pragma aux (TASM) excep13_386;
#pragma aux (TASM) excep14_386;
#pragma aux (TASM) excep15_386;
#pragma aux (TASM) excep16_386;
#pragma aux (TASM) excep17_386;
#pragma aux (TASM) extender_ds;
#pragma aux (TASM) clearregs;
#pragma aux (TASM) page_fault;
#pragma aux (TASM) prot_cbrk ;
#pragma aux (TASM) timer_handler ;

/* copy32 */
#pragma aux (TASM) read32 ;
#pragma aux (TASM) store32 ;
#pragma aux (TASM) bzero32 ;
#pragma aux (TASM) cpy16_32 ;
#pragma aux (TASM) cpy32_16 ;
#pragma aux (TASM) strcpy16_32 ;
#pragma aux (TASM) strcpy32_16 ;
#pragma aux (TASM) cpy32_32 ;
#pragma aux (TASM) getstr32_16 ;
#pragma aux (TASM) put_user_byte ;

/* dpmi */
#pragma aux (TASM2) AllocLDT ;
#pragma aux (TASM2) FreeLDT ;
#pragma aux (TASM2) SegToSel ;
#pragma aux (TASM2) SelInc ;
#pragma aux (TASM2) LockSel ;
#pragma aux (TASM2) UnlockSel ;
#pragma aux (TASM2) GetBaseAddress ;
#pragma aux (TASM2) SetBaseAddress ;
#pragma aux (TASM2) SetLimit ;
#pragma aux (TASM2) SetAccess ;
#pragma aux (TASM2) CreatAlias ;
#pragma aux (TASM2) GetDescriptor ;
#pragma aux (TASM2) SetDescriptor ;
#pragma aux (TASM2) AllocSpecialLDT ;

#pragma aux (TASM2) AllocDosMem ;
#pragma aux (TASM2) FreeDosMem ;
#pragma aux (TASM2) ResizeDosMem ;

#pragma aux (TASM2) GetRealModeVector ;
#pragma aux (TASM2) SetRealModeVector ;
#pragma aux (TASM2) GetExceptionVector ;
#pragma aux (TASM2) SetExceptionVector ;
#pragma aux (TASM2) GetProtModeVector ;
#pragma aux (TASM2) SetProtModeVector ;
#pragma aux (TASM2) GetExceptionVector32 ;
#pragma aux (TASM2) SetExceptionVector32 ;
#pragma aux (TASM2) GetProtModeVector32 ;
#pragma aux (TASM2) SetProtModeVector32 ;

#pragma aux (TASM2) SimulateRMint ;
#pragma aux (TASM2) CallRMprocFar ;
#pragma aux (TASM2) CallRMprocIret ;
#pragma aux (TASM2) AllocRMcallAddress ;
#pragma aux (TASM2) FreeRMcallAddress ;
#pragma aux (TASM2) GetStateSaveAddress ;
#pragma aux (TASM2) SaveState ;
#pragma aux (TASM2) RestoreState ;

#pragma aux (TASM2) GetDPMIVersion ;

#pragma aux (TASM2) GetFreeMemInfo ;
#pragma aux (TASM2) AllocMem ;
#pragma aux (TASM2) FreeMem ;
#pragma aux (TASM2) ResizeMem ;
#pragma aux (TASM2) GetFreeMemInfo32 ;

#pragma aux (TASM2) LockLinRegion;
#pragma aux (TASM2) UnlockLinRegion;
#pragma aux (TASM2) MarkRealModePageable;
#pragma aux (TASM2) RelockRealModeRegion;
#pragma aux (TASM2) GetPageSize;

#pragma aux (TASM2) MarkPageDemand;
#pragma aux (TASM2) DiscardPage;

#pragma aux (TASM2) PhysicalMap;

#pragma aux (TASM2) DisableVirtuelInterruptState ;
#pragma aux (TASM2) EnableVirtuelInterruptState ;
#pragma aux (TASM2) GetVirtuelInterruptState ;

#pragma aux (TASM2) GetVendorEntry ;

#pragma aux (TASM2) ebugWatchpoint ;
#pragma aux (TASM2) rDebugWatchpoint ;
#pragma aux (TASM2) tateDebugWatchpoint ;
#pragma aux (TASM2) tDebugWatchpoint ;

#pragma aux (TASM2) lsl32 ;
#pragma aux (TASM2) lsl16 ;
#pragma aux (TASM2) lar32 ;
#pragma aux (TASM2) lar16 ;
#pragma aux (TASM2) verr16 ;
#pragma aux (TASM2) verw16 ;
#pragma aux (TASM2) sgdt16 ;
#pragma aux (TASM2) sidt16 ;
#pragma aux (TASM2) sldt16 ;
#pragma aux (TASM2) str16 ;

#pragma aux (TASM) GetCS ;
#pragma aux (TASM) GetDS ;
#pragma aux (TASM) GetES ;
#pragma aux (TASM) dos_exit;
#pragma aux (TASM) DpmiEnableFpu;
#pragma aux (TASM) DpmiDisableFpu;
#pragma aux (TASM) GetDpmiEntryPoint;
#pragma aux (TASM) GetDpmiHostParagraph;
#pragma aux (TASM) DpmiEnterProtectedMode;

#pragma aux (TASM) real_to_protected;
#pragma aux (TASM) protected_to_real;

#pragma aux (TASM) farcopy;
#pragma aux (TASM) extmovedata;
#pragma aux (TASM) extmalloc;
#pragma aux (TASM) extfree;
#pragma aux (TASM) set_memory_type;


#pragma aux (TASM2) GetMultipleDescriptors;
#pragma aux (TASM2) SetMultipleDescriptors;
#pragma aux (TASM2) GetProtModeExceptionVector32;
#pragma aux (TASM2) GetRealModeExceptionVector32;
#pragma aux (TASM2) SetProtModeExceptionVector32;
#pragma aux (TASM2) SetRealModeExceptionVector32;
#pragma aux (TASM2) GetDPMICapabilities;
#pragma aux (TASM2) AllocLinearMemory;
#pragma aux (TASM2) ResizeLinearMemory;
#pragma aux (TASM2) GetPageAttributes;
#pragma aux (TASM2) ModifyPageAttributes;
#pragma aux (TASM2) MapDeviceInMemoryBlock;
#pragma aux (TASM2) MapDOSMemInMemoryBlock;
#pragma aux (TASM2) GetMemoryBlockData;
#pragma aux (TASM2) GetMemoryInfo;
#pragma aux (TASM2) FreePhysicalMapping;
#pragma aux (TASM2) DpmiGetCoproStatus;
#pragma aux (TASM2) DpmiSetCoproStatus;


#pragma aux (TASM2) npx_installed ;
#pragma aux (TASM2) emu_init ;
#pragma aux (TASM2) emu_switch ;
#pragma aux (TASM2) do_fninit;
#pragma aux (TASM2) do_frstor;
#pragma aux (TASM2) do_fnsave;

